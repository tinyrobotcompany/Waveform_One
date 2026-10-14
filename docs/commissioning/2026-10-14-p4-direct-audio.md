# P4 onboard microphone comparison

Branch: `feature/p4-direct-audio`, based on recognition commit `b9dd4a5`.
This experiment keeps the S3 LED controller and existing USB capture code.

## Capture path

The Waveshare 7B BSP 3.0.1 initializes the ES7210 ADC over the board I2C bus
and reads its I2S input. The [manufacturer's example](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/tree/main/examples/esp-idf/06_i2s_codec)
uses 16 kHz, 16-bit stereo through `esp_codec_dev`. We use that format and
select channel 0 without mixing or rescaling; the existing recognition and
presence readers receive 16 kHz mono through a separate 16-second ring.
Microphone reads run in their own task, independently of network requests.

Build settings under `Waveform recognition audio`:

- `CONFIG_WAVEFORM_P4_MIC=y`: onboard input; disabled retains S3 USB input.
- `CONFIG_WAVEFORM_P4_MIC_CHANNEL=0`: one stereo input selected, range 0–1.
- `CONFIG_WAVEFORM_P4_MIC_GAIN_DB=24`: initial adjustable gain, not a calibrated result.

Source selection is compile-time for this first comparison. A change requires
rebuilding and flashing the P4. No automatic microphone fallback obscures which
source was tested. USB controller events do not gate the onboard source.
Successful ADC reads establish readiness; read errors invalidate the ring and
readiness, close the codec and retry after one second. Initialization failure
is logged without rebooting the screen. Installed codec reads have a one-second
timeout. No speaker playback, voice enhancement or software AGC is enabled.

## Validation and comparison

Host tests cover signed stereo selection, channel isolation and bounds. Existing
recognition response-policy tests still apply. Build/test logs are under
`build/p4-direct-audio/`. Neither compilation nor these tests establish audio
quality on the physical board.

First verify startup source, sample rate (approximately 80,000 selected samples
per five seconds), noise level and clipping. Then use the same song, speaker
position and volume on each source. Record start, identified track, song change
and stop timestamps. The LED still follows the S3 microphone during this stage;
its behavior does not demonstrate P4 audio quality. Recognition timing, provider,
album cache and silence thresholds are unchanged from the baseline.

Limitations: onboard gain/channel and acoustic placement need hardware checks.
The codec adapter does not expose DMA-overflow counters through its public API;
sample-rate diagnostics can reveal loss but cannot locate every missed sample.
The existing loudness-based silence classifier remains an independent risk.

## Initial hardware bring-up

The P4 application was flashed over `/dev/cu.usbmodem5CF71081441`; esptool
verified the written hash. The S3 was not flashed. Full repository tests and
the native P4 build passed before flashing (application size `0x754410`).
Runtime diagnostics confirm ACRCloud and `source=p4_es7210`, stereo channel 0
at 24 dB gain. Initial capture intervals delivered 80,384 samples in 5.000690 s
and 80,128 in 5.009999 s. Initial 200 ms quality snapshots measured -60.5 and
-58.8 dBFS with zero rail samples; the presence state stayed quiet. These are
startup observations, not recognition accuracy or a long-run capture guarantee.
The baseline USB controller logged a stream restart, which did not interrupt
the independent onboard capture. A busy GUI update was retried successfully.
The device rejoined home Wi-Fi. Song trials are pending user playback.

### First song: Dancing Queen — ABBA

The user reported playback, then explicitly reported stopping. During the
playback interval channel 0's sampled levels were mostly -52 to -57 dBFS;
the background estimate rose with them. Presence remained false and no service
request started. After the stop, levels returned to about -60 dBFS. This proves
local request admission did not open; it does not prove a provider failure,
wrong ADC channel or poor microphone hardware. The five-second snapshots do
not capture an exact acoustic onset time.

Next diagnostic: measure both stereo channels over every five-second interval,
including DC-corrected RMS and rail-sample percentage. Source, selected channel,
24 dB gain and detector thresholds remain unchanged. No raw recording is saved.
Tests exercise constant DC, known alternating amplitude, full-scale rails and
80,000-sample accumulation, alongside stereo channel selection. Native firmware
build passed; channel comparison needs another announced playback trial.

### Continuous recognition experiment

The second playback trial showed both inputs responding, with channel 1 about
2–3 dB higher, but the local presence gate still did not open. The user explicitly
prioritized recognition before background rejection. The candidate now enables
`CONFIG_WAVEFORM_CONTINUOUS_RECOGNITION=y`: arriving PCM establishes recognition
activity, independent of the level detector. The level detector still runs for
diagnostics, but its start/stop votes neither reset the session nor hide matches.
The worker's `level_sound` field represents recognition activity in this mode;
the `Audio presence sound` diagnostic remains the actual acoustic vote.

Network/controller failures, stream-generation changes, skipped samples and
three seconds without new PCM still invalidate results. Request spacing, budget,
service backoff and provider remain unchanged. The S3 LED behavior is unchanged.
After music stops, an album may remain visible: acoustic stop handling is
explicitly deferred in this experiment, not claimed fixed.

The production activity helper is tested with zero-valued PCM while the actual
level detector stays quiet: requests and match admission remain eligible, and
stream loss/disconnect still invalidate them. Full offline repository checks,
ASan/UBSan regression tests and the native P4 build passed before flashing.
Application size: `0x754510`; logs use the `continuous-` prefix in the build
artifact directory. End-to-end provider success must be checked on hardware.

Continuous-mode hardware observation: flash hash verified. At 15:30:18.517 and
15:30:20.359 ACRCloud returned "Lay All Your Love On Me" / ABBA (1557 ms and
918 ms service calls). Actual acoustic presence still read false, but requests
and match display proceeded. User confirmation of the currently playing title
is pending; these are not end-to-end onset timings or a verified accuracy claim.
Several following windows returned NoMatch.

At 15:30:34.667 the device rebooted on
`sdio_push_data_to_queue:702 (pkt_rxbuff)`. Inspection of the pinned ESP-Hosted
source shows this is a failed receive-buffer allocation assertion. An earlier
15:19:27.336 assertion at `sdio_rx_get_buffer:671 (*buf)` occurred before the
continuous-mode change. Memory pressure/fragmentation versus a leak has not
been distinguished; do not attribute the cause to microphone gain or ACRCloud.
Further runtime memory evidence is needed. Stability is not established.

### Artwork failure and repeated restarts

The user confirmed the matched title was correct and that title/artist were
visible, with only the artwork missing. Catalogue HTTPS setup failed immediately
after recognition; later a successful catalogue selection was followed by an
SDIO allocation assertion while retrieving additional data. The user observed
repeated blue boot screens. The pinned SDIO allocator explicitly requests
internal DMA-capable RAM. The current 16 KB ordinary-allocation threshold sends
many TLS/application allocations into that same limited internal heap.

Candidate: lower the ordinary internal preference threshold to 1024 bytes and
reserve 65536 internal bytes for explicit internal/DMA allocations. Keep default
mbedTLS allocation, since this project previously observed early-boot failures
with forced PSRAM-only TLS allocation. Add internal free/largest, DMA largest and
PSRAM free diagnostics every five seconds. This addresses the evidenced resource
class; runtime validation must establish whether it prevents exhaustion.

Source review also showed that same-song matches never retried failed cover
enrichment. Missing covers now retry on fresh verified matches, no faster than
30 seconds after a failed attempt. New tracks still resolve album selection;
existing cache hits preserve their artwork. Successful JPEG decode is logged.
No driver assertion was removed or suppressed. Build/tests and subsequent
hardware observations use `memory-` artifact names.

## Combined colour/audio checkpoint — 2026-10-14

PR #9 was merged into main at `a6c12786d2a426c11e43c13a314368da251edd6a`.
Claude built the combined firmware in `.worktrees/album-colours-audio`, using
this worktree's audio changes and local sdkconfig. The colour stack fix is
`d3cd3fb`; the follow-up include fix is `ab270fd`. The S3 was not flashed.

Verified in the combined startup log: `continuous_pcm`, `p4_es7210`, 16 kHz,
16-bit stereo, selected channel 0, gain 24 dB. The user confirmed recognition
was working very well after this flash. Logs contain successful 400 x 400
artwork decodes. The recorded run spans approximately 8.5 minutes without a
restart; it is not a long-duration stability test.

The original logs are in the primary checkout's `build/album-colours-audio/`.
A local snapshot of the combined binary, ELF, generated sdkconfig header and
flash arguments is preserved in `build/album-colours-audio/preserved-2026-10-14/`.
Application binary SHA-256:
`ee5d89e62156f95140591d9364ca6d317019f3fafbc5a731805053b6f4a3eec8`.
This identifies the handed-off build artifact; no new device readback was done.

Required experimental build settings (local sdkconfig, not production defaults):

```text
CONFIG_WAVEFORM_P4_MIC=y
CONFIG_WAVEFORM_CONTINUOUS_RECOGNITION=y
CONFIG_WAVEFORM_P4_MIC_CHANNEL=0
CONFIG_WAVEFORM_P4_MIC_GAIN_DB=24
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536
CONFIG_MBEDTLS_DEFAULT_MEM_ALLOC=y
```

Do not flash main/PR #9 alone: it does not contain the onboard input or
continuous-recognition experiment. Integrate the audio checkpoint with main,
build and verify these settings and the colour stack fix before any next flash.
Retain the running firmware while investigating the remaining issues.

### Remaining evidence and limits

Internal free memory fell from 93,727 to 52,535 bytes; largest DMA block fell
from 48,128 to 3,968 bytes. The final roughly two minutes were stable at the
latter value. The log contains 90 deferred GUI update warnings, including
bursts approximately 30 ms apart. This confirms contention, not lost updates.

Pinned ESP-Hosted `host/drivers/mempool/mempool.c` retains returned buffers in
its free list; SDIO streaming also grows reusable receive buffers. Retention
can explain a declining heap followed by a plateau, but attribution requires
allocation evidence. Do not label this a proven leak or declare it harmless.
The colour implementation already skips palette work when the target palette
is unchanged. Increasing the GUI lock timeout while holding the session mutex
would risk delaying audio/session handling; inspect delivery scheduling first.

Acoustic stop detection remains deliberately bypassed. This checkpoint makes
no claim that albums clear promptly on silence, that background sound is
rejected, or that recognition meets a measured end-to-end latency target.
