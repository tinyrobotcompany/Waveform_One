#pragma once
#include <complex>
#include <cstddef>
#include <cstdint>

namespace music_detection {
constexpr unsigned kSamples = 15600, kFrames = 96, kBands = 64;
struct FeatureWorkspace {
    std::complex<float> fft[512];
    std::complex<float> twiddles[256];
};
// Exact model input: 0.975 seconds of 16 kHz, mono, signed little-endian PCM.
// The caller owns all workspace and output; input audio is never modified.
bool features(const uint8_t *pcm, size_t bytes, FeatureWorkspace &workspace,
              int8_t *output, size_t capacity, void (*yield)() = nullptr, float gain = 1);
// Bounded level normalization for room microphones; zero audio stays zero.
// This is not an activity decision and never replaces the calibrated gate.
float input_gain(const uint8_t *pcm, size_t bytes);
}
