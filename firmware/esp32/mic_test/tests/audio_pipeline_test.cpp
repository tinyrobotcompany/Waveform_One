#include "audio_pipeline.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <vector>

using namespace audio;
constexpr float pi = 3.14159265358979323846f;

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

// Independent host reference transform. Firmware uses ESP-DSP instead.
void fft(std::vector<float>& data) {
    std::vector<std::complex<double>> x(kFftSize);
    for (int i = 0; i < kFftSize; ++i) x[i] = {data[2 * i], data[2 * i + 1]};
    for (int i = 1, j = 0; i < kFftSize; ++i) {
        int bit = kFftSize >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }
    for (int length = 2; length <= kFftSize; length <<= 1) {
        const auto step = std::polar(1.0, -2.0 * double(pi) / length);
        for (int base = 0; base < kFftSize; base += length) {
            std::complex<double> w = 1.0;
            for (int j = 0; j < length / 2; ++j) {
                const auto even = x[base + j];
                const auto odd = w * x[base + j + length / 2];
                x[base + j] = even + odd;
                x[base + j + length / 2] = even - odd;
                w *= step;
            }
        }
    }
    for (int i = 0; i < kFftSize; ++i) {
        data[2 * i] = float(x[i].real());
        data[2 * i + 1] = float(x[i].imag());
    }
}

struct Measurement { float rms; Bands power; };
Measurement tone(float amplitude, float bin = 43.0f, float dc = 0.0f) {
    Spectrum spectrum;
    std::vector<int32_t> words(kFftSize * 2);
    std::vector<float> data(kFftSize * 2);
    for (int i = 0; i < kFftSize; ++i) {
        const double sample = dc + amplitude * std::sin(2.0 * double(pi) * bin * i / kFftSize);
        words[2 * i] = int32_t(std::llround(sample * 8388608.0) * 256);
        words[2 * i + 1] = 0x12345600; // Unused right slot must not affect measurements.
    }
    const float rms = spectrum.prepare(words.data(), data.data());
    fft(data);
    return {rms, spectrum.power(data.data())};
}

void calibrate(Pipeline& pipeline, float rms = 0.0f, Bands power = {}) {
    for (int i = 0; i < kCalibrationFrames; ++i) {
        const auto f = pipeline.process(rms, power);
        require(!f.active && !f.beat, "calibration must stay dark and suppress beats");
        require(std::all_of(f.levels.begin(), f.levels.end(), [](float x) { return x == 0; }),
                "calibration must output zero display levels");
    }
    require(pipeline.calibrated(), "calibration must complete");
}

void testSpectrum() {
    Spectrum spectrum;
    for (int b = 0; b < kBandCount; ++b) require(spectrum.binCount(b) > 0, "no empty bass bands");
    const auto silence = tone(0.0f, 43.0f, 0.25f);
    require(silence.rms == 0.0f, "DC offset must not count as audio RMS");
    require(std::accumulate(silence.power.begin(), silence.power.end(), 0.0f) == 0.0f,
            "DC-only input must have zero spectral power");
    const auto sine = tone(0.02f, 43.0f, -0.1f);
    require(std::abs(sine.rms - 0.02f / std::sqrt(2.0f)) < 1e-6f, "signed PCM RMS scale");
    const float power = std::accumulate(sine.power.begin(), sine.power.end(), 0.0f);
    require(std::abs(power / (sine.rms * sine.rms) - 1.0f) < 0.001f,
            "Hann-normalized band power must match in-range tone mean square");
    const auto quieter = tone(0.002f);
    const float quietPower = std::accumulate(quieter.power.begin(), quieter.power.end(), 0.0f);
    require(std::abs(quietPower / power - 0.01f) < 0.00001f, "10x amplitude must mean 100x power");
    const auto peak = std::max_element(sine.power.begin(), sine.power.end()) - sine.power.begin();
    const float frequency = 43.0f * kSampleRate / kFftSize;
    require(spectrum.lowerEdge(int(peak)) <= frequency && spectrum.upperEdge(int(peak)) > frequency,
            "tone must map to the correct frequency band");
}

void testSilenceAndAmbient() {
    Pipeline zero;
    calibrate(zero);
    for (int i = 0; i < 1000; ++i) {
        const auto f = zero.process(0.0f, {});
        require(!f.active && !f.beat && f.flux == 0.0f, "long silence must never create activity");
    }
    Pipeline ambient;
    Bands noise;
    noise.fill(0.006f * 0.006f / kBandCount);
    calibrate(ambient, 0.006f, noise);
    for (int i = 0; i < 300; ++i) {
        const float rms = i % 2 ? 0.0059f : 0.007f;
        Bands power;
        power.fill(rms * rms / kBandCount);
        const auto f = ambient.process(rms, power);
        require(!f.active && !f.beat, "ambient fluctuations around old 0.006 gate must stay quiet");
        require(std::all_of(f.levels.begin(), f.levels.end(), [](float x) { return x == 0; }),
                "ambient noise must not be normalized into visible bars");
    }
}

void testToneAndDecay() {
    Pipeline p;
    calibrate(p);
    const auto sine = tone(0.02f);
    auto f = p.process(sine.rms, sine.power);
    require(f.active && f.beat, "clear tone onset must be detected");
    for (int i = 0; i < 100; ++i) {
        f = p.process(sine.rms, sine.power);
        require(f.flux == 0.0f && !f.beat, "steady tone must not create repeated flux or beats");
    }
    Bands previous = f.levels;
    for (int i = 0; i < 100; ++i) {
        f = p.process(0.0f, {});
        require(!f.beat && f.flux == 0.0f, "music stopping must not create a beat");
        for (int b = 0; b < kBandCount; ++b) {
            require(f.levels[b] <= previous[b], "silence decay must never rebound");
        }
        previous = f.levels;
    }
    require(!f.active, "silence must close the gate");
    require(std::all_of(f.levels.begin(), f.levels.end(), [](float x) { return x == 0; }),
            "silence must settle to exact zero");
    const auto quiet = tone(0.002f);
    f = p.process(quiet.rms, quiet.power);
    require(f.active, "quiet tone clearly above calibrated noise must remain visible");
    for (int i = 0; i < 100; ++i) f = p.process(quiet.rms, quiet.power);
    const float quietPeak = *std::max_element(f.levels.begin(), f.levels.end());
    require(quietPeak > 0.0f && quietPeak < 0.5f, "quiet tone must not grow toward full scale");
    f = p.process(sine.rms, sine.power);
    require(f.beat, "a real amplitude increase must trigger after a steady passage");
}

void testDisplayIndependence() {
    Config alternate;
    alternate.displayAttackSeconds = 0.8f;
    alternate.displayReleaseSeconds = 1.5f;
    alternate.displayFloorDb = -90.0f;
    alternate.displayCeilingDb = -35.0f;
    Pipeline a, b(alternate);
    calibrate(a); calibrate(b);
    bool displayDiffers = false;
    for (int i = 0; i < 500; ++i) {
        Bands power{};
        const float rms = (i % 80 < 20) ? 0.02f : (i % 80 < 40) ? 0.001f : 0.0f;
        power[10] = rms * rms;
        const auto x = a.process(rms, power), y = b.process(rms, power);
        require(x.flux == y.flux && x.fluxThreshold == y.fluxThreshold && x.beat == y.beat,
                "display parameters must have zero influence on detection");
        displayDiffers |= x.levels != y.levels;
    }
    require(displayDiffers, "independence test must actually change the display");
}

void testGateHistoryAndRecovery() {
    Config config;
    config.minimumOpenRms = 0.006f;
    config.minimumCloseRms = 0.004f;
    Pipeline p(config);
    calibrate(p);
    Bands fixed{};
    fixed[10] = 0.001f * 0.001f;
    // Additional out-of-range audio can change total RMS while these bands
    // stay fixed. Gate transitions alone must never become spectral onsets.
    p.process(0.007f, fixed);
    for (int i = 0; i < 30; ++i) {
        const auto f = p.process(0.005f, fixed);
        require(f.active && f.flux == 0.0f, "hysteresis must hold the gate between thresholds");
    }
    for (int i = 0; i < 30; ++i) p.process(0.003f, fixed);
    const auto reopen = p.process(0.007f, fixed);
    require(reopen.active && reopen.flux == 0.0f && !reopen.beat,
            "gate reopening must not manufacture flux from unchanged bands");
    p.discontinuity();
    fixed[10] = 0.02f * 0.02f;
    const auto resume = p.process(0.02f, fixed);
    require(resume.flux == 0.0f && !resume.beat, "lost frames must re-prime detection without a false beat");
}

void testCalibrationOutliersAndRefractory() {
    Pipeline p;
    Bands power{};
    for (int i = 0; i < kCalibrationFrames; ++i) {
        power[10] = i < 5 ? 0.01f : 1e-8f;
        p.process(std::sqrt(power[10]), power);
    }
    require(std::abs(p.noiseRms() - 0.0001f) < 1e-8f, "few calibration transients must not set the floor");
    power[10] = 0.01f;
    require(p.process(0.1f, power).beat, "first clear onset after calibration");
    p.process(0.0f, {});
    require(!p.process(0.1f, power).beat, "refractory interval must suppress rapid repeated beats");
}

int main() {
    testSpectrum();
    testSilenceAndAmbient();
    testToneAndDecay();
    testDisplayIndependence();
    testGateHistoryAndRecovery();
    testCalibrationOutliersAndRefractory();
    std::puts("PASS: spectrum normalization/mapping, silence/noise, onsets/decay, display independence, gate history, recovery, calibration and refractory timing");
}
