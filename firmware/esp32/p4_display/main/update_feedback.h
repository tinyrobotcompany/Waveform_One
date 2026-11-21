#pragma once
#include <cstdint>
#include <string>
namespace update_feedback {
// Bridge the UI-to-worker queue without hiding a fast completion or error.
class Pending {
    int64_t until_ = 0;
    std::string before_;
public:
    void begin(int64_t now, const char *message) { until_ = now + 8000000; before_ = message; }
    bool active(int64_t now, bool busy, const char *message) {
        if (busy || before_ != message || now >= until_) until_ = 0;
        return busy || until_ > now;
    }
};
}
