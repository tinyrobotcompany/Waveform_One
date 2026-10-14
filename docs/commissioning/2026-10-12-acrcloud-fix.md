# ACRCloud recognition correction — 12 October 2026

Base: Claude's `96a0be8`, retained in an isolated `fix/acrcloud-recognition`
worktree. Main and origin/main remain `0a25902733f5cd35177a0d46342e2c8fac396ce8`.
No S3 firmware changes are required; continuous verified PCM remains the source.

## Evidence and corrections

1. Quiet votes survived a confidently resumed musical passage. A test with
   2.2 seconds of quiet, 0.6 seconds of resumed sound and a 0.4-second dip falsely
   emitted SilenceStarted. Resetting the vote history after sustained resumed
   sound fixes that exact regression. A single cough still preserves stop votes.
2. The recognition task owned both the detector and blocking HTTPS/catalogue/art
   work. Album following also held the display at an expected gap for up to ten
   seconds. A separate presence task now reads the same verified stream, clears
   the display on its stop decision and invalidates pending responses via the
   sound epoch. The slow worker cannot delay that decision. Stop/restart tests
   prove responses from the previous sound session remain invalid.
3. ACRCloud still computed/gated requests on a Shazam fingerprint, although the
   ACRCloud request contains WAV audio. That work and its heap are removed from
   the ACRCloud path. The Shazam fallback retains its own fingerprint path.
4. Album following displayed predicted titles without a lookup and deferred
   checks to expected track ends. Displayed identities now come from lookup
   results. ACRCloud uses quick 3/5/8-second startup attempts and periodic
   five-second checks/retries. Confirmations use the latest five seconds.
   Existing 12-per-rolling-minute budget and service failure/Retry-After handling
   remain in force. A miss retains artwork while further checks continue.
5. Candidate selection chose the longest matching fragment even when only
   milliseconds separated it from the primary match. In real public Beyoncé
   responses the original and sampled DJ mix both scored 100; 220 ms extra
   coverage made the old parser choose the remix. The parser now preserves the
   primary candidate for near ties (within 500 ms coverage and 3 score points),
   while substantially longer comparable-confidence matches can beat incidental
   fragments. Non-finite, negative and out-of-range ranking values are rejected.

## Verification

- Detector regression failed before its fix and passes after it.
- Presence/session tests cover an unfinished network request, stop/restart,
  absolute audio positions and stream interruption.
- Policy tests cover periodic checks despite a long album duration, request
  budget/cooldown preservation and PCM eligibility without a Shazam signature.
- Candidate regression failed before the parser correction and passes after it;
  existing incidental-short-match tests still pass.
- All firmware host tests pass with AddressSanitizer/UndefinedBehaviorSanitizer.
  Full Node, Python, Rust and firmware repository checks passed before the final
  candidate correction; firmware checks and the P4 build passed again afterward.
- ESP-IDF 6.1 P4 build passes. Credentials remain ignored local configuration;
  neither credentials nor generated credential headers are printed/committed.

## Independent service checks

No laptop playback or microphone recording was used. Verified existing public
fixtures were converted using the native downsampler/WAV construction and sent
in five paced ACRCloud requests. Four-second clips returned the expected song
identities for Wicked, jazz, classical and rock. Classical confidence was only
49, and release metadata varied; these results do not prove broad reliability.
The old ranking selected a Beyoncé remix. Four further public-fixture checks at
8/16 kHz, 4/8 seconds and a later offset showed Beyoncé as the first candidate;
several sampled mixes scored 100 too. Merely extending or resampling the clip
was not sufficient while the greedy longest-fragment parser remained in place.

Official protocol reference:
https://docs.acrcloud.com/reference/identification-api/identification-api

## Installation/acceptance

Prepared artifacts and logs:
`/Users/simonholmes/Projects/Applications/Waveform_One/build/acrcloud-recognition/`.
`firmware-sha256.json` identifies the prepared image. It has NOT yet been flashed.
A quiet, paused startup and controlled live trial are required. Verify correct
first-song identity, artwork retention during rechecks, clearing after stopping
while a request is outstanding, and a different song without a silent gap.
Do not claim five-second recognition or universal music/noise rejection before
those measurements. The detector remains acoustic activity detection, not a
validated semantic music/speech classifier. Artwork release selection also needs
separate validation: identical recordings can occur on multiple releases.

## Installed 13 October 2026

After the user confirmed installation could proceed, the P4 app-only flash at
0x10000 completed successfully with `Hash of data verified`. The installed
SHA-256 is `2e86b22ddb2e08377f27f2c56869d000eb36bd17769890fca874d865a5441e10`.
Startup logs confirm display/touch initialization, ACRCloud mode, valid PCM
lookup windows and successful HTTPS exchanges returning NoMatch. No observed
crash/assert markers. Monitor session remains active for the controlled trial.

Sound/quiet events still occur in ambient conditions with playback paused, and
resulting lookups return NoMatch. This is not evidence of semantic music/noise
rejection. Live song identity, transition timing, artwork retention and stop
latency remain pending user-controlled tests.
# Industry cross-check and faster scheduling — 2026-10-13

User confirmed playback had been stopped for some time and the ACRCloud project uses `recorded`. Microphone project mode is therefore not the identified fault. Faster cadence and level diagnostics passed the firmware host suite with ASan/UBSan and the ESP-IDF 6.1 P4 build. No silence threshold has been changed without the new runtime evidence.

Installed the faster-cadence diagnostic build on 2026-10-13, app-only at 0x10000; esptool verified the written hash. Prior runtime logs preserved as project-local `build/acrcloud-recognition/runtime-before-2p5s.log`. New paused-room diagnostics showed background estimates about -51 to -46 dB, mostly inactive presence, with brief starts followed by silence about two seconds later. This startup baseline does NOT reproduce or resolve the post-match retention failure. Controlled music/stop trial remains required; no semantic music classification claim is made.

Primary sources checked:

- [Apple continuous matching](https://developer.apple.com/documentation/shazamkit/shsession/matchstreamingbuffer(_:at:)): continuous buffers generate asynchronous searches. [Android stream contract](https://developer.apple.com/shazamkit/android/shazamkit/com.shazam.shazamkit/-streaming-session/match-stream.html) requires contiguous timestamped PCM, including 16 kHz mono.
- [ACRCloud official SDK](https://github.com/acrcloud/acrcloud_sdk_python): supports 8 kHz mono 16-bit WAV and separate audio/fingerprint paths; file recognition defaults to a 10-second clip. This is not a guarantee that microphone music always needs 10 seconds or that shorter clips are reliable.
- [ACRCloud scanning example](https://github.com/acrcloud/acrcloud_scan_files_python): scan interval and recognition clip length are independent settings. Its batch defaults are not a live-device latency target.
- [ACRCloud project configuration](https://docs.acrcloud.com/reference/console-api/base-projects): `recorded` is intended for microphone/noisy audio; `linein` is the default for clean source audio. Actual project mode and quota have NOT been verified. Recognition credentials alone do not provide console API access.
- [ACRCloud classification](https://docs.acrcloud.com/reference/console-api/file-scanning): music/speech detection is a distinct optional capability. File-scanning classification does not establish availability or latency for this device's identification account.
- [AudD stream monitoring](https://docs.audd.io/sdks/go/): continuous stream recognition and asynchronous match events are supported; extra catalogue metadata adds latency. This comparison does not propose replacing ACRCloud.

Derived design: continuous acquisition, independent local presence processing, overlapping rolling recognition windows, asynchronous match publication and later catalogue/artwork enrichment. No source establishes 2.5 seconds as a universal industry cadence.

Observed hardware trial: U2 “A Sort of Homecoming” around 7 seconds and “Wire” around 6 seconds according to the user. Request durations are only one component of screen latency. After playback stopped, S3 repeatedly reported quiet and ACRCloud returned NoMatch, but P4 presence stayed active and the album remained. The background estimate/level history at that time was not logged; the exact presence failure mechanism remains unverified.

Prepared change (not installed): 313-hop (2.504-second) ACR recheck target, at least 2.5 seconds between actual request starts, rolling 24/minute firmware cap, existing provider cooldowns unchanged. Existing five-second verification audio is retained; a shorter interval does not imply a shorter clip. Host tests cover the 24-request limit and its rolling expiry, and 313-hop recheck scheduling. Five-second level/background/history diagnostics were added to investigate silence without recording microphone PCM. Firmware cadence is a test setting, not an account quota assertion or a five-second recognition guarantee.
