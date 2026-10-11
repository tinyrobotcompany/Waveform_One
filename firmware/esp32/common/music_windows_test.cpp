#include "music_windows.h"
#include <array>
#include <cassert>
#include <thread>
int main() {
    std::array<uint8_t, music_detection::kSamples * 2> a{}, b{};
    music_detection::Windows windows;
    windows.initialize(a.data(), b.data());
    for (unsigned i = 0; i < music_detection::kSamples; ++i) assert(windows.push(-123, 1, 100));
    auto *first = windows.acquire(); assert(first && first->epoch == 1 && first->captured_at == 100);
    assert(!windows.acquire());
    for (unsigned i = 0; i < music_detection::kSamples; ++i) assert(windows.push(456, 2, 200));
    // Both buffers owned: microphone producer drops rather than overwrites.
    assert(!windows.push(789, 3, 300));
    windows.discontinuity();
    assert(first->pcm[0] == 0x85 && first->pcm[1] == 0xff);
    auto *second = windows.acquire(); assert(second && second != first && second->epoch == 2);
    assert(second->pcm[0] == 0xc8 && second->pcm[1] == 1);
    windows.release(*first);
    assert(windows.push(123, 3, 300));
    windows.discontinuity(); // A partial old-generation window is never queued.
    for (unsigned i = 0; i < music_detection::kSamples; ++i) assert(windows.push(-32768, 4, 400));
    first = windows.acquire(); assert(first && first->epoch == 4 && first->captured_at == 400);
    assert(first->pcm[0] == 0 && first->pcm[1] == 128);
    windows.release(*second); windows.release(*first);
    // Both ready windows are acquired in capture order, regardless of slot.
    for (unsigned i = 0; i < music_detection::kSamples; ++i) windows.push(1, 5, 500);
    first = windows.acquire(); windows.release(*first);
    for (unsigned i = 0; i < music_detection::kSamples; ++i) windows.push(2, 5, 600);
    for (unsigned i = 0; i < music_detection::kSamples; ++i) windows.push(3, 5, 700);
    first = windows.acquire(); assert(first->captured_at == 600);
    second = windows.acquire(); assert(second->captured_at == 700);
    windows.release(*first); windows.release(*second);
    std::atomic_bool finished{false};
    std::thread producer([&] {
        for (unsigned i = 0; i < music_detection::kSamples * 100; ++i) windows.push(int16_t(i), 6, i + 1000);
        finished.store(true);
    });
    unsigned received = 0;
    do {
        auto *window = windows.acquire();
        if (!window) { std::this_thread::yield(); continue; }
        const unsigned initial = window->pcm[0] | unsigned(window->pcm[1]) << 8;
        for (unsigned i = 0; i < music_detection::kSamples; ++i) {
            const unsigned value = window->pcm[2*i] | unsigned(window->pcm[2*i+1]) << 8;
            assert(value == uint16_t(initial + i)); // Contiguous and never overwritten.
        }
        assert(window->epoch == 6);
        windows.release(*window); ++received;
    } while (!finished.load());
    producer.join();
    assert(received > 0);
}
