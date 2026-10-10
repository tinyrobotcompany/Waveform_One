#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "audio_capture.h"

namespace {
std::string packet(unsigned id, unsigned sequence)
{
    std::string payload;
    uint32_t hash = 2166136261u;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < 256; ++i) {
        const uint8_t byte = static_cast<uint8_t>(i + sequence);
        payload += hex[byte >> 4];
        payload += hex[byte & 15];
        hash = (hash ^ byte) * 16777619u;
    }
    char checksum[9];
    std::snprintf(checksum, sizeof(checksum), "%08x", hash);
    return "WF1 " + std::to_string(id) + " PCM " + std::to_string(sequence)
        + " " + payload + " " + checksum + "\n";
}
}

int main()
{
    using namespace audio_capture;
    std::vector<uint8_t> pcm(kBytes + 2, 0xa5);
    Decoder capture;
    auto feed = [&](std::string_view bytes) {
        for (char byte : bytes) capture.push(byte);
    };
    assert(capture_request(42) == "WF1 42 CAPTURE\n");
    assert(capture_request(0).empty());
    assert(capture_request(65536).empty());
    assert(!capture.begin(0, pcm.data(), kBytes));
    assert(!capture.begin(42, nullptr, kBytes));
    assert(!capture.begin(42, pcm.data(), kBytes - 1));
    assert(capture.begin(42, pcm.data() + 1, kBytes));
    assert(!capture.begin(43, pcm.data() + 1, kBytes));
    feed("boot diagnostic\nWF1 41 AUDIO 16000 128000\nWF1 42 AUD");
    assert(capture.result() == Result::Receiving);
    feed("IO 16000 128000\r\n");
    for (unsigned sequence = 0; sequence < kPackets; ++sequence) {
        feed("SHUT RMS=0.004\nWF1 7 OK MODE waterfall\n");
        feed(packet(42, sequence));
    }
    assert(capture.result() == Result::Receiving);
    assert(capture.size() == kBytes);
    feed("WF1 42 END 1000\n");
    assert(capture.result() == Result::Complete);
    assert(pcm.front() == 0xa5 && pcm.back() == 0xa5);
    for (unsigned sequence = 0; sequence < kPackets; ++sequence)
        for (unsigned i = 0; i < 256; ++i)
            assert(pcm[1 + sequence * 256 + i] == static_cast<uint8_t>(i + sequence));
    feed(packet(42, 0));
    assert(capture.result() == Result::Complete);

    auto reset = [&] { capture.reset(); assert(capture.begin(42, pcm.data() + 1, kBytes)); };
    reset(); feed(packet(42, 0)); assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 AUDIO 44100 128000\n"); assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 AUDIO 16000 128000\nWF1 42 AUDIO 16000 128000\n");
    assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 AUDIO 16000 128000\n"); feed(packet(42, 1));
    assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 AUDIO 16000 128000\n"); feed(packet(42, 0)); feed(packet(42, 0));
    assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 AUDIO 16000 128000\n");
    auto corrupt = packet(42, 0); corrupt[corrupt.size() - 2] = corrupt[corrupt.size() - 2] == '0' ? '1' : '0';
    feed(corrupt); assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 AUDIO 16000 128000\nWF1 42 END 1000\n");
    assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 ERR BUSY\n"); assert(capture.result() == Result::Busy);
    reset(); feed("WF1 42 ERR AUDIO_LOST\n"); assert(capture.result() == Result::AudioLost);
    reset(); feed("WF1 42 ERR UNKNOWN\n"); assert(capture.result() == Result::Invalid);
    reset(); feed(std::string(1000, 'x') + "\nWF1 42 AUDIO 16000 128000\n");
    feed(packet(42, 0)); assert(capture.size() == 256);
    reset(); feed("WF1 42 AUDIO 16000 128000\n");
    corrupt = packet(42, 0); corrupt[30] = 'z'; feed(corrupt);
    assert(capture.result() == Result::Invalid);
    reset(); feed("WF1 42 AUDIO 16000 128000\nWF1 42 PCM 0 " + std::string(900, '0') + "\n");
    assert(capture.result() == Result::Invalid);
    capture.reset(); assert(capture.result() == Result::Idle && capture.size() == 0);
}
