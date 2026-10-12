# Silent music-recognition benchmark

This desktop experiment reads saved public WAV files. It never plays audio,
opens a microphone/serial port, flashes a board or calls a recognition service.
The device firmware and its eight-second capture requirement are unchanged.

## Fingerprint experiment

`benchmark.py` builds the production fingerprint implementation plus an
experimental copy under the specified output folder. The copy changes only
its input-length guard to accept two through eight seconds, aligned to the
existing 128-sample processing hop. No FFT, gain, peak selection or encoding
code is changed. Each eight-second result must equal the production result.

Five tracks × three offsets × five durations give 75 cases. Three repetitions
check signature determinism and supply a median MacBook processing time.
A generated signature or a high peak count is **not** a confirmed song match.
The service, live capture and MCU timings require separate measurements.

## Classifier experiment

The optional reference uses the old quantized YAMNet model in an isolated host
build, not in firmware. Model SHA-256 is checked. Rebuild its feature extractor
and TFLite Micro runner from the historical reference source at `92103a6`, with
ESP TFLite Micro revision `5d505b5ffac8e60923bf29a0cc6e6348c84e3763`.
Retain the model's LICENSE, NOTICE and provenance when extracting it.

The model takes 15,600-sample windows at 16 kHz, with a 7,800-sample hop. Compare
the earlier three-class maximum with the broader music/instrument/vocal class
group. A proposed state policy requires two strong windows to enter, tolerates
short weak windows, exits after sustained negative evidence, and bounds uncertain
retention to four seconds. Exact silence resets it. This silence check uses a
whole classifier window; it is not a validated fast microphone silence detector.

Sources are checksum-verified in local manifests. Cases include speech, real
household/outdoor noises, synthetic silence/noise, attenuated clean music,
music/speech mixtures at -6/0/+6 dB speech-to-music RMS, and music-to-speech/silence
transitions. Mixtures are normalized together to prevent clipping.

Scores are model confidences, not calibrated probabilities. These are exploratory
samples, not an independent holdout or proof of general music-only operation.
A single public speech example and a few song excerpts do not cover all rooms,
languages, genres, a cappella, TV or human/musical transients. Lowering thresholds
on this corpus alone would risk overfitting.

## Run

From this worktree, use project-local generated artifacts:

```sh
BENCH_DIR=/Users/simonholmes/Projects/Applications/Waveform_One/build/music-recognition-benchmark
python3 tools/music_benchmark/benchmark.py \
  --manifest "$BENCH_DIR/manifest.json" \
  --output "$BENCH_DIR/results"
```

The manifest is a JSON list with `id`, `wav`, `wav_sha256` and provenance/expected
metadata. WAV input must be uncompressed, signed PCM16, mono, 16 kHz. Decode public
source previews into new WAV files rather than reusing unverified raw PCM caches.
Audio conversion is file-to-file only; do not use playback commands.

For classification, additionally provide:

```sh
  --non-music-manifest "$BENCH_DIR/non-music-manifest.json" \
  --extractor "$BENCH_DIR/reference/build/music_features" \
  --classifier "$BENCH_DIR/reference/build/music_micro" \
  --model "$BENCH_DIR/reference/firmware/esp32/common/music_model/yamnet_int8.tflite" \
  --labels "$BENCH_DIR/labels.txt" \
  --speech "$BENCH_DIR/fixtures/speech.wav"
```

The classifier options must all be supplied together. Results include source,
fixture and model hashes, per-window decisions, clip statistics and baseline
parity. CPU times are desktop measurements; classifier timing includes subprocess
launch/model initialization and does not represent steady embedded inference.

Run regression tests with:

```sh
python3 -m unittest discover -s tools/music_benchmark -p 'test_*.py' -v
```

Sources: the local manifest records Apple preview URLs and file hashes;
[TensorFlow's public speech fixture](https://raw.githubusercontent.com/tensorflow/tflite-support/master/tensorflow_lite_support/cc/test/testdata/task/audio/speech.wav)
and [ESC-50 metadata/audio](https://github.com/karolpiczak/ESC-50) provide the
non-music references. Audio/model data and binaries remain under ignored `build/`,
not in Git. No private-room audio is used or uploaded.

## Optional, separate catalogue identification check

`benchmark.py` remains entirely network-free. To check whether a signature really
identifies its source song, invoke `catalogue_check.py` explicitly:

```sh
python3 tools/music_benchmark/catalogue_check.py \
  --benchmark-dir "$BENCH_DIR" --seconds 4
```

This sends only known public-fixture fingerprints to the existing recognition
service; it never plays audio, records a microphone or uploads WAV/PCM. Requests
are paced at least sixty seconds apart, at most five per run, and stop on errors
or any Retry-After. Do not restart a stopped run during a service cooldown.
Review returned titles/artists/albums against the manifest before labelling a
result correct. The clip-duration-plus-request budget is calculated, not a
physical-device timing measurement. See the separate catalogue report.

### Four-second hardware trial

The native P4 later moved to continuous streaming (see the streaming recognition
commissioning record); at the time of this trial it requested `CAPTURE 4`; the S3 continues to support
legacy eight-second requests. This is separate from the reference music-only
classifier, which remains disabled on both boards. Current firmware fingerprint
outputs are checked against the four-second benchmark signatures. The benchmark
itself pins its original eight-second source to colour commit `29f8bcd` so later
firmware changes cannot silently change historical comparisons. See
[the commissioning record](../../docs/commissioning/2026-10-12-four-second-recognition.md)
for pre-flash checks and live-test limitations.

### Streaming signatures

`stream_cli.cpp` runs a saved WAV through the device's continuous analyser
and prints the signature for each `START_S:LENGTH_S` window, with its peak
overlap against the same audio fingerprinted as a separate recording:

```sh
c++ -O2 -std=c++17 -I firmware/esp32/p4_display/main tools/music_benchmark/stream_cli.cpp \
  firmware/esp32/p4_display/main/fingerprint.cpp -o build/stream_cli
build/stream_cli "$BENCH_DIR/fixtures/jazz.wav" 2:3 2:5 2:8
```
