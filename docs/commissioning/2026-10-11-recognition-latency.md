# Recognition latency — 11 October 2026

> Historical experiment: the active firmware is now restored to the pre-filter baseline. See [complete rollback](2026-10-11-complete-filter-rollback.md); retained changes described here are no longer deployed.

Later genre profiling and deployment status are recorded in
[the genre reliability report](2026-10-11-genre-reliability.md).
The results and hashes below describe the earlier commissioning run.

Branch: `feature/p4-recognition-stability`; worktree:
`.worktrees/p4-recognition-stability`. Mandatory target: correct title, artist and
album within five seconds of audible music starting, including classification,
capture, fingerprinting and HTTPS. The earlier user-confirmed match took about
ten seconds. This document records the new path; the target is not established
by host tests or capture duration alone.

## Implementation

- S3 continuously records independently downsampled raw 16 kHz mono signed PCM
  in an eight-second ring. A separate owned snapshot prevents continued audio
  from overwriting a clip being transferred. These two fixed PSRAM buffers use
  512,000 bytes. Audio discontinuities and confirmed quiet discard history and
  invalidate active captures. Temporary uncertainty does not discard history.
- P4 requests a four-second recent clip after confirmed music. A valid no-match
  permits one eight-second fallback after the normal one-second scheduling
  interval. Subsequent misses use the existing 15-second pause. Failed capture,
  transport and HTTP requests retain failure backoff and Retry-After; a session
  change resets the clip plan without resetting that failure schedule.
- The new command is `WF1 <id> CAPTURE_COMPACT 64000` or `128000`. The header
  declares exactly the requested sample count; END must agree with every ordered
  packet and byte count. Legacy `CAPTURE` still records eight fresh seconds;
  `CAPTURE_RECENT` supports buffered hexadecimal packets for compatibility.
- Compact `DPCM` packets encode the first signed sample, zigzag/unsigned LEB128
  differences, then base64. This is lossless: P4 reconstructs all 128 original
  signed samples and checks the existing FNV checksum over their 256 PCM bytes.
  Payloads are bounded at 512 characters, including alternating signed extremes.
  Invalid padding, variable integers, reconstructed sample ranges, extra bytes,
  packet order or checksums reject the clip. The diagnostic helper drops both
  PCM and DPCM payload lines rather than saving microphone recordings.
- Fingerprinting accepts complete four- or eight-second clips, preserves the
  upstream peak/reference algorithm, and uses an optional 40,960-byte internal
  RAM FFT workspace. If that allocation is unavailable it falls back to PSRAM.
  Both buffers are released before HTTPS. Fingerprint and capture compilation
  use `-O3`. Music/speech policy, visualizer audio and external lookup gating
  remain independent of this transport optimization.
- Recognition checks session readiness every 100 ms and completion every 20 ms;
  active USB captures are polled every 10 ms. Completion retains its bounded
  deadline, cancellation and request-identity ownership checks.

## Verification

Verified: host firmware suite passed with ASan/UBSan, including rolling history,
snapshot ownership, discontinuities, short/legacy transport, malformed compact
payloads, 1,000 randomized lossless round trips and signed extrema. Four-second
and internal-workspace fingerprints match the independent upstream reference.
Existing speech policy, USB failure/disconnect, completion, HTTP and cooldown
tests also passed. Repository Node/Python suites passed; the Rust suite passed
outside the sandbox after a local-socket permission failure inside it.

Verified: ESP-IDF 6.1 builds passed for S3 and P4. Both app-only writes were
hash-verified through the existing connection; S3 NVS/model/OTA records were
preserved. Final P4 build has `WAVEFORM_DEV_FLASH_BRIDGE=OFF`.

S3 app SHA-256:
`95d67723cd4015463665865580a3ba2744234a1dc154a7a3200f9467e41080f3`.
P4 app SHA-256:
`d62b71965c273afc28c1f95025dc597ad066740e3f4a73c919e930c6f8dc53a8`.

An intermediate on-device trace completed a buffered 128,000-byte clip in
651 ms and its fingerprint in 1,203 ms, followed by a valid no-match response in
730 ms. This establishes pipeline timing, not catalogue-match accuracy. A
fallback interrupted by confirmed quiet returned AudioLost and scheduled
failure backoff, rather than using an incomplete clip. The short S3 trace had
no audio-drop or crash markers. Catalogue tests use Apple's public 30-second
preview of “To the Youth,” with private diagnostic timestamps; no room PCM is
saved.

Verified in the final production trace: preview process start at monitor
25.022 s; music telemetry at 27.428 s (2.405 s after start); complete compact
capture at 27.738 s (321 ms capture); fingerprint at 28.887 s (1,178 ms,
internal FFT allocated); successful service match at 29.818 s, **4.795 s after
preview process start**. Firmware publishes title/artist/album immediately after
that lookup, before artwork. Artwork was ready at 30.235 s, **5.212 s after
start**, so cover rendering has not demonstrated the same five-second target.
The UART trace confirms a match, but does not contain its song metadata; correct
title/artist identity and visible P4/phone timing still need confirmation.

The preview ended after 30.887 s. Music telemetry did not become quiet during
the remaining observation, so the second trial was correctly withheld instead
of claiming a clean stop/start test. Other room audio versus classifier silence
behavior needs clarification. Later refreshes still completed checked captures,
including the eight-second fallback, and produced matches; this does not prove
they matched the preview after it ended. No crash markers appeared in the
120-second P4 trace, and the development key was absent and TCP 8765 closed.
Private logs: `/tmp/waveform-latency-runtime.log` and
`/tmp/waveform-latency-events.jsonl`; neither includes raw PCM packets.

## Remaining acceptance

The five-second target needs measured successful catalogue matches, including
first-song and stop/start cases. Broader vocal, jazz, classical, quiet-volume,
voice-over-music, room noise and no-silence transition tests remain open, as do
long-running playback and disconnect stress. Public-preview playback starts
provide a conservative process-start timing reference; they do not substitute
for a broad set of real room tests. Service/network variation and any longer
fallback can exceed five seconds and must be reported honestly.
