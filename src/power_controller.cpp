/**
 * power_controller.cpp
 *
 * The LEDC peripheral works in 1 of 2 directions for changing frequency:
 *   • use ledcChangeFrequency() — limited to ~40 kHz transitions
 *   • tear down + re-install the channel at the new frequency
 *
 * We go with the second approach: it is bullet-proof and works for every
 * 20–200 kHz target.  Frequency changes happen at human speed (< 100 Hz/s)
 * so the brief glitch (~30 µs) is invisible to the resonant tank.
 */

#include "power_controller.h"
#include "logging.h"
#include <Arduino.h>
#include <driver/ledc.h>
#include <math.h>

namespace Melter {
namespace Power {

static Setpoint     g_sp {
    MELTER_FREQ_DEFAULT_HZ,
    MELTER_DUTY_DEFAULT_PCT,
    MELTER_POWER_DEFAULT_W
};
static LiveReadings g_live {};
static bool         g_comp = true;       // full-bridge by default
static bool         g_out  = false;
static uint8_t      g_ramp_duty = 0;
static uint32_t     g_ramp_start_ms = 0;
static bool         g_ramping = false;

/* -------- LEDC install / uninstall helper ----------------------------- */
static void install_channels(uint32_t freq_hz) {
    ledc_timer_config_t tcfg = {};
    tcfg.speed_mode      = LEDC_HIGH_SPEED_MODE;
    tcfg.duty_resolution = (ledc_timer_bit_t)LEDC_RES_BITS;
    tcfg.timer_num       = (ledc_timer_t)LEDC_TIMER;
    tcfg.freq_hz         = freq_hz;
    tcfg.clk_cfg         = LEDC_USE_APB_CLK;
    ESP_ERROR_CHECK( ledc_timer_config(&tcfg) );

    ledc_channel_config_t ccfg = {};
    ccfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    ccfg.channel    = (ledc_channel_t)LEDC_CHAN_A;
    ccfg.timer_sel  = (ledc_timer_t)LEDC_TIMER;
    ccfg.gpio_num   = PIN_PWM_OUT_A;
    ccfg.duty       = 0;
    ccfg.hpoint     = 0;
    ESP_ERROR_CHECK( ledc_channel_config(&ccfg) );

    ccfg.channel  = (ledc_channel_t)LEDC_CHAN_B;
    ccfg.gpio_num = PIN_PWM_OUT_B;
    ESP_ERROR_CHECK( ledc_channel_config(&ccfg) );
}

static uint16_t pct_to_count(uint8_t pct) {
    if (pct > MELTER_DUTY_MAX_PCT) pct = MELTER_DUTY_MAX_PCT;
    uint32_t max = (1UL << LEDC_RES_BITS) - 1;
    return (uint16_t)((uint32_t)pct * max / 100);
}

static void apply_duty_to_hardware(uint8_t pct) {
    uint16_t c = pct_to_count(pct);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_A, c);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_A);
    if (g_comp) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_B, c);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_B);
    } else {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_B, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_B);
    }
}

/* ------------------------------- API --------------------------------- */
void begin() {
    Log::taskTag("power");
    pinMode(PIN_DRIVER_ENABLE, OUTPUT);
    digitalWrite(PIN_DRIVER_ENABLE, LOW);

    install_channels(g_sp.freq_hz);
    apply_duty_to_hardware(0);
    g_live.actual_duty_pct = 0;
    g_live.actual_freq_hz = g_sp.freq_hz;
    g_live.output_enabled = false;
    g_live.complementary  = g_comp;
    LOG_I("init freq=%lu duty=%u%%", g_sp.freq_hz, g_sp.duty_pct);
}

void end() {
    digitalWrite(PIN_DRIVER_ENABLE, LOW);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_A, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_A);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_B, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_B);
    g_out = false;
}

void enableComplementary(bool on) { g_comp = on; g_live.complementary = on; }
bool isOutputEnabled() { return g_out; }

void enableOutput(bool on) {
    if (g_out == on) return;
    g_out = on;
    g_live.output_enabled = on;
    if (on) {
        digitalWrite(PIN_DRIVER_ENABLE, HIGH);
        if (MELTER_SOFT_START_MS > 0) {
            g_ramp_duty    = 0;
            g_ramp_start_ms = millis();
            g_ramping       = true;
            LOG_I("soft start ramp begin");
        } else {
            apply_duty_to_hardware(g_sp.duty_pct);
            g_live.actual_duty_pct = g_sp.duty_pct;
        }
    } else {
        digitalWrite(PIN_DRIVER_ENABLE, LOW);
        apply_duty_to_hardware(0);
        g_live.actual_duty_pct = 0;
        g_ramping = false;
        LOG_I("output disabled");
    }
}

void setFrequency(uint32_t hz) {
    if (hz < MELTER_FREQ_MIN_HZ) hz = MELTER_FREQ_MIN_HZ;
    if (hz > MELTER_FREQ_MAX_HZ) hz = MELTER_FREQ_MAX_HZ;
    if (g_sp.freq_hz == hz) return;
    g_sp.freq_hz = hz;
    if (g_out) {
        install_channels(hz);
        apply_duty_to_hardware(g_live.actual_duty_pct);
    }
    g_live.actual_freq_hz = hz;
    LOG_I("freq -> %lu Hz", hz);
}

void setDuty(uint8_t percent) {
    if (percent > MELTER_DUTY_MAX_PCT) percent = MELTER_DUTY_MAX_PCT;
    g_sp.duty_pct = percent;
    if (g_out && !g_ramping) {
        apply_duty_to_hardware(percent);
        g_live.actual_duty_pct = percent;
    }
}

void setPowerTarget(uint16_t w) {
    if (w < MELTER_POWER_MIN_W) w = MELTER_POWER_MIN_W;
    if (w > MELTER_POWER_MAX_W) w = MELTER_POWER_MAX_W;
    g_sp.power_w = w;
}

void setSetpoint(const Setpoint& sp) {
    setFrequency(sp.freq_hz);
    setDuty(sp.duty_pct);
    setPowerTarget(sp.power_w);
}

Setpoint getSetpoint() { return g_sp; }

void apply() {
    install_channels(g_sp.freq_hz);
    apply_duty_to_hardware(g_live.actual_duty_pct);
}

void softStartTick(uint32_t elapsed_ms) {
    if (!g_ramping) return;
    if (elapsed_ms >= MELTER_SOFT_START_MS) {
        g_ramping = false;
        apply_duty_to_hardware(g_sp.duty_pct);
        g_live.actual_duty_pct = g_sp.duty_pct;
        LOG_I("soft start complete -> %u%%", g_sp.duty_pct);
        return;
    }
    uint32_t pct32 = (uint32_t)g_sp.duty_pct * elapsed_ms / MELTER_SOFT_START_MS;
    if (pct32 != g_ramp_duty) {
        g_ramp_duty = (uint8_t)pct32;
        apply_duty_to_hardware(g_ramp_duty);
        g_live.actual_duty_pct = g_ramp_duty;
    }
}

LiveReadings live() { return g_live; }
uint16_t rawDutyCount() { return ledc_get_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHAN_A); }

/* ----- Hardware sensor helpers (kept here to share ADC) ---------------- */
float busVoltage() {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; ++i) {
        acc += analogReadMilliVolts(PIN_SENSE_VBUS);
        delayMicroseconds(ADC_SAMPLE_DELAY_US);
    }
    float vadc_mv = acc / (float)ADC_SAMPLES;
    return vadc_mv * 1e-3f * SENSE_VBUS_DIVIDER;
}

float primaryCurrent() {
    // RMS approximation from average-absolute. Assumes a roughly sinusoidal
    // current around 1.65 V offset.  Multiply by 1.11 to get RMS from AVG.
    int32_t acc = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; ++i) {
        acc += analogReadMilliVolts(PIN_SENSE_CURRENT);
        delayMicroseconds(ADC_SAMPLE_DELAY_US);
    }
    float vadc_mv = acc / (float)ADC_SAMPLES;
    float amps = (vadc_mv - SENSE_CURRENT_OFFSET_MV) / SENSE_CURRENT_MV_PER_AMP;
    return fabsf(amps);
}

}  // namespace Power
}  // namespace Melter
