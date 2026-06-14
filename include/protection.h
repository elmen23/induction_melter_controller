/**
 * =============================================================================
 *  protection.h — Safety & fault manager
 * =============================================================================
 *  Runs in its own FreeRTOS task at the highest priority so it can latch
 *  faults BEFORE the PID or power driver get a chance to act on bad data.
 *
 *  Triggers:
 *     • over-current on primary
 *     • IGBT / heatsink over-temperature
 *     • coolant over-temperature
 *     • coolant flow loss
 *     • driver board fault pin
 *     • Vbus over- / under-voltage
 *     • sensor read fail (NaN, out of range)
 *
 *  Latches once tripped.  Cleared only by an explicit user action via
 *  the UI / web / serial command.
 * ===========================================================================*/

#ifndef MELTER_PROTECTION_H
#define MELTER_PROTECTION_H

#include "config.h"
#include <Arduino.h>

namespace Melter {
namespace Protection {

void     begin();
void     startTask();         // pin to core 1, high priority

void     arm();               // user presses START
void     disarm();            // user presses STOP
bool     isArmed();

/* Call from other modules to inject a fault */
void     raise(melter_fault_t f, const char* note = "");

/* Accessors */
melter_fault_t  lastFault();
const char*     lastNote();
bool            hasFault();
uint32_t        faultCount();

/* Latched readings used by the UI */
struct Snapshot {
    float    current_a;
    float    vbus_v;
    float    igbt_c;
    float    coolant_c;
    float    flow_lpm;
    bool     driver_fault_pin;
    uint32_t uptime_ms;
};
Snapshot snapshot();

/* Manual reset */
void     clearFault();

}  // namespace Protection
}  // namespace Melter

#endif
