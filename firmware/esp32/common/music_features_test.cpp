#include "music_features.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
int main() {
    std::vector<uint8_t> pcm(music_detection::kSamples * 2);
    music_detection::FeatureWorkspace workspace{};
    int8_t output[6144];
    assert(music_detection::input_gain(pcm.data(), pcm.size()) == 1);
    assert(!music_detection::features(nullptr, pcm.size(), workspace, output, sizeof(output)));
    assert(!music_detection::features(pcm.data(), pcm.size()-1, workspace, output, sizeof(output)));
    assert(!music_detection::features(pcm.data(), pcm.size(), workspace, output, sizeof(output)-1));
    assert(!music_detection::features(pcm.data(), pcm.size(), workspace, nullptr, sizeof(output)));
    assert(!music_detection::features(pcm.data(), pcm.size(), workspace, output, sizeof(output), nullptr, std::numeric_limits<float>::quiet_NaN()));
    assert(music_detection::features(pcm.data(), pcm.size(), workspace, output, sizeof(output)));
    for (auto value : output) assert(value == -128); // log(.001) at model quantization.
    for (size_t i = 0; i < pcm.size(); i += 2) { pcm[i] = 1; pcm[i+1] = 0; }
    assert(music_detection::input_gain(pcm.data(), pcm.size()) == 256);
    pcm[0] = 0; pcm[1] = 128; // -32768, test signed little-endian peak limit.
    assert(music_detection::input_gain(pcm.data(), pcm.size()) <= .95f);
    assert(music_detection::features(pcm.data(), pcm.size(), workspace, output, sizeof(output), nullptr, music_detection::input_gain(pcm.data(), pcm.size())));
}
