#pragma once
#include "music_features.h"
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace music_detection {
// One audio producer, one inference consumer. Ownership release/acquire keeps
// samples and metadata immutable while the consumer reads. Never waits/allocates.
class Windows {
    enum class Ownership { Free, Filling, Ready, Reading };
public:
    struct Window {
        uint8_t *pcm = nullptr;
        unsigned epoch = 0;
        int64_t captured_at = 0;
    private:
        std::atomic<Ownership> ownership{Ownership::Free};
        friend class Windows;
    };
    void initialize(uint8_t *first, uint8_t *second) {
        windows_[0].pcm = first; windows_[1].pcm = second;
    }
    bool push(int16_t sample, unsigned epoch, int64_t now) {
        if (filling_ < 0) {
            for (int index = 0; index < 2; ++index) {
                Ownership expected = Ownership::Free;
                if (windows_[index].ownership.compare_exchange_strong(expected, Ownership::Filling)) {
                    filling_ = index; used_ = 0; windows_[index].epoch = epoch; break;
                }
            }
        }
        if (filling_ < 0) return false;
        auto &window = windows_[filling_];
        window.pcm[used_++] = uint16_t(sample) & 255;
        window.pcm[used_++] = uint16_t(sample) >> 8;
        if (used_ == kSamples * 2) {
            window.captured_at = now;
            window.ownership.store(Ownership::Ready);
            filling_ = -1;
        }
        return true;
    }
    Window *acquire() {
        Window *oldest = nullptr;
        for (auto &window : windows_) {
            if (window.ownership.load() == Ownership::Ready &&
                (!oldest || window.captured_at < oldest->captured_at)) oldest = &window;
        }
        if (!oldest) return nullptr;
        Ownership expected = Ownership::Ready;
        return oldest->ownership.compare_exchange_strong(expected, Ownership::Reading) ? oldest : nullptr;
    }
    void release(Window &window) { window.ownership.store(Ownership::Free); }
    // Called only by producer. Does not reclaim consumer-owned/queued storage.
    void discontinuity() {
        if (filling_ >= 0) windows_[filling_].ownership.store(Ownership::Free);
        filling_ = -1; used_ = 0;
    }
private:
    Window windows_[2];
    int filling_ = -1;
    size_t used_ = 0;
};
}
