# Four-second recognition commissioning — 12 October 2026

## Scope

The colour interface remains enabled. The music-only classifier remains disabled.
The S3 accepts explicit `CAPTURE 4` and `CAPTURE 8` requests; legacy `CAPTURE`
still requests eight seconds. The P4 explicitly requests four seconds and requires
all 500 checksummed packets, the 64,000-sample header and matching END before
using the PCM. An older S3 header fails safely rather than truncating its capture.
Capture ownership, cancellation, USB timeouts, activity gating and recognition
retry/retention policy are unchanged.

Only fingerprint.cpp receives `-O3`, without fast-math or algorithm changes.
Previously observed P4 logs spent roughly 8.1 seconds capturing and 1.3–1.6 seconds
fingerprinting; this was additional to service and artwork latency. Shortening
capture does not prove total five-second recognition on hardware.

## Verification before flashing

- Firmware host tests passed with AddressSanitizer and UndefinedBehaviorSanitizer.
- Tests cover four-second command parsing, full completion, overflow recovery,
  request ID reuse and rejection of unsupported durations. Legacy eight-second
  capture remains tested.
- P4 tests cover complete four-second framing, stale/wrong-length headers, wrong
  END counts, checksum/sequence errors, canaries, timeout and disconnect cleanup.
- All 15 production four-second signatures (five genres, three offsets) exactly
  match the benchmark outputs with `-O3` enabled.
- Benchmark now pins its historical eight-second baseline to commit
  `29f8bcd35256b6f07f382117c8ca9d689243ca40`, independently of current firmware.
- Both ESP-IDF 6.1 firmware builds passed. Compile commands confirm source-specific
  `-O3` and no fast-math.
- Node, Python and firmware repository checks passed. Rust test server requires
  execution outside the filesystem sandbox; that rerun passed.

## Hardware procedure and evidence

User authorised installation and confirmed quiet-room availability. Music is
paused for microphone calibration on the S3 restart. Both images are application
updates; no partition table or saved configuration erase is requested. S3 is
reached through the P4 native maintenance bridge because it is connected to the
P4 USB host, rather than appearing as a separate Mac serial port.

Artifacts and diagnostic logs are kept in
`build/music-recognition-benchmark/recognition/` in the primary project folder.
`firmware-sha256.json` identifies the exact binaries. Desktop Python is the
ESP-IDF/esptool flashing and serial diagnostic transport; both board apps are C++.
No microphone audio is persisted. Live recognition timing and reliability remain
pending installation and announced, user-controlled music trials.

### Installation completed

All three app writes (maintenance P4, final S3, final P4) exited successfully with
`Hash of data verified`. Final SHA-256 values:

- S3: `bc0a43a6a65a4cec50b7dc0a167d90bd5a8107695038e0e09ce20e843cc9fbec`
- P4: `ac7b0f83af100cac5e62385f299783936fc2a6c05880f4939908a4eda990244c`

Final P4 startup logs confirm display/touch initialisation, S3 style
acknowledgement and home Wi-Fi at `192.168.1.26`. Initial full captures contain
128,000 PCM bytes and take 4,101 ms and 4,014 ms: the new paired protocol is
running successfully on hardware. Startup observation had no crash/assert
markers. The first ambient capture's fingerprint took 1,095 ms and service
lookup took 2,469 ms, returning NoMatch. This is not a controlled music trial or
a five-second recognition result. Baseline sound activity still toggles in the
room; no music-only filter was enabled. Quiet-room calibration was requested
during controller restart, but the P4 log does not expose the S3 calibration
thresholds, so no quantitative calibration claim is made here.

A single bounded, redacted serial monitor remains open for the announced music
trial, avoiding additional monitor-induced P4 resets. User-controlled song
identification timing and success rate remain to be measured.

### Recognition delay investigation

The user reported a very slow eventual match for Girls on Film by Duran Duran.
The previous 30-minute monitor had expired before this trial: that trial has no
serial timing evidence. Do not use earlier ambient logs as its measurement.
Earlier logs contain 168 successful full captures, 61 completed fingerprints,
48 lookups (all NoMatch) and 335 activity transitions. This shows substantial
work invalidation in ambient conditions; it does not establish the cause of the
Duran Duran result. Source confirms quiet transitions invalidate the session,
NoMatch waits 15 seconds, and first failures wait 60 seconds unless the service
requires longer.

A diagnostic P4 build is prepared, but has NOT been flashed. It logs request IDs,
session cancellation stages, retry durations and aggregate PCM RMS/peak/DC/rail
clipping counts, without storing audio or changing detection/retry behaviour.
Quality-stat boundary/signed sample tests and all firmware ASan/UBSan tests pass;
ESP-IDF 6.1 P4 build passes. The next serial monitor is bounded to eight hours
and 32 MiB and includes UTC timestamps so it can cover user response delays.
Await the requested pause confirmation before flashing/reconnecting UART or
starting the repeated live trial. Preserve the disabled classifier and colours.

Diagnostic P4 firmware was subsequently installed after the user's readiness
reply. App-only flash completed with verified hash. Startup logs confirm display
and touch initialisation and Wi-Fi ready at 192.168.1.26. The persistent UTC-timed
serial monitor is now running for the announced repeat of Girls on Film; no
controlled trial result is available yet. S3 firmware and calibration policy
were not changed by this diagnostic P4 update.

### Live trial confirmations and next optimisation

- User confirmed Girls on Film / Duran Duran correctly and described recognition
  as fast. Initial device phases were 4,101 ms capture + 1,188 ms fingerprint +
  553 ms lookup. Physical start was not synchronised with the message timestamp.
- User confirmed Inception / McCoy Tyner's song and artist, reporting about five
  seconds by their count, but subsequently reported the album was wrong. Device
  phases were 4,101 + 1,189 + 1,320 ms. Displayed/expected album values are pending.
  Correct song identity does not establish correct release/album metadata.
- The normal pause is now five seconds after either a match or a miss, replacing
  30/15 seconds. Failure backoff and Retry-After handling are unchanged, with
  regression tests covering session changes and cooldown preservation. This
  yields approximately 10–12-second request cycles at current measured costs,
  rather than 36-second matched cycles. It is not a five-second track-transition
  guarantee, and the higher request cadence needs live rate-limit validation.
- Exact normalisation avoids software double multiplication for each of 64,000
  samples, using exact integer 1x/32x gains or cached fractional magnitudes. It
  adds 52,004 bytes to the PSRAM workspace, not internal SRAM. Exhaustive signed
  sample tests cover threshold boundaries, extreme values and workspace reuse.
  Forty-five four-second fingerprints (five clips, three offsets, three signal
  levels) are byte-identical to the pre-optimisation executable.
- Profiling separately measures setup, FFT, spectral spreading, peak extraction
  and scheduler yields. The host reference signature test confirms profiling
  leaves the signature unchanged.
- All firmware ASan/UBSan tests and the ESP-IDF 6.1 P4 build pass. The optimised
  binary is prepared as `waveform_p4_optimised.bin` with its SHA-256 recorded
  separately. This build has NOT yet been flashed or timed on hardware. No S3
  change is needed. The music-only classifier remains disabled.

### Optimised build installed

After the user confirmed music was paused, the previous serial monitor was
identified and stopped. Previous runtime logs were preserved as
`pre-optimisation-diagnostic-runtime.log`. The P4 application-only update
completed with verified flash hash. Installed binary SHA-256:
`7eb8d46b07b68bb91636c4efc724941102a267f569ef7081934e45b2a4065aa5`.
Display/touch, controller and Wi-Fi startup passed with no observed crash/assert
markers. The UTC-timed monitor writes `optimised-runtime.log`.

Ambient startup requests confirm NoMatch now schedules approximately 5,000 ms
rather than 15,000 ms. Two profiled ambient fingerprints took approximately
1.18 seconds overall: setup 55 ms, FFT 823–825 ms, spectral spreading 138–142 ms,
peak extraction 95–98 ms, scheduler yields 59–61 ms. Gain normalisation alone
has not shown a large overall reduction on hardware. The newly measured FFT
stage is the principal processing bottleneck. A controlled music comparison
with this build is pending. The classifier remains disabled.

### Higher-cadence rate-limit finding

The five-second normal pause generated repeated lookups from ambient audio while
the user said music was paused. At 15:22:23 UTC request 7 received HTTP 429 with
no Retry-After header. The existing failure schedule correctly imposed 60 seconds
and survived activity changes. Request 8 began at 15:23:23.737 and matched at
15:23:30.060, near the user's next playback acknowledgement at 15:23:25. This
trial has no synchronised physical start timer. Phases were capture 4,101 ms,
fingerprint 1,156 ms and lookup 1,193 ms; artwork took 716 ms. Subsequent request
9 missed and request 10 matched. User confirmation is pending.

The naive higher service-request cadence is not accepted: local sampling and
external lookup cadence must be separated, and ambient audio must not exhaust
the request budget before music starts. Preserve service cooldowns. Do not
represent a five-second local pause as a reliable higher-rate identification
solution. Gain caching alone has not produced a material processing improvement;
the FFT remains the measured CPU bottleneck.
