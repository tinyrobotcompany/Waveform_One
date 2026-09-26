# ESP32-S3 wiring — current working bench setup

Recorded 26 September 2026. This is the reference for the **direct-wired
Waveshare RGB-Matrix-P3-64x32 LED panel** and **INMP441 microphone**.
The user confirmed the panel works with this direct connection. The microphone
mapping below matches the previously working mic test and was reconfirmed by
the user. Combined microphone/FFT-to-panel firmware is in `visualizer`.
On 26 September the user confirmed music response on the panel, then confirmed
that the room-listening profile works at substantially lower playback volumes.

## LED panel: supplied rainbow cable to ESP32

Plug the supplied rainbow connector into the panel socket marked **IN**.
Connect its loose wires directly to the ESP32. The two 74HCT245 chips and the
separate grey IDC ribbon/header assembly are not in this working signal path.

**Colours repeat.** Lay the cable flat and count across it from the **brown
outer edge** towards the **blue outer edge**. The first column below is that
wire count, not the manufacturer's connector pin number. Use GPIO labels on
the ESP32 board, not the physical position of its header legs.

| Wire from brown edge | Cable colour | HUB75 signal | ESP32 connection | Waveshare connector pin |
| ---: | --- | --- | --- | ---: |
| 1 | Brown (outer edge) | R1 | GPIO4 | 16 |
| 2 | Red | G1 | GPIO8 | 15 |
| 3 | Orange | B1 | GPIO9 | 14 |
| 4 | Yellow | GND | GND | 13 |
| 5 | Green | R2 | GPIO10 | 12 |
| 6 | Blue | G2 | GPIO11 | 11 |
| 7 | Purple | B2 | GPIO12 | 10 |
| 8 | Grey | E | Leave disconnected | 9 |
| 9 | White | A | GPIO13 | 8 |
| 10 | Black | B | GPIO14 | 7 |
| 11 | Brown | C | GPIO15 | 6 |
| 12 | Red | D | GPIO16 | 5 |
| 13 | Orange | CLK | GPIO17 | 4 |
| 14 | Yellow | LAT / STB | GPIO18 | 3 |
| 15 | Green | OE | GPIO21 | 2 |
| 16 | Blue (outer edge) | GND | GND | 1 |

**The black signal wire is B (GPIO14), not ground.** Both GND wires in this
cable must join ESP32/common ground. This colour mapping applies to the supplied
Waveshare cable, not arbitrary rainbow jumper bundles.

Reference: [Waveshare supplied 16-pin cable diagram](https://docs.waveshare.com/assets/images/HUB75-GPIO-define-4fcc7f8b11b60a490cceddd451d7ec8f.webp).
Connector pin numbers above follow that diagram. Do not infer connector
orientation from an unlabelled front/back view.

## INMP441 microphone to ESP32

| Microphone pin | ESP32 connection | Purpose |
| --- | --- | --- |
| SCK / BCLK | GPIO5 | I2S bit clock |
| WS | GPIO6 | I2S word select / LRCLK |
| SD | GPIO7 | Audio data into ESP32 / DIN |
| L/R | GND | Select left channel |
| GND | GND | Ground |
| VDD | 3V3 | Microphone supply — never 5 V |

These assignments are confirmed in [mic_test/main/main.cpp](mic_test/main/main.cpp).
GPIO5, GPIO6 and GPIO7 do not overlap the panel assignments.

## Power

- LED panel: external regulated **5 V** supply through its separate red/black
  power lead and central VCC/GND connector. Do not power the panel through the
  ESP32 or breadboard signal wires.
- ESP32: laptop USB.
- Microphone: ESP32 **3V3**.
- Panel supply GND, ESP32 GND and microphone GND share a common ground.
- Keep external panel **+5 V** separate from ESP32 **3V3**, GPIOs and USB-powered
  **5V/VIN**. The rainbow connector carries signals and ground, not panel power.

Before changing wiring: turn off the panel supply and unplug ESP32 USB;
disconnect the analyser USB too if attached.
To start: turn on panel 5 V first, then connect ESP32 USB so the panel receives
the startup sequence. To stop: unplug ESP32 USB, then turn off panel 5 V.

## Firmware folders

| Folder | Purpose |
| --- | --- |
| [led_test](led_test/) | Standalone panel wiring/test patterns; user confirmed direct wiring works |
| [mic_test](mic_test/) | Standalone microphone capture and FFT pipeline |
| [visualizer](visualizer/) | Combined microphone FFT and 24 frequency bars on the panel |
| [kovi_scroll](kovi_scroll/) | Separate fun demo: `I Love Kovi K` |
| [quinie_scroll](quinie_scroll/) | Separate fun demo: `Aye, Aye Quinie, Fine Ta?` with coloured characters |

Flashing one application replaces the firmware running on the ESP32; it does
not delete the other source folders. The scrolling demos and panel test do
not capture microphone audio. No serial monitor is required for flashed code
to run. USB port names can change when moving the cable.

## Evidence and limits

The rainbow mapping follows Waveshare's diagram and the GPIO mapping in the
working panel firmware. The mic mapping follows its tested source and the
user's wiring confirmation. Earlier buffered-wiring instructions are historical
and do not describe this setup. Direct 3.3 V signalling worked on this bench;
that observation does not establish reliability for every panel or cable length.
This document covers wiring only, not the Pi, recognition service or enclosure.

## Firmware testing and future development

From the repository/worktree root, run all four host suites with:

```sh
sh firmware/esp32/tests/run.sh
```

Requires a C++17 compiler with AddressSanitizer and UndefinedBehaviorSanitizer
(Apple Clang on the development Mac). No ESP32 or ESP-IDF activation is required.

- `mic_test/tests`: spectrum normalization/frequency mapping, DC rejection,
  calibration, silence/noise, decay, beat refractory timing, discontinuities,
  and independence of detection from display scaling.
- `led_test/tests`: RGB test patterns, upper/lower scan halves, row/column wrap.
- `visualizer/tests/bars_test.cpp`: calibration blanking, band positions,
  gaps, colour zones, level bounds and non-finite input handling.
- `visualizer/tests/sensitivity_test.cpp`: quieter signals formerly blocked
  by the gate, brief musical dips, ambient fluctuations and return to black.

These are host unit/component tests, not a full hardware simulation. Firmware
also runs a synthetic-tone FFT self-test using ESP-DSP at startup. Electrical
integrity, I2S acquisition, real-time scheduling, stale-frame blanking and
physical refresh quality still need on-device tests. The fun scrolling demos
have build validation but no dedicated automated rendering suites.

For new visual styles use TDD: add a failing deterministic test for rendering,
colour/level mapping or animation timing, implement it, then refactor with all
tests green. Pass time/audio frames into rendering logic rather than testing
against wall-clock timing. Keep hardware-dependent code thin and verify it on
the board. For Pi integration add protocol and integration tests alongside
unit tests. Existing hardware bring-up was not consistently test-first.

### Visual styles and Pi control

The visualizer boots into `classic`, preserving the original bar display.
`mirrored` expands coloured frequency bands symmetrically around the centre.
`waterfall` adds a new frequency row every 50 ms, with up to 1.6 seconds of
history below it; colour indicates intensity. Silence scrolls black into the
history. A stale audio stream clears every mode after the existing 500 ms limit.

Use the [Rust Pi controller](../../pi/README.md) to switch modes over the same
USB cable. Commands change the renderer only, not calibration or audio gain.
Mode selections last until reboot. Tests for symmetry, history timing, stale
data and malformed USB commands run with the existing host suites.

Next milestones: Qt/QML touchscreen controls and the persistent Pi service.
Continue on the existing `feature/led-panel-test` worktree.

### Visualizer activity and blanking

The visualizer now gates on noise-subtracted spectral RMS, not total microphone
RMS. Startup calibration is fixed after three seconds; it does not adapt upward
while music plays. Pause music and keep the room quiet during calibration.
The opening threshold is 15% of calibrated room RMS (minimum 0.00010), and closing
is 10% (minimum 0.00007), both applied **after** per-band noise subtraction.
The 0.4-second closing hold and display decay remain in place. These ratios are
an initial room-listening tuning, not a guarantee of distinguishing all noise
from all music; sustained changing ambient sounds can also activate the display.

`RMS` is raw sound level; `CLEAN` and `GATE_RMS` are the cleaned signal level.
Compare `GATE_RMS` with `OPEN`/`CLOSE`. `SHUT` means the activity gate has closed;
`DISPLAY=BARS` may still appear briefly while existing bars fade. `GATE_CLOSED`
means that fade has reached black. `BEAT` records any detected beat since the
last printed line, so it need not match that line's instantaneous flux/RMS.

The host regression covers sustained quiet spectral activity below the old raw
threshold, and return to black when only calibrated ambient noise remains.
Confirm responsiveness and silence behaviour on the real panel after flashing.
