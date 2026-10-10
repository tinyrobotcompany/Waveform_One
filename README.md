# Waveform One

[![CI and Release](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml/badge.svg)](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml)
[![Latest release](https://img.shields.io/github/v/release/tinyrobotcompany/Waveform_One)](https://github.com/tinyrobotcompany/Waveform_One/releases/latest)

A music-reactive LED display using an ESP32-S3, INMP441 microphone and 64×32
HUB75 panel. The current working prototype connects the panel directly to the
ESP32-S3. The ESP32-P4 touchscreen controller selects visual styles over USB,
joins the user's home Wi-Fi and hosts the local phone remote.

Migration to the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B display controller is
under development. Display, touch, home Wi-Fi setup, the phone remote and S3
style control are working; music recognition is the next milestone. The working
ESP32-S3 audio and HUB75 engine remains intact during this migration.

- [Wiring, firmware programs and build instructions](firmware/esp32/README.md)
- [ESP32-P4 display bring-up and wiring](firmware/esp32/p4_display/README.md)
- [ESP32-P4 controller architecture decision](docs/architecture/0001-esp32-p4-display-controller.md)
- [Contributing and local tests](CONTRIBUTING.md)
- [Release notes](CHANGELOG.md)
- [Over-the-air updates: development and production](pi/updater/README.md)
- [CI and releases](docs/delivery.md)
- [Pi setup and visual-style controls](pi/README.md)

Signed releases support Pi application and ESP firmware updates on commissioned devices.
Development devices install automatically; production devices ask the user to install.
Existing factory-layout ESP boards need a one-time USB migration before OTA updates.
