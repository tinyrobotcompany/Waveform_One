#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include "visualizer_config.h"

static audio::Bands noise(float rms)
{
    audio::Bands bands{};
    bands.fill(rms * rms / audio::kBandCount);
    return bands;
}
static void calibrate(audio::Pipeline& p, float rms)
{
    for (int i = 0; i < audio::kCalibrationFrames; ++i) p.process(rms, noise(rms));
}
static int height(const audio::Frame& f)
{
    return int(*std::max_element(f.levels.begin(), f.levels.end()) * 32 + 0.5f);
}
int main()
{
    audio::Pipeline oldProfile, room(visualizerConfig());
    constexpr float ambient = 0.002202f; // Noise RMS observed on the user's bench.
    calibrate(oldProfile, ambient);
    calibrate(room, ambient);
    auto music = noise(ambient);
    // Quiet tonal component over ambient; total RMS is below the old gate.
    const float musicRms = ambient * 1.4f;
    music[12] += musicRms * musicRms - ambient * ambient;
    audio::Frame f;
    for (int i = 0; i < 100; ++i) {
        assert(!oldProfile.process(musicRms, music).active);
        f = room.process(musicRms, music);
        assert(f.active);
    }
    assert(height(f) >= 8 && height(f) < 32);
    // A short musical dip must not close the gate immediately.
    for (int i = 0; i < 4; ++i) assert(room.process(ambient, noise(ambient)).active);
    // Ambient returning for longer must still decay to black, without beats.
    for (int i = 0; i < 150; ++i) {
        f = room.process(ambient, noise(ambient));
        assert(!f.beat);
    }
    assert(!f.active && height(f) == 0);
    for (int i = 0; i < 300; ++i) {
        const float rms = ambient * (i % 2 ? 0.98f : 1.16f);
        f = room.process(rms, noise(rms));
        assert(!f.active && !f.beat && height(f) == 0);
    }
    audio::Pipeline quiet(visualizerConfig()), original;
    calibrate(quiet, 0);
    calibrate(original, 0);
    auto faint = noise(0);
    faint[12] = 0.00018f * 0.00018f;
    for (int i = 0; i < 100; ++i) {
        f = quiet.process(0.00018f, faint);
        assert(!original.process(0.00018f, faint).active);
    }
    assert(f.active && height(f) >= 3);
    for (int i = 0; i < 300; ++i) f = quiet.process(0, {});
    assert(!f.active && height(f) == 0);
    std::puts("PASS: quieter audio visible, brief dips held, ambient and silence settle to black");
}
