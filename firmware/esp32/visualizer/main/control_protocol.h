#pragma once
#include <charconv>
#include "styles.h"

namespace control {
struct Command { unsigned id = 0; bool changeMode = false; bool capture = false; visual::Mode mode = visual::Mode::Classic; };
inline bool parse(std::string_view line, Command& output) {
    if (line.substr(0, 4) != "WF1 ") return false;
    line.remove_prefix(4);
    const auto space = line.find(' ');
    if (space == std::string_view::npos) return false;
    unsigned id = 0;
    const auto parsed = std::from_chars(line.data(), line.data()+space, id);
    if (parsed.ec != std::errc{} || parsed.ptr != line.data()+space || id == 0 || id > 65535) return false;
    line.remove_prefix(space+1);
    Command next{};
    next.id = id;
    if (line == "CAPTURE") { next.capture = true; }
    else if (line != "STATUS") {
        if (line.substr(0, 5) != "MODE " || !visual::parseMode(line.substr(5), next.mode)) return false;
        next.changeMode = true;
    }
    output = next;
    return true;
}
class Lines {
public:
    template<class Callback> void push(char c, Callback ready) {
        if (c == '\n') {
            if (!discard_ && length_) ready(std::string_view(buffer_, length_));
            length_ = 0; discard_ = false;
        } else if (c != '\r') {
            if (c < 32 || c > 126 || length_ == sizeof(buffer_)) discard_ = true;
            if (!discard_) buffer_[length_++] = c;
        }
    }
private:
    char buffer_[96]{};
    size_t length_ = 0;
    bool discard_ = false;
};
}
