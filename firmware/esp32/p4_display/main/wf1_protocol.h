#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>

namespace wf1 {

enum class Mode { Classic, Mirrored, Waterfall };

inline constexpr std::string_view name(Mode mode)
{
    switch (mode) {
    case Mode::Classic: return "classic";
    case Mode::Mirrored: return "mirrored";
    case Mode::Waterfall: return "waterfall";
    }
    return "mirrored";
}

inline std::string mode_request(unsigned id, Mode mode)
{
    if (id == 0 || id > 65535) return {};
    return "WF1 " + std::to_string(id) + " MODE " + std::string(name(mode)) + "\n";
}

inline std::string status_request(unsigned id)
{
    if (id == 0 || id > 65535) return {};
    return "WF1 " + std::to_string(id) + " STATUS\n";
}

struct Reply {
    unsigned id = 0;
    bool ok = false;
    Mode mode = Mode::Mirrored;
};

namespace detail {

inline bool next_token(std::string_view &line, std::string_view &token)
{
    while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
    if (line.empty()) return false;
    const size_t end = line.find(' ');
    token = line.substr(0, end);
    line = end == std::string_view::npos ? std::string_view{} : line.substr(end + 1);
    return true;
}

inline bool parse_mode(std::string_view value, Mode &mode)
{
    if (value == "classic") mode = Mode::Classic;
    else if (value == "mirrored") mode = Mode::Mirrored;
    else if (value == "waterfall") mode = Mode::Waterfall;
    else return false;
    return true;
}

inline bool parse_reply(std::string_view line, unsigned expected_id, Reply &reply)
{
    std::array<std::string_view, 6> tokens{};
    size_t count = 0;
    while (count < tokens.size() && next_token(line, tokens[count])) ++count;
    if (!line.empty() || count < 3 || tokens[0] != "WF1") return false;

    unsigned id = 0;
    const auto parsed = std::from_chars(tokens[1].data(), tokens[1].data() + tokens[1].size(), id);
    if (parsed.ec != std::errc{} || parsed.ptr != tokens[1].data() + tokens[1].size()
        || id != expected_id) {
        return false;
    }

    Reply next{};
    next.id = id;
    if (count == 5 && tokens[2] == "OK" && tokens[3] == "MODE"
        && parse_mode(tokens[4], next.mode)) {
        next.ok = true;
    }
    reply = next;
    return true;
}

} // namespace detail

class Replies {
public:
    explicit Replies(unsigned expected_id) : expected_id_(expected_id) {}

    void expect(unsigned id)
    {
        expected_id_ = id;
        length_ = 0;
        discard_ = false;
    }

    template <class Callback> void push(char byte, Callback ready)
    {
        if (byte == '\n') {
            if (!discard_ && length_ != 0) {
                Reply reply{};
                if (detail::parse_reply(std::string_view(buffer_.data(), length_), expected_id_, reply)) {
                    ready(reply);
                }
            }
            length_ = 0;
            discard_ = false;
            return;
        }
        if (byte == '\r' || discard_) return;
        if (byte < 32 || byte > 126 || length_ == buffer_.size()) {
            discard_ = true;
            return;
        }
        buffer_[length_++] = byte;
    }

private:
    unsigned expected_id_;
    std::array<char, 512> buffer_{};
    size_t length_ = 0;
    bool discard_ = false;
};

} // namespace wf1
