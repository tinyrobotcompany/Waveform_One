#include "owner_credentials_ui.h"
#include "settings_layout.h"
#include "recognition_credentials.h"
#include "display_fonts.h"
#include <cstring>

namespace {
constexpr lv_color_t background = LV_COLOR_MAKE(0x08, 0x0d, 0x0a);
constexpr lv_color_t panel_color = LV_COLOR_MAKE(0x12, 0x1a, 0x13);
constexpr lv_color_t foreground = LV_COLOR_MAKE(0xed, 0xf3, 0xe8);
constexpr lv_color_t accent = LV_COLOR_MAKE(0xa8, 0xd5, 0x7a);
constexpr lv_color_t line = LV_COLOR_MAKE(0x37, 0x48, 0x38);
lv_obj_t *panel, *entry, *keyboard, *status, *remove_label, *tabs[3];
recognition_credentials::Record draft;
unsigned selected = 0;
bool confirm_disable = false, pending_restart = false;
const char *names[] = {"Regional host", "Access key", "Access secret"};
char *field(unsigned index) { return index == 0 ? draft.host : index == 1 ? draft.key : draft.secret; }
void clear_draft() {
    // Clear our copy; LVGL's allocator does not promise forensic RAM erasure.
    volatile unsigned char *bytes = reinterpret_cast<volatile unsigned char *>(&draft);
    for (size_t i = 0; i < sizeof(draft); ++i) bytes[i] = 0;
    draft.version = 1;
    lv_textarea_set_text(entry, "");
}
lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font) {
    auto *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_style_text_font(object, display_font(font), 0);
    lv_obj_set_style_text_color(object, foreground, 0);
    return object;
}
lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int width,
                 lv_event_cb_t callback, const lv_font_t *font = &lv_font_montserrat_16) {
    auto *object = lv_button_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, 48);
    lv_obj_set_style_bg_color(object, panel_color, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_border_color(object, line, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    lv_obj_set_style_bg_color(object, accent, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_center(label(object, text, font));
    lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED, nullptr);
    return object;
}
void remember_field() {
    const char *value = lv_textarea_get_text(entry);
    const size_t length = std::strlen(value);
    if (length < sizeof(draft.host)) std::memcpy(field(selected), value, length + 1);
}
void cancel_confirmation() {
    confirm_disable = false;
    lv_label_set_text(remove_label, "Disable recognition");
}
void select_field(unsigned index) {
    remember_field();
    selected = index;
    cancel_confirmation();
    lv_textarea_set_password_mode(entry, selected != 0);
    lv_textarea_set_text(entry, field(selected));
    lv_textarea_set_placeholder_text(entry, selected == 0 ? "identify-eu-west-1.acrcloud.com" : names[selected]);
    for (unsigned i = 0; i < 3; ++i)
        lv_obj_set_style_border_color(tabs[i], i == selected ? accent : line, 0);
    lv_keyboard_set_textarea(keyboard, entry);
}
void close(lv_event_t *) {
    clear_draft();
    cancel_confirmation();
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}
void open(lv_event_t *) {
    clear_draft();
    selected = 0;
    select_field(0);
    const auto state = recognition_credentials::owner_state();
    const char *message = "Enter all three fields from your ACRCloud project. Existing keys are never displayed.";
    if (state == recognition_credentials::OwnerState::Configured)
        message = "Owner credentials saved. Enter all three fields to replace them.";
    else if (state == recognition_credentials::OwnerState::Disabled)
        message = "Cloud recognition disabled in saved settings. Add credentials to enable it after restart.";
    else if (state == recognition_credentials::OwnerState::Error)
        message = "Could not read saved credentials. Save a complete configuration to repair them.";
    if (pending_restart) message = "Change saved for next restart. Current recognition continues until then.";
    lv_label_set_text(status, message);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel);
}
void save(lv_event_t *) {
    remember_field();
    cancel_confirmation();
    recognition_credentials::Record checked;
    if (!recognition_credentials::owner_record(checked, draft.host, draft.key, draft.secret)) {
        lv_label_set_text(status, "Enter a regional *.acrcloud.com host, access key and secret. No spaces or URL prefix.");
        return;
    }
    const bool saved = recognition_credentials::save_owner(checked);
    volatile unsigned char *bytes = reinterpret_cast<volatile unsigned char *>(&checked);
    for (size_t i = 0; i < sizeof(checked); ++i) bytes[i] = 0;
    clear_draft();
    if (saved) {
        pending_restart = true;
        lv_label_set_text(status, "Saved. Restart after any software update finishes. Account validity is not yet checked.");
    } else lv_label_set_text(status, "Could not save. Previous settings remain. Re-enter the fields to retry.");
}
void disable(lv_event_t *) {
    if (!confirm_disable) {
        confirm_disable = true;
        lv_label_set_text(remove_label, "Confirm disable");
        lv_label_set_text(status, "Disable cloud recognition after restart? Current uploads continue until restart. Tap again to confirm.");
        return;
    }
    const bool saved = recognition_credentials::disable_owner();
    clear_draft();
    cancel_confirmation();
    if (saved) {
        pending_restart = true;
        lv_label_set_text(status, "Disabled for next restart. Restart after updates finish to stop cloud recognition.");
    } else lv_label_set_text(status, "Could not save. Previous recognition settings remain.");
}
}
void owner_credentials_ui_create(lv_obj_t *screen, lv_obj_t *settings) {
    const auto tile = settings_layout::tile(settings_layout::Account);
    auto *entry = button(settings, "Recognition account", tile.x, tile.y, tile.w, open, &lv_font_montserrat_20);
    lv_obj_set_height(entry, tile.h);
    panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 1024, 600);
    lv_obj_center(panel);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, background, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    auto *title = label(panel, "ACRCloud recognition account", &lv_font_montserrat_32);
    lv_obj_set_pos(title, 32, 28);
    button(panel, "Done", 858, 24, 130, close);
    status = label(panel, "", &lv_font_montserrat_16);
    lv_obj_set_pos(status, 32, 88);
    lv_obj_set_width(status, 950);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    for (unsigned i = 0; i < 3; ++i) {
        tabs[i] = button(panel, names[i], 32 + i * 225, 145, 210, [](lv_event_t *event) {
            for (unsigned index = 0; index < 3; ++index)
                if (lv_event_get_target(event) == tabs[index]) select_field(index);
        });
    }
    auto *remove = button(panel, "Disable recognition", 730, 145, 258, disable);
    remove_label = lv_obj_get_child(remove, 0);
    entry = lv_textarea_create(panel);
    lv_obj_set_pos(entry, 32, 210);
    lv_obj_set_size(entry, 660, 52);
    lv_textarea_set_one_line(entry, true);
    lv_textarea_set_max_length(entry, 255);
    lv_textarea_set_accepted_chars(entry, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-");
    lv_textarea_set_password_show_time(entry, 0);
    lv_obj_add_event_cb(entry, [](lv_event_t *) { cancel_confirmation(); }, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_style_bg_color(entry, panel_color, 0);
    lv_obj_set_style_text_color(entry, foreground, 0);
    lv_obj_set_style_border_color(entry, accent, 0);
    button(panel, "Save for next restart", 730, 210, 258, save);
    auto *note = label(panel, "Uses your ACRCloud plan and quota. No credentials are sent to the phone.", &lv_font_montserrat_16);
    lv_obj_set_pos(note, 32, 279);
    keyboard = lv_keyboard_create(panel);
    lv_obj_set_size(keyboard, 1024, 280);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(keyboard, [](lv_event_t *) { select_field((selected + 1) % 3); }, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(keyboard, close, LV_EVENT_CANCEL, nullptr);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}
