# Recognition and artwork acceptance target

Owner target, 27 September 2026: the new track's correct album artwork must be
visible on the Pi no later than four seconds after the new song starts.
Measure the complete path, including capture, identification, UI polling, image
fetch/decode and paint. An API match timestamp alone is not sufficient evidence.

## Current measurements

Known track: Gloria by U2. The live worker correctly returned Gloria/U2 and a cover
URL. Recorded successful attempts: about 8.07–8.12 seconds capture plus 0.61–0.63
seconds identification. The owner observed four seconds to recognition while a
capture was already in progress. This does not establish the worst-case target.

The currently deployed ESP protocol and Pi decoder require complete eight-second clips.
They cannot satisfy the new target consistently. Simply shortening the delay
between requests will not fix this bottleneck.

## Required experiment

Implement a backward-compatible shorter capture path and evaluate approximately
2–3 seconds of audio, with longer fallback for difficult passages. Budget the
remaining time for fingerprinting, service response and actual cover rendering.
Test arbitrary song-change offsets, quiet introductions and ordinary listening
volume. Record first correct artwork latency, match success rate and wrong matches;
keep the previous artwork visible while identifying. A longer fallback counts as
a latency miss, not a four-second success.

The target is not yet achieved. The unofficial recognition provider and remote
artwork service cannot provide a hard four-second guarantee for every song or
network outage. Report misses explicitly; do not substitute incorrect artwork.
If consistent four-second performance is mandatory across all conditions, a
playback-metadata integration or locally available identification/catalogue path
must be evaluated rather than claiming acoustic recognition has such a guarantee.

Keep the diagnostic ESP baseline unchanged until the shortened protocol is tested
and a controlled firmware test is scheduled. Automatic updates remain paused.

## Music stopped while background audio continues

Audio activity alone does not prove music is playing: television can keep the
ESP session active. Keep artwork through isolated recognition misses, but clear
it after three consecutive completed no-match results when the last match is at
least ten seconds old. A successful match resets the miss counter. Expire the
last match after 45 seconds even when the provider fails. Silence or a new
session clears it immediately. These are recognition heuristics, not a reliable
music-versus-speech classifier. With the legacy eight-second capture, three
misses can take roughly 30 seconds; the short-capture experiment reduces that
wait. TV music that is itself recognized may legitimately produce a new match.

## Development status — 28 September 2026

The branch implements optional `CAPTURE 2` and `/api/capture/short`, retaining
the existing eight-second protocol. `WAVEFORM_FAST_RECOGNITION=1` enables the
overlapping capture/recognition experiment; its default remains disabled.
Firmware build, host tests, and the Linux serial/HTTP integration test on the Pi
passed. The Pi binary and ESP image were staged under
`~/waveform-incident-20260927/fast-build`, but the firmware transfer and activation
were not executed. Testing was postponed by the owner. No four-second hardware
acceptance result exists.

The deployed recognition-worker diagnostic uses bounded artwork retention with
the legacy capture path. The fast deployment is paused while a possible
ESP-controlled display architecture is evaluated. Preserve the current working
hardware as a reference; the new display does not itself replace ShazamIO.
