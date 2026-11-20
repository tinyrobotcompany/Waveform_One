#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace microphone_privacy {
// Every edge invalidates previously captured audio, even a mute/unmute during I/O.
inline std::atomic<uint32_t> state{0};
inline std::atomic_bool storage_error{false};
bool initialize();
bool request(bool mute);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
inline void set_muted(bool muted) {
    uint32_t previous = state.load();
    while (bool(previous & 1) != muted &&
           !state.compare_exchange_weak(previous, ((previous + 2) & ~1U) | unsigned(muted))) {}
}
inline bool muted() { return state.load() & 1; }
struct Permit {
    uint32_t generation = state.load();
    bool valid() const { return !(generation & 1) && !muted() && state.load() == generation; }
    static bool check(void *context) { return static_cast<Permit *>(context)->valid(); }
};
inline void wipe(void *memory, size_t bytes) {
    auto *target = static_cast<volatile unsigned char *>(memory);
    while (bytes--) *target++ = 0;
}
struct Scrub {
    void *memory; size_t bytes;
    ~Scrub() { wipe(memory, bytes); }
};
}
void microphone_privacy_start();
