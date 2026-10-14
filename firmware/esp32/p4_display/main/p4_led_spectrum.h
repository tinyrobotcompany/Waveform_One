#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "audio_pipeline.h"

namespace p4_led {
// Independent 16 kHz/512-point visualizer; no shared ESP-DSP global table and
// no changes to the recognition FFT or microphone. Allocate this in PSRAM.
class Spectrum {
public:
    static constexpr unsigned size = 512;
    Spectrum() {
        for (unsigned i = 0; i < size; ++i)
            window_[i] = 0.5f - 0.5f * std::cos(6.28318530718f * i / (size - 1));
        for (unsigned i = 0; i < size / 2; ++i)
            twiddles_[i] = std::polar(1.0f, -6.28318530718f * i / size);
    }
    audio::Frame process(const int16_t *pcm) {
        float mean = 0, window_power = 0;
        for (unsigned i = 0; i < size; ++i) mean += pcm[i] / 32768.0f;
        mean /= size;
        audio::Frame out{};
        out.calibrated = true; // no room calibration/filter in this experiment
        for (unsigned i = 0; i < size; ++i) {
            const float sample = pcm[i] / 32768.0f - mean;
            out.rms += sample * sample;
            data_[i] = sample * window_[i]; window_power += window_[i] * window_[i];
        }
        out.rms = std::sqrt(out.rms / size);
        for (unsigned i = 1, reversed = 0; i < size; ++i) {
            unsigned bit = size / 2;
            for (; reversed & bit; bit >>= 1) reversed ^= bit;
            reversed ^= bit;
            if (i < reversed) std::swap(data_[i], data_[reversed]);
        }
        for (unsigned length = 2; length <= size; length <<= 1)
            for (unsigned start = 0; start < size; start += length)
                for (unsigned i = 0; i < length / 2; ++i) {
                    const auto odd = data_[start+i+length/2] * twiddles_[i*size/length];
                    const auto even = data_[start+i];
                    data_[start+i] = even + odd; data_[start+i+length/2] = even - odd;
                }
        for (unsigned bin = 2; bin < size / 2; ++bin) {
            const float hz = bin * 16000.0f / size;
            if (hz >= 7500) continue;
            const int band = std::min(23, int(24 * std::log(hz / 60) / std::log(7500.0f / 60)));
            out.bandRms[band] += std::norm(data_[bin]) * 2 / (size * window_power);
        }
        for (unsigned b = 0; b < 24; ++b) {
            out.bandRms[b] = std::sqrt(out.bandRms[b]);
            const float db = 20 * std::log10(std::max(out.bandRms[b], 1e-9f));
            // Diagnostic visual range only; does not admit/reject recognition.
            out.levels[b] = std::clamp((db + 80) / 45, 0.0f, 1.0f);
        }
        return out;
    }
private:
    std::array<float, size> window_{};
    std::array<std::complex<float>, size> data_{};
    std::array<std::complex<float>, size / 2> twiddles_{};
};
}
