# Separate song-identification check — 12 October 2026

The first benchmark measured fingerprint generation and music classification separately,
but did not call the recognition service. It was incomplete as a song-identification test.
This second stage completes an initial service check with the music filter entirely bypassed.

Five saved public previews were tested at offset zero using four seconds of PCM each.
Only their generated fingerprints were sent to the same endpoint and with the same request
format/user agent as the production firmware. No audio was played, recorded or uploaded.
No microphone, P4 or S3 was accessed. The first stage's classifier results are not included
in any identification timing below.

| Case | Returned song | Measured service seconds | Calculated 4s + fingerprint + service seconds | Album review |
|---|---|---|---|---|
| jazz | Take Five | 0.708 | 4.714 | matches_source_album |
| classical | Symphony No. 5 in C Minor, Op. 67: I. Allegro con brio | 0.479 | 4.486 | matches_source_album |
| beyonce | Crazy In Love (feat. JAŸ-Z) | 0.542 | 4.550 | single_release_instead_of_source_album |
| wicked | Popular | 0.334 | 4.341 | matches_source_album |
| rock | Whole Lotta Love | 0.352 | 4.358 | matches_source_album |

All five returned the expected song and artist, with four matching source albums and one
returning an alternate release: Beyoncé's single instead of Dangerously in Love. The Dave
Brubeck artist label is shortened relative to The Dave Brubeck Quartet; title and Time Out
album match. Requests were spaced at least sixty seconds apart; no HTTP error, Retry-After
or rate-limit response was observed. This interval is a conservative test choice, not a
claim about the service's documented quota.

The actual measurement is service request/response latency (0.334–0.708 seconds). Adding
four seconds of audio plus the measured desktop fingerprint time gives a **calculated**
4.341–4.714-second budget. It is not a stopwatch result from music starting on physical
hardware: input files were already recorded. MacBook compute, TLS/network behavior, USB,
scheduling and screen refresh differ from the P4. Artwork download is excluded.

The P4 firmware is unchanged and still collects eight seconds. Its recognition time has
not yet improved. A shorter capture implementation and actual device timing remain necessary
before claiming the five-second target. Five clean excerpts at one offset do not establish
reliability across genres, recordings, microphone noise or service churn.

`catalogue_check.py` is an explicitly invoked network check, separate from the network-free
`benchmark.py`. It caps a run at five known/checksum-verified public fixtures, uses response
size/time limits, rejects redirects, requires at least sixty seconds between requests, and
stops the run on any HTTP error or service Retry-After. Tests mock the network and verify
rate-limit stopping, response bounds and metadata parsing; they never call the live service.

Evidence: project-local `build/music-recognition-benchmark/catalogue-4s-offset0.json` and
`catalogue-4s.log`. The service result metadata above was reviewed against the fixture manifest.
