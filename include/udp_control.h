#pragma once
#include <Arduino.h>
#include "config.h"

// Binary UDP control packet (22 bytes, little-endian, no padding).
//
// v2: the plain XOR checksum has been replaced with a truncated
// HMAC-SHA256 authentication tag keyed with UDP_AUTH_KEY (config.h).
// A checksum only catches accidental corruption - anyone who joined the
// WiFi could still forge a valid-checksum packet. An HMAC tag can't be
// forged without the key, so an attacker on the same WiFi network can no
// longer inject flight-control input just by being associated.
//
// seq is also widened to uint32_t and is now REQUIRED to strictly
// increase between accepted packets (wraparound-safe comparison) - this
// closes a replay hole where a captured, still-checksum-valid packet
// could otherwise be resent later.
//
// Byte layout:
//   [0]      magic     = UDP_MAGIC_BYTE
//   [1..4]   seq       uint32, must strictly increase, wraps at 2^32
//   [5..6]   roll      uint16, 1000-2000
//   [7..8]   pitch     uint16, 1000-2000
//   [9..10]  throttle  uint16, 1000-2000
//   [11..12] yaw       uint16, 1000-2000
//   [13]     auxMask   bit0..3 = aux1..aux4 (1 = ON)
//   [14..21] tag       HMAC-SHA256(UDP_AUTH_KEY, bytes[0..13]), truncated
//                       to UDP_AUTH_TAG_LEN (8) bytes
#pragma pack(push, 1)
struct ControlPacket {
  uint8_t  magic;
  uint32_t seq;
  uint16_t roll;
  uint16_t pitch;
  uint16_t throttle;
  uint16_t yaw;
  uint8_t  auxMask;
  uint8_t  tag[UDP_AUTH_TAG_LEN];
};
#pragma pack(pop)

// Number of bytes at the front of ControlPacket that are covered by the
// HMAC tag (everything except the tag field itself).
#define UDP_AUTH_COVERED_LEN (sizeof(ControlPacket) - UDP_AUTH_TAG_LEN)

void udpControlInit();

// Non-blocking - call this every control-loop tick. Reads at most one
// packet per call; drops (silently) anything malformed, unauthenticated,
// or replayed.
void udpControlPoll();

// True if a valid, authenticated packet has arrived within
// FAILSAFE_TIMEOUT_MS.
bool udpControlLinkFresh();

uint32_t udpControlLastSeq();
uint32_t udpControlPacketLossCount();

// Packets dropped for failing size/magic/auth-tag/replay checks - a
// nonzero and climbing count here (while otherwise looking "linked")
// is a sign something on your WiFi network is sending bad or forged
// packets and is worth investigating before you fly.
uint32_t udpControlRejectedCount();
