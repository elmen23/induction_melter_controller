/**
 * protection.cpp
 */

#include "protection.h"
#include "logging.h"
#include "power_controller.h"
#include "user_input.h"
#include <Arduino.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>

namespace Melter {
namespace Protection {

static volatile bool     g_armed          = false;
static volatile bool     g_latched        = false;
static volatile uint32_t g_fault_count    = 0;
static volatile uint8_t  g_oc_window_ms   = 0;
static volatile uint8_t  g_last_fault_code= 0;
static char              g_note[64]       = "";
static volatile uint32_t g_last_flow_pulse_ms = 0;
static volatile uint32_t g_pulse_count        = 0;

/* ---------------- ISR for water flow sensor ----------------- */
static void IRAM_ATTR flow_isr() {
    g_pulse_count++;
    g_last_flow_pulse_ms = millis();
}

/* ---------------- NTC math ----------------- */
static float ntc_celsius(uint8_t pin, float series_r, float nominal_r,
                         float nominal_t_k, float beta) {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        acc += analogReadMilliVolts(pin);
    }
    float v_mv   = acc / 16.0f;
    float v_supply = 3300.0f;
    float r_ntc  = series_r * v_mv / (v_supply - v_mv);
    float inv_t  = (1.0f / nominal_t_k) + (1.0f / beta) * logf(r_ntc / nominal_r);
    float t_k    = 1.0f / inv_t;
    return t_k - 273.15f;
}

/* ---------------- MAX6675 thermocouple over SPI ----------------- */
static SPISettings g_tc_spi(TC_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0);
static float read_thermocouple_c() {
    digitalWrite(PIN_TC_CS, LOW);
    SPI.beginTransaction(g_tc_spi);
    uint16_t hi = SPI.transfer(0x00);
    uint16_t lo = SPI.transfer(0x00);
    SPI.endTransaction();
    digitalWrite(PIN_TC_CS, HIGH);

    uint16_t raw = ((hi << 8) | lo) >> 3;
    if (raw & 0x1000) {  // open thermocouple flag
        return NAN;
    }
    return raw * 0.25f;
}

/* ---------------- Public API ----------------- */
void begin() {
    Log::taskTag("protect");
    pinMode(PIN_DRIVER_FAULT, INPUT_PULLUP);
    pinMode(PIN_TC_CS, OUTPUT);
    digitalWrite(PIN_TC_CS, HIGH);
    pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), flow_isr, FALLING);
    g_last_flow_pulse_ms = millis();
    LOG_I("protections armed, defaults loaded");
}

void startTask() {
    xTaskCreatePinnedToCore(
        [](void* arg) {
            Log::taskTag("protect");
            TickType_t last = xTaskGetTickCount();
            uint32_t last_flow_calc = 0;
            uint32_t pulses_at_calc = 0;
            while (true) {
                TickType_t now = xTaskGetTickCount();
                vTaskDelayUntil(&last, pdMS_TO_TICKS(20));   // 50 Hz

                Snapshot s = snapshot();
                if (isnan(s.igbt_c) || isnan(s.coolant_c) || isnan(s.current_a)) {
                    if (g_armed) raise(FAULT_SENSOR, "ADC read NaN");
                    continue;
                }

                /* Coolant flow rate (L/min) */
                uint32_t t = millis();
                if (t - last_flow_calc >= 1000) {
                    uint32_t delta = g_pulse_count - pulses_at_calc;
                    pulses_at_calc = g_pulse_count;
                    last_flow_calc = t;
                    float lpm = (delta * 60.0f * 1000.0f)
                              / (1000.0f * FLOW_PULSES_PER_LITRE);
                    if (lpm < 0.05f) lpm = 0.0f;
                    s.flow_lpm = lpm;
                }

                if (!g_armed) continue;

                /* Driver fault pin */
                if (digitalRead(PIN_DRIVER_FAULT) == LOW) {
                    raise(FAULT_DRIVER, "driver board pulled FAULT low");
                    continue;
                }

                /* Over-current debounce */
                if (s.current_a > MELTER_OC_LIMIT_AMPS) {
                    g_oc_window_ms += 20;
                    if (g_oc_window_ms >= MELTER_OC_HOLD_MS) {
                        raise(FAULT_OVERCURRENT, "primary OC");
                        continue;
                    }
                } else {
                    g_oc_window_ms = 0;
                }

                /* IGBT over-temperature */
                if (s.igbt_c > MELTER_IGBT_OT_LIMIT_C) {
                    raise(FAULT_IGBT_OVERTEMP, "heatsink hot");
                    continue;
                }
                if (s.coolant_c > MELTER_COOLANT_OT_LIMIT_C) {
                    raise(FAULT_COOLANT_OVERTEMP, "coolant hot");
                    continue;
                }

                /* Coolant flow */
                if ((t - g_last_flow_pulse_ms) > MELTER_FLOW_TIMEOUT_MS) {
                    raise(FAULT_NO_FLOW, "no pulses");
                    continue;
                }
                if (s.flow_lpm < MELTER_FLOW_MIN_LMIN) {
                    raise(FAULT_NO_FLOW, "low flow");
                    continue;
                }

                /* Vbus */
                if (s.vbus_v > MELTER_VBUS_OV_LIMIT_V) {
                    raise(FAULT_VBUS_OV, "over-voltage");
                    continue;
                }
                if (s.vbus_v < MELTER_VBUS_UV_LIMIT_V) {
                    raise(FAULT_VBUS_UV, "under-voltage");
                    continue;
                }
            }
        },
        "protect", TASK_STACK_PROTECT, nullptr,
        TASK_PRIO_PROTECT, nullptr, 1);
}

void arm()   { g_armed = true; g_oc_window_ms = 0; LOG_I("ARMED"); }
void disarm(){ g_armed = false; Power::enableOutput(false); LOG_I("DISARMED"); }
bool isArmed() { return g_armed; }

void raise(melter_fault_t f, const char* note) {
    if (g_latched) return;        // already tripped, do not re-trigger
    g_latched = true;
    g_armed   = false;
    g_fault_count++;
    g_last_fault_code = (uint8_t)f;
    strncpy(g_note, note, sizeof(g_note) - 1);
    g_note[sizeof(g_note) - 1] = 0;
    LOG_E("FAULT %s (%s)", fault_to_string(f), g_note);
    Power::enableOutput(false);
    digitalWrite(PIN_RELAY_MAIN, LOW);
    /* one long + two short beeps via the input module */
    Input::beep(150, 2000);
    delay(60);
    Input::beep(80, 2500);
    delay(60);
    Input::beep(80, 2500);
}

melter_fault_t lastFault() {
    return g_latched ? (melter_fault_t)g_last_fault_code : FAULT_NONE;
}
const char* lastNote() { return g_note; }
bool hasFault() { return g_latched; }
uint32_t faultCount() { return g_fault_count; }

void clearFault() {
    g_latched = false;
    g_note[0] = 0;
    g_oc_window_ms = 0;
    LOG_I("fault cleared");
}

Snapshot snapshot() {
    Snapshot s {};
    s.current_a   = Power::primaryCurrent();
    s.vbus_v      = Power::busVoltage();
    s.igbt_c      = ntc_celsius(PIN_NTC_IGBT,
                                NTC_SERIES_R_OHM, NTC_NOMINAL_R_OHM,
                                NTC_NOMINAL_T_KELVIN, NTC_BETA);
    s.coolant_c   = ntc_celsius(PIN_NTC_COOLANT,
                                NTC_COOLANT_SERIES_R, NTC_NOMINAL_R_OHM,
                                NTC_NOMINAL_T_KELVIN, NTC_BETA);
    s.flow_lpm    = 0.0f;     // updated in the task loop
    s.driver_fault_pin = (digitalRead(PIN_DRIVER_FAULT) == LOW);
    s.uptime_ms   = millis();
    return s;
}

}  // namespace Protection
}  // namespace Melter
