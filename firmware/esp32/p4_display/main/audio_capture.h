#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace audio_capture {

inline constexpr size_t kPacketBytes = 256;
inline constexpr size_t kPackets = 1000;
inline constexpr size_t kBytes = kPacketBytes * kPackets;
inline constexpr unsigned kTimeoutMs = 15000;

enum class Result { Idle, Receiving, Complete, Busy, AudioLost, Invalid };

inline std::string capture_request(unsigned id)
{
    if (id == 0 || id > 65535) return {};
    return "WF1 " + std::to_string(id) + " CAPTURE\n";
}

// No allocation in the USB receive callback. The caller owns a fixed PCM buffer;
// a clip is usable only after the matching END and all 1,000 checked packets.
class Decoder {
public:
    bool begin(unsigned id, uint8_t *buffer, size_t capacity)
    {
        if (result_ == Result::Receiving || id == 0 || id > 65535
            || buffer == nullptr || capacity < kBytes) return false;
        reset();
        id_ = id;
        buffer_ = buffer;
        result_ = Result::Receiving;
        return true;
    }

    void reset() { *this = Decoder{}; }
    Result result() const { return result_; }
    size_t size() const { return bytes_; }

    void push(char byte)
    {
        if (result_ != Result::Receiving) return;
        if (byte == '\n') {
            if (length_ != 0) line(std::string_view(line_.data(), length_), discard_);
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
    static bool number(std::string_view text, unsigned &value, int base = 10)
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

    void line(std::string_view text, bool corrupt)
    {
        std::array<std::string_view, 7> fields{};
        size_t count = 0;
        while (!text.empty() && count < fields.size()) {
            const size_t end = text.find(' ');
            fields[count++] = text.substr(0, end);
            text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        }
        unsigned id = 0;
        if (count < 2 || fields[0] != "WF1" || !number(fields[1], id) || id != id_) return;
        if (corrupt || !text.empty()) { result_ = Result::Invalid; return; }
        if (count == 5 && fields[2] == "AUDIO" && fields[3] == "16000"
            && fields[4] == "128000" && !started_) {
            started_ = true;
            return;
        }
        if (count == 4 && fields[2] == "ERR") {
            result_ = fields[3] == "BUSY" ? Result::Busy
                : fields[3] == "AUDIO_LOST" ? Result::AudioLost : Result::Invalid;
            return;
        }
        if (started_ && count == 6 && fields[2] == "PCM" && bytes_ < kBytes) {
            unsigned sequence = 0, expected_hash = 0;
            if (!number(fields[3], sequence) || sequence != bytes_ / kPacketBytes
                || fields[4].size() != kPacketBytes * 2 || fields[5].size() != 8
                || !number(fields[5], expected_hash, 16)) {
                result_ = Result::Invalid;
                return;
            }
            std::array<uint8_t, kPacketBytes> packet{};
            uint32_t hash = 2166136261u;
            for (size_t index = 0; index < packet.size(); ++index) {
                const int high = nibble(fields[4][index * 2]);
                const int low = nibble(fields[4][index * 2 + 1]);
                if (high < 0 || low < 0) { result_ = Result::Invalid; return; }
                packet[index] = static_cast<uint8_t>((high << 4) | low);
                hash = (hash ^ packet[index]) * 16777619u;
            }
            if (hash != expected_hash) { result_ = Result::Invalid; return; }
            for (uint8_t byte : packet) buffer_[bytes_++] = byte;
            return;
        }
        result_ = started_ && count == 4 && fields[2] == "END"
            && fields[3] == "1000" && bytes_ == kBytes ? Result::Complete : Result::Invalid;
    }

    unsigned id_ = 0;
    uint8_t *buffer_ = nullptr;
    size_t bytes_ = 0;
    Result result_ = Result::Idle;
    bool started_ = false;
    std::array<char, 600> line_{};
    size_t length_ = 0;
    bool discard_ = false;
};

} // namespace audio_capture
