#pragma once
#include <Arduino.h>

void failsafeInit();

// True only when it's safe to transmit iBUS: a phone is actually
// connected to the AP AND a fresh control packet has been received.
// When false, the control task withholds iBUS frames on purpose so the
// flight controller's own RC-loss failsafe takes over.
bool failsafeIsLinkOk();
