#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
namespace idle_display {
struct Config {
    uint32_t version=1;
    uint16_t minutes=5;
    uint8_t enabled=0, percent=10, twelve_hour=0, large=1, accent=0, reserved=0;
    bool valid() const { return version==1 && minutes>=1 && minutes<=240 && enabled<=1 && percent>=10 && percent<=100 && twelve_hour<=1 && large<=1 && accent<=1; }
};
class Policy {
    int64_t idle_since_=-1, awake_until_=0;
public:
    void wake(int64_t now_ms) { idle_since_=now_ms; awake_until_=now_ms+60000; }
    int brightness(const Config &config, int64_t now_ms, bool idle, uint32_t untouched_ms,
                   bool override, int selected, int night) {
        if (!idle) { idle_since_=-1; return selected; }
        if (idle_since_<0 || now_ms<idle_since_) idle_since_=now_ms;
        if (override || now_ms<awake_until_) return selected;
        const int64_t delay=int64_t(config.minutes)*60000;
        return config.valid() && config.enabled && now_ms-idle_since_>=delay && untouched_ms>=delay
            ? std::min(night,int(config.percent)) : night;
    }
};
inline void format_clock(char *out, size_t size, int hour, int minute, bool twelve_hour) {
    if (hour<0 || hour>23 || minute<0 || minute>59) std::snprintf(out,size,"--:--");
    else if (twelve_hour) std::snprintf(out,size,"%d:%02d %s",hour%12 ? hour%12 : 12,minute,hour<12?"AM":"PM");
    else std::snprintf(out,size,"%02d:%02d",hour,minute);
}
bool load(Config &output);
bool save(const Config &value);
}
