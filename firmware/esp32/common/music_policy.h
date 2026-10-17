#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace music_detection {
// Scores are model outputs, not calibrated probabilities: the best of YAMNet's
// Music, Singing and Rapping classes.
inline float music_score(const int8_t *scores) {
    if (!scores) return 0;
    const int best = std::max({int(scores[132]), int(scores[24]), int(scores[31])});
    return (best + 128) / 256.0f;
}

// Whether the sound the level detector hears is music.
//
// Measured on the P4's onboard microphones (2026-10-17): music at -60 to
// -68 dBFS over a -74 dBFS room reached 0.2 in 30-100% of windows. Real talk
// next to the device mostly scored 0, with rare bursts up to 0.67, each
// covering at most two separate windows. Windows are analysed every ~0.4 s
// and overlap, so only windows with separate audio count as evidence.
class Presence {
public:
    static constexpr float kWeak = 0.2f;
    static constexpr unsigned kNeeded = 3;            // separate music-like windows...
    static constexpr int64_t kSpanUs = 6000000;       // ...within 6 s enter music
    static constexpr int64_t kHoldUs = 15000000;      // music holds this long without one
    static constexpr uint64_t kWindowSamples = 15600;

    // end is the stream position just after the window's last sample.
    void update(float score, int64_t now, uint64_t end) {
        if (!std::isfinite(score) || score < 0 || score > 1) return;
        if (music_ && now - positive_at_ > kHoldUs) music_ = false;
        if (score < kWeak) return;
        if (music_) positive_at_ = now;
        if (count_ && end < last_end_ + kWindowSamples) return; // same audio again
        unsigned kept = 0;
        for (unsigned i = 0; i < count_; ++i)
            if (now - at_[i] <= kSpanUs) at_[kept++] = at_[i];
        count_ = kept;
        if (count_ < kNeeded) at_[count_++] = now;
        else { for (unsigned i = 1; i < kNeeded; ++i) at_[i - 1] = at_[i]; at_[kNeeded - 1] = now; }
        last_end_ = end;
        if (count_ >= kNeeded) { music_ = true; positive_at_ = now; }
    }
    bool music(int64_t now) const { return music_ && now - positive_at_ <= kHoldUs; }
    // Silence: whatever follows must earn its own decision.
    void reset() { *this = {}; }

private:
    bool music_ = false;
    int64_t positive_at_ = 0, at_[kNeeded]{};
    unsigned count_ = 0;
    uint64_t last_end_ = 0;
};
}
