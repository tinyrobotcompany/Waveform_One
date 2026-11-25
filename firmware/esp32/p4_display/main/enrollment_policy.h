#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace enrollment {
inline bool valid_url(const char *url) {
    if (!url || std::strncmp(url, "https://", 8)) return false;
    const char *host = url + 8, *end = host;
    while ((*end >= 'a' && *end <= 'z') || (*end >= 'A' && *end <= 'Z') ||
           (*end >= '0' && *end <= '9') || *end == '-' || *end == '.') ++end;
    return end > host && *host != '.' && *host != '-' && end[-1] != '.' && end[-1] != '-' &&
           std::strcmp(end, "/v1/device") == 0;
}
struct Schedule {
    int64_t next = 0, retry = 0;
    unsigned failures = 0;
    char address[16]{};
    bool due(int64_t now, const char *ip, int64_t renewal) const {
        return now >= retry && (now >= next || std::strcmp(ip, address) || (renewal && now >= renewal));
    }
    void success(int64_t now, const char *ip, bool pending, int64_t renewal, unsigned jitter=0) {
        failures = 0;
        std::snprintf(address, sizeof(address), "%s", ip);
        next = now + (pending ? 30 : 86400 + std::min(jitter,900U));
        if (!pending && renewal > now) next = std::min(next, renewal);
        retry = pending ? next : 0;
    }
    void failure(int64_t now, unsigned server_delay = 0) {
        failures = std::min(failures + 1, 10U);
        retry = now + std::max(std::min(30U << failures, 21600U), std::min(server_delay, 86400U));
    }
};
}
