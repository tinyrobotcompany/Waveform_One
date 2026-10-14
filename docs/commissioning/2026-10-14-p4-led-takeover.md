# P4 HUB75 takeover experiment

Status: software experiment, **not flashed or electrically validated**.
Branch `feature/p4-led-takeover` includes `085bca8`, preserving the P4 microphone,
continuous recognition, album colours and album-cache recovery. The running
P4/S3 configuration is unchanged. S3 removal is not yet accepted.

## Source coverage and evidence

- [Waveshare 7B schematic](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf),
  expansion nets P1/P3; compare against the physical board revision before wiring.
- Installed Waveshare BSP 3.0.1 defines onboard I2C on 7/8, audio on 9–13,
  amplifier enable 53, LCD backlight/reset 32/33, and SD card 39–44.
- Working sdkconfig assigns C6 SDIO 14–19 and reset 54. UART debug uses 37/38.
- [Espressif advanced RGB matrix example](https://github.com/espressif/esp-idf/tree/v6.1/examples/peripherals/parlio/parlio_tx/advanced_rgb_led_matrix)
  and the installed IDF 6.1 PARLIO driver demonstrate 16-bit DMA loop output
  and buffer-switch callbacks. No library download is needed.
- Existing S3 `visualizer/main/panel.cpp` supplies the tested panel's
  FM6124/FM6126A register sequence. This experiment retains those bit values.

These establish a plausible pin/resource plan, not signal integrity or
coexistence under load. Physical connector pin numbering/orientation is not
inferred from the order of extracted PDF text.

## Candidate mapping

GPIO numbers, **not connector cavity numbers**. This is a bench proposal;
do not connect it until socket orientation and the adapter wiring are verified.

| HUB75 signal | P4 GPIO |
|---|---:|
| R1 / G1 / B1 | 2 / 3 / 4 |
| R2 / G2 / B2 | 5 / 28 / 29 |
| A / B / C / D | 30 / 31 / 34 / 49 |
| LAT | 50 |
| OE (active low) | 51 |
| CLK | 52 |

The panel's existing 16-pin IDC cable remains. Two PH2.0 12-contact cable
assemblies take the required GPIOs from the P4 to a buffered adapter, whose
HUB75 socket accepts the 16-pin cable. These connectors have different jobs;
no 12-pin plug replaces the 16-pin panel plug. The user has the panel cable
but not the PH2.0 assemblies. No separate buffer was identifiable in the photo.

Adapter requirements: 13 unidirectional 3.3-to-5 V channels (e.g. two
74AHCT245s), shared ground, correct HUB75 signal mapping, local decoupling and
OE held inactive at startup. Keep the panel on its suitable separate 5 V
power feed. Do not feed panel current through GPIO sockets. Disconnect S3
signal outputs before connecting P4 outputs; never drive the same line from
both controllers. Make wiring changes with power off.

## Implementation

`CONFIG_WAVEFORM_P4_LED` defaults off and depends on the onboard microphone.
When off, the S3 USB control path is unchanged. When enabled, LED mode controls
route locally and the USB S3 controller is not started. The first hardware
build must retain `CONFIG_WAVEFORM_P4_LED_PATTERN=y`: fixed coloured stripes,
black borders, conservative duty cycle. Audio mode is selected separately.

PARLIO emits 16-bit words at 1 MHz: six RGB bits, LAT/OE and four address bits.
Each of 16 row pairs occupies 128 clocks, with 64 colour shifts, latch on the
last shift, blank guard clocks and 32 illuminated clocks. Nominal scan rate is
488 Hz. This is a starting waveform, not a measured refresh/brightness result.

Two 4096-byte frames live in aligned PSRAM. The task cannot write either an
active or pending frame; it waits up to 500 ms for the DMA-switch callback.
Failure blanks OE and stops only the LED task. DMA descriptors/task stack still
need internal memory; the device's already-low DMA headroom requires measurement.
No claims of zero internal-memory cost are made.

Audio mode uses a separate reader of the same 16 kHz mono ring as recognition,
a 512-point Hann-window FFT and the existing 24-band/three-style renderer.
Analysis state lives in PSRAM. It does not reconfigure I2S or touch recognition
FFT state. Spectrum covers 60 Hz–7.5 kHz (the 48 kHz S3 path reached 16 kHz).
The -80 to -35 dB visual range is provisional; no voice/music classification or
room calibration is introduced. Stale capture blanks after 500 ms.

## Validation and acceptance

Host tests check all row addresses, RGB packing, latch blanking, lit dwell,
frame boundaries, buffer handoff rejection and 100 alternating swaps. Silence,
constant DC and a known 1 kHz/16 kHz tone check spectrum scaling and frequency.
Native build artifacts and stack-usage output are under `build/p4-led-takeover/`
and the ESP-IDF build directory. Neither proves physical wiring or DMA timing.

Hardware stages, still pending:

1. Obtain/verify adapter and cables; continuity-check mapping and ground.
2. Power off and move panel signals from S3 to the buffered P4 adapter.
3. Flash diagnostic build, check colours, row order, ghosting, brightness and
   OE/CLK/LAT on an oscilloscope/logic analyser where available.
4. Test audio mode, three UI styles, disconnection/stale-input blanking and
   recognition/artwork concurrently. Record memory, resets, capture delivery
   and recognition latency against the saved baseline during a sustained run.
5. Only after these pass, remove the S3 and external microphone and revise the
   carrier-board design. The existing carrier PCB still contains an S3.

Do not flash this branch merely because compilation succeeds. The current
physical prerequisites are missing. The cache fix is already running on P4;
its saved pre-experiment source remains on `fix/album-cache-recovery`.
