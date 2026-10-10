#pragma once
#include "lvgl.h"

LV_FONT_DECLARE(waveform_font_16);
LV_FONT_DECLARE(waveform_font_20);
LV_FONT_DECLARE(waveform_font_24);
LV_FONT_DECLARE(waveform_font_32);
LV_FONT_DECLARE(waveform_font_40);
LV_FONT_DECLARE(waveform_font_48);

inline const lv_font_t *display_font(const lv_font_t *font)
{
    if (font == &lv_font_montserrat_16) return &waveform_font_16;
    if (font == &lv_font_montserrat_20) return &waveform_font_20;
    if (font == &lv_font_montserrat_24) return &waveform_font_24;
    if (font == &lv_font_montserrat_32) return &waveform_font_32;
    if (font == &lv_font_montserrat_48) return &waveform_font_48;
    return font;
}
