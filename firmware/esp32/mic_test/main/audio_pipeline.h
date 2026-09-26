#pragma once

#include <array>
#include <cstdint>

namespace audio {

constexpr int kSampleRate = 48000;
constexpr int kFftSize = 2048;
constexpr int kBandCount = 24;
constexpr float kFrameSeconds = float(kFftSize) / kSampleRate;
constexpr int kCalibrationFrames = 71; // Just over three seconds of room silence.
using Bands = std::array<float, kBandCount>;

// All amplitudes are relative to PCM full scale, before display processing.
struct Config {
    bool gateOnCleanSpectrum = false; // Reference mic test retains its raw-RMS gate.
    float minimumOpenRms = 0.0002f;
    float minimumCloseRms = 0.00012f;
    float openNoiseRatio = 2.0f;
    float closeNoiseRatio = 1.4f;
    float closeHoldSeconds = 0.17f;
    float bandNoisePowerRatio = 1.5f;
    float minimumFlux = 0.002f; // Sum of positive band-RMS changes.
    float fluxThresholdDeviations = 2.5f;
    float fluxHistorySeconds = 0.75f;
    float beatRefractorySeconds = 0.18f;
    float displayFloorDb = -70.0f;
    float displayCeilingDb = -20.0f;
    float displayAttackSeconds = 0.04f;
    float displayReleaseSeconds = 0.15f;
};

struct Frame {
    float rms = 0.0f;
    float flux = 0.0f;
    float fluxThreshold = 0.0f;
    float openRms = 0.0f;
    float closeRms = 0.0f;
    float gateRms = 0.0f; // Actual gate input: raw or noise-subtracted RMS.
    bool calibrated = false;
    bool active = false;
    bool beat = false;
    Bands bandRms{}; // Noise-subtracted spectrum; never display-scaled.
    Bands levels{};
};

class Spectrum {
public:
    Spectrum();
    // Full stereo frame, 24-bit signed PCM in the upper 24 bits of each word.
    // Returns unwindowed, DC-removed RMS and writes a complex FFT input.
    float prepare(const int32_t* stereoWords, float* complexInput) const;
    Bands power(const float* complexOutput) const;
    int binCount(int band) const { return binCounts_[band]; }
    float lowerEdge(int band) const { return edges_[band]; }
    float upperEdge(int band) const { return edges_[band + 1]; }

private:
    std::array<float, kFftSize> window_{};
    std::array<float, kBandCount + 1> edges_{};
    std::array<int, kFftSize / 2> binBands_{};
    std::array<int, kBandCount> binCounts_{};
    float powerScale_ = 0.0f;
};

class Pipeline {
public:
    explicit Pipeline(Config config = {}) : config_(config) {}
    Frame process(float rms, const Bands& power);
    // Call after lost input/FFT frames. Keep calibration, but re-prime detection.
    void discontinuity();
    bool calibrated() const { return calibrationCount_ == kCalibrationFrames; }
    float noiseRms() const { return noiseRms_; }

private:
    void calibrate(float rms, const Bands& power);
    Config config_;
    std::array<std::array<float, kCalibrationFrames>, kBandCount + 1> noiseSamples_{};
    int calibrationCount_ = 0;
    float noiseRms_ = 0.0f;
    Bands noisePower_{};
    Bands previous_{};
    Bands levels_{};
    float fluxMean_ = 0.0f;
    float fluxDeviation_ = 0.0f;
    float quietSeconds_ = 0.0f;
    float sinceBeatSeconds_ = 1.0f;
    bool active_ = false;
    bool primed_ = false;
};

} // namespace audio
