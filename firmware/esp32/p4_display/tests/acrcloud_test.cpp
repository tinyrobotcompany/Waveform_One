#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "acrcloud.h"

namespace {
std::string hex(const std::array<uint8_t, 20> &digest)
{
    std::string text; char pair[3];
    for (uint8_t byte : digest) { std::snprintf(pair, sizeof(pair), "%02x", byte); text += pair; }
    return text;
}
double rms(const std::vector<int16_t> &samples, size_t skip)
{
    double sum = 0;
    for (size_t i = skip; i < samples.size() - skip; ++i) sum += double(samples[i]) * samples[i];
    return std::sqrt(sum / (samples.size() - 2 * skip));
}
} // namespace

int main()
{
    using namespace acrcloud;
    // Standard vectors: FIPS 180 and RFC 2202.
    assert(hex(sha1(reinterpret_cast<const uint8_t *>("abc"), 3)) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    assert(hex(sha1(nullptr, 0)) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    const std::string many(1000, 'a');
    assert(hex(sha1(reinterpret_cast<const uint8_t *>(many.data()), many.size())) == "291e9a6c66994949b57ba5e650361e98fc36b1ba");
    assert(hex(hmac_sha1("Jefe", "what do ya want for nothing?")) == "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
    assert(hex(hmac_sha1(std::string(80, '\xaa'), "Test Using Larger Than Block-Size Key - Hash Key First"))
           == "aa4ae5e15272d00e95705637ce8a3b55ed402112");
    const uint8_t text[] = {'M', 'a', 'n'};
    assert(base64(text, 3) == "TWFu" && base64(text, 2) == "TWE=" && base64(text, 1) == "TQ==");

    // Matches Python's hmac/base64 for the documented string to sign.
    Credentials credentials{"identify-eu-west-1.acrcloud.com", "key123", "secret456"};
    assert(credentials.valid() && !Credentials{}.valid());
    assert(signature(credentials, "1791800000") == "0pBcK7D3pj3N9loSIFJI1uk9h48=");

    // Down-sampling keeps 1 kHz and removes 6 kHz, which would alias.
    for (double hz : {1000.0, 6000.0}) {
        std::vector<int16_t> input(16000), output(8000);
        for (size_t i = 0; i < input.size(); ++i) input[i] = int16_t(10000 * std::sin(2 * M_PI * hz * i / 16000));
        downsample(input.data(), input.size(), output.data());
        const double gain = rms(output, 16) / (10000 / std::sqrt(2.0));
        assert(hz < 2000 ? std::abs(gain - 1) < 0.02 : gain < 0.05);
    }

    // Multipart body: signed fields, then an 8 kHz mono WAV.
    const int16_t samples[4] = {1, -2, 3, -32768};
    const auto bytes = body(credentials, "1791800000", samples, 4, "BOUNDARY");
    const std::string all(bytes.begin(), bytes.end());
    assert(all.find("--BOUNDARY\r\nContent-Disposition: form-data; name=\"access_key\"\r\n\r\nkey123\r\n") == 0);
    assert(all.find("name=\"sample_bytes\"\r\n\r\n52\r\n") != std::string::npos);
    assert(all.find("name=\"signature\"\r\n\r\n0pBcK7D3pj3N9loSIFJI1uk9h48=\r\n") != std::string::npos);
    assert(all.find("name=\"data_type\"\r\n\r\naudio\r\n") != std::string::npos);
    assert(all.find("secret456") == std::string::npos);
    const size_t wav = all.find("RIFF");
    assert(wav != std::string::npos && all.compare(wav + 8, 8, "WAVEfmt ") == 0);
    assert(uint8_t(all[wav + 24]) == 0x40 && uint8_t(all[wav + 25]) == 0x1f);  // 8000 Hz
    assert(uint8_t(all[wav + 44]) == 1 && uint8_t(all[wav + 50]) == 0x00 && uint8_t(all[wav + 51]) == 0x80);
    assert(all.substr(all.size() - 16) == "\r\n--BOUNDARY--\r\n");

    // Responses. The longest match wins over a short chance match.
    RecognitionTrack track{};
    cJSON *root = cJSON_Parse(R"J({"status":{"code":0,"msg":"Success"},"metadata":{"music":[
        {"title":"Chance","artists":[{"name":"Other"}],"album":{"name":"X"},"score":100,
         "db_begin_time_offset_ms":73960,"sample_begin_time_offset_ms":1760,"sample_end_time_offset_ms":3380},
        {"title":"Whole Lotta Love (1990 Remaster)","artists":[{"name":"Led Zeppelin"},{"name":"Other"}],
         "album":{"name":"Led Zeppelin II"},"score":97,"duration_ms":334000,
         "db_begin_time_offset_ms":29160,"sample_begin_time_offset_ms":0,"sample_end_time_offset_ms":3000}]}})J");
    assert(parse(root, track) == Result::Matched);
    assert(std::strcmp(track.title, "Whole Lotta Love (1990 Remaster)") == 0);
    assert(std::strcmp(track.artist, "Led Zeppelin") == 0 && std::strcmp(track.album, "Led Zeppelin II") == 0);
    assert(track.duration_s == 334 && track.offset_s == 29.16);
    cJSON_Delete(root);
    root = cJSON_Parse(R"({"status":{"code":1001,"msg":"No result"}})");
    assert(parse(root, track) == Result::NoMatch && track.title[0] == '\0');
    cJSON_Delete(root);
    for (const char *limited : {R"({"status":{"code":3003}})", R"({"status":{"code":3015}})"}) {
        root = cJSON_Parse(limited); assert(parse(root, track) == Result::Limited); cJSON_Delete(root);
    }
    for (const char *bad : {R"({"status":{"code":3001}})", R"({"status":{}})", R"({"metadata":{}})"}) {
        root = cJSON_Parse(bad); assert(parse(root, track) == Result::Failed); cJSON_Delete(root);
    }
    // Unsafe text is refused rather than shown.
    root = cJSON_Parse(R"({"status":{"code":0},"metadata":{"music":[{"title":"bad\u0007title"}]}})");
    assert(parse(root, track) == Result::NoMatch);
    cJSON_Delete(root);
}
