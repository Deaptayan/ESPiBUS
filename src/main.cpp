#include <Arduino.h>
#include <esp_system.h>
#include "config.h"
#include "channels.h"
#include "ibus.h"
#include "wifi_ap.h"
#include "udp_control.h"
#include "web_server.h"
#include "failsafe.h"
#include "tasks.h"

// Human-readable reset reason, logged every boot. This is what lets you
// tell "I pressed reset" apart from "the board silently rebooted itself"
// (e.g. brownout, watchdog, panic) after the fact - the difference
// matters a lot when you're trying to figure out why a link glitched.
static const char *resetReasonString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external pin";
    case ESP_RST_SW:        return "software reset (esp_restart)";
    case ESP_RST_PANIC:     return "PANIC / crash";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
    case ESP_RST_BROWNOUT:  return "BROWNOUT (power supply couldn't keep up)";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "unknown";
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[Boot] ESP32 starting...");

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.print("[Boot] Reset reason: ");
  Serial.println(resetReasonString(reason));
  if (reason == ESP_RST_BROWNOUT || reason == ESP_RST_PANIC ||
      reason == ESP_RST_INT_WDT  || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
    Serial.println("[Boot][WARNING] Last reset was NOT a clean power-on/manual reset.");
    Serial.println("[Boot][WARNING] If this repeats, suspect power supply / brownout before flying.");
  }

  channelsInit();
  Serial.println("[Boot] channels ready");

  ibusInit();
  Serial.println("[Boot] iBUS UART ready");

  wifiApInit();
  Serial.println("[Boot] WiFi AP ready");

  udpControlInit();
  Serial.println("[Boot] UDP listener ready");

  webServerInit();
  Serial.println("[Boot] web server ready");

  failsafeInit();

  Serial.println("[System] Init complete.");
  tasksStart();
}

void loop() {
  // All real work happens in the FreeRTOS tasks started by tasksStart().
  // This also doubles as a crude "is the board alive at all" check: if
  // this stops printing, the whole chip (not just one task) is stuck.
  // It is NOT a substitute for checking real link health - use the web
  // status page / udpControlLinkFresh() for that.
  Serial.println("[Heartbeat] alive");
  vTaskDelay(pdMS_TO_TICKS(2000));
}
