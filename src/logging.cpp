/**
 * logging.cpp
 */
#include "logging.h"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace Melter {
namespace Log {

static const char* g_tag = "main";

void begin(unsigned long baud) {
    Serial.begin(baud);
    // wait briefly for the USB-CDC to attach on boards with native USB
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 1500) { delay(10); }
}

void taskTag(const char* tag) {
    g_tag = (tag && *tag) ? tag : "main";
}

void emit(char level, const char* file, int line, const char* fmt, ...) {
    static const char* lvl[] = { "?", "E", "I", "D", "V" };
    int idx = (level == 'E') ? 1 : (level == 'I') ? 2 : (level == 'D') ? 3
              : (level == 'V') ? 4 : 0;
    char msgbuf[256];

    // --- format user message
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);

    // --- shorten file path
    const char* base = strrchr(file, '/');
    base = base ? base + 1 : file;

    Serial.printf("[%7lu] [%s] [%-7s] %-20s:%-4d  %s\r\n",
                  millis(), lvl[idx], g_tag, base, line, msgbuf);
}

}  // namespace Log
}  // namespace Melter
