# Music-presence and capture stability — 11 October 2026

> Historical experiment: the active firmware is now restored to the pre-filter baseline. See [complete rollback](2026-10-11-complete-filter-rollback.md); retained changes described here are no longer deployed.

Later genre profiling and deployment status are recorded in
[the genre reliability report](2026-10-11-genre-reliability.md).
The results and hashes below describe the earlier commissioning run.

Branch/worktree: `feature/p4-recognition-stability`,
`.worktrees/p4-recognition-stability`.

## Reproduced causes

The previous S3 runtime trace contains positive music scores interleaved with
quiet/unknown telemetry and a closed cleaned-spectrum gate. The audio task used
that display gate as silence evidence, invalidating the classifier generation
and P4 recognition session. Quiet music or musical bands absorbed into the
calibration floor could therefore blank both displays and cancel recognition.

A natural stop also completed an active capture as `AUDIO_LOST`. If completion
arrived before the next quiet heartbeat, recognition scheduled hardware-failure
backoff for a normal playback stop.

The first run of the corrected arbitration additionally reproduced two real I2S
overflow events at eight-second capture starts. Both returned `AUDIO_LOST` and
the first delayed subsequent recognition through failure backoff. Default I2S
DMA retained only 30 ms while recent capture copies its PSRAM snapshot under
the producer lock. This failure was not dismissed as a classification problem.

## Changes

- Music entry retains the existing two-positive classifier policy. The spectral
  display gate no longer starts or stops music. Raw energy can confirm silence,
  but cannot start music by itself. Only absolute silence can bypass the existing
  sustained classifier exit policy. The second hardware run demonstrated that
  two low scores below the calibrated room floor can still be music: the jazz
  preview returned to a strong positive score immediately afterwards. That
  shorter relative-silence shortcut was removed and covered by a failing-then-
  passing regression test. Fresh positive results protect music below the
  calibration floor. Stale results and genuine audio discontinuities still
  invalidate appropriate state.
- Only a confirmed music-to-quiet transition invalidates inference; startup
  silence no longer repeatedly discards the classifier's entry evidence.
- Classifier-confirmed music displays smoothed raw FFT bands on the existing
  fixed dB scale. The separate cleaned-spectrum gate cannot zero those bars.
- Normal cancellation uses `STOPPED`, preceded by quiet telemetry. P4 distinguishes
  it from hardware failures without shortening existing service/failure cooldowns.
- I2S DMA now retains 128 ms, or three FFT frames, using 24 descriptors of 256
  stereo frames (49,152 bytes of sample storage). Display audio calculations use
  `-O3`; actual overflows still invalidate captures rather than hiding lost audio.

## Verification

Firmware host tests passed with ASan/UBSan. Added tests cover quiet confirmed
music, genuine silence, stale results, startup entry preservation, negative
musical passages, independent LED levels and normal-stop completion ordering
through production USB transport. Retry tests preserve failure/service deadlines
while avoiding a new failure penalty on stop. Node/Python checks passed. Rust
checks passed outside the sandbox after the sandbox denied a local test socket.
ESP-IDF 6.1 S3 and P4 builds passed.

Hardware runs two and three use the public Apple previews of “Don't You Want Me” (The
Human League, Dare) and “To the Youth” (Christian Sands' trio recording), with a
public speech clip and silent gaps. The native maintenance bridge observes the
ordinary USB decoder; the desktop helper only flashes, plays fixtures and saves
diagnostic text. PCM/DPCM packets and the maintenance key are never saved.
The bridge is disabled in the final production P4 build.

The final 120-second run had zero I2S overflow/frame-rejection or crash markers.
Human League playback began at 12.002 s, music telemetry at 15.548 s, a checked
four-second capture completed in 311 ms, and the service matched at 17.829 s:
**5.827 s after playback process start**. Artwork completed at 18.233 s. The
validation log independently confirms the correct title and artist. Music
telemetry remained playing throughout the preview, including closed display-
gate sections. The speech clip did not start music.

Jazz playback began at 70.012 s and music telemetry at 77.999 s. Four- and
eight-second requests completed without audio loss, but three lookups returned
valid no-match responses. Music telemetry remained playing throughout the
preview, including three low scores followed by strong positive results. An
eight-second excerpt (8–16 s) decoded directly from the public Apple preview
also returned HTTP 200 with zero matches using the same native fingerprint
implementation. This isolates that specific miss from microphone/USB transport;
it does not establish which recordings the service catalogue contains or prove
all native fingerprints are correct. No private room PCM was retained.

A post-playback refresh was cancelled with `STOPPED`; the next song started
captures promptly after its music decision, without a minute-long failure delay.
Natural room silence was reported about nine seconds after both playback
processes ended, due to the sustained classifier exit. **The previous two-second
stop response has not been retained in this room test.** Absolute silence still
has the bounded 1.5-second fast path; false relative-silence exits are removed.
Fast room-silence discrimination remains a follow-up, alongside recognition
latency and missed catalogue results, rather than being reported as complete.

Final S3 app SHA-256:
`c91005335117dd5c81d67ebde10797497998b3903b95e35b98a5eb231995b039`.
Final production P4 app SHA-256:
`4d7835339c700db38e77cd467b891f8dec3ff3697e9ca169681b258b05044fab`.
Both app-only writes were hash-verified, preserving S3 model/NVS/OTA records.
The final 60-second production boot check reconnected the S3, acknowledged its
LED style and reported quiet activity, with zero crash markers. The development
key was absent and TCP 8765 was closed. Private production evidence:
`/tmp/waveform-p4-stability-final-runtime.log` and
`/tmp/waveform-stability-production-summary.log`.

Private evidence: `/tmp/waveform-stability-v3-s3-runtime.log`,
`/tmp/waveform-stability-v3-p4-runtime.log`,
`/tmp/waveform-stability-v3-events.jsonl`. The earlier runs remain in the
corresponding filenames with `v2` and without a version suffix.

## Remaining acceptance

The five-second requirement needs repeatable correct catalogue results, not
just a successful service response. Long-running real-room playback, low-volume
music, mixtures with voices, no-silence song transitions and disconnect stress
remain open. These short fixtures cannot establish broad classifier accuracy or
customer readiness. Physical LED appearance and phone/P4 rendering need user
confirmation in addition to protocol traces.
