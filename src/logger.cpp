/**
 * logger.cpp
 */
#include "logger.h"
#include "config.h"
#include "logging.h"
#include "power_controller.h"
#include "protection.h"
#include <SPI.h>
#include <SD.h>

namespace Melter {
namespace Logger {

static bool     g_ok = false;
static File     g_f;
static uint32_t g_bytes = 0;

void begin() {
    Log::taskTag("logger");
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, SPI, 20000000UL)) {
        SPI.end();
        LOG_W("no SD card — logging disabled");
        return;
    }
    g_ok = true;
    bool fresh = !SD.exists(LOG_FILE_PATH);
    g_f = SD.open(LOG_FILE_PATH, FILE_APPEND);
    if (!g_f) { g_ok = false; LOG_E("cannot open %s", LOG_FILE_PATH); return; }
    if (fresh) {
        g_f.println(LOG_HEADER);
        g_f.flush();
    }
    g_bytes = g_f.size();
    LOG_I("SD logger ready, %lu bytes written so far", g_bytes);
}

void startTask() {
    xTaskCreatePinnedToCore(
        [](void*) {
            Log::taskTag("logger");
            uint32_t last = 0;
            while (true) {
                if (g_ok && g_f) {
                    uint32_t now = millis();
                    if (now - last >= LOG_INTERVAL_MS) {
                        last = now;
                        auto sp   = Power::getSetpoint();
                        auto live = Power::live();
                        auto pr   = Protection::snapshot();
                        g_f.printf("%lu,%lu,%u,%u,%.2f,%.2f,%.1f,%.1f,%.2f,%s\r\n",
                            now,
                            live.actual_freq_hz,
                            live.actual_duty_pct,
                            sp.power_w,
                            pr.current_a, pr.vbus_v,
                            pr.igbt_c, pr.coolant_c, pr.flow_lpm,
                            state_to_string(Protection::hasFault() ? ST_FAULT
                                          : (Power::isOutputEnabled() ? ST_RUN : ST_IDLE)));
                        g_bytes = g_f.size();
                        g_f.flush();
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        },
        "logger", TASK_STACK_LOGGER, nullptr, TASK_PRIO_LOGGER, nullptr, 0);
}

bool isMounted()   { return g_ok; }
uint32_t bytesWritten() { return g_bytes; }

}  // namespace Logger
}  // namespace Melter
