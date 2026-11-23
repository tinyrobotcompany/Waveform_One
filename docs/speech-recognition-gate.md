# Speech and song recognition

The P4 uses the existing YAMNet model locally. Previously every sustained sound
could enter the cautious recognition fallback after 30 seconds, including a podcast
that the classifier correctly declined to call music. LEDs used a different gate.

The new speech decision requires two separate 0.975-second windows within six
seconds, with a speech score at least 0.6 and a music/singing/rapping score below
0.2. Evidence expires after six seconds. Music evidence clears speech suppression;
normal music detection still requires its existing three independent votes.
These scores are not calibrated probabilities, and the thresholds are policy
choices requiring hardware validation, not measured accuracy claims.

Speech classes use the existing model's YAMNet indices 0, 1, 2, 3 and 5. Singing
and rapping remain music classes 24 and 31; music is class 132. Source class map:
https://github.com/tensorflow/models/blob/master/research/audioset/yamnet/yamnet_class_map.csv

Confirmed speech overrides a previously visible song and the cautious fallback.
It resets the uncertain-sound timer, stops the recognition planner, and blocks new
requests even during an already-started acquisition. ACRCloud checks this decision
before opening a connection and between bounded transfer chunks. Data already sent
cannot be recalled. Uncertain audio and unavailable-classifier behavior retain the
existing fallback. This is not a privacy guarantee or perfect speech detection.

## Verification

Host tests cover long podcasts past the fallback deadline, stale visible songs,
acquisition bypass, overlapping windows, expired votes, silence/reset, invalid
scores and singing/rapping. Existing music-presence and recognition HTTP cancellation
tests remain required. Build the P4 firmware before release.

Physical acceptance is pending: play at least ten minutes of voice-only podcast,
including short pauses, followed by quiet instrumental music, singing and rap.
Capture `waveform_music` speech/music scores and `Lookup id` lines. Once speech is
confirmed, no new lookup should begin; the next genuine song must resume normal
recognition. Repeat music-to-podcast and intermittent speech-over-music transitions.
Record actual lookup counts and recognition latency; do not report accuracy
percentages without a labelled, repeatable benchmark.
