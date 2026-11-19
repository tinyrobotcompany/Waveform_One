# Proposed recognition pilot

This pilot checks the practicality of the larger protocol in PR #26. It is not
a measured accuracy claim or a replacement for a representative benchmark.

1. Freeze a list of 20 recordings before testing: four rock/pop, four acoustic
   or quiet-intro, four electronic/dance, four jazz/classical/instrumental, and
   four live/cover/remaster examples. Record exact versions and expected artist
   and title; score album edition separately. Use legally available playback.
2. Play each recording once in a quiet room and once with controlled background
   noise: 40 acquisition trials. Record actual distance, music/background SPL,
   speaker and network conditions. Use a fixed 30-second recognition deadline
   and a saved randomized order, retaining failures rather than replacing them.
3. Add ten mid-song changes, ten full-song/gapless transitions and ten stops to
   silence. Run ten one-minute non-music trials (speech, room noise and silence).
4. Record firmware/image identifier, provider project mode (without secrets),
   timestamps for playback/display events, wrong answers/corrections, timeouts,
   request counts, and returned error types. Count displayed answers against
   ground truth, not merely successful API responses.
5. Report counts and denominators, first/stable-correct latency, wrong/no-match
   outcomes, false positives, silence-release timing, and requests/listening hour.
   Reconcile requests with the provider's usage window when available. State
   that this small selected set is a pilot; do not extrapolate a sales accuracy
   percentage. No raw household audio or credentials belong in published data.

Suggested first session: establish the setup and run ten recordings plus five
changes/stops to check logging and timing. Freeze any protocol corrections before
the complete pilot; keep rehearsal data separate. Once the pilot is repeatable,
proceed to the 100-recording, three-condition protocol in PR #26 before tuning
request frequency. Hardware and listening time must be scheduled with the owner;
this document neither starts playback nor sends paid recognition requests.

For the offline fix, first use a spare/test device with no configured key and
confirm cold boot, persistent configuration message, reactive LEDs and local
controls. Then test an already-provisioned device with WAN blocked. Do not erase
the working prototype's credentials to create the first case. Verify actual OTA
trial acceptance on hardware in addition to host startup/persistence tests.
