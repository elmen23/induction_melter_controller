/**
 * web_interface.cpp
 *
 * WiFi + AP fallback + AsyncWebServer + WebSocket telemetry.
 * Tiny JSON is built with ArduinoJson for size and speed.
 */

#include "web_interface.h"
#include "config.h"
#include "logging.h"
#include "power_controller.h"
#include "protection.h"
#include "user_input.h"
#include "pid.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Update.h>

namespace Melter {
namespace Web {

/* ---- module state ---- */
static AsyncWebServer       g_server(WEB_DEFAULT_PORT);
static AsyncWebSocket       g_ws("/ws");
static DNSServer            g_dns;
static Preferences          g_prefs;
static bool                 g_sta_connected = false;
static uint32_t             g_sta_lost_at   = 0;
static uint8_t             *g_fault_ring[8] = {};
static uint8_t             g_fault_ring_idx = 0;

/* helper: send a 200 OK with JSON body */
static void json_ok(AsyncWebServerRequest* req, JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", body);
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
}
static void json_err(AsyncWebServerRequest* req, int code, const char* msg) {
    JsonDocument d;
    d["ok"] = false;
    d["error"] = msg;
    String body; serializeJson(d, body);
    req->send(code, "application/json", body);
}

/* -------- build full snapshot -------- */
static String build_snapshot_json() {
    JsonDocument doc;
    auto sp   = Power::getSetpoint();
    auto live = Power::live();
    auto pr   = Protection::snapshot();
    JsonObject root = doc.to<JsonObject>();

    root["ts"]            = millis();
    root["state"]         = state_to_string(
        Protection::hasFault() ? ST_FAULT :
        (Input::eStopLatched() ? ST_ESTOP :
         (Power::isOutputEnabled() ? ST_RUN : ST_IDLE)));
    root["freq_hz"]       = sp.freq_hz;
    root["duty_pct"]      = sp.duty_pct;
    root["power_w"]       = sp.power_w;
    root["freq_actual"]   = live.actual_freq_hz;
    root["duty_actual"]   = live.actual_duty_pct;
    root["current_a"]     = pr.current_a;
    root["vbus_v"]        = pr.vbus_v;
    root["igbt_c"]        = pr.igbt_c;
    root["coolant_c"]     = pr.coolant_c;
    root["flow_lpm"]      = pr.flow_lpm;
    root["driver_fault"]  = pr.driver_fault_pin;
    root["fault"]         = Protection::hasFault();
    root["fault_code"]    = (int)Protection::lastFault();
    root["fault_note"]    = Protection::lastNote() ? Protection::lastNote() : "";
    root["fault_count"]   = Protection::faultCount();
    root["armed"]         = Protection::isArmed();
    root["estop"]         = Input::eStopLatched();
    root["pid_on"]        = Pid::isEnabled();
    root["pid_out"]       = Pid::output();
    root["pid_err"]       = Pid::lastError();
    root["uptime"]        = pr.uptime_ms;
    root["rssi"]          = WiFi.RSSI();
    root["clients"]       = g_ws.count();
    root["free_heap"]     = ESP.getFreeHeap();
    root["version"]       = MELTER_FIRMWARE_VERSION;
    root["heap_min"]      = ESP.getMinFreeHeap();

    String out;
    serializeJson(doc, out);
    return out;
}

/* -------- REST handlers -------- */
static void api_status(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", build_snapshot_json());
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
}

static void api_set_freq(AsyncWebServerRequest* req) {
    if (!req->hasParam("v")) return json_err(req, 400, "missing v");
    uint32_t v = req->getParam("v")->value().toInt();
    Power::setFrequency(v);
    JsonDocument d; d["ok"] = true; d["freq_hz"] = Power::getSetpoint().freq_hz;
    json_ok(req, d);
}
static void api_set_duty(AsyncWebServerRequest* req) {
    if (!req->hasParam("v")) return json_err(req, 400, "missing v");
    int v = req->getParam("v")->value().toInt();
    Power::setDuty((uint8_t)constrain(v, 0, MELTER_DUTY_MAX_PCT));
    JsonDocument d; d["ok"] = true; d["duty_pct"] = Power::getSetpoint().duty_pct;
    json_ok(req, d);
}
static void api_set_power(AsyncWebServerRequest* req) {
    if (!req->hasParam("v")) return json_err(req, 400, "missing v");
    int v = req->getParam("v")->value().toInt();
    Power::setPowerTarget((uint16_t)constrain(v, 0, MELTER_POWER_MAX_W));
    JsonDocument d; d["ok"] = true; d["power_w"] = Power::getSetpoint().power_w;
    json_ok(req, d);
}
static void api_preset(AsyncWebServerRequest* req) {
    if (!req->hasParam("i")) return json_err(req, 400, "missing i");
    int i = req->getParam("i")->value().toInt();
    if (i < 0 || i >= (int)MELTER_PRESET_COUNT) return json_err(req, 404, "bad index");
    const auto& p = MELTER_PRESETS[i];
    Power::setFrequency(p.freq_hz);
    Power::setDuty(p.duty_pct);
    Power::setPowerTarget(p.power_w);
    JsonDocument d;
    d["ok"] = true;
    d["name"] = p.name;
    json_ok(req, d);
}
static void api_arm(AsyncWebServerRequest* req) {
    if (Input::eStopLatched()) return json_err(req, 423, "E-STOP latched");
    if (Protection::hasFault()) return json_err(req, 423, "fault active — clear first");
    Protection::arm();
    Power::enableOutput(true);
    digitalWrite(PIN_RELAY_MAIN, HIGH);
    Input::setStatusLed(true);
    JsonDocument d; d["ok"] = true; json_ok(req, d);
}
static void api_disarm(AsyncWebServerRequest* req) {
    Protection::disarm();
    Power::enableOutput(false);
    digitalWrite(PIN_RELAY_MAIN, LOW);
    Input::setStatusLed(false);
    JsonDocument d; d["ok"] = true; json_ok(req, d);
}
static void api_estop(AsyncWebServerRequest* req) {
    Protection::disarm();
    Power::enableOutput(false);
    digitalWrite(PIN_RELAY_MAIN, LOW);
    Input::setStatusLed(false);
    Input::clearEStop();
    JsonDocument d; d["ok"] = true; json_ok(req, d);
}
static void api_clear_fault(AsyncWebServerRequest* req) {
    Protection::clearFault();
    Input::clearEStop();
    JsonDocument d; d["ok"] = true; json_ok(req, d);
}
static void api_pid_enable(AsyncWebServerRequest* req) {
    if (!req->hasParam("v")) return json_err(req, 400, "missing v");
    Pid::setEnabled(req->getParam("v")->value() == "1");
    JsonDocument d; d["ok"] = true; d["on"] = Pid::isEnabled();
    json_ok(req, d);
}
static void api_pid_tune(AsyncWebServerRequest* req) {
    if (req->hasParam("kp")) Pid::setTunings(
            req->getParam("kp")->value().toFloat(),
            req->hasParam("ki") ? req->getParam("ki")->value().toFloat() : 0.5,
            req->hasParam("kd") ? req->getParam("kd")->value().toFloat() : 0.1);
    JsonDocument d; d["ok"] = true; json_ok(req, d);
}
static void api_presets(AsyncWebServerRequest* req) {
    JsonDocument d;
    JsonArray a = d["presets"].to<JsonArray>();
    for (uint8_t i = 0; i < MELTER_PRESET_COUNT; ++i) {
        JsonObject o = a.add<JsonObject>();
        o["i"]     = i;
        o["name"]  = MELTER_PRESETS[i].name;
        o["freq"]  = MELTER_PRESETS[i].freq_hz;
        o["duty"]  = MELTER_PRESETS[i].duty_pct;
        o["power"] = MELTER_PRESETS[i].power_w;
    }
    json_ok(req, d);
}
static void api_wifi_scan(AsyncWebServerRequest* req) {
    JsonDocument d;
    JsonArray a = d["aps"].to<JsonArray>();
    int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; ++i) {
        JsonObject o = a.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    json_ok(req, d);
}
static void api_wifi_save(AsyncWebServerRequest* req) {
    if (!req->hasParam("ssid")) return json_err(req, 400, "missing ssid");
    String ssid = req->getParam("ssid")->value();
    String pass = req->hasParam("pass") ? req->getParam("pass")->value() : "";
    g_prefs.begin("melter", false);
    g_prefs.putString(NVS_KEY_SSID, ssid);
    g_prefs.putString(NVS_KEY_PASS, pass);
    g_prefs.end();
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), pass.c_str());
    JsonDocument d; d["ok"] = true; json_ok(req, d);
}
static void api_reboot(AsyncWebServerRequest* req) {
    JsonDocument d; d["ok"] = true; json_ok(req, d);
    req->send(200, "application/json", "{\"ok\":true}");
    delay(250);
    ESP.restart();
}
static void api_ota(AsyncWebServerRequest* req, const String& filename,
                    size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
    if (final) {
        if (Update.end(true)) {
            LOG_I("OTA success, rebooting");
        } else {
            Update.printError(Serial);
        }
    }
}
static void api_factory(AsyncWebServerRequest* req) {
    g_prefs.begin("melter", false);
    g_prefs.clear();
    g_prefs.end();
    JsonDocument d; d["ok"] = true; json_ok(req, d);
    delay(150);
    ESP.restart();
}

/* -------- WebSocket events -------- */
static void on_ws_event(AsyncWebSocket* srv, AsyncWebSocketClient* cli,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        LOG_I("WS client #%u connected", cli->id());
        cli->text(build_snapshot_json());
    } else if (type == WS_EVT_DISCONNECT) {
        LOG_I("WS client #%u disconnected", cli->id());
    } else if (type == WS_EVT_DATA) {
        // optional inbound commands {"cmd":"arm"} etc.
        JsonDocument d;
        if (deserializeJson(d, (const char*)data, len)) return;
        const char* cmd = d["cmd"] | "";
        if (!strcmp(cmd, "arm"))    { Protection::arm(); Power::enableOutput(true); }
        else if (!strcmp(cmd, "disarm")) { Protection::disarm(); Power::enableOutput(false); }
        else if (!strcmp(cmd, "estop"))  { Protection::disarm(); Power::enableOutput(false); Input::clearEStop(); }
        else if (!strcmp(cmd, "clearFault")) { Protection::clearFault(); Input::clearEStop(); }
    }
}

/* -------- HTTP basic auth -------- */
static bool check_auth(AsyncWebServerRequest* req) {
    if (req->url() == "/api/wifi/scan") return true; // public
    if (!req->authenticate(MELTER_HTTP_USER, MELTER_HTTP_PASS)) {
        req->requestAuthentication();
        return false;
    }
    return true;
}

/* -------- AP mode static IP + DNS -------- */
static void start_ap() {
    String ap_name = String(MELTER_HOSTNAME_DEFAULT) + "-" +
                     String((uint32_t)ESP.getEfuseMac(), HEX).substring(0, 4);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ap_name.c_str());
    g_dns.start(53, "*", WiFi.softAPIP());
    LOG_I("AP started: %s @ %s", ap_name.c_str(), WiFi.softAPIP().toString().c_str());
}
static void start_sta() {
    g_prefs.begin("melter", true);
    String ssid = g_prefs.getString(NVS_KEY_SSID, "");
    String pass = g_prefs.getString(NVS_KEY_PASS, "");
    g_prefs.end();
    if (ssid.isEmpty()) { start_ap(); return; }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    LOG_I("STA connecting to %s …", ssid.c_str());
}

void begin() {
    Log::taskTag("web");

    if (!LittleFS.begin(true)) {
        LOG_E("LittleFS mount failed");
    }

    g_server.on("/", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!check_auth(r)) return;
        r->send(LittleFS, "/index.html", "text/html");
    });
    g_server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!check_auth(r)) return;
        r->send(LittleFS, "/style.css", "text/css");
    });
    g_server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!check_auth(r)) return;
        r->send(LittleFS, "/app.js", "application/javascript");
    });

    g_server.on("/api/status",   HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_status(r); });
    g_server.on("/api/set/freq", HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_set_freq(r); });
    g_server.on("/api/set/duty", HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_set_duty(r); });
    g_server.on("/api/set/power",HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_set_power(r); });
    g_server.on("/api/preset",   HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_preset(r); });
    g_server.on("/api/presets",  HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_presets(r); });
    g_server.on("/api/arm",      HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_arm(r); });
    g_server.on("/api/disarm",   HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_disarm(r); });
    g_server.on("/api/estop",    HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_estop(r); });
    g_server.on("/api/clear",    HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_clear_fault(r); });
    g_server.on("/api/pid/on",   HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_pid_enable(r); });
    g_server.on("/api/pid/tune", HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_pid_tune(r); });
    g_server.on("/api/wifi/scan",HTTP_GET, [](AsyncWebServerRequest* r){ api_wifi_scan(r); });
    g_server.on("/api/wifi/save",HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_wifi_save(r); });
    g_server.on("/api/reboot",   HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_reboot(r); });
    g_server.on("/api/factory",  HTTP_GET, [](AsyncWebServerRequest* r){ if(check_auth(r)) api_factory(r); });
    g_server.on("/api/ota",      HTTP_POST,
        [](AsyncWebServerRequest* r){
            if (!check_auth(r)) return;
            bool ok = !Update.hasError();
            r->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
            if (ok) delay(200), ESP.restart();
        },
        api_ota);

    g_ws.onEvent(on_ws_event);
    g_server.addHandler(&g_ws);
    g_server.begin();
    LOG_I("HTTP server on port %d", WEB_DEFAULT_PORT);

    start_sta();
}

void startTask() {
    xTaskCreatePinnedToCore(
        [](void*) {
            Log::taskTag("web");
            uint32_t last_ws = 0;
            uint32_t last_scan = 0;
            while (true) {
                uint32_t now = millis();

                /* AP mode DNS */
                if (WiFi.getMode() & WIFI_AP) g_dns.processNextRequest();

                /* STA link watchdog */
                bool sta = (WiFi.status() == WL_CONNECTED);
                if (sta != g_sta_connected) {
                    g_sta_connected = sta;
                    if (sta) {
                        LOG_I("STA got IP %s", WiFi.localIP().toString().c_str());
                        MDNS.begin(MELTER_HOSTNAME_DEFAULT);
                        MDNS.addService("http", "tcp", WEB_DEFAULT_PORT);
                        g_sta_lost_at = 0;
                    } else {
                        LOG_W("STA disconnected");
                        g_sta_lost_at = now;
                    }
                }
                /* Fall back to AP if STA stuck */
                if (!sta && g_sta_lost_at && (now - g_sta_lost_at) > 15000) {
                    LOG_W("Falling back to AP mode");
                    start_ap();
                    g_sta_lost_at = 0;
                }

                /* Push telemetry over WebSocket */
                if (g_ws.count() && (now - last_ws) >= WEB_REFRESH_MS) {
                    last_ws = now;
                    g_ws.textAll(build_snapshot_json());
                }
                g_ws.cleanup();

                /* LED / buzzer feedback for arm state */
                static uint32_t last_blink = 0;
                bool armed = Protection::isArmed() && !Protection::hasFault();
                if (armed) {
                    if (now - last_blink > 250) { last_blink = now;
                        Input::setStatusLed(!digitalRead(PIN_LED_STATUS));
                    }
                } else if (Protection::hasFault() || Input::eStopLatched()) {
                    if (now - last_blink > 120) { last_blink = now;
                        Input::setStatusLed(!digitalRead(PIN_LED_STATUS));
                    }
                } else {
                    Input::setStatusLed(false);
                }

                vTaskDelay(pdMS_TO_TICKS(20));
            }
        },
        "web", TASK_STACK_WEB, nullptr, TASK_PRIO_WEB, nullptr, 0);
}

bool isStaConnected() { return g_sta_connected; }
String ipAddress()     { return g_sta_connected ? WiFi.localIP().toString()
                                               : WiFi.softAPIP().toString(); }
String macAddress()    { return WiFi.macAddress(); }
int    clients()       { return g_ws.count(); }

}  // namespace Web
}  // namespace Melter
