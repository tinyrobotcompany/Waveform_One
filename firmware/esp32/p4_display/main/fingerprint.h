#pragma once

#include <array>
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace fingerprint {

inline constexpr unsigned kHopSamples = 128;
// Longest signature the legacy format accepts: eight seconds.
inline constexpr unsigned kMaxSignatureHops = 1000;
// Audio the analyser can still turn into a signature: about 16 seconds.
inline constexpr unsigned kHistoryHops = 2048;

struct Peak { uint32_t pass; uint16_t magnitude; uint16_t frequency; };
struct Signature {
    unsigned samples = 0;
    std::array<std::vector<Peak>, 3> bands;
    size_t peaks() const { return bands[0].size() + bands[1].size() + bands[2].size(); }
};

// A peak that passed every loudness-independent test. Its level is kept as a
// natural log of the unscaled power so the clip's gain can be applied later.
struct Candidate {
    uint32_t hop;
    uint16_t bin;
    double power, before, after;
};

// Continuous form of the legacy signature algorithm. Each 128-sample hop is
// transformed as it arrives, so a signature for any recent stretch of audio is
// ready as soon as that audio has been heard. Peak selection only compares
// powers with each other, so it runs on unscaled audio; the clip-dependent
// gain is applied when a signature is requested.
//
// Allocate in PSRAM with 16-byte alignment, never on a task's stack.
// Processing is local; only the compact signature goes to the recognition
// service.
class Analyzer {
public:
    Analyzer();
    // Starts a new recording: hop numbering restarts at zero and no audio
    // before this point is used.
    void reset();
    void push(const int16_t *hop);
    uint32_t hops() const { return hops_; }
    // Signature for hops [first, last), as if those hops were a separate
    // recording. Needs last <= hops(), last - first <= kMaxSignatureHops and
    // first still within kHistoryHops of hops().
    bool signature(uint32_t first, uint32_t last, Signature &output) const;

private:
    void transform();
    void extract(uint32_t pass);

    uint32_t hops_ = 0;
    float samples_[2048];
    float window_[2048];
    alignas(16) std::complex<float> fft_[2048];
    std::complex<float> twiddles_[1024];
    float powers_[256][1025];
    float spread_[256][1025];
    uint16_t hop_peak_[kHistoryHops];
    static constexpr size_t kCandidates = 16384;
    Candidate candidates_[kCandidates];
    size_t candidate_count_ = 0;   // total ever stored since reset
    int64_t lost_through_ = -1;    // newest hop whose candidates were overwritten
};

// Batch form: a complete four-second mono signed little-endian 16 kHz clip.
// The legacy signature stops after >=3.1 seconds and >=255 peaks, or clip end.
bool generate(const uint8_t *pcm, size_t bytes, Analyzer &analyzer, Signature &output);
std::vector<uint8_t> encode(const Signature &signature);
std::string uri(const Signature &signature);

// Checks the platform FFT against the portable reference on a fixed frame.
// Returns the largest relative power error found.
double self_test();

} // namespace fingerprint
