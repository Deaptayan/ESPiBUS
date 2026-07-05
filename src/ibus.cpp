#include "ibus.h"

#define IBUS_SERIAL Serial1

void ibusInit() {
  // RX pin unused (-1): this is a one-way link to the flight controller.
  IBUS_SERIAL.begin(IBUS_BAUD, SERIAL_8N1, -1, IBUS_TX_PIN);
}

void ibusSendFrame(const uint16_t *channels, size_t numChannels) {
  uint8_t packet[32];
  packet[0] = 0x20;  // length byte
  packet[1] = 0x40;  // command byte

  for (size_t i = 0; i < IBUS_CHANNELS; i++) {
    uint16_t v = (i < numChannels) ? channels[i] : PWM_MID;
    packet[2 + i * 2]     = v & 0xFF;
    packet[2 + i * 2 + 1] = (v >> 8) & 0xFF;
  }

  uint16_t checksum = 0xFFFF;
  for (int i = 0; i < 30; i++) checksum -= packet[i];
  packet[30] = checksum & 0xFF;
  packet[31] = (checksum >> 8) & 0xFF;

  IBUS_SERIAL.write(packet, 32);
}
