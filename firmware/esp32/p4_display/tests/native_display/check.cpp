#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <vector>
#include "album_palette.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "display_fonts.h"
#include "display_text.h"
#include "recognition_feedback.h"
#include "search_waveform.h"

constexpr lv_color_t kBackground = LV_COLOR_MAKE(0x08,0x0d,0x0a);
constexpr lv_color_t kForeground = LV_COLOR_MAKE(0xed,0xf3,0xe8);
constexpr lv_color_t kAccent = LV_COLOR_MAKE(0xa8,0xd5,0x7a);
constexpr lv_color_t kSearchAccent = LV_COLOR_MAKE(0xa7,0x9a,0xe8);
constexpr lv_color_t kPanel = LV_COLOR_MAKE(0x12,0x1a,0x13);
constexpr lv_color_t kLine = LV_COLOR_MAKE(0x37,0x48,0x38);
constexpr lv_color_t kMuted = LV_COLOR_MAKE(0xa7,0xb2,0xa7);
lv_obj_t *headline, *artist, *album, *eyebrow, *recognition_status, *recognition_dot;
RecognitionStatus theme_status = RecognitionStatus::Waiting;
album_palette::Palette current_palette{};
bool album_colours=true, searching=false;
RecognitionTrack displayed_track{};
lv_obj_t *artwork=nullptr, *brand_label=nullptr, *appearance_buttons[2]{}, *style_buttons[3]{}, *style_status=nullptr;
void on_style(lv_event_t *) {}
void on_appearance(lv_event_t *) {}
lv_color_t theme_search() { return lv_color_hex(current_palette.search); }
lv_color_t theme_accent() { return lv_color_hex(current_palette.accent); }
#include "native_palette.inc"
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
        const auto *international = display_font(font);
        for (uint32_t cp : {0x2bcU,0x355U,0x6771U,0x4eacU,0xc11cU,0xc6b8U,0x05e9U,0x0639U,
                            0xfecbU,0x1f3b5U,0x1d11eU,0x0939U}) {
            lv_font_glyph_dsc_t glyph{};
            assert(lv_font_get_glyph_dsc(international,&glyph,cp,0));
            assert(!glyph.is_placeholder && glyph.resolved_font != font);
            auto *buffer = lv_draw_buf_create(glyph.box_w,glyph.box_h,LV_COLOR_FORMAT_A8,0);
            assert(buffer && lv_font_get_glyph_bitmap(&glyph,buffer));
            bool ink = false;
            for (uint32_t i = 0; i < buffer->data_size; ++i) ink |= buffer->data[i] != 0;
            assert(ink); lv_draw_buf_destroy(buffer);
        }
        lv_font_glyph_dsc_t missing{};
        assert(!lv_font_get_glyph_dsc(international,&missing,0x10ffff,0));
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
    set_label_text(headline,"東京 · 서울 · Donʼt 🎵");
    set_label_text(artist,"الموسيقى · שלום · हिन्दी");
    set_label_text(album,"世界の音楽 · 𝄞 · Deluxe");
    screenshot("native-international.ppm",display);
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
    if (const char *directory=std::getenv("WAVEFORM_ALBUM_FIXTURES")) {
        lv_obj_add_flag(image,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(mark,LV_OBJ_FLAG_HIDDEN);
        artwork=cover;
        const char *names[]={"monty","emmet","beatles","dexter"};
        const char *titles[]={"A Jamaican in Paris","Universal Truth","Rubber Soul","Other Side of Round Midnight"};
        const char *artists[]={"Monty Alexander","Emmet Cohen","The Beatles","Dexter Gordon"};
        for(unsigned i=0;i<4;++i) {
            std::string base=std::string(directory)+"/"+names[i];
            FILE *palette=std::fopen((base+".palette").c_str(),"r"); assert(palette);
            assert(std::fscanf(palette,"%x %x %x %x %x",&current_palette.background,&current_palette.panel,
                &current_palette.accent,&current_palette.search,&current_palette.line)==5); std::fclose(palette);
            std::vector<uint8_t> rgb(320*320*2);
            FILE *pixels=std::fopen((base+".rgb565").c_str(),"rb"); assert(pixels);
            assert(std::fread(rgb.data(),1,rgb.size(),pixels)==rgb.size()); std::fclose(pixels);
            lv_image_dsc_t descriptor{};
            descriptor.header.magic=LV_IMAGE_HEADER_MAGIC; descriptor.header.cf=LV_COLOR_FORMAT_RGB565;
            descriptor.header.w=320; descriptor.header.h=320; descriptor.header.stride=640;
            descriptor.data_size=rgb.size(); descriptor.data=rgb.data();
            auto *picture=lv_image_create(cover); lv_image_set_src(picture,&descriptor);
            lv_image_set_scale(picture,358*256/320); lv_obj_center(picture);
            std::snprintf(displayed_track.title,sizeof(displayed_track.title),"%s",titles[i]);
            set_label_text(headline,titles[i]); set_label_text(artist,artists[i]); set_label_text(album,"Album colours");
            set_recognition_feedback(RecognitionStatus::Matched,true); paint_palette();
            screenshot(("native-album-"+std::string(names[i])+".ppm").c_str(),display);
            lv_obj_delete(picture); lv_image_cache_drop(&descriptor);
        }
    }
    auto *settings_panel=lv_obj_create(screen);
    lv_obj_set_size(settings_panel,1024,600); lv_obj_set_pos(settings_panel,0,0);
    lv_obj_set_style_bg_color(settings_panel,kBackground,0);
    lv_obj_set_style_border_width(settings_panel,0,0); lv_obj_set_style_pad_all(settings_panel,0,0);
    #include "native_appearance.inc"
    lv_obj_update_layout(settings_panel);
    for(auto *button : appearance_buttons) {
        lv_area_t button_bounds{},label_bounds{};
        lv_obj_get_coords(button,&button_bounds); lv_obj_get_coords(lv_obj_get_child(button,0),&label_bounds);
        assert(button_bounds.y2<255 && button_bounds.x2<535);
        assert(label_bounds.x1>=button_bounds.x1 && label_bounds.x2<=button_bounds.x2);
    }
    screenshot("native-appearance-settings.ppm",display);
    std::puts("PASS: actual LVGL font glyphs and native text layout, apostrophes/accents and feedback states");
}
