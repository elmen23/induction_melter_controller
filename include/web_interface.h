/**
 * =============================================================================
 *  web_interface.h — WiFi (AP+STA) + AsyncWebServer + WebSocket telemetry
 * =============================================================================
 *  Behaviour
 *  ---------
 *   • On first boot the device comes up in AP mode  ("Melter-XXXX", open).
 *   • The user joins the AP, opens http://192.168.4.1, picks an SSID.
 *   • Credentials are stored in NVS; on subsequent boots the device joins
 *     the saved network and serves the dashboard at the assigned IP.
 *   • AP fallback: if the STA link is lost for > 15 s, AP mode is re-enabled.
 *
 *  Two transports for telemetry:
 *     – WebSocket (/ws) pushes a JSON snapshot every WEB_REFRESH_MS.
 *     – REST API   (/api/…)  exposes every action as a one-shot request.
 *
 *  Basic-auth protects the API and the dashboard.
 * ===========================================================================*/

#ifndef MELTER_WEB_INTERFACE_H
#define MELTER_WEB_INTERFACE_H

#include <Arduino.h>
#include <stdint.h>

namespace Melter {
namespace Web {

void begin();          // loads files from LittleFS, starts server, AP+STA
void startTask();      // pumps WebSocket clients and reconnects WiFi

bool isStaConnected();
String ipAddress();
String macAddress();
int    clients();      // connected WS clients

}  // namespace Web
}  // namespace Melter

#endif
