// Streams a 16 kHz mono PCM16 WAV through the device's continuous analyser and
// prints the signature for each requested window as one JSON line, with its
// peak overlap against the same audio fingerprinted as a separate recording.
// Usage: stream_cli FILE.wav START_S:LENGTH_S [START_S:LENGTH_S ...]
#include "fingerprint.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<uint8_t> wav((std::istreambuf_iterator<char>(input)), {});
    // Locate the data chunk rather than assuming a 44-byte header.
    size_t at = 12, data = 0, size = 0;
    while (at + 8 <= wav.size()) {
        const uint32_t length = wav[at + 4] | wav[at + 5] << 8 | wav[at + 6] << 16 | uint32_t(wav[at + 7]) << 24;
        if (std::string(reinterpret_cast<char *>(&wav[at]), 4) == "data") { data = at + 8; size = length; break; }
        at += 8 + length + (length & 1);
    }
    if (data == 0 || data + size > wav.size()) return 3;
    const size_t hops = size / 256;
    auto stream = std::make_unique<fingerprint::Analyzer>();
    auto separate = std::make_unique<fingerprint::Analyzer>();
    std::vector<int16_t> samples(hops * 128);
    for (size_t i = 0; i < samples.size(); ++i) samples[i] = int16_t(wav[data + i * 2] | wav[data + i * 2 + 1] << 8);
    for (int arg = 2; arg < argc; ++arg) {
        double start = 0, length = 0;
        if (std::sscanf(argv[arg], "%lf:%lf", &start, &length) != 2) return 4;
        const uint32_t first = uint32_t(std::lround(start * 125)), last = first + uint32_t(std::lround(length * 125));
        // Five seconds of real history before the window, as on the device.
        stream->reset();
        for (size_t hop = first > 625 ? first - 625 : 0; hop < last && hop < hops; ++hop) stream->push(&samples[hop * 128]);
        const uint32_t base = first > 625 ? first - 625 : 0;
        fingerprint::Signature windowed, recorded;
        if (last > hops || !stream->signature(first - base, last - base, windowed)) { std::printf("{\"window\":\"%s\",\"accepted\":false}\n", argv[arg]); continue; }
        fingerprint::generate(reinterpret_cast<const uint8_t *>(&samples[first * 128]), (last - first) * 256, *separate, recorded);
        size_t same = 0;
        for (unsigned band = 0; band < 3; ++band)
            for (const auto &a : windowed.bands[band])
                for (const auto &b : recorded.bands[band])
                    if (a.pass == b.pass && a.frequency == b.frequency) { ++same; break; }
        std::printf("{\"window\":\"%s\",\"accepted\":true,\"start_s\":%.3f,\"samples\":%u,\"samplems\":%u,"
                    "\"peaks\":%zu,\"recorded_peaks\":%zu,\"shared_peaks\":%zu,\"uri\":\"%s\"}\n",
            argv[arg], first / 125.0, windowed.samples, windowed.samples / 16, windowed.peaks(), recorded.peaks(), same,
            fingerprint::uri(windowed).c_str());
    }
}
