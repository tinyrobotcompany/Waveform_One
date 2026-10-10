#pragma once

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <limits>
#include <string>
#include <string_view>

namespace recognition_policy {
inline constexpr int64_t kIntervalUs = 1000000;
inline constexpr int64_t kTrackLifetimeUs = 90000000;
struct Session { uint64_t epoch; bool ready; };

class Sessions {
public:
    Session snapshot() const { return {epoch_, online_ && connected_ && active_}; }
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
    bool set_activity(bool active)
    {
        if (active_ == active) return false;
        active_ = active; ++epoch_; return true;
    }
private:
    uint64_t epoch_ = 1;
    bool online_ = false, connected_ = false, active_ = false;
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

class Retries {
public:
    bool due(int64_t now) const { return now >= next_; }
    void begin(int64_t now) { next_ = now + kIntervalUs; }
    void activity_changed(int64_t now) { if (failures_ == 0) next_ = now; }
    void succeeded(int64_t now, bool matched) {
        failures_ = 0; next_ = now + (matched ? 30000000 : 15000000);
    }
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

inline bool expired(int64_t matched_at, int64_t now) { return now - matched_at >= kTrackLifetimeUs; }
} // namespace recognition_policy
