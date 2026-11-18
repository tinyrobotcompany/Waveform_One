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

**Waveform One does not save audio recordings on the device.** Automatic music
recognition is a core feature and runs during normal operation. This notice
describes the current ESP32-P4 firmware; temporary audio capture and transmission
for recognition are different from saving a permanent recording.

**What the microphone captures.** While powered on and running, the microphone
continuously captures nearby sound to animate the LEDs and identify music. Audio
is processed in temporary RAM, including a rolling 16-second buffer. Buffers are
reused; this is not a recording archive or a guarantee that every temporary copy
is securely erased after exactly 16 seconds. Waveform One does not provide
conversation transcription or a feature for replaying recorded conversations.

**What leaves the device.** For recognition, the P4 sends actual audio clips of
up to 10 seconds directly to ACRCloud over HTTPS, not to a Waveform One audio-storage
server. Requests can repeat approximately every 2.5 seconds while recognition is
active, so clips overlap substantially. Requests also include an account access-key
identifier, request timestamp and authentication signature; the service can see
the connecting network's public IP address. The access secret is used locally to
sign requests, rather than sent as an audio-request field.

**Nearby voices.** Clips may include nearby voices and other background sounds.
Sound classification cannot guarantee that speech is excluded, and sustained
unclassified sound can also trigger a recognition request. Do not assume that
only music leaves the device. People nearby should be made aware that automatic
music recognition can send samples containing their voices to ACRCloud.

**Provider processing and retention.** ACRCloud states that temporary raw audio
is deleted when request processing succeeds or fails, without a retained audio
archive or long-term audio backup. Recognition results, fingerprints, request
records and technical logs have separate retention and deletion rules; they are
not all deleted with the audio. Processing location depends on the ACRCloud
project's configured region. See [ACRCloud's data-processing terms](https://www.acrcloud.com/dpa/)
and [privacy policy](https://www.acrcloud.com/privacy/). These are the
provider's commitments, not a claim that Waveform One independently verifies
server-side deletion. Requests about provider-held data should follow the
applicable account's ACRCloud privacy and deletion procedures.

**Other stored information.** Device diagnostics include recognised titles,
artists, timings and sound levels, and can be retained when captured by a
connected development computer. Recognition metadata and artwork may be cached
for display. ACRCloud credentials are stored on the device so recognition
continues after firmware updates; that credential storage is separate from audio
buffering. Using your own ACRCloud account changes whose account is used, not
whether recognition audio is transmitted to ACRCloud.

**Stopping capture.** Powering off Waveform One stops microphone capture and
uploads. Disconnecting its internet connection prevents cloud recognition
requests from reaching ACRCloud, but does not itself turn off the microphone.
This notice does not promise a microphone-mute or cloud-opt-out control that the
current firmware does not implement.
<!-- privacy-notice:end -->
