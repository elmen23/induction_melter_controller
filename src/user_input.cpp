/**
 * user_input.cpp
 */

#include "user_input.h"
#include "logging.h"
#include <Arduino.h>
#include <OneButton.h>

namespace Melter {
namespace Input {

static volatile bool g_estop   = false;
static volatile uint8_t g_evt[ (uint8_t)Btn::BTN_COUNT ] = {};

static OneButton btnStart(PIN_BTN_START, true, true);
static OneButton btnStop (PIN_BTN_STOP,  true, true);

static void on_evt(Btn b) { g_evt[(uint8_t)b] = 1; LOG_D("event %s", name(b)); }

void begin() {
    Log::taskTag("input");
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_BTN_STOP,  INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_BUZZER,     OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    btnStart.attachClick         ([](){ on_evt(Btn::START); });
    btnStart.attachLongPressStart([](){ on_evt(Btn::START_LONG); });

    btnStop.attachClick          ([](){
        g_estop = true;
        on_evt(Btn::STOP);
        on_evt(Btn::ESTOP_HARD);
    });
    btnStop.attachLongPressStart ([](){
        g_estop = true;
        on_evt(Btn::STOP_LONG);
        on_evt(Btn::ESTOP_HARD);
    });

    if (digitalRead(PIN_BTN_STOP) == LOW) {
        g_estop = true;
        on_evt(Btn::ESTOP_HARD);
    }
    LOG_I("physical input ready (START, STOP/E-STOP, LED, buzzer)");
}

void tick() { btnStart.tick(); btnStop.tick(); }

bool consume(Btn b) {
    uint8_t i = (uint8_t)b;
    if (i >= (uint8_t)Btn::BTN_COUNT) return false;
    bool r = g_evt[i];
    g_evt[i] = 0;
    return r;
}

const char* name(Btn b) {
    switch (b) {
        case Btn::START:      return "START";
        case Btn::START_LONG: return "START_LONG";
        case Btn::STOP:       return "STOP";
        case Btn::STOP_LONG:  return "STOP_LONG";
        case Btn::ESTOP_HARD: return "ESTOP_HARD";
        default:              return "?";
    }
}

bool eStopLatched() { return g_estop; }
void clearEStop()   { g_estop = false; LOG_I("E-stop cleared"); }

void setStatusLed(bool on) { digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW); }

void beep(uint16_t ms, uint16_t hz) {
    if (ms == 0) return;
    /* crude square-wave tone on a buzzer pin */
    uint32_t period_us = 1000000UL / hz / 2;
    uint32_t end = millis() + ms;
    while ((int32_t)(end - millis()) > 0) {
        digitalWrite(PIN_BUZZER, HIGH);
        delayMicroseconds(period_us);
        digitalWrite(PIN_BUZZER, LOW);
        delayMicroseconds(period_us);
    }
}

}  // namespace Input
}  // namespace Melter
