/**
 * =============================================================================
 *  logging.h — Minimal log facade over Serial with level gating
 * =============================================================================
 *  Use LOG_I("msg %d", x) anywhere.  All macros compile out below LOG_LEVEL.
 * ===========================================================================*/

#ifndef MELTER_LOGGING_H
#define MELTER_LOGGING_H

#include <Arduino.h>
#include "config.h"

#if LOG_LEVEL >= 1
#  define LOG_E(fmt, ...) ::Melter::Log::emit('E', __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#  define LOG_E(fmt, ...) do {} while (0)
#endif

#if LOG_LEVEL >= 2
#  define LOG_I(fmt, ...) ::Melter::Log::emit('I', __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#  define LOG_I(fmt, ...) do {} while (0)
#endif

#if LOG_LEVEL >= 3
#  define LOG_D(fmt, ...) ::Melter::Log::emit('D', __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#  define LOG_D(fmt, ...) do {} while (0)
#endif

#if LOG_LEVEL >= 4
#  define LOG_V(fmt, ...) ::Melter::Log::emit('V', __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#  define LOG_V(fmt, ...) do {} while (0)
#endif

namespace Melter {
namespace Log {

void begin(unsigned long baud = 115200);
void emit(char level, const char* file, int line, const char* fmt, ...);
void taskTag(const char* tag);   // sets a per-task label shown in brackets

}  // namespace Log
}  // namespace Melter

#endif
