// Native adaptation of ShazamIO 0.8.1's legacy algorithm and signature format.
// Copyright (c) 2021 dotX12. MIT license: third_party/ShazamIO-LICENSE.txt.
#include "fingerprint.h"
#include "audio_capture.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fingerprint {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr size_t kMaxPeaks = 4096;

int sample(const uint8_t *pcm, size_t index)
{
    const unsigned value = pcm[index * 2] | (unsigned(pcm[index * 2 + 1]) << 8);
    return value >= 32768 ? int(value) - 65536 : int(value);
}

void fft(Workspace &w)
{
    for (unsigned i = 1, reversed = 0; i < 2048; ++i) {
        unsigned bit = 1024;
        for (; reversed & bit; bit >>= 1) reversed ^= bit;
        reversed ^= bit;
        if (i < reversed) std::swap(w.fft[i], w.fft[reversed]);
    }
    for (unsigned length = 2; length <= 2048; length <<= 1) {
        for (unsigned start = 0; start < 2048; start += length) {
            for (unsigned i = 0; i < length / 2; ++i) {
                const auto odd = w.fft[start + i + length / 2] * w.twiddles[i * 2048 / length];
                const auto even = w.fft[start + i];
                w.fft[start + i] = even + odd;
                w.fft[start + i + length / 2] = even - odd;
            }
        }
    }
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
}

bool generate(const uint8_t *pcm, size_t bytes, Workspace &w, Signature &output, void (*yield)())
{
    output = {};
    if (pcm == nullptr || bytes != audio_capture::kBytes) return false;
    std::memset(w.powers, 0, sizeof(w.powers));
    std::memset(w.spread, 0, sizeof(w.spread));
    std::memset(w.samples, 0, sizeof(w.samples));
    for (unsigned i = 0; i < 2048; ++i) w.window[i] = float(0.5 - 0.5 * std::cos(2 * kPi * (i + 1) / 2049));
    for (unsigned i = 0; i < 1024; ++i) w.twiddles[i] = {
        float(std::cos(-2 * kPi * i / 2048)), float(std::sin(-2 * kPi * i / 2048))};
    int peak_sample = 0;
    for (size_t i = 0; i < bytes / 2; ++i) peak_sample = std::max(peak_sample, std::abs(sample(pcm, i)));
    if (peak_sample == 0) return true;
    const double gain = std::max(1.0, std::min(32.0, 26000.0 / peak_sample));
    unsigned position = 0, pass = 0;
    for (size_t offset = 0; offset < bytes / 2; offset += 128) {
        for (unsigned i = 0; i < 128; ++i) w.samples[position + i] = float(int(sample(pcm, offset + i) * gain));
        position = (position + 128) % 2048;
        for (unsigned i = 0; i < 2048; ++i) w.fft[i] = {w.samples[(position + i) % 2048] * w.window[i], 0};
        fft(w);
        float *powers = w.powers[pass % 256];
        float *spread = w.spread[pass % 256];
        for (unsigned bin = 0; bin < 1025; ++bin) powers[bin] = std::max(1e-10f, std::norm(w.fft[bin]) / 131072.0f);
        for (unsigned bin = 0; bin < 1025; ++bin) {
            const float value = bin < 1022 ? std::max({powers[bin], powers[bin + 1], powers[bin + 2]}) : powers[bin];
            float maximum = value;
            for (unsigned back : {1u, 3u, 6u}) {
                float &previous = w.spread[(pass + 256 - back) % 256][bin];
                maximum = std::max(maximum, previous);
                previous = maximum;
            }
            spread[bin] = value;
        }
        ++pass;
        output.samples += 128;
        if (pass >= 46) {
            const float *original = w.powers[(pass + 256 - 46) % 256];
            const float *neighbors = w.spread[(pass + 256 - 49) % 256];
            for (unsigned bin = 10; bin < 1015; ++bin) {
                const float power = original[bin];
                if (power < 1.0f / 64 || power < neighbors[bin - 1]) continue;
                float maximum = 0;
                for (int delta : {-10, -7, -4, -3, 1, 2, 5, 8}) maximum = std::max(maximum, neighbors[int(bin) + delta]);
                if (power <= maximum) continue;
                for (int delta : {-53, -45, 165, 172, 179, 186, 193, 200, 214, 221, 228, 235, 242, 249}) {
                    const unsigned index = unsigned(int(pass % 256) + 256 + delta) % 256;
                    maximum = std::max(maximum, w.spread[index][bin - 1]);
                }
                if (power <= maximum) continue;
                const double magnitude = std::log(std::max(1.0 / 64, double(power))) * 1477.3 + 6144;
                const double before = std::log(std::max(1.0 / 64, double(original[bin - 1]))) * 1477.3 + 6144;
                const double after = std::log(std::max(1.0 / 64, double(original[bin + 1]))) * 1477.3 + 6144;
                const double curvature = magnitude * 2 - before - after;
                if (curvature <= 0) continue;
                const double frequency = bin * 64 + (after - before) * 32 / curvature;
                const double hz = frequency * 16000 / 2 / 1024 / 64;
                // The legacy protocol uses only these three frequency bands.
                const int band = hz > 250 && hz < 520 ? 0 : hz > 520 && hz < 1450 ? 1
                    : hz > 1450 && hz < 3500 ? 2 : -1;
                if (band < 0 || magnitude < 0 || magnitude > 65535 || frequency < 0 || frequency > 65535) continue;
                if (output.peaks() >= kMaxPeaks) { output = {}; return false; }
                output.bands[band].push_back({pass - 46, uint16_t(magnitude), uint16_t(frequency)});
            }
        }
        if (yield != nullptr && pass % 8 == 0) yield();
        if (output.samples >= 49600 && output.peaks() >= 255) break;
    }
    return true;
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
