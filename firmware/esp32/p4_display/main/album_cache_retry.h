#pragma once
#include <algorithm>
#include <cstdint>

namespace album_cache {
// One background request: a new album supersedes an in-flight result. Call
// under the cache mutex. Failures retry independently of cover downloads.
class Retry {
public:
    uint64_t request() { attempts_ = 0; next_ = 0; pending_ = true; return ++generation_; }
    bool due(int64_t now) const { return pending_ && now >= next_; }
    uint64_t generation() const { return generation_; }
    bool complete(uint64_t generation, bool success, int64_t now, int64_t service_delay) {
        if (generation != generation_) return false;
        if (success) { pending_ = false; return true; }
        if (++attempts_ >= 3) { pending_ = false; return false; }
        next_ = now + std::max<int64_t>(service_delay, attempts_ * 30000000LL);
        return false;
    }
    bool pending() const { return pending_; }
private:
    uint64_t generation_ = 0;
    int64_t next_ = 0;
    unsigned attempts_ = 0;
    bool pending_ = false;
};
}
