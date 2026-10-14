#include "unicode_font.h"
#include "display_fonts.h"
#include <array>
#include <cstdint>
#include <cstring>

#ifdef WAVEFORM_NATIVE_FONT_CHECK
#include "unicode_data.inc"
#else
extern const unsigned char unicode_data[] asm("_binary_waveform_unicode_bin_start");
#endif

namespace {
uint32_t read32(const uint8_t *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
constexpr uint32_t kScalarMask = 0x1fffff;
const uint8_t *find(uint32_t cp)
{
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return nullptr;
    uint32_t low = 0, high = read32(unicode_data + 4);
    while (low < high) {
        const uint32_t mid = low + (high - low) / 2;
        const auto *record = unicode_data + 16 + mid * 8;
        const uint32_t key = read32(record) & kScalarMask;
        if (key < cp) low = mid + 1;
        else if (key > cp) high = mid;
        else return record;
    }
    return nullptr;
}
struct Font {
    lv_font_t primary{};
    lv_font_t fallback{};
    unsigned pixels{};
};

bool descriptor(const lv_font_t *font, lv_font_glyph_dsc_t *glyph, uint32_t cp, uint32_t)
{
    // Format selectors/joiners are layout instructions, not visible letters.
    // This bitmap backend supplies monochrome emoji; don't draw Unifont's
    // diagnostic symbols for invisible controls or variation selectors.
    if (cp == 0x200c || cp == 0x200d || cp == 0x2060 || cp == 0xfeff || cp == 0x34f
        || (cp >= 0xfe00 && cp <= 0xfe0f) || (cp >= 0xe0100 && cp <= 0xe01ef)) {
        glyph->format = LV_FONT_GLYPH_FORMAT_NONE;
        return true;
    }
    const auto *record = find(cp);
    if (!record) return false;
    const auto *config = static_cast<const Font *>(font->dsc);
    const uint32_t flags = read32(record);
    const unsigned width = flags & 0x80000000U ? 16 : 8;
    glyph->box_w = width * config->pixels / 16;
    glyph->box_h = config->pixels;
    glyph->adv_w = flags & 0x40000000U ? 0 : glyph->box_w;
    glyph->ofs_x = flags & 0x40000000U ? -static_cast<int16_t>(glyph->box_w) : 0;
    glyph->ofs_y = -static_cast<int16_t>(config->pixels * 2 / 16);
    glyph->format = LV_FONT_GLYPH_FORMAT_A8;
    glyph->gid.index = cp;
    return true;
}

const void *bitmap(lv_font_glyph_dsc_t *glyph, lv_draw_buf_t *buffer)
{
    if (!buffer || glyph->req_raw_bitmap) return nullptr;
    const auto *record = find(glyph->gid.index);
    if (!record || buffer->header.w < glyph->box_w || buffer->header.h < glyph->box_h
        || buffer->header.stride < glyph->box_w
        || uint64_t(buffer->header.stride) * glyph->box_h > buffer->data_size) return nullptr;
    const unsigned width = read32(record) & 0x80000000U ? 16 : 8;
    const auto *source = unicode_data + read32(unicode_data + 8) + read32(record + 4);
    const unsigned stride = buffer->header.stride;
    // Each draw worker owns its LVGL A8 buffer. Source glyphs are immutable;
    // there is no shared decoder/scratch buffer and no per-glyph heap cache.
    std::memset(buffer->data, 0, buffer->data_size);
    for (unsigned y = 0; y < glyph->box_h; ++y) {
        for (unsigned x = 0; x < glyph->box_w; ++x) {
            const unsigned sx = x * width / glyph->box_w, sy = y * 16 / glyph->box_h;
            buffer->data[y * stride + x] = source[sy * (width / 8) + sx / 8] & (0x80U >> (sx % 8)) ? 255 : 0;
        }
    }
    lv_draw_buf_flush_cache(buffer, nullptr);
    return buffer;
}

// Initialized before screen construction; no mutation after draw tasks start.
struct Fonts {
    std::array<Font, 6> entries;
    Fonts()
    {
        const lv_font_t *base[] = {&waveform_font_16, &waveform_font_20, &waveform_font_24,
                                  &waveform_font_32, &waveform_font_40, &waveform_font_48};
        const unsigned sizes[] = {16, 20, 24, 32, 40, 48};
        for (unsigned i = 0; i < entries.size(); ++i) {
            auto &entry = entries[i];
            entry.pixels = sizes[i]; entry.primary = *base[i];
            entry.fallback.get_glyph_dsc = descriptor;
            entry.fallback.get_glyph_bitmap = bitmap;
            entry.fallback.line_height = base[i]->line_height;
            entry.fallback.base_line = base[i]->base_line;
            entry.fallback.dsc = &entry;
            entry.fallback.fallback = base[i]->fallback; // Preserve existing UI icon glyphs.
            entry.primary.fallback = &entry.fallback;
        }
    }
};
}

const lv_font_t *unicode_font(const lv_font_t *primary, unsigned pixels)
{
    static Fonts fonts;
    for (auto &font : fonts.entries) if (font.pixels == pixels) return &font.primary;
    return primary;
}
