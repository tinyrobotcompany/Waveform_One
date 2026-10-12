#pragma once

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>

namespace recognition_policy {
struct Session { uint64_t epoch; bool ready; };

// Network and controller availability. Any change invalidates in-flight
// work: a lookup that began on a different network must not be shown.
class Sessions {
public:
    Session snapshot() const { return {epoch_, online_ && connected_}; }
    bool set_network(bool available, std::string_view pairing_url)
    {
        // Pairing rotates independently of the network. Keep only the origin,
        // never a pairing code, when deciding whether a recognition is stale.
        std::string origin;
        if (available && pairing_url.substr(0, 7) == "http://") {
            const size_t end = pairing_url.find_first_of("/?#", 7);
            origin = pairing_url.substr(0, end);
            if (origin.size() <= 7 || origin.size() > 64) origin.clear();
        }
        available = available && !origin.empty();
        if (available == online_ && origin == origin_) return false;
        online_ = available; origin_ = origin; ++epoch_;
        return true;
    }
    bool set_controller(bool available)
    {
        if (connected_ == available) return false;
        connected_ = available; ++epoch_; return true;
    }
private:
    uint64_t epoch_ = 1;
    bool online_ = false, connected_ = false;
    std::string origin_;
};

// Retry-After accepts either delta-seconds or an IMF-fixdate in UTC.
inline int64_t retry_after(std::string_view value, int64_t epoch = 0)
{
    int64_t seconds = 0;
    bool digits = !value.empty();
    for (char c : value) {
        if (c < '0' || c > '9') { digits = false; break; }
        if (seconds > (std::numeric_limits<int64_t>::max() / 1000000 - (c - '0')) / 10) return 0;
        seconds = seconds * 10 + c - '0';
    }
    if (digits) return seconds * 1000000;
    if (value.size() != 29 || epoch <= 0) return 0;
    std::tm date{};
    const std::string text(value);
    const char *end = strptime(text.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &date);
    if (end == nullptr || *end != '\0') return 0;
    const int year = date.tm_year + 1900;
    if (year < 1970 || year > 2100 || date.tm_mon < 0 || date.tm_mon >= 12
        || date.tm_hour < 0 || date.tm_hour > 23 || date.tm_min < 0 || date.tm_min > 59
        || date.tm_sec < 0 || date.tm_sec > 59) return 0;
    const auto leap = [](int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); };
    int64_t days = 0;
    for (int y = 1970; y < year; ++y) days += 365 + leap(y);
    constexpr int months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int month_days = months[date.tm_mon] + (date.tm_mon == 1 && leap(year));
    if (date.tm_mday < 1 || date.tm_mday > month_days) return 0;
    for (int m = 0; m < date.tm_mon; ++m) days += months[m] + (m == 1 && leap(year));
    const int64_t requested = (days + date.tm_mday - 1) * 86400
        + date.tm_hour * 3600 + date.tm_min * 60 + date.tm_sec;
    return std::max<int64_t>(0, requested - epoch) * 1000000;
}

// Service failures back off independently of the music: 60 s doubling to
// five minutes, or longer if the service asks.
class Retries {
public:
    bool due(int64_t now) const { return now >= next_; }
    int64_t remaining_us(int64_t now) const { return due(now) ? 0 : next_ - now; }
    void succeeded() { failures_ = 0; }
    void failed(int64_t now, int64_t service_delay = 0)
    {
        failures_ = std::min(failures_ + 1, 4u);
        const int64_t delay = std::max(service_delay, std::min<int64_t>(300000000, 30000000LL << failures_));
        next_ = now > std::numeric_limits<int64_t>::max() - delay
            ? std::numeric_limits<int64_t>::max() : now + delay;
    }
private:
    unsigned failures_ = 0;
    int64_t next_ = 0;
};

// At most a fixed number of lookups in any rolling minute. The free
// fallback service answers a fourth with HTTP 429 and a minute's lockout
// (measured 2026-10-12); a paid service allows more.
class Budget {
public:
    static constexpr unsigned kMaxPerWindow = 16;
    static constexpr int64_t kWindowUs = 60000000;
    explicit Budget(unsigned per_window = 3) : per_window_(std::min(per_window, kMaxPerWindow)) {}
    bool available(int64_t now) const
    {
        unsigned recent = 0;
        for (unsigned i = 0; i < per_window_; ++i) recent += spent_[i] >= 0 && now - spent_[i] < kWindowUs;
        return recent < per_window_;
    }
    void spend(int64_t now) { spent_[next_++ % per_window_] = now; }
private:
    unsigned per_window_;
    int64_t spent_[kMaxPerWindow] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    unsigned next_ = 0;
};

// Which audio to send, and when. Positions are 128-sample hops of the
// continuous recognition audio (125 per second).
//
// - When sound starts, look up the first 3 s, then 5 s, then 8 s of it, then
//   the latest 8 s every 30 s while it stays unidentified.
// - Once a song is identified, the service says where in the song the clip
//   was and the catalogue says how long the song is, so the next lookup waits
//   until 4 s after that song should end. Without a length, check every 60 s.
// - Silence ends everything: the display returns to its standard screen.
struct Window { uint32_t first = 0, last = 0; };

class Planner {
public:
    static constexpr uint32_t kHopsPerSecond = 125;
    static constexpr uint32_t kMaxWindow = 8 * kHopsPerSecond;
    static constexpr unsigned kMaxQuickTries = 3;

    Planner() = default;
    // Seconds of sound before each quick try.
    explicit Planner(std::initializer_list<uint32_t> seconds)
    {
        tries_ = 0;
        for (uint32_t value : seconds) if (tries_ < kMaxQuickTries) first_tries_[tries_++] = value * kHopsPerSecond;
    }
    static constexpr uint32_t kRetryHops = 30 * kHopsPerSecond;
    static constexpr uint32_t kCautiousFirstHops = 6 * kHopsPerSecond;
    static constexpr uint32_t kCautiousRetryHops = 120 * kHopsPerSecond;
    static constexpr uint32_t kVerifyHops = 60 * kHopsPerSecond;
    static constexpr uint32_t kNextSongHops = 4 * kHopsPerSecond;

    void sound_started(uint32_t hop) { begin(hop); active_ = true; }
    // Recent sounds were not music the service knows (an air conditioner,
    // talk): one try after 6 s, then every two minutes, until something matches.
    void set_cautious(bool value) { cautious_ = value; }
    bool cautious() const { return cautious_; }
    // Silence or a gap in the audio. Returns true if a song was showing.
    bool stopped()
    {
        const bool showing = matched_;
        active_ = matched_ = ended_ = false;
        segment_ = attempts_ = last_attempt_ = end_ = verify_ = 0; misses_ = 0;
        return showing;
    }
    bool active() const { return active_; }
    bool matched() const { return matched_; }

    // The next lookup window, if one is due once heard hops exist.
    bool due(uint32_t heard, Window &window)
    {
        if (!active_ || heard <= segment_) return false;
        if (matched_ && end_ != 0 && !ended_ && heard >= end_ + kNextSongHops) {
            // The song should be over: identify whatever follows it.
            begin(end_); ended_ = true;
        }
        const uint32_t elapsed = heard - segment_;
        bool ready;
        if (matched_ && !ended_) ready = end_ == 0 && heard >= verify_;
        else if (cautious_) ready = attempts_ == 0 ? elapsed >= kCautiousFirstHops : heard >= last_attempt_ + kCautiousRetryHops;
        else if (attempts_ < tries_) ready = elapsed >= first_tries_[attempts_];
        else ready = heard >= last_attempt_ + kRetryHops;
        if (!ready) return false;
        window.last = heard;
        window.first = std::max(segment_, heard > kMaxWindow ? heard - kMaxWindow : 0);
        return true;
    }

    void attempted(uint32_t heard) { ++attempts_; last_attempt_ = heard; }

    // A song was identified in window. offset_s is the song position at the
    // window's first hop; duration_s is zero when unknown.
    void matched(const Window &window, uint32_t heard, double offset_s, double duration_s)
    {
        matched_ = true; ended_ = false; misses_ = 0;
        end_ = 0; verify_ = heard + kVerifyHops;
        if (duration_s >= 30 && duration_s <= 3600 && offset_s >= 0 && offset_s < duration_s) {
            const double end = window.first + (duration_s - offset_s) * kHopsPerSecond;
            // Too close to call: check again shortly instead.
            if (end > heard + kHopsPerSecond && end < double(std::numeric_limits<uint32_t>::max())) end_ = uint32_t(end);
            else verify_ = heard + 5 * kHopsPerSecond;
        }
    }

    // Nothing identified. Returns true when a showing song must be cleared:
    // its expected end has passed, or two checks in a row heard nothing.
    bool missed()
    {
        if (!matched_) return false;
        if (ended_ || ++misses_ >= 2) {
            matched_ = false; ended_ = false; end_ = 0; misses_ = 0;
            return true;
        }
        return false;
    }

    uint32_t expected_end() const { return end_; }
    // Sound that the quick lookups could not identify.
    bool unidentified() const { return active_ && !matched_ && attempts_ >= (cautious_ ? 1u : tries_); }

private:
    void begin(uint32_t hop) { segment_ = hop; attempts_ = 0; last_attempt_ = hop; }

    uint32_t first_tries_[kMaxQuickTries] = {4 * kHopsPerSecond, 10 * kHopsPerSecond};
    unsigned tries_ = 2;
    bool cautious_ = false;
    bool active_ = false, matched_ = false, ended_ = false;
    uint32_t segment_ = 0, attempts_ = 0, last_attempt_ = 0;
    uint32_t end_ = 0, verify_ = 0;
    unsigned misses_ = 0;
};
} // namespace recognition_policy
