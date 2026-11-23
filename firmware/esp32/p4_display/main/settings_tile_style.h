#pragma once
#include "lvgl.h"

// Every settings-hub destination uses the same unselected outline and feedback.
// Selection/toggle styling belongs inside the destination page, not on its tile.
inline void style_settings_tile(lv_obj_t *button, lv_color_t accent, lv_color_t pressed)
{
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, accent, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(button, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(button, 9, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(button, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, pressed, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
}
