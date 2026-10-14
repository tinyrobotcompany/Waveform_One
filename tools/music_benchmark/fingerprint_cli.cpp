#include "fingerprint.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 3;
    std::vector<uint8_t> pcm((std::istreambuf_iterator<char>(input)), {});
    if (pcm.size() > 256000) return 4;
    auto workspace = std::make_unique<fingerprint::Workspace>();
    fingerprint::Signature signature;
    const auto start = std::chrono::steady_clock::now();
    const bool ok = fingerprint::generate(pcm.data(), pcm.size(), *workspace, signature);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count();
    if (!ok) { std::puts("{\"accepted\":false}"); return 0; }
    const auto uri = fingerprint::uri(signature);
    std::printf("{\"accepted\":true,\"samples\":%u,\"samplems\":%u,\"peaks\":%zu,\"cpu_ms\":%.3f,\"uri\":\"%s\"}\n",
        signature.samples,signature.samples/16,signature.peaks(),ms,uri.c_str());
}
