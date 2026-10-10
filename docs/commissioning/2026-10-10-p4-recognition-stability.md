# P4 recognition stability and search feedback

Branch: `feature/p4-recognition-stability`, based on merged PR #6 (`0a25902`).

## Source coverage and evidence

Reviewed the production S3 diagnostic decoder, USB polling, recognition worker,
session/retry policy, and all three LVGL/worker track-expiry paths.

Verified in source: a three-second gap in diagnostic reports previously became
“quiet”, changed the playback epoch and immediately cleared metadata. Separately,
a track expired 90 seconds after its last recognition even while music was active.
These paths explain possible mid-song clearing; they do not prove that every
user-observed background-noise incident took either path.

The corrected decoder distinguishes unknown reports from confirmed quiet.
Unknown reports pause new capture requests and preserve the playback epoch and
existing cover. Positive audio heartbeats renew track retention independently of
recognition success. Confirmed quiet, USB disconnection and Wi-Fi origin changes
still invalidate old results. A 90-second limit remains for abandoned telemetry.
Service backoff is unchanged and is never reset by missing reports.

Capturing/identifying pulse the existing artwork frame in cool lavender (`#A79AE8`) over 2.4 seconds and show
“Finding your song...”. Repeated progress callbacks do not restart the animation;
other statuses restore the normal border. Existing artwork stays visible. A lookup with an existing track says
“Checking what’s playing...” rather than implying the previous match was lost.

## Validation

- Firmware host suite passed with ASan/UBSan, including production USB transport.
- Regression tests cover missing-report state, recovery without an epoch change,
  confirmed quiet, and continued track retention beyond 90 seconds of playback.
- ESP-IDF 6.1 P4 build passed, including LVGL animation APIs.
- Full repository Node/Python/firmware/Rust suites passed. The Rust HTTP suite
  required an unsandboxed rerun to bind its loopback socket.
- The connected P4 was backed up and its application flashed at `0x10000`;
  esptool verified the written hash. Application SHA-256:
  `d5fc939d0d35a93414a56338a2044d931f9198d87c0a1a92a4500b6c87c1ed32`.
- Device logs confirm boot, display/touch initialization, Wi-Fi reconnection,
  S3 Mirrored acknowledgement, a complete 256,000-byte capture, fingerprinting
  and verified HTTPS lookups, including two recognized tracks with decoded
  artwork. Quiet/playing transitions were observed;
  without the user's corresponding playback observation, these do not establish
  a false gate close or successful silence handling.
- Recovery image and logs remain private under
  `/tmp/waveform-p4-stability-hardware-20261010/`. Saved settings were preserved.

## Remaining acceptance checks

Play a song for longer than 90 seconds, add the background noise that caused the
reported regression, and confirm the album remains visible. Confirm silence
still clears promptly, and the next lookup pulses the lavender frame without hiding the
current album. If a genuine quiet diagnostic occurs while music continues, use
that live evidence before changing the calibrated gate thresholds.

## Color and status refinement

The user confirmed the search effect was visible and requested a cooler color.
The pulse is now lavender (`#A79AE8`); the remaining green UI accents are unchanged.
Routine checks with an existing match say “Checking what's playing...”. They
start 30 seconds after a successful match to detect song changes, preserving the
current cover. A visible pulse is therefore not itself evidence of lost recognition.
The prior hardware log shows repeated successful matches roughly 40 seconds apart
(the pause plus capture/fingerprint/lookup), including a several-minute interval
without a quiet transition. This does not establish noise immunity or the exact
cause of every user-observed album disappearance.

The lavender/status refinement passed the ESP-IDF 6.1 P4 build. Firmware SHA-256:
`a1bf4ebc7bd99c1d49dbbf2e6a1a75b2ac5b237366027cc1a7c78390564c0225`.

Application-only flash completed and esptool verified its hash. Visual approval
of the lavender color remains with the user.

## Animated search placeholder and detection investigation

An attempted prototype used fifteen rounded lavender bars beneath the WAVEFORM
ONE text. Hardware testing exposed an instruction-access fault in the LVGL
refresh/flush path and repeated resets when searching began. The inner animation
was removed and the working lavender-border version restored. At that rollback,
the requested inner animation was incomplete; the replacement is recorded below.

The latest live log on the lavender firmware showed two complete captures,
fingerprints, successful HTTPS matches and artwork, with roughly 11 seconds from
capture start to the first displayed match. No capture, network or no-match
failure was reproduced in that observation. This does not prove the returned
track was correct or resolve the user's reported difficulty on an unidentified
song; the failing song/artist and corresponding playback observation are needed.

The wave animation passed the ESP-IDF 6.1 P4 build after correcting typed LVGL
flag calls. A premature flash reinstalled the previous lavender image before the
corrected build; it completed with hash verification. The final animation binary
SHA-256 is `0cc5bc9f1a090e4afb1e2850a6d12daba1c16adcca0526d27a9d970aaa7944e4`.

The crashing binary was rolled back. The recovery build passed and application-only
flash was hash-verified. Recovery firmware SHA-256:
`e96c9742389b6469a4aac8a3a9acc97919de1dcb11098aea0939726cc3887680`.
Crash logs and the ELF were preserved privately for diagnosis. Python processes
used here are Mac-hosted ESP-IDF/esptool and serial diagnostics; recognition on
the P4 remains native C++.

Recovery verification: the device log reached at least 77 seconds of uptime,
with one initial power-on boot and no panic or subsequent reboot. USB capture
continued. Numerous quiet/playing transitions invalidated captures; this does
not prove false silence without a corresponding continuous-playback observation.
The Mac serial log reader was then stopped. The device remains on the recovered
lavender-border firmware, without the inner-wave prototype.

## Fixed-image replacement for the inner animation

The replacement renders the same rounded lavender waveform into one 320×90
RGB565 image beneath the logo. It allocates a 57,600-byte aligned buffer once,
updates pixels on the LVGL timer thread at 25 frames/second, and invalidates the
single fixed artwork rectangle. There are no independently moving/resizing bar
objects. Search completion pauses the timer and hides the image; a displayed
cover is never overlaid. Repeated search callbacks do not restart the cycle.

The crash dump from the discarded prototype contains rectangle coordinates in
the corrupted stack, with surviving addresses in LVGL refresh and the adapter's
partial-frame flush path. The pinned adapter also contains an unbounded legacy
rectangle-splitting routine. This supports avoiding fragmented damage regions;
the exact corruption mechanism has not been independently reproduced.

The full firmware host suite passed with ASan/UBSan, including new tests for
all 1,024 animation phases, guarded buffer boundaries, fixed gutters, rejected
buffer sizes and seamless cycle wrap. The ESP-IDF 6.1 P4 build passed before
flash. Application-only flash completed with hash verification. Binary SHA-256:
`31cadbdb89c1801f724ef585d96d0bf3186b1adf3c89ad3e73954c392dd08622`.

The device was observed for three minutes: one initial power-on boot, no panic
or subsequent restart, and four successful recognitions. Testing included the
initial search, routine checks with retained artwork, a quiet/playing transition,
a fresh search returning no match, and its later successful retry with artwork.
The serial reader then exited normally; recognition remains native C++.
Visual approval remains with the user, and this is a smoke test rather than
proof of prolonged noise immunity or universal song coverage.

## Album-first phone remote and final visual adjustments

The user approved the fixed-image search animation. Its waveform remains
lavender. An initial search has a lavender frame; an identified track has a
green frame both while idle and during routine checks. The native NOW PLAYING
font increased from Montserrat 16 to 48, with tighter metadata spacing to retain
room for long titles, artist, album and status.

The old native phone page had a static listening placeholder and permanently
visible settings, with no recognition-state route. It now opens on artwork,
song, artist and album; settings are a separate modal. `/api/state` requires a
valid paired cookie and device Host, returns uncached JSON, and owns a synchronized
copy of the worker's borrowed track. Expiry uses the shared audio heartbeat
policy. Settings preserve CSRF/Origin checks; style reflects USB acknowledgement.
The phone keeps a known album during checks and temporary network failures,
disables controls on session expiry, and renders metadata as text. Artwork URLs
are validated and loaded directly by the phone under a restricted HTTPS
`*.mzstatic.com` image CSP.

Validation completed before flash:

- Full firmware host suite with ASan/UBSan, including snapshot ownership,
  concurrent callback/read consistency, expiry/playing retention, JSON escaping
  and rejected artwork origins.
- Headless Chrome checks of the actual embedded page at 320/390/768/844-pixel
  widths: album-first layout, settings/CSRF requests, search and retained-cover
  checks, missing/failed artwork, metadata injection, reconnect and session loss.
  Screenshots were visually inspected. Recognition and artwork in this browser
  check are fixtures, not live device responses.
- ESP-IDF 6.1 P4 build succeeded; application-only flash was hash-verified.
  Binary SHA-256:
  `b257a2ebe438c7668c0a10bf8b9a9509a0caf9b796bbbaf493123ea6e0d3bb06`.
- The live device served a byte-for-byte match of the tested phone page (200)
  and rejected an unpaired state request (401). Post-flash serial logs confirmed
  successful recognition and artwork retrieval.

The serial smoke check ran for 130 seconds and then exited normally: one initial
boot, no panic or subsequent reset, and three successful recognitions while
music remained active. The serial reader is stopped; the device remains on the
new firmware.

## Native status parity and Unicode typography

The P4 feedback row now mirrors the phone: green dot with “Enjoy the music”,
lavender dot with “Finding your song…” or “Checking what’s playing…”. Retained
tracks remain visible during checks/misses; the feedback uses the same known-track
semantics as the phone. Song titles increased to 40 pixels, artists to 32,
albums to 24 and feedback to 20. The metadata panel grew downward with bounded
two-line fields, leaving a separate feedback row.

The bundled LVGL Montserrat fonts contained ASCII, degree/bullet and UI icons,
but not curly apostrophes or accented names. Generated fonts now include 1,871
glyphs at every used size, preferring the same Montserrat source and filling
missing characters from DejaVu. Coverage includes Latin/extended Latin,
Greek/Cyrillic, combining marks, punctuation, currency, arrows and musical
symbols where present in those sources. Licenses and sources are retained.
Full Unicode, CJK, emoji and complex-script shaping are not claimed. The native
label helper checks actual glyph availability, preserving supported UTF-8 and
using `?` for unsupported/malformed characters; the phone retains raw metadata.

The generator preserves kerning and produces plain, static 4bpp bitmaps, with
no new font decompression path. Sparse character maps preserve genuine holes
in the pinned LVGL lookup. Directional controls already rejected by recognition
are excluded; compiler bidi checks remain enabled. Generated C files are marked
as generated for review, with character coverage checked against their tables.

Validation before flash:

- Firmware host suite with ASan/UBSan, including feedback parity and UTF-8
  preservation/fallback checks.
- Six Node font-table tests check the entire declared coverage against the
  generated C maps, including straight/curly apostrophes, French accents,
  smart quotes, dashes, musical and copyright symbols.
- A host build of the pinned LVGL checks actual glyph descriptors and bitmaps,
  missing-glyph fallback and metadata/status separation. It renders the actual
  native panel construction and label helpers, extracted from `main.cpp`.
  Apostrophe, long accented-title and lavender search previews were visually
  inspected; the fixed waveform still fits below the logo.
- ESP-IDF 6.1 P4 build succeeded before flashing; application-only flash was
  hash-verified. Firmware is 4,693,504 bytes with 44% of its app partition free.
  Binary SHA-256:
  `5b0a6db421bc09326848c3cb65873568639511e25eaecb6a45b273c77e0b56e0`.

The post-flash serial check completed 130 seconds: one initial boot, no panic
or subsequent reset, and four successful recognitions. Quiet/playing transitions
and new searches also occurred. This verifies a short hardware smoke test;
the font previews use fixture metadata and do not independently confirm the
identity of the song playing in the room. The serial reader has exited.

Real paired-phone visual acceptance and the enlarged native heading remain
user checks. Sustained playback/noise immunity is still separate from this
short smoke test. The backlog now includes LED visualization EQ, explicitly
independent of raw recognition audio and speaker playback.
