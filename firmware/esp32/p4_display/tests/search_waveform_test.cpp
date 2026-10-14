#include <array>
#include <cassert>
#include <limits>
#include "search_waveform.h"

int main()
{
    using namespace search_waveform;
    std::array<uint16_t, kPixels + 2> storage{};
    storage.front() = 0x1234; storage.back() = 0x5678;
    auto *pixels = storage.data() + 1;
    assert(!render(nullptr, kPixels, 0));
    assert(!render(pixels, kPixels - 1, 0));
    assert(!render(pixels, kPixels + 1, 0));
    assert(pixels[0] == 0); // rejected requests do not write
    std::array<uint16_t, kPixels> first{};
    assert(render(first.data(), first.size(), 0));
    bool changed = false;
    for (unsigned phase = 0; phase < 1024; ++phase) {
        assert(render(pixels, kPixels, phase));
        assert(storage.front() == 0x1234 && storage.back() == 0x5678);
        for (unsigned y = 0; y < kHeight; ++y) {
            for (unsigned x = 0; x < kWidth; ++x) {
                // Fixed gutters and inter-bar gaps must remain background.
                if (x < 14 || x >= 306 || (x - 14) % 20 >= 12)
                    assert(pixels[y * kWidth + x] == kBackground);
            }
        }
        changed = changed || !std::equal(first.begin(), first.end(), pixels);
    }
    assert(changed);
    assert(render(pixels, kPixels, 1024));
    assert(std::equal(first.begin(), first.end(), pixels));
    assert(render(pixels, kPixels, std::numeric_limits<unsigned>::max()));
    assert(storage.front() == 0x1234 && storage.back() == 0x5678);
}
