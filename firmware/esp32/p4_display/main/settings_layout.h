#pragma once
namespace settings_layout {
enum Destination { Microphone, Display, Appearance, Wifi, Phone, Leds, Updates, Account, count };
struct Rect { int x, y, w, h; };
constexpr Rect tile(unsigned index) { return {48 + int(index % 3) * 310, 130 + int(index / 3) * 140, 280, 100}; }
}
