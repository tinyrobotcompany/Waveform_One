#include "audio_pipeline.h"

#include <algorithm>
#include <cmath>

namespace audio {
namespace {
constexpr float kPi = 3.14159265358979323846f;
float coefficient(float seconds) {
    return 1.0f - std::exp(-kFrameSeconds / std::max(seconds, 0.0001f));
}
}

Spectrum::Spectrum() {
    float windowPower = 0.0f;
    for (int i = 0; i < kFftSize; ++i) {
        window_[i] = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (kFftSize - 1));
        windowPower += window_[i] * window_[i];
    }
    // One-sided power: sum across bins estimates time-domain mean square.
    // DC and Nyquist are excluded by the visualizer's 60 Hz–16 kHz range.
    powerScale_ = 2.0f / (kFftSize * windowPower);
    for (int b = 0; b <= kBandCount; ++b) {
        edges_[b] = 60.0f * std::pow(16000.0f / 60.0f, float(b) / kBandCount);
    }
    edges_[kBandCount] = 16000.0f;
    binBands_.fill(-1);
    for (int bin = 1; bin < kFftSize / 2; ++bin) {
        const float hz = float(bin * kSampleRate) / kFftSize;
        for (int b = 0; b < kBandCount; ++b) {
            if (hz >= edges_[b] && hz < edges_[b + 1]) {
                binBands_[bin] = b;
                ++binCounts_[b];
                break;
            }
        }
    }
}

float Spectrum::prepare(const int32_t* stereoWords, float* complexInput) const {
    double mean = 0.0;
    for (int i = 0; i < kFftSize; ++i) {
        const float sample = float(stereoWords[2 * i] >> 8) / 8388608.0f;
        complexInput[2 * i] = sample;
        complexInput[2 * i + 1] = 0.0f;
        mean += sample;
    }
    mean /= kFftSize;
    double sumSquares = 0.0;
    for (int i = 0; i < kFftSize; ++i) {
        const float centered = complexInput[2 * i] - float(mean);
        sumSquares += double(centered) * centered;
        complexInput[2 * i] = centered * window_[i];
    }
    return std::sqrt(sumSquares / kFftSize);
}

Bands Spectrum::power(const float* complexOutput) const {
    Bands result{};
    for (int bin = 1; bin < kFftSize / 2; ++bin) {
        const int band = binBands_[bin];
        if (band < 0) continue;
        const float real = complexOutput[2 * bin];
        const float imag = complexOutput[2 * bin + 1];
        result[band] += (real * real + imag * imag) * powerScale_;
    }
    return result;
}

void Pipeline::calibrate(float rms, const Bands& power) {
    noiseSamples_[0][calibrationCount_] = rms * rms;
    for (int b = 0; b < kBandCount; ++b) {
        noiseSamples_[b + 1][calibrationCount_] = power[b];
    }
    if (++calibrationCount_ != kCalibrationFrames) return;

    // 80th percentile tolerates occasional transients without using the
    // quietest frame as the floor. Never adapt this reference to playing music.
    constexpr int percentile = (kCalibrationFrames - 1) * 4 / 5;
    for (auto& samples : noiseSamples_) {
        std::nth_element(samples.begin(), samples.begin() + percentile, samples.end());
    }
    noiseRms_ = std::sqrt(noiseSamples_[0][percentile]);
    for (int b = 0; b < kBandCount; ++b) {
        noisePower_[b] = noiseSamples_[b + 1][percentile];
    }
}

void Pipeline::discontinuity() {
    primed_ = false;
    active_ = false;
    quietSeconds_ = 0.0f;
    fluxMean_ = fluxDeviation_ = 0.0f;
    previous_.fill(0.0f);
    levels_.fill(0.0f);
}

Frame Pipeline::process(float rms, const Bands& power) {
    Frame frame;
    frame.rms = rms;
    if (!calibrated()) {
        calibrate(rms, power);
        // The final calibration frame primes detection, but remains dark.
        if (!calibrated()) return frame;
        for (int b = 0; b < kBandCount; ++b) {
            previous_[b] = std::sqrt(std::max(0.0f,
                power[b] - config_.bandNoisePowerRatio * noisePower_[b]));
        }
        primed_ = true;
        frame.calibrated = true;
        frame.openRms = std::max(config_.minimumOpenRms, noiseRms_ * config_.openNoiseRatio);
        frame.closeRms = std::max(config_.minimumCloseRms, noiseRms_ * config_.closeNoiseRatio);
        return frame;
    }

    frame.calibrated = true;
    frame.openRms = std::max(config_.minimumOpenRms, noiseRms_ * config_.openNoiseRatio);
    frame.closeRms = std::max(config_.minimumCloseRms, noiseRms_ * config_.closeNoiseRatio);

    // Detection is continuous, including while the display gate is closed.
    // Neither the activity gate nor animation ever modifies its history.
    float cleanPower = 0.0f;
    for (int b = 0; b < kBandCount; ++b) {
        frame.bandRms[b] = std::sqrt(std::max(0.0f,
            power[b] - config_.bandNoisePowerRatio * noisePower_[b]));
        cleanPower += frame.bandRms[b] * frame.bandRms[b];
        if (primed_) {
            frame.flux += std::max(0.0f, frame.bandRms[b] - previous_[b])
                * (b < 8 ? 1.5f : 1.0f);
        }
        previous_[b] = frame.bandRms[b];
    }
    primed_ = true;

    frame.gateRms = config_.gateOnCleanSpectrum ? std::sqrt(cleanPower) : rms;
    if (!active_ && frame.gateRms >= frame.openRms && cleanPower > 0.0f) {
        active_ = true;
        quietSeconds_ = 0.0f;
    }
    if (active_) {
        quietSeconds_ = frame.gateRms < frame.closeRms || cleanPower == 0.0f
            ? quietSeconds_ + kFrameSeconds : 0.0f;
        if (quietSeconds_ >= config_.closeHoldSeconds) active_ = false;
    }
    frame.active = active_;

    // Compare with the preceding baseline, then update it with this frame.
    frame.fluxThreshold = std::max(config_.minimumFlux,
        fluxMean_ + config_.fluxThresholdDeviations * fluxDeviation_);
    sinceBeatSeconds_ += kFrameSeconds;
    frame.beat = active_ && frame.gateRms >= frame.openRms
        && sinceBeatSeconds_ >= config_.beatRefractorySeconds
        && frame.flux > frame.fluxThreshold;
    if (frame.beat) sinceBeatSeconds_ = 0.0f;
    const float historyAlpha = coefficient(config_.fluxHistorySeconds);
    const float deviation = std::abs(frame.flux - fluxMean_);
    fluxMean_ += historyAlpha * (frame.flux - fluxMean_);
    fluxDeviation_ += historyAlpha * (deviation - fluxDeviation_);

    for (int b = 0; b < kBandCount; ++b) {
        float target = 0.0f;
        if (active_ && frame.bandRms[b] > 0.0f) {
            const float db = 20.0f * std::log10(frame.bandRms[b]);
            target = std::clamp((db - config_.displayFloorDb)
                / (config_.displayCeilingDb - config_.displayFloorDb), 0.0f, 1.0f);
        }
        const float alpha = coefficient(target > levels_[b]
            ? config_.displayAttackSeconds : config_.displayReleaseSeconds);
        levels_[b] += alpha * (target - levels_[b]);
        if (levels_[b] < 0.001f) levels_[b] = 0.0f;
    }
    frame.levels = levels_;
    return frame;
}

} // namespace audio
