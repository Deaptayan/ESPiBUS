#include "failsafe.h"
#include "udp_control.h"
#include <WiFi.h>

void failsafeInit() {
  // Nothing to initialize yet; kept as its own module so failsafe logic
  // (e.g. adding a "safe frame" mode instead of frame-withholding) can
  // grow here without touching other modules.
}

bool failsafeIsLinkOk() {
  if (WiFi.softAPgetStationNum() == 0) return false;  // phone not connected
  if (!udpControlLinkFresh()) return false;            // stick data stale
  return true;
}
