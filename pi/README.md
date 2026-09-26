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

### Touchscreen and idle-mode refinement

The main screen now prioritizes the clock, greeting and track/artwork area. Styles
are in Settings → Styles. Settings uses separate You/Styles/Screen/Phone tabs,
a scrolling content pane and fixed action buttons. On the Pi kiosk, tapping the
name opens an embedded keyboard; other browsers retain their native keyboard
and can optionally open the embedded one. The kiosk URL must include `?kiosk=1`.

Screen activity no longer equates every ESP OPEN diagnostic with music. It
requires at least three bands at level 2/9, a peak of 3/9, total level 12/216,
and one second of sustained qualifying activity before updating the screen's
music timer. Weak readings and isolated narrow-band vibration do not reset the
30-second hold. These are display heuristics, not a music classifier; confirm
quiet-room and low-volume playback on the actual hardware. LED gating is unchanged.
The authenticated `/api/wake` endpoint holds the selected backlight for 60 seconds
following touch/keyboard interaction without altering audio activity state.

The optional ShazamIO service below supplies real track metadata and artwork.
The default vinyl illustration is a placeholder when no match or cover is
available; it is never presented as an identified album cover.

Browser regression checks use a synthetic backend and exercise 800×480, 390×844
and 844×390 layouts, keyboard entry/save, styles, scrolling and quiet-screen
presentation. Run `pi/web/browser-check.cjs` with `PLAYWRIGHT_MODULE` pointing to
an installed Playwright module and `CHROME_BIN` pointing to Chrome/Chromium.
For example, install Playwright into a temporary directory using
`npm install --prefix /tmp/waveform-browser-tests playwright`, then:

```sh
PLAYWRIGHT_MODULE=/tmp/waveform-browser-tests/node_modules/playwright \
CHROME_BIN='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' \
node pi/web/browser-check.cjs
```

These browser checks complement the Rust activity tests and actual Pi inspection;
they do not claim physical touchscreen or iPhone Safari validation.

## Music recognition and album artwork

The optional `waveform-recognition` user service uses **ShazamIO 0.8.1**. No API
key, AudD account or subscription is used. It is an unofficial Shazam client;
service availability and compatibility are not guaranteed for a commercial
product. Tested runtime: Raspberry Pi OS ARM64, Python 3.13.

Update the ESP visualizer firmware first, then run on the Pi from this checkout:

```sh
sh pi/scripts/install-display.sh
sh pi/scripts/install-recognition.sh
```

During sustained music the worker requests eight seconds of 16 kHz, signed
16-bit mono PCM from the existing INMP441 microphone, over USB. It generates
an acoustic fingerprint locally with ShazamIO and sends the fingerprint to
Shazam. Raw audio is kept in memory, not written to disk. Cover images load
from the HTTPS URL returned by the provider. No new wiring is needed.

A successful match supplies title, artist, album (when provided), artwork
(when provided), and the **Now Playing** tag. A recording can occur on several
releases, so the provider's album/cover is not proof of the physical edition
being played. Unmatched music shows a listening message, never invented data.
The worker waits 30 seconds after each result before another capture; errors
back off from 60 to 300 seconds. A no-match response clears the previous song.
Matches expire after 90 seconds without confirmation and are hidden immediately
in quiet mode or on disconnection. A playback-session identifier prevents a
late result from being shown for a later listening session.

```sh
systemctl --user status waveform-recognition
journalctl --user -u waveform-recognition -n 30 --no-pager
systemctl --user stop waveform-recognition  # disable recognition temporarily
systemctl --user disable --now waveform-recognition  # also disable at login
```

`recognition.json` in `~/.config/waveform-one/` contains a short-lived metadata
snapshot. The worker reuses the local remote token to call the display service;
it never opens the ESP serial device itself. `/api/capture` additionally rejects
non-loopback callers. It rejects captures outside active listening, overlapping captures, incomplete
clips, checksum/sequence errors, and changed listening sessions. Capture may
queue a style change for approximately eight seconds, while the LED rendering
continues. Stop both services before manually flashing or monitoring the ESP.

Host tests: `PYTHONPATH=pi/recognition python3 -m unittest discover -s
pi/recognition/tests -v` (also part of `scripts/test.sh`). Real recognition
requires internet and music; unit tests do not make external requests.
