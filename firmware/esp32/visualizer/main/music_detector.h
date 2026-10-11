#pragma once
#include <cstddef>
#include <cstdint>
#include "music_policy.h"
bool music_detector_start();
bool music_detector_ready();
void music_detector_audio(const int32_t *stereo, size_t frames);
void music_detector_discontinuity();
// Energy is used only for prompt silence detection, never as music evidence.
music_detection::State music_detector_state(bool audible, int64_t now);
