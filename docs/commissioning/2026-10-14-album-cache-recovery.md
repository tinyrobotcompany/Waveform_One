# Album cache recovery

Baseline: `b904af4` (P4 onboard microphone, continuous recognition, PR #9 colours).
The LED takeover experiment is not included. No recognition timing or microphone
settings are changed.

## Evidence

The saved `build/album-colours-audio/combined-serial.log` reports unavailable
track lists twice for collection 1530391535, then ready caches for U2 collections
1443144590 (11 tracks) and 1440884549 (8 tracks). It does not identify why the
ABBA responses failed; do not claim a proven HTTP or oversized-album cause.

Source tracing found:

- Failed track-list fetches were discarded without retry.
- Catalogue resolution/prefetch was inside the missing-artwork branch. A good
  provider cover could permanently suppress catalogue recovery.
- Track lists above 64 songs were rejected despite requesting up to 200.
- Cache/context lifetimes were measured from initial loading, not continued
  verified playback, allowing a cache to expire during a long album.

## Change

Track-list jobs retry in their background task, with three total attempts and
30/60-second delays (or longer service Retry-After). A newer request invalidates
old completions. Catalogue lookup retries after 30 seconds on a fresh match,
even if a cover already exists. Covers can update the pending/completed cache
without fetching its track list again. Successful same-album cache matches
refresh inactivity timestamps, including matches of the currently shown song.

Track capacity is 200, with a 512 KiB HTTP-body limit. The bounded track storage
is allocated in PSRAM; large lists increase PSRAM use and parsing work. Failure
logs distinguish HTTP retrieval from unusable track-list contents.

Host regression coverage includes large box sets, overflow rejection, bounded
retries, Retry-After, success and superseded requests. Full tests and native
build results are recorded in `build/cache-recovery/` in this worktree.

## Hardware acceptance still required

Observe `Album cache ready`, then `Album cache hit` after a same-album song
change. Measure provider-match-to-display timing separately from acoustic-onset
timing. Check artwork, colour, microphone source, reset count and DMA headroom.
An unavailable cache must retry without needing the artwork to fail.

This is a metadata/artwork cache, not local audio recognition or time-based
next-track prediction. A fresh provider match is still required before showing
a different song. The saved working binary remains available in the primary
checkout under `build/album-colours-audio/preserved-2026-10-14/`.
