#pragma once
#include <Arduino.h>

// =====================================================================
// WiFi Access Point
// =====================================================================
#define WIFI_SSID       "ESPiBUS RC"
#define WIFI_PASSWORD   "12345678"   // >= 8 chars. CHANGE THIS.
#define WIFI_CHANNEL    1                          // fixed channel, avoid auto-scan
#define WIFI_MAX_CONN   4                          // test value; tighten back to 1 once connection works

// =====================================================================
// UDP control link (binary protocol, see udp_control.h for the struct)
// =====================================================================
#define UDP_PORT        1234
#define UDP_MAGIC_BYTE  0xA5

// Every control packet must carry a valid HMAC-SHA256 tag (truncated to
// 8 bytes) computed with this key, or it is dropped before it ever
// reaches channelsSetStick(). This is what stops anyone who's merely
// joined the WiFi (or guessed/cracked the AP password) from injecting
// flight-control input - the AP password alone is NOT sufficient
// authentication for a control channel.
//
// >>> CHANGE THIS to your own random secret before flying. <<<
// Your phone-side sender / tools/test_udp_sender.py --key must use the
// exact same string.
#define UDP_AUTH_KEY  "k7mP2xQ9vL4nR8wT1sD6yH3zA5cF0jU"

// Length of the truncated HMAC tag appended to each packet, in bytes.
// 8 bytes (64 bits) is enough to make forgery over a rate-limited local
// link computationally infeasible while keeping packets small.
#define UDP_AUTH_TAG_LEN  8

// =====================================================================
// iBUS output
// =====================================================================
#define IBUS_BAUD       115200
#define IBUS_CHANNELS   14
#define IBUS_FRAME_MS   7      // ~142 Hz, standard iBUS cadence

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3 pin map - adjust to your wiring
  #define IBUS_TX_PIN   17
#elif defined(CONFIG_IDF_TARGET_ESP32)
  // ESP32 DevKit V1 (WROOM-32) pin map - GPIO17 is UART2 TX (hardware
  // UART, shares nothing with the USB/serial-monitor UART0) - adjust
  // to your wiring if you've routed it elsewhere.
  #define IBUS_TX_PIN   17
#else
  // ESP32-C3 pin map - fewer usable GPIOs, adjust to your wiring
  #define IBUS_TX_PIN   4
#endif

// =====================================================================
// Failsafe
// =====================================================================
// If no valid UDP control packet arrives within this window, the control
// task stops transmitting iBUS frames entirely so the flight controller's
// OWN RC-loss failsafe takes over (configure RC_FS_TIMEOUT / FS_THR_ENABLE
// etc. in ArduPilot - see README).
#define FAILSAFE_TIMEOUT_MS   400

// =====================================================================
// Channel mapping
// Matches ArduPilot's DEFAULT RCMAP (Roll, Pitch, Throttle, Yaw) so you
// don't need to touch RCMAP1..4 params on the flight controller.
// =====================================================================
#define CH_ROLL      0
#define CH_PITCH     1
#define CH_THROTTLE  2
#define CH_YAW       3
#define CH_AUX1      4
#define CH_AUX2      5
#define CH_AUX3      6
#define CH_AUX4      7

#define PWM_MIN   1000
#define PWM_MID   1500
#define PWM_MAX   2000

// =====================================================================
// Emergency stop
// =====================================================================
// When latched (via the web UI E-STOP button, or the /estop endpoint),
// throttle is forced to PWM_MIN in every outgoing iBUS frame, regardless
// of what the UDP control link is sending. Unlike a link failsafe, iBUS
// frames KEEP flowing while E-STOP is latched - this gives the flight
// controller an explicit, unambiguous zero-throttle command rather than
// an RC-loss condition, which is easier to reason about in the moment.
// It stays latched until explicitly cleared - it will not silently
// clear itself if the link recovers or a new packet arrives.
