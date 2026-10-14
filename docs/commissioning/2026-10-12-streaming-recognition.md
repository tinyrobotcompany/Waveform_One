# Streaming recognition — 12 October 2026

## Why

Live logs showed each attempt was a fixed cycle: 5 s pause, 4.1 s capture,
1.1 s fingerprint, 0.5–1.3 s lookup — about 11 s. A song that started during the
pause, or whose first clip missed, took about 20 s. Lookups every ~11 s drew HTTP
429 every four or five requests, each costing a 60 s cooldown. Quiet/playing
flicker from the S3 gate discarded captures in progress, and the gate reported
"playing" in a quiet room, so songs stayed on screen after the music stopped.

## What changed

1. **Continuous audio.** The S3 accepts `WF1 <id> STREAM` and sends PCM without
   end. The P4 keeps 16 s in a PSRAM ring; any gap starts a new recording.
2. **Streaming fingerprint.** The legacy algorithm runs per 128-sample hop as
   audio arrives. Peak selection only compares powers, so it runs on unscaled
   audio and the clip gain is applied when a signature is requested. The FFT uses
   ESP-DSP, checked against the reference transform at boot.
3. **Sound and silence on the P4.** Levels are compared with the room background
   learnt while quiet (6 dB for 0.6 s starts sound; within 4 dB for 2 s is
   silence). Silence clears the song immediately. The S3 gate is no longer used
   for recognition.
4. **One lookup per song.** First 3, 5 and 8 s of a sound, then every 30 s while
   unidentified. After a match, the service's song position plus the catalogue
   length time the next lookup to 3 s after the song ends; otherwise a check
   every 60 s, clearing after two misses. Burst of four, then one per 15 s.
5. **Album choice.** Apple's public catalogue search lists every release of the
   song. The service's album stands unless it is a single, EP, compilation,
   soundtrack or other recording; else the catalogue's most relevant regular
   album; the album already playing wins for the next song.

## Verification before hardware

- All firmware host test groups (27) pass under ASan/UBSan, plus the repository
  Node, Python and Rust checks. Both ESP-IDF 6.1 builds pass.
- Streaming fingerprint equals the upstream reference on its test input, equals
  amplify-first fingerprints at 13× gain, and equals a separate recording of the
  same window except the first 61 passes, where real history replaces silence.
- On the five public fixtures, 80–100 % of the peaks in 3/5/8 s streaming
  windows also appear in separately recorded clips (classical 3 s lowest at
  80 %); the differences sit in each window's first half-second.
- Album choice on saved real catalogue responses: Beyoncé → Dangerously in Love
  (service single replaced), Take Five → Time Out (soundtrack rejected), Girls on
  Film → Duran Duran (Decade/Greatest rejected), Inception → Inception, Whole
  Lotta Love → Led Zeppelin II, Popular → Wicked. The classical credit did not
  match, so the service's album is kept.

## Not yet verified

- **Service acceptance of streaming signatures, and the meaning of `offset`.**
  A desktop check against the recognition service was not run. The first device
  matches confirm acceptance; consecutive matches of one song should show offsets
  advancing with real time (logged as `offset_s`). If `offset` instead marks the
  clip end, next-song timing is early by up to 8 s; the check-after-miss path
  still recovers.
- Room thresholds come from one room's logs (background about −48 dBFS, music
  −36 dBFS). Tune with the `Listening` lines logged every 10 s.
- ESP-DSP speed on the P4: `analysis_cpu` in the same log line.
- Compilations whose names give nothing away ("The Impulse Story") are kept if
  the service picks them.

## Hardware trial

Flash both boards (S3 first: an old S3 ignores STREAM, so recognition stays idle
until both are updated). With the room quiet for the first 2 s after boot, then:

1. Play a song from silence: time needle-drop to title on screen (target ≤ 4 s).
2. Let it play to the next track: the next title should arrive within ~4 s of the
   change, with no lookups in between (`Lookup id=` lines).
3. Stop the music: the standard screen should return about 2 s later.
4. Talk or run a tap for a minute: at most three quick lookups, then one per 30 s
   until it fades into the background.
5. Play an album side: the album name should stay the record's own album.
