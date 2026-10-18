#include "music_features.h"
#include <cstdio>
#include <cstring>
#include <vector>
int main(int argc, char **argv)
{
    if (argc != 3 && (argc != 4 || std::strcmp(argv[3], "--normalize") != 0)) return 2;
    FILE *in = std::fopen(argv[1], "rb"); if (!in) return 2;
    std::vector<uint8_t> pcm(music_detection::kSamples * 2);
    const auto size = std::fread(pcm.data(), 1, pcm.size(), in);
    const bool extra = std::fgetc(in) != EOF; std::fclose(in);
    music_detection::FeatureWorkspace workspace{};
    int8_t output[music_detection::kFrames * music_detection::kBands];
    const float gain = argc == 4 ? music_detection::input_gain(pcm.data(), size) : 1;
    if (extra || !music_detection::features(pcm.data(),size,workspace,output,sizeof(output),nullptr,gain)) return 1;
    FILE *out = std::fopen(argv[2], "wb"); if (!out) return 2;
    const bool ok = std::fwrite(output, 1, sizeof(output), out) == sizeof(output);
    std::fclose(out); return ok ? 0 : 1;
}
