# Microphone / FFT test

This version separates audio measurements from display animation. It uses the
existing INMP441 left-channel I2S wiring: BCLK GPIO5, WS GPIO6, DIN GPIO7.

## Run it

Build and flash from this directory using your ESP-IDF environment:

```sh
idf.py build
idf.py -p /dev/cu.usbmodem5C930441681 flash monitor
```

Use the current serial port if it differs. **Pause music and keep the room quiet
during the three-second startup calibration.** Resume music after
`Calibration complete`. If music was playing during calibration, pause it and
reset the board. Calibration is repeated on each reset; nothing is written to
flash. The FFT self-test runs before microphone acquisition and fails explicitly
if the target transform's power scaling is wrong.

## Processing

```text
left-channel PCM → DC removal → unwindowed RMS
                           └→ Hann → complex FFT → normalized band power
                                                     ↓
                                             fixed noise subtraction
                                              ├→ flux → beat decision
                                              └→ fixed dB map → animation
```

- 48 kHz, 2048 samples, 24 logarithmic bands covering 60 Hz–16 kHz. Every band
  has at least one bin. Resolution is 23.4375 Hz; the first band contains the
  70.3125 Hz bin. These are finite-window spectral estimates, not sharply
  separated filters. Analysis uses contiguous, non-overlapping 42.67 ms frames.
- Band power is integrated one-sided power, scaled by
  `2 / (FFT_SIZE * sum(window²))`. It is not divided by the number of bins.
  An in-range stationary tone's total band power estimates its mean square.
- Calibration stores the 80th percentile of total and per-band power from 71
  frames. Occasional transients do not set the floor; sustained sound does.
  That reference stays fixed during playback.
- Activity opens above twice the calibrated room RMS (at least `0.0002`) and
  closes after 170 ms below 1.4 times room RMS (at least `0.00012`). The hold
  timer also closes the gate when no band has noise-subtracted energy.
- Band features are `sqrt(max(power - 1.5 * noisePower, 0))`. Flux sums positive
  changes in these features, with a 1.5 weight on the first eight bands. Its
  history advances even when the display gate is closed. Detection compares
  against the preceding flux mean plus 2.5 absolute-deviation estimates, with
  an absolute minimum of `0.002`, sufficient input RMS, and 180 ms refractory
  time. This detects onsets; it does not estimate musical tempo.
- Display levels map noise-subtracted band RMS from −70 to −20 dB relative to
  PCM full scale, then use 40 ms attack and 150 ms release. There is **no AGC**
  or peak-relative normalization. A quiet tone cannot grow to full height just
  because it becomes the loudest remaining sound. Wide bands contain more
  total energy for broadband input; this is an energy display, not a spectral
  density display.
- Dropped I2S buffers and failed/short reads invalidate the temporal history.
  The next valid frame primes detection without producing a beat.

Parameters and their units live in `main/audio_pipeline.h`. The defaults are a
starting point, not measured calibration results for a particular room. A quiet
passage must still be distinguishable from the ambient noise: an RMS gate cannot
identify music whose level is indistinguishable from background sound. Changing
the room noise substantially requires recalibration by resetting in quiet.

## Read the output

`CAL` means calibrating, `SHUT` means the activity gate is closed, and `OPEN`
means active. The 24 digits are display levels 0–9. `RMS` and `dB` are measured
before display processing; `FLUX` and `THR` are in noise-subtracted band-amplitude
units. `OPEN=` is the current opening threshold. Flux is allowed to be nonzero
while the gate is closed; the gate controls whether it qualifies as a beat.

Output is printed every second analysis frame (~11.7 Hz). `BEAT` is latched over
that interval, so it may refer to the immediately preceding, unprinted frame;
the printed RMS/flux are the latest frame. Low RMS with residual bars briefly
after music stops is expected release animation, not a new onset.

## Verification

Run portable DSP regression tests from the repository root:

```sh
sh firmware/esp32/mic_test/tests/run.sh
```

The tests run with address/undefined-behaviour sanitizers and cover PCM/DC
handling, window/power normalization against a reference FFT, band mapping,
long silence, ambient noise around the old threshold, steady tones, real
amplitude increases, bounded quiet-tone display levels, monotonic silence
decay, display/detector independence, gate reopening, calibration outliers,
refractory time, and recovery after lost frames. The board's startup self-test
uses the actual ESP-DSP transform; host tests do not emulate I2S hardware.

For the hardware acceptance check, save one labelled recording containing:

1. Startup calibration with music paused, then at least ten seconds of silence.
2. A steady tone: a starting onset, then stable bars without recurring beats.
3. Quiet music followed by louder music: levels should increase with amplitude.
4. Music stopped for at least two seconds: bars should settle to zero without
   rebounding and without beats.

Keep the calibration line and any `I2S dropped` warnings in the log. If the
fixed-scale spectrum still shows unexplained broadband bursts, capture raw PCM
to distinguish real room sounds from input corruption before changing gates.

The previous `main.cpp` and component CMake file are retained in `backups/`.
