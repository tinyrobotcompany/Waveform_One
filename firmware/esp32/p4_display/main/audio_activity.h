#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace audio_activity {
// Consume the existing calibrated S3 visualizer diagnostics. Never infer
// silence from amplified PCM or a failed recognition lookup.
class Decoder {
public:
    void push(char byte, int64_t now) {
        if (byte == '\r') return;
        if (byte != '\n') {
            if (size_ < line_.size()) line_[size_++] = byte;
            else overflow_ = true;
            return;
        }
        if (!overflow_) parse({line_.data(), size_}, now);
        size_ = 0; overflow_ = false;
    }
    bool active(int64_t now) const {
        return active_ && known_ && now - received_ < 3000000;
    }
private:
    void parse(std::string_view line, int64_t now) {
        if (line.size() < 10 || (line.substr(0, 5) != "BEAT " && line.substr(0, 5) != "     ")
            || line.back() != '|' || line.find(" GATE_RMS=") == line.npos
            || line.find(" DISPLAY=") == line.npos) return;
        line.remove_prefix(5);
        if (line.substr(0, 9) == "OPEN RMS=") { active_ = true; quiet_ = 0; }
        else if (line.substr(0, 9) == "SHUT RMS=") {
            // Three half-second reports avoid clearing during a brief quiet beat.
            if (++quiet_ >= 3) active_ = false;
        } else return;
        known_ = true; received_ = now;
    }
    std::array<char, 512> line_{};
    size_t size_ = 0;
    bool overflow_ = false, known_ = false, active_ = false;
    unsigned quiet_ = 0;
    int64_t received_ = 0;
};
}
