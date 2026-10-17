#pragma once
#include "audio_pipeline.h"
#include "visualizer_config.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace p4_led {
// LED-only room reference. Never modifies microphone PCM or recognition state.
class Levels {
public:
    using Noise = audio::Bands;
    static constexpr size_t max_samples = 160;
    static constexpr float quiet_seconds = 3.0f;
    static bool valid(const Noise &noise) {
        for (float power : noise)
            if (!std::isfinite(power) || power < 1e-12f || power > 1.0f) return false;
        return true;
    }
    bool load(const Noise &noise) {
        if (!valid(noise)) return false;
        noise_ = noise; ready_ = true; reset(); return true;
    }
    bool ready() const { return ready_; }
    const Noise &noise() const { return noise_; }
    void reset() {
        levels_.fill(0);
        // A capture discontinuity must not join disconnected quiet samples.
        if (!ready_) { count_ = 0; elapsed_ = 0; }
    }
    void apply(audio::Frame &frame, float seconds) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 0.1f) : 0.0f;
        frame.levels = {};
        if (!ready_) {
            frame.calibrated = false;
            if (dt == 0) return;
            for (unsigned b = 0; b < audio::kBandCount; ++b)
                samples_[b][count_] = frame.bandRms[b] * frame.bandRms[b];
            ++count_; elapsed_ += dt;
            if ((elapsed_ < quiet_seconds || count_ < 64) && count_ < max_samples) return;
            for (unsigned b = 0; b < audio::kBandCount; ++b) {
                // Same robust percentile used in the S3 room calibration.
                const size_t index = (count_ - 1) * 4 / 5;
                auto &samples = samples_[b];
                std::nth_element(samples.begin(), samples.begin()+index, samples.begin()+count_);
                noise_[b] = std::max(samples[index], 1e-12f);
            }
            ready_ = valid(noise_);
            if (!ready_) { count_ = 0; elapsed_ = 0; }
            return; // Keep calibration's final frame dark too.
        }
        frame.calibrated = true;
        const auto config = visualizerConfig();
        for (unsigned b = 0; b < audio::kBandCount; ++b) {
            const float power = frame.bandRms[b] * frame.bandRms[b];
            const float above_noise = 10 * std::log10(std::max(power, 1e-18f) / noise_[b]);
            // A provisional visual range, not a music classifier: below 3 dB
            // stays dark, 24 dB above the room reference reaches full height.
            const float target = std::clamp((above_noise - 3) / 21, 0.0f, 1.0f);
            const float tau = target > levels_[b] ? config.displayAttackSeconds : config.displayReleaseSeconds;
            levels_[b] += (1 - std::exp(-dt / tau)) * (target - levels_[b]);
            if (levels_[b] < 0.001f) levels_[b] = 0;
        }
        frame.levels = levels_;
    }
private:
    std::array<std::array<float, max_samples>, audio::kBandCount> samples_{};
    Noise noise_{}, levels_{};
    size_t count_ = 0;
    float elapsed_ = 0;
    bool ready_ = false;
};
}
