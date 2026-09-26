#pragma once
#include <array>
#include <string_view>
#include "bars.h"

namespace visual {
enum class Mode { Classic, Mirrored, Waterfall };
inline constexpr Mode kDefaultMode = Mode::Mirrored;
inline const char* name(Mode mode) {
    switch (mode) {
        case Mode::Classic: return "classic";
        case Mode::Mirrored: return "mirrored";
        case Mode::Waterfall: return "waterfall";
    }
    return "classic";
}
inline bool parseMode(std::string_view text, Mode& mode) {
    for (auto candidate : {Mode::Classic, Mode::Mirrored, Mode::Waterfall}) {
        if (text == name(candidate)) { mode = candidate; return true; }
    }
    return false;
}
class Renderer {
public:
    void setMode(Mode mode) {
        if (mode == mode_) return;
        mode_ = mode;
        clear();
    }
    void render(const audio::Frame& frame, uint64_t milliseconds) {
        if (!frame.calibrated) { clear(); return; }
        if (mode_ == Mode::Waterfall) {
            if (started_ && milliseconds >= lastStep_ && milliseconds - lastStep_ < 50) return;
            const uint64_t steps = !started_ || milliseconds < lastStep_ ? 32
                : std::min<uint64_t>((milliseconds - lastStep_) / 50, 32);
            // Insert black gaps after missed updates rather than inventing samples.
            for (int y = 31; y >= 0; --y) for (int x = 0; x < 64; ++x)
                pixels_[y * 64 + x] = y >= int(steps) ? pixels_[(y-int(steps))*64+x] : 0;
            for (int x = 0; x < 64; ++x) {
                const float level = safe(frame.levels[x * audio::kBandCount / 64]);
                pixels_[x] = level < 0.02f ? 0 : level < 0.2f ? 4
                    : level < 0.4f ? 6 : level < 0.6f ? 2 : level < 0.8f ? 3 : 1;
            }
            lastStep_ = milliseconds;
            started_ = true;
            return;
        }
        constexpr uint8_t colours[] = {4, 6, 2, 3, 1, 5};
        for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x) {
            uint8_t colour = 0;
            if (mode_ == Mode::Classic) colour = panel::pixel(frame, x, y);
            else {
                const int band = ((x + 1) * audio::kBandCount - 1) / 64;
                const int end = (band + 1) * 64 / audio::kBandCount;
                const int radius = int(safe(frame.levels[band]) * 16 + 0.5f);
                if (x != end-1 && y >= 16-radius && y < 16+radius)
                    colour = colours[band / 4];
            }
            pixels_[y*64+x] = colour;
        }
    }
    uint8_t pixel(int x, int y) const {
        return x < 0 || x >= 64 || y < 0 || y >= 32 ? 0 : pixels_[y*64+x];
    }
    uint8_t rowPair(int x, int row) const { return pixel(x, row) | (pixel(x, row+16) << 3); }
private:
    static float safe(float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0; }
    void clear() { pixels_.fill(0); started_ = false; }
    Mode mode_ = kDefaultMode;
    std::array<uint8_t, 64*32> pixels_{};
    uint64_t lastStep_ = 0;
    bool started_ = false;
};
}
