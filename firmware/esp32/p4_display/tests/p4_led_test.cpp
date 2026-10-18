#include "p4_hub75.h"
#include "p4_led_spectrum.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <set>

int main() {
    using namespace p4_hub75;
    assert(std::set<int>(pins.begin(), pins.end()).size() == 13);
    for (int pin : pins) {
        // Protect I2C, onboard audio, C6 SDIO/reset, LCD and debug signals.
        assert(!(pin >= 6 && pin <= 27));
        assert(pin != 32 && pin != 33 && pin != 35 && pin != 36 && pin != 37 && pin != 38);
        assert(pin < 53);
    }
    Frame frame{};
    encode(frame, [](unsigned x, unsigned row) { return (x+row) & 63; });
    for (unsigned row = 0; row < rows; ++row) {
        unsigned pulses = 0, lit = 0;
        for (unsigned x = 0; x < clocks; ++x) {
            const auto word = frame[row*clocks+x];
            assert((word >> 8) == row);
            if (word & latch) { ++pulses; assert(word & blank); assert(x == 63); }
            if (!(word & blank)) { ++lit; assert(x >= 66 && x < 98); assert(!(word & latch)); }
            if (x < 64) { assert(word & blank); assert((word & 63) == ((x+row)&63)); }
        }
        assert(pulses == 1 && lit == 32);
        assert(frame[row*clocks] & blank);
        assert(frame[(row+1)*clocks-1] & blank);
    }
    // Initial loop start emits NO callback in IDF 6.1. Its buffer must
    // remain protected while we prepare and await the first replacement.
    Buffers startup;
    assert(!startup.started(-1) && !startup.started(2));
    assert(startup.started(0));
    assert(startup.writable() == 1 && !startup.submit(0));
    assert(!startup.started(1) && !startup.switched(0));
    assert(startup.submit(1));
    assert(startup.writable() == -1);
    assert(!startup.started(0) && !startup.switched(0));
    assert(startup.switched(1) && startup.writable() == 0);
    Buffers pending;
    assert(pending.submit(0) && !pending.started(1));
    Buffers buffers;
    assert(buffers.writable() == 0);
    assert(!buffers.submit(1));
    assert(buffers.submit(0));
    assert(buffers.writable() == -1 && !buffers.submit(0));
    assert(!buffers.switched(1) && buffers.writable() == -1);
    assert(buffers.switched(0) && buffers.writable() == 1);
    assert(!buffers.switched(0));
    for (int i = 0; i < 100; ++i) {
        const auto next = buffers.writable();
        assert(buffers.submit(next));
        assert(buffers.writable() == -1);
        assert(buffers.switched(next));
        assert(buffers.writable() == 1-next);
    }
    auto spectrum = std::make_unique<p4_led::Spectrum>();
    constexpr unsigned fft_size = p4_led::Spectrum::size;
    for (unsigned b = 0; b < 24; ++b) assert(spectrum->binCount(b) > 0);
    int16_t pcm[fft_size]{};
    auto out = spectrum->process(pcm);
    assert(out.rms == 0);
    for (float level : out.levels) assert(level == 0);
    for (auto &sample : pcm) sample = 1234;
    out = spectrum->process(pcm);
    assert(out.rms < 1e-6f);
    for (float level : out.levels) assert(level == 0);
    // A 1 kHz tone must retain its physical frequency and RMS at 16 kHz.
    for (unsigned i = 0; i < fft_size; ++i) pcm[i] = int16_t(3276 * std::sin(6.28318530718 * 1000 * i / 16000));
    out = spectrum->process(pcm);
    float power = 0;
    for (float rms : out.bandRms) power += rms*rms;
    assert(std::abs(out.rms - 0.1/std::sqrt(2.0)) < 0.0001);
    assert(std::abs(power - 0.005) < 0.00001);
    const auto strongest = std::max_element(out.bandRms.begin(), out.bandRms.end()) - out.bandRms.begin();
    assert(strongest == int(24 * std::log(1000.0/60) / std::log(7500.0/60)));
    // The former 512-point mapping left the second bass band permanently empty.
    for (unsigned i = 0; i < fft_size; ++i)
        pcm[i] = int16_t(3276 * std::sin(6.28318530718 * 78.125 * i / 16000));
    p4_led::Levels::Noise reference{};
    reference.fill(1e-8f);
    assert(spectrum->loadNoise(reference));
    spectrum->reset();
    out = spectrum->process(pcm, 0.02f);
    assert(out.bandRms[1] > 0.04f && out.levels[1] > 0);
    const float first = out.levels[1];
    out = spectrum->process(pcm, 0.02f);
    assert(out.levels[1] > first && out.levels[1] < 1);
    // Real elapsed time gives the same envelope at different update rates.
    spectrum->reset();
    for (int i = 0; i < 5; ++i) out = spectrum->process(pcm, 0.02f);
    const float at100ms = out.levels[1];
    spectrum->reset();
    for (int i = 0; i < 10; ++i) out = spectrum->process(pcm, 0.01f);
    assert(std::abs(out.levels[1] - at100ms) < 0.00001f);
    std::fill(std::begin(pcm), std::end(pcm), 0);
    out = spectrum->process(pcm, 0.02f);
    assert(out.levels[1] > 0 && out.levels[1] < at100ms);
    assert(std::abs(out.levels[1] - at100ms * std::exp(-0.02f / visualizerConfig().displayReleaseSeconds)) < 0.00001f);
    spectrum->reset();
    out = spectrum->process(pcm);
    for (float level : out.levels) assert(level == 0);
    auto levels = std::make_unique<p4_led::Levels>();
    audio::Frame quiet{};
    for (auto &rms : quiet.bandRms) rms = 0.0001f;
    for (int i = 0; i < 160 && !levels->ready(); ++i) {
        quiet.calibrated = true;
        levels->apply(quiet, 0.025f);
        assert(!quiet.calibrated);
        for (float value : quiet.levels) assert(value == 0);
    }
    assert(levels->ready());
    assert(std::abs(levels->noise()[0] - 1e-8f) < 1e-12f);
    levels->apply(quiet, 0.025f);
    assert(quiet.calibrated);
    for (float value : quiet.levels) assert(value == 0);
    auto invalid = levels->noise(); invalid[3] = NAN;
    assert(!levels->load(invalid));
    invalid[3] = 0; assert(!levels->load(invalid));
    // Same SNR across different absolute microphone band levels must result
    // in equal display height. This addresses the measured missing colours.
    auto reference2 = levels->noise();
    audio::Frame music{};
    for (unsigned b = 0; b < audio::kBandCount; ++b) {
        reference2[b] = std::pow(10.0f, (-80.0f - b * 0.5f) / 10);
        music.bandRms[b] = std::sqrt(reference2[b] * std::pow(10.0f, 13.0f / 10));
    }
    assert(levels->load(reference2));
    for (int i = 0; i < 100; ++i) levels->apply(music, 0.02f);
    for (float value : music.levels) assert(std::abs(value - 10.0f / 21) < 0.0001f);
    // Never feed display normalization back into the spectral measurements.
    for (unsigned b = 0; b < audio::kBandCount; ++b)
        assert(std::abs(music.bandRms[b]*music.bandRms[b] / reference2[b] - std::pow(10.0f, 1.3f)) < 0.0001f);
    const auto saved = levels->noise();
    levels->reset();
    assert(levels->ready() && levels->noise() == saved);
    for (unsigned b = 0; b < audio::kBandCount; ++b) quiet.bandRms[b] = std::sqrt(saved[b]);
    levels->apply(quiet, 0.02f);
    for (float value : quiet.levels) assert(value == 0);
}
