#pragma once

#include <cstddef>
#include <cstdint>

enum class RecognitionStatus { Waiting, Capturing, Identifying, Matched, NoMatch, Unavailable };
struct RecognitionTrack {
    int64_t matched_at_us = 0;
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
using RecognitionCallback = void (*)(RecognitionStatus status, const RecognitionTrack *track,
                                    const RecognitionArtwork *artwork);
void recognition_start(RecognitionCallback callback);
void recognition_set_network(bool online, const char *address);
void recognition_set_controller(bool connected);

void recognition_set_activity(bool active);
