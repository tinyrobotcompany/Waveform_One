# Complete pre-filter rollback — 11 October 2026

## Scope and reason

The earlier partial rollback disabled classifier inference but retained experimental capture,
transport, fingerprint and retry behavior. It did not satisfy the requested restoration.
This rollback restores the audio and recognition implementation from
`f4e67520f0cef3f2784926dd7f5936f79d014bca`, the parent of classifier commit `92103a6`.
Work remains in `feature/p4-recognition-stability` at `.worktrees/p4-recognition-stability`.
Main is untouched; the classifier was never merged into main.

Restored together:

- Original spectral sound gate and legacy activity telemetry; no music/speech classifier.
- Original live eight-second raw PCM capture, framing, USB transport and completion handling.
- Original fingerprint implementation and compiler settings; no short-clip or rolling-history experiments.
- Original recognition scheduling, retry policy and match expiry; no newer transition/latency experiments.
- Original S3 dependencies, partition table and generated configuration from baseline defaults:
  160 MHz CPU, no PSRAM, original instruction/data caches, no model partition or inference library.
- Original baseline host tests. Added classifier, codec, history, request and bridge code/tests are removed.

UI, phone remote, fonts/Unicode and search animations are preserved because they already exist
in the baseline commit. The sole runtime exception is the independent P4 hosted Wi-Fi RX
buffer-ownership safety backport, including its exact dependency pin and regression tests.
It prevents a double free and does not alter audio activity, capture or recognition policy.

Original source and uncommitted work were archived before restoration in
`/tmp/waveform-complete-rollback-source.tar.gz` and
`/tmp/waveform-complete-rollback.patch`. These local temporary backups are not durable release artifacts.

## Verification

- Exact source comparison passed for 62 restored/removed firmware/tool paths against `f4e6752`.
  CMake and test-runner differences contain only the independent Wi-Fi ownership fix/tests.
- Full firmware host suite passed with ASan/UBSan, including the Wi-Fi ownership regression.
- ESP-IDF 6.1 S3 and P4 builds passed; neither final application map contains classifier,
  model-inference, rolling-history, compact-codec or development-bridge symbols.
- `git diff --check` passed.

Application SHA-256:

- S3: `bc0f3d94e36eb09f45571c22e884e7576317971edcecc6ba1aa956a599d70106`
- P4: `430c894887dd322bf7bf188fe67dab0eb9f473c63f0bceabdc2cac79ab3bf04e`

## Deployment and remaining acceptance

Both devices were flashed successfully; esptool verified all written hashes.
S3 baseline bootloader, original partition table and application were written without erasing
NVS or OTA selection data. The removed model partition's old bytes may remain in unused flash,
but there is no partition entry or linked code that can load them. P4 final application was
written after the temporary maintenance transport was used to flash S3.

The announced 35-second startup check passed: S3 legacy telemetry was received, the phone
remote came online at 8.9 seconds, no fatal/reset-loop marker appeared, no classifier trace
or maintenance key was emitted, and port 8765 refused the connection. Two original raw
captures completed with 256000 bytes in 8.111 and 8.109 seconds. No test music was played.

Activity alternated between quiet and playing during this check. This is a limitation of
the restored sound gate, which does not discriminate conversation/background noise from
music; the check does not prove a stable quiet room, stable song recognition or latency.
Do not interpret successful transport/startup as successful catalogue recognition.

Local verification logs:
`/tmp/waveform-complete-rollback-{host-tests,s3-build,p4-build,s3-flash,p4-flash,startup}.log`.
The final startup result is `/tmp/waveform-complete-rollback-startup-result.log`.
The temporary desktop Python tool is only a flash transport; both device applications are native C++.
No private microphone recording or audio upload is used for validation.

This restores the requested historical behavior, not proof that it meets the five-second target.
The baseline uses eight-second live capture and its original post-match pauses/expiry policy.
Continuous-track freshness and broad genre recognition remain unaccepted requirements.
No controlled song test or timing claim is made by this rollback.
