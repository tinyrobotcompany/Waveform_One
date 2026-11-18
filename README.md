# Waveform One

[![CI and Release](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml/badge.svg)](https://github.com/tinyrobotcompany/Waveform_One/actions/workflows/release.yml)
[![Latest release](https://img.shields.io/github/v/release/tinyrobotcompany/Waveform_One)](https://github.com/tinyrobotcompany/Waveform_One/releases/latest)

A music-reactive LED display using an ESP32-S3, INMP441 microphone and 64×32
HUB75 panel. The current working prototype connects the panel directly to the
ESP32-S3. The ESP32-P4 touchscreen controller selects visual styles over USB,
joins the user's home Wi-Fi and hosts the local phone remote.

Migration to the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B display controller is
under development. Display, touch, home Wi-Fi setup, the phone remote and S3
style control are working. Native music recognition has host/reference
verification and awaits physical P4 acceptance. The working
ESP32-S3 audio and HUB75 engine remains intact during this migration.

- [Wiring, firmware programs and build instructions](firmware/esp32/README.md)
- [ESP32-P4 display bring-up and wiring](firmware/esp32/p4_display/README.md)
- [ESP32-P4 controller architecture decision](docs/architecture/0001-esp32-p4-display-controller.md)
- [Contributing and local tests](CONTRIBUTING.md)
- [Release notes](CHANGELOG.md)
- [Native P4 over-the-air updates](docs/p4-ota.md)
- [Legacy Pi over-the-air updates](pi/updater/README.md)
- [CI and releases](docs/delivery.md)
- [Pi setup and visual-style controls](pi/README.md)

Native P4 signed updates follow Tempo: monthly checks, an overnight installation
window, countdown and postponement, with controls on the touchscreen and paired phone.
The P4 needs a one-time USB bootstrap into the A/B layout before OTA updates.
Legacy commissioned Pi/S3 devices retain their existing updater.

<!-- privacy-notice:start -->
## Microphone and privacy

**Waveform One does not save audio recordings on the device.** While running,
its microphone continuously captures nearby sound in temporary memory to animate
the LED display and identify music. The current P4 firmware uses a rolling
16-second audio buffer; this is not a permanent recording archive or a guarantee
that every temporary copy is securely erased after 16 seconds.

For recognition, the P4 sends actual audio clips of up to 10 seconds directly to
ACRCloud over HTTPS. Requests can repeat approximately every 2.5 seconds while
recognition is active, so clips overlap. Clips may include nearby voices and
other background sounds. Sound classification cannot guarantee that speech is
excluded, and sustained unclassified sound can also trigger a recognition request.
Waveform One does not provide conversation transcription.

ACRCloud states that it deletes temporary raw audio when request processing
succeeds or fails. Recognition results, fingerprints and technical logs have
separate retention rules; see [ACRCloud's data-processing terms](https://www.acrcloud.com/dpa/).
These are the provider's commitments, not a claim of independently verified deletion.
Device diagnostics include recognised titles, artists, timings and sound levels;
those logs can be retained when captured by a connected development computer.
Using your own ACRCloud account does not prevent audio from being sent to ACRCloud.

This notice describes the current P4 implementation. Separate cloud-recognition
controls and a microphone-off control are proposed features, not current promises.
<!-- privacy-notice:end -->
