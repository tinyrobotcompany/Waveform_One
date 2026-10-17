# Recognition quiet-audio cooldown recovery — 2026-10-17

`calibrated-comparison.log` records ACRCloud code 2004 at 11:02:12,
followed by `retry_ms=59999`. Music is clearly present by 11:02:46,
but the next request starts only at 11:03:12. It matches Michelle in
1123 ms; album selection takes 996 ms and artwork finishes at 11:03:15.
Album cache loads 14 Rubber Soul tracks at 11:03:16. These timings are
not an exact user-play-to-display measurement; playback start was not timestamped.

ACRCloud documents 2004 as unable to generate fingerprint:
https://docs.acrcloud.com/sdk-reference/error-codes
The parser incorrectly classified this sample outcome as a service failure.
The recognition-only correction maps 2004 to NoMatch, retaining existing
request cadence/budget, display-evidence handling, and backoff for real
service errors and quotas. Regression tests cover 2004, track clearing,
and unchanged service-error/rate-limit classifications. Full host suite and
P4 build pass. The recognition-only app was subsequently flashed and its flash hash verified. Quiet-room hardware logs (`recognition-2004-serial.log`) show consecutive no_match responses at 11:09:32 and 11:09:34, with the next lookup proceeding instead of a 60-second cooldown. This verifies recovery for the observed quiet-audio case; stopping and broader reliability remain unverified. LED code and its NVS calibration were preserved. Firmware SHA-256: 23ce6d5f4e3988f51770a40b23969a12c0251b5dfedccade3ac98b019ec80554.


## Playback and cached track-change validation

Simon counted approximately four seconds to the first visible recognition of
What Goes On (The Beatles, Rubber Soul), then approximately three seconds
when skipping to Girl. These are user observations, not precise instrumented
end-to-end measurements.

Serial evidence in recognition-2004-serial.log:
- 11:10:25.223: What Goes On matched, request duration 921 ms.
- 11:10:26.657: Rubber Soul selected, catalogue duration 1410 ms.
- 11:10:27.198: artwork decoded; 11:10:28.018: cache ready, 14 tracks.
- 11:11:43.667: Girl matched, request duration 636 ms.
- 11:11:43.687: explicit album cache hit, collection 1441164359, artwork=1.

The cache avoids catalogue/artwork retrieval after a confirmed track change;
it does not guess a track purely from album order. Some requests still return
no_match during playback; later requests confirm the same song. No 60-second
quiet-fingerprint cooldown occurred in these trials. Long-running stability,
other genres/volumes, stop-to-landing timing, and reconnects remain to test.

LED behaviour was explicitly accepted and must remain unchanged. Future
recognition optimizations should be measured separately from this checkpoint.


## Subsequent failure: Joshua Redman, Mack the Knife (Trios Live)

The successful Beatles trials did not establish general reliability. User
reported over a minute without recognition, then identified the playing track
and confirmed volume was increased only afterwards. Serial requests continued
about every 2.5 seconds with five-second PCM clips. Lookup 109 last matched
Girl at 11:14:01; lookups 110–198 returned no_match. Lookup 199 finally matched
Mack the Knife at 11:17:47 (1064 ms request), selected Trios Live at 11:17:48,
and cached seven tracks at 11:17:50. Subsequent requests again returned
no_match. The exact playback-start time is unknown; this is not a measured
226-second recognition latency. No firmware change occurred during recovery.

The live match shows the service can identify this recording but does not
establish adequate matching reliability. Existing no_match logging conflates
provider codes 1001 and 2004, so their contribution cannot be recovered from
this capture. A recognition-only follow-up now logs the exact code and expands
five-second verification windows to the retained eight seconds after two
misses, returning to five seconds on a match. It preserves cadence, display
policy, LED processing, and calibration. Host regressions and native build
pass; this follow-up is uncommitted and not flashed. Its effectiveness on
this recording is unverified.

## Same-sample root-cause diagnostics

The recovery-window experiment was removed from working production policy;
its patch is retained in build/p4-led-startup/unflashed-recovery-window.patch.
The current bench-only diagnostic flag compares the first >=5-second no-match
with original 16 kHz PCM and a normalized 16 kHz copy, at most two additional
requests per boot. Diagnostic matches never update the screen. Normal LED and
recognition policy files are unchanged from the pushed checkpoint.

Evidence in primary build/p4-led-test-2026-10-17/:
- Failed run ids 110–198: 89 no_match responses; median response 779 ms,
  minimum 501 ms, maximum 2321 ms.
- pcm-evidence-serial.log: service codes 1001 and 2004 both observed.
  A 7.184-second 8 kHz sample (code 1001) exported with verified SHA1
  85863ad5787c76fdd1b13e574170b8fdbcfe1375. RMS -65.30 dBFS,
  peak 159 counts, RMS 17.80 counts. Original 16 kHz export lost serial
  data and cannot be treated as a complete verified recording.
- Mac replay of that exact 8 kHz WAV, amplitude-normalized copy, and original
  again: all code 1001. No recognition improvement from amplification.
- A second serial export also lost data; abandoned export-based comparison.
- pcm-comparison-serial.log: on-device comparison completed 11:46:53.
  Same 99,840-sample / 6.24-second input: submitted 8 kHz code 1001,
  original 16 kHz code 1001, normalized 16 kHz code 1001.
  Original peak 263 counts, RMS 71.124 counts. Conversion bypass and
  amplitude normalization did not recover this sample's recognition.
- Subsequent ordinary requests identified Mata Zyklek by Angine de Poitrine.
  Current playback identity requires user confirmation; do not attribute this
  second comparison to the earlier Joshua Redman track without confirmation.

These results do not establish a final root cause. They refute conversion
bypass or amplitude normalization as sufficient fixes for the compared samples.
A clean reference of the same musical passage is still needed to distinguish
acoustic capture quality from service coverage/matching limitations. Diagnostic
firmware is currently flashed; the flag defaults off and is locally enabled.
This is not a production acceptance checkpoint or a validated recognition fix.

## Handoff at Simon's request — investigation stopped

Simon confirmed Mata Zyklek was playing. He subsequently reported same-album
track identification taking up to 15 seconds and considers both recognition
and the cache experience unacceptable. No final root cause or reliable fix
has been established. Earlier four-/three-second trials must not be presented
as overall acceptance. Stop-to-landing behaviour is also unverified.

Branch: fix/p4-led-dma-startup
Worktree: /Users/simonholmes/Projects/Applications/Waveform_One/.worktrees/p4-led-dma-startup

Hardware: P4 onboard ES7210 microphone and directly wired HUB75, port
/dev/cu.usbmodem101. The accepted LED implementation and its NVS room reference
must remain unchanged. Main contains PR #9's album colours; this branch also
contains the earlier P4 microphone, album-cache recovery, and HUB75 commits.

Current flashed firmware: the direct on-device PCM comparison build, NOT a
verified recognition fix. Local sdkconfig enables
CONFIG_WAVEFORM_RECOGNITION_DIAGNOSTIC_PCM=y. This defaults off in source and
should be disabled for ordinary performance testing: it adds up to two
synchronous service requests and waits once per boot after a qualifying miss.
No diagnostic match is published. The previously proposed eight-second
recovery change is NOT included in the current source or flash.

Local build command: sh build/p4-led-startup/build.sh (installed ESP-IDF 6.1;
network proxies intentionally disabled). Build output:
firmware/esp32/p4_display/build/waveform_p4_display.bin. Flash application only
at 0x10000; retain NVS/calibration and storage. Original pre-test application
backup: primary checkout build/p4-led-test-2026-10-17/previous-app.bin.
Validated LED/cooldown checkpoint binary in this worktree:
build/p4-led-startup/recognition-2004.bin.

Evidence directory (local, not committed):
/Users/simonholmes/Projects/Applications/Waveform_One/build/p4-led-test-2026-10-17/
It includes serial logs, flash logs, verified 8 kHz WAV, normalized WAV,
replay JSON results, and diagnostic capture/decode/replay scripts. Raw 16 kHz
serial captures are incomplete and must not be represented as verified files.
Credentials remain in the existing untracked acrcloud.local symlink;
managed_components is a local symlink to installed dependencies. Neither is
part of the commit. Do not overwrite local sdkconfig or credentials blindly.

Validation: firmware host tests passed with ASan/UBSan; the native diagnostic
P4 build passed; firmware flash hash verified. The direct comparison confirms
that conversion bypass and digital amplification did not rescue that sample,
not that microphone quality or service matching is adequate. A separate issue
also appears in logs: Soul Dance was resolved to Wish during the live-album
session; investigate album-version selection independently of recognition.

Do not resume tests, flash, or play music without Simon's instruction. He
requested stopping and handing the task to Claude.
