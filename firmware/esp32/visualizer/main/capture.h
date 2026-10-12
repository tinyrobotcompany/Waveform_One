#pragma once
#include <cstddef>
#include <cstdint>
void capture_init();
bool capture_start(unsigned id, unsigned seconds = 8);
// Continuous PCM with an unbounded sequence and no END. A new STREAM request
// replaces an existing stream; a bounded capture still makes it BUSY.
bool capture_start_stream(unsigned id);
void capture_stop_stream();
void capture_abort(unsigned id);
void capture_audio(const int32_t *stereo, size_t frames);
void capture_discontinuity();
void capture_send();

// A bounded capture is in progress. Streams never block firmware updates.
bool capture_active();
