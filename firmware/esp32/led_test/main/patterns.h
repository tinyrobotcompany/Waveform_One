#pragma once

#include <cstdint>

namespace panel {
constexpr int kWidth = 64;
constexpr int kHeight = 32;
constexpr int kScanRows = kHeight / 2;
enum class Pattern { Black, Red, Green, Blue, Halves, Rows, Columns, Bars, Count };
constexpr const char* kNames[] = {
    "BLACK", "RED", "GREEN", "BLUE", "TOP RED / BOTTOM BLUE",
    "MOVING ROW", "MOVING COLUMN", "RGB BARS"
};

// RGB bit mask: bit 0 red, bit 1 green, bit 2 blue. No PWM brightness here;
// brightness is bounded by the driver's short output-enable interval.
constexpr uint8_t pixel(Pattern pattern, int step, int x, int y)
{
    switch (pattern) {
    case Pattern::Red: return 1;
    case Pattern::Green: return 2;
    case Pattern::Blue: return 4;
    case Pattern::Halves: return y < kScanRows ? 1 : 4;
    case Pattern::Rows: return y == step % kHeight ? 2 : 0;
    case Pattern::Columns: return x == step % kWidth ? 1 : 0;
    case Pattern::Bars: return uint8_t(1u << (x * 3 / kWidth));
    default: return 0;
    }
}

constexpr uint8_t rowPair(Pattern pattern, int step, int x, int row)
{
    return pixel(pattern, step, x, row)
        | (pixel(pattern, step, x, row + kScanRows) << 3);
}
} // namespace panel
