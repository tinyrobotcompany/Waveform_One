#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace microphone_pcm {
struct Levels {
    uint64_t count = 0, rails = 0;
    int64_t sum = 0;
    uint64_t squares = 0;
    void add(int16_t sample) {
        ++count;
        sum += sample;
        squares += int64_t(sample) * sample;
        rails += sample == INT16_MIN || sample == INT16_MAX;
    }
    double dc() const { return count ? double(sum) / count : 0; }
    double dbfs() const {
        const double power = count ? std::max(0.0, double(squares) / count - dc() * dc()) : 0;
        return 10 * std::log10(power / (32768.0 * 32768.0) + 1e-12);
    }
    double clipped_percent() const { return count ? 100.0 * rails / count : 0; }
};
// Keep one microphone intact: averaging uncalibrated microphones can cancel
// frequencies. The ADC's interleaved stereo rate is frames/second, not samples.
inline bool select(const int16_t *stereo, size_t frames, unsigned channel, int16_t *mono) {
    if (channel > 1 || !stereo || !mono) return false;
    for (size_t i = 0; i < frames; ++i) mono[i] = stereo[2 * i + channel];
    return true;
}
}
