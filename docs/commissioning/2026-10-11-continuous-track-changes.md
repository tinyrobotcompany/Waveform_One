# Continuous-playback track changes — 11 October 2026

> Historical experiment: the active firmware is now restored to the pre-filter baseline. See [complete rollback](2026-10-11-complete-filter-rollback.md); retained changes described here are no longer deployed.

The user reported that the previous song remained on screen for more than a
minute after the music changed. Work stays on `feature/p4-recognition-stability`
in `.worktrees/p4-recognition-stability`; no merge to main is involved.

## Confirmed code defects and contributing behavior

`recognition_policy::expired` used the newer of the last match and last audio
activity timestamps. Continuous sound therefore kept old metadata alive
indefinitely, including after no-match and failed lookup responses. Worker,
native UI and phone state all used this policy. Sound activity cannot establish
that the same track is still playing.

The matched retry delay was 30 seconds. Failed capture/service attempts use an
independent 60–300-second capped backoff and honor Retry-After. These delays
explain how a previous song can remain visible while no new identification is
available. The prior startup check recorded HTTP 429; the exact cause of the
user's latest delayed transition was not captured before deployment. It must
not be attributed solely to a specific service failure without its logs.

## Change

- Match freshness is bounded to 30 seconds after the last successful lookup,
  independent of continuing audio. Native UI has a one-second expiry timer;
  worker and phone apply the same deadline.
- Confirmed-track rechecks use a full eight-second rolling sample. They do not
  perform a rapid short/long pair on each miss. True stop/start retains the
  shorter four-second initial probe and its one permitted fallback.
- Matched rechecks and ordinary misses wait 15 seconds after an attempt.
  A five-second trial received HTTP 429 and was withdrawn; failure and service
  cooldowns remain unchanged.
- Expiring a track does not reset retry state, cancel a service cooldown or
  renew metadata merely because audio remains active.
- The music/speech classifier remains removed. UI, phone, fonts, animation,
  raw rolling capture and previous transport/memory fixes remain.

Shortening a recheck delay does **not** establish a five-second identification
time. Snapshot history can include both songs at a boundary and an eight-second
sample does not guarantee an immediate new-song match. The present backend has
also rejected faster repeated requests with HTTP 429.

## Validation

Full firmware host tests passed with ASan/UBSan. Added regressions cover an old
match expiring despite continuous activity, phone-state boundary expiry,
service cooldown surviving expiry, full-sample rechecks and fast stop/start.
ESP-IDF 6.1 P4 production build (development bridge OFF) passed.

First announced 55-second hardware test used an intermediate **15-second**
matched recheck delay. Jazz (Take Five) began at 5.041 seconds; correct match
key 53531056 arrived at 13.570. Jazz ended at 30.821; rock (Whole Lotta Love)
began at 30.827, a 6 ms player handoff. Activity stayed Playing throughout.
An already captured jazz snapshot still matched jazz at 31.953, then the full
rock sample matched key 61106767 at 50.388: **19.561 seconds after transition**.
There were no HTTP rejection, classifier, fatal or reboot markers in the trace.
The result was correctly treated as too slow; it prompted the five-second
recheck change. It does not prove broad genre reliability or the five-second
recognition target.

The announced repeat with a five-second matched pause began jazz at 5.053
seconds and rock at 30.314, a 3 ms handoff. Jazz matched correctly at 13.820,
22.389 and 30.368. The first rock recheck returned HTTP 429 at 38.347; no
Retry-After header was supplied, and the independent failure backoff applied.
No new song was identified during that pass. This does not prove an exact
provider request quota or establish that every earlier failure had this cause.
It does demonstrate that the faster trial did not deliver reliable transitions.

The five-second schedule was withdrawn. Final deployment retains the 30-second
match freshness bound and full-sample rechecks with the tested 15-second pause.
Further backend work is required for the five-second recognition target.
No private microphone PCM is stored; tests use public previews and record
timestamps, hashes and service match keys.

Final P4 application SHA-256 (written hash verified):
`438d9d0603150d5f41d4a6e39b5aa9047ef80d1edb737358f23ff27154473b8a`.
The S3 classifier rollback remains unchanged. Main and origin/main remain at
`0a25902733f5cd35177a0d46342e2c8fac396ce8`.

## Alternative backend benchmark — public files only

AudD's documented standard API accepts audio files and returns song metadata,
including Apple Music data. Its documentation states that the standard endpoint
has no rate limits; sustained account usage is billed per request. Official
sources: [API documentation](https://docs.audd.io/) and
[pricing](https://audd.io/pricing/). This is a candidate, not a proven device fix.

Four-second mono 16 kHz WAV clips were extracted directly from each current
public corpus WAV (not the older cached `.pcm` fixtures). Anonymous endpoint
benchmarks returned the correct song and artist for Beethoven Symphony No. 5
in 0.887 seconds, Take Five in 0.906 seconds and Whole Lotta Love in 0.691
seconds, including the desktop HTTP round trip. Returned album editions can
differ from the catalogue preview's release; song identification does not prove
the exact album edition. No room audio was recorded, saved or uploaded.

An earlier request made from an older cached `classical.pcm` returned Für Elise.
Its bytes differ from the current classical WAV prefix, so that result is
excluded from the Beethoven benchmark. Fixture provenance must be checked
before comparing backends; filenames alone are insufficient.

AudD is not integrated or enabled on the device. Unlike the present Shazam
fingerprint request, its standard API requires sending a short audio clip.
Moving to it requires the user's choice about audio upload and a provider token
for repeatable usage. The published trial offers 300 requests for 14 days for
eligible new accounts without a credit card. Public-file results are not a
five-second hardware acceptance test; noise, microphone quality, all genres,
continuous transitions, metadata editions and operating cost remain to assess.
