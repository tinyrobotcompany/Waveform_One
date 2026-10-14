#include <cassert>
#include <cstdio>
#include "microphone_pcm.h"

int main() {
    microphone_pcm::Levels level;
    assert(level.dbfs() == -120 && level.clipped_percent() == 0);
    for (unsigned i = 0; i < 80000; ++i) level.add(1000);
    assert(level.dc() == 1000 && level.dbfs() == -120);
    level = {};
    for (unsigned i = 0; i < 80000; ++i) level.add(i % 2 ? 16384 : -16384);
    assert(std::abs(level.dbfs() + 6.0206) < 0.001 && level.dc() == 0);
    assert(level.clipped_percent() == 0);
    level = {};
    level.add(INT16_MIN); level.add(INT16_MAX); level.add(0); level.add(0);
    assert(level.clipped_percent() == 50 && level.dc() == -0.25);
    const int16_t stereo[] = {-32768, 32767, -1, 1, 1234, -1234, 0, 17};
    int16_t output[5] = {0, 0, 0, 0, 99};
    assert(microphone_pcm::select(stereo, 4, 0, output));
    assert(output[0] == -32768 && output[1] == -1 && output[2] == 1234 && output[3] == 0);
    assert(output[4] == 99);
    assert(microphone_pcm::select(stereo, 4, 1, output));
    assert(output[0] == 32767 && output[1] == 1 && output[2] == -1234 && output[3] == 17);
    assert(output[4] == 99);
    assert(!microphone_pcm::select(stereo, 4, 2, output));
    assert(!microphone_pcm::select(nullptr, 4, 0, output));
    assert(!microphone_pcm::select(stereo, 4, 0, nullptr));
    std::puts("PASS: onboard stereo frame selection preserves signed PCM without averaging or rate doubling");
}
