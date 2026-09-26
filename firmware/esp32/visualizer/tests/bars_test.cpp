#include <cassert>
#include <limits>
#include "bars.h"
int main()
{
    audio::Frame f{};
    f.levels.fill(1);
    assert(panel::rowPair(f, 0, 0) == 0); // Calibration must stay blank.
    f.calibrated = true;
    assert(panel::rowPair(f, 0, 0) == (1 | (2 << 3)));
    f.levels.fill(0);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 64; ++x) assert(panel::pixel(f, x, y) == 0);
    for (int b = 0; b < audio::kBandCount; ++b) {
        f.levels.fill(0);
        f.levels[b] = 1;
        const int start = b * 64 / 24;
        const int end = (b + 1) * 64 / 24;
        for (int x = 0; x < 64; ++x)
            assert((panel::pixel(f, x, 31) != 0) == (x >= start && x < end - 1));
    }
    f.levels.fill(std::numeric_limits<float>::quiet_NaN());
    assert(panel::pixel(f, 0, 31) == 0);
    f.levels.fill(0.5f);
    assert(panel::pixel(f, 0, 15) == 0);
    assert(panel::pixel(f, 0, 16) == 2);
}
