/**
 * =============================================================================
 *  user_input.h — Physical START / STOP / E-STOP buttons + status LED
 * =============================================================================
 *  All non-safety UI lives in the web dashboard.  The hardware buttons are
 *  intentionally kept simple: START arming and STOP / E-STOP killing power
 *  immediately, regardless of what the WiFi stack is doing.
 * ===========================================================================*/

#ifndef MELTER_USER_INPUT_H
#define MELTER_USER_INPUT_H

#include <Arduino.h>
#include <stdint.h>

namespace Melter {
namespace Input {

void begin();
void tick();        // call from a low-priority task (≈ every 5 ms)

/* Button events — consumed once read */
enum class Btn : uint8_t {
    START,        // short press
    START_LONG,   // ≥ 1.5 s
    STOP,
    STOP_LONG,
    ESTOP_HARD,   // physical STOP line is hardware-wired for instant cut-off
    BTN_COUNT
};

bool        consume(Btn b);
const char* name(Btn b);

/* E-STOP can be latched in the firmware (e.g. after a watchdog).  Call
   `clearEStop()` from the web "Reset" action. */
bool     eStopLatched();
void     clearEStop();

/* Convenience wrappers wired to the buzzer / status LED */
void setStatusLed(bool on);
void beep(uint16_t ms = 80, uint16_t hz = 2500);

}  // namespace Input
}  // namespace Melter

#endif
