#include "album_palette.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <vector>

using album_palette::Palette;
namespace detail = album_palette::detail;

static constexpr uint16_t rgb565(unsigned r, unsigned g, unsigned b) {
    return uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
struct Cover {
    unsigned width, height; std::vector<uint8_t> bytes;
    Cover(unsigned w, unsigned h, uint16_t fill) : width(w), height(h), bytes(w * h * 2) { rect(0, 0, w, h, fill); }
    void rect(unsigned x0, unsigned y0, unsigned x1, unsigned y1, uint16_t value) {
        for (unsigned y = y0; y < y1; ++y) for (unsigned x = x0; x < x1; ++x) {
            bytes[(y * width + x) * 2] = value & 255; bytes[(y * width + x) * 2 + 1] = value >> 8;
        }
    }
    Palette palette() const { return album_palette::extract(bytes.data(), width, height); }
};
static float hue(uint32_t c) { const auto l = detail::lab(c); return std::atan2(l.b, l.a) * 57.29578f; }
static float hue_gap(float a, float b) { const float d = std::fabs(a - b); return std::min(d, 360 - d); }

// Every text colour is readable on every part of the background, as shown in RGB565.
static void check_readable(const Palette &p) {
    std::vector<uint32_t> surfaces(std::begin(p.field), std::end(p.field));
    surfaces.push_back(p.background);
    for (uint32_t surface : surfaces) {
        for (uint32_t text : {p.text, p.secondary, p.tertiary, p.quaternary})
            assert(detail::contrast(text, surface) >= album_palette::kTextContrast);
        for (uint32_t accent : {p.accent, p.search})
            assert(detail::contrast(accent, surface) >= album_palette::kAccentContrast);
    }
    // Light backgrounds take dark text, and dark backgrounds light text.
    assert(p.light == (detail::luminance(p.text) < detail::luminance(p.background)));
}

int main() {
    // Classic black keeps the original colours exactly.
    const Palette classic{};
    assert(classic.background == 0x080d0a && classic.panel == 0x121a13 && classic.line == 0x374838);
    assert(classic.text == 0xedf3e8 && classic.tertiary == 0xa7b2a7);
    assert(classic.accent == 0xa8d57a && classic.search == 0xa79ae8 && !classic.light);
    for (uint32_t cell : classic.field) assert(cell == classic.background);

    // No usable artwork: Classic.
    uint8_t one[] = {0, 0};
    assert(album_palette::extract(nullptr, 320, 320) == classic);
    assert(album_palette::extract(one, 1, 1) == classic);
    assert(album_palette::extract(one, 1025, 8) == classic);

    // Solid covers become their own colour, readable whatever it is.
    for (uint16_t fill : {rgb565(220, 30, 30), rgb565(40, 200, 60), rgb565(30, 40, 200), rgb565(250, 160, 20),
                          rgb565(255, 255, 255), rgb565(0, 0, 0), rgb565(128, 128, 128), rgb565(250, 240, 120)}) {
        const Cover cover(64, 64, fill);
        const Palette p = cover.palette();
        check_readable(p);
        assert(p == cover.palette()); // deterministic
    }
    // White covers give light backgrounds with dark text; black covers the reverse.
    const Palette white = Cover(64, 64, rgb565(250, 250, 250)).palette();
    assert(white.light && detail::luminance(white.background) > .7f);
    const Palette black = Cover(64, 64, rgb565(5, 5, 5)).palette();
    assert(!black.light && detail::luminance(black.background) < .01f);

    // The background continues the cover's edges, as Apple's does: a red
    // frame around a blue centre gives a red background.
    Cover framed(200, 200, rgb565(200, 20, 20));
    framed.rect(30, 30, 170, 170, rgb565(20, 40, 200));
    const Palette frame = framed.palette();
    check_readable(frame);
    assert(hue_gap(hue(frame.background), hue(0xc81414)) < 20);
    assert(hue_gap(hue(frame.text), hue(0x1428c8)) < 30); // the other prominent colour

    // A small vivid detail on a grey cover (a title, a sky) becomes the accent.
    Cover grey(200, 200, rgb565(60, 60, 60));
    grey.rect(20, 20, 70, 40, rgb565(250, 210, 0));
    const Palette detail_palette = grey.palette();
    check_readable(detail_palette);
    assert(hue_gap(hue(detail_palette.accent), hue(0xfad200)) < 20);
    // Grey covers do not get dim grey text: it is a near-white neutral.
    assert(detail::luminance(detail_palette.text) > .6f);

    // Many busy pseudo-random covers: always readable.
    uint32_t seed = 12345;
    const auto next = [&seed] { seed = seed * 1103515245 + 12345; return (seed >> 8) & 0xffff; };
    for (int i = 0; i < 300; ++i) {
        Cover cover(96, 96, uint16_t(next()));
        for (int block = 0; block < 6; ++block) {
            const unsigned x = next() % 90, y = next() % 90;
            cover.rect(x, y, std::min(96U, x + 4 + next() % 60), std::min(96U, y + 4 + next() % 60), uint16_t(next()));
        }
        check_readable(cover.palette());
    }

    // Colour field: rejects bad buffers; flat fields are flat on average;
    // gradients are smooth (no bands) and every pixel is a valid level.
    assert(!album_palette::render_field(classic, nullptr, 64, 40));
    std::vector<uint16_t> image(256 * 160);
    assert(!album_palette::render_field(classic, image.data(), 4, 4));
    Palette flat{};
    for (uint32_t &cell : flat.field) cell = 0x3a6b9c;
    assert(album_palette::render_field(flat, image.data(), 256, 160));
    const auto level_mean = [&](unsigned bx, unsigned by, unsigned channel) {
        float sum = 0;
        for (unsigned y = by * 4; y < by * 4 + 4; ++y) for (unsigned x = bx * 4; x < bx * 4 + 4; ++x) {
            const uint16_t v = image[y * 256 + x];
            sum += channel == 0 ? (v >> 11) * 255.f / 31 : channel == 1 ? ((v >> 5) & 63) * 255.f / 63 : (v & 31) * 255.f / 31;
        }
        return sum / 16;
    };
    for (unsigned by = 0; by < 40; ++by) for (unsigned bx = 0; bx < 64; ++bx) {
        assert(std::fabs(level_mean(bx, by, 0) - 0x3a) < 2.5f);
        assert(std::fabs(level_mean(bx, by, 1) - 0x6b) < 2.5f);
        assert(std::fabs(level_mean(bx, by, 2) - 0x9c) < 2.5f);
    }
    Palette ramp{};
    for (unsigned i = 0; i < album_palette::kFieldWidth * album_palette::kFieldHeight; ++i)
        ramp.field[i] = (i % album_palette::kFieldWidth) * 9 << 16; // dark red ramp, 0 to 63
    assert(album_palette::render_field(ramp, image.data(), 256, 160));
    for (unsigned by = 0; by < 40; ++by) for (unsigned bx = 1; bx < 64; ++bx) {
        const float step = level_mean(bx, by, 0) - level_mean(bx - 1, by, 0);
        assert(step > -.5f && step < 2.5f); // rises steadily; plain quantising would jump 8 levels at once
    }
}
