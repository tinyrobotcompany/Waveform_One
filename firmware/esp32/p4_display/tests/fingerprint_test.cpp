#include <cassert>
#include <cmath>
#include <memory>
#include <vector>
#include "audio_capture.h"
#include "fingerprint.h"
#include "fingerprint_reference.h"

int main()
{
    auto workspace = std::make_unique<fingerprint::Workspace>();
    std::vector<uint8_t> pcm(audio_capture::kBytes);
    fingerprint::Signature signature;
    assert(!fingerprint::generate(nullptr, pcm.size(), *workspace, signature));
    assert(!fingerprint::generate(pcm.data(), pcm.size() - 1, *workspace, signature));
    assert(fingerprint::generate(pcm.data(), pcm.size(), *workspace, signature));
    assert(signature.peaks() == 0);

    uint32_t random = 0x12345678;
    for (size_t i = 0; i < pcm.size() / 2; ++i) {
        random = 1664525u * random + 1013904223u;
        const uint16_t sample = static_cast<uint16_t>(int(random >> 16) - 32768);
        pcm[i * 2] = static_cast<uint8_t>(sample);
        pcm[i * 2 + 1] = static_cast<uint8_t>(sample >> 8);
    }
    assert(fingerprint::generate(pcm.data(), pcm.size(), *workspace, signature));
    assert(signature.samples == kReferenceSamples);
    assert(signature.peaks() == std::size(kReferencePeaks));
    std::array<size_t, 3> positions{};
    for (const auto &expected : kReferencePeaks) {
        const auto &peak = signature.bands[expected.band][positions[expected.band]++];
        assert(peak.pass == expected.pass);
        // The device's float32 FFT rounds occasionally to an adjacent integer.
        assert(std::abs(int(peak.magnitude) - int(expected.magnitude)) <= 1);
        assert(std::abs(int(peak.frequency) - int(expected.frequency)) <= 1);
    }
    signature = {};
    signature.samples = 49664;
    signature.bands[0] = {{1, 12000, 2600}, {300, 14000, 3000}};
    signature.bands[2] = {{20, 16000, 22000}};
    assert(fingerprint::uri(signature) == kReferenceUri);
    signature.bands[0][1].pass = 0;
    assert(fingerprint::encode(signature).empty());
}
