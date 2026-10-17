#pragma once

#include <cstddef>
#include <cstdint>

enum class RecognitionStatus { Waiting, Capturing, Identifying, Matched, NoMatch, Unavailable };
struct RecognitionTrack {
    int64_t matched_at_us = 0;
    double offset_s = -1;   // song position at the clip's start; negative if unknown
    double sample_begin_ms = -1; // matched region in the submitted clip; unknown when absent
    double sample_end_ms = -1;
    double match_score = -1;
    double duration_s = 0;  // song length from the catalogue; zero if unknown
    char title[513]{};
    char artist[513]{};
    char album[513]{};
    char artwork_url[2049]{};
};
struct RecognitionArtwork {
    const uint8_t *pixels = nullptr;
    unsigned width = 0;
    unsigned height = 0;
};
// Called from a background task. Strings and RGB565 pixels are borrowed for the
// duration of the callback. UI must copy pixels if retaining the artwork.
// Return false if the display could not apply the update; clears are retried.
using RecognitionCallback = bool (*)(RecognitionStatus status, const RecognitionTrack *track,
                                    const RecognitionArtwork *artwork);
void recognition_start(RecognitionCallback callback);
void recognition_set_network(bool online, const char *address);
void recognition_set_controller(bool connected);
// The room's level detector currently hears sound (music, or anything as
// loud). Safe to call from any task; false until recognition has started.
bool recognition_sound_heard();

