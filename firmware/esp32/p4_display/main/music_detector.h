#pragma once

// YAMNet music classification of the onboard microphone, run only while the
// level detector hears sound. Safe to call from any task.
enum class MusicVerdict {
    Unavailable, // no model or no memory: callers fall back to the level detector
    Undecided,   // silence, or sound not (yet) shown to be music: talk, appliances
    Music,
};
void music_detector_start();
MusicVerdict music_detector_verdict();
