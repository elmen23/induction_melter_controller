/**
 * =============================================================================
 *  config.h — Central configuration & hardware pin map
 *  Project : 1-channel induction melting controller (ESP32)
 *  Author  : Mavis (mavis-llc) — generated for the user
 * =============================================================================
 *
 *  Edit this file to match YOUR wiring before flashing. Every other module
 *  pulls its constants from here, so you should never have to grep through
 *  the source tree to change a pin number.
 *
 *  Keep the limits (`MELTER_*`) conservative until you have validated the
 *  inverter with a dummy load / water-cooled coil. Once the resonant tank
 *  is tuned and you know the safe operating envelope, push them outwards.
 * ===========================================================================*/

#ifndef MELTER_CONFIG_H
#define MELTER_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Build-time switches                                                       *
 * ------------------------------------------------------------------------- */
#define MELTER_FIRMWARE_VERSION   "1.0.0"
#define MELTER_FIRMWARE_BUILD     __DATE__ " " __TIME__
#define MELTER_DEVICE_NAME        "Induction Melter 1CH"
#define MELTER_HOSTNAME_DEFAULT   "melter"
#define MELTER_HTTP_USER          "admin"
#define MELTER_HTTP_PASS          "melter"   // Change in production!

#ifndef CONTROLLER_DEBUG
#define CONTROLLER_DEBUG          0           // set to 1 for verbose serial
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL                 2           // 0=NONE 1=ERR 2=INF 3=DBG 4=V
#endif

/* ------------------------------------------------------------------------- *
 *  PWM output — half-bridge driver board                                     *
 *  Most ZVS / Royer drivers take a single PWM enable + one frequency pin.   *
 *  For a true variable-frequency full-bridge, use both channels.            *
 * ------------------------------------------------------------------------- */
#define PIN_PWM_OUT_A             25          // LEDC channel 0
#define PIN_PWM_OUT_B             26          // LEDC channel 1 (complementary)
#define PIN_DRIVER_ENABLE         27          // HIGH = driver armed
#define PIN_DRIVER_FAULT          34          // INPUT only — driver pulls LOW on fault

#define LEDC_CHAN_A               0
#define LEDC_CHAN_B               1
#define LEDC_TIMER                0
#define LEDC_RES_BITS             10          // 0..1023 duty resolution

/* ------------------------------------------------------------------------- *
 *  Sensing — current / voltage / temperature / coolant                      *
 * ------------------------------------------------------------------------- */
// AC current sensor (ACS712-30A → 66 mV/A, Vcc/2 = 1.65V on 3.3V ADC)
#define PIN_SENSE_CURRENT         35          // ADC1_CH7 — input-only, safe
#define SENSE_CURRENT_OFFSET_MV   1650
#define SENSE_CURRENT_MV_PER_AMP  66

// DC bus voltage (resistive divider 100k / 10k → /11)
#define PIN_SENSE_VBUS            32          // ADC1_CH4
#define SENSE_VBUS_DIVIDER        11.0f

// Type-K thermocouple amplifier (MAX6675)
#define PIN_TC_CS                 5           // SPI CS for MAX6675
#define PIN_TC_SO                 18          // SPI MISO (shared with SD)
#define PIN_TC_SCK                19          // SPI SCK  (shared with SD)
#define TC_SPI_CLOCK_HZ           4500000UL

// Coolant water flow (YF-S401, hall-effect, 5.5 Hz per L/min)
#define PIN_FLOW_SENSOR           4           // interrupt-capable
#define FLOW_PULSES_PER_LITRE     450

// NTC on the IGBT / heatsink (10k B3950, voltage divider to 3V3)
#define PIN_NTC_IGBT              33          // ADC1_CH5
#define NTC_SERIES_R_OHM          10000.0f
#define NTC_NOMINAL_R_OHM         10000.0f
#define NTC_NOMINAL_T_KELVIN      298.15f     // 25 °C
#define NTC_BETA                 3950.0f

// Coolant temperature (second NTC, 10k B3950)
#define PIN_NTC_COOLANT           36          // ADC1_CH0 — input only
#define NTC_COOLANT_SERIES_R      10000.0f

/* ------------------------------------------------------------------------- *
 *  User interface — physical E-STOP / START / STOP buttons + status LED
 *  All other UI lives in the web dashboard (WiFi / Ethernet).  No OLED.
 * ------------------------------------------------------------------------- */
#define PIN_BTN_START             2           // on-board LED on most dev boards
#define PIN_BTN_STOP              0           // BOOT button — hardware E-STOP
#define PIN_BUZZER                13          // PWM-able for tones
#define PIN_LED_STATUS            2           // mirrors the start button
#define PIN_RELAY_MAIN            23          // coil / driver main contactor

/* Optional — keep these around in case you want a hardware encoder later.
   The firmware does NOT use them by default.                                */
#define PIN_ENCODER_A             16
#define PIN_ENCODER_B             17
#define PIN_ENCODER_BTN           5
#define ENCODER_STEPS_PER_DETENT  4

/* ------------------------------------------------------------------------- *
 *  SD card (SPI, used in parallel with the thermocouple CS)                 *
 * ------------------------------------------------------------------------- */
#define PIN_SD_CS                 14
#define PIN_SD_SCK                19          // shared with TC_SCK
#define PIN_SD_MISO               18          // shared with TC_SO
#define PIN_SD_MOSI               19          // ⚠ SD & TC share SCK/MISO only if
                                             //  CS is on different pins — OK here

/* ------------------------------------------------------------------------- *
 *  Operating limits  ⚠ TUNE THESE FOR YOUR COIL / DRIVER                    *
 * ------------------------------------------------------------------------- */
#define MELTER_FREQ_MIN_HZ        20000UL     // 20 kHz
#define MELTER_FREQ_MAX_HZ        200000UL    // 200 kHz
#define MELTER_FREQ_DEFAULT_HZ    50000UL     // 50 kHz — good starting point
#define MELTER_FREQ_STEP_HZ       100UL

#define MELTER_DUTY_MIN_PCT       0
#define MELTER_DUTY_MAX_PCT       95          // never run 100% on a half-bridge
#define MELTER_DUTY_DEFAULT_PCT   40
#define MELTER_DUTY_STEP_PCT      1

#define MELTER_POWER_MIN_W        0
#define MELTER_POWER_MAX_W        3000
#define MELTER_POWER_DEFAULT_W    1500
#define MELTER_POWER_STEP_W       50

/* Sensor-driven protections */
#define MELTER_OC_LIMIT_AMPS      30.0f
#define MELTER_OC_HOLD_MS         200
#define MELTER_IGBT_OT_LIMIT_C    75.0f
#define MELTER_COOLANT_OT_LIMIT_C 55.0f
#define MELTER_FLOW_MIN_LMIN      1.0f
#define MELTER_FLOW_TIMEOUT_MS    3000UL
#define MELTER_VBUS_OV_LIMIT_V    120.0f
#define MELTER_VBUS_UV_LIMIT_V    10.0f

/* Soft-start ramp (ms) — ramps duty from 0 → target on enable */
#define MELTER_SOFT_START_MS      800

/* ADC sampling */
#define ADC_SAMPLES               64
#define ADC_SAMPLE_DELAY_US       200
#define SENSE_LOOP_HZ             200         // 5 ms control tick

/* Watchdog */
#define WDT_TIMEOUT_S             10

/* Preferences (NVS) keys */
#define NVS_KEY_FREQ              "freq"
#define NVS_KEY_DUTY              "duty"
#define NVS_KEY_POWER             "pwr"
#define NVS_KEY_PRESET            "preset"
#define NVS_KEY_PID_KP            "pid.kp"
#define NVS_KEY_PID_KI            "pid.ki"
#define NVS_KEY_PID_KD            "pid.kd"
#define NVS_KEY_SSID              "net.ssid"
#define NVS_KEY_PASS              "net.pass"
#define NVS_KEY_AP_MODE           "net.ap"
#define NVS_KEY_DEVICENAME        "hostname"

/* Web interface */
#define WEB_DEFAULT_PORT          80
#define WEB_REFRESH_MS            100         // telemetry push cadence
#define WEB_HISTORY_LEN           120         // sparkline buffer (~12 s @100 ms)
#define WEB_TITLE                 "Induction Melter 1CH"

/* Task priorities (0 = lowest) */
#define TASK_PRIO_PROTECT         5           // highest — must preempt PID
#define TASK_PRIO_CONTROL         4
#define TASK_PRIO_SENSE           3
#define TASK_PRIO_WEB             2
#define TASK_PRIO_LOGGER          1

/* Task stack sizes (bytes) */
#define TASK_STACK_SENSE          4096
#define TASK_STACK_PROTECT        4096
#define TASK_STACK_WEB            12288
#define TASK_STACK_LOGGER         4096
#define TASK_STACK_CONTROL        4096

/* Logger (SD) */
#define LOG_FILE_PATH             "/melter.csv"
#define LOG_HEADER                "epoch_ms,freq_hz,duty_pct,current_a,vbus_v,"  \
                                  "igbt_c,coolant_c,flow_lpm,state"
#define LOG_INTERVAL_MS           1000UL

/* Preset recipes (load = target metal) */
typedef struct {
    const char* name;        // user-facing
    uint32_t    freq_hz;
    uint8_t     duty_pct;
    uint16_t    power_w;
} metal_preset_t;

static const metal_preset_t MELTER_PRESETS[] = {
    { "Custom",      50000,  40, 1500 },
    { "Aluminium",   30000,  55, 2200 },
    { "Copper",      80000,  45, 1800 },
    { "Brass",       60000,  45, 1700 },
    { "Iron",        45000,  60, 2400 },
    { "Steel",       40000,  60, 2400 },
    { "Gold",       120000,  35,  900 },
    { "Silver",     100000,  35, 1000 },
    { "Lead",        25000,  30,  600 },
};
#define MELTER_PRESET_COUNT  (sizeof(MELTER_PRESETS) / sizeof(MELTER_PRESETS[0]))

/* Operating states (also used in JSON API) */
typedef enum : uint8_t {
    ST_BOOT,
    ST_IDLE,
    ST_SOFT_START,
    ST_RUN,
    ST_FAULT,
    ST_ESTOP,
    ST_TUNING,
} melter_state_t;

/* Fault codes */
typedef enum : uint8_t {
    FAULT_NONE = 0,
    FAULT_OVERCURRENT,
    FAULT_IGBT_OVERTEMP,
    FAULT_COOLANT_OVERTEMP,
    FAULT_NO_FLOW,
    FAULT_DRIVER,
    FAULT_VBUS_OV,
    FAULT_VBUS_UV,
    FAULT_WATCHDOG,
    FAULT_SENSOR,
} melter_fault_t;

static const char* fault_to_string(melter_fault_t f) {
    switch (f) {
        case FAULT_NONE:             return "OK";
        case FAULT_OVERCURRENT:      return "Over current";
        case FAULT_IGBT_OVERTEMP:    return "IGBT over-temp";
        case FAULT_COOLANT_OVERTEMP: return "Coolant over-temp";
        case FAULT_NO_FLOW:          return "No coolant flow";
        case FAULT_DRIVER:           return "Driver fault pin";
        case FAULT_VBUS_OV:          return "Vbus over-voltage";
        case FAULT_VBUS_UV:          return "Vbus under-voltage";
        case FAULT_WATCHDOG:         return "Watchdog reset";
        case FAULT_SENSOR:           return "Sensor read fail";
    }
    return "Unknown";
}

static inline const char* state_to_string(melter_state_t s) {
    switch (s) {
        case ST_BOOT:       return "Boot";
        case ST_IDLE:       return "Idle";
        case ST_SOFT_START: return "SoftStart";
        case ST_RUN:        return "Run";
        case ST_FAULT:      return "FAULT";
        case ST_ESTOP:      return "E-STOP";
        case ST_TUNING:     return "Tuning";
    }
    return "?";
}

#endif  /* MELTER_CONFIG_H */
