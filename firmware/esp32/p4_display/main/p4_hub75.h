#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace p4_hub75 {
// Waveshare 7B expansion GPIOs; physical socket orientation must be verified.
inline constexpr std::array<int, 13> pins{2,3,4,5,28,29,30,31,34,49,50,51,52};
inline constexpr unsigned width = 64, rows = 16, clocks = 128;
inline constexpr unsigned words = rows * clocks;
inline constexpr uint16_t latch = 1U << 6, blank = 1U << 7;
using Frame = std::array<uint16_t, words>;

// 64 shift clocks (latch on the last), then blanking and 32 illuminated clocks.
// Every row/address transition and every buffer boundary is blanked.
template<class Pixel> void encode(Frame &out, Pixel pixel) {
    for (unsigned row = 0; row < rows; ++row) {
        const uint16_t address = row << 8;
        for (unsigned x = 0; x < clocks; ++x) {
            uint16_t value = address | blank;
            if (x < width) {
                value |= pixel(x, row) & 63;
                if (x == width - 1) value |= latch;
            }
            else if (x >= 66 && x < 98) value = address;
            out[row * clocks + x] = value;
        }
    }
}
inline uint8_t diagnostic(unsigned x, unsigned row) {
    // RGB stripes above, complementary stripes below; black edge for alignment.
    if (x == 0 || x == 63 || row == 0 || row == 15) return 0;
    const unsigned colour = 1U << ((x / 8) % 3);
    return colour | ((colour ^ 7) << 3);
}
// Task-owned handoff state. A buffer stays immutable until the DMA callback
// confirms that the replacement is active. Unexpected callbacks are rejected.
class Buffers {
public:
    // IDF starts the first loop synchronously, without a buffer-switch event.
    // Call only after that first transmit succeeds; retain this frame until
    // an actual replacement callback releases it.
    bool started(int index) {
        if (active_ >= 0 || pending_ >= 0 || index < 0 || index > 1) return false;
        active_ = index;
        return true;
    }
    int writable() const { return pending_ >= 0 ? -1 : active_ == 0 ? 1 : 0; }
    bool submit(int index) {
        if (index != writable() || index < 0) return false;
        pending_ = index; return true;
    }
    bool switched(int index) {
        if (pending_ < 0 || index != pending_) return false;
        active_ = pending_; pending_ = -1; return true;
    }
private:
    int active_ = -1, pending_ = -1;
};
}
