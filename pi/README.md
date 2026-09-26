# Pi control integration

A Rust USB controller and persistent display service. Audio capture, noise
calibration and panel refresh stay on the ESP. The display service serves one
responsive interface to Chromium on the Pi and to paired phone browsers.

Tested target: Raspberry Pi 4, 64-bit Debian 13, ESP32-S3 native USB Serial/JTAG.
The prototype uses versioned text commands; the planned binary/CRC protocol and
PCM streaming remain future work. See [protocol](../protocol/specification.md).

## Wiring

Power the Pi through its normal USB-C supply. Connect a Pi USB-A port to the
ESP's existing USB connector using a data cable. Keep the panel on its own
existing 5 V supply and preserve the common-ground wiring. The Pi's micro-HDMI
ports connect displays, not the ESP. No extra GPIO wires are needed for control.

## Build and use

Install Rust (tested with 1.98.1) and a C linker/compiler. From the repository:

```sh
cargo test --manifest-path pi/core/Cargo.toml --locked
cargo build --manifest-path pi/core/Cargo.toml --release --locked
ls -l /dev/serial/by-id/
pi/core/target/release/waveform-control /dev/ttyACM0 interactive
```

Use the stable `/dev/serial/by-id/...` path instead of `/dev/ttyACM0` when multiple
USB devices are attached. The user must belong to `dialout`; do not run the
controller as root. Only one program should own the serial port at a time.
Close the controller before flashing or running a monitor.

At `waveform>` type `classic`, `mirrored`, `waterfall`, `status`, or `quit`.
For a single change, replace `interactive` with a mode name. The controller
reports success only after the ESP acknowledges that request. It times out
after three seconds when the firmware lacks the protocol or the device is
unavailable; it does not silently claim a mode changed.

Settings are volatile. ESP reboot returns to classic and starts three-second
noise calibration. Keep music paused for five seconds following a reboot.
The controller avoids deliberately toggling reset lines; USB opening behaviour
can still depend on the OS/adapter. Disconnecting the Pi leaves the ESP running
its current mode as long as it still has power.

GitHub releases include an ARM64 Linux controller archive, built on Ubuntu 24.04
(glibc 2.39 or later required). Extract it and run the executable as above. It is
not an auto-update installer. Native source builds support other compatible Pi OS
versions. The 7-inch screen uses the shared browser interface described below.

## Display and phone remote (first working slice)

The `waveform-display` Rust daemon serves the same local web interface to the
Pi's Chromium kiosk and phone browsers. It owns the USB connection, serializes
style commands, confirms acknowledgements and retries disconnected devices.
Existing diagnostic telemetry drives a frequency preview and a 30-second idle
clock. Missing/stale telemetry is shown separately from silence. Names persist
in `~/.config/waveform-one/preferences.json`; styles remain ESP-owned and reset
on ESP reboot as before. No ESP firmware update is needed for this slice.

From a source checkout on the Pi, run `sh pi/scripts/install-display.sh`.
The current installer targets the measured prototype USB serial identity and
`/sys/class/backlight/10-0045`; adapt the installed user service for other devices.
It starts on user-session startup. Launch `~/.local/bin/waveform-kiosk` within the
Pi graphical session. This launches a dedicated Chromium profile without
interfering with a normal browser profile. The installer adds a desktop autostart entry for the kiosk.
Stop the service before using the serial CLI, monitor or flasher:
`systemctl --user stop waveform-display`.

On a phone on the same Wi-Fi, open `http://raspberrypi.local:8080`. Open Settings
on the Pi and expand Connect your phone and scan the QR code, or enter the device token on the
phone once; it stays in browser local storage. The token is generated locally,
is not committed, and is required for every API request. The page assets contain
no token. This prototype uses local HTTP: use only a trusted LAN and do not
forward port 8080 to the internet. Remote internet access/TLS are not configured.
To revoke all browser access, stop the service, remove its `remote-token` file,
and restart it; relaunch/re-pair browsers with the new token.

Screen brightness ranges from 10–100%; idle caps it at 15%. The service user
needs write access to the selected backlight (the prototype's user is in video).
This does not change LED-panel brightness. A screen-off preference, palettes,
LED brightness and mic sensitivity controls are still pending firmware/API work.
Recognition, PCM transport and real album metadata are not implemented yet; the
UI explicitly reports that setup is pending rather than inventing a track.
No audio recordings are currently uploaded. OTA installation is a later phase.

For local development the daemon defaults to `127.0.0.1:8080`:
`pi/core/target/debug/waveform-display /dev/your-serial-port`.
`WAVEFORM_BIND`, `WAVEFORM_CONFIG_DIR` and `WAVEFORM_BACKLIGHT` override deployment
settings. API routes: GET `/api/state`, POST `/api/mode/{classic|mirrored|waterfall}`,
and POST `/api/preferences` with a JSON name and optional screen_brightness.
All APIs require `Authorization: Bearer <device-token>`; no cross-origin access
is enabled. Integration tests start a real local HTTP server with no serial
hardware and verify pairing, validation, disconnected behaviour and persistence.
