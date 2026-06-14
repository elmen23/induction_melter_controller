/**
 * pid.cpp
 */
#include "pid.h"
#include "config.h"
#include <PID_v1.h>

namespace Melter {
namespace Pid {

static double g_input    = 0.0;
static double g_output   = 0.0;
static double g_setpoint = 0.0;
static double g_last_err = 0.0;
static bool   g_on       = false;

static PID g_pid(&g_input, &g_output, &g_setpoint, 2.0, 0.5, 0.1, DIRECT);

void begin() {
    g_pid.SetMode(AUTOMATIC);
    g_pid.SetSampleTime(20);
    g_pid.SetOutputLimits(0, MELTER_DUTY_MAX_PCT);
}

void setTunings(double kp, double ki, double kd) { g_pid.SetTunings(kp, ki, kd); }
void setOutputLimits(double mn, double mx)        { g_pid.SetOutputLimits(mn, mx); }
void setSetpoint(double w)                        { g_setpoint = w; }
double setpoint()                                 { return g_setpoint; }
void setEnabled(bool on)                          { g_on = on; g_pid.SetMode(on ? AUTOMATIC : MANUAL); if (!on) g_output = 0; }
bool isEnabled()                                  { return g_on; }

void tick(double measured_w, double dt) {
    if (!g_on) return;
    g_input = measured_w;
    g_last_err = g_setpoint - measured_w;
    g_pid.Compute();
}
double output()    { return g_output; }
double lastError() { return g_last_err; }

}  // namespace Pid
}  // namespace Melter
