---
"waveform-one": minor
---

Add the first native firmware target for the Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B display controller. Initialize its 1024 x 600
display and GT911 touch input through the pinned Waveshare BSP, provide the
native Now Playing and Settings interfaces, join the user's home Wi-Fi through
the ESP32-C6 radio, host the local phone remote and control the existing
ESP32-S3 LED engine over USB. Report the actual P4 silicon, flash and PSRAM
identity during startup.
