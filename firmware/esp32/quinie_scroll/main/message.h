#pragma once
#include <cstdint>

namespace panel {
constexpr int kWidth = 64;
constexpr int kScanRows = 16;
constexpr char kMessage[] = "Aye, Aye Quinie, Fine Ta?";
constexpr int kScale = 2;
constexpr int kAdvance = 6 * kScale;
constexpr int kTextWidth = (sizeof(kMessage) - 1) * kAdvance - kScale;

// Five-bit rows, with bit 4 at the left. Original 5x7 glyphs.
inline uint8_t glyphRow(char ch, int row)
{
    constexpr char letters[] = "Aye,QuinFTa?";
    constexpr uint8_t glyphs[][7] = {
        {14,17,17,31,17,17,17}, // A
        {0,0,17,17,15,1,14},   // y
        {0,0,14,17,31,16,14},  // e
        {0,0,0,0,0,4,8},      // comma
        {14,17,17,17,21,18,13},// Q
        {0,0,17,17,17,17,15}, // u
        {4,0,12,4,4,4,14},    // i
        {0,0,30,17,17,17,17}, // n
        {31,16,16,30,16,16,16},// F
        {31,4,4,4,4,4,4},     // T
        {0,0,14,1,15,17,15},  // a
        {14,17,1,2,4,0,4},    // ?
    };
    static_assert(sizeof(letters) - 1 == sizeof(glyphs) / sizeof(glyphs[0]));
    for (int i = 0; letters[i]; ++i)
        if (ch == letters[i]) return glyphs[i][row];
    return 0;
}

inline uint8_t characterColour(int index)
{
    // RGB bits: red, yellow, green, cyan, blue, magenta, white.
    constexpr uint8_t palette[] = {1,3,2,6,4,5,7};
    int visibleCharacters = 0;
    for (int i = 0; i < index; ++i)
        if (kMessage[i] != ' ') ++visibleCharacters;
    return palette[visibleCharacters % 7];
}

inline uint8_t pixel(int offset, int x, int y)
{
    const int localX = x - offset;
    const int localY = y - 9;
    if (localX < 0 || localX >= kTextWidth || localY < 0 || localY >= 14)
        return 0;
    const int column = (localX % kAdvance) / kScale;
    if (column >= 5) return 0;
    const int index = localX / kAdvance;
    const auto bits = glyphRow(kMessage[index], localY / kScale);
    return (bits & (1 << (4 - column))) ? characterColour(index) : 0;
}

inline uint8_t rowPair(int offset, int x, int row)
{
    return pixel(offset, x, row) | (pixel(offset, x, row + 16) << 3);
}
} // namespace panel
