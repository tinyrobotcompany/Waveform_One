# Recognition stability — 17 October 2026

Branch `fix/recognition-stability`, on top of Codex's `fix/p4-led-dma-startup`
(`82a7df1`). Accepted by the owner on the device ("everything worked
perfectly"). Logs: primary checkout `build/recognition-stability-2026-10-17/`.

## Root causes

Four symptoms, three causes. Evidence: about 1,000 logged lookups from
`build/p4-led-test-2026-10-17/` and `build/album-colours-audio/`.

1. **The display never returned home, ACRCloud ran around the clock and
   the LED showed room noise.** Continuous mode treated any PCM as music.
   It was enabled because the level detector missed music at the P4
   microphones' low level: its background rose with quiet music (1.25 dB/s)
   and it decided on single 0.2 s blocks. `Planner::missed()` was also never
   called. One log made 357 requests in 15 minutes.
2. **Jazz rarely matched** (Mack the Knife 0/89). Almost every lookup sent
   a 5 s clip, because a shown song was never cleared. Pop and rock matched
   80–90% even so.
3. **Talk lit the LED and started lookups.** Level alone cannot tell talk
   from music.

The P4 microphones hear music only 5–12 dB above a -72 to -77 dBFS room.
Raising the gain or normalising digitally cannot add signal-to-noise.

## Changes

- `cda3eb0`: The level detector decides on several blocks, and stops when
  the median is within 2.5 dB of the room for 3 s (8 s with a song shown).
  The background never follows sound more than 3 dB above it. It is
  replayed against the logged sessions in tests. Clips are 10 s. Continuous
  mode and the PCM diagnostic are off in the local sdkconfig.
- `bed47fe`, `343af08`: Codex's YAMNet classifier (`92103a6`) runs on the
  P4, only while sound is heard (about 360 ms per window).
  - **Model storage:** a 4 MB `music_model` partition, taken from the
    unused `storage` partition, with the SHA-256 checked at boot.
  - **Memory:** a 1.5 MB PSRAM arena; ESP-NN needs about 1.06 MB.
  - **Music decision:** three music-like windows with separate audio
    within 6 s, held for 15 s. Real talk scored up to 0.67 in single
    bursts.
  - **What music controls:** the session, the listening display, lookups
    and the LED. A shown song also counts as music. Sound with no music
    verdict for 30 s gets cautious lookups.

## Device results

| Test | Before | After |
|---|---|---|
| Silence | Lookups every 2.5 s | None |
| Talk | LED lit; lookups | LED dark; home screen; no lookups |
| Iron Maiden, Invaders | — | Matched 6 s after music verdict, 16/16 |
| Joshua Redman, Mack the Knife | 0/89 | 11/11 |
| Music stops | Album stayed | Home screen |

Flashing changed the partition table (backup:
`device-partitions-before.bin`). NVS was not touched. A blank device needs
the standard flash arguments, which include the model.
