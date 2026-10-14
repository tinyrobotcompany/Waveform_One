# Audio handoff — 12 October 2026

The user stopped Codex audio work and intends to use Claude for audio detection.
Codex will handle artwork colour matching in a later task. Audio is **not accepted**:
recognition timing, stop detection, request pacing and album accuracy remain open.

## Branch and installed code

- Branch: `feature/music-recognition-benchmark`.
- Worktree: `/Users/simonholmes/Projects/Applications/Waveform_One/.worktrees/music-recognition-benchmark`.
- Branch includes the colour work from `29f8bcd`; it is stacked above that feature,
  not just main. Do not merge automatically.
- Installed P4 binary SHA-256: `7eb8d46b07b68bb91636c4efc724941102a267f569ef7081934e45b2a4065aa5`.
- Installed S3 binary SHA-256: `bc0a43a6a65a4cec50b7dc0a167d90bd5a8107695038e0e09ce20e843cc9fbec`.
- Four-second capture is installed on both boards. S3 retains legacy eight-second
  CAPTURE support; P4 explicitly requests CAPTURE 4 and validates all 500 packets.
- Music-only classifier remains disabled. Baseline sound activity gate remains
  the clean-spectrum calibrated S3 visualizer gate.
- Five-second normal pauses after matches and misses are installed. **This naive
  higher service cadence caused HTTP 429 and is not an accepted solution.**
  Failure backoff and Retry-After handling remain intact.
- Exact gain normalisation optimisation preserves fingerprint outputs, but has
  not delivered a material overall hardware processing improvement.

## Observed failures and costs

- User confirmed Girls on Film / Duran Duran and Inception / McCoy Tyner song and
  artist. Inception's album was wrong; returned and expected album names were
  not supplied. Never equate song identity with correct album/release identity.
- User reported album remaining on the P4 over ten seconds after music stopped.
  Logs still showed Playing, ambient captures and later Quiet/Playing toggles.
  No stop-detection fix was implemented after that report.
- `recognition_policy::expired` extends match life using sound-activity heartbeats;
  false Playing telemetry can therefore keep a match alive indefinitely.
  This is a diagnosed risk, not a verified complete root cause or finished fix.
- P4 `audio_activity::Decoder` receives existing S3 OPEN/SHUT diagnostics, waits
  for three SHUT reports and treats missing telemetry as Unknown. Its raw source
  diagnostics include RMS, OPEN, CLOSE, CLEAN, GATE_RMS and band counts, but those
  values are not currently forwarded into the P4 serial logs. Inspect the true
  stop path before changing thresholds or TTLs.
- Logged active attempts: capture ~4.1 s, fingerprint ~1.0–1.2 s, HTTPS lookup
  ~0.55–1.32 s. Artwork follows metadata and is not the initial match bottleneck.
- Profiling shows FFT ~0.73–0.82 s, spreading ~0.12–0.14 s, peak extraction
  ~0.08–0.10 s, setup ~0.05 s and scheduler yields ~0.06 s.
- HTTP 429 occurred at 15:22:23 UTC and 15:26:16 UTC. Each imposed ~60 seconds
  of failure cooldown. Frequent local sampling must be separated from external
  lookups; ambient sound must not consume the service budget before music starts.

## Artifacts and hardware access

All artifacts live under the primary project folder:
`/Users/simonholmes/Projects/Applications/Waveform_One/build/music-recognition-benchmark/recognition/`.
Key logs: `optimised-runtime.log`, `pre-optimisation-diagnostic-runtime.log`,
`p4-optimised-flash.log`, `optimised-host-tests.log`, `optimisation-parity.json`.
See the accompanying four-second commissioning record for the trial details.

P4 UART: `/dev/cu.usbmodem5CF71081441`. Phone IP observed: `192.168.1.26`.
S3 connects to P4 USB host and has no separate Mac serial port. Existing native
P4 maintenance firmware and the desktop flash transport are archived locally in
this artifact directory. Python is an esptool/diagnostic transport, not the device
application. Both board applications are C++. Preserve the independent hosted
Wi-Fi RX ownership fix and pinned dependency when building.

Codex stopped its serial monitor at handoff. Check port ownership before opening
it; opening UART can reset P4. The desktop monitor is bounded to eight hours and
32 MiB, redacts pairing tokens, and never records PCM/DPCM. Announce hardware
trials before starting them. Do not play audio from the laptop. **When asking the
user a question, stop all work and wait for their answer.**

## Existing validation

Firmware host ASan/UBSan suites pass; exact gain tests cover signed PCM boundaries
and reuse. Forty-five public-fixture signatures are byte-identical before/after
gain optimisation. Both ESP-IDF 6.1 target builds passed and installed image hashes
were verified. These tests do not establish real-room music-only detection,
correct album release selection, reliable five-second recognition or fast clearing
on music stop. Repeat hardware acceptance after implementing an evidenced fix.

Colour feedback: palettes overly revolve around blue and brown/green. User wants
closer artwork-to-screen matching. No colour implementation changes were made in
this audio phase.
