/**
 * =============================================================================
 *  main.cpp — Induction Melter 1-channel controller (ESP32)
 * =============================================================================
 *  Top-level wiring.  Brings up every subsystem, restores the saved
 *  set-point from NVS, starts the FreeRTOS tasks, then loops in a low
 *  priority idle to:
 *     • pump the physical button debouncers
 *     • run the soft-start ramp
 *     • service the PID loop
 *     • feed the watchdog
 * ===========================================================================*/

#include "config.h"
#include "logging.h"
#include "power_controller.h"
#include "protection.h"
#include "user_input.h"
#include "pid.h"
#include "web_interface.h"
#include "logger.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

/* ------------ NVS state (saved when user clicks Reset / set) ------------ */
static Preferences g_prefs;
static void loadNvs() {
    g_prefs.begin("melter", true);
    uint32_t f = g_prefs.getUInt(NVS_KEY_FREQ,  MELTER_FREQ_DEFAULT_HZ);
    uint8_t  d = g_prefs.getUChar(NVS_KEY_DUTY, MELTER_DUTY_DEFAULT_PCT);
    uint16_t p = g_prefs.getUShort(NVS_KEY_POWER, MELTER_POWER_DEFAULT_W);
    float kp = g_prefs.getFloat(NVS_KEY_PID_KP, 2.0f);
    float ki = g_prefs.getFloat(NVS_KEY_PID_KI, 0.5f);
    float kd = g_prefs.getFloat(NVS_KEY_PID_KD, 0.1f);
    g_prefs.end();
    Power::setSetpoint({ f, d, p });
    Pid::setTunings(kp, ki, kd);
    LOG_I("NVS loaded: freq=%lu duty=%u power=%u pid=(%.2f %.2f %.2f)",
          f, d, p, kp, ki, kd);
}
static void saveNvs() {
    auto sp = Power::getSetpoint();
    g_prefs.begin("melter", false);
    g_prefs.putUInt  (NVS_KEY_FREQ,  sp.freq_hz);
    g_prefs.putUChar (NVS_KEY_DUTY,  sp.duty_pct);
    g_prefs.putUShort(NVS_KEY_POWER, sp.power_w);
    g_prefs.end();
}

/* ------------ watchdog ------------ */
static void initWdt() {
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);
}

void setup() {
    Melter::Log::begin();
    Melter::Log::taskTag("setup");
    LOG_I("=== %s v%s ===", MELTER_DEVICE_NAME, MELTER_FIRMWARE_VERSION);
    LOG_I("build: %s", MELTER_FIRMWARE_BUILD);

    /* hardware */
    pinMode(PIN_RELAY_MAIN, OUTPUT);
    digitalWrite(PIN_RELAY_MAIN, LOW);

    /* modules */
    Melter::Power::begin();
    Melter::Input::begin();
    Melter::Protection::begin();
    Melter::Pid::begin();
    Melter::Logger::begin();
    Melter::Web::begin();

    loadNvs();

    /* tasks */
    Melter::Protection::startTask();
    Melter::Web::startTask();
    Melter::Logger::startTask();

    initWdt();
    LOG_I("=== boot complete ===");
}

/* Main loop — keep this LIGHT. Heavy work happens in tasks. */
static uint32_t g_soft_start_ms = 0;
static uint32_t g_last_save    = 0;
static uint32_t g_last_pid     = 0;
static uint32_t g_last_input   = 0;
static bool     g_was_armed    = false;

void loop() {
    uint32_t now = millis();

    /* Hardware button debouncing */
    if (now - g_last_input >= 5) {
        g_last_input = now;
        Melter::Input::tick();

        if (Melter::Input::consume(Melter::Input::Btn::START)) {
            if (!Melter::Protection::isArmed()
                && !Melter::Protection::hasFault()
                && !Melter::Input::eStopLatched()) {
                Melter::Protection::arm();
                Melter::Power::enableOutput(true);
                digitalWrite(PIN_RELAY_MAIN, HIGH);
                Melter::Input::setStatusLed(true);
                Melter::Input::beep(60);
            }
        }
        if (Melter::Input::consume(Melter::Input::Btn::STOP)
         || Melter::Input::consume(Melter::Input::Btn::ESTOP_HARD)) {
            Melter::Protection::disarm();
            Melter::Power::enableOutput(false);
            digitalWrite(PIN_RELAY_MAIN, LOW);
            Melter::Input::setStatusLed(false);
            Melter::Input::beep(120, 1800);
        }
    }

    /* Edge-detect "armed" for the soft-start timer */
    bool armed = Melter::Protection::isArmed();
    if (armed && !g_was_armed) g_soft_start_ms = now;
    if (!armed)                g_soft_start_ms = 0;
    g_was_armed = armed;
    if (armed) Melter::Power::softStartTick(now - g_soft_start_ms);

    /* PID tick at 50 Hz (cheap arithmetic) */
    if (now - g_last_pid >= 20) {
        g_last_pid = now;
        auto sp  = Melter::Power::getSetpoint();
        auto pr  = Melter::Protection::snapshot();
        double measured_w = pr.current_a * pr.vbus_v;
        Melter::Pid::tick(measured_w, 0.02);
        if (Melter::Pid::isEnabled() && Melter::Power::isOutputEnabled()) {
            Melter::Power::setDuty((uint8_t)Melter::Pid::output());
        }
    }

    /* Save NVS every 30 s (and on disarm) */
    if (now - g_last_save >= 30000UL) {
        g_last_save = now;
        saveNvs();
    }

    /* Feed the WDT */
    esp_task_wdt_reset();
    delay(2);
}
