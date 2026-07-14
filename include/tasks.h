#pragma once

// Creates and starts the control task and web task. On dual-core boards
// (BOARD_HAS_DUAL_CORE=1, e.g. ESP32-S3, ESP32 DevKit V1) they're pinned
// to separate cores. On single-core boards (e.g. ESP32-C3) they run on
// the one available core, separated by FreeRTOS priority instead.
void tasksStart();
