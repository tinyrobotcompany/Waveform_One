#pragma once
#include <algorithm>
#include <cstdint>

namespace display_schedule {
struct Config {
    uint32_t version = 1;
    uint16_t start = 23 * 60, end = 7 * 60;
    uint8_t enabled = 1, percent = 10;
    uint8_t reserved[2]{};
    bool valid() const {
        return version == 1 && start < 1440 && end < 1440 && start != end &&
            start % 15 == 0 && end % 15 == 0 && enabled <= 1 && percent >= 10 && percent <= 100;
    }
};
inline bool in_window(const Config &config, int minute) {
    if (minute < 0 || minute >= 1440 || !config.valid()) return false;
    return config.start < config.end ? minute >= config.start && minute < config.end
        : minute >= config.start || minute < config.end;
}
inline int brightness(const Config &config, int minute, bool idle, bool interaction,
                      int selected) {
    return config.enabled && idle && !interaction && in_window(config, minute)
        ? std::min(selected, int(config.percent)) : selected;
}
// Missing settings use defaults; storage failures do not erase NVS.
bool load(Config &output);
bool save(const Config &value);
}
