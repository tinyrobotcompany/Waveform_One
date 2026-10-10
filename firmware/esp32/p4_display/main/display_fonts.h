#pragma once
#include "lvgl.h"
#include "unicode_font.h"

LV_FONT_DECLARE(waveform_font_16);
LV_FONT_DECLARE(waveform_font_20);
LV_FONT_DECLARE(waveform_font_24);
LV_FONT_DECLARE(waveform_font_32);
LV_FONT_DECLARE(waveform_font_40);
LV_FONT_DECLARE(waveform_font_48);

inline const lv_font_t *display_font(const lv_font_t *font)
{
    if (font == &lv_font_montserrat_16 || font == &waveform_font_16) return unicode_font(&waveform_font_16, 16);
    if (font == &lv_font_montserrat_20 || font == &waveform_font_20) return unicode_font(&waveform_font_20, 20);
    if (font == &lv_font_montserrat_24 || font == &waveform_font_24) return unicode_font(&waveform_font_24, 24);
    if (font == &lv_font_montserrat_32 || font == &waveform_font_32) return unicode_font(&waveform_font_32, 32);
    if (font == &waveform_font_40) return unicode_font(&waveform_font_40, 40);
    if (font == &lv_font_montserrat_48 || font == &waveform_font_48) return unicode_font(&waveform_font_48, 48);
    return font;
}
