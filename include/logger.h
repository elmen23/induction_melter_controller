/**
 * =============================================================================
 *  logger.h — CSV data logger (SD card, optional)
 * =============================================================================
 *  Periodically appends a row with all live readings to /melter.csv.
 *  Header is written the first time the file is created.
 *  Gracefully no-ops if no SD card is present (most small induction
 *  melters run without one).
 * ===========================================================================*/

#ifndef MELTER_LOGGER_H
#define MELTER_LOGGER_H

#include <Arduino.h>

namespace Melter {
namespace Logger {

void begin();         // initialise SD + open log file
void startTask();     // periodic write

bool     isMounted();
uint32_t bytesWritten();

}  // namespace Logger
}  // namespace Melter

#endif
