# Waveform One

[![CI and Release](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml/badge.svg)](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml)
[![Latest release](https://img.shields.io/github/v/release/tinyrobotcompany/Waveform_One)](https://github.com/tinyrobotcompany/Waveform_One/releases/latest)

A music-reactive 64×32 HUB75 LED display with a touchscreen and local phone
remote. The current P4 configuration uses the Waveshare
ESP32-P4-WIFI6-Touch-LCD-7B, onboard ES7210 audio capture and direct P4 LED drive.
It joins home Wi-Fi and uses ACRCloud for music identification. Legacy S3/USB
and Pi configurations remain in the repository; their behavior and dependencies
must not be confused with the current P4 product.

Public source and local audio processing are foundations for long-term ownership.
Cloud song identification still requires ACRCloud. A repeatable recognition
accuracy benchmark and a documented cold-boot offline demonstration are pending;
see the evidence and acceptance plans below rather than assuming a measured
accuracy percentage or a complete offline guarantee.

- [Wiring, firmware programs and build instructions](firmware/esp32/README.md)
- [ESP32-P4 display bring-up and wiring](firmware/esp32/p4_display/README.md)
- [ESP32-P4 controller architecture decision](docs/architecture/0001-esp32-p4-display-controller.md)
- [Contributing and local tests](CONTRIBUTING.md)
- [Release notes](CHANGELOG.md)
- [Native P4 over-the-air updates](docs/p4-ota.md)
- [Recognition accuracy protocol and result template](docs/validation/recognition-protocol.md)
- [Recognition providers and commercial-cost review](docs/product/recognition-commercial-review.md)
- [Offline visualizer acceptance plan](docs/validation/offline-visualizer.md)
- [Ownership and service continuity](docs/product/longevity.md)
- [Buyer-readiness checklist](https://github.com/tinyrobotcompany/Waveform_One/issues/25)
- [Legacy Pi over-the-air updates](pi/updater/README.md)
- [CI and releases](docs/delivery.md)
- [Pi setup and visual-style controls](pi/README.md)

Native P4 signed updates follow Tempo: monthly checks, an overnight installation
window, countdown and postponement, with controls on the touchscreen and paired phone.
The P4 needs a one-time USB bootstrap into the A/B layout before OTA updates.
Legacy commissioned Pi/S3 devices retain their existing updater.
