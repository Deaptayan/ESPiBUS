#include "wifi_ap.h"
#include "config.h"
#include <WiFi.h>
#include <esp_wifi.h>

void wifiApInit() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, /*hidden=*/0, WIFI_MAX_CONN);

  // NOTE: esp_wifi_set_ps(WIFI_PS_NONE) temporarily removed for testing -
  // re-add once basic connection is confirmed working.

  Serial.print("[WiFi AP] SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("[WiFi AP] IP:   ");
  Serial.println(WiFi.softAPIP());
}
