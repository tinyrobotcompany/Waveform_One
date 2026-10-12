#pragma once

#include <array>
#include <complex>
#include <cstdint>
#include <string>
#include <vector>
#include "fingerprint_gain.h"

namespace fingerprint {

struct Peak { uint32_t pass; uint16_t magnitude; uint16_t frequency; };
struct Signature {
    unsigned samples = 0;
    std::array<std::vector<Peak>, 3> bands;
    size_t peaks() const { return bands[0].size() + bands[1].size() + bands[2].size(); }
};

// Allocate this workspace in PSRAM, never on a task's stack. Processing is local;
// only the compact signature goes to the recognition service.
struct Workspace {
    Normalisation normalisation;
    float powers[256][1025];
    float spread[256][1025];
    float samples[2048];
    float window[2048];
    std::complex<float> fft[2048];
    std::complex<float> twiddles[1024];
};

// Optional aggregate timings; clock must be monotonic and in microseconds.
struct Profile {
    int64_t setup_us = 0, fft_us = 0, spread_us = 0, peaks_us = 0, yield_us = 0;
};

// Input is a complete four-second mono signed little-endian 16 kHz clip.
// The legacy signature stops after >=3.1 seconds and >=255 peaks, or clip end.
bool generate(const uint8_t *pcm, size_t bytes, Workspace &workspace, Signature &output,
              void (*yield)() = nullptr, Profile *profile = nullptr, int64_t (*clock)() = nullptr);
std::vector<uint8_t> encode(const Signature &signature);
std::string uri(const Signature &signature);

} // namespace fingerprint
