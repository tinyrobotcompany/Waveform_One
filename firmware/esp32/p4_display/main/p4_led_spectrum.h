#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "audio_pipeline.h"
#include "p4_led_levels.h"

namespace p4_led {
// Independent 16 kHz/1024-point visualizer; no shared ESP-DSP global table and
// no changes to the recognition FFT or microphone. Allocate this in PSRAM.
class Spectrum {
public:
    static constexpr unsigned size = 1024;
    Spectrum() {
        for (unsigned i = 0; i < size; ++i)
            window_[i] = 0.5f - 0.5f * std::cos(6.28318530718f * i / (size - 1));
        for (unsigned i = 0; i < size / 2; ++i)
            twiddles_[i] = std::polar(1.0f, -6.28318530718f * i / size);
    }
    unsigned binCount(unsigned band) const {
        unsigned count = 0;
        for (unsigned bin = 1; bin < size / 2; ++bin)
            if (bandFor(bin) == int(band)) ++count;
        return count;
    }
    void reset() { levels_.reset(); }
    bool calibrated() const { return levels_.ready(); }
    bool loadNoise(const Levels::Noise &noise) { return levels_.load(noise); }
    const Levels::Noise &noise() const { return levels_.noise(); }
    audio::Frame process(const int16_t *pcm, float elapsed = float(size) / 16000) {
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
        for (unsigned bin = 1; bin < size / 2; ++bin) {
            const int band = bandFor(bin);
            if (band < 0) continue;
            out.bandRms[band] += std::norm(data_[bin]) * 2 / (size * window_power);
        }
        for (unsigned b = 0; b < 24; ++b) {
            out.bandRms[b] = std::sqrt(out.bandRms[b]);
        }
        levels_.apply(out, elapsed);
        return out;
    }
private:
    static int bandFor(unsigned bin) {
        const float hz = bin * 16000.0f / size;
        if (hz < 60 || hz >= 7500) return -1;
        return std::min(23, int(24 * std::log(hz / 60) / std::log(7500.0f / 60)));
    }
    Levels levels_{};
    std::array<float, size> window_{};
    std::array<std::complex<float>, size> data_{};
    std::array<std::complex<float>, size / 2> twiddles_{};
};
}
