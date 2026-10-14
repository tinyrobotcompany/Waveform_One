#include "p4_hub75.h"
#include "p4_led_spectrum.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <set>

int main() {
    using namespace p4_hub75;
    assert(std::set<int>(pins.begin(), pins.end()).size() == 13);
    for (int pin : pins) {
        // Protect I2C, onboard audio, C6 SDIO/reset, LCD and debug signals.
        assert(!(pin >= 6 && pin <= 27));
        assert(pin != 32 && pin != 33 && pin != 35 && pin != 36 && pin != 37 && pin != 38);
        assert(pin < 53);
    }
    Frame frame{};
    encode(frame, [](unsigned x, unsigned row) { return (x+row) & 63; });
    for (unsigned row = 0; row < rows; ++row) {
        unsigned pulses = 0, lit = 0;
        for (unsigned x = 0; x < clocks; ++x) {
            const auto word = frame[row*clocks+x];
            assert((word >> 8) == row);
            if (word & latch) { ++pulses; assert(word & blank); assert(x == 63); }
            if (!(word & blank)) { ++lit; assert(x >= 66 && x < 98); assert(!(word & latch)); }
            if (x < 64) { assert(word & blank); assert((word & 63) == ((x+row)&63)); }
        }
        assert(pulses == 1 && lit == 32);
        assert(frame[row*clocks] & blank);
        assert(frame[(row+1)*clocks-1] & blank);
    }
    Buffers buffers;
    assert(buffers.writable() == 0);
    assert(!buffers.submit(1));
    assert(buffers.submit(0));
    assert(buffers.writable() == -1 && !buffers.submit(0));
    assert(!buffers.switched(1) && buffers.writable() == -1);
    assert(buffers.switched(0) && buffers.writable() == 1);
    assert(!buffers.switched(0));
    for (int i = 0; i < 100; ++i) {
        const auto next = buffers.writable();
        assert(buffers.submit(next));
        assert(buffers.writable() == -1);
        assert(buffers.switched(next));
        assert(buffers.writable() == 1-next);
    }
    auto spectrum = std::make_unique<p4_led::Spectrum>();
    int16_t pcm[512]{};
    auto out = spectrum->process(pcm);
    assert(out.rms == 0);
    for (float level : out.levels) assert(level == 0);
    for (auto &sample : pcm) sample = 1234;
    out = spectrum->process(pcm);
    assert(out.rms < 1e-6f);
    for (float level : out.levels) assert(level == 0);
    // A 1 kHz tone must retain its physical frequency and RMS at 16 kHz.
    for (unsigned i = 0; i < 512; ++i) pcm[i] = int16_t(3276 * std::sin(6.28318530718 * 1000 * i / 16000));
    out = spectrum->process(pcm);
    float power = 0;
    for (float rms : out.bandRms) power += rms*rms;
    assert(std::abs(out.rms - 0.1/std::sqrt(2.0)) < 0.0001);
    assert(std::abs(power - 0.005) < 0.00001);
    const auto strongest = std::max_element(out.bandRms.begin(), out.bandRms.end()) - out.bandRms.begin();
    assert(strongest == int(24 * std::log(1000.0/60) / std::log(7500.0/60)));
}
