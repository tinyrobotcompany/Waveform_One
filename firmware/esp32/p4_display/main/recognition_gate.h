#pragma once
#include "music_detector.h"

namespace recognition_gate {
struct Decision { bool active, led, cautious; };
inline Decision decide(bool sound, MusicVerdict verdict, bool track_visible, bool fallback_due)
{
    if (!sound || verdict == MusicVerdict::Speech) return {false, false, false};
    const bool classified = verdict == MusicVerdict::Music || verdict == MusicVerdict::Unavailable || track_visible;
    return {classified || fallback_due, classified, fallback_due && !classified};
}
}
