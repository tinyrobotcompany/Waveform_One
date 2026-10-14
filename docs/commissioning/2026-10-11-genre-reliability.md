# Genre recognition and runtime profiling — 11 October 2026

> Historical experiment: the active firmware is now restored to the pre-filter baseline. See [complete rollback](2026-10-11-complete-filter-rollback.md); retained changes described here are no longer deployed.

Work remains on `feature/p4-recognition-stability` in
`.worktrees/p4-recognition-stability`. No merge or new PR was made.

## Evidence and fixes

User trials reported approximately four-second LED entry, missed jazz/vocal
recognition and five-to-seven-second exits. The classifier, capture, fingerprint,
HTTPS and metadata paths were measured separately on the connected S3/P4.

The S3 initially spent about 145 ms in its frontend and 1,050 ms in inference.
The chip is the ESP32-S3 with embedded 8 MB AP_3v3 octal PSRAM. The supported
80 MHz RAM setting reduced initial inference measurements to approximately
820–840 ms, with longer-run measurements around 910–920 ms. Frontend time stayed
around 145–165 ms. No RAM integrity failure, crash or I2S loss was observed in
these short tests; this is not a long-duration thermal/memory qualification.

Fingerprint generation previously used three bands and stopped when 255 peaks
had accumulated after 3.1 seconds. The complete mode now retains all four bands
through 5.5 kHz and processes the entire four/eight-second clip. Peak spreading,
frequency rounding and log-magnitude clamping follow the complete reference.
The legacy fixture remains explicitly tested. Independent shazamio_core 1.2.0
macOS-arm64 reference counts for the deterministic four-second LCG fixture are
39/115/240/255, with first-peak values checked within one float-rounding unit.
Sparse one-LSB noise previously fabricated 106 low-magnitude peaks; the correct
log floor produces none, covered by a failing-then-passing regression.

The peak rings retain only the frequency neighborhood that can affect accepted
peaks. Workspace fell from 2,140,160 to 1,505,280 bytes; modern/legacy golden
fixtures remain unchanged. Yields remain bounded, now every 32 FFT frames.
Both targets use 1 ms scheduler ticks, so reducing yields saves only tens of
milliseconds. Four-second hardware fingerprints fell from about 1.25–1.30 s to
1.11–1.13 s in the next pass. A subsequent single-precision PCM gain change
removes software double conversion/multiplication for each sample on the P4;
host golden tests pass. A four-second fingerprint in the final production boot
check took 1,025 ms; that uncontrolled song does not provide an equivalent
fixture comparison or an onset-to-identification latency.

The classical fallback reproduced an actual device failure:
`esp-aes: Failed to allocate memory`, followed by TLS write error `-0x0084`.
The worker held peak vectors, a URI string, JSON's URI copy and a printed body
while HTTPS needed internal buffers. Request serialization now puts the body
in PSRAM and releases the other allocations before HTTPS. Host tests exercise
production serialization, correct sample duration/URI/timestamp, release of all
JSON allocations and vector capacity before TLS, explicit external-RAM
allocation and allocation failure cleanup. Repeated lookups logged approximately
93–109 KB free internal RAM and a 30–56 KB largest internal block; the observed
AES allocation failure did not recur. These numbers are internal heap figures,
not a separately measured DMA reserve.

## Fixture results and limits

Public Apple previews were decoded to mono 16 kHz PCM in `/tmp`. They are not
committed, and no private room PCM/DPCM or maintenance key was retained.
The desktop helper only flashes, plays public fixtures and observes diagnostics;
recognition and classification stay native C++ on the devices.

Native frontend/TFLite Micro evaluation covered 770 windows: speech, silence,
white noise, five genres, 1% volume versions, and speech mixed at half/equal/twice
music RMS. Pure music had strong scores in 149/150 windows; quiet versions also
had 149/150. Speech/silence/noise had no strong music windows in 20/20 windows.
Equal or louder speech reduced music classification substantially; these short
fixtures do not establish broad voice/noise discrimination. Reference-kernel
outputs differed from desktop LiteRT by up to 22 quantization steps (0.085938);
target ESP-NN validation is separate.

After the memory fix, the 400-second hardware pass produced:

| Public preview | Metadata result | Lookup result after playback process start |
| --- | --- | --- |
| Beethoven Fifth, Berlin Philharmonic / Karajan | Correct title and performers | 7.84 s |
| Take Five, Dave Brubeck Quartet | Correct song, artist returned as Dave Brubeck | 6.63 s |
| Crazy in Love, Beyoncé | Correct title and artist | 5.40 s |
| Popular, Ariana Grande / Wicked | Correct title and artist | 10.93 s |
| Whole Lotta Love, Led Zeppelin | Returned You Make My Dreams, Hall & Oates | Failure; not a successful latency result |

The same logs show recognition of other tracks between scheduled clips,
including Koi and You Make My Dreams. The user subsequently confirmed that
nothing else was playing. Treat these as incorrect recognition results and
false music activation during the scheduled gaps, rather than attributing them
to overlapping playback. The precise failing stage remains under investigation.
Do not claim five-of-five accuracy or use pre-existing music activity as a
successful fixture-entry timing. The next hardware run needs isolated audio
with capture correlation and service-match evidence. A direct lookup of the
first four seconds of the same rock fixture, using the current native code and
fresh request IDs, returned HTTP 200, Whole Lotta Love / Led Zeppelin, and five
match entries. The fixture and clean native lookup therefore pass this check;
the live microphone path remains unresolved.

After that clarification, the production P4 was rebuilt with capture hashes,
DC/AC energy and service match-count/offset diagnostics. These retain no
microphone PCM and change no detection thresholds or recognition outcomes.
Firmware host tests (ASan/UBSan) and the P4 build passed; the app-only flash
completed with a verified hash. Current diagnostic production P4 SHA-256:
`bbae2bc66ebe0df3e0c025dcc7da54b3d6092cf4f47d4ccbff19ab31d3e35b7b`.
The development bridge remains OFF and the S3 policy remains unchanged from
the deployed version (the source two-of-three change is still not flashed).

A subsequent 150-second observation scheduled no test playback until 60.025 s,
then an 18-second rock preview ending at 78.230 s. No activity or lookup was
reported during the initial quiet interval. Playing was reported at 66.004 s;
the complete four-second capture had hash `78c8424e`, peak 1892, DC -0.000850
and AC RMS 0.015659. The native fingerprint took 1020 ms and the service returned
five match entries at 68.092 s, with first ID 479771505, offset 28.420 s and
timeskew -0.002120. The first match ID agrees with the clean Whole Lotta Love
lookup. Production builds omit title/artist logging, so the actual displayed
metadata was subsequently confirmed by the user along with audible playback
of the intended Led Zeppelin clip.
Quiet returned at 85.971 s and no further lookup was reported through 150 s.
No fatal runtime markers occurred. This is approximately 8.1 s to lookup and
7.7 s from player exit to quiet, so neither timing is accepted. The wrong
matches did not reproduce in this run; the intermittent failure remains open.
Playback timestamps remain process timestamps; the user confirmed audible
playback and the displayed Whole Lotta Love metadata for this specific run.

The clean eight-second classical excerpt also returned HTTP 200 with the correct
title and Karajan/Berlin performers using the updated native fingerprint. The
earlier To the Youth preview returned no match even with an independent modern
fingerprint, correct duration and fresh request IDs. This does not establish the
catalogue coverage or correctness of every fingerprint. A comparison run hit
HTTP 429 and stopped immediately; later fixture starts were paced 78 seconds
apart. Existing failure and service cooldowns remain intact.

Known music remained protected through short low scores. Normal room exits
still took roughly 7–10 seconds in these tests; the two-second exit requirement
is unresolved. Speech did not enter music during its scheduled clip. There were
no I2S loss/frame-rejection or crash markers in the two 400-second genre runs.

## Source versus deployment

The flashed S3 uses 80 MHz RAM and immediate entry for scores >=0.90, with two
consecutive ordinary positives. The worktree additionally contains a tested
two-of-three entry rule, allowing one interrupted window; it has not yet been
flashed or hardware-validated. Sustained exit/known-track retention is unchanged
by these entry rules. A low-score musical passage must not be used as permission
to clear a recognised album.

The final P4 production build includes the request-memory and complete
fingerprint fixes, plus the single-precision gain optimization.
The final boot check provides a processing-time measurement for that gain
optimization, but its five-genre accuracy/timing pass is still pending.
The temporary authenticated development bridge is disabled before returning
control to the user. Build/flash hashes and production boot results are below.
The five-second goal, fast room exit and broad accuracy
remain open; current evidence does not establish customer readiness.

## Wi-Fi startup reset caught before handoff

The first production boot monitor caught `assert failed: tlsf_free` / “block
already marked as free”, then a software reset. Its earlier filter counted
only Backtrace/Guru Meditation/abort markers and incorrectly printed zero;
raw-log review caught the assertion. The replacement monitor treats assertions,
register dumps, stack protection faults and stack overflows as failures too.

Addr2line with that build's ELF traced the free to `sdio_process_rx_task`,
ESP-Hosted 1.4.7 `sdio_drv.c:986`. The old driver calls `HOSTED_FREE(copy_payload)`
when the receive callback returns an error. IDF 6.1 supplies Wi-Fi remote 1.3.2,
whose callback consumes even a rejected buffer before a netif callback is
installed. The caller then frees the same memory twice. This precedes any
music lookup and is separate from the AES allocation failure.

[Espressif's 2.11.0 changelog](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/changelog.md)
documents the ownership correction; its
[P4/C6 double-free issue](https://github.com/espressif/esp-hosted-mcu/issues/210)
also describes the newer callback contract. The narrow backport keeps the
existing 1.4.7 host/C6 protocol rather than upgrading the radio stack. It pins
the component version and validates the exact original SDIO source hash,
generates a patched build source, and substitutes only that source in the
driver target. Downloaded managed sources stay untouched. If a future source
revision changes, configuration fails for review rather than silently skipping
the fix. A version-aware production helper preserves the older callback's
caller-owned failure contract. ASan/UBSan tests cover one free on success and
failure for both remote 1.3.0 and 1.3.2 contracts.

The intermediate P4 hash
`8ecc4291794da8ca64532aece66f0cddaa786bc13b606cf6e1c1dba452725b52`
was superseded after that boot assertion; it is not the handoff firmware.
Final ownership-fixed production P4 SHA-256:
`fdb6b057332efabd27c888baca2623806488a267df5503a26c680f05e680627f`.
The app-only write was hash-verified. The subsequent 120-second production boot
check reconnected the S3, acknowledged Mirrored mode, joined Wi-Fi and completed
a capture/fingerprint/TLS match/artwork request while music was already present.
It then returned to quiet. It recorded zero fatal markers with the expanded
filter, one expected power-on boot and no unexpected reset. The development key
was absent and TCP 8765 was closed. This single extended boot is evidence of the
fixed path, not prolonged reliability or repeated cold-boot qualification.
All Node/Python/Rust and firmware ASan/UBSan suites passed after the ownership
fix. S3/P4 IDF 6.1 builds passed. Changes remain uncommitted in the same worktree;
the S3 two-of-three entry rule remains unflashed pending isolated-audio testing.
Final private boot evidence: `/tmp/waveform-p4-ownership-final-runtime.log` and
`/tmp/waveform-ownership-production-summary.log`. The earlier failed assertion
is preserved in `/tmp/waveform-p4-genre-final-runtime.log` rather than overwritten.

Private evidence: `/tmp/waveform-profile-{40,80}mhz-*`,
`/tmp/waveform-genres-complete-*`, `/tmp/waveform-genres-memory-fixed-*`,
`/tmp/waveform-expanded-genre-evaluation.tsv`, and
`/tmp/waveform-classical-complete-direct-{headers.txt,response.json}`.
