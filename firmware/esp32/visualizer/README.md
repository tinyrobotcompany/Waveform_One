# Microphone FFT on the LED panel

First combined sound-reactive application. Uses the [documented direct
wiring](../README.md) without changes: microphone GPIO5/6/7 and panel
GPIO4/8–18/21. Reuses mic_test's audio_pipeline.cpp rather than retuning or
duplicating the tested DSP. ESP-DSP is pinned to the tested 1.8.2 version.

## Run

Keep music paused when starting/resetting. Power the panel, connect ESP USB,
and flash from this folder in an activated ESP-IDF terminal:

```sh
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash monitor
```

Update the port if it changed. Exit any existing monitor with Ctrl-] first.
After startup the display stays black during approximately three seconds of
quiet noise calibration. Wait for `Calibration complete`, then play music.
Reset in a quiet room if music was playing during calibration.

24 bars run from low frequencies on the left to high frequencies on the right.
Each has a green base, yellow middle and red tip. Display levels use the
existing fixed-dB/noise-gated pipeline; no extra AGC or beat flashing is added.
Silence decays towards black. Audio failure blanks stale bars after 500 ms.

## Quiet-listening sensitivity

`main/visualizer_config.h` selects a room-listening profile without changing
the standalone microphone test's defaults:

| Setting | Original | Room-listening profile |
| --- | ---: | ---: |
| Gate opens above calibrated RMS | 2.0x | 1.25x |
| Gate closes below calibrated RMS | 1.4x | 1.10x |
| Minimum opening RMS | 0.00020 | 0.00010 |
| Minimum closing RMS | 0.00012 | 0.00007 |
| Quiet hold | 0.17 s | 0.40 s |
| Display range | -70 to -20 dBFS | -80 to -35 dBFS |
| Display release | 0.15 s | 0.25 s |

Spectral noise subtraction, calibration and flux calculation are unchanged;
there is no AGC. The lowered activity gate also changes beat eligibility, but
beat events do not drive these bars. Increasing sensitivity can admit more
room sounds; hardware verification at the desired listening volume is needed.
Noise calibration still requires a quiet room. Sound below the measured noise
floor cannot be recovered just by adjusting display gain.

The monitor reports `DISPLAY`, `BANDS` and `PEAK_PX` twice per second.
`GATE_CLOSED` means software suppressed the signal; `BELOW_DISPLAY` means
clean bands remain too small to light a pixel. `NO_CLEAN_BANDS` means spectral
noise subtraction removed all band energy. `BARS` means nonzero pixels were
requested (it does not electrically verify that the panel displayed them).
The older 0–9 band string is coarse; use `PEAK_PX` for small bars.

## Scheduling and diagnostics

Audio capture and FFT run in the main task on core 0. A task on core 1 refreshes
the panel continuously using the working bit-banged driver. A single-element
queue passes the latest complete frame without blocking audio capture. All
rows in a scan use the same frame. The existing FFT startup self-test and
I2S overflow warnings are retained.

Build and host tests do not verify electrical performance. Check the panel
with silence, speech and music and watch for `I2S dropped` warnings. The scan
driver is the bench baseline; final refresh quality remains hardware work.

## Host validation

All firmware host suites can also be run from the worktree root using
`sh firmware/esp32/tests/run.sh`.

```sh
bash ../mic_test/tests/run.sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I main -I ../mic_test/main tests/bars_test.cpp -o /tmp/waveform-bars-test
/tmp/waveform-bars-test
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I main -I ../mic_test/main tests/sensitivity_test.cpp \
  ../mic_test/main/audio_pipeline.cpp -o /tmp/waveform-sensitivity-test
/tmp/waveform-sensitivity-test
```

To restore the standalone LED test, flash from `../led_test`. The fun demos
remain in `../kovi_scroll` and `../quinie_scroll`.

## Bench result — 26 September 2026

The user confirmed that microphone-driven bars work with the direct wiring.
After the room-listening profile was applied, the user confirmed response at
much lower playback volumes. The supplied boot log showed the FFT self-test
passing and no I2S overflow warnings in that excerpt. This is a working bench
baseline, not a completed long-duration or electrical validation.

### USB audio capture for Pi recognition

`WF1 <id> CAPTURE` starts one eight-second clip. Reply:
`WF1 <id> AUDIO 16000 128000`, followed by exactly 1000 ordered
`WF1 <id> PCM <sequence> <512 hex characters> <8-digit FNV-1a checksum>`
lines, then `WF1 <id> END 1000`. Sequences start at zero; decoded bytes are
little-endian signed 16-bit mono. The checksum covers each 256-byte payload.
Diagnostic lines can occur between packets. Another CAPTURE command while capturing
returns `ERR BUSY`; dropped samples/queue overflow return `ERR AUDIO_LOST`.

A separate low-pass/downsample path converts the 48 kHz left microphone channel
to 16 kHz. A bounded FreeRTOS queue keeps USB sending out of the audio-analysis
loop. Capture never changes FFT/gating input, panel GPIOs, LED sensitivity or
rendering mode. Host clients must enforce a timeout and discard incomplete data.
