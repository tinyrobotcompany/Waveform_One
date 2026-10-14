#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include "cJSON.h"
#include "recognition.h"
#include "recognition_metadata.h"

// ACRCloud identification (https://docs.acrcloud.com/reference/identification-api):
// a signed multipart POST of a short audio clip. ACRCloud fingerprints at
// 8 kHz mono, so the clip is sent as an 8 kHz 16-bit mono WAV.
namespace acrcloud {

struct Credentials {
    const char *host = "";
    const char *access_key = "";
    const char *access_secret = "";
    bool valid() const { return host[0] && access_key[0] && access_secret[0]; }
};

inline std::array<uint8_t, 20> sha1(const uint8_t *data, size_t size)
{
    uint32_t h[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    const auto rotate = [](uint32_t value, unsigned bits) { return (value << bits) | (value >> (32 - bits)); };
    const uint64_t bits = uint64_t(size) * 8;
    const size_t padded = ((size + 8) / 64 + 1) * 64;
    uint8_t block[64];
    for (size_t offset = 0; offset < padded; offset += 64) {
        for (size_t i = 0; i < 64; ++i) {
            const size_t at = offset + i;
            block[i] = at < size ? data[at] : at == size ? 0x80
                : at >= padded - 8 ? uint8_t(bits >> (8 * (padded - 1 - at))) : 0;
        }
        uint32_t w[80];
        for (unsigned i = 0; i < 16; ++i) {
            w[i] = uint32_t(block[i * 4]) << 24 | uint32_t(block[i * 4 + 1]) << 16
                | uint32_t(block[i * 4 + 2]) << 8 | block[i * 4 + 3];
        }
        for (unsigned i = 16; i < 80; ++i) w[i] = rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (unsigned i = 0; i < 80; ++i) {
            const uint32_t f = i < 20 ? (b & c) | (~b & d) : i < 40 || i >= 60 ? b ^ c ^ d : (b & c) | (b & d) | (c & d);
            const uint32_t k = i < 20 ? 0x5a827999 : i < 40 ? 0x6ed9eba1 : i < 60 ? 0x8f1bbcdc : 0xca62c1d6;
            const uint32_t next = rotate(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotate(b, 30); b = a; a = next;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::array<uint8_t, 20> digest{};
    for (unsigned i = 0; i < 20; ++i) digest[i] = uint8_t(h[i / 4] >> (24 - 8 * (i % 4)));
    return digest;
}

inline std::array<uint8_t, 20> hmac_sha1(std::string_view key, std::string_view message)
{
    std::array<uint8_t, 64> block{};
    if (key.size() > 64) {
        const auto hashed = sha1(reinterpret_cast<const uint8_t *>(key.data()), key.size());
        std::memcpy(block.data(), hashed.data(), hashed.size());
    } else {
        std::memcpy(block.data(), key.data(), key.size());
    }
    std::vector<uint8_t> inner(64 + message.size()), outer(64 + 20);
    for (unsigned i = 0; i < 64; ++i) { inner[i] = block[i] ^ 0x36; outer[i] = block[i] ^ 0x5c; }
    std::memcpy(inner.data() + 64, message.data(), message.size());
    const auto first = sha1(inner.data(), inner.size());
    std::memcpy(outer.data() + 64, first.data(), first.size());
    return sha1(outer.data(), outer.size());
}

inline std::string base64(const uint8_t *bytes, size_t size)
{
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (size_t offset = 0; offset < size; offset += 3) {
        const unsigned value = (unsigned(bytes[offset]) << 16)
            | (offset + 1 < size ? unsigned(bytes[offset + 1]) << 8 : 0)
            | (offset + 2 < size ? bytes[offset + 2] : 0);
        result += alphabet[(value >> 18) & 63]; result += alphabet[(value >> 12) & 63];
        result += offset + 1 < size ? alphabet[(value >> 6) & 63] : '=';
        result += offset + 2 < size ? alphabet[value & 63] : '=';
    }
    return result;
}

inline std::string signature(const Credentials &credentials, const std::string &timestamp)
{
    const std::string text = std::string("POST\n/v1/identify\n") + credentials.access_key
        + "\naudio\n1\n" + timestamp;
    const auto digest = hmac_sha1(credentials.access_secret, text);
    return base64(digest.data(), digest.size());
}

// 16 kHz to 8 kHz: a 31-tap half-band low-pass (cut-off 3.6 kHz), then every
// second sample. Output holds count / 2 samples.
inline void downsample(const int16_t *input, size_t count, int16_t *output)
{
    static constexpr float taps[16] = {  // centre first; odd offsets only, others are zero
        0.5f, 0.3163f, 0.0f, -0.1009f, 0.0f, 0.0545f, 0.0f, -0.0330f,
        0.0f, 0.0205f, 0.0f, -0.0124f, 0.0f, 0.0070f, 0.0f, -0.0035f};
    for (size_t out = 0; out < count / 2; ++out) {
        const long centre = long(out * 2);
        float sum = taps[0] * input[centre];
        for (long k = 1; k < 16; k += 2) {
            const long before = centre - k, after = centre + k;
            const float a = before >= 0 ? input[before] : 0.0f;
            const float b = after < long(count) ? input[after] : 0.0f;
            sum += taps[k] * (a + b);
        }
        output[out] = int16_t(sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum);
    }
}

// The complete multipart body: form fields, then the WAV file.
inline std::vector<uint8_t> body(const Credentials &credentials, const std::string &timestamp,
                                 const int16_t *samples_8k, size_t count, const std::string &boundary)
{
    const uint32_t data = uint32_t(count * 2);
    std::vector<uint8_t> wav(44 + data);
    const auto put32 = [&](size_t at, uint32_t value) { for (unsigned i = 0; i < 4; ++i) wav[at + i] = uint8_t(value >> (8 * i)); };
    const auto put16 = [&](size_t at, uint16_t value) { wav[at] = uint8_t(value); wav[at + 1] = uint8_t(value >> 8); };
    const auto text = [&](size_t at, std::string_view value) { for (char c : value) wav[at++] = uint8_t(c); };
    text(0, "RIFF"); put32(4, 36 + data); text(8, "WAVEfmt ");
    put32(16, 16); put16(20, 1); put16(22, 1); put32(24, 8000); put32(28, 16000); put16(32, 2); put16(34, 16);
    text(36, "data"); put32(40, data);
    for (size_t i = 0; i < count; ++i) put16(44 + i * 2, uint16_t(samples_8k[i]));

    std::string head;
    const auto field = [&](const char *name, const std::string &value) {
        head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + name + "\"\r\n\r\n" + value + "\r\n";
    };
    field("access_key", credentials.access_key);
    field("sample_bytes", std::to_string(wav.size()));
    field("timestamp", timestamp);
    field("signature", signature(credentials, timestamp));
    field("data_type", "audio");
    field("signature_version", "1");
    head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"sample\"; filename=\"clip.wav\"\r\n"
            "Content-Type: audio/wav\r\n\r\n";
    const std::string tail = "\r\n--" + boundary + "--\r\n";
    std::vector<uint8_t> output(head.begin(), head.end());
    output.insert(output.end(), wav.begin(), wav.end());
    output.insert(output.end(), tail.begin(), tail.end());
    return output;
}

enum class Result { Matched, NoMatch, Limited, Failed };

// Preserve the primary candidate when confidence and coverage are close.
// Sampled remixes can cover a few more milliseconds than the original and
// must not replace it on that numerical detail. Meaningfully longer coverage
// can still beat a short incidental match at comparable confidence.
inline Result parse(const cJSON *root, RecognitionTrack &track)
{
    using recognition_metadata::field;
    track = {};
    const cJSON *code = field(field(root, "status"), "code");
    if (!cJSON_IsNumber(code)) return Result::Failed;
    if (code->valueint == 1001) return Result::NoMatch;
    if (code->valueint == 3003 || code->valueint == 3015) return Result::Limited;
    if (code->valueint != 0) return Result::Failed;
    const cJSON *best = nullptr;
    double best_length = -1, best_score = -1;
    const cJSON *music = nullptr;
    cJSON_ArrayForEach(music, field(field(root, "metadata"), "music")) {
        const cJSON *begin = field(music, "sample_begin_time_offset_ms");
        const cJSON *end = field(music, "sample_end_time_offset_ms");
        const cJSON *score = field(music, "score");
        const double length = cJSON_IsNumber(begin) && cJSON_IsNumber(end) ? end->valuedouble - begin->valuedouble : 0;
        const double value = cJSON_IsNumber(score) ? score->valuedouble : 0;
        if (!std::isfinite(length) || length < 0 || !std::isfinite(value) || value < 0 || value > 100) continue;
        if (best == nullptr || value > best_score + 3
            || (std::abs(value - best_score) <= 3 && length > best_length + 500)) {
            best = music; best_length = length; best_score = value;
        }
    }
    if (best == nullptr || !recognition_metadata::text(field(best, "title"), track.title)) { track = {}; return Result::NoMatch; }
    recognition_metadata::text(field(cJSON_GetArrayItem(field(best, "artists"), 0), "name"), track.artist);
    recognition_metadata::text(field(field(best, "album"), "name"), track.album);
    const cJSON *duration = field(best, "duration_ms");
    if (cJSON_IsNumber(duration) && duration->valuedouble > 0) track.duration_s = duration->valuedouble / 1000;
    const cJSON *matched_begin = field(best, "sample_begin_time_offset_ms");
    const cJSON *matched_end = field(best, "sample_end_time_offset_ms");
    if (cJSON_IsNumber(matched_begin) && cJSON_IsNumber(matched_end) &&
        std::isfinite(matched_begin->valuedouble) && std::isfinite(matched_end->valuedouble) &&
        matched_begin->valuedouble >= 0 && matched_end->valuedouble > matched_begin->valuedouble) {
        track.sample_begin_ms = matched_begin->valuedouble;
        track.sample_end_ms = matched_end->valuedouble;
    }
    const cJSON *confidence = field(best, "score");
    if (cJSON_IsNumber(confidence)) track.match_score = confidence->valuedouble;
    // Song position at the clip's start.
    const cJSON *db = field(best, "db_begin_time_offset_ms");
    const cJSON *sample = field(best, "sample_begin_time_offset_ms");
    if (cJSON_IsNumber(db) && cJSON_IsNumber(sample)) {
        const double offset = (db->valuedouble - sample->valuedouble) / 1000;
        if (offset >= 0 && offset < 36000) track.offset_s = offset;
    }
    return Result::Matched;
}

} // namespace acrcloud
