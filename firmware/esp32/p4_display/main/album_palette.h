#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace album_palette {
struct Palette {
    uint32_t background = 0x080d0a, panel = 0x121a13, accent = 0xa8d57a;
    uint32_t search = 0xa79ae8, line = 0x374838;
    bool operator==(const Palette &p) const { return background == p.background && panel == p.panel
        && accent == p.accent && search == p.search && line == p.line; }
    bool operator!=(const Palette &p) const { return !(*this == p); }
};
inline uint32_t hsv(float h, float s, float v) {
    h = std::fmod(h + 360.f, 360.f) / 60.f;
    const float c = v * s, x = c * (1.f - std::fabs(std::fmod(h, 2.f) - 1.f)), m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 1) { r=c; g=x; } else if (h < 2) { r=x; g=c; }
    else if (h < 3) { g=c; b=x; } else if (h < 4) { g=x; b=c; }
    else if (h < 5) { r=x; b=c; } else { r=c; b=x; }
    return (uint32_t(std::lround((r+m)*255)) << 16) | (uint32_t(std::lround((g+m)*255)) << 8)
        | uint32_t(std::lround((b+m)*255));
}
// RGB565 is the decoder's little-endian output. Sample at most 1024 pixels;
// fixed-size hue buckets avoid allocations, full-image scans and browser CORS.
inline Palette extract(const uint8_t *pixels, unsigned width, unsigned height) {
    if (!pixels || !width || !height || width > 1024 || height > 1024) return {};
    struct Bin { float weight=0, saturation=0; } bins[12]{};
    unsigned chromatic=0, samples=0;
    for (unsigned y=0; y<std::min(height,32U); ++y) for (unsigned x=0; x<std::min(width,32U); ++x) {
        const size_t offset = (size_t(y*height/std::min(height,32U))*width
            + x*width/std::min(width,32U))*2;
        const unsigned pixel = pixels[offset] | (unsigned(pixels[offset+1]) << 8);
        const float r=((pixel>>11)&31)/31.f, g=((pixel>>5)&63)/63.f, b=(pixel&31)/31.f;
        const float high=std::max({r,g,b}), low=std::min({r,g,b}), delta=high-low;
        ++samples;
        if (high < .05f || delta < .018f || delta/high < .12f) continue;
        float hue = high==r ? 60.f*std::fmod((g-b)/delta+6.f,6.f)
            : high==g ? 60.f*((b-r)/delta+2.f) : 60.f*((r-g)/delta+4.f);
        const unsigned index=std::min(11U,unsigned(hue/30.f));
        const float weight=(.5f + .5f*delta/high) * (.25f + .75f*(1.f-high));
        bins[index].weight+=weight; bins[index].saturation+=weight*delta/high; ++chromatic;
    }
    if (chromatic < samples/20+1) return {}; // Missing/neutral covers use the standard palette.
    unsigned primary=0;
    for (unsigned i=1; i<12; ++i) if (bins[i].weight>bins[primary].weight) primary=i;
    unsigned secondary=primary;
    for (unsigned i=0; i<12; ++i) {
        const unsigned distance=std::min((i+12-primary)%12,(primary+12-i)%12);
        if (distance>=3 && (secondary==primary || bins[i].weight>bins[secondary].weight)) secondary=i;
    }
    const float hue=primary*30.f+15.f;
    const float saturation=std::clamp(bins[primary].saturation/bins[primary].weight,.25f,.65f);
    const float other=secondary!=primary && bins[secondary].weight>=bins[primary].weight*.2f
        ? secondary*30.f+15.f : hue+180.f;
    return {hsv(hue,saturation,.20f),hsv(hue,saturation,.28f),hsv(hue,.42f,.94f),
        hsv(other,.35f,.94f),hsv(hue,.30f,.42f)};
}
} // namespace album_palette
