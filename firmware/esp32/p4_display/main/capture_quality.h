#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace capture_quality {
// Aggregate diagnostics only: no allocation, retained PCM or changed samples.
struct Stats {
    bool valid = false;
    size_t samples = 0;
    unsigned peak = 0;
    size_t clipped = 0;
    double rms = 0, dc = 0;
};
inline Stats inspect(const uint8_t *pcm, size_t bytes) {
    if (!pcm || bytes == 0 || bytes % 2 != 0) return {};
    Stats result{};
    result.samples = bytes / 2;
    // Four-second clips fit comfortably in these 64-bit accumulators.
    if (result.samples > 128000) return {};
    uint64_t squares = 0;
    int64_t sum = 0;
    for (size_t i = 0; i < bytes; i += 2) {
        const unsigned word = pcm[i] | (unsigned(pcm[i + 1]) << 8);
        const int sample = word >= 32768 ? int(word) - 65536 : int(word);
        const unsigned magnitude = sample < 0 ? unsigned(-sample) : unsigned(sample);
        if (magnitude > result.peak) result.peak = magnitude;
        if (sample == -32768 || sample == 32767) ++result.clipped;
        squares += uint64_t(magnitude) * magnitude;
        sum += sample;
    }
    result.rms = std::sqrt(double(squares) / result.samples);
    result.dc = double(sum) / result.samples;
    result.valid = true;
    return result;
}
}
