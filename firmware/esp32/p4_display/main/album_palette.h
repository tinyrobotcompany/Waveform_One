#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

// Album artwork colours, modelled on Apple Music's artwork colours: one
// background colour taken from the cover (light or dark, as the cover is),
// four text colours from the cover's other prominent colours (primary,
// secondary, and the two blended towards the background), plus a soft
// colour field for the background, like Apple Music's Now Playing screen.
// Every text colour is checked against every field cell, as the display
// shows it (RGB565), so text stays readable whatever the cover.
// Colours are compared in OKLab, where equal distances look equally different.
namespace album_palette {

// Minimum WCAG contrast ratios. Body text needs 4.5:1; the large heading and
// the artwork border are "large text / graphics" (3:1).
inline constexpr float kTextContrast = 4.5f, kAccentContrast = 3.f;
inline constexpr unsigned kFieldWidth = 8, kFieldHeight = 5;

struct Palette {
    // Classic black: the original Waveform One colours.
    uint32_t background = 0x080d0a, panel = 0x121a13, line = 0x374838;
    uint32_t text = 0xedf3e8, secondary = 0xedf3e8, tertiary = 0xa7b2a7, quaternary = 0xa7b2a7;
    uint32_t accent = 0xa8d57a, search = 0xa79ae8;
    // Background colour field, row-major, top-left first. Classic: flat.
    uint32_t field[kFieldWidth * kFieldHeight] = {
        0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a,
        0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a,
        0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a,
        0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a,
        0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a, 0x080d0a};
    bool light = false; // dark text on a light background

    bool operator==(const Palette &p) const {
        return background == p.background && panel == p.panel && line == p.line && text == p.text
            && secondary == p.secondary && tertiary == p.tertiary && quaternary == p.quaternary
            && accent == p.accent && search == p.search && light == p.light
            && std::equal(std::begin(field), std::end(field), std::begin(p.field));
    }
    bool operator!=(const Palette &p) const { return !(*this == p); }
};

namespace detail {
struct Lab { float l = 0, a = 0, b = 0; };

inline float linear(float v) { return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f); }
inline float gamma(float v) {
    v = std::clamp(v, 0.f, 1.f);
    return v <= .0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.f / 2.4f) - .055f;
}
inline Lab lab(float r, float g, float b) { // sRGB 0..1
    r = linear(r); g = linear(g); b = linear(b);
    const float l = std::cbrt(.4122214708f*r + .5363325363f*g + .0514459929f*b);
    const float m = std::cbrt(.2119034982f*r + .6806995451f*g + .1073969566f*b);
    const float s = std::cbrt(.0883024619f*r + .2817188376f*g + .6299787005f*b);
    return {.2104542553f*l + .7936177850f*m - .0040720468f*s,
            1.9779984951f*l - 2.4285922050f*m + .4505937099f*s,
            .0259040371f*l + .7827717662f*m - .8086757660f*s};
}
inline Lab lab(uint32_t rgb) {
    return lab(((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f);
}
// Out-of-gamut colours keep their lightness and hue and lose chroma.
inline uint32_t rgb(Lab c) {
    for (int attempt = 0; attempt < 24; ++attempt) {
        const float l = std::pow(c.l + .3963377774f*c.a + .2158037573f*c.b, 3.f);
        const float m = std::pow(c.l - .1055613458f*c.a - .0638541728f*c.b, 3.f);
        const float s = std::pow(c.l - .0894841775f*c.a - 1.2914855480f*c.b, 3.f);
        const float r = 4.0767416621f*l - 3.3077115913f*m + .2309699292f*s;
        const float g = -1.2684380046f*l + 2.6097574011f*m - .3413193965f*s;
        const float b = -.0041960863f*l - .7034186147f*m + 1.7076147010f*s;
        if (attempt == 23 || (r > -.001f && r < 1.001f && g > -.001f && g < 1.001f && b > -.001f && b < 1.001f)) {
            return (uint32_t(std::lround(gamma(r) * 255)) << 16) | (uint32_t(std::lround(gamma(g) * 255)) << 8)
                | uint32_t(std::lround(gamma(b) * 255));
        }
        c.a *= .85f; c.b *= .85f;
    }
    return 0;
}
inline float distance(Lab x, Lab y) { return std::hypot(x.l - y.l, x.a - y.a, x.b - y.b); }
inline float chroma(Lab c) { return std::hypot(c.a, c.b); }
inline Lab mix(Lab x, Lab y, float t) { return {x.l + (y.l-x.l)*t, x.a + (y.a-x.a)*t, x.b + (y.b-x.b)*t}; }

// What the panel shows: colours are rounded to RGB565.
inline uint32_t shown(uint32_t c) {
    const unsigned r = ((c >> 16) & 255) >> 3, g = ((c >> 8) & 255) >> 2, b = (c & 255) >> 3;
    return ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31);
}
inline float luminance(uint32_t c) {
    c = shown(c);
    return .2126f * linear(((c >> 16) & 255) / 255.f) + .7152f * linear(((c >> 8) & 255) / 255.f)
        + .0722f * linear((c & 255) / 255.f);
}
inline float contrast(uint32_t x, uint32_t y) {
    const float a = luminance(x), b = luminance(y);
    return (std::max(a, b) + .05f) / (std::min(a, b) + .05f);
}
inline float worst(uint32_t colour, const uint32_t *against, size_t count) {
    float result = 1e9f;
    for (size_t i = 0; i < count; ++i) result = std::min(result, contrast(colour, against[i]));
    return result;
}
// Moves a colour's lightness towards black (dark) or white (light) until it
// reaches `ratio` against every colour in `against`, keeping its hue.
inline uint32_t readable(Lab c, bool dark, const uint32_t *against, size_t count, float ratio) {
    if (worst(rgb(c), against, count) >= ratio) return rgb(c);
    float near = c.l, far = dark ? 0.f : 1.f;
    for (int i = 0; i < 18; ++i) {
        const float middle = (near + far) / 2;
        if (worst(rgb({middle, c.a, c.b}), against, count) >= ratio) far = middle; else near = middle;
    }
    return rgb({far, c.a, c.b});
}

struct Cluster { float weight = 0, edge = 0; Lab sum{}; Lab mean() const { return {sum.l/weight, sum.a/weight, sum.b/weight}; } };
struct Work {
    static constexpr unsigned kGrid = 48, kBins = 512;
    Cluster bins[kBins];
    Cluster clusters[16];
    unsigned cluster_count = 0;
    Lab cells[kFieldWidth * kFieldHeight];
};
} // namespace detail

// `pixels` is the decoder's little-endian RGB565 cover. Returns the Classic
// palette when there is no usable artwork. Allocates 13 kB while it runs.
inline Palette extract(const uint8_t *pixels, unsigned width, unsigned height) {
    using namespace detail;
    if (!pixels || width < 8 || height < 8 || width > 1024 || height > 1024) return {};
    std::unique_ptr<Work> work(new (std::nothrow) Work{});
    if (!work) return {};
    auto at = [&](unsigned x, unsigned y) {
        const size_t offset = (size_t(y) * width + x) * 2;
        const unsigned p = pixels[offset] | (unsigned(pixels[offset + 1]) << 8);
        return lab(((p >> 11) & 31) / 31.f, ((p >> 5) & 63) / 63.f, (p & 31) / 31.f);
    };

    // 1. Sample a 48 x 48 grid into a coarse 8 x 8 x 8 colour histogram. The
    // outer ring of the cover counts separately: Apple's background colour
    // generally continues the artwork's edges.
    constexpr unsigned grid = Work::kGrid;
    float total = 0;
    for (unsigned gy = 0; gy < grid; ++gy) for (unsigned gx = 0; gx < grid; ++gx) {
        const unsigned x = (gx * 2 + 1) * width / (grid * 2), y = (gy * 2 + 1) * height / (grid * 2);
        const Lab c = at(x, y);
        const uint32_t q = rgb(c);
        auto &bin = work->bins[(((q >> 21) & 7) << 6) | (((q >> 13) & 7) << 3) | ((q >> 5) & 7)];
        const bool edge = gx < 4 || gy < 4 || gx >= grid - 4 || gy >= grid - 4;
        bin.weight += 1; bin.edge += edge ? 1 : 0;
        bin.sum.l += c.l; bin.sum.a += c.a; bin.sum.b += c.b;
        total += 1;
    }
    // 2. Merge bins into perceptually distinct colours, largest first.
    for (;;) {
        Cluster *largest = nullptr;
        for (auto &bin : work->bins) if (bin.weight > 0 && (!largest || bin.weight > largest->weight)) largest = &bin;
        if (!largest) break;
        const Lab colour = largest->mean();
        Cluster *into = nullptr;
        for (unsigned i = 0; i < work->cluster_count; ++i)
            if (distance(work->clusters[i].mean(), colour) < .09f) { into = &work->clusters[i]; break; }
        if (!into && work->cluster_count < 16) into = &work->clusters[work->cluster_count++];
        if (into) {
            into->weight += largest->weight; into->edge += largest->edge;
            into->sum.l += largest->sum.l; into->sum.a += largest->sum.a; into->sum.b += largest->sum.b;
        }
        *largest = {};
    }
    const unsigned count = work->cluster_count;
    if (count == 0) return {};

    // 3. Background: the colour that covers most of the edges and the cover.
    unsigned back = 0;
    for (unsigned i = 1; i < count; ++i) {
        const auto score = [&](const Cluster &c) { return c.edge * 3 + c.weight; };
        if (score(work->clusters[i]) > score(work->clusters[back])) back = i;
    }
    Lab background = work->clusters[back].mean();
    // A near-white wall of light is uncomfortable across a room; keep the
    // lightest backgrounds just off white, as Apple does.
    background.l = std::min(background.l, .93f);
    Palette palette{};
    palette.background = rgb(background);
    palette.light = luminance(palette.background) > .18f; // where black text beats white
    const bool dark_text = palette.light;

    // 4. Colour field: the cover's colours spread across the screen and
    // blurred, pulled halfway to the background so text stays calm on it.
    for (unsigned cy = 0; cy < kFieldHeight; ++cy) for (unsigned cx = 0; cx < kFieldWidth; ++cx) {
        Lab sum{}; float n = 0;
        for (unsigned sy = 0; sy < 6; ++sy) for (unsigned sx = 0; sx < 6; ++sx) {
            const Lab c = at(((cx * 6 + sx) * 2 + 1) * width / (kFieldWidth * 12),
                             ((cy * 6 + sy) * 2 + 1) * height / (kFieldHeight * 12));
            sum.l += c.l; sum.a += c.a; sum.b += c.b; n += 1;
        }
        work->cells[cy * kFieldWidth + cx] = mix({sum.l / n, sum.a / n, sum.b / n}, background, .55f);
    }
    for (int pass = 0; pass < 2; ++pass) { // soften: 3 x 3 average
        Lab soft[kFieldWidth * kFieldHeight];
        for (int y = 0; y < int(kFieldHeight); ++y) for (int x = 0; x < int(kFieldWidth); ++x) {
            Lab sum{}; float n = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                const int nx = std::clamp(x + dx, 0, int(kFieldWidth) - 1), ny = std::clamp(y + dy, 0, int(kFieldHeight) - 1);
                const Lab c = work->cells[ny * kFieldWidth + nx];
                sum.l += c.l; sum.a += c.a; sum.b += c.b; n += 1;
            }
            soft[y * kFieldWidth + x] = {sum.l / n, sum.a / n, sum.b / n};
        }
        std::copy(std::begin(soft), std::end(soft), work->cells);
    }
    // Keep every cell on the background's side of the light/dark divide, at
    // least as far from the text as the background itself is.
    // Measured as the panel shows it, not by OKLab lightness, which differs
    // from luminance between hues.
    const float limit = dark_text ? std::min(luminance(palette.background), .55f)
                                  : std::max(luminance(palette.background), .05f);
    for (unsigned i = 0; i < kFieldWidth * kFieldHeight; ++i) {
        Lab c = work->cells[i];
        const auto beyond = [&](float l) {
            const float y = luminance(rgb({l, c.a, c.b}));
            return dark_text ? y < limit : y > limit;
        };
        if (beyond(c.l)) {
            float inside = dark_text ? 1.f : 0.f, outside = c.l;
            for (int step = 0; step < 18; ++step) {
                const float middle = (inside + outside) / 2;
                if (beyond(middle)) outside = middle; else inside = middle;
            }
            c.l = inside;
        }
        palette.field[i] = rgb(c);
    }
    uint32_t surfaces[kFieldWidth * kFieldHeight + 2];
    std::copy(std::begin(palette.field), std::end(palette.field), surfaces);
    surfaces[kFieldWidth * kFieldHeight] = palette.background;

    // 5. Text colours: the cover's other prominent colours, most prominent
    // and most colourful first, made readable on the whole field.
    struct Candidate { Lab colour; float score; };
    Candidate candidates[16]; unsigned candidate_count = 0;
    for (unsigned i = 0; i < count; ++i) {
        const Cluster &c = work->clusters[i];
        const Lab colour = c.mean();
        if (i == back || c.weight < total * .012f || distance(colour, background) < .12f) continue;
        candidates[candidate_count++] = {colour, c.weight / total * (.35f + std::min(chroma(colour), .2f) * 6.f)};
    }
    std::sort(candidates, candidates + candidate_count, [](const Candidate &x, const Candidate &y) { return x.score > y.score; });
    // Fallback: white or black, tinted with the background's hue.
    const Lab neutral{dark_text ? .18f : .97f, background.a * .25f, background.b * .25f};
    const uint32_t *against = surfaces; const size_t against_count = kFieldWidth * kFieldHeight + 1;
    uint32_t chosen[2]; Lab chosen_lab[2]; unsigned found = 0;
    for (unsigned i = 0; i < candidate_count && found < 2; ++i) {
        // Greys read as dim, not as a colour: they become the neutral, at
        // full contrast, as Apple's do.
        const Lab colour = chroma(candidates[i].colour) < .04f ? neutral : candidates[i].colour;
        const uint32_t fitted = readable(colour, dark_text, against, against_count, kTextContrast);
        const Lab shown_lab = lab(fitted);
        if (found == 1 && distance(shown_lab, chosen_lab[0]) < .1f) continue;
        chosen[found] = fitted; chosen_lab[found] = shown_lab; ++found;
    }
    if (found == 0) { chosen[0] = readable(neutral, dark_text, against, against_count, kTextContrast); chosen_lab[0] = lab(chosen[0]); found = 1; }
    palette.text = chosen[0];
    palette.secondary = found > 1 ? chosen[1] : readable(mix(chosen_lab[0], background, .3f), dark_text, against, against_count, kTextContrast);
    // Tertiary and quaternary: primary and secondary softened towards the
    // background, as far as readability allows.
    const auto soften = [&](uint32_t colour) {
        const Lab c = lab(colour);
        for (float t = .4f; t > 0; t -= .05f) {
            const uint32_t candidate = rgb(mix(c, background, t));
            if (worst(candidate, against, against_count) >= kTextContrast) return candidate;
        }
        return colour;
    };
    palette.tertiary = soften(palette.text);
    palette.quaternary = soften(palette.secondary);
    // Accent (heading, artwork border, status dot) and search animation: the
    // cover's most vivid colours, even small ones (a title, a sky), readable
    // as large text. Covers without one use the text colours.
    Lab vivid[2]; unsigned vivid_count = 0;
    for (unsigned pick = 0; pick < 2; ++pick) {
        float best = 0;
        for (unsigned i = 0; i < count; ++i) {
            const Cluster &c = work->clusters[i];
            const Lab colour = c.mean();
            if (c.weight < total * .004f || chroma(colour) < .06f || distance(colour, background) < .12f) continue;
            if (pick == 1 && distance(colour, vivid[0]) < .15f) continue;
            const float score = chroma(colour) * std::sqrt(c.weight / total);
            if (score > best) { best = score; vivid[pick] = colour; }
        }
        if (best > 0) vivid_count = pick + 1; else break;
    }
    palette.accent = vivid_count > 0 ? readable(vivid[0], dark_text, against, against_count, kAccentContrast) : palette.text;
    palette.search = vivid_count > 1 ? readable(vivid[1], dark_text, against, against_count, kAccentContrast) : palette.secondary;
    // Panels (buttons, the empty artwork frame) and lines sit just off the
    // background, towards the text.
    const float towards = dark_text ? -1.f : 1.f;
    palette.panel = rgb({std::clamp(background.l + towards * .06f, 0.f, 1.f), background.a, background.b});
    palette.line = rgb(mix({std::clamp(background.l + towards * .2f, 0.f, 1.f), background.a, background.b},
                           lab(palette.accent), .25f));
    return palette;
}

// Fades the colours (not the field, which cross-fades as an image) from one
// palette to another; `step` runs from 0 to 255.
inline Palette blend(const Palette &from, const Palette &to, unsigned step) {
    const auto mix = [step](uint32_t a, uint32_t b) {
        uint32_t value = 0;
        for (unsigned shift : {0U, 8U, 16U})
            value |= ((((a >> shift) & 255) * (255 - step) + ((b >> shift) & 255) * step) / 255) << shift;
        return value;
    };
    Palette result = to;
    result.background = mix(from.background, to.background); result.panel = mix(from.panel, to.panel);
    result.line = mix(from.line, to.line); result.text = mix(from.text, to.text);
    result.secondary = mix(from.secondary, to.secondary); result.tertiary = mix(from.tertiary, to.tertiary);
    result.quaternary = mix(from.quaternary, to.quaternary);
    result.accent = mix(from.accent, to.accent); result.search = mix(from.search, to.search);
    return result;
}

// Draws the colour field as a smooth little-endian RGB565 image: bilinear
// between the field's cells, with 4 x 4 ordered dithering so the panel's
// 65,536 colours show no bands.
inline bool render_field(const Palette &palette, uint16_t *pixels, unsigned width, unsigned height) {
    if (!pixels || width < kFieldWidth || height < kFieldHeight) return false;
    static constexpr uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    float cells[kFieldWidth * kFieldHeight][3];
    for (unsigned i = 0; i < kFieldWidth * kFieldHeight; ++i) {
        const uint32_t c = palette.field[i];
        // Levels in the panel's own units: 31 red, 63 green, 31 blue.
        cells[i][0] = ((c >> 16) & 255) * 31.f / 255; cells[i][1] = ((c >> 8) & 255) * 63.f / 255;
        cells[i][2] = (c & 255) * 31.f / 255;
    }
    const auto position = [](unsigned at, unsigned size, unsigned cells, unsigned &index, float &t) {
        const float f = std::clamp((at + .5f) * cells / size - .5f, 0.f, cells - 1.f);
        index = std::min(unsigned(f), cells - 2); t = f - index;
    };
    for (unsigned y = 0; y < height; ++y) {
        unsigned row; float ty; position(y, height, kFieldHeight, row, ty);
        float line[kFieldWidth][3];
        for (unsigned x = 0; x < kFieldWidth; ++x) for (unsigned k = 0; k < 3; ++k)
            line[x][k] = cells[row * kFieldWidth + x][k] * (1 - ty) + cells[(row + 1) * kFieldWidth + x][k] * ty;
        for (unsigned x = 0; x < width; ++x) {
            unsigned column; float tx; position(x, width, kFieldWidth, column, tx);
            const float d = (kBayer[y & 3][x & 3] + .5f) / 16;
            unsigned level[3];
            for (unsigned k = 0; k < 3; ++k) {
                const float v = line[column][k] * (1 - tx) + line[column + 1][k] * tx;
                level[k] = std::min(unsigned(v + d), k == 1 ? 63U : 31U);
            }
            pixels[size_t(y) * width + x] = uint16_t((level[0] << 11) | (level[1] << 5) | level[2]);
        }
    }
    return true;
}

} // namespace album_palette
