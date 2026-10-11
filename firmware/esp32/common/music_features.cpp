#include "music_features.h"
#include "music_feature_constants.h"
#include <algorithm>
#include <cmath>

namespace music_detection {
namespace {
void fft(FeatureWorkspace &w)
{
    for (unsigned i = 1, reversed = 0; i < 512; ++i) {
        unsigned bit = 256;
        for (; reversed & bit; bit >>= 1) reversed ^= bit;
        reversed ^= bit;
        if (i < reversed) std::swap(w.fft[i], w.fft[reversed]);
    }
    for (unsigned length = 2; length <= 512; length <<= 1) {
        for (unsigned start = 0; start < 512; start += length) {
            for (unsigned i = 0; i < length / 2; ++i) {
                const auto odd = w.fft[start + i + length / 2] * w.twiddles[i * 512 / length];
                const auto even = w.fft[start + i];
                w.fft[start + i] = even + odd; w.fft[start + i + length / 2] = even - odd;
            }
        }
    }
}
}
float input_gain(const uint8_t *pcm, size_t bytes) {
    if (!pcm || bytes != kSamples * 2) return 1;
    double power = 0;
    float peak = 0;
    for (size_t at = 0; at < bytes; at += 2) {
        const unsigned word = pcm[at] | (unsigned(pcm[at + 1]) << 8);
        const float value = (word >= 32768 ? int(word) - 65536 : int(word)) / 32768.f;
        power += double(value) * value;
        peak = std::max(peak, std::abs(value));
    }
    if (peak == 0) return 1;
    const float rms = float(std::sqrt(power / kSamples));
    return std::clamp(std::min(.08f / rms, .95f / peak), .01f, 256.f);
}
bool features(const uint8_t *pcm, size_t bytes, FeatureWorkspace &w,
              int8_t *output, size_t capacity, void (*yield)(), float gain)
{
    if (!pcm || bytes != kSamples * 2 || !output || capacity != kFrames * kBands ||
        !std::isfinite(gain) || gain < .01f || gain > 256.f) return false;
    constexpr double pi = 3.14159265358979323846;
    for (unsigned i = 0; i < 256; ++i) w.twiddles[i] = {
        float(std::cos(-2 * pi * i / 512)), float(std::sin(-2 * pi * i / 512))};
    for (unsigned frame = 0; frame < kFrames; ++frame) {
        for (unsigned i = 0; i < 512; ++i) {
            float sample = 0;
            if (i < 400) {
                const unsigned at = (frame * 160 + i) * 2;
                const unsigned word = pcm[at] | (unsigned(pcm[at + 1]) << 8);
                sample = (word >= 32768 ? int(word) - 65536 : int(word)) / 32768.0f;
                sample *= gain * constants::kHann[i];
            }
            w.fft[i] = {sample, 0};
        }
        fft(w);
        float mel[kBands];
        std::copy_n(constants::kMelBias, kBands, mel);
        for (unsigned bin = 0; bin < 257; ++bin) {
            const float magnitude = std::abs(w.fft[bin]);
            const auto &weight = constants::kMel[bin];
            mel[weight.first] += magnitude * weight.first_weight;
            mel[weight.second] += magnitude * weight.second_weight;
        }
        for (unsigned band = 0; band < kBands; ++band) {
            const float value = std::log(mel[band]) / constants::kInputScale;
            const int quantized = int(std::round(value)) + constants::kInputZero;
            output[frame * kBands + band] = int8_t(std::clamp(quantized, -128, 127));
        }
        if (yield && frame % 8 == 7) yield();
    }
    return true;
}
}
