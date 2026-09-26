#pragma once
#include <algorithm>
#include <cmath>
#include "audio_pipeline.h"

namespace panel {
constexpr int kWidth = 64;
constexpr int kScanRows = 16;
inline uint8_t pixel(const audio::Frame& frame, int x, int y)
{
    if (!frame.calibrated || x < 0 || x >= kWidth || y < 0 || y >= 32) return 0;
    // 24 bands occupy 2 or 3 columns each. Leave each band's last column black.
    const int band = ((x + 1) * audio::kBandCount - 1) / kWidth;
    const int end = (band + 1) * kWidth / audio::kBandCount;
    if (x == end - 1) return 0;
    const float level = frame.levels[band];
    if (!std::isfinite(level)) return 0;
    const int height = static_cast<int>(std::clamp(level, 0.0f, 1.0f) * 32 + 0.5f);
    if (y < 32 - height) return 0;
    return y < 6 ? 1 : y < 14 ? 3 : 2; // Red tips, yellow middle, green base.
}
inline uint8_t rowPair(const audio::Frame& frame, int x, int row)
{
    return pixel(frame, x, row) | (pixel(frame, x, row + 16) << 3);
}
}
