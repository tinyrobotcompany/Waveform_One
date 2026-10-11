# Offline music classifier evaluation

The firmware prototype is native C++, on the S3. These desktop tools only
prepare the model and evaluate public fixtures. They do not provide a music
service, upload audio or run on the Pi/P4/S3.

Use an isolated Python 3.13 environment with `requirements.txt`. Download the
original Google model from the URL in
`firmware/esp32/common/music_model/provenance.json`; `prepare_model.py` rejects any
source with a different SHA-256. From the worktree root:

```sh
python tools/music_detection/prepare_model.py /tmp/yamnet-original.tflite firmware/esp32/common/music_model
git clone --branch v1.4.1 --depth 1 https://github.com/espressif/esp-tflite-micro.git /tmp/esp-tflite-micro
cmake -S tools/music_detection -B /tmp/music-host -DTFLM_ROOT=/tmp/esp-tflite-micro
cmake --build /tmp/music-host -j 8
python tools/music_detection/verify_frontend.py /tmp/yamnet-original.tflite firmware/esp32/common/music_model/yamnet_int8.tflite /tmp/music-host/music_features /tmp/speech.wav /tmp/music.ogg
python tools/music_detection/evaluate.py firmware/esp32/common/music_model/yamnet_int8.tflite /tmp/music-host/music_features /tmp/music-host/music_micro /tmp/speech.wav /tmp/results.tsv /tmp/music.ogg
```

The host runtime build verifies the exact Espressif revision. The parity script
compares native unnormalized features with Google's full model and confirms that
the extracted CNN produces identical LiteRT output when given identical input.
Float FFT rounding may change a few input quantization bins by one step; the
script checks the maximum difference and number of changed bins. It reports
score drift instead of claiming bit-exact end-to-end inference.

`evaluate.py` uses bounded level normalization, the actual TFLite Micro reference
kernels, deterministic fixtures, and speech mixed at half/equal/double music RMS.
It reports per-window scores and desktop/micro runtime differences. These scores
are not calibrated probabilities. Evenly spaced fixture windows do not establish
continuous session stability or device inference timing. The S3 uses ESP-NN
kernels, which require separate target validation. See the commissioning report.

Private-room recordings are not needed by these tools and must not be collected
or retained without the user's explicit consent.

## Flashing through the existing P4 connection

`flash_via_p4.py` transports standard esptool through an explicitly enabled,
temporary native C++ P4 development bridge. It obtains a short-lived key from
the physical P4 console, never prints it, and exposes its esptool socket only on
desktop loopback. It refuses to open a serial port already held by another tool.
Keep exclusive port ownership until each flash finishes and verifies its hash.

Build the temporary P4 with `idf.py -DWAVEFORM_DEV_FLASH_BRIDGE=ON build`.
Pass `--plan /tmp/plan.json`, a JSON array of esptool argument arrays, containing
`flash-id`, `read-flash` or `write-flash`. A `diagnostics` row takes a duration
of 1–120 seconds and an output path; it observes live S3 diagnostic text while
the normal P4 decoder continues running, without resetting the S3 or changing
its activity state. P4 console text is saved beside it with `.p4.log` appended.
PCM capture lines are discarded; no audio clips are requested by this tool.

Always back up the S3 and inspect its partition table/OTA records before writing.
Preserve NVS and OTA metadata; standard full-flash arguments include an initial
OTA record and must not be used blindly for an existing device. Rebuild and
flash the final P4 with `WAVEFORM_DEV_FLASH_BRIDGE=OFF` after commissioning.
