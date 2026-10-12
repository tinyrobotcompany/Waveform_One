#include <cassert>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>
#include "fingerprint.h"
#include "fingerprint_reference.h"

namespace {
constexpr size_t kClipBytes = 128000; // four seconds

std::vector<int16_t> noise(size_t count, uint32_t seed, double scale = 1.0)
{
    std::vector<int16_t> samples(count);
    for (auto &sample : samples) {
        seed = 1664525u * seed + 1013904223u;
        sample = int16_t(std::lround((int(seed >> 16) - 32768) * scale));
    }
    return samples;
}

std::vector<uint8_t> bytes(const int16_t *samples, size_t count)
{
    std::vector<uint8_t> pcm(count * 2);
    for (size_t i = 0; i < count; ++i) {
        pcm[i * 2] = uint8_t(uint16_t(samples[i]));
        pcm[i * 2 + 1] = uint8_t(uint16_t(samples[i]) >> 8);
    }
    return pcm;
}

bool same_peak(const fingerprint::Peak &a, const fingerprint::Peak &b, int tolerance)
{
    // A negative tolerance compares only position: time and frequency.
    return a.pass == b.pass && (tolerance < 0 || std::abs(int(a.magnitude) - int(b.magnitude)) <= tolerance)
        && std::abs(int(a.frequency) - int(b.frequency)) <= 1;
}

// Fraction of expected peaks found in actual, per band.
double overlap(const fingerprint::Signature &expected, const fingerprint::Signature &actual, int tolerance)
{
    size_t found = 0;
    for (unsigned band = 0; band < 3; ++band) {
        for (const auto &peak : expected.bands[band]) {
            for (const auto &other : actual.bands[band]) {
                if (same_peak(peak, other, tolerance)) { ++found; break; }
            }
        }
    }
    return expected.peaks() ? double(found) / expected.peaks() : 1.0;
}
} // namespace

int main()
{
    auto analyzer = std::make_unique<fingerprint::Analyzer>();
    std::vector<uint8_t> pcm(kClipBytes);
    fingerprint::Signature signature;
    assert(!fingerprint::generate(nullptr, pcm.size(), *analyzer, signature));
    assert(!fingerprint::generate(pcm.data(), pcm.size() - 1, *analyzer, signature));
    assert(!fingerprint::generate(pcm.data(), 0, *analyzer, signature));
    assert(!fingerprint::generate(pcm.data(), 1001 * 256, *analyzer, signature));
    assert(fingerprint::generate(pcm.data(), pcm.size(), *analyzer, signature));
    assert(signature.peaks() == 0);

    // Full-scale input needs no gain: the upstream reference applies exactly.
    uint32_t random = 0x12345678;
    for (size_t i = 0; i < pcm.size() / 2; ++i) {
        random = 1664525u * random + 1013904223u;
        const uint16_t sample = static_cast<uint16_t>(int(random >> 16) - 32768);
        pcm[i * 2] = static_cast<uint8_t>(sample);
        pcm[i * 2 + 1] = static_cast<uint8_t>(sample >> 8);
    }
    assert(fingerprint::generate(pcm.data(), pcm.size(), *analyzer, signature));
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

    // Quiet audio: gain applied to the finished peaks must select the same
    // peaks as amplifying the samples first, as the legacy algorithm did.
    for (double scale : {0.06, 0.02}) {
        const auto quiet = noise(kClipBytes / 2, 0x9e3779b9, scale);
        int peak = 0;
        for (int16_t sample : quiet) peak = std::max(peak, std::abs(int(sample)));
        const double gain = std::max(1.0, std::min(32.0, 26000.0 / peak));
        std::vector<int16_t> amplified(quiet.size());
        for (size_t i = 0; i < quiet.size(); ++i) amplified[i] = int16_t(int(quiet[i] * gain));
        fingerprint::Signature legacy, scaled;
        const auto quiet_pcm = bytes(quiet.data(), quiet.size());
        const auto amplified_pcm = bytes(amplified.data(), amplified.size());
        assert(fingerprint::generate(amplified_pcm.data(), amplified_pcm.size(), *analyzer, legacy));
        assert(fingerprint::generate(quiet_pcm.data(), quiet_pcm.size(), *analyzer, scaled));
        assert(legacy.peaks() >= 255);
        // At the 32x cap the amplified copy is still quiet and gets gained
        // again here, so only peak positions remain comparable.
        const int tolerance = gain < 32 ? 2 : -1;
        assert(overlap(legacy, scaled, tolerance) >= 0.97 && overlap(scaled, legacy, tolerance) >= 0.97);
    }

    // A window of a continuous stream matches the same audio recorded alone,
    // except where the stream's real history replaces the recording's silence.
    const auto stream = noise(2500 * fingerprint::kHopSamples, 0x51ed270b);
    analyzer->reset();
    for (size_t hop = 0; hop < 2500; ++hop) analyzer->push(&stream[hop * fingerprint::kHopSamples]);
    assert(analyzer->hops() == 2500);
    fingerprint::Signature windowed, recorded;
    constexpr uint32_t first = 1900, last = 2400;
    assert(analyzer->signature(first, last, windowed));
    const auto clip = bytes(&stream[first * fingerprint::kHopSamples], (last - first) * fingerprint::kHopSamples);
    auto separate = std::make_unique<fingerprint::Analyzer>();
    assert(fingerprint::generate(clip.data(), clip.size(), *separate, recorded));
    assert(windowed.samples == recorded.samples && windowed.peaks() > 200);
    for (unsigned band = 0; band < 3; ++band) {
        std::vector<fingerprint::Peak> a, b;
        for (const auto &peak : windowed.bands[band]) if (peak.pass >= 61) a.push_back(peak);
        for (const auto &peak : recorded.bands[band]) if (peak.pass >= 61) b.push_back(peak);
        assert(a.size() == b.size());
        for (size_t i = 0; i < a.size(); ++i) assert(same_peak(a[i], b[i], 0));
    }
    // Windows must be recent, complete and no longer than eight seconds.
    assert(!analyzer->signature(400, 900, windowed));
    assert(!analyzer->signature(2400, 2501, windowed));
    assert(!analyzer->signature(1400, 2401, windowed));
    assert(!analyzer->signature(2000, 2000, windowed));
    assert(analyzer->signature(1500, 2500, windowed) && windowed.samples > 0);
    // Reset forgets history.
    analyzer->reset();
    assert(analyzer->hops() == 0 && !analyzer->signature(0, 1, windowed));

    // The platform FFT agrees with the portable reference.
    assert(fingerprint::self_test() < 1e-5);

    signature = {};
    signature.samples = 49664;
    signature.bands[0] = {{1, 12000, 2600}, {300, 14000, 3000}};
    signature.bands[2] = {{20, 16000, 22000}};
    assert(fingerprint::uri(signature) == kReferenceUri);
    signature.bands[0][1].pass = 0;
    assert(fingerprint::encode(signature).empty());
}
