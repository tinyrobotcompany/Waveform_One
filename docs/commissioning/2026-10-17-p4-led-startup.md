# P4 LED first hardware test — 2026-10-17

User confirmed S3 signals disconnected and separate 5 V panel supply. Reported
wiring matches the proposed GPIO mapping; ribbon orientation and direct 3.3 V
signal margins have not been electrically measured. No level buffers fitted.

Connected P4: revision 3.2, 32 MB flash, USB Serial/JTAG /dev/cu.usbmodem101.
Read device partition table matches factory app at 0x10000, size 8 MB.
Original full app partition backed up before flashing; no NVS/storage writes.
Evidence in primary checkout build/p4-led-test-2026-10-17/ (manifest, original
app, partition table, flash log, diagnostic-serial.log).

The original diagnostic binary flashed and hash-verified. It started PARLIO
at 2537 ms, then failed DMA handoff at 3085 ms and deliberately blanked OE.
Onboard ES7210 microphone initialized. This is a verified firmware startup
fault, separate from the user's earlier white flicker with LED output disabled.

Installed ESP-IDF v6.1 esp_driver_parlio/src/parlio_tx.c initializes
buffer_need_switch=false and sets it only on replacement loop transmission
(around lines 680–684). The EOF callback dispatches on_buffer_switched only
when that flag is true (around lines 740–750). Initial transmit emits no such
callback. The worker incorrectly waited for it on its first frame.

Fix: start an immutable all-OE-high blank frame, record initial ownership only
after successful transmit, then submit the other buffer and await a real
replacement event. Keep the 500 ms timeout and fail-blank path. Host regression
covers no-event startup, rejection of active buffer writes/reinitialization,
and first replacement ownership. Full firmware host suite passed.

Corrected firmware native build passed and app-only flash hash verified.
Corrected runtime log fixed-serial.log shows diagnostic PARLIO startup, onboard
ES7210 microphone initialization, and repeated DMA frame replacements (201,
233, 229 updates in successive reporting intervals) with no handoff timeout.
Largest internal DMA block was 30208 bytes at 11 and 16 seconds.
Panel remained powered off for this verification; visual test is next. Physical pattern, signal integrity,
audio visualization and recognition coexistence remain acceptance tests.

## Visual diagnostic and audio-mode flash

Simon confirmed the diagnostic pattern was working well. This validates the
basic direct-wired pattern on this setup, not electrical margins or long runs.

With Simon's approval, rebuilt with CONFIG_WAVEFORM_P4_LED=y and diagnostic
pattern disabled. All seven audio/recognition/memory preservation settings
verified. Build passed, app-only flash hash verified. Source startup fix is
unchanged. Audio runtime log shows mode=onboard_audio, ES7210 microphone, and
repeated frame updates without a DMA timeout during the 25-second capture.

Simon reported white flickering bands while the flash was still writing
(approximately 54% complete in contemporaneous flash log). Panel power-off
requested during flash. This observation does not establish steady-state
audio-mode behaviour. Audio visual test and recognition coexistence pending.
Artifacts: primary build/p4-led-test-2026-10-17/audio-flash.log and
audio-serial.log; worktree build/p4-led-startup/audio.bin and audio-build.log.

## LED response correction

Simon reported fast music recognition but only partial LED response in all
three styles. Code comparison found the experimental 512-point/16 kHz FFT
assigned no bins to band 1 (73–90 Hz approximately), and omitted S3's display
attack/release envelope. The S3 active visualizer_config.h overrides reference
audio defaults: -80 to -35 dBFS, attack 40 ms, release 250 ms. The range already
matched P4; the corrected code reads this active profile directly, using only
its display fields (no recognition gating/calibration changes).

LED FFT now uses 1024 samples, providing bins to all 24 bands. Read the latest
complete window with overlap each update; reject truncated/discontinuous data.
Use elapsed time for smoothing and reset on stream breaks/stale input.
Logs now distinguish panel updates from spectrum analyses and report peak RMS.
Regressions cover all bands populated, a 78.125 Hz bass tone, attack/release,
update-rate-independent smoothing and reset. P4's 16 kHz microphone stream
still limits visualization to 7.5 kHz versus the old S3's 16 kHz upper range.
Live musical equivalence has not yet been established.

Response build host suite and native build passed; app-only flash hash verified.
response-serial.log confirms onboard_audio with 178–214 analyses per roughly
five-second reporting interval (36–43/s), continuous DMA updates, and no LED
timeout during the 25-second check. Largest DMA block settled at 25088 bytes.
At 16179 ms recognition reported Album cache ready collection=1441164359
tracks=14 bytes=23628 failure=none. This verifies cache population in the
actual new firmware, but not yet a same-album skip/cache-hit trial.

## Paused-versus-playing band measurement and pending calibration build

Primary build/p4-led-test-2026-10-17/quiet-band-baseline.json records seven
quiet intervals. music-band-comparison.json records seven music intervals.
At 2743–3354 Hz: quiet median -95 dBFS, music -82 dBFS, displayed 2%.
Several upper bands rose 8–13 dB with music but displayed 0–3%. Thus the fixed
absolute LED range discards measured musical energy; this does not establish
frequency loss in microphone capture. The earlier raw quiet/music levels
were approximately -72 and -61 dBFS.

A built, not-yet-flashed LED-only Levels mapper calibrates 3 seconds of quiet
(80th-percentile per-band power, at least 64 frames), saves 24 powers in NVS
namespace p4_led/noise_v1, and reuses them on reboot. Initial calibration must
be performed while Simon confirms music paused. This room reference is not
a music/speech classifier. A different room may require recalibration; no
user-facing recalibration control has been implemented in this experiment.

Display mapping spans 3–24 dB above each band's reference with existing 40 ms
attack / 250 ms release. Those limits are provisional visual design choices,
not measured product acceptance thresholds. Raw band RMS and recognition PCM
are unchanged. Recognition gating, gain, service cadence and album cache are
unchanged. Tests cover quiet suppression, persistent-reference validation,
equal SNR across different absolute band levels, envelope timing and
non-mutation of spectral data. Host suite and native P4 build passed.
Subsequently flashed with music paused. The serial log confirms
`LED room reference: loaded` after a controlled restart at 11:02:02 local.
Simon then confirmed the LED response was even better than before and
requested that it remain unchanged. Preserve this LED code and saved reference.
