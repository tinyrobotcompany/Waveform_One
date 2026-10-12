#include <cassert>
#include <cmath>
#include <vector>
#include "capture_quality.h"

int main() {
    using capture_quality::inspect;
    const uint8_t silence[] = {0, 0, 0, 0};
    auto value = inspect(silence, sizeof(silence));
    assert(value.valid && value.samples == 2 && value.peak == 0 && value.rms == 0);
    const uint8_t extremes[] = {0xff, 0x7f, 0, 0x80};
    value = inspect(extremes, sizeof(extremes));
    assert(value.valid && value.peak == 32768 && value.clipped == 2);
    assert(std::abs(value.dc + .5) < 1e-9);
    assert(std::abs(value.rms - std::sqrt((32767.0*32767.0 + 32768.0*32768.0)/2)) < 1e-6);
    const uint8_t signed_values[] = {0xe8, 3, 0x18, 0xfc}; // +1000, -1000
    value = inspect(signed_values, sizeof(signed_values));
    assert(value.peak == 1000 && value.rms == 1000 && value.dc == 0 && value.clipped == 0);
    assert(!inspect(nullptr, 2).valid);
    assert(!inspect(silence, 0).valid);
    assert(!inspect(silence, 3).valid);
    assert(!inspect(silence, 256002).valid); // Reject before reading an oversized buffer.
    std::vector<uint8_t> full(128000, 0xff); // -1, complete four-second clip
    value = inspect(full.data(), full.size());
    assert(value.samples == 64000 && value.peak == 1 && value.rms == 1 && value.dc == -1);
}
