#pragma once
#include <cstdint>

namespace panel {
constexpr int kWidth = 64;
constexpr int kScanRows = 16;
constexpr char kMessage[] = "I Love Kovi K";
constexpr int kScale = 2;
constexpr int kAdvance = 6 * kScale;
constexpr int kTextWidth = (sizeof(kMessage) - 1) * kAdvance - kScale;

// Original 5x7 glyphs, one five-bit mask per row; bit 4 is leftmost.
inline uint8_t glyphRow(char ch, int row)
{
    constexpr uint8_t glyphs[][7] = {
        {14, 4, 4, 4, 4, 4, 14},       // I
        {16, 16, 16, 16, 16, 16, 31}, // L
        {0, 0, 14, 17, 17, 17, 14},   // o
        {0, 0, 17, 17, 17, 10, 4},    // v
        {0, 0, 14, 17, 31, 16, 14},   // e
        {17, 18, 20, 24, 20, 18, 17}, // K
        {4, 0, 12, 4, 4, 4, 14},      // i
    };
    constexpr char letters[] = "ILoveKi";
    for (int i = 0; letters[i]; ++i)
        if (ch == letters[i]) return glyphs[i][row];
    return 0;
}

inline uint8_t pixel(int offset, int x, int y)
{
    const int localX = x - offset;
    const int localY = y - 9; // Centre the 14-pixel-high text on 32 rows.
    if (localX < 0 || localX >= kTextWidth || localY < 0 || localY >= 14)
        return 0;
    const int column = (localX % kAdvance) / kScale;
    if (column >= 5) return 0;
    const auto bits = glyphRow(kMessage[localX / kAdvance], localY / kScale);
    return (bits & (1 << (4 - column))) ? 5 : 0; // Magenta: red + blue.
}

inline uint8_t rowPair(int offset, int x, int row)
{
    return pixel(offset, x, row) | (pixel(offset, x, row + 16) << 3);
}
} // namespace panel
