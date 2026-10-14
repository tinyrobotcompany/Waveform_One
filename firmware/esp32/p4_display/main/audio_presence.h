#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include "listening.h"

namespace audio_presence {
struct Snapshot {
    bool sound = false;
    uint32_t segment = UINT32_MAX;
    uint64_t start_sample = 0;
    uint64_t quiet_sample = 0;
};
// Owned by the audio task. Absolute positions allow the network worker to choose
// audio from the same recording without sharing the detector or its history.
class Tracker {
public:
    template<class Callback> void reset(Callback changed) {
        detector_.discontinuity();
        state_.sound = false; state_.quiet_sample = 0;
        next_ = std::numeric_limits<uint64_t>::max();
        changed(state_, listening::Event::None);
    }
    template<class Callback>
    void feed(const int16_t *pcm, size_t count, uint64_t position, uint32_t segment, Callback changed) {
        if (!pcm || count == 0 || count % 128 != 0) return;
        if (segment != state_.segment || position != next_) {
            reset(changed);
            state_.segment = segment; base_ = position; hops_ = 0;
        }
        for (size_t i = 0; i < count; i += 128) {
            uint32_t start = 0;
            const auto event = detector_.push(pcm + i, 128, hops_++, start);
            if (event == listening::Event::SoundStarted) {
                state_.sound = true; state_.start_sample = base_ + uint64_t(start) * 128;
                changed(state_, event);
            } else if (event == listening::Event::SilenceStarted) {
                state_.sound = false; state_.quiet_sample = position + i + 128;
                changed(state_, event);
            }
        }
        next_ = position + count;
    }
    void set_patient(bool value) { detector_.set_patient(value); }
    Snapshot snapshot() const { return state_; }
    const listening::Detector &detector() const { return detector_; }
private:
    listening::Detector detector_;
    Snapshot state_{};
    uint64_t base_ = 0, next_ = std::numeric_limits<uint64_t>::max();
    uint32_t hops_ = 0;
};
}
