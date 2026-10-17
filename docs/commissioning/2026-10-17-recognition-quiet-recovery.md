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
