#include "tasks.h"
#include "config.h"
#include "channels.h"
#include "ibus.h"
#include "udp_control.h"
#include "web_server.h"
#include "failsafe.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ---------------------------------------------------------------------
// Control task: the only thing that matters for flight safety. Polls
// UDP, updates the failsafe check, and sends one iBUS frame every
// IBUS_FRAME_MS on a self-correcting schedule (vTaskDelayUntil), so
// jitter from the web task or WiFi stack can't drift the frame timing.
// ---------------------------------------------------------------------
static void controlTask(void *param) {
  (void)param;
  uint16_t snap[IBUS_CHANNELS];
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    udpControlPoll();

    if (failsafeIsLinkOk()) {
      channelsGetSnapshot(snap, IBUS_CHANNELS);
      ibusSendFrame(snap, IBUS_CHANNELS);
    }
    // else: deliberately withhold the iBUS frame. The flight controller
    // will see no valid iBUS frames and trigger its OWN RC-loss failsafe
    // (configure RC_FS_TIMEOUT / FS_THR_ENABLE etc. - see README).

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(IBUS_FRAME_MS));
  }
}

// ---------------------------------------------------------------------
// Web task: everything non-critical. Lower priority than the control
// task, so FreeRTOS always lets the control task run first/on-time,
// even on a single core.
// ---------------------------------------------------------------------
static void webTask(void *param) {
  (void)param;
  for (;;) {
    webServerPoll();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void tasksStart() {
#if defined(BOARD_HAS_DUAL_CORE) && BOARD_HAS_DUAL_CORE
  // ESP32-S3 (dual core): pin the control loop to core 1, away from the
  // WiFi/LWIP stack's internal work which mostly runs on core 0, and
  // put the web task on core 0 alongside it.
  xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(webTask,     "web",     4096, NULL, 1, NULL, 0);
#else
  // ESP32-C3 (single core RISC-V): only core 0 exists. Priority alone
  // provides the separation - the higher-priority control task preempts
  // the web task whenever it needs to run, so a slow HTTP request still
  // can't delay an iBUS frame.
  xTaskCreate(controlTask, "control", 4096, NULL, 3, NULL);
  xTaskCreate(webTask,     "web",     4096, NULL, 1, NULL);
#endif
}
