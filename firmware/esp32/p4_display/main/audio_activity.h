#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace audio_activity {
enum class State { Unknown, Quiet, Playing };
// The native S3 classifier supplies the same decision used by the LEDs.
// Energy diagnostics are deliberately ignored: loud speech is not music.
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
    State state(int64_t now) const {
        if (!known_ || now - received_ >= 3000000) return State::Unknown;
        return state_;
    }
private:
    void parse(std::string_view line, int64_t now) {
#ifdef WAVEFORM_ALLOW_LEGACY_ACTIVITY
        // Staged development bridge keeps the existing S3 usable until the
        // classifier firmware is installed. Product builds require classification.
        if (legacy_ && line.size() >= 10 && line.back() == '|' &&
            (line.substr(0, 5) == "BEAT " || line.substr(0, 5) == "     ") &&
            line.find(" GATE_RMS=") != line.npos && line.find(" DISPLAY=") != line.npos) {
            if (line.substr(5, 9) == "OPEN RMS=") { state_ = State::Playing; quiet_ = 0; }
            else if (line.substr(5, 9) == "SHUT RMS=") { if (++quiet_ >= 3) state_ = State::Quiet; }
            else return;
            known_ = true; received_ = now; return;
        }
#endif
        if (line == "WF1 MUSIC MUSIC") state_ = State::Playing;
        else if (line == "WF1 MUSIC QUIET") state_ = State::Quiet;
        else if (line == "WF1 MUSIC UNKNOWN") state_ = State::Unknown;
        else return;
#ifdef WAVEFORM_ALLOW_LEGACY_ACTIVITY
        legacy_ = false;
#endif
        known_ = true; received_ = now;
    }
    std::array<char, 512> line_{};
    size_t size_ = 0;
    bool overflow_ = false, known_ = false;
    State state_ = State::Unknown;
#ifdef WAVEFORM_ALLOW_LEGACY_ACTIVITY
    bool legacy_ = true;
    unsigned quiet_ = 0;
#endif
    int64_t received_ = 0;
};
}
