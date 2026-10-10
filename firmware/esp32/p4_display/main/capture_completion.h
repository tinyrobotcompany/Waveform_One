#pragma once
#include <cstdint>
#include <mutex>
#include "usb_controller.h"

namespace capture_completion {
struct Clip { AudioCaptureStatus status = AudioCaptureStatus::Invalid; uint8_t *pcm = nullptr; };

// One completion slot with explicit ownership and a per-request identity. A
// cancelled capture may finish after the worker has started another request.
class Mailbox {
public:
    explicit Mailbox(void (*release)(void *)) : release_(release) {}
    ~Mailbox() { release_(clip_.pcm); }
    uint64_t begin() {
        std::lock_guard<std::mutex> lock(mutex_);
        discard();
        if (++sequence_ == 0) ++sequence_;
        active_ = sequence_; return active_;
    }
    void complete(uint64_t id, AudioCaptureStatus status, uint8_t *pcm) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (id != active_ || active_ == 0 || pending_) { release_(pcm); return; }
        clip_ = {status, pcm}; pending_ = true;
    }
    bool take(uint64_t id, Clip &clip) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (id != active_ || !pending_) return false;
        clip = clip_; clip_ = {}; pending_ = false; active_ = 0; return true;
    }
    void cancel(uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (id == active_) { discard(); active_ = 0; }
    }
private:
    void discard() { release_(clip_.pcm); clip_ = {}; pending_ = false; }
    void (*release_)(void *);
    std::mutex mutex_;
    uint64_t sequence_ = 0, active_ = 0;
    Clip clip_{};
    bool pending_ = false;
};

enum class WaitResult { Complete, Cancelled, TimedOut };

// The production worker and host tests execute this same bounded wait loop.
// Pause must yield briefly so cancellation and deadlines are observed promptly.
template<class Clock, class Epoch, class Pause>
WaitResult wait(Mailbox &mailbox, uint64_t id, uint64_t epoch, int64_t deadline,
                Clip &clip, Clock now, Epoch current_epoch, Pause pause)
{
    clip = {};
    while (true) {
        if (current_epoch() != epoch) { mailbox.cancel(id); return WaitResult::Cancelled; }
        if (now() >= deadline) { mailbox.cancel(id); return WaitResult::TimedOut; }
        if (mailbox.take(id, clip)) return WaitResult::Complete;
        pause();
    }
}
}
