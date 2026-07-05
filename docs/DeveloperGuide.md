# ESP32 iBUS Bridge (S3 dual-core / C3 single-core)

WiFi-to-iBUS bridge for ArduPilot on an STM32H743VIT6 FC. A phone connects to
the ESP32's WiFi AP and sends stick/AUX data over UDP; the ESP32 converts
that into a standard FlySky iBUS serial stream for the FC's RC input.

## Project layout

```
include/        headers for each module
src/
  main.cpp      wiring only - setup() calls each module's init, then starts tasks
  channels.cpp  thread-safe shared channel state (14 x uint16)
  ibus.cpp      builds + sends the 32-byte iBUS frame
  udp_control.cpp   parses the binary UDP control packet
  wifi_ap.cpp   softAP setup
  web_server.cpp    AUX toggle UI + JSON status endpoint (static PROGMEM page)
  failsafe.cpp  single source of truth for "is it safe to transmit iBUS?"
  tasks.cpp     creates the control task + web task (core-aware, see below)
tools/
  test_udp_sender.py   bench-test the ESP32 side without a phone app
```

## Building

```
pio run -e esp32s3 -t upload -t monitor   # for an ESP32-S3 board
pio run -e esp32c3 -t upload -t monitor   # for an ESP32-C3 board
```

The only difference between the two environments is one build flag,
`BOARD_HAS_DUAL_CORE`, set in `platformio.ini`. Everything else is shared code.

## How the core split works

- **S3 (dual core):** `tasksStart()` pins the control task to core 1 and the
  web task to core 0, via `xTaskCreatePinnedToCore`.
- **C3 (single core RISC-V):** only core 0 exists, so both tasks run there.
  Separation comes from FreeRTOS **priority** instead: the control task
  (priority 3) always preempts the web task (priority 1), so a slow HTTP
  request still can't delay an iBUS frame.

Either way, the control task runs on a fixed `vTaskDelayUntil` schedule, not
`delay()`, so its timing doesn't drift even if `webServerPoll()` briefly runs
long.

## Wiring

- `IBUS_TX_PIN` (see `config.h`, defaults: GPIO17 on S3, GPIO4 on C3) →
  the FC's RC/iBUS input pin.
- **Common ground between the ESP32 and the FC is required.**
- iBUS is a plain (non-inverted) UART signal at 115200 baud, so no signal
  inverter is needed — unlike SBUS.
- ESP32 GPIOs are 3.3V logic; most FC RC inputs are 3.3V-tolerant, but check
  your specific FC's datasheet before connecting.

## ArduPilot (STM32H743) configuration

On the UART you connected `IBUS_TX_PIN` to:
- `SERIALx_PROTOCOL = 23` (RCIN) if using a spare UART for RC input, **or**
  wire it into the FC's dedicated RCIN pin if it has one (protocol is
  auto-detected there).
- `RC_PROTOCOLS` — enable the IBUS bit.
- `RC_FS_TIMEOUT` — how long (seconds) with no valid RC frames before the FC
  declares RC failsafe. This is what actually protects you when the ESP32
  stops transmitting (see Failsafe below) — set it deliberately, don't leave
  it at a value you haven't checked.
- `FS_THR_ENABLE` / `FS_OPTIONS` — what the FC *does* on RC failsafe (RTL,
  Land, etc.). Set this to something sane for your vehicle before flying.
- Default `RCMAP1..4` (Roll, Pitch, Throttle, Yaw) is used as-is — this
  firmware's channel order already matches it, so you shouldn't need to
  touch RCMAP.

**Test this on the bench, disarmed, props off, before ever connecting a
battery to a propped aircraft.**

## UDP control packet format (v2 - authenticated)

Binary, 22 bytes, little-endian, no padding — this is what your phone app
needs to send to `UDP_PORT` (default 1234) on the ESP32's AP address
(default `192.168.4.1`):

| Bytes  | Field    | Type    | Notes                                             |
|--------|----------|---------|----------------------------------------------------|
| 0      | magic    | uint8   | must equal `0xA5`                                  |
| 1-4    | seq      | uint32  | must strictly increase every packet (anti-replay)  |
| 5-6    | roll     | uint16  | 1000-2000                                          |
| 7-8    | pitch    | uint16  | 1000-2000                                          |
| 9-10   | throttle | uint16  | 1000-2000                                          |
| 11-12  | yaw      | uint16  | 1000-2000                                          |
| 13     | auxMask  | uint8   | bit0..3 = aux1..aux4 (1 = ON)                      |
| 14-21  | tag      | 8 bytes | `HMAC-SHA256(UDP_AUTH_KEY, bytes[0..13])[:8]`       |

**`UDP_AUTH_KEY` (in `config.h`) must be changed from its placeholder value
before flying**, and your phone-side sender must sign packets with the exact
same key. Without a matching key, every packet is silently rejected — the
firmware logs a loud warning at boot if the placeholder key is still set.

Any packet that's the wrong size, has the wrong magic byte, fails HMAC
verification, or isn't a strictly-increasing sequence number (replay) is
silently dropped — it will never reach the flight controller as garbage or
spoofed RC input. The `/status` endpoint's `rejected` counter tracks how many
packets have failed one of these checks; if it's climbing while you're not
actively testing malformed input, something on your WiFi network is sending
bad or forged packets.

`tools/test_udp_sender.py` builds this exact authenticated packet, so you can
validate the whole chain from a laptop before writing (or instead of writing)
a phone app — pass the same key you put in `config.h`:

```
python3 tools/test_udp_sender.py --key "your-secret-here" --throttle 1000
```

### On the phone app itself

This repo only covers the ESP32 firmware. For the phone side, you have two
practical options:
1. Write a small native/Flutter/etc. app that reads two virtual joysticks
   and sends this packet format over UDP at ~50 Hz.
2. Use an existing generic "UDP gamepad/joystick" app and adjust
   `udp_control.cpp` to match whatever packet format it actually sends
   (many use a different layout — check its docs first).

## Failsafe behavior

The control task only transmits an iBUS frame when **both** are true:
- a client is connected to the AP (`WiFi.softAPgetStationNum() > 0`), and
- a valid control packet arrived within `FAILSAFE_TIMEOUT_MS` (400ms default).

If either condition fails, the ESP32 **stops sending iBUS frames entirely**
rather than sending a fabricated "safe" value. This is deliberate: it lets
the flight controller's own, already-tested RC-loss failsafe (`RC_FS_TIMEOUT`
etc.) take over, instead of ArduPilot treating a fake low-throttle frame as
genuine (if unusual) pilot input.

## Known limitations / things worth adding next

- **This is still fundamentally a WiFi/UDP link: no frequency hopping, a
  fixed single channel, and consumer-WiFi range/latency/jitter — nowhere
  near the reliability of a proper RC system (FrSky/Crossfire/etc.) built
  for this job. Treat it as a bench/test link. It should not be the sole,
  primary control link for an actual propped, flying aircraft**, regardless
  of the software hardening below — that's a physical-layer limitation no
  amount of code can fix.
- No telemetry back to the phone beyond stick echo/aux/packet-loss/rejected —
  battery voltage or RSSI would be useful additions to `handleStatus()`.
- No rate-of-change limiting on stick inputs — a single valid-but-wild
  packet from a buggy phone app can still produce an instantaneous
  full-range jump.
- No independent hardware watchdog — a firmware hang that isn't caught by
  FreeRTOS's own watchdogs could stop iBUS output without necessarily
  triggering a logged reset.

## Safety features (added after initial review)

- **Authenticated control packets.** UDP packets now carry an HMAC-SHA256
  tag keyed with `UDP_AUTH_KEY` (see the packet format section above) instead
  of a plain checksum, and sequence numbers must strictly increase. Together
  these stop anyone merely associated to the WiFi from injecting or
  replaying control input — but only if you actually change `UDP_AUTH_KEY`
  from its placeholder value first.
- **Emergency stop.** The web UI has an E-STOP button (`/estop?set=1` to
  latch, `/estop?set=0` to clear) that forces throttle to `PWM_MIN` in every
  outgoing iBUS frame while latched, independent of the UDP link. Frames
  keep flowing during E-STOP (rather than going silent like a link
  failsafe) so the flight controller sees an explicit zero-throttle command.
  It does not auto-clear.
- **Reset-reason logging.** Every boot logs *why* the chip last reset
  (power-on, brownout, panic, watchdog, etc.) via `esp_reset_reason()`, with
  a loud warning if it wasn't a clean power-on/manual reset. This is what
  would have immediately surfaced the brownout-suspect symptoms this project
  hit during initial bring-up, instead of requiring a multi-step debugging
  session.
