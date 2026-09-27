# Waveform One

[![CI and Release](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml/badge.svg)](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml)
[![Latest release](https://img.shields.io/github/v/release/tinyrobotcompany/Waveform_One)](https://github.com/tinyrobotcompany/Waveform_One/releases/latest)

A music-reactive LED display using an ESP32-S3, INMP441 microphone and 64×32
HUB75 panel. The current working prototype connects the panel directly to the ESP32.
The Raspberry Pi can select visual styles over USB using the Rust controller.

- [Wiring, firmware programs and build instructions](firmware/esp32/README.md)
- [Contributing and local tests](CONTRIBUTING.md)
- [Release notes](CHANGELOG.md)
- [Over-the-air updates: development and production](pi/updater/README.md)
- [CI and releases](docs/delivery.md)
- [Pi setup and visual-style controls](pi/README.md)

Signed releases support Pi application and ESP firmware updates on commissioned devices.
Development devices install automatically; production devices ask the user to install.
Existing factory-layout ESP boards need a one-time USB migration before OTA updates.
