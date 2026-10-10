#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace search_waveform {
inline constexpr unsigned kWidth = 320, kHeight = 90;
inline constexpr size_t kPixels = kWidth * kHeight;
constexpr uint16_t rgb565(unsigned red, unsigned green, unsigned blue) {
    return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
}
inline constexpr uint16_t kBackground = rgb565(0x12, 0x1a, 0x13);

// Decorative search feedback, not a representation of microphone levels.
// Render into one fixed RGB565 image; no per-bar LVGL objects or resizing.
inline bool render(uint16_t *pixels, size_t count, unsigned phase)
{
    if (pixels == nullptr || count != kPixels) return false;
    std::fill_n(pixels, count, kBackground);
    for (unsigned bar = 0; bar < 15; ++bar) {
        const float wave = 0.5f + 0.5f * std::sin((phase % 1024) * 0.006135923f + bar * 0.65f);
        const unsigned height = 16 + static_cast<unsigned>(64 * wave);
        const unsigned top = (kHeight - height) / 2;
        const unsigned left = 14 + bar * 20;
        const unsigned opacity = 100 + static_cast<unsigned>(155 * wave);
        const auto blend = [opacity](unsigned color, unsigned background) {
            return (color * opacity + background * (255 - opacity)) / 255;
        };
        const uint16_t color = rgb565(blend(0xa7, 0x12), blend(0x9a, 0x1a), blend(0xe8, 0x13));
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < 12; ++x) {
                const int dx = static_cast<int>(x) * 2 - 11;
                const int dy = y < 6 ? static_cast<int>(y) * 2 - 11
                    : y >= height - 6 ? static_cast<int>(height - 1 - y) * 2 - 11 : 0;
                if (dx * dx + dy * dy <= 144)
                    pixels[(top + y) * kWidth + left + x] = color;
            }
        }
    }
    return true;
}
} // namespace search_waveform
