#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

// Continuous 16 kHz mono PCM from the S3 controller. The S3 answers
// "WF1 <id> STREAM" with "AUDIO 16000 STREAM", then sends checksummed 256-byte
// PCM packets with a sequence that only ever increases by one.
namespace audio_stream {

inline constexpr unsigned kSampleRate = 16000;
inline constexpr size_t kPacketBytes = 256;
inline constexpr size_t kPacketSamples = kPacketBytes / 2;

inline std::string request(unsigned id)
{
    if (id == 0 || id > 65535) return {};
    return "WF1 " + std::to_string(id) + " STREAM\n";
}

enum class Result { Idle, Starting, Streaming, Busy, Lost, Invalid };

// Parses one stream's lines without allocating; runs on the USB receive task.
// Any gap, checksum error or malformed packet ends the stream: the caller
// restarts it and marks a discontinuity so no audio is spliced across a gap.
class Decoder {
public:
    void begin(unsigned id) { *this = Decoder{}; id_ = id; result_ = Result::Starting; }
    void reset() { *this = Decoder{}; }
    Result result() const { return result_; }

    // Sink receives kPacketSamples signed samples for every verified packet.
    template<class Sink> void push(char byte, Sink sink)
    {
        if (result_ != Result::Starting && result_ != Result::Streaming) return;
        if (byte == '\n') {
            if (length_ != 0) line(std::string_view(line_.data(), length_), discard_, sink);
            length_ = 0;
            discard_ = false;
        } else if (byte != '\r') {
            if (static_cast<unsigned char>(byte) < 32
                || static_cast<unsigned char>(byte) > 126 || length_ == line_.size()) {
                discard_ = true;
            }
            if (!discard_) line_[length_++] = byte;
        }
    }

private:
    static bool number(std::string_view text, uint32_t &value, int base = 10)
    {
        if (text.empty()) return false;
        auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
        return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
    }

    static int nibble(char byte)
    {
        if (byte >= '0' && byte <= '9') return byte - '0';
        if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
        if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
        return -1;
    }

    template<class Sink> void line(std::string_view text, bool corrupt, Sink sink)
    {
        std::array<std::string_view, 7> fields{};
        size_t count = 0;
        while (!text.empty() && count < fields.size()) {
            const size_t end = text.find(' ');
            fields[count++] = text.substr(0, end);
            text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        }
        uint32_t id = 0;
        // Diagnostics and other requests share the link; ignore them.
        if (count < 2 || fields[0] != "WF1" || !number(fields[1], id) || id != id_) return;
        if (corrupt || !text.empty()) { result_ = Result::Invalid; return; }
        if (result_ == Result::Starting) {
            if (count == 5 && fields[2] == "AUDIO" && fields[3] == "16000" && fields[4] == "STREAM") {
                result_ = Result::Streaming;
            } else if (count == 4 && fields[2] == "ERR" && fields[3] == "BUSY") {
                result_ = Result::Busy;
            } else {
                result_ = Result::Invalid;
            }
            return;
        }
        if (count == 4 && fields[2] == "ERR") {
            result_ = fields[3] == "AUDIO_LOST" ? Result::Lost : Result::Invalid;
            return;
        }
        uint32_t sequence = 0, expected_hash = 0;
        if (count != 6 || fields[2] != "PCM" || !number(fields[3], sequence)
            || sequence != next_sequence_ || fields[4].size() != kPacketBytes * 2
            || fields[5].size() != 8 || !number(fields[5], expected_hash, 16)) {
            result_ = Result::Invalid;
            return;
        }
        std::array<int16_t, kPacketSamples> samples{};
        uint32_t hash = 2166136261u;
        for (size_t index = 0; index < kPacketBytes; ++index) {
            const int high = nibble(fields[4][index * 2]);
            const int low = nibble(fields[4][index * 2 + 1]);
            if (high < 0 || low < 0) { result_ = Result::Invalid; return; }
            const auto value = static_cast<uint8_t>((high << 4) | low);
            hash = (hash ^ value) * 16777619u;
            auto &sample = samples[index / 2];
            sample = static_cast<int16_t>(index % 2 == 0 ? value
                : static_cast<uint16_t>(static_cast<uint16_t>(sample) | (value << 8)));
        }
        if (hash != expected_hash) { result_ = Result::Invalid; return; }
        ++next_sequence_;
        sink(samples.data());
    }

    uint32_t id_ = 0;
    uint32_t next_sequence_ = 0;
    Result result_ = Result::Idle;
    std::array<char, 600> line_{};
    size_t length_ = 0;
    bool discard_ = false;
};

// Single-producer, single-consumer sample ring. Positions count samples since
// boot, so the reader can tell exactly which audio it has and whether it fell
// behind. A discontinuity bumps the segment: audio either side of it belongs
// to different recordings and must never be analysed as one.
template<size_t Capacity> class Ring {
public:
    static_assert(Capacity % kPacketSamples == 0, "whole packets only");

    void write(const int16_t *samples, size_t count)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < count; ++i) buffer_[(written_ + i) % Capacity] = samples[i];
        written_ += count;
    }

    void discontinuity()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++segment_;
        segment_start_ = written_;
    }

    struct Read { size_t count = 0; uint64_t position = 0; uint32_t segment = 0; bool skipped = false; };

    // Copies samples from position onward, never older than the current
    // segment and never older than the buffer still holds.
    Read read(uint64_t position, int16_t *output, size_t capacity) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Read result{};
        result.segment = segment_;
        uint64_t oldest = written_ > Capacity ? written_ - Capacity : 0;
        if (segment_start_ > oldest) oldest = segment_start_;
        if (position < oldest) { position = oldest; result.skipped = true; }
        if (position > written_) position = written_;
        const uint64_t available = written_ - position;
        result.count = available < capacity ? static_cast<size_t>(available) : capacity;
        for (size_t i = 0; i < result.count; ++i) output[i] = buffer_[(position + i) % Capacity];
        result.position = position;
        return result;
    }

    uint64_t written() const { std::lock_guard<std::mutex> lock(mutex_); return written_; }

private:
    mutable std::mutex mutex_;
    int16_t buffer_[Capacity]{};
    uint64_t written_ = 0, segment_start_ = 0;
    uint32_t segment_ = 0;
};

} // namespace audio_stream
