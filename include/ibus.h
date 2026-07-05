#pragma once
#include <Arduino.h>
#include "config.h"

void ibusInit();

// Builds and transmits one 32-byte iBUS frame from the given channel array.
void ibusSendFrame(const uint16_t *channels, size_t numChannels);
