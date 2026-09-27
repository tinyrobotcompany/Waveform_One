#pragma once
#include <cstddef>
#include <cstdint>
void capture_init();
bool capture_start(unsigned id);
void capture_abort(unsigned id);
void capture_audio(const int32_t *stereo, size_t frames);
void capture_discontinuity();
void capture_send();

bool capture_active();
