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

Normal retries wait one second after completion during active playback. Service/capture failures back
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

The corrected firmware reduces the normal pause to one second and uses the
existing calibrated S3 diagnostics to clear silence independently of HTTP and
invalidate old results. Tests exercise fragmented/invalid diagnostics, quiet
debouncing, report expiry, disconnects and activity epochs alongside production
USB capture. The hardware log confirms actual playing/quiet transitions.
Title/artist/album are now vertically laid out with bounded label heights; the
technical footer is removed, and progress text uses plain language. The user
provided a photo proving the original fixed-position labels overlapped on a
long live-recording title. Visual confirmation of the revised layout is pending.

## Remaining hardware acceptance

1. Confirm the target is the Waveshare P4 and record silicon/PSRAM identity.
2. Observe an eight-second capture completing while panel refresh and style
   changes remain smooth. Record capture result and byte count without saving audio.
3. Play known music and verify actual microphone-to-screen title, artist, album
   and artwork. Measure capture, fingerprint, service and artwork durations.
4. Exercise silence, no match, track transitions and artwork expiry.
5. Disconnect Wi-Fi and S3 during capture/lookup; old results must not reappear.
   Restore connections and verify retries preserve their backoff.
6. Pair a phone during recognition and rotate its QR; neither should clear music.
7. Run prolonged playback, watch free internal/PSRAM memory, and reboot to verify
   saved networking and control recovery.

## Assumptions and residual risks

The earlier no-subscription recognition choice is retained. This is an unofficial
external service and needs a separate availability/licensing decision before
customer distribution. Device capture throughput, fingerprint latency and HTTPS lookup have been
physically verified in the initial smoke test. Known-song recognition, artwork
rendering and prolonged combined Wi-Fi/USB/display stability still need verification. The phone remote remains the existing control interface; this change
adds Now Playing metadata/artwork to the P4 touchscreen.
