#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "audio_stream.h"

namespace {
std::string packet(unsigned id, unsigned sequence, int16_t base)
{
    uint8_t bytes[audio_stream::kPacketBytes];
    for (unsigned i = 0; i < audio_stream::kPacketSamples; ++i) {
        const auto value = uint16_t(int16_t(base + int(i)));
        bytes[i * 2] = uint8_t(value);
        bytes[i * 2 + 1] = uint8_t(value >> 8);
    }
    uint32_t hash = 2166136261u;
    std::string hex;
    char pair[3];
    for (uint8_t byte : bytes) {
        hash = (hash ^ byte) * 16777619u;
        std::snprintf(pair, sizeof(pair), "%02x", byte);
        hex += pair;
    }
    char tail[16];
    std::snprintf(tail, sizeof(tail), " %08x\n", static_cast<unsigned>(hash));
    return "\nWF1 " + std::to_string(id) + " PCM " + std::to_string(sequence) + " " + hex + tail;
}

struct Fixture {
    audio_stream::Decoder decoder;
    std::vector<int16_t> received;
    void feed(const std::string &text)
    {
        for (char c : text) decoder.push(c, [&](const int16_t *samples) {
            received.insert(received.end(), samples, samples + audio_stream::kPacketSamples);
        });
    }
};
} // namespace

int main()
{
    using audio_stream::Result;
    assert(audio_stream::request(7) == "WF1 7 STREAM\n");
    assert(audio_stream::request(0).empty() && audio_stream::request(65536).empty());

    Fixture ok;
    ok.decoder.begin(7);
    assert(ok.decoder.result() == Result::Starting);
    // Diagnostics and other replies on the same link are ignored.
    ok.feed("     OPEN RMS=0.1 GATE_RMS=0.1 DISPLAY=BARS |1|\nWF1 3 OK MODE classic\n");
    assert(ok.decoder.result() == Result::Starting);
    ok.feed("\nWF1 7 AUDIO 16000 STREAM\n");
    assert(ok.decoder.result() == Result::Streaming);
    for (unsigned sequence = 0; sequence < 300; ++sequence) ok.feed(packet(7, sequence, int16_t(-1000 + sequence)));
    assert(ok.decoder.result() == Result::Streaming && ok.received.size() == 300 * 128);
    assert(ok.received[0] == -1000 && ok.received[127] == -873 && ok.received[128] == -999);

    // A skipped packet, a bad checksum or a corrupt line ends the stream.
    Fixture gap; gap.decoder.begin(8); gap.feed("WF1 8 AUDIO 16000 STREAM\n");
    gap.feed(packet(8, 0, 0)); gap.feed(packet(8, 2, 0));
    assert(gap.decoder.result() == Result::Invalid && gap.received.size() == 128);
    Fixture checksum; checksum.decoder.begin(9); checksum.feed("WF1 9 AUDIO 16000 STREAM\n");
    std::string bad = packet(9, 0, 0); bad[bad.size() - 2] = bad[bad.size() - 2] == '0' ? '1' : '0';
    checksum.feed(bad);
    assert(checksum.decoder.result() == Result::Invalid && checksum.received.empty());
    Fixture corrupt; corrupt.decoder.begin(10); corrupt.feed("WF1 10 AUDIO 16000 STREAM\n");
    std::string broken = packet(10, 0, 0); broken[30] = '\x01';
    corrupt.feed(broken);
    assert(corrupt.decoder.result() == Result::Invalid);
    // Refused, lost and wrong headers.
    Fixture busy; busy.decoder.begin(11); busy.feed("WF1 11 ERR BUSY\n");
    assert(busy.decoder.result() == Result::Busy);
    Fixture lost; lost.decoder.begin(12); lost.feed("WF1 12 AUDIO 16000 STREAM\nWF1 12 ERR AUDIO_LOST\n");
    assert(lost.decoder.result() == Result::Lost);
    Fixture bounded; bounded.decoder.begin(13); bounded.feed("WF1 13 AUDIO 16000 64000\n");
    assert(bounded.decoder.result() == Result::Invalid);
    Fixture early; early.decoder.begin(14); early.feed(packet(14, 0, 0));
    assert(early.decoder.result() == Result::Invalid && early.received.empty());
    // Once ended, nothing more is accepted until restarted.
    gap.feed(packet(8, 1, 0));
    assert(gap.received.size() == 128);

    // Ring: positions, wrap-around, falling behind and discontinuities.
    auto ring = std::make_unique<audio_stream::Ring<512>>();
    int16_t block[128];
    int16_t out[1024];
    for (int round = 0; round < 3; ++round) {
        for (int i = 0; i < 128; ++i) block[i] = int16_t(round * 128 + i);
        ring->write(block, 128);
    }
    auto read = ring->read(0, out, 1024);
    assert(!read.skipped && read.position == 0 && read.count == 384 && out[383] == 383);
    read = ring->read(100, out, 10);
    assert(read.count == 10 && out[0] == 100);
    for (int round = 3; round < 6; ++round) {
        for (int i = 0; i < 128; ++i) block[i] = int16_t(round * 128 + i);
        ring->write(block, 128);
    }
    read = ring->read(0, out, 1024);  // 768 written, 512 kept
    assert(read.skipped && read.position == 256 && read.count == 512 && out[0] == 256 && out[511] == 767);
    const auto before = read.segment;
    ring->discontinuity();
    read = ring->read(700, out, 1024);
    assert(read.segment != before && read.skipped && read.position == 768 && read.count == 0);
    ring->write(block, 128);
    read = ring->read(768, out, 1024);
    assert(!read.skipped && read.count == 128 && ring->written() == 896);
}
