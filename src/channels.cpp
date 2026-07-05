#include "channels.h"

static uint16_t channels[IBUS_CHANNELS];
static bool auxState[4] = { false, false, false, false };
static volatile bool estopLatched = false;
static portMUX_TYPE muxLock = portMUX_INITIALIZER_UNLOCKED;

void channelsInit() {
  portENTER_CRITICAL(&muxLock);
  for (int i = 0; i < IBUS_CHANNELS; i++) channels[i] = PWM_MID;
  channels[CH_THROTTLE] = PWM_MIN;   // always boot at zero throttle
  for (int i = 0; i < 4; i++) {
    auxState[i] = false;
    channels[CH_AUX1 + i] = PWM_MIN;
  }
  portEXIT_CRITICAL(&muxLock);
}

void channelsSetStick(uint16_t roll, uint16_t pitch, uint16_t throttle, uint16_t yaw) {
  roll     = constrain(roll,     PWM_MIN, PWM_MAX);
  pitch    = constrain(pitch,    PWM_MIN, PWM_MAX);
  throttle = constrain(throttle, PWM_MIN, PWM_MAX);
  yaw      = constrain(yaw,      PWM_MIN, PWM_MAX);

  portENTER_CRITICAL(&muxLock);
  channels[CH_ROLL]     = roll;
  channels[CH_PITCH]    = pitch;
  channels[CH_THROTTLE] = throttle;
  channels[CH_YAW]      = yaw;
  portEXIT_CRITICAL(&muxLock);
}

void channelsSetAuxMask(uint8_t mask) {
  portENTER_CRITICAL(&muxLock);
  for (int i = 0; i < 4; i++) {
    bool state = mask & (1 << i);
    auxState[i] = state;
    channels[CH_AUX1 + i] = state ? PWM_MAX : PWM_MIN;
  }
  portEXIT_CRITICAL(&muxLock);
}

void channelsToggleAux(int auxNum) {
  if (auxNum < 1 || auxNum > 4) return;
  portENTER_CRITICAL(&muxLock);
  auxState[auxNum - 1] = !auxState[auxNum - 1];
  channels[CH_AUX1 + auxNum - 1] = auxState[auxNum - 1] ? PWM_MAX : PWM_MIN;
  portEXIT_CRITICAL(&muxLock);
}

bool channelsGetAux(int auxNum) {
  if (auxNum < 1 || auxNum > 4) return false;
  bool v;
  portENTER_CRITICAL(&muxLock);
  v = auxState[auxNum - 1];
  portEXIT_CRITICAL(&muxLock);
  return v;
}

void channelsGetSnapshot(uint16_t *outArray, size_t len) {
  portENTER_CRITICAL(&muxLock);
  for (size_t i = 0; i < len && i < IBUS_CHANNELS; i++) outArray[i] = channels[i];
  bool estop = estopLatched;
  portEXIT_CRITICAL(&muxLock);

  // Enforced outside the critical section (on the copy) so this can't
  // ever be skipped by a future edit that adds an early return above.
  if (estop && len > CH_THROTTLE) outArray[CH_THROTTLE] = PWM_MIN;
}

void channelsSetEstop(bool latched) {
  portENTER_CRITICAL(&muxLock);
  estopLatched = latched;
  portEXIT_CRITICAL(&muxLock);
  Serial.println(latched ? "[E-STOP] LATCHED - throttle forced to PWM_MIN"
                          : "[E-STOP] cleared");
}

bool channelsIsEstop() {
  portENTER_CRITICAL(&muxLock);
  bool v = estopLatched;
  portEXIT_CRITICAL(&muxLock);
  return v;
}
