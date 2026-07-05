# ESPiBUS-RC

> **An open-source ESP32 Wi-Fi RC receiver with native FlySky iBUS output for any iBUS-compatible flight controller.**

ESPiBUS-RC transforms an ESP32 into a low-latency Wi-Fi remote control receiver. It receives RC commands from a mobile device over Wi-Fi and outputs a standard FlySky iBUS signal, allowing it to work with any flight controller that supports iBUS input.

The project was designed with reliability, modularity, and future expansion in mind, making it suitable as a companion communication module for custom flight controllers and robotics projects.

> **Current Status:** Active Development (Stable for bench testing)

---

# Features

* Native FlySky iBUS output
* Compatible with any iBUS-enabled flight controller
* ESP32-S3 and ESP32-C3 support
* PlatformIO project
* FreeRTOS-based architecture
* Deterministic control loop using `vTaskDelayUntil()`
* Binary UDP control protocol
* Packet authentication (HMAC-SHA256)
* Packet sequence validation
* Packet loss detection
* Modular codebase
* Built-in failsafe handling
* Lightweight web interface
* Designed for future telemetry support

---

# Planned Features

* Flight telemetry
* Android companion application
* OTA firmware updates
* Web-based configuration
* Wi-Fi diagnostics
* Link quality monitoring
* Optical flow companion module

---

# Project Structure

```text
include/
    config.h
    channels.h
    failsafe.h
    ibus.h
    tasks.h
    udp_control.h
    web_server.h
    wifi_ap.h

src/
    main.cpp
    channels.cpp
    failsafe.cpp
    ibus.cpp
    tasks.cpp
    udp_control.cpp
    web_server.cpp
    wifi_ap.cpp

tools/
    test_udp_sender.py
```

---

# Supported Hardware

## Microcontrollers

* ESP32-S3
* ESP32-C3

## Flight Controllers

Any flight controller with FlySky iBUS support, including firmware such as:

* ArduPilot
* Betaflight
* INAV
* PX4 (configured for iBUS input)

---

# Wiring

```
ESP32                Flight Controller

TX  ------------->   iBUS / RCIN

GND ------------->   GND
```

No inverter is required since FlySky iBUS uses a standard UART signal.

---

# Building

```bash
pio run -e esp32s3 -t upload -t monitor
```

or

```bash
pio run -e esp32c3 -t upload -t monitor
```

---

# Design Philosophy

ESPiBUS-RC is built around a modular architecture instead of a single Arduino sketch.

Core modules include:

* Wi-Fi Access Point
* UDP Receiver
* Thread-safe RC Channel Manager
* iBUS Generator
* Failsafe Manager
* Web Interface
* FreeRTOS Task Scheduler

The control task runs with deterministic timing using FreeRTOS, ensuring that web requests or other non-critical operations cannot interfere with RC signal generation.

---

# Safety

This project is intended for development and research.

Before flying:

* Verify all channel mappings.
* Configure RC failsafe correctly in your flight controller.
* Perform all initial testing without propellers.
* Verify failsafe behaviour before every flight.

The authors are not responsible for damage or injury resulting from improper use.

---

# Acknowledgements

This project was originally inspired by Boyyt357's **ESP32-WiFi-Joystick-to-iBUS-Translator**.

While it began as a reference during development, the current implementation has evolved into an independent codebase featuring a modular architecture, FreeRTOS task scheduling, authenticated binary communication, enhanced failsafe handling, improved security, and numerous structural and performance improvements.

---

# License

See the LICENSE file for details.
