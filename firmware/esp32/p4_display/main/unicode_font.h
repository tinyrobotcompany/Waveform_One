#pragma once
#include "lvgl.h"

// Preserve the existing typeface, using the shared Unicode glyph pack only
// when the primary font has no glyph. No text substitution takes place.
const lv_font_t *unicode_font(const lv_font_t *primary, unsigned pixels);
