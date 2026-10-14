# Recognition handoff from Claude — 12 October 2026

The owner ended this session after the trial below and is returning the work
to Codex. Nothing here is accepted. Logs are in
`build/music-recognition-benchmark/recognition/streaming-runtime*.log` in the
primary project folder.

## What is on the branch

- **Audio:** the S3 streams PCM continuously (`WF1 STREAM`). The P4 keeps 16 s,
  fingerprints each hop and detects sound and silence from the levels
  (`listening.h`).
- **Recognition service:** ACRCloud when an untracked `acrcloud.local` exists at
  the repository root. CMake generates `acrcloud_config.h` from it; never commit
  either. The P4 sends a signed 8 kHz WAV and parses it in `acrcloud.h`.
  - Lookups run at 3, 5 and 8 s, at most 12 per minute.
  - Without credentials it falls back to Shazam, at 4 and 10 s, at most 3 per
    minute. Shazam answers a fourth lookup in a minute with a 429 and a one-minute
    lockout.
- **Album choice** (`album_resolver.h`): the catalogue's most relevant regular
  album wins over compilations named by the service.
- **Album following** (`album_sequence.h`): after a match, the P4 loads the iTunes
  track list and shows each next track at a gap near the expected end. It
  returns to live listening when the audio stops fitting the album.
- **Fixes along the way:**
  - TLS and JSON memory moved to the ordinary heap and PSRAM.
  - A 32 KB static buffer in the FFT self-test made one boot in three crash while
    creating the idle task. Removed; 5 of 5 resets clean afterwards.

## Owner's last trial (17:49–17:59 UTC)

- **Worked:**
  - First track on screen in about 4 s, with the correct album (Rubber Soul, not
    the service's "1962–1966").
  - The next track appeared at the change, before any lookup.
  - Skipped tracks were corrected quickly.
- **Album art is not stable:** it sometimes drops during a song.
- **Stop to rest screen took 9 s** (owner's count).
- **A new album was not identified after 30 s or more.** 12 lookups from 17:56:47
  returned no match. The levels show music at +10 to +20 dB over the
  background, yet `Silence` and `Sound started` alternated every few seconds.
  Each cycle restarts the quick tries and the cautious mode.

## Open problems, in order

1. **Silence is declared during loud music.** Check that `quiet_history_` is only
   compared over blocks recorded in the current sound state. Better, replace
   level-only activity detection with a music classifier; there is a YAMNet
   prototype from commit `92103a6`. Air conditioning and talk also trigger
   lookups.
2. **Artwork drops mid-song.** Likely the display clears on a false silence and
   the restore path reuses the image. Verify on the device.
3. **Album following trusts the catalogue album's track order.** That order is
   wrong when the listener plays a compilation.
4. **Lookups take 0.5–3 s on the device but 0.2–0.3 s from a Mac.** The connection
   is now kept open; this is not yet measured after that change.
