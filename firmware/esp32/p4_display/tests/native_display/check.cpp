#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include "display_fonts.h"
#include "display_text.h"
#include "recognition_feedback.h"
#include "search_waveform.h"

constexpr lv_color_t kBackground = LV_COLOR_MAKE(0x08,0x0d,0x0a);
constexpr lv_color_t kForeground = LV_COLOR_MAKE(0xed,0xf3,0xe8);
constexpr lv_color_t kAccent = LV_COLOR_MAKE(0xa8,0xd5,0x7a);
constexpr lv_color_t kSearchAccent = LV_COLOR_MAKE(0xa7,0x9a,0xe8);
constexpr lv_color_t kMuted = LV_COLOR_MAKE(0x84,0x91,0x84);
lv_obj_t *headline, *artist, *album, *eyebrow, *recognition_status, *recognition_dot;
#include "native_helpers.inc"
static uint16_t pixels[1024*600];
static void flush(lv_display_t *display, const lv_area_t *, uint8_t *) { lv_display_flush_ready(display); }
void screenshot(const char *file, lv_display_t *display) {
    lv_refr_now(display);
    FILE *out = std::fopen(file, "wb"); assert(out);
    std::fprintf(out, "P6\n1024 600\n255\n");
    for (uint16_t pixel : pixels) {
        const unsigned char rgb[] = {static_cast<unsigned char>(((pixel>>11)&31)*255/31),
            static_cast<unsigned char>(((pixel>>5)&63)*255/63), static_cast<unsigned char>((pixel&31)*255/31)};
        std::fwrite(rgb, 1, 3, out);
    }
    std::fclose(out);
}
int main() {
    lv_init();
    auto *display = lv_display_create(1024,600);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    auto *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen,kBackground,0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    for (const auto *font : {&waveform_font_16,&waveform_font_20,&waveform_font_24,
                            &waveform_font_32,&waveform_font_40,&waveform_font_48}) {
        for (uint32_t cp : {uint32_t('\''),0x2019U,0x201cU,0x2014U,0x2026U,0xe9U,0xf6U,0x153U,0x2117U,0x266bU}) {
            lv_font_glyph_dsc_t glyph{};
            assert(lv_font_get_glyph_dsc(font,&glyph,cp,0));
            assert(!glyph.is_placeholder && glyph.box_w && glyph.box_h);
            assert(lv_font_get_glyph_static_bitmap(&glyph) != nullptr);
        }
        lv_font_glyph_dsc_t missing{};
        assert(!lv_font_get_glyph_dsc(font,&missing,0x355,0));
    }
    #include "native_panel.inc"
    auto *cover = lv_obj_create(screen);
    lv_obj_set_pos(cover,42,98); lv_obj_set_size(cover,390,390);
    lv_obj_set_style_bg_color(cover,LV_COLOR_MAKE(0x12,0x1a,0x13),0);
    lv_obj_set_style_border_color(cover,kAccent,0);
    auto *mark=make_label(cover,"WAVEFORM\nONE",&lv_font_montserrat_48,kMuted); lv_obj_center(mark);
    set_label_text(headline,"Don’t You Want Me");
    set_label_text(artist,"The Human League"); set_label_text(album,"Dare — Deluxe Edition");
    set_recognition_feedback(RecognitionStatus::Matched,true);
    lv_obj_update_layout(screen);
    lv_area_t bounds{},status_bounds{};
    lv_obj_get_coords(album,&bounds); lv_obj_get_coords(recognition_status,&status_bounds);
    assert(bounds.y2 < status_bounds.y1);
    screenshot("native-apostrophe.ppm",display);
    set_label_text(headline,"In My Time of Dying (Live from Earls Court, 1975)");
    set_label_text(artist,"Beyoncé · Björk · Sigur Rós · Led Zeppelin");
    set_label_text(album,"Cœur — Noël… “Live” «Été» · Deluxe Edition");
    set_recognition_feedback(RecognitionStatus::Identifying,true);
    lv_obj_update_layout(screen);
    lv_obj_get_coords(album,&bounds); lv_obj_get_coords(recognition_status,&status_bounds);
    assert(bounds.y2 < status_bounds.y1);
    screenshot("native-long-title.ppm",display);
    set_label_text(headline,"Listening for music"); set_label_text(artist,"Play a song to bring this screen to life.");
    set_label_text(album,""); set_recognition_feedback(RecognitionStatus::Capturing,false);
    static uint16_t wave_pixels[search_waveform::kPixels];
    assert(search_waveform::render(wave_pixels,search_waveform::kPixels,128));
    static lv_image_dsc_t wave{};
    wave.header.magic=LV_IMAGE_HEADER_MAGIC; wave.header.cf=LV_COLOR_FORMAT_RGB565;
    wave.header.w=search_waveform::kWidth; wave.header.h=search_waveform::kHeight;
    wave.header.stride=search_waveform::kWidth*2;
    wave.data_size=sizeof(wave_pixels); wave.data=reinterpret_cast<uint8_t *>(wave_pixels);
    auto *image=lv_image_create(cover); lv_image_set_src(image,&wave);
    lv_obj_align(image,LV_ALIGN_BOTTOM_MID,0,-26);
    lv_obj_set_style_border_color(cover,kSearchAccent,0);
    screenshot("native-search.ppm",display);
    std::puts("PASS: actual LVGL font glyphs and native text layout, apostrophes/accents and feedback states");
}
