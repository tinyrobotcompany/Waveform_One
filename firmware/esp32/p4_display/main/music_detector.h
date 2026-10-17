#pragma once

// YAMNet music classification of the onboard microphone, run only while the
// level detector hears sound. Safe to call from any task.
enum class MusicVerdict {
    Unavailable, // no model or no memory: callers fall back to the level detector
    Undecided,   // listening, no decision yet (or silence)
    Music,
    NotMusic,    // sound with no music-like window for 10 s: talk, appliances
};
void music_detector_start();
MusicVerdict music_detector_verdict();
