# ADR 0001: Replace the Raspberry Pi display controller with ESP32-P4

**Status:** Accepted for incremental implementation  
**Date:** 2026-10-09

## Context

The working prototype uses an ESP32-S3 for microphone acquisition, FFT and
HUB75 refresh, plus a Raspberry Pi 4 for the touchscreen UI, recognition and
updates. The production direction replaces the Pi and its seven-inch display
with a Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.

The existing S3 signal path is working and has tighter real-time requirements
than the UI and network workloads. Moving it during the display migration
would make hardware failures harder to isolate.

## Decision

Use two controllers with a versioned USB boundary:

- the ESP32-S3 remains the audio, FFT and HUB75 LED engine;
- the ESP32-P4 drives its integrated display and touch controller, manages
  Wi-Fi, user interaction, artwork and product coordination;
- the P4 acts as USB host and communicates with the S3 using the existing WF1
  protocol, extended through backward-compatible commands where required;
- device OTA is implemented in native embedded code. Python is not part of
  the device update path;
- recognition may use an external service, but that dependency remains behind
  a small recognition interface and must meet the product latency and licensing
  requirements before selection.

## Alternatives considered

### Move the LED panel and microphone to ESP32-P4 immediately

This would reduce controller count, but combines display, network, touch,
audio capture and time-sensitive HUB75 refresh before the P4 hardware has been
characterized. It also discards a working subsystem. It may be reconsidered
for a later custom PCB after timing and power measurements.

### Keep the Raspberry Pi

This preserves the current software but keeps the cost, boot time, storage,
power and operating-system maintenance that motivated the hardware change.
The Pi may remain a temporary development tool, not a production dependency.

## Consequences

- The Pi browser UI is reimplemented as a native LVGL application; it cannot
  be copied directly to the P4.
- The S3 firmware remains usable while P4 work proceeds.
- USB host compatibility and power behavior must be proven on both boards.
- Recognition without a Pi needs either an embedded implementation or a
  remote service.
- The legacy Pi updater remains only until P4 update and recovery tests pass,
  after which its Python OTA code is removed.
