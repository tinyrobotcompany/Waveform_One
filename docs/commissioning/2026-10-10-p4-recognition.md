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

Normal retries wait 30 seconds after completion. Service/capture failures back
off for 60, 120, 240 and then 300 seconds. Backoff survives reconnection.
Existing artwork survives temporary misses, expires after 90 seconds, and is
cleared immediately when the Wi-Fi origin or S3 connection changes. The GUI's
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

## Hardware acceptance — pending

No P4 was flashed as part of the evidence above. The connected serial adapter
alone does not identify its attached MCU. Confirm the P4 port and the P4-to-S3
USB connection before installation. Preserve a recovery copy of the current P4
application and flash only the P4 application, keeping its existing partition
table and saved Wi-Fi credentials.

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
customer distribution. Device capture throughput, fingerprint latency, artwork
rendering and combined Wi-Fi/USB/display stability are not yet physically
verified. The phone remote remains the existing control interface; this change
adds Now Playing metadata/artwork to the P4 touchscreen.
