// Native adaptation of ShazamIO 0.8.1's legacy algorithm and signature format.
// Copyright (c) 2021 dotX12. MIT license: third_party/ShazamIO-LICENSE.txt.
#include "fingerprint.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(ESP_PLATFORM)
#include "dsps_fft2r.h"
#include "esp_heap_caps.h"
#endif

namespace fingerprint {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr size_t kMaxPeaks = 4096;
constexpr double kMaxGain = 32;
const double kFloor = std::log(1.0 / 64);

void reference_fft(std::complex<float> *data, const std::complex<float> *twiddles)
{
    for (unsigned i = 1, reversed = 0; i < 2048; ++i) {
        unsigned bit = 1024;
        for (; reversed & bit; bit >>= 1) reversed ^= bit;
        reversed ^= bit;
        if (i < reversed) std::swap(data[i], data[reversed]);
    }
    for (unsigned length = 2; length <= 2048; length <<= 1) {
        for (unsigned start = 0; start < 2048; start += length) {
            for (unsigned i = 0; i < length / 2; ++i) {
                const auto odd = data[start + i + length / 2] * twiddles[i * 2048 / length];
                const auto even = data[start + i];
                data[start + i] = even + odd;
                data[start + i + length / 2] = even - odd;
            }
        }
    }
}

#if defined(ESP_PLATFORM)
bool platform_fft(std::complex<float> *data)
{
    // Keep the 8 KB twiddle table in PSRAM: internal RAM is needed for TLS.
    static float *table = static_cast<float *>(heap_caps_aligned_alloc(16, sizeof(float) * 2048,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    static bool ready = table != nullptr && dsps_fft2r_init_fc32(table, 2048) == ESP_OK;
    if (!ready) return false;
    float *values = reinterpret_cast<float *>(data);
    return dsps_fft2r_fc32(values, 2048) == ESP_OK && dsps_bit_rev_fc32(values, 2048) == ESP_OK;
}
#endif

void fill_twiddles(std::complex<float> *twiddles)
{
    for (unsigned i = 0; i < 1024; ++i) twiddles[i] = {
        float(std::cos(-2 * kPi * i / 2048)), float(std::sin(-2 * kPi * i / 2048))};
}

void append32(std::vector<uint8_t> &bytes, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
}
void put32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<uint8_t>(value >> (i * 8));
}
uint32_t crc32(const uint8_t *bytes, size_t size)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}
// Band of a peak once its levels are known; -1 if it is outside all three.
int band_of(double before, double after, double magnitude, unsigned bin, double &frequency)
{
    const double curvature = magnitude * 2 - before - after;
    if (curvature <= 0) return -1;
    frequency = bin * 64 + (after - before) * 32 / curvature;
    const double hz = frequency * 16000 / 2 / 1024 / 64;
    // The legacy protocol uses only these three frequency bands.
    return hz > 250 && hz < 520 ? 0 : hz > 520 && hz < 1450 ? 1 : hz > 1450 && hz < 3500 ? 2 : -1;
}
} // namespace

Analyzer::Analyzer()
{
    for (unsigned i = 0; i < 2048; ++i) window_[i] = float(0.5 - 0.5 * std::cos(2 * kPi * (i + 1) / 2049));
    fill_twiddles(twiddles_);
    reset();
}

void Analyzer::reset()
{
    hops_ = 0;
    candidate_count_ = 0;
    lost_through_ = -1;
    std::memset(samples_, 0, sizeof(samples_));
    std::memset(powers_, 0, sizeof(powers_));
    std::memset(spread_, 0, sizeof(spread_));
}

void Analyzer::transform()
{
#if defined(ESP_PLATFORM)
    if (platform_fft(fft_)) return;
#endif
    reference_fft(fft_, twiddles_);
}

void Analyzer::push(const int16_t *hop)
{
    // Hop numbers index fixed rings; restart long before they could wrap.
    if (hops_ >= 0x7fffffffu) reset();
    const unsigned position = (hops_ % 16) * kHopSamples;
    int peak = 0;
    for (unsigned i = 0; i < kHopSamples; ++i) {
        samples_[position + i] = hop[i];
        peak = std::max(peak, std::abs(int(hop[i])));
    }
    hop_peak_[hops_ % kHistoryHops] = uint16_t(std::min(peak, 32767));
    const unsigned start = (position + kHopSamples) % 2048;
    for (unsigned i = 0; i < 2048; ++i) fft_[i] = {samples_[(start + i) % 2048] * window_[i], 0};
    transform();
    const uint32_t pass = hops_;
    float *powers = powers_[pass % 256];
    float *spread = spread_[pass % 256];
    for (unsigned bin = 0; bin < 1025; ++bin) powers[bin] = std::norm(fft_[bin]) / 131072.0f;
    for (unsigned bin = 0; bin < 1025; ++bin) {
        const float value = bin < 1022 ? std::max({powers[bin], powers[bin + 1], powers[bin + 2]}) : powers[bin];
        float maximum = value;
        for (unsigned back : {1u, 3u, 6u}) {
            float &previous = spread_[(pass + 256 - back) % 256][bin];
            maximum = std::max(maximum, previous);
            previous = maximum;
        }
        spread[bin] = value;
    }
    ++hops_;
    if (hops_ >= 46) extract(hops_);
}

void Analyzer::extract(uint32_t pass)
{
    const float *original = powers_[(pass + 256 - 46) % 256];
    const float *neighbors = spread_[(pass + 256 - 49) % 256];
    for (unsigned bin = 10; bin < 1015; ++bin) {
        const float power = original[bin];
        // No gain can lift this to the legacy 1/64 threshold.
        if (power * float(kMaxGain * kMaxGain) < 1.0f / 64 || power < neighbors[bin - 1]) continue;
        float maximum = 0;
        for (int delta : {-10, -7, -4, -3, 1, 2, 5, 8}) maximum = std::max(maximum, neighbors[int(bin) + delta]);
        if (power <= maximum) continue;
        for (int delta : {-53, -45, 165, 172, 179, 186, 193, 200, 214, 221, 228, 235, 242, 249}) {
            const unsigned index = unsigned(int(pass % 256) + 256 + delta) % 256;
            maximum = std::max(maximum, spread_[index][bin - 1]);
        }
        if (power <= maximum) continue;
        const auto level = [](float value) { return std::log(std::max(1e-30, double(value))); };
        Candidate candidate{pass - 46, uint16_t(bin), level(power), level(original[bin - 1]), level(original[bin + 1])};
        // Neighbours at or above 1/64 are never floored, so the band is final.
        double frequency = 0;
        if (original[bin - 1] >= 1.0f / 64 && original[bin + 1] >= 1.0f / 64
            && band_of(candidate.before, candidate.after, candidate.power, bin, frequency) < 0) continue;
        Candidate &slot = candidates_[candidate_count_ % kCandidates];
        if (candidate_count_ >= kCandidates) lost_through_ = std::max<int64_t>(lost_through_, slot.hop);
        slot = candidate;
        ++candidate_count_;
    }
}

bool Analyzer::signature(uint32_t first, uint32_t last, Signature &output) const
{
    output = {};
    if (first >= last || last > hops_ || last - first > kMaxSignatureHops
        || hops_ - first > kHistoryHops || int64_t(first) <= lost_through_) return false;
    int peak_sample = 0;
    for (uint32_t hop = first; hop < last; ++hop) peak_sample = std::max<int>(peak_sample, hop_peak_[hop % kHistoryHops]);
    if (peak_sample == 0) return true;
    const double gain = std::max(1.0, std::min(kMaxGain, 26000.0 / peak_sample));
    const double shift = 2 * std::log(gain);
    // Oldest stored candidate that could belong to this clip.
    size_t index = candidate_count_ > kCandidates ? candidate_count_ - kCandidates : 0;
    while (index < candidate_count_ && candidates_[index % kCandidates].hop < first) ++index;
    // Pass counts hops heard so far, so the hop 46 passes back is now final.
    for (uint32_t pass = 1; first + pass <= last; ++pass) {
        output.samples += kHopSamples;
        if (pass >= 46) {
            const uint32_t hop = first + pass - 46;
            for (; index < candidate_count_ && candidates_[index % kCandidates].hop == hop; ++index) {
                const Candidate &candidate = candidates_[index % kCandidates];
                if (candidate.power + shift < kFloor) continue;
                const double magnitude = std::max(kFloor, candidate.power + shift) * 1477.3 + 6144;
                const double before = std::max(kFloor, candidate.before + shift) * 1477.3 + 6144;
                const double after = std::max(kFloor, candidate.after + shift) * 1477.3 + 6144;
                double frequency = 0;
                const int band = band_of(before, after, magnitude, candidate.bin, frequency);
                if (band < 0 || magnitude < 0 || magnitude > 65535 || frequency < 0 || frequency > 65535) continue;
                if (output.peaks() >= kMaxPeaks) { output = {}; return false; }
                output.bands[band].push_back({pass - 46, uint16_t(magnitude), uint16_t(frequency)});
            }
        }
        if (output.samples >= 49600 && output.peaks() >= 255) break;
    }
    return true;
}

bool generate(const uint8_t *pcm, size_t bytes, Analyzer &analyzer, Signature &output)
{
    output = {};
    constexpr size_t kHopBytes = kHopSamples * 2;
    if (pcm == nullptr || bytes == 0 || bytes % kHopBytes != 0 || bytes / kHopBytes > kMaxSignatureHops) return false;
    analyzer.reset();
    int16_t hop[kHopSamples];
    for (size_t offset = 0; offset < bytes; offset += kHopBytes) {
        for (unsigned i = 0; i < kHopSamples; ++i) {
            hop[i] = int16_t(uint16_t(pcm[offset + i * 2] | (unsigned(pcm[offset + i * 2 + 1]) << 8)));
        }
        analyzer.push(hop);
    }
    return analyzer.signature(0, analyzer.hops(), output);
}

double self_test()
{
    std::complex<float> twiddles[1024];
    fill_twiddles(twiddles);
    alignas(16) static std::complex<float> expected[2048], actual[2048];
    uint32_t random = 0x2468ace1;
    for (unsigned i = 0; i < 2048; ++i) {
        random = 1664525u * random + 1013904223u;
        const float sample = float(int(random >> 16) - 32768) * float(0.5 - 0.5 * std::cos(2 * kPi * (i + 1) / 2049));
        expected[i] = actual[i] = {sample, 0};
    }
    reference_fft(expected, twiddles);
#if defined(ESP_PLATFORM)
    if (!platform_fft(actual)) return INFINITY;
#else
    reference_fft(actual, twiddles);
#endif
    double largest = 0, error = 0;
    for (unsigned bin = 0; bin < 1025; ++bin) largest = std::max(largest, double(std::norm(expected[bin])));
    for (unsigned bin = 0; bin < 1025; ++bin) {
        error = std::max(error, std::abs(double(std::norm(expected[bin])) - double(std::norm(actual[bin]))) / largest);
    }
    return error;
}

std::vector<uint8_t> encode(const Signature &signature)
{
    if (signature.samples > 128000 || signature.peaks() > kMaxPeaks) return {};
    std::vector<uint8_t> bytes(48, 0);
    put32(bytes, 0, 0xcafe2580); put32(bytes, 12, 0x94119c00);
    put32(bytes, 28, 3u << 27); put32(bytes, 40, signature.samples + 3840);
    put32(bytes, 44, 0x7c0000);
    append32(bytes, 0x40000000); append32(bytes, 0);
    for (unsigned band = 0; band < signature.bands.size(); ++band) {
        if (signature.bands[band].empty()) continue;
        append32(bytes, 0x60030040 + band);
        const size_t length_offset = bytes.size(); append32(bytes, 0);
        const size_t start = bytes.size();
        uint32_t previous = 0;
        for (const Peak &peak : signature.bands[band]) {
            if (peak.pass < previous || peak.pass > signature.samples / 128) return {};
            if (peak.pass - previous >= 255) { bytes.push_back(255); append32(bytes, peak.pass); previous = peak.pass; }
            bytes.push_back(static_cast<uint8_t>(peak.pass - previous));
            bytes.push_back(static_cast<uint8_t>(peak.magnitude)); bytes.push_back(static_cast<uint8_t>(peak.magnitude >> 8));
            bytes.push_back(static_cast<uint8_t>(peak.frequency)); bytes.push_back(static_cast<uint8_t>(peak.frequency >> 8));
            previous = peak.pass;
        }
        put32(bytes, length_offset, bytes.size() - start);
        while (bytes.size() % 4) bytes.push_back(0);
    }
    put32(bytes, 8, bytes.size() - 48); put32(bytes, 52, bytes.size() - 48);
    put32(bytes, 4, crc32(bytes.data() + 8, bytes.size() - 8));
    return bytes;
}

std::string uri(const Signature &signature)
{
    const auto bytes = encode(signature);
    if (bytes.empty()) return {};
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result = "data:audio/vnd.shazam.sig;base64,";
    for (size_t offset = 0; offset < bytes.size(); offset += 3) {
        const unsigned value = (unsigned(bytes[offset]) << 16)
            | (offset + 1 < bytes.size() ? unsigned(bytes[offset + 1]) << 8 : 0)
            | (offset + 2 < bytes.size() ? bytes[offset + 2] : 0);
        result += alphabet[(value >> 18) & 63]; result += alphabet[(value >> 12) & 63];
        result += offset + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '=';
        result += offset + 2 < bytes.size() ? alphabet[value & 63] : '=';
    }
    return result;
}
} // namespace fingerprint
