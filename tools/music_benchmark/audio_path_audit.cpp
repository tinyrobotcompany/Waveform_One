// Offline measurement of the production resamplers. Generates numeric signals
// in memory; never records a microphone, plays sound or calls a service.
#include <cmath>
#include <cstdio>
#include <vector>
#include "capture_samples.h"
#include "acrcloud.h"

double rms(const std::vector<int16_t> &samples, size_t skip) {
    double energy = 0;
    for (size_t i = skip; i < samples.size() - skip; ++i)
        energy += double(samples[i]) * samples[i];
    return std::sqrt(energy / (samples.size() - 2 * skip));
}
int main() {
    constexpr double pi = 3.14159265358979323846;
    constexpr double amplitude = 3276.8; // -20 dBFS peak, avoids clipping.
    std::puts("input_hz,samples_16k,samples_8k,gain_48_to_16_db,gain_48_to_8_db");
    // Avoid output DC/Nyquist frequencies, whose sampled sine amplitude is
    // phase-dependent and would misrepresent the filter response.
    for (int hz : {100, 1000, 3000, 3500, 3900, 4100, 4500, 5100, 6500, 7900, 8100, 10100, 15900}) {
        capture::Samples decimator;
        std::vector<int16_t> pcm;
        pcm.reserve(16000);
        for (int i = 0; i < 48000; ++i) {
            const int32_t word = int32_t(amplitude * 65536.0 * std::sin(2 * pi * hz * i / 48000));
            decimator.push(word, [&](int16_t value) { pcm.push_back(value); });
        }
        std::vector<int16_t> reduced(pcm.size() / 2);
        acrcloud::downsample(pcm.data(), pcm.size(), reduced.data());
        const double reference = amplitude / std::sqrt(2.0);
        std::printf("%d,%zu,%zu,%.3f,%.3f\n", hz, pcm.size(), reduced.size(),
            20 * std::log10(std::max(rms(pcm, 160), 1e-9) / reference),
            20 * std::log10(std::max(rms(reduced, 80), 1e-9) / reference));
    }
}
