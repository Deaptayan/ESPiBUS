#include "udp_control.h"
#include "channels.h"
#include <WiFiUdp.h>
#include <mbedtls/md.h>
#include <string.h>

static WiFiUDP udp;
static volatile uint32_t lastPacketMillis = 0;
static volatile uint32_t lastSeq = 0;
static volatile uint32_t lostPackets = 0;
static volatile uint32_t rejectedPackets = 0;
static bool haveFirstPacket = false;

// Constant-time compare, so a timing side-channel can't leak how many
// leading bytes of a forged tag happened to match.
static bool constantTimeEqual(const uint8_t *a, const uint8_t *b, size_t len) {
  uint8_t diff = 0;
  for (size_t i = 0; i < len; i++) diff |= (a[i] ^ b[i]);
  return diff == 0;
}

// Computes HMAC-SHA256(UDP_AUTH_KEY, data) and writes the first
// UDP_AUTH_TAG_LEN bytes of it into out.
static void computeAuthTag(const uint8_t *data, size_t len, uint8_t *out) {
  uint8_t fullTag[32];  // SHA-256 output size
  const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(mdInfo,
                   (const uint8_t *)UDP_AUTH_KEY, strlen(UDP_AUTH_KEY),
                   data, len,
                   fullTag);
  memcpy(out, fullTag, UDP_AUTH_TAG_LEN);
}

void udpControlInit() {
  udp.begin(UDP_PORT);

  // Loudly warn at boot if the default placeholder key is still in use -
  // this is exactly the kind of thing that's easy to forget before a
  // flight, and it silently defeats the whole authentication scheme.
  if (strcmp(UDP_AUTH_KEY, "REPLACE_ME_WITH_A_LONG_RANDOM_SECRET_BEFORE_FLYING") == 0) {
    Serial.println("[UDP][WARNING] UDP_AUTH_KEY is still the default placeholder!");
    Serial.println("[UDP][WARNING] Anyone on this WiFi can forge control packets.");
    Serial.println("[UDP][WARNING] Set a real random UDP_AUTH_KEY in config.h before flying.");
  }
}

void udpControlPoll() {
  // If we've gone quiet for a while, the flight controller's own
  // RC-loss failsafe (see failsafe.cpp) has already taken over, so
  // there's no live command stream left to "replay" against. Reset the
  // sequence guard so a controller app that restarted (and therefore
  // resets its own seq counter to 0) can reconnect without needing the
  // ESP32 power-cycled. This does NOT reopen a real replay window: an
  // attacker's captured-old-packet only becomes acceptable again after
  // the same multi-second silence a genuine reconnect requires, and by
  // then the FC is already in its own failsafe state anyway.
  if (haveFirstPacket && (millis() - lastPacketMillis) >= FAILSAFE_TIMEOUT_MS) {
    haveFirstPacket = false;
  }

  int packetSize = udp.parsePacket();
  if (packetSize <= 0) return;

  if (packetSize != sizeof(ControlPacket)) {
    udp.flush();   // wrong size -> not our protocol / corrupted, drop it
    rejectedPackets++;
    return;
  }

  ControlPacket pkt;
  int len = udp.read((uint8_t *)&pkt, sizeof(pkt));
  if (len != sizeof(pkt)) { rejectedPackets++; return; }

  if (pkt.magic != UDP_MAGIC_BYTE) { rejectedPackets++; return; }

  uint8_t expectedTag[UDP_AUTH_TAG_LEN];
  computeAuthTag((const uint8_t *)&pkt, UDP_AUTH_COVERED_LEN, expectedTag);
  if (!constantTimeEqual(expectedTag, pkt.tag, UDP_AUTH_TAG_LEN)) {
    rejectedPackets++;   // wrong key / forged / corrupted - never trust it
    return;
  }

  // Anti-replay: seq must strictly increase (wraparound-safe signed
  // comparison), or this packet is either a duplicate or an old one
  // being replayed - either way, ignore it.
  if (haveFirstPacket && (int32_t)(pkt.seq - lastSeq) <= 0) {
    rejectedPackets++;
    return;
  }

  if (haveFirstPacket) {
    uint32_t expectedSeq = lastSeq + 1;
    if (pkt.seq != expectedSeq) {
      lostPackets += (pkt.seq - expectedSeq);
    }
  }
  lastSeq = pkt.seq;
  haveFirstPacket = true;
  lastPacketMillis = millis();

  channelsSetStick(pkt.roll, pkt.pitch, pkt.throttle, pkt.yaw);
  channelsSetAuxMask(pkt.auxMask);
}

bool udpControlLinkFresh() {
  if (!haveFirstPacket) return false;
  return (millis() - lastPacketMillis) < FAILSAFE_TIMEOUT_MS;
}

uint32_t udpControlLastSeq() { return lastSeq; }
uint32_t udpControlPacketLossCount() { return lostPackets; }
uint32_t udpControlRejectedCount() { return rejectedPackets; }