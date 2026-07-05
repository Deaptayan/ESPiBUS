#pragma once
#include <Arduino.h>
#include "config.h"

// Thread-safe store for the 14 iBUS channels. Written by the UDP-control
// task and the web-server task (AUX toggles), read by the control task
// when it builds an iBUS frame. Guarded internally with a critical section
// (portMUX) so it's safe to call from any task on either single- or
// dual-core builds.

void channelsInit();

// Set the 4 stick channels (values are clamped to PWM_MIN..PWM_MAX).
void channelsSetStick(uint16_t roll, uint16_t pitch, uint16_t throttle, uint16_t yaw);

// Set all 4 AUX channels at once from a bitmask (bit0..bit3 -> aux1..aux4).
void channelsSetAuxMask(uint8_t mask);

// Flip a single AUX channel (1..4). Used by the web UI toggle switches.
void channelsToggleAux(int auxNum);

// Read back whether a given AUX (1..4) is currently ON.
bool channelsGetAux(int auxNum);

// Copy the full 14-channel array out (thread-safe snapshot). If E-STOP
// is latched, the returned throttle value is forced to PWM_MIN
// regardless of what was last set via channelsSetStick().
void channelsGetSnapshot(uint16_t *outArray, size_t len);

// Emergency stop. When latched, every snapshot forces throttle to
// PWM_MIN, but frames keep being sent (see config.h for why). Stays
// latched until channelsSetEstop(false) is called explicitly - it does
// NOT auto-clear when the link recovers or new stick data arrives.
void channelsSetEstop(bool latched);
bool channelsIsEstop();
