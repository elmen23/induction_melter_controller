/**
 * =============================================================================
 *  pid.h — Closed-loop power controller
 * =============================================================================
 *  The inverter does not have a direct "watts" knob — only frequency and
 *  duty.  The PID adjusts the duty cycle so the measured primary current
 *  × Vbus matches the requested power target.
 *
 *  P = I_primary * Vbus
 *  Setpoint = MELTER_POWER_DEFAULT_W (clamped)
 * ===========================================================================*/

#ifndef MELTER_PID_H
#define MELTER_PID_H

#include <Arduino.h>

namespace Melter {
namespace Pid {

void     begin();
void     setTunings(double kp, double ki, double kd);
void     setOutputLimits(double min, double max);
void     setSetpoint(double watts);
double   setpoint();
void     setEnabled(bool on);
bool     isEnabled();

/* 5 ms tick from the control task */
void     tick(double measured_watts, double dt_seconds);

double   output();           // 0..100 duty %
double   lastError();

}  // namespace Pid
}  // namespace Melter

#endif
