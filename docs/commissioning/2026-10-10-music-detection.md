# Native music detection prototype — 10 October 2026

## Deployment state

Worktree: `.worktrees/p4-recognition-stability`, branch
`feature/p4-recognition-stability`. The existing connection is Mac → P4 UART
(`/dev/cu.usbmodem5CF71081441`) → S3 native USB Serial/JTAG. The S3 is enumerated
by the P4 USB host, so it does not appear as a separate Mac port. No cable change
is required. A temporary native C++ P4 development bridge successfully identified
the S3 and read its full 16 MB flash before any S3 write.

Read-only identification reports ESP32-S3 revision 0.2, 16 MB flash and embedded
8 MB AP_3v3 PSRAM. The private backup is
`/tmp/waveform-s3-before-music-detection.bin`, 16,777,216 bytes, SHA-256
`c37e33752f019a44337cd80a8bcd6bb531a4310ca35b09294a054af32917b495`.
Both existing 3 MB OTA slots and all NVS/PHY/OTA metadata offsets match the new
partition layout. CRC-valid OTA sequence 1 is valid in slot 0; sequence 2 in slot
1 is invalid. Commissioning targets slot 0 while preserving both OTA records.
The S3 bootloader, partition table, active app and model have now been installed
through that connection, with esptool hash verification for every written image.
NVS, PHY and OTA metadata were not written. A 45-second live diagnostic sample
completed 37 native evaluations: minimum 1,130 ms, median 1,138 ms, maximum
1,145 ms. No I2S-drop warnings, crash or reset markers appeared in that sample.
Scores ranged from 0 to 0.008 and settled on quiet; the room's actual sound was
not independently labelled, so this is runtime evidence, not music accuracy.
The final P4 image has also been flashed and hash-verified with the bridge
disabled. On 11 October the user confirmed speech did not trigger detection,
music did trigger it, and a real song subsequently matched in about ten seconds.
The track title was not reported. Broader room and prolonged-playback acceptance
remain open.

The user subsequently set a mandatory five-second recognition target. This is
not yet achieved: the successful observed match took about ten seconds. The next
performance work must overlap classification with a bounded rolling audio buffer
and evaluate shorter captures/faster fingerprints without sacrificing accuracy.

### Longer-running scheduling failure and correction

A later concurrent S3/P4 trace exposed 19 I2S-drop reports in one minute and
44 unknown messages among 113 classifier heartbeats. The raw S3 messages and P4
state transitions agree: this was audio discontinuity on the S3, not P4 parsing
or USB heartbeat corruption. The first short, freshly booted sample did not
establish long-running stability.

The audio main task and inference initially shared priority 1, and the main task
performed synchronous diagnostic printing. The correction gives audio priority
3 on core 0 and moves routine console output to a priority-1 diagnostic task.
That task consumes a one-item overwrite queue of copied frame/status snapshots;
console delays discard older diagnostics rather than blocking audio. Overflow
counts remain reported and discontinuities still invalidate classifier/capture
data. Inference remains priority 1 on core 0; LED scanning remains on core 1.
Corrected S3 app SHA-256:
`6d4de76d0c974cf4f9155d4d333304bf4b29c3f5254551f4f66ea8c5a2f34bd7`.
The build and complete firmware host suite passed; this app has been flashed and
hash-verified. The subsequent two-minute target trace completed 84 evaluations
at 1,127–1,147 ms (median 1,138 ms), with zero I2S-drop reports and zero crash
markers. Quiet/unknown transitions still occur while collecting fresh evidence
after the separate quiet gate changes; these are not false music detections.

Public speech and jazz fixtures were played through the Mac audio output. The
music fixture produced music-positive classifier results and a successful
256,000-byte capture; speech alone did not produce a music state in the sampled
trace. A longer jazz playback produced three successful captures, 255–257
fingerprint peaks and verified-TLS service responses in 1.064–1.228 seconds. All
three responses were valid no-match results for that public fixture. This is
end-to-end pipeline evidence, not a successful catalogue match. The user then
confirmed a separate real-song match at approximately ten seconds.

Final P4 SHA-256:
`103c95c1f89513be23fd1d4e95d6ef5b1de062df848e0367e8e5902d22fb038f`.
Its flash hash was verified; the bounded production startup trace found no
crashes, no development key, and TCP port 8765 closed. No old firmware was
restored. Both devices retain native music detection.

The development bridge requires a random key available only on the physical
console, expires after ten minutes, refuses takeover during a capture, and
restores normal S3 operation on connection loss. ROM flashing and live diagnostic
modes share bounded transport buffers. It is compiled out by default; final P4
firmware must use `WAVEFORM_DEV_FLASH_BRIDGE=OFF`. The desktop helper only transports
standard esptool traffic and saves diagnostic logs; it is not a music runtime.

## Implemented path

The S3 continuously downsamples independent microphone samples to 16 kHz,
collects 0.975-second windows in two bounded buffers, and evaluates YAMNet in a
native TFLite Micro task on core 0. The existing audio/FFT consumer never waits
for inference. Core 1 continues scanning the HUB75 panel. Full buffers are not
overwritten while inference reads them. Unavailable buffers drop classifier
samples rather than blocking I2S. Audio discontinuities invalidate classifier
generations; late results are rejected. Stale results become unknown.

The classifier's bounded level normalization changes only its own float feature
input. The raw eight-second capture used for fingerprints, FFT display levels,
LED styles and GPIOs remain independent. Music, singing and rap outputs are
eligible; speech is not a veto. Two positive windows enter music, sustained
negative evidence for five seconds ends it, and uncertainty holds a preceding
positive for at most eight seconds. The existing energy gate provides a separate
1.5-second quiet confirmation after its 0.4-second close hold. Silence invalidates
previous music evidence; speech after a pause must earn a new decision.

The S3 emits `WF1 MUSIC MUSIC`, `WF1 MUSIC QUIET` or `WF1 MUSIC UNKNOWN`
heartbeats. Its LED frame gating and the P4 lookup/session gate use this same
decision. The P4 ignores the older energy diagnostic lines, accepts only exact
classification messages, and changes missing/stale heartbeats to unknown after
three seconds. Unknown pauses recognition without immediately invalidating an
existing album. Existing retry/Retry-After policy is unchanged.

## Model and resources

Google's pinned YAMNet classifier is Apache-2.0; source and modified graph hashes,
size and input quantization are in
`firmware/esp32/common/music_model/provenance.json`. All CNN weights and all 521
classes remain. Native C++ replaces the unsupported float waveform front-end.
The model is SHA-256/schema checked before use.

The model occupies 4,046,312 bytes in a new 4 MB data partition at `0x620000`.
The two 3 MB OTA slots, NVS, OTA metadata and PHY offsets are unchanged. Standard
IDF flash arguments include the model partition. **App-only OTA cannot install
the new partition table/model on an older device**: initial commissioning must
install those assets explicitly. Do not erase NVS or reset OTA metadata blindly.

Native host TFLite Micro reference kernels used a 242,592-byte arena for this
model; one measured desktop inference was 42,958 microseconds. These are host
measurements, not S3 performance evidence. The target reserves a 320 KB arena,
two 31,200-byte PCM buffers and approximately 6 KB feature workspace in PSRAM.
Allocation must not consume internal DMA/task memory. Missing PSRAM/model or
initialization failure reports unknown without a reboot. An OTA image is not
marked healthy until classifier initialization succeeds.

S3 defaults request octal PSRAM at 40 MHz and CPU 240 MHz. Missing PSRAM is ignored
at boot to prevent a boot loop. The installed module reports embedded 8 MB PSRAM;
successful inference confirms usable allocation. Target processing takes about
1.14 seconds per 0.975-second window, so the bounded producer occasionally skips
classifier samples instead of blocking audio. Initial music confirmation needs
two positive evaluations and adds time before the existing recognition capture.
Further performance/recognition-latency acceptance is required. If the S3 cannot meet memory/timing requirements, move inference
to the P4 with a bounded continuous PCM transport rather than changing thresholds
and claiming music detection.

## Verification and fixture evidence

Both final S3 and P4 builds passed with ESP-IDF 6.1. Repository Node and Python
suites passed, as did Rust after rerunning outside the sandbox to allow its local
HTTP test server to bind. All firmware host suites passed after the final changes.
Firmware host tests include
ASan/UBSan, classifier input bounds, normalization, entry/exit/uncertainty policy,
speech interruptions, concurrent classifier buffer ownership and the production
P4 USB decoder/capture path.

Built application SHA-256 values:

- S3: `6d4de76d0c974cf4f9155d4d333304bf4b29c3f5254551f4f66ea8c5a2f34bd7`
- Final P4 (bridge disabled): `103c95c1f89513be23fd1d4e95d6ef5b1de062df848e0367e8e5902d22fb038f`

Native unnormalized preprocessing matched Google's original quantized feature
tensor with differences of at most one quantization step and under 1% of bins in
253 tested windows (maximum 20 changed bins out of 6,144). The extracted CNN
produced identical LiteRT output when fed
the original model's exact quantized features. Float FFT rounding and microkernel
rounding can change scores; this is not bit-exact end-to-end inference.

Offline fixture results are saved in
[`2026-10-10-music-fixtures.tsv`](2026-10-10-music-fixtures.tsv). Each music case
contains 32 evenly spaced windows. Results use native normalized features and
the pinned Espressif TFLite Micro reference kernels, not a Python music service.

- Speech: 0/4 music-positive windows; silence: 0/8; white noise: 0/8.
- Instrumental jazz: 31/32 positive; orchestral music: 28/32; vocal music: 32/32.
- At 1% input amplitude: jazz 31/32; orchestra 28/32; vocals 32/32.
- Equal-RMS speech mixed with music: jazz 20/32; orchestra 27/32; vocals 31/32.
- Speech twice music RMS: jazz 13/32; orchestra 19/32; vocals 30/32.

These are preliminary per-window observations, **not an accuracy estimate**.
Speech masking instrumental music remains difficult. Evenly spaced windows do
not test temporal retention. Scores are not calibrated probabilities. Maximum
score drift between desktop LiteRT and micro reference kernels reached 0.171875
in one difficult mixture; target ESP-NN outputs must be evaluated independently.

Fixture sources (downloaded only to `/tmp`, audio not added to the repository):

| Fixture | Source | SHA-256 |
| --- | --- | --- |
| Speech | [TensorFlow test audio](https://raw.githubusercontent.com/tensorflow/tflite-support/master/tensorflow_lite_support/cc/test/testdata/task/audio/speech.wav) | `219f55927c62af71fa7dc29581b647ade375286498dd0e342cc07c72cc8edc04` |
| Jazz | [Kevin MacLeod — Vibe Ace](https://librosa.org/data/audio/Kevin_MacLeod_-_Vibe_Ace.ogg), CC-BY-3.0 | `6c23aed3dd5aa57f2b1652ecab68d15d9b82ad257f54e639eb2880ca09bc118a` |
| Orchestra | [Hungarian Dance No. 5](https://librosa.org/data/audio/Hungarian_Dance_number_5_-_Allegro_in_F_sharp_minor_(string_orchestra).ogg) | `919b48aa4cc66a0357d2cd5728664c5ab8f15c4b3469460df4b59470d35d3e49` |
| Vocals | [Karissa Hobbs — Let's Go Fishin'](https://librosa.org/data/audio/Karissa_Hobbs_-_Lets_Go_Fishin.ogg) | `27b3667c396c1831511aa3c415fcf582b6e8be560cafb844c5b67b76b72c1cb3` |

Preparation, pinned development dependencies and reproduction commands are in
[`tools/music_detection/README.md`](../../tools/music_detection/README.md).

## Required hardware acceptance

- Identification and full S3 backup completed through the unchanged P4 connection.
  Preserve the working P4 backup at
  `/tmp/waveform-p4-before-music-detection.bin`.
- Verify S3 allocation, per-window latency, no I2S overflows/watchdog resets,
  uninterrupted LED scan/control/capture, and classifier heartbeats.
- Verify both firmware versions together; an old S3 does not emit classifier
  heartbeats, so the new P4 would pause recognition. Do not flash the P4 alone.
- Test real silence, fan/traffic noise, conversation/TV speech, quiet jazz,
  classical/solo instruments, percussion, rap/a cappella and voice/music mixtures.
- Measure initial recognition latency, stop latency and album retention through
  voices/noise, Wi-Fi/service failures and transitions without a silent gap.
- Add a rolling recognition prebuffer if entry confirmation delays recognition
  materially. Current classifier windows do not provide a prebuffer to the
  eight-second capture protocol.
- Validate long-duration operation before marking the backlog item complete.
