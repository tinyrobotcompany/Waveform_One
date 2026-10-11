#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace music_detection {
enum class State { Unknown, Quiet, Music };
inline const char *name(State state) {
    return state == State::Music ? "MUSIC" : state == State::Quiet ? "QUIET" : "UNKNOWN";
}
// Scores are model outputs, not calibrated probabilities. Speech is not a
// veto: vocals and people talking over a song must remain eligible as music.
inline float music_score(const int8_t *scores) {
    if (!scores) return 0;
    const int best = std::max({int(scores[132]), int(scores[24]), int(scores[31])});
    return (best + 128) / 256.0f;
}
class Policy {
public:
    State update(float score, int64_t now) {
        if (!std::isfinite(score) || score < 0 || score > 1) { reset(); return State::Unknown; }
        if (last_ && (now <= last_ || now - last_ > 3000000)) reset();
        last_ = now;
        if (score >= 0.50f) {
            negative_since_ = 0;
            if (++positives_ >= 2 || state_ == State::Music) state_ = State::Music;
            positive_at_ = now;
        } else {
            positives_ = 0;
            if (score <= 0.20f) {
                if (!negative_since_) negative_since_ = now;
                // Hold through short interruptions and speech over a song.
                if (state_ != State::Music || now - negative_since_ >= 5000000)
                    state_ = State::Quiet;
            } else {
                negative_since_ = 0;
                if (state_ != State::Music || now - positive_at_ >= 8000000)
                    state_ = State::Unknown;
            }
        }
        // Indefinite alternating weak/negative results must not retain music.
        if (state_ == State::Music && now - positive_at_ >= 8000000) state_ = State::Unknown;
        return state_;
    }
    void reset() { state_ = State::Unknown; positives_ = 0; last_ = positive_at_ = negative_since_ = 0; }
private:
    State state_ = State::Unknown;
    unsigned positives_ = 0;
    int64_t last_ = 0, positive_at_ = 0, negative_since_ = 0;
};
}
