# Native P4 recognition development — 10 October 2026

Branch: `feature/p4-music-recognition`, based on merged PR #5 at
`1278c3cd46993ed70512498561e16c450e0b7c56`.

## Implemented path

The P4 owns the existing USB connection and requests `WF1 <id> CAPTURE`.
The S3 firmware and its microphone/LED wiring are unchanged. A usable clip needs
the exact 16 kHz/128,000-sample header, all 1,000 ordered 256-byte PCM packets,
valid per-packet FNV-1a checksums and a matching END. Capture storage is allocated
in PSRAM. Capture fails after 15 seconds; failed clips are never fingerprinted.
Style requests continue through the same owner during capture.

The background P4 task normalizes quiet clips, generates the legacy Shazam
fingerprint locally, and sends only that fingerprint over certificate-verified
HTTPS. It displays title, artist and album; artwork is downloaded only from
HTTPS `*.mzstatic.com` addresses without redirects and decoded to RGB565.
Recognition responses are limited to 128 KiB, compressed artwork to 512 KiB,
and decoded artwork to 1024×1024. HTTP operations use five-second I/O timeouts
and check a 25-second deadline between operations. A blocking I/O can extend
that deadline by its own timeout.

Matched attempts wait 30 seconds; misses wait 15 seconds. Playback stop/start
removes the normal pause, but preserves failure cooldowns. Retry-After delta
seconds and UTC dates are honored, with overflow-safe scheduling. Service/capture failures back
off for 60, 120, 240 and then 300 seconds. Backoff survives reconnection.
Existing artwork survives temporary misses during active playback, expires after
90 seconds, and is cleared immediately when the Wi-Fi origin or S3 connection
changes. Existing calibrated S3 OPEN/SHUT diagnostics are consumed without
changing S3 firmware: three consecutive quiet reports clear the track and
invalidate any in-flight capture or lookup; reports expire after three seconds.
A fresh active report enables capture again. Service backoff survives activity
changes. The GUI's
one-second timer also enforces expiry during a slow recognition operation.
Rotating or using a phone pairing code does not invalidate recognition.
PCM stays in memory and is not saved to disk or included in logs.

## Source coverage and verified evidence

- Existing S3 capture/control implementation and Pi recognition adapter.
- P4 USB, network pairing lifecycle, LVGL UI and pinned Waveshare BSP.
- Pinned MIT-licensed ShazamIO algorithm/format/request definitions and cJSON.
- Host decoder and production USB-adapter tests with ASan/UBSan: complete clips,
  fragmentation, packet loss/duplication/corruption, S3 errors, concurrent
  requests, shared style controls, allocation/write failure, exact timeout,
  queue saturation, disconnect and old-session rejection.
- Native fingerprints match the independent upstream reference's 288 peaks;
  the separate binary-encoding fixture is byte-exact.
- Metadata tests cover missing matches, UTF-8, display controls, and artwork
  host/scheme restrictions. Policy tests cover network and USB invalidation,
  pairing rotation, persistent capped backoff and expiry.
- ESP-IDF 6.1 successfully built the native recognition firmware during
  development. Final build/test results are recorded in the development handoff.
- A Mac-hosted invocation of the same native fingerprint code identified the
  first eight seconds of upstream `examples/data/Gloria.ogg` as **I Will
  Survive — Gloria Gaynor**, with 15 matches and a 400×400 Apple artwork URL.
  The upstream `dora.ogg` clip returned no match; its native peaks matched the
  upstream Python result exactly for every band, pass, magnitude and frequency.
  These are example recordings, not the device microphone.

## Hardware commissioning — initial smoke test passed

The P4 was identified and flashed on 10 October 2026 using
`/dev/cu.usbmodem5CF71081441`. Silicon is v3.2, flash 32 MB, and PSRAM 32 MB
at 200 MHz; the PSRAM startup memory test passed. Secure boot and flash encryption
are disabled. A recovery copy of addresses `0x0` through `0x80ffff` was saved
privately before writing. Its SHA-256 is
`3850c9776e29e2385d39d70fa2c6c595202022026b330bb82ebee538b8678868`.
The saved partition table matched the build byte for byte. Only the application
at `0x10000` was written; esptool verified its hash. Application SHA-256:
`a3e964db35ffbb64d3e5d409581fd0c63830580b612cf6f391513e68e25dcfe8`.
An initial 921600-baud backup read failed with corrupt serial data; the complete
backup and application flash both succeeded at 460800 baud.

Device logs confirm display/touch initialization, USB S3 connection and Mirrored
style acknowledgement, and automatic reconnection using saved Wi-Fi credentials.
The first real microphone capture completed with result 0, 256,000 bytes, in
8,099 ms. Native fingerprint generation produced 256 peaks from 95,360 samples
in 1,568 ms. The TLS certificate was validated and the lookup returned no match
in 1,159 ms. This proves the device capture/fingerprint/HTTPS path; it does not
prove a known-song match or artwork rendering.

Recovery binary and raw logs remain under
`/tmp/waveform-p4-recognition-hardware-20261010/`; the recovery binary contains
saved device settings and must not be committed. No microphone PCM was saved.

## User-observed regression and correction

The original hardware run continued successfully after its first match:
all captures completed with 256,000 bytes, two lookups returned matches, and
both covers decoded successfully. The user confirmed the second was Led Zeppelin,
but only after more than 30 seconds. Intervening lookups returned no match;
there was no capture error or recognition-worker stall in the captured log.
The 30-second post-lookup pause caused approximately 40-second cycles. Keeping
artwork through misses for 90 seconds also retained the previous album after
playback stopped. These are application-policy defects, rather than evidence of
a stalled worker. A failed match on another song remains a service/fingerprint
coverage issue unless further evidence establishes a specific defect.

The corrected firmware uses the
existing calibrated S3 diagnostics to clear silence independently of HTTP and
invalidate old results. Tests exercise fragmented/invalid diagnostics, quiet
debouncing, report expiry, disconnects and activity epochs alongside production
USB capture. The hardware log confirms actual playing/quiet transitions and a subsequent
match with artwork. An intermediate one-second retry experiment produced three
successive matches about 11 seconds apart, followed by a verified HTTP 429.
That experiment was replaced by 30-second pauses after a match and 15 seconds
after a miss; stop/start removes normal waits, but never failure backoff. Safe
HTTP status/size diagnostics and Retry-After handling identify and respect the
service limit without logging response contents or audio.
Title/artist/album are now vertically laid out with bounded label heights; the
technical footer is removed, and progress text uses plain language. The user
provided a photo proving the original fixed-position labels overlapped on a
long live-recording title. Visual confirmation of the revised layout is pending.

## Final corrected hardware sequence

The final application was flashed at `0x10000` and verified by esptool.
Application SHA-256: `f0b709d877962100234667be4826a3c31025320747d2f7f2fd07315671fac530`.
The final runtime log records:

- Match and artwork ready at 23,295 ms after boot.
- Next capture completed without error; lookup returned no match at 63,667 ms.
- Calibrated quiet transition at 67,131 ms, clearing the track through the
  activity callback independently of the lookup schedule.
- Playback resumed at 71,731 ms. A fresh capture completed at 80,770 ms;
  lookup matched at 83,046 ms and artwork was ready at 83,508 ms: 11,777 ms
  from the resumed-activity report. The 15-second miss pause was removed by
  the playback transition.
- No HTTP 429 occurred in this sequence. This is a short smoke test, not proof
  of unrestricted service availability or long-running stability.

Final runtime evidence is in `rate-monitor.log` in the private recovery/log
folder. Host firmware tests, the P4 build and the full pre-commit checks passed.
The feature remains on `feature/p4-music-recognition` in its dedicated worktree;
main and origin/main remain at the PR #5 merge SHA. Visual confirmation of the
new label layout remains pending.

## User-confirmed stop/start acceptance

The user confirmed the corrected firmware identified the first song about six
seconds after music started, returned to the silent/listening screen about two
seconds after playback stopped, and identified the second song about six seconds
after it started. These are user-observed wall-clock timings, separate from the
serial measurements above and the fixed eight-second capture duration.

## PR review hardening

The recognition worker now waits at most 17 seconds (the controller's 15-second
capture limit plus a two-second margin), checking session changes every 100 ms.
A timeout publishes unavailable status and schedules failure backoff. A session
change abandons that wait immediately. A capture-ID mailbox replaces the
one-item completion queue: pending PCM has explicit ownership, stale/duplicate
callbacks are freed, and a late callback cannot complete a newer request.
The USB adapter returns the caller's capture ID on every completion path.

Regression tests execute the production wait/mailbox code for lost callbacks,
timeout recovery, session cancellation, pending and late completions, duplicate
callbacks, stale-slot cleanup, and concurrent callback/cancellation. The adapter
tests also verify capture IDs on success and failure. Retry-After tests now
reject impossible calendar dates, invalid clock fields and non-leap February
29 while accepting a valid leap day.

Firmware host tests with ASan/UBSan and the ESP-IDF 6.1 P4 build passed for these
review fixes. These fixes have not yet been flashed; the user-confirmed playback
timings above refer to the previously commissioned application.

## Remaining hardware acceptance

1. Confirm the revised layout is readable for long title, artist and album text.
2. Exercise prolonged playback, song changes without a silent gap and artwork
   expiry; watch free internal/PSRAM memory and display/control responsiveness.
3. Disconnect Wi-Fi and S3 during capture/lookup; old results must not reappear.
   Restore connections and verify retries preserve their backoff.
4. Pair a phone during recognition and rotate its QR; neither should clear music.
5. Reboot and verify saved networking and control recovery.

## Assumptions and residual risks

The earlier no-subscription recognition choice is retained. This is an unofficial
external service and needs a separate availability/licensing decision before
customer distribution. Device capture throughput, fingerprint latency and HTTPS lookup have been
physically verified. Known-song recognition, artwork and stop/start clearing
were verified on the device and confirmed by the user. Prolonged combined
Wi-Fi/USB/display stability and song changes without a silent gap remain to be
verified. The phone remote remains the existing control interface; this change
adds Now Playing metadata/artwork to the P4 touchscreen.
