# Product audio audit — 2026-10-13

Status: source and documentation audit with offline measurements. The device is not product-ready: a user-confirmed continuous song caused false silence and album dismissal. This report does not claim that the microphone hardware or recognition accuracy has passed acceptance.

Current checkpoint (14 October 2026): the sections below are a chronological
record; installation statements apply to the image named in each section.
The latest acquisition and recognised-track display retry corrections are built
and host-tested, but not flashed. Commits on `fix/acrcloud-recognition`:

- `b901f64`: verified album metadata cache helper and regression tests.
- `ba0e9fc`: native ACRCloud acquisition/presence integration, match evidence,
  bounded quiet-period attempts, service backoff, album cache use and clear retry.
- `6e44cae`: retry recognised-track updates when the GUI callback is busy.

The complete pre-commit checks passed for these commits with dependency fetching
disabled. Required test cleanup was explicitly approved for generated fixtures
under this worktree's `build/commit-checks/`; existing project files were retained.
Hardware recognition, acoustic false stops and real-stop timing remain open.

## Evidence and current path

Current firmware is native S3/P4. S3 reads 48 kHz stereo I²S, selects the first slot, filters/decimates to 16 kHz mono, and streams checksummed PCM to P4. P4 makes independent level decisions, keeps a rolling audio buffer, and sends filtered 8 kHz mono 16-bit WAV to ACRCloud. ACRCloud performs the recognition fingerprinting. Apple catalogue resolution/artwork follows a match; the first title is already published before that enrichment finishes.

The user confirmed ACRCloud project type `recorded`. Current rechecks target 2.504 seconds, with minimum 2.5 seconds between actual request starts and a rolling 24/minute firmware cap. These are our settings, not verified account entitlements. This cadence means up to 1,440 requests/hour while active. Account capacity and per-device cost need validation before choosing a shipping cadence.

User-observed initial recognition was around 7 seconds for U2 “A Sort of Homecoming” and 6 seconds for “Wire”. These are playback-to-display estimates; logged HTTP time alone is not end-to-end latency. During confirmed continuous “Drive My Car”, P4 emitted Silence at 06:56:12 UTC after a match at 06:56:11, then reidentified the song. User confirmed a later real stop cleared the album but did not time it. Raw level history overlaps between low music passages and paused-room activity; these measurements alone do not establish acoustic SNR or semantic music presence.

## Findings

| Area | Verified in source/measurement | Missing evidence / implication |
| --- | --- | --- |
| Microphone identity | README, firmware and commissioning name INMP441. A purchasing BOM instead recommends SPH0645. | Confirm actual fitted part and module revision before hardware sign-off; do not assume BOM equals assembled device. |
| I²S | Philips timing, 32-bit stereo slots, 64 clocks/frame; first slot consumed. | Matches INMP441 documentation if L/R selects left. Physical L/R, clocks, supply and port condition remain unmeasured. |
| Scaling | First resampler divides left-aligned word by 65536: consistent with 24-bit data in a 32-bit word converted to 16 bits. | Electrical timing/bit alignment, raw rail count, DC, source clipping and real SNR still require device measurements. Digital gain cannot improve input SNR. |
| Resampling | Actual production functions produce 16,000 then 8,000 samples from 48,000. Tested 100/1,000/3,000 Hz combined gains are -0.026/-0.007/+0.024 dB. | Synthetic steady tones at one level are not microphone validation or a complete filter specification. Transition band is broad: combined 4,100 Hz gain -8.543 dB, 4,500 Hz -28.456 dB. Quantized zero output does not mean infinite attenuation. No evidence yet connects this to recognition misses. |
| Presence | P4 uses raw mean-square level and an adaptive scalar background. S3 removes DC, estimates noise per spectral band and uses different gate rules. | Inconsistent decisions are expected from different observations. Raw RMS is not a music classifier. P4 low-level votes currently invalidate session and album immediately. |
| Fingerprinting | ACR path uploads audio; local Shazam analyzer is bypassed. | ACRCloud's official fingerprint SDK may reduce bandwidth, but compatible P4/RISC-V support and embedded resource costs are unverified. Shazam signatures are not interchangeable. |
| Match/album | ACRCloud provides recording metadata; Apple can replace album/artwork. | A recording appearing on several releases cannot uniquely identify which physical album the listener owns. Track accuracy and exact edition accuracy must be separate metrics. |
| Classification | Earlier local YAMNet evaluation reported 4.3875–8.775 s entry for tested rock/speech mixtures. | That policy failed the responsiveness goal. Reinstalling it without new evaluation would repeat a known failure. |

## Primary documentation and design implications

1. [INMP441 datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf): 24-bit two's-complement I²S, one-clock bit delay, 32 clocks per slot, L/R-selected output; electrical decoupling/data-line recommendations must be checked against the actual module.
2. [Espressif I²S guide](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32s3/api-reference/peripherals/i2s.html): distinguish data width, slot width and clock ratios. Source configuration is compatible in principle, but a scope/logic analyzer is needed to independently verify physical timing.
3. [TDK microphone mounting guide](https://invensense.tdk.com/wp-content/uploads/2015/02/Recommendations-for-Mounting-and-Connecting-InvenSense-MEMS-Microphones1.pdf): enclosure, acoustic opening, gasket and cavity affect response. Repeat measurements in the intended enclosure and across LED/power states; an exposed breakout is not production acoustic acceptance.
4. [ACRCloud identification API](https://docs.acrcloud.com/reference/identification-api/identification-api): accepts audio or its SDK fingerprints, recommends fingerprints for bandwidth/speed and short uploads. Our WAV path is supported; the recommendation is not proof of P4 SDK availability or a guaranteed latency improvement.
5. [ACRCloud source selection](https://docs.acrcloud.com/faq/service-usage): recorded mode is appropriate for microphones/noisy sources and is confirmed by the user here.
6. [Apple streaming contract](https://developer.apple.com/shazamkit/android/shazamkit/com.shazam.shazamkit/-streaming-session/match-stream.html): contiguous timestamped buffers matter to recognition. Preserve continuous acquisition irrespective of UI/filter state; explicitly detect discontinuities.
7. [Google YAMNet tutorial](https://www.tensorflow.org/tutorials/audio/transfer_learning_audio): a trained sound classifier operates on spectral features and overlapping windows (0.96 s / 0.48 s in this model). Its availability is not proof of acceptable recall, latency or compute on our hardware.

## Recommended implementation order

1. **Measure capture quality before altering gain/filter coefficients.** Add aggregate sample count, mean/DC, AC RMS, peak/rails, zero fraction and transport discontinuities; compare background spectrum with music spectrum. Keep measurements distinct from detector decisions. Calibrated dB SPL requires a reference instrument; logged dBFS is not dB SPL. Current added diagnostics report the last 200 ms block every five seconds, so they can reveal a problem but cannot exclude intermittent clipping between reports. A release measurement needs continuous aggregate/max counters.
2. **Separate observations from product state.** Continuous audio acquisition must survive uncertainty. Model Quiet, SoundUnknown, MusicConfirmed and ServiceUnavailable explicitly; NoMatch is not silence. Record reason and sample timestamp for every transition. Validate whether a fresh match supports the quiet portion of a clip rather than treating an HTTP response timestamp as proof music is still playing. Keep album selection/artwork independent of recognition timing. The exact silence/classifier/hangover policy must be selected using labeled data, not chosen by changing one dB constant.
3. **Evaluate music classification in shadow mode.** A classifier or spectral detector should initially log decisions without suppressing capture, LED or album updates. Compare against annotated music, speech, noise and mixtures, especially jazz/classical quiet passages. Do not deploy the previously failed model/policy merely because it is available. Measure P4 CPU, RAM, inference duration and scheduling interference.
4. **Benchmark recognition separately.** Compare 3/5/8-second windows and 2.5/5-second intervals using the same inputs and declared API-call budget. Keep transport, acoustic conditions and catalogue truth fixed. Report misses and incorrect matches alongside latency; do not count only successes. Compare official ACR fingerprints only after platform support is established.
5. **Validate complete state transitions and longevity.** Music start, low passages, speech over music, song changes without silence, actual stop, network failure, USB gaps, reboot and service quota exhaustion all need assertions. Only then tune candidate settings using a development set and evaluate an untouched holdout.

## Proposed acceptance sheet (targets, not results)

Define the supported speaker distance, playback level, room/reverberation, background-noise range, network latency and catalogue coverage before making a saleable performance claim. At minimum include jazz, classical, pop, rock, electronic and vocal/instrumental music; speech alone, TV, fans and handling noise; mixed speech/music; several volumes and microphone orientations.

- Preserve the user's <=5-second recognition objective. Measure from annotated audible onset to the displayed correct title, reporting p50/p95/p99, fraction within five seconds and failures. A percentile result is not a guarantee for every song.
- Proposed real-stop target: <=2 seconds at p95 under the declared background conditions, with tail latency reported. No current pass claim.
- Measure wrong-song and wrong-release rates independently. Proposed initial track correctness gate: >=99% in the declared test set, with confidence intervals and catalogue misses disclosed; this is an engineering proposal, not an agreed product promise.
- Count false landing-page returns per music hour and false music activation per non-music hour. Require zero observed false album dismissals in the release corpus, then a sustained hardware soak; zero events in a short test does not prove zero production failures.
- Use at least 100 independent tracks with repeated start/stop and gapless transition trials for initial development. Reserve different recordings/rooms for holdout; do not select thresholds against the same clips used for acceptance. Expand sample size according to the required error-rate confidence.
- Log audio sample positions, capture/window duration, queue age, request start/end, matched coverage/score when supplied, metadata/artwork duration, UI update and state-change reason. Synchronize playback markers for exact end-to-end timing.

## Artifacts and validation

`tools/music_benchmark/audio_path_audit.cpp` calls the production resamplers on synthetic in-memory tones. It performs no audio playback, microphone recording or service calls. Output: primary checkout `build/acrcloud-recognition/audio-path-response.csv`. Built and run with ASan/UBSan.

Additional DC/AC/rail/zero telemetry is prepared in the P4 source, with deterministic DC, rail and digital-zero tests. It does not change the detector thresholds or state decisions and is not yet flashed. Existing installed firmware remains the 2.5-second cadence plus level-history build. All generated data remains under the project `build` directory; no private microphone PCM was persisted.

Validation completed: full firmware host suite with ASan/UBSan, ESP-IDF 6.1 P4 build, and `git diff --check`. Logs are `build/acrcloud-recognition/audio-quality-host-tests.log` and `audio-quality-p4-build.log` in the primary checkout.

### Installed diagnostic observations

The diagnostic-only image was subsequently installed app-only on 2026-10-13; esptool verified the written hash (`audio-quality-flash.log`). Previous live logs are preserved in `runtime-before-quality.log`. No gain, filter coefficients or detection thresholds changed in this installation.

Paused-room snapshots around 08:39:41–08:40:06 UTC reported 0% sample rails, DC means from about -66 to +50 counts and AC levels about -52 to -47.7 dBFS. Raw-to-AC differences in those blocks were about 0–2.2 dB. These snapshots do not support a large persistent DC-offset or clipping diagnosis. They do not exclude intermittent defects or clipping during music. P4 was mostly quiet with transient sound events; S3 also switched repeatedly. Continuous-song comparison with these diagnostics is pending.

Reproduce the offline resampler measurement from the feature worktree (choose an existing project-local output directory):

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I firmware/esp32/visualizer/main -I firmware/esp32/p4_display/main \
  -I firmware/esp32/p4_display/third_party \
  tools/music_benchmark/audio_path_audit.cpp -o build/audio_path_audit
build/audio_path_audit > build/audio-path-response.csv
```

### Confirmed startup failure and album cache restoration

The user confirmed uninterrupted “Drive My Car” playback during the 08:42 UTC trial. Local SoundStarted/Silence cycles at 08:42:21/23, 23/25, 26/28 and 37/39 repeatedly reset the planner before its initial three-second request. The first match was at 08:42:47.632; that successful HTTP operation took 599 ms. This is not end-to-end recognition latency. A quantized level replay reproduces a start followed by a false stop; it is a derived level fixture, not a recording of the private microphone waveform. Continuous-quiet timing alone has not been shown to solve the full trace and was not deployed.

Album metadata caching is restored independently of the legacy predictive Follower. A separate lower-priority task prefetches the selected album's track list. Subsequent *verified* song matches reuse the cached album metadata and shared decoded artwork, including out-of-order tracks. The cache requires the same catalogue context, unexpired metadata, matching title/version and lead artist, and rejects duplicate/ambiguous titles. It does not publish a predicted next song or hold the album during silence. One completed cache and a latest pending request are retained; obsolete in-flight requests cannot overwrite a newer request's result. A cache miss follows ordinary catalogue resolution.

Validation: firmware host suite (ASan/UBSan), native P4 build and diff whitespace check passed. New tests cover same-album reuse, skipping backwards, wrong artist, live-version mismatch, expired context, different album and duplicate titles. Artifacts: `cache-host-tests.log`, `cache-p4-build.log` under the primary project's `build/acrcloud-recognition/`. Hardware installation/acceptance is recorded separately below; these checks do not claim the false-silence defect is fixed.

Installation: app-only flash completed with verified hash (`album-cache-flash.log`), and P4 booted without a reported crash. At 09:00:40 UTC it recognised “What Goes On”, selected Rubber Soul, then logged `Album cache ready collection=1441164359 tracks=14` at 09:00:41.991. This verifies live prefetch; reuse on a different verified track is still pending. The diagnostic/silence policy is unchanged. Archived prior trace: `runtime-before-album-cache.log`.

Live cache reuse confirmed: at 09:13:15.021 UTC the P4 recognised “Girl” (HTTP duration 595 ms) and logged the Rubber Soul cache hit with `artwork=1` in the same millisecond. The user reported the skip appeared instantaneous. This is a user-observed successful transition, not a measured zero-latency end-to-end result. An earlier “If I Needed Someone” match also hit the cache at 09:13:02.805. A Silence event followed at 09:13:21.146; whether playback continued at that exact event is unconfirmed, and the independent silence instability remains unresolved.

### Match-coverage instrumentation

The recognition response now retains validated sample begin/end offsets and confidence, and logs their absolute 16 kHz input sample positions before session rejection. Missing, non-finite, reversed, out-of-clip and beyond-supported-window coordinates cannot become valid evidence. This is instrumentation only: it does not yet change quiet decisions or promise stop latency. ASan/UBSan firmware tests and native P4 build passed (`evidence-host-tests.log`, `evidence-p4-build.log`).

ACRCloud's identification API shows these offsets in its sample response. Its metadata documentation defines them as positions within the submitted sample, and notes that detailed offsets may require enablement for recognition accounts. Therefore field availability must be checked on this actual account before using them in display state policy:
- https://docs.acrcloud.com/reference/identification-api/identification-api
- https://docs.acrcloud.com/reference/console-api/bm-projects/metadata/custom-files

Neither an HTTP response arrival timestamp nor an overlapping clip containing an old song proves that the song is still playing. The follow-up measurement must compare the recognised sample interval with the quiet interval, rather than merely adding an album hold timer.

Match-evidence diagnostic firmware was flashed app-only with verified hash (`match-evidence-flash.log`), and rebooted successfully. At 09:27:06.928 UTC, “Run For Your Life (Remastered 2009)” returned a valid matched interval 0–6680 ms within an approximately 6944 ms submitted clip (absolute samples 28800–135680; clip end 139904). Thus this account does supply the detailed offsets. Its reported score was 22; no new confidence cutoff was introduced. Rubber Soul's 14-track cache also loaded successfully after reboot. A controlled actual-stop comparison is the next required measurement; the detector and session cancellation behavior are still unchanged.

### Capture continuity and display-clear correction

The user confirmed the album was still visible when they pressed pause in the 09:28 UTC trial. The earlier silence log therefore cannot be used as a measured screen-clear time. Source inspection found that `update_recognition` silently returned if its one-second GUI lock acquisition failed; remote state was already updated, allowing the phone and P4 to disagree. No historical log proves that this timeout caused this specific observation.

Implemented two bounded corrections, without changing acoustic thresholds or stop windows:

- ACR acquisition preserves its rolling planner across brief level-based quiet/resume decisions. It expires the acquisition after a full eight-second retained-window interval of quiet. Display stop handling remains independent: this is not an eight-second album hold. Physical network/controller changes and PCM discontinuities still reset the planner. A worker ring overrun also re-arms acquisition rather than leaving it inactive.
- An interrupted lookup can survive level chatter only when valid service coverage reaches into the resumed sound in the same physical stream and connection. Matches confined to the old sound, missing coverage on a reused prefix, genuine stops and disconnects remain rejected. Minimum request spacing, rolling request cap and service backoff remain in force.
- The UI callback acknowledges whether it applied the update. Empty/stop updates are retained and retried by the independent audio task if the GUI is busy. Newer epochs or tracks supersede pending old clears. A 10 ms lock attempt replaces the prior one-second blocking wait. The log distinguishes an applied empty update from a detector decision. Applied LVGL state is still not an optical measurement of display scanout.

Full firmware host suite with ASan/UBSan and P4 build passed (`continuity-host-tests.log`, `continuity-p4-build.log`). New tests cover repeated short interruptions retaining the 3/5/8-second acquisition schedule, long-quiet reset, physical epoch separation, late-result guards, GUI retry and supersession. Acoustic classification itself is not solved by this patch: false level decisions may still dismiss a displayed album. Live acceptance of the corrections is pending; do not call the recognition product-ready.

Installation: the continuity/display-delivery image was flashed app-only with a verified hash (`continuity-flash.log`) and booted at approximately 10:15:43 UTC. Runtime confirms successful empty-state application acknowledgements. This verifies boot and callback delivery, not recognition accuracy or stable playback. The pre-fix trace is archived as `runtime-before-continuity-fix.log`; live controlled acceptance remains pending. Work remains on `fix/acrcloud-recognition`; no commit, push or merge was performed in this correction step.

### Autonomous checkpoint (no downloads or deletions)

The user authorised quiet autonomous playback, then explicitly prohibited downloading anything and deleting any files. No downloads, file deletions, cleanup scripts, installs, commits, pushes or merges were performed during this autonomous phase. Existing local music fixtures were inspected. Computer Use reported missing permissions, preventing Music-app selection of Drive My Car. The local afplay attempt at gain 0.15 failed with `AudioQueueStart failed (-66680)`; its elevated retry was interrupted, so it is not counted as a completed trial. No controlled playback/stop latency or recognition pass is claimed.

The uncontrolled log between approximately 10:15:43 and 10:25:42 UTC contains 113 lookup completions, zero matches, 171 GUI-busy warnings and 411 applied empty-update acknowledgements. These are runtime counts, not an accuracy benchmark: no controlled test audio was established. A concrete GUI retry at MCU time 583056 ms was applied at 583080 ms, verifying delivery after a failed lock attempt. Recognition-through-chatter acceptance is still pending.

Artifacts in the primary project's `build/acrcloud-recognition/`: `autonomous-trials-20261013.jsonl` (append-only command markers) and `autonomous-checkpoint-20261013.json` (summary). The serial monitor was stopped through its existing exec session with Ctrl-C, which returned exit code 130. No assistant-started test audio remains running. The installed firmware remains the continuity/display-delivery candidate with the verified album cache. This is the natural stopping point until controlled playback can be completed.

### 2026-10-14 — bounded acquisition through uncertain level decisions

Verified source defect: preserving Planner state alone did not finish an acquisition while the level detector was quiet. The worker still required `Session.ready`, which includes `sound`, before issuing any request. A second defect discarded failures on an intervening session change before recording failure backoff.

The candidate exposes transport availability separately and permits an already-started, unmatched ACRCloud acquisition to finish its bounded three attempts during a quiet vote. Existing 2.5-second request spacing, rolling budget, Retry-After, physical discontinuity invalidation and eight-second quiet expiry remain enforced. A quiet room does not start an acquisition, and completing the three attempts does not permit further quiet-room requests. Successful matching does not start a new quiet acquisition. Provider outcomes update retry state before display/session rejection.

A provider response while the current session is quiet/unavailable is logged but not displayed, including when the request began after the quiet transition. Request logs include absolute input sample bounds and `level_sound`; the response logs already include matched sample coverage. This makes the disagreement observable without saving microphone PCM or allowing old music to revive the screen after a real stop.

Evidence quality:

- Verified: a regression using the old admission rule fails when an acquisition's level state becomes quiet; the corrected rule passes.
- Derived: replaying the logged event times at 08:42:21.737/23.166/23.978/25.809/26.421/28.261 on 2026-10-13 admits two requests under the old gate and three under the candidate by 8.008 seconds after the first logged start. These are host-arrival timestamps rounded to 8 ms hops, not raw PCM, exact microphone onset, or new service accuracy measurements.
- Verified: six ASan/UBSan host suites passed (policy, presence, listening, ACRCloud, HTTP, USB stream transport). Native ESP-IDF 6.1 P4 compilation/link/image-size checks and `git diff --check` passed. Build artifacts are in this worktree's `build/audio-investigation-20261014/`, with the image in the existing firmware build directory.
- Not verified: hardware recognition latency, service match rate in air-conditioning noise, and false album dismissal. This candidate has not been flashed. The S3 remains unchanged.

Source coverage: current worker/session/planner/level-detector paths, recorded continuous-play events, provider request/response parsing and transport tests; ACRCloud's [recorded-audio guidance](https://docs.acrcloud.com/faq/service-usage), [official progressive-capture SDK](https://github.com/acrcloud/acrcloud_sdk_unity3d), and [identification API](https://docs.acrcloud.com/reference/identification-api/identification-api) reviewed in the preceding investigation. No new external libraries or models were installed. Private credential files and unrelated PCB work were excluded.

Residual limitations: the first acquisition still needs a sound-start event; loudness-only display dismissal is unchanged. Finishing an acquisition after a real stop can consume up to the remaining bounded attempts, but cannot display those quiet-session responses. This is an acquisition correction and comparison candidate, not a completed acoustic classifier or product acceptance.

Next controlled trial: install the candidate with music paused, announce the test, then collect a known continuous song followed by an explicitly marked pause. Compare `level_sound`, absolute clip/match coverage, provider outcome, rejected presentation, and applied display updates. A provider match withheld during confirmed continuous playback directly identifies a local-state false negative; a NoMatch does not establish whether capture or provider matching is responsible. Do not choose new level/spectral thresholds from this scheduling replay.

### 2026-10-14 — retry recognised-track display updates

Verified source defect: `publish()` discarded the GUI callback's failure for updates containing a track. Only empty updates were retried. A busy GUI could therefore lose the recognised title/artwork until another lookup or enrichment happened to republish it.

The worker now retains pending update status/epoch and retries with its own still-owned track and image. Clearing/replacing that payload cancels pending work; a newer status replaces it, and stale epochs are discarded. No borrowed pixel pointer is stored in the retry object. The visible-track flag is set after the callback successfully applies the update. Empty updates retain their existing independent retry path.

Validation: ASan/UBSan policy tests cover busy callback, newer-status replacement, stale epoch and cancellation. The album-cache test also passed. Native P4 build and image-size checks passed (`build/audio-investigation-20261014/display-retry-build.log`). No hardware was connected or flashed for this change; optical update timing remains unmeasured. This corrects update delivery, not the acoustic stop classifier.
