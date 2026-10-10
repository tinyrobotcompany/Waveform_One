#pragma once
#include "recognition.h"

namespace recognition_feedback {
inline bool searching(RecognitionStatus status) {
    return status == RecognitionStatus::Capturing || status == RecognitionStatus::Identifying;
}
inline const char *text(RecognitionStatus status, bool has_track) {
    if (searching(status)) return has_track ? "Checking what’s playing…" : "Finding your song…";
    if (status == RecognitionStatus::Unavailable) return "Trying again shortly";
    return has_track ? "Enjoy the music" : "Play a song to get started";
}
} // namespace recognition_feedback
