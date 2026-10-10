#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace recognition_policy {
inline constexpr int64_t kIntervalUs = 30000000;
inline constexpr int64_t kTrackLifetimeUs = 90000000;
struct Session { uint64_t epoch; bool ready; };

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

class Retries {
public:
    bool due(int64_t now) const { return now >= next_; }
    void begin(int64_t now) { next_ = now + kIntervalUs; }
    void succeeded(int64_t now) { failures_ = 0; begin(now); }
    void failed(int64_t now)
    {
        failures_ = std::min(failures_ + 1, 4u);
        next_ = now + std::min<int64_t>(300000000, kIntervalUs << failures_);
    }
private:
    unsigned failures_ = 0;
    int64_t next_ = 0;
};

inline bool expired(int64_t matched_at, int64_t now) { return now - matched_at >= kTrackLifetimeUs; }
} // namespace recognition_policy
