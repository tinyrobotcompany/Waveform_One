#include "idle_display.h"
#include <cassert>
#include <string>
int main() {
    using namespace idle_display;
    Config config;
    Policy policy;
    assert(policy.brightness(config, 0, true, 0, false, 80, 80) == 80);
    config.enabled=1; config.minutes=1;
    assert(policy.brightness(config, 59999, true, 59999, false, 80, 80) == 80);
    assert(policy.brightness(config, 60000, true, 60000, false, 80, 80) == 10);
    assert(policy.brightness(config, 60001, true, 0, false, 80, 80) == 80);
    assert(policy.brightness(config, 120001, true, 60000, true, 80, 10) == 80);
    assert(policy.brightness(config, 120002, false, 60001, false, 80, 10) == 80);
    assert(policy.brightness(config, 120003, true, 60002, false, 80, 80) == 80);
    assert(policy.brightness(config, 180003, true, 120002, false, 80, 30) == 10);
    config.enabled=0;
    assert(policy.brightness(config, 180004, true, 120003, false, 80, 30) == 30);
    config.minutes=0; assert(!config.valid());
    config.minutes=241; assert(!config.valid());
    config.minutes=37; assert(config.valid());
    char text[16];
    format_clock(text,sizeof(text),0,5,false); assert(std::string(text)=="00:05");
    format_clock(text,sizeof(text),0,5,true); assert(std::string(text)=="12:05 AM");
    format_clock(text,sizeof(text),12,5,true); assert(std::string(text)=="12:05 PM");
    format_clock(text,sizeof(text),23,59,true); assert(std::string(text)=="11:59 PM");
    format_clock(text,sizeof(text),-1,0,true); assert(std::string(text)=="--:--");
}
