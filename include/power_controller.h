/**
 * =============================================================================
 *  power_controller.h — Variable frequency + duty cycle PWM driver
 * =============================================================================
 *  Uses the ESP32 LEDC peripheral to drive a half-bridge / full-bridge
 *  induction heating inverter.  LEDC can reach 40 MHz in theory; we
 *  limit the API to 20–200 kHz which covers 99 % of small induction
 *  melters.
 *
 *  Two output channels:
 *     • CH_A (LEDC_CHAN_A) — leading leg
 *     • CH_B (LEDC_CHAN_B) — complementary lagging leg
 *
 *  If the hardware is a single-input driver (ZVS Royer, E-class, etc.),
 *  just wire PIN_PWM_OUT_A.  CH_B is gated by `enableComplementary()`.
 * ===========================================================================*/

#ifndef MELTER_POWER_CONTROLLER_H
#define MELTER_POWER_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

namespace Melter {
namespace Power {

struct Setpoint {
    uint32_t freq_hz;     // 20 k – 200 k
    uint8_t  duty_pct;    // 0 – 95
    uint16_t power_w;     // target output power (used by PID)
};

struct LiveReadings {
    uint32_t actual_freq_hz;
    uint8_t  actual_duty_pct;
    bool     output_enabled;
    bool     complementary;   // full-bridge mode?
};

/* Lifecycle */
void     begin();
void     end();

/* Configuration */
void     enableComplementary(bool on);
void     enableOutput(bool on);
bool     isOutputEnabled();

/* Setters — atomically swap the active setpoint.  They DO NOT touch
   the LEDC hardware if the output is disabled. */
void     setFrequency(uint32_t hz);
void     setDuty(uint8_t percent);
void     setPowerTarget(uint16_t watts);
void     setSetpoint(const Setpoint& sp);
Setpoint getSetpoint();

/* Atomic set */
void     apply();      // push current setpoint to the LEDC peripheral

/* Slow ramp helper — call from control task */
void     softStartTick(uint32_t elapsed_ms);

/* Hardware diagnostics */
LiveReadings live();
float        busVoltage();          // volts
float        primaryCurrent();       // amps rms approx
uint16_t     rawDutyCount();         // 0..2^LEDC_RES_BITS-1

}  // namespace Power
}  // namespace Melter

#endif
