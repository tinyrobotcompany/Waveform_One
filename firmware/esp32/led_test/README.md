# LED panel wiring diagnostic

> **Current wiring reference:** [ESP32 panel + microphone README](../README.md).
> The panel now works with the supplied rainbow cable connected directly to
> the ESP32, bypassing both buffer chips. The buffered wiring and September 22
> validation notes below are historical. The current code uses **500 us** row
> on-time, superseding the original 15 us setting described below.

Standalone ESP-IDF application for the actual Waveshare **RGB-Matrix-P3-64x32**
(192 x 96 mm, 1/16 scan), ESP32-S3 and two SN74HCT245N buffers. This temporarily
replaces the running mic application when flashed; `../mic_test` remains intact
and can be flashed again. No microphone, FFT, Wi-Fi or music recognition runs here.

## What to expect

Each stage lasts four seconds, repeating every 32 seconds:

1. Black.
2. Dim red.
3. Dim green.
4. Dim blue.
5. Top half red, bottom half blue.
6. One green row moving through all 32 rows.
7. One red column moving through all 64 columns.
8. Three vertical red/green/blue bars.

Serial logs identify each stage. The conservative software clock and 15 us
output-enable pulse are for wiring diagnosis, not the final visualizer refresh
driver. OE stays high during shifting, latching, logging and scheduler waits.
Only the 15 us illuminated interval disables ordinary interrupts/preemption.
Refresh rate/brightness and electrical waveforms still need hardware validation.

## Agreed wiring

U1 = upper chip, U2 = lower chip. Physical DIP leg numbers, notch at top.

| Signal | ESP32 GPIO | Buffer input | Buffer output |
| --- | ---: | --- | --- |
| R1 | 4 | U1 2 | U1 18 |
| G1 | 8 | U1 3 | U1 17 |
| B1 | 9 | U1 4 | U1 16 |
| R2 | 10 | U1 5 | U1 15 |
| G2 | 11 | U1 6 | U1 14 |
| B2 | 12 | U1 7 | U1 13 |
| A | 13 | U1 8 | U1 12 |
| B | 14 | U1 9 | U1 11 |
| C | 15 | U2 2 | U2 18 |
| D | 16 | U2 3 | U2 17 |
| CLK | 17 | U2 4 | U2 16 through 47 ohm series resistor |
| LAT | 18 | U2 5 | U2 15 |
| Panel OE | 21 | U2 6 | U2 14 |

Both chips: legs 20 and 1 to external 5 V; legs 10 and 19 to ground;
100 nF ceramic between supply and ground close to each chip. U2 unused inputs
7/8/9 go to ground; outputs 13/12/11 stay disconnected. Panel OE is a different
signal from the chips' enable on leg 19.

10 kohm from U2 leg 6 / GPIO21 junction to **ESP32 3V3**, not 5 V.
Microphone remains on 3V3 with BCLK/WS/SD GPIO5/6/7, untouched by this firmware.
Panel and chips share external 5 V. Panel's power cable bypasses the breadboard.
All grounds join; external 5 V must not join USB-powered ESP32 5V or 3V3.
The 3V3 microphone supply must not share the chips' positive rail.
Use the panel INPUT; E is unused. Trace cable contacts rather than assuming
colour identifies a unique signal (several colours repeat).

## Power and flash

Do all wiring with both supplies disconnected. Confirm 5 V and polarity before
use. HCT inputs must not be driven above their supply: with signal wires attached,
power the chips from external 5 V **before** plugging in ESP32 USB. Remove ESP32
USB **before** turning that 5 V off. This ordering is for the temporary bench
setup; boot-time blanking without ESP32 power is not guaranteed by the 3V3 pull-up.
Do not treat software dimming as a substitute for correct supply wiring.

From this folder in the existing configured ESP-IDF 6.1 environment:

```sh
idf.py build
idf.py -p /dev/cu.YOUR_ESP32_PORT -b 115200 flash monitor
```

Exit monitor with Ctrl+]. Flashing overwrites the mic app, not source files.
To restore it, build/flash `../mic_test` with the same port. Disconnect panel
signals with all power off before running standalone mic firmware again.

Host pattern checks:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined tests/patterns_test.cpp -o /tmp/waveform-led-patterns-test
/tmp/waveform-led-patterns-test
```

These checks cover RGB output masks, independent upper/lower row addressing,
scan wrap and colour-bar boundaries. They do not validate physical wiring.

## Validation, 22 September 2026

- ESP-IDF 6.1 / ESP32-S3 build passed: app size 0x2b380, 83% partition free.
- Host checks passed with AddressSanitizer and UndefinedBehaviorSanitizer.
- Flash and physical display verification are pending an attached ESP32 USB port.
- Build used the existing EIM environment under `~/.espressif/tools/python/v6.1/venv`
  with `IDF_TOOLS_PATH=~/.espressif/tools` and `ESP_IDF_VERSION=6.1`; no SDK or
  Python installation changes were made.

## Sources

- [Waveshare panel specifications](https://docs.waveshare.com/RGB-Matrix-Px-64x32)
- [Waveshare cable pinout](https://docs.waveshare.com/assets/images/HUB75-GPIO-define-4fcc7f8b11b60a490cceddd451d7ec8f.webp)
- [TI SN74HCT245 datasheet](https://www.ti.com/lit/gpn/SN74HCT245)
- Local ESP-IDF 6.1 GPIO and ROM delay headers.

Scope: this diagnostic and the existing mic pin configuration were inspected;
unrelated Pi/UI/CAD code was not loaded or changed. Build and host-test evidence
cannot establish that the physical panel works; that requires flash and observation.
