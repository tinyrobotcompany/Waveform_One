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
// Measured on the P4's onboard microphones' levels (music at -60 to -68 dBFS
// over a -74 dBFS room, 0.4 s reverberation; 2026-10-17): speech in four
// voices and six household noises never scored above 0.06, while music
// reached 0.2 in 30-100% of windows, usually within the first second. Quiet
// music can go up to 18 s between such windows.
class Presence {
public:
    static constexpr float kWeak = 0.2f, kStrong = 0.5f;
    static constexpr int64_t kPairUs = 4000000;      // two weak windows this close enter music
    static constexpr int64_t kHoldUs = 20000000;     // music holds this long without a positive
    static constexpr int64_t kVerdictUs = 10000000;  // sound this long with no positive is not music

    void update(float score, int64_t now) {
        if (!std::isfinite(score) || score < 0 || score > 1) return;
        if (!started_) { started_ = true; since_ = now; }
        if (music_ && now - positive_at_ > kHoldUs) music_ = false;
        if (score < kWeak) return;
        if (score >= kStrong || (weak_at_ && now - weak_at_ <= kPairUs)) music_ = true;
        weak_at_ = now;
        if (music_) positive_at_ = now;
    }
    bool music(int64_t now) const { return music_ && now - positive_at_ <= kHoldUs; }
    // Long enough without a single music-like window to call it something else
    // (talk, a vacuum cleaner). Never true while music is held.
    bool not_music(int64_t now) const {
        return started_ && !music(now) && now - since_ >= kVerdictUs
            && (!weak_at_ || now - weak_at_ >= kVerdictUs);
    }
    // Silence: whatever follows must earn its own decision.
    void reset() { *this = {}; }

private:
    bool started_ = false, music_ = false;
    int64_t since_ = 0, weak_at_ = 0, positive_at_ = 0;
};
}
