#include "microphone_privacy.h"
#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/time.h>
#include <time.h>

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "display_fonts.h"
#include "display_text.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "network.h"
#include "setup_guide.h"
#include "recognition.h"
#include "owner_credentials_ui.h"
#include "recognition_feedback.h"
#include "display_schedule.h"
#include "search_waveform.h"
#include "update_feedback.h"
#include "album_palette.h"
#include "usb_controller.h"
#include "music_detector.h"
#include "p4_led.h"
#include "update_service.h"
#include "update_platform.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "waveform_p4";
constexpr lv_color_t kBackground = LV_COLOR_MAKE(0x08, 0x0d, 0x0a);
constexpr lv_color_t kPanel = LV_COLOR_MAKE(0x12, 0x1a, 0x13);
constexpr lv_color_t kForeground = LV_COLOR_MAKE(0xed, 0xf3, 0xe8);
constexpr lv_color_t kAccent = LV_COLOR_MAKE(0xa8, 0xd5, 0x7a);
constexpr lv_color_t kSearchAccent = LV_COLOR_MAKE(0xa7, 0x9a, 0xe8);
constexpr lv_color_t kMuted = LV_COLOR_MAKE(0xa7, 0xb2, 0xa7);
constexpr lv_color_t kLine = LV_COLOR_MAKE(0x37, 0x48, 0x38);

lv_obj_t *artwork = nullptr;
lv_obj_t *artwork_mark = nullptr;
lv_obj_t *search_wave = nullptr;
lv_timer_t *search_wave_timer = nullptr;
lv_image_dsc_t search_wave_descriptor{};
uint16_t *search_wave_pixels = nullptr;
unsigned search_wave_phase = 0;
bool search_wave_active = false;
lv_obj_t *eyebrow = nullptr;
lv_obj_t *brand_label = nullptr;
lv_obj_t *headline = nullptr;
lv_obj_t *artist = nullptr;
lv_obj_t *album = nullptr;
lv_obj_t *recognition_status = nullptr;
lv_obj_t *recognition_dot = nullptr;
lv_obj_t *artwork_image = nullptr;
lv_image_dsc_t artwork_descriptor{};
uint8_t *artwork_pixels = nullptr;
const uint8_t *artwork_source = nullptr;
RecognitionTrack displayed_track{};
lv_obj_t *settings_panel = nullptr;
lv_obj_t *brightness_label = nullptr;
lv_obj_t *brightness_slider = nullptr;
lv_obj_t *welcome_label = nullptr;
lv_obj_t *clock_label = nullptr, *idle_clock = nullptr;
lv_obj_t *display_panel = nullptr, *night_enabled = nullptr, *night_start = nullptr, *night_end = nullptr;
lv_obj_t *night_level = nullptr, *night_level_label = nullptr, *night_status = nullptr;
display_schedule::Config night_config;
bool night_storage_ready = true;
std::atomic_int effective_brightness{100};
std::atomic<int64_t> manual_brightness_until{0};
void create_display_preferences(lv_obj_t *screen);

lv_obj_t *wifi_indicator = nullptr;
lv_obj_t *wifi_header_button = nullptr;
lv_obj_t *network_qr = nullptr;
lv_obj_t *setup_panel = nullptr, *setup_qr = nullptr, *setup_instructions = nullptr;
lv_obj_t *setup_step = nullptr;
setup_guide::Guide setup;
std::string setup_address;
void refresh_setup_guide();
void create_setup_guide(lv_obj_t *screen);
void on_open_setup(lv_event_t *);

lv_obj_t *network_heading = nullptr;
lv_obj_t *network_instructions = nullptr;
lv_obj_t *forget_phones_button = nullptr, *forget_phones_text = nullptr;
bool forget_phones_confirm = false;
lv_obj_t *wifi_dropdown = nullptr;
lv_obj_t *wifi_password = nullptr;
lv_obj_t *wifi_keyboard = nullptr;
lv_obj_t *wifi_status = nullptr;
lv_obj_t *wifi_entry_panel = nullptr;
lv_obj_t *wifi_entry_network = nullptr;
lv_obj_t *wifi_entry_status = nullptr;
lv_obj_t *wifi_entry_show_label = nullptr;
lv_obj_t *wifi_entry_cancel = nullptr;
lv_obj_t *wifi_entry_connect = nullptr;
lv_obj_t *wifi_entry_spinner = nullptr;
lv_obj_t *network_qr_placeholder = nullptr;
lv_obj_t *style_status = nullptr;
lv_obj_t *style_buttons[3]{};
control_policy::WifiNetworkList wifi_networks{};
bool wifi_connecting = false;
std::atomic_int current_brightness{100};

bool searching = false;
bool album_colours = true;
bool wifi_online = false;
RecognitionStatus theme_status = RecognitionStatus::Waiting;
album_palette::Palette cover_palette{}, current_palette{}, target_palette{}, fade_from{};
lv_obj_t *appearance_buttons[2]{};
lv_obj_t *wifi_header_text = nullptr, *settings_button = nullptr, *settings_text = nullptr, *header_rule = nullptr;
// The album colour field fills the screen behind everything. field_back
// shows the current field; field_front holds the previous one on top while
// it fades out. Each buffer is a full 1024 x 600 RGB565 image in PSRAM.
constexpr unsigned kScreenWidth = 1024, kScreenHeight = 600;
lv_obj_t *field_back = nullptr, *field_front = nullptr;
lv_image_dsc_t field_images[2]{};
uint16_t *field_pixels[2]{};
unsigned field_shown = 0;
lv_color_t theme_accent() { return lv_color_hex(current_palette.accent); }
lv_color_t theme_search() { return lv_color_hex(current_palette.search); }
// The colour functions stay out of line: they run from the recognition task's
// display callback, and inlined there their frames (palettes, field rows)
// would sit on that task's nearly full stack for every status update.
[[gnu::noinline]] void paint_palette()
{
    const auto &p = current_palette;
    const auto colour = [](uint32_t value) { return lv_color_hex(value); };
    lv_obj_set_style_bg_color(lv_screen_active(), colour(p.background), 0);
    lv_obj_set_style_bg_color(artwork, colour(p.panel), 0);
    lv_obj_set_style_border_color(artwork, displayed_track.title[0] ? theme_accent()
        : searching ? theme_search() : colour(p.line), 0);
    lv_obj_set_style_text_color(artwork_mark, colour(p.tertiary), 0);
    lv_obj_set_style_text_color(eyebrow, theme_accent(), 0);
    lv_obj_set_style_text_color(headline, colour(p.text), 0);
    lv_obj_set_style_text_color(artist, colour(p.secondary), 0);
    lv_obj_set_style_text_color(album, colour(p.tertiary), 0);
    lv_obj_set_style_text_color(recognition_status, colour(p.quaternary), 0);
    lv_obj_set_style_bg_color(recognition_dot,
        recognition_feedback::searching(theme_status) ? theme_search() : theme_accent(), 0);
    if (brand_label == nullptr) return; // the host preview has no header
    lv_obj_set_style_text_color(brand_label, theme_accent(), 0);
    lv_obj_set_style_text_color(welcome_label, colour(p.tertiary), 0);
    lv_obj_set_style_text_color(clock_label, colour(p.text), 0);
    if (idle_clock) lv_obj_set_style_text_color(idle_clock, colour(p.text), 0);
    lv_obj_set_style_text_color(wifi_indicator, wifi_online ? theme_accent() : colour(p.tertiary), 0);
    lv_obj_set_style_bg_color(wifi_header_button, colour(p.panel), 0);
    lv_obj_set_style_border_color(wifi_header_button, theme_accent(), 0);
    lv_obj_set_style_text_color(wifi_header_text, colour(p.text), 0);
    lv_obj_set_style_bg_color(settings_button, colour(p.panel), 0);
    lv_obj_set_style_border_color(settings_button, colour(p.line), 0);
    lv_obj_set_style_text_color(settings_text, colour(p.text), 0);
    lv_obj_set_style_bg_color(header_rule, colour(p.line), 0);
}
void fade_palette(void *, int32_t progress)
{
    current_palette = album_palette::blend(fade_from, target_palette, static_cast<unsigned>(progress));
    paint_palette();
    if (field_front == nullptr) return;
    lv_obj_set_style_image_opa(field_front, static_cast<lv_opa_t>(255 - progress), 0);
    if (progress >= 255) lv_obj_add_flag(field_front, LV_OBJ_FLAG_HIDDEN);
}
// Draws the new field into the buffer not on screen, under the old one,
// which then fades out. A change during a fade jumps to the field it was
// fading to first, so the buffer being drawn is never the one on show.
[[gnu::noinline]] void show_field(const album_palette::Palette &palette)
{
    if (field_back == nullptr) return; // no buffers: the plain background colour shows
    lv_image_set_src(field_front, &field_images[field_shown]);
    lv_obj_set_style_image_opa(field_front, LV_OPA_COVER, 0);
    lv_obj_remove_flag(field_front, LV_OBJ_FLAG_HIDDEN);
    const unsigned next = 1 - field_shown;
    album_palette::render_field(palette, field_pixels[next], kScreenWidth, kScreenHeight);
    lv_image_cache_drop(&field_images[next]);
    lv_image_set_src(field_back, &field_images[next]);
    field_shown = next;
}
void create_field(lv_obj_t *screen)
{
    for (unsigned i = 0; i < 2; ++i) {
        field_pixels[i] = static_cast<uint16_t *>(heap_caps_aligned_alloc(64,
            kScreenWidth * kScreenHeight * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (field_pixels[i] == nullptr) {
            ESP_LOGW(kTag, "Colour field allocation failed; using plain backgrounds");
            heap_caps_free(field_pixels[0]); field_pixels[0] = field_pixels[1] = nullptr;
            return;
        }
        auto &image = field_images[i];
        image.header.magic = LV_IMAGE_HEADER_MAGIC;
        image.header.cf = LV_COLOR_FORMAT_RGB565;
        image.header.w = kScreenWidth;
        image.header.h = kScreenHeight;
        image.header.stride = kScreenWidth * sizeof(uint16_t);
        image.data_size = kScreenWidth * kScreenHeight * sizeof(uint16_t);
        image.data = reinterpret_cast<uint8_t *>(field_pixels[i]);
    }
    album_palette::render_field(current_palette, field_pixels[0], kScreenWidth, kScreenHeight);
    field_back = lv_image_create(screen);
    field_front = lv_image_create(screen);
    for (lv_obj_t *field : {field_back, field_front}) {
        lv_obj_set_pos(field, 0, 0);
        lv_obj_remove_flag(field, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_image_set_src(field_back, &field_images[0]);
    lv_obj_add_flag(field_front, LV_OBJ_FLAG_HIDDEN);
}
[[gnu::noinline]] void apply_palette()
{
    const auto next = album_colours ? cover_palette : album_palette::Palette{};
    for (unsigned i=0; i<2; ++i) if (appearance_buttons[i])
        lv_obj_set_style_border_color(appearance_buttons[i], album_colours == (i==0) ? kAccent : kLine, 0);
    // Status callbacks must not redraw the whole background or restart its fade.
    if (next == target_palette) return;
    lv_anim_delete(nullptr, fade_palette);
    fade_from = current_palette; target_palette = next;
    show_field(next);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_exec_cb(&animation, fade_palette);
    lv_anim_set_values(&animation, 0, 255);
    lv_anim_set_duration(&animation, 700);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}
bool remote_set_appearance(bool value)
{
    if (!bsp_display_lock(1000)) return false;
    const bool saved = network_set_appearance(value);
    if (saved) { album_colours = value; apply_palette(); }
    bsp_display_unlock();
    return saved;
}
void on_appearance(lv_event_t *event)
{
    const bool value = reinterpret_cast<intptr_t>(lv_event_get_user_data(event)) == 0;
    if (network_set_appearance(value)) { album_colours = value; apply_palette(); }
}

void pulse_artwork(void *object, int32_t opacity)
{
    lv_obj_set_style_border_opa(static_cast<lv_obj_t *>(object), opacity, 0);
}
void paint_search_wave()
{
    search_waveform::render(search_wave_pixels, search_waveform::kPixels, search_wave_phase,
                            current_palette.search, current_palette.panel);
    lv_image_cache_drop(&search_wave_descriptor);
    // One fixed redraw rectangle, including the border. Never invalidate
    // individual moving bars in the adapter's partial-buffer repair path.
    lv_obj_invalidate(artwork);
}
void tick_search_wave(lv_timer_t *)
{
    search_wave_phase = (search_wave_phase + 16) % 1024;
    paint_search_wave();
}
void set_search_wave(bool active)
{
    if (search_wave == nullptr || active == search_wave_active) return;
    search_wave_active = active;
    if (!active) {
        lv_timer_pause(search_wave_timer);
        lv_obj_add_flag(search_wave, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    search_wave_phase = 0;
    paint_search_wave();
    lv_obj_remove_flag(search_wave, LV_OBJ_FLAG_HIDDEN);
    lv_timer_resume(search_wave_timer);
}
void set_searching(bool active, bool has_track = false)
{
    set_search_wave(active && lv_obj_has_flag(artwork_image, LV_OBJ_FLAG_HIDDEN));
    lv_obj_set_style_border_color(artwork, has_track ? theme_accent() : active ? theme_search() : lv_color_hex(current_palette.line), 0);
    if (searching == active) return;
    searching = active;
    lv_anim_delete(artwork, pulse_artwork);
    lv_obj_set_style_border_width(artwork, active ? 3 : 2, 0);
    lv_obj_set_style_border_opa(artwork, LV_OPA_COVER, 0);
    if (!active) return;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, artwork);
    lv_anim_set_exec_cb(&animation, pulse_artwork);
    lv_anim_set_values(&animation, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_duration(&animation, 1200);
    lv_anim_set_playback_duration(&animation, 1200);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}

[[gnu::noinline]] void clear_recognition_track()
{
    lv_label_set_text(headline, "Listening for music");
    lv_label_set_text(artist, "Play a song to bring this screen to life.");
    lv_label_set_text(album, "");
    lv_obj_add_flag(artwork_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(artwork_mark, LV_OBJ_FLAG_HIDDEN);
    lv_image_cache_drop(&artwork_descriptor);
    heap_caps_free(artwork_pixels);
    artwork_descriptor = {};
    artwork_pixels = nullptr; artwork_source = nullptr; displayed_track = {};
    cover_palette = {};
    network_set_palette(cover_palette);
    apply_palette();
}

void log_board_identity()
{
    esp_chip_info_t chip{};
    uint32_t flash_size = 0;
    esp_chip_info(&chip);

    ESP_LOGI(kTag, "Waveform One ESP32-P4 display bring-up");
    ESP_LOGI(kTag, "Target=%s silicon=v%d.%d cores=%d",
             CONFIG_IDF_TARGET, chip.revision / 100, chip.revision % 100, chip.cores);
    if (esp_flash_get_size(nullptr, &flash_size) == ESP_OK) {
        ESP_LOGI(kTag, "Flash=%" PRIu32 " MB", flash_size / (1024U * 1024U));
    }
    ESP_LOGI(kTag, "PSRAM=%s free=%u bytes",
             esp_psram_is_initialized() ? "ready" : "not initialized",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void set_label_text(lv_obj_t *label, const char *text)
{
    const std::string rendered = display_text::renderable(text);
    lv_label_set_text(label, rendered.c_str());
}

void set_recognition_feedback(RecognitionStatus status, bool has_track)
{
    lv_obj_set_style_bg_color(recognition_dot,
        recognition_feedback::searching(status) ? theme_search() : theme_accent(), 0);
    theme_status = status;
    set_label_text(recognition_status, microphone_privacy::muted() ? "Microphone muted" : recognition_feedback::text(status, has_track));
}

lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                     lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, display_font(font), 0);
    set_label_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t *microphone_panel = nullptr, *microphone_button_label = nullptr, *microphone_message = nullptr;
void refresh_microphone() {
    set_label_text(microphone_button_label, microphone_privacy::storage_error.load() ? "Save muted setting" : microphone_privacy::muted() ? "Unmute microphone" : "Mute microphone");
    set_label_text(microphone_message, microphone_privacy::storage_error.load()
        ? "Microphone muted. Setting could not be saved.\nRetry before restarting; the previous saved setting may return."
        : microphone_privacy::muted()
            ? "Microphone muted. Capture and music recognition are paused.\nThis setting stays muted after restart and updates."
            : "Microphone on. Local audio capture is enabled.");
}
void create_microphone_controls(lv_obj_t *screen) {
    auto *open = lv_button_create(settings_panel);
    lv_obj_set_size(open, 210, 36);
    lv_obj_align(open, LV_ALIGN_TOP_LEFT, 245, 215);
    lv_obj_set_style_bg_color(open, kPanel, 0);
    lv_obj_set_style_border_width(open, 2, 0);
    lv_obj_set_style_border_color(open, kLine, 0);
    lv_obj_center(make_label(open, "Microphone", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(open, [](lv_event_t *) { refresh_microphone(); lv_obj_remove_flag(microphone_panel, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(microphone_panel); }, LV_EVENT_CLICKED, nullptr);
    microphone_panel = lv_obj_create(screen);
    lv_obj_set_size(microphone_panel, 1024, 600);
    lv_obj_center(microphone_panel);
    lv_obj_set_style_bg_color(microphone_panel, kBackground, 0);
    lv_obj_set_style_bg_opa(microphone_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(microphone_panel, 0, 0);
    lv_obj_remove_flag(microphone_panel, LV_OBJ_FLAG_SCROLLABLE);
    auto *title = make_label(microphone_panel, "Microphone", &lv_font_montserrat_48, kForeground);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 48, 35);
    microphone_message = make_label(microphone_panel, "", &lv_font_montserrat_20, kForeground);
    lv_obj_set_width(microphone_message, 900);
    lv_obj_align(microphone_message, LV_ALIGN_TOP_LEFT, 48, 145);
    auto *toggle = lv_button_create(microphone_panel);
    lv_obj_set_size(toggle, 340, 65); lv_obj_align(toggle, LV_ALIGN_TOP_LEFT, 48, 260);
    lv_obj_set_style_bg_color(toggle, kPanel, 0);
    lv_obj_set_style_border_width(toggle, 2, 0); lv_obj_set_style_border_color(toggle, kAccent, 0);
    microphone_button_label = make_label(toggle, "", &lv_font_montserrat_20, kForeground);
    lv_obj_center(microphone_button_label);
    lv_obj_add_event_cb(toggle, [](lv_event_t *) { microphone_privacy::request(microphone_privacy::storage_error.load() || !microphone_privacy::muted()); clear_recognition_track(); set_recognition_feedback(RecognitionStatus::Waiting, false); refresh_microphone(); }, LV_EVENT_CLICKED, nullptr);
    auto *explanation = make_label(microphone_panel,
        "Muting stops sound-reactive LEDs and new recognition uploads.\nAudio already sent cannot be recalled. This is a software mute;\nit does not physically disconnect or remove power from the microphones.",
        &lv_font_montserrat_20, kMuted);
    lv_obj_set_width(explanation, 900); lv_obj_align(explanation, LV_ALIGN_TOP_LEFT, 48, 380);
    auto *close = lv_button_create(microphone_panel);
    lv_obj_set_size(close, 110, 52); lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -48, 40);
    lv_obj_set_style_bg_color(close, kPanel, 0);
    lv_obj_center(make_label(close, "Done", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(close, [](lv_event_t *) { lv_obj_add_flag(microphone_panel, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, nullptr);
    refresh_microphone(); lv_obj_add_flag(microphone_panel, LV_OBJ_FLAG_HIDDEN);
}

void on_open_settings(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        lv_obj_remove_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
        network_request_scan();
    }
}

void on_close_settings(lv_event_t *event)
{
    forget_phones_confirm = false;
    if (forget_phones_text && !lv_obj_has_state(forget_phones_button, LV_STATE_DISABLED))
        lv_label_set_text(forget_phones_text, "Forget paired phones");
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        setup.settings_closed(); refresh_setup_guide();
        lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void on_wifi_password(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const auto *network = control_policy::selected_wifi_network(
        wifi_networks, lv_dropdown_get_selected(wifi_dropdown));
    if (network == nullptr) {
        lv_label_set_text(wifi_status, "Choose a home Wi-Fi network first.");
        return;
    }
    lv_label_set_text_fmt(wifi_entry_network, "Network: %s", network->ssid);
    wifi_connecting = false;
    lv_textarea_set_text(wifi_password, "");
    lv_textarea_set_password_mode(wifi_password, true);
    lv_label_set_text(wifi_entry_show_label, "Show");
    lv_label_set_text(wifi_entry_status, network->open
                                             ? "Open network. Leave the password empty."
                                             : "Enter the network password.");
    lv_obj_clear_state(wifi_entry_cancel, LV_STATE_DISABLED);
    lv_obj_clear_state(wifi_entry_connect, LV_STATE_DISABLED);
    lv_obj_add_flag(wifi_entry_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(wifi_entry_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(wifi_entry_panel);
    lv_keyboard_set_textarea(wifi_keyboard, wifi_password);
    lv_obj_add_state(wifi_password, LV_STATE_FOCUSED);
}

void on_wifi_password_visibility(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || wifi_connecting) return;
    const bool hidden = lv_textarea_get_password_mode(wifi_password);
    lv_textarea_set_password_mode(wifi_password, !hidden);
    lv_label_set_text(wifi_entry_show_label, hidden ? "Hide" : "Show");
    lv_keyboard_set_textarea(wifi_keyboard, wifi_password);
    lv_obj_add_state(wifi_password, LV_STATE_FOCUSED);
}

void on_wifi_modal_cancel(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if ((code != LV_EVENT_CLICKED && code != LV_EVENT_CANCEL) || wifi_connecting) return;
    lv_obj_add_flag(wifi_entry_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_state(wifi_password, LV_STATE_FOCUSED);
}

void on_wifi_refresh(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    lv_label_set_text(wifi_status, "Scanning for networks…");
    network_request_scan();
}

void on_wifi_connect(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if ((code != LV_EVENT_CLICKED && code != LV_EVENT_READY) || wifi_connecting) return;
    const auto *network = control_policy::selected_wifi_network(
        wifi_networks, lv_dropdown_get_selected(wifi_dropdown));
    const char *password = lv_textarea_get_text(wifi_password);
    if (network == nullptr) {
        lv_label_set_text(wifi_status, "Choose a home Wi-Fi network first.");
        return;
    }
    if (!control_policy::valid_wifi_password(password, network->open)) {
        lv_label_set_text(wifi_entry_status,
                          network->open ? "This network is open. Leave the password empty."
                                        : "Wi-Fi passwords are 8 to 63 characters.");
        return;
    }
    if (network_configure_home(*network, password)) {
        wifi_connecting = true;
        lv_label_set_text(wifi_status, "Checking Wi-Fi credentials…");
        lv_label_set_text(wifi_entry_status, "Connecting… Checking the password.");
        lv_obj_add_state(wifi_entry_cancel, LV_STATE_DISABLED);
        lv_obj_add_state(wifi_entry_connect, LV_STATE_DISABLED);
        lv_obj_remove_flag(wifi_entry_spinner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(wifi_status, "A Wi-Fi connection is already in progress.");
        lv_label_set_text(wifi_entry_status, "Please wait and try again.");
    }
}

void on_brightness(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    const int value = lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(event)));
    if (!control_policy::valid_brightness(value)) return;
    const esp_err_t result = bsp_display_brightness_set(value);
    if (result == ESP_OK) {
        current_brightness.store(value);
        effective_brightness.store(value);
        network_set_display_brightness(value);
        manual_brightness_until.store(esp_timer_get_time() + 60000000);
        network_set_brightness(value);
        lv_label_set_text_fmt(brightness_label, "%d%%", value);
    } else {
        lv_slider_set_value(brightness_slider, current_brightness.load(), LV_ANIM_OFF);
        ESP_LOGW(kTag, "Brightness update failed: %s", esp_err_to_name(result));
    }
}

void set_style_visual(LedStyle style)
{
    const int selected = style == LedStyle::Classic ? 0 : style == LedStyle::Mirrored ? 1 : 2;
    for (int index = 0; index < 3; ++index) {
        lv_obj_set_style_border_color(style_buttons[index], index == selected ? kAccent : kLine, 0);
        lv_obj_set_style_text_color(style_buttons[index], index == selected ? kAccent : kForeground, 0);
    }
}

void on_style(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const auto style = static_cast<LedStyle>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    const StyleRequestResult result = (p4_led_enabled() ? p4_led_set_style(style) : usb_controller_set_style(style));
    lv_label_set_text(style_status, result == StyleRequestResult::Queued
                                        ? "Applying…"
                                        : "LED controller is not available.");
}

bool remote_set_style(LedStyle style)
{
    const bool queued = (p4_led_enabled() ? p4_led_set_style(style) : usb_controller_set_style(style)) == StyleRequestResult::Queued;
    if (bsp_display_lock(1000)) {
        lv_label_set_text(style_status, queued ? "Applying…" : "LED controller is not available.");
        bsp_display_unlock();
    }
    return queued;
}

bool remote_set_brightness(int percent)
{
    if (!control_policy::valid_brightness(percent) || !bsp_display_lock(1000)) return false;
    const esp_err_t result = bsp_display_brightness_set(percent);
    if (result == ESP_OK) {
        current_brightness.store(percent);
        effective_brightness.store(percent);
        manual_brightness_until.store(esp_timer_get_time() + 60000000);
        network_set_brightness(percent);
        network_set_display_brightness(percent);
        lv_label_set_text_fmt(brightness_label, "%d%%", percent);
        lv_slider_set_value(brightness_slider, percent, LV_ANIM_OFF);
    } else ESP_LOGW(kTag, "Remote brightness failed: %s", esp_err_to_name(result));
    bsp_display_unlock();
    return result == ESP_OK;
}

void remote_set_name(const char *name)
{
    network_set_name(name);
    if (bsp_display_lock(1000)) {
        const std::string greeting = std::string("Welcome, ") + name;
        set_label_text(welcome_label, greeting.c_str());
        bsp_display_unlock();
    }
}

void remote_set_time(std::time_t epoch)
{
    timeval value{};
    value.tv_sec = epoch;
    settimeofday(&value, nullptr);
}

void update_clock(lv_timer_t *)
{
    const std::time_t now = std::time(nullptr);
    const bool valid_time = now >= 1700000000;
    tm local{};
    if (valid_time) {
        localtime_r(&now, &local);
        lv_label_set_text_fmt(clock_label, "%02d:%02d", local.tm_hour, local.tm_min);
        lv_label_set_text_fmt(idle_clock, "%02d:%02d", local.tm_hour, local.tm_min);
    }
    const bool idle = !recognition_music_heard() && !displayed_track.title[0] && !search_wave_active;
    if (valid_time && idle) {
        lv_obj_remove_flag(idle_clock, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(artwork_mark, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(idle_clock, LV_OBJ_FLAG_HIDDEN);
        if (!displayed_track.title[0]) lv_obj_remove_flag(artwork_mark, LV_OBJ_FLAG_HIDDEN);
    }
    const bool interaction = lv_display_get_inactive_time(nullptr) < 60000 ||
        esp_timer_get_time() < manual_brightness_until.load();
    const int target = display_schedule::brightness(night_config,
        valid_time ? local.tm_hour * 60 + local.tm_min : -1,
        idle, interaction, current_brightness.load());
    if (target != effective_brightness.load() && bsp_display_brightness_set(target) == ESP_OK) {
        effective_brightness.store(target);
        network_set_display_brightness(target);
    }
}

void update_controller_status(LedControllerState state)
{
    recognition_set_controller(state == LedControllerState::ConnectedClassic
        || state == LedControllerState::ConnectedMirrored || state == LedControllerState::ConnectedWaterfall);
    const char *text = "waiting for USB";
    switch (state) {
    case LedControllerState::Waiting:
        break;
    case LedControllerState::Connecting:
        text = "LED: connecting...";
        break;
    case LedControllerState::ConnectedClassic:
        text = "LED: connected  /  Classic";
        break;
    case LedControllerState::ConnectedMirrored:
        text = "LED: connected  /  Mirrored";
        break;
    case LedControllerState::ConnectedWaterfall:
        text = "LED: connected  /  Waterfall";
        break;
    case LedControllerState::ProtocolError:
        text = "LED: connected, control unavailable";
        break;
    case LedControllerState::Disconnected:
        text = "LED: disconnected";
        break;
    }
    LedStyle acknowledged{};
    if (control_policy::acknowledged_style(state, acknowledged)) network_set_style(acknowledged);
    if (bsp_display_lock(1000)) {
        if (control_policy::acknowledged_style(state, acknowledged)) {
            set_style_visual(acknowledged);
            lv_label_set_text(style_status, "Applied");
        } else if (state == LedControllerState::ProtocolError ||
                   state == LedControllerState::Disconnected) {
            lv_label_set_text(style_status, "LED controller unavailable");
        }
        bsp_display_unlock();
    }
    ESP_LOGI(kTag, "LED controller: %s", text);
}

void phone_pairing_forgotten(bool saved)
{
    if (!bsp_display_lock(-1)) return;
    lv_obj_remove_state(forget_phones_button, LV_STATE_DISABLED);
    lv_label_set_text(forget_phones_text, "Forget paired phones");
    lv_label_set_text(network_instructions, saved ? "Paired phones forgotten.\nScan the new QR to connect."
        : "Could not save removal.\nPhones remain paired.\nPlease try again.");
    bsp_display_unlock();
}

void on_forget_phones(lv_event_t *)
{
    if (!forget_phones_confirm) {
        forget_phones_confirm = true;
        lv_label_set_text(forget_phones_text, "Tap again to forget all");
        return;
    }
    forget_phones_confirm = false;
    if (network_forget_paired_phones()) {
        lv_obj_add_state(forget_phones_button, LV_STATE_DISABLED);
        lv_label_set_text(forget_phones_text, "Forgetting phones…");
    } else lv_label_set_text(forget_phones_text, "Busy — try again");
}

void remote_set_network(NetworkMode mode, const char *address)
{
    recognition_set_network(mode == NetworkMode::HomeWifi, address);
    if (!bsp_display_lock(1000)) return;
    setup_address = mode == NetworkMode::HomeWifi && address ? address : "";
    setup.network(!setup_address.empty() ? setup_guide::Network::Ready :
        mode == NetworkMode::Reconnecting ? setup_guide::Network::Reconnecting : setup_guide::Network::Unconfigured);
    refresh_setup_guide();
    wifi_online = mode == NetworkMode::HomeWifi;
    paint_palette();
    if (mode == NetworkMode::Unconfigured) {
        lv_obj_align(wifi_indicator, LV_ALIGN_TOP_RIGHT, -350, 28);
        lv_obj_add_flag(network_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(network_qr_placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(wifi_header_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(network_heading, "PHONE REMOTE");
        lv_label_set_text(network_instructions,
                          "Connect Waveform One to your\nhome Wi-Fi in Settings. The\napp QR will appear here.");
        lv_label_set_text(wifi_status, "Choose your home Wi-Fi network.");
    } else if (mode == NetworkMode::Reconnecting) {
        lv_obj_align(wifi_indicator, LV_ALIGN_TOP_RIGHT, -260, 28);
        lv_obj_add_flag(network_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(network_qr_placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(wifi_header_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(network_heading, "PHONE REMOTE");
        lv_label_set_text(network_instructions,
                          "Reconnecting to home Wi-Fi.\nPaired phones reconnect\nat the same address.");
        lv_label_set_text(wifi_status, "Reconnecting to home Wi-Fi…");
    } else {
        lv_obj_align(wifi_indicator, LV_ALIGN_TOP_RIGHT, -260, 28);
        lv_qrcode_set_data(network_qr, address);
        lv_obj_remove_flag(network_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(network_qr_placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(wifi_header_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(network_heading, "PHONE REMOTE");
        lv_label_set_text(network_instructions,
                          "Scan to connect your phone.\nPairing lasts 90 days\non this network address.");
        lv_label_set_text(wifi_status, "Connected to home Wi-Fi.");
    }
    bsp_display_unlock();
}

void remote_set_networks(const control_policy::WifiNetworkList &networks)
{
    if (!bsp_display_lock(1000)) return;
    wifi_networks = networks;
    std::string options;
    for (std::size_t index = 0; index < networks.count; ++index) {
        if (!options.empty()) options.push_back('\n');
        options += networks.items[index].ssid;
    }
    if (options.empty()) options = "No networks found";
    lv_dropdown_set_options(wifi_dropdown, options.c_str());
    lv_label_set_text(wifi_status, networks.count == 0
                                       ? "No supported networks found. Tap Scan to retry."
                                       : "Select a network and enter its password.");
    bsp_display_unlock();
}

void remote_set_wifi_configuration(WifiConfigurationState state)
{
    if (!bsp_display_lock(1000)) return;
    wifi_connecting = false;
    lv_obj_add_flag(wifi_entry_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_state(wifi_entry_cancel, LV_STATE_DISABLED);
    lv_obj_remove_state(wifi_entry_connect, LV_STATE_DISABLED);
    setup.joined(state == WifiConfigurationState::Connected);
    refresh_setup_guide();
    if (state == WifiConfigurationState::Connected) {
        lv_label_set_text(wifi_entry_status, "Connected to home Wi-Fi.");
        lv_label_set_text(wifi_status, "Connected to home Wi-Fi.");
        lv_obj_add_flag(wifi_entry_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_state(wifi_password, LV_STATE_FOCUSED);
    } else if (state == WifiConfigurationState::Failed) {
        lv_label_set_text(wifi_entry_status, "Could not connect. Check the password and try again.");
        lv_label_set_text(wifi_status, "Wi-Fi connection failed. No credentials were saved.");
    } else {
        lv_label_set_text(wifi_entry_status, "Storage recovery required. Credentials were preserved.");
        lv_label_set_text(wifi_status, "Storage recovery required. See the device log.");
    }
    bsp_display_unlock();
}

lv_obj_t *updates_panel = nullptr, *updates_status = nullptr, *updates_version = nullptr;
lv_obj_t *updates_progress = nullptr, *updates_result = nullptr;
lv_obj_t *update_buttons[3]{};
update_feedback::Pending update_pending;
lv_obj_t *update_wave = nullptr;
lv_image_dsc_t update_wave_descriptor{};
uint16_t *update_wave_pixels = nullptr;
unsigned update_wave_phase = 0;
bool update_wave_active = false;

void tick_update_wave(lv_timer_t *)
{
    if (!update_wave || !update_wave_active || lv_obj_has_flag(updates_panel, LV_OBJ_FLAG_HIDDEN)) return;
    update_wave_phase = (update_wave_phase + 16) % 1024;
    search_waveform::render(update_wave_pixels, search_waveform::kPixels, update_wave_phase, 0xa8d57a, 0x080d0a);
    lv_image_cache_drop(&update_wave_descriptor);
    lv_obj_invalidate(update_wave);
}

std::atomic<unsigned> ui_health_ticks{0};

void update_boot_guard(void *)
{
    esp_ota_img_states_t state;
    const bool trial = esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK
        && state == ESP_OTA_IMG_PENDING_VERIFY;
    if (trial) {
        for (unsigned seconds = 0; seconds < 120; ++seconds) {
            // Local rendering and the initialized OTA worker are sufficient; internet is optional.
            if (ui_health_ticks.load() >= 10 && waveform_update_local_ready() && recognition_local_ready() && waveform_update_validate_trial()) {
                vTaskDelete(nullptr);
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        ESP_LOGE(kTag, "Trial boot did not complete local health checks; restarting for rollback");
        esp_restart();
    }
    vTaskDelete(nullptr);
}

void refresh_updates(lv_timer_t *)
{
    ++ui_health_ticks;
    waveform_update_status_t value{};
    waveform_update_status(&value);
    lv_label_set_text_fmt(updates_version, "Installed: %s    Available: %s",
        value.installed[0] ? value.installed : "Starting", value.available[0] ? value.available : "None");

    lv_label_set_text(updates_result, value.last_install_message);
    lv_bar_set_value(updates_progress, value.progress, LV_ANIM_OFF);
    const bool worker_busy = value.state == UPDATE_CHECKING || value.state == UPDATE_DOWNLOADING
        || value.state == UPDATE_VERIFYING || value.state == UPDATE_RESTARTING;
    const bool busy = update_pending.active(esp_timer_get_time(), worker_busy, value.message);
    update_wave_active = busy;
    if (update_wave) {
        if (busy) lv_obj_remove_flag(update_wave, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(update_wave, LV_OBJ_FLAG_HIDDEN);
    }
    if (worker_busy || !busy) lv_label_set_text(updates_status, value.message);
    for (unsigned i = 0; i < 3; ++i) {
        const bool disabled = busy || (i == 0 && value.countdown_seconds > 0)
            || (i == 1 && !value.available[0]);
        if (disabled) lv_obj_add_state(update_buttons[i], LV_STATE_DISABLED);
        else lv_obj_remove_state(update_buttons[i], LV_STATE_DISABLED);
    }
}

void on_update_action(lv_event_t *event)
{
    const char *action = static_cast<const char *>(lv_event_get_user_data(event));
    waveform_update_status_t before{};
    waveform_update_status(&before);
    const bool queued = waveform_update_request(action);
    if (queued) {
        update_pending.begin(esp_timer_get_time(), before.message);
        refresh_updates(nullptr);
        lv_label_set_text(updates_status, strcmp(action, "check") == 0 ? "Checking for updates…"
            : strcmp(action, "install") == 0 ? "Requesting installation…" : "Postponing installation…");
    } else lv_label_set_text(updates_status, "Update service busy. Please try again.");
}

void style_update_button(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, kPanel, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, kAccent, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(button, 9, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, kLine, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, kPanel, LV_STATE_DISABLED);
    lv_obj_set_style_border_color(button, kLine, LV_STATE_DISABLED);
    lv_obj_set_style_opa(button, LV_OPA_50, LV_STATE_DISABLED);
}

void refresh_setup_guide()
{
    if (!setup_panel) return;
    if (setup.visible()) {
        lv_obj_remove_flag(setup_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(setup_panel);
    } else lv_obj_add_flag(setup_panel, LV_OBJ_FLAG_HIDDEN);
    if (setup.qr_ready() && !setup_address.empty()) {
        lv_qrcode_set_data(setup_qr, setup_address.c_str());
        lv_obj_remove_flag(setup_qr, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(setup_step, "Step 2 of 2 — Connect your phone");
        lv_label_set_text(setup_instructions,
            "1. Connect your phone to the same home Wi-Fi.\n\n"
            "2. Scan this QR with your phone camera.\n\n"
            "3. Open the link to control Waveform One.\n\n"
            "No app download is needed. If a link expires,\nscan the current QR again.");
    } else {
        lv_obj_add_flag(setup_qr, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(setup_step, setup.reconnecting() ? "Reconnecting to home Wi-Fi" : "Step 1 of 2 — Connect to home Wi-Fi");
        lv_label_set_text(setup_instructions, setup.reconnecting()
            ? "Waiting for the saved network.\n\nThe QR will return when connected.\n\nTo use another network, choose Wi-Fi below."
            : "1. Tap Choose Wi-Fi below.\n\n2. Select your home network in Settings.\n\n3. Enter its password on the touchscreen.\n\nAfter connection, return here to scan the QR.\nYou can also set up your phone later.");
    }
}

void on_open_setup(lv_event_t *) { setup.open(); refresh_setup_guide(); }

void create_setup_guide(lv_obj_t *screen)
{
    auto *open = lv_button_create(settings_panel);
    lv_obj_set_size(open, 180, 52);
    style_update_button(open);
    lv_obj_align(open, LV_ALIGN_TOP_LEFT, 420, 26);
    lv_obj_center(make_label(open, "Phone setup", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(open, on_open_setup, LV_EVENT_CLICKED, nullptr);

    setup_panel = lv_obj_create(screen);
    lv_obj_set_size(setup_panel, 1024, 600);
    lv_obj_center(setup_panel);
    lv_obj_set_style_bg_color(setup_panel, kBackground, 0);
    lv_obj_set_style_bg_opa(setup_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(setup_panel, 0, 0);
    lv_obj_set_style_pad_all(setup_panel, 0, 0);
    lv_obj_clear_flag(setup_panel, LV_OBJ_FLAG_SCROLLABLE);
    auto *title = make_label(setup_panel, "Set up your phone", &lv_font_montserrat_32, kForeground);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 48, 35);
    setup_step = make_label(setup_panel, "Starting Wi-Fi…", &lv_font_montserrat_20, kAccent);
    lv_obj_align(setup_step, LV_ALIGN_TOP_LEFT, 48, 95);
    setup_qr = lv_qrcode_create(setup_panel);
    lv_qrcode_set_size(setup_qr, 300);
    lv_qrcode_set_dark_color(setup_qr, lv_color_black());
    lv_qrcode_set_light_color(setup_qr, lv_color_white());
    lv_qrcode_set_quiet_zone(setup_qr, true);
    lv_obj_align(setup_qr, LV_ALIGN_TOP_LEFT, 48, 165);
    lv_obj_add_flag(setup_qr, LV_OBJ_FLAG_HIDDEN);
    setup_instructions = make_label(setup_panel, "", &lv_font_montserrat_20, kForeground);
    lv_obj_set_width(setup_instructions, 570);
    lv_label_set_long_mode(setup_instructions, LV_LABEL_LONG_WRAP);
    lv_obj_align(setup_instructions, LV_ALIGN_TOP_LEFT, 400, 155);
    auto *wifi = lv_button_create(setup_panel);
    lv_obj_set_size(wifi, 220, 52);
    style_update_button(wifi);
    lv_obj_align(wifi, LV_ALIGN_BOTTOM_LEFT, 400, -45);
    lv_obj_center(make_label(wifi, "Choose Wi-Fi", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(wifi, [](lv_event_t *) {
        setup.choose_wifi(); refresh_setup_guide();
        lv_obj_remove_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
        network_request_scan();
    }, LV_EVENT_CLICKED, nullptr);
    auto *done = lv_button_create(setup_panel);
    lv_obj_set_size(done, 220, 52);
    style_update_button(done);
    lv_obj_align(done, LV_ALIGN_BOTTOM_LEFT, 650, -45);
    lv_obj_center(make_label(done, "Done / later", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(done, [](lv_event_t *) {
        setup.dismiss(); refresh_setup_guide();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(setup_panel, LV_OBJ_FLAG_HIDDEN);
}

void create_updates_screen(lv_obj_t *screen)
{
    auto *open = lv_button_create(settings_panel);
    lv_obj_set_size(open, 220, 52);
    lv_obj_set_style_bg_opa(open, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(open, kLine, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(open, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(open, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(open, kAccent, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(open, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_align(open, LV_ALIGN_TOP_RIGHT, -180, 26);
    lv_obj_center(make_label(open, "Software updates", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(open, [](lv_event_t *) { lv_obj_remove_flag(updates_panel, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, nullptr);
    updates_panel = lv_obj_create(screen);
    lv_obj_set_size(updates_panel, 1024, 600);
    lv_obj_center(updates_panel);
    lv_obj_set_style_bg_color(updates_panel, kBackground, 0);
    lv_obj_set_style_bg_opa(updates_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(updates_panel, 0, 0);
    lv_obj_remove_flag(updates_panel, LV_OBJ_FLAG_SCROLLABLE);
    auto *title = make_label(updates_panel, "Software updates", &lv_font_montserrat_48, kForeground);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 32, 30);
    auto *close = lv_button_create(updates_panel);
    style_update_button(close);
    lv_obj_set_size(close, 120, 52);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -32, 26);
    lv_obj_center(make_label(close, "Done", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(close, [](lv_event_t *) { lv_obj_add_flag(updates_panel, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, nullptr);
    updates_version = make_label(updates_panel, "Starting…", &lv_font_montserrat_20, kMuted);
    lv_obj_align(updates_version, LV_ALIGN_TOP_LEFT, 32, 120);
    updates_status = make_label(updates_panel, "", &lv_font_montserrat_24, kForeground);
    lv_obj_set_width(updates_status, 900);
    lv_label_set_long_mode(updates_status, LV_LABEL_LONG_WRAP);
    lv_obj_align(updates_status, LV_ALIGN_TOP_LEFT, 32, 175);
    update_wave_pixels = static_cast<uint16_t *>(heap_caps_aligned_alloc(64,
        search_waveform::kPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (update_wave_pixels) {
        search_waveform::render(update_wave_pixels, search_waveform::kPixels, 0, 0xa8d57a, 0x080d0a);
        update_wave_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        update_wave_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
        update_wave_descriptor.header.w = search_waveform::kWidth;
        update_wave_descriptor.header.h = search_waveform::kHeight;
        update_wave_descriptor.header.stride = search_waveform::kWidth * sizeof(uint16_t);
        update_wave_descriptor.data_size = search_waveform::kPixels * sizeof(uint16_t);
        update_wave_descriptor.data = reinterpret_cast<uint8_t *>(update_wave_pixels);
        update_wave = lv_image_create(updates_panel);
        lv_image_set_src(update_wave, &update_wave_descriptor);
        lv_obj_align(update_wave, LV_ALIGN_TOP_MID, 0, 235);
        lv_obj_add_flag(update_wave, LV_OBJ_FLAG_HIDDEN);
        lv_timer_create(tick_update_wave, 40, nullptr);
    }
    updates_progress = lv_bar_create(updates_panel);
    lv_obj_set_style_bg_color(updates_progress, kLine, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(updates_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(updates_progress, kAccent, LV_PART_INDICATOR);
    lv_obj_set_size(updates_progress, 900, 20);
    lv_obj_align(updates_progress, LV_ALIGN_TOP_LEFT, 32, 325);
    const char *labels[] = {"Check now", "Install now", "Postpone 24 hours"};
    static const char *actions[] = {"check", "install", "postpone"};
    for (unsigned i = 0; i < 3; ++i) {
        auto *button = update_buttons[i] = lv_button_create(updates_panel);
        style_update_button(button);
        lv_obj_set_size(button, 280, 60);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 32 + 310 * i, 350);
        lv_obj_center(make_label(button, labels[i], &lv_font_montserrat_20, kForeground));
        lv_obj_add_event_cb(button, on_update_action, LV_EVENT_CLICKED, const_cast<char *>(actions[i]));
    }
    updates_result = make_label(updates_panel, "", &lv_font_montserrat_16, kMuted);
    lv_obj_set_width(updates_result, 900);
    lv_label_set_long_mode(updates_result, LV_LABEL_LONG_WRAP);
    lv_obj_align(updates_result, LV_ALIGN_TOP_LEFT, 32, 425);
    auto *help = make_label(updates_panel, "Keep power connected while installing.\nRelease notes and schedule settings are on your paired phone.", &lv_font_montserrat_20, kMuted);
    lv_obj_align(help, LV_ALIGN_TOP_LEFT, 32, 490);
    lv_obj_add_flag(updates_panel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(refresh_updates, 1000, nullptr);
}

void create_product_screen()
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, kBackground, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    create_field(screen);

    lv_obj_t *brand = brand_label = make_label(screen, "WAVEFORM ONE", &lv_font_montserrat_20, kAccent);
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 42, 28);

    welcome_label = make_label(screen, "Welcome, Simon", &lv_font_montserrat_16, kMuted);
    lv_obj_align(welcome_label, LV_ALIGN_TOP_LEFT, 240, 31);

    clock_label = make_label(screen, "--:--", &lv_font_montserrat_20, kForeground);
    lv_obj_align(clock_label, LV_ALIGN_TOP_RIGHT, -190, 28);
    lv_obj_add_flag(clock_label, LV_OBJ_FLAG_HIDDEN);

    wifi_indicator = make_label(screen, LV_SYMBOL_WIFI, &lv_font_montserrat_20, kMuted);
    lv_obj_align(wifi_indicator, LV_ALIGN_TOP_RIGHT, -350, 28);

    wifi_header_button = lv_button_create(screen);
    lv_obj_set_size(wifi_header_button, 150, 46);
    lv_obj_set_style_bg_color(wifi_header_button, kPanel, 0);
    lv_obj_set_style_border_color(wifi_header_button, kAccent, 0);
    lv_obj_set_style_border_width(wifi_header_button, 1, 0);
    lv_obj_set_style_radius(wifi_header_button, 9, 0);
    lv_obj_align(wifi_header_button, LV_ALIGN_TOP_RIGHT, -180, 15);
    lv_obj_add_event_cb(wifi_header_button, on_open_settings, LV_EVENT_CLICKED, nullptr);
    wifi_header_text = make_label(wifi_header_button, "Connect Wi-Fi",
                                            &lv_font_montserrat_16, kForeground);
    lv_obj_center(wifi_header_text);

    settings_button = lv_button_create(screen);
    lv_obj_set_size(settings_button, 120, 46);
    lv_obj_set_style_bg_color(settings_button, kPanel, 0);
    lv_obj_set_style_border_color(settings_button, kLine, 0);
    lv_obj_set_style_border_width(settings_button, 1, 0);
    lv_obj_set_style_radius(settings_button, 9, 0);
    lv_obj_align(settings_button, LV_ALIGN_TOP_RIGHT, -42, 15);
    lv_obj_add_event_cb(settings_button, on_open_settings, LV_EVENT_CLICKED, nullptr);
    settings_text = make_label(settings_button, "Settings", &lv_font_montserrat_16,
                                         kForeground);
    lv_obj_center(settings_text);

    header_rule = lv_obj_create(screen);
    lv_obj_set_size(header_rule, 940, 2);
    lv_obj_set_style_border_width(header_rule, 0, 0);
    lv_obj_set_style_bg_color(header_rule, kLine, 0);
    lv_obj_align(header_rule, LV_ALIGN_TOP_MID, 0, 70);

    artwork = lv_obj_create(screen);
    lv_obj_set_size(artwork, 390, 390);
    lv_obj_set_style_border_color(artwork, kLine, 0);
    lv_obj_set_style_border_width(artwork, 2, 0);
    lv_obj_set_style_radius(artwork, 14, 0);
    lv_obj_set_style_bg_color(artwork, kPanel, 0);
    lv_obj_set_style_bg_opa(artwork, LV_OPA_COVER, 0);
    lv_obj_clear_flag(artwork, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(artwork, LV_ALIGN_TOP_LEFT, 42, 98);

    artwork_mark = make_label(artwork, "WAVEFORM\nONE", &lv_font_montserrat_48, kMuted);
    lv_obj_set_style_text_align(artwork_mark, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(artwork_mark);
    idle_clock = make_label(artwork, "--:--", &lv_font_montserrat_48, kForeground);
    lv_obj_center(idle_clock);
    lv_obj_add_flag(idle_clock, LV_OBJ_FLAG_HIDDEN);

    search_wave_pixels = static_cast<uint16_t *>(heap_caps_aligned_alloc(64,
        search_waveform::kPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (search_wave_pixels != nullptr) {
        search_waveform::render(search_wave_pixels, search_waveform::kPixels, 0);
        search_wave_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        search_wave_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
        search_wave_descriptor.header.w = search_waveform::kWidth;
        search_wave_descriptor.header.h = search_waveform::kHeight;
        search_wave_descriptor.header.stride = search_waveform::kWidth * sizeof(uint16_t);
        search_wave_descriptor.data_size = search_waveform::kPixels * sizeof(uint16_t);
        search_wave_descriptor.data = reinterpret_cast<uint8_t *>(search_wave_pixels);
        search_wave = lv_image_create(artwork);
        lv_image_set_src(search_wave, &search_wave_descriptor);
        lv_obj_align(search_wave, LV_ALIGN_BOTTOM_MID, 0, -26);
        lv_obj_add_flag(search_wave, LV_OBJ_FLAG_HIDDEN);
        search_wave_timer = lv_timer_create(tick_search_wave, 40, nullptr);
        lv_timer_pause(search_wave_timer);
    } else {
        ESP_LOGW(kTag, "Search animation allocation failed; keeping border feedback");
    }


    lv_obj_t *information = lv_obj_create(screen);
    lv_obj_set_size(information, 510, 432);
    lv_obj_set_style_border_width(information, 0, 0);
    lv_obj_set_style_bg_opa(information, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(information, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(information, LV_ALIGN_TOP_RIGHT, -42, 98);

    lv_obj_set_style_pad_all(information, 0, 0);
    lv_obj_set_style_pad_top(information, 14, 0);
    lv_obj_set_style_pad_row(information, 8, 0);
    lv_obj_set_flex_flow(information, LV_FLEX_FLOW_COLUMN);

    eyebrow = make_label(information, "NOW PLAYING", &lv_font_montserrat_48, kAccent);


    headline = make_label(information, "Listening for music", &waveform_font_40, kForeground);
    lv_obj_set_width(headline, 490);
    lv_label_set_long_mode(headline, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(headline, 108, 0);


    artist = make_label(information, "Play a song to bring this screen to life.",
                        &lv_font_montserrat_32, kForeground);
    lv_obj_set_width(artist, 490);
    lv_label_set_long_mode(artist, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(artist, 86, 0);


    album = make_label(information, "",
                       &lv_font_montserrat_24, kMuted);
    lv_obj_set_width(album, 490);
    lv_label_set_long_mode(album, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(album, 68, 0);


    recognition_status = make_label(information,
        "Play a song to get started",
        &lv_font_montserrat_20, kMuted);
    lv_obj_add_flag(recognition_status, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(recognition_status, LV_ALIGN_BOTTOM_LEFT, 18, -14);
    lv_obj_set_width(recognition_status, 472);
    lv_label_set_long_mode(recognition_status, LV_LABEL_LONG_WRAP);
    recognition_dot = lv_obj_create(information);
    lv_obj_remove_style_all(recognition_dot);
    lv_obj_set_size(recognition_dot, 8, 8);
    lv_obj_add_flag(recognition_dot, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_remove_flag(recognition_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(recognition_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(recognition_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(recognition_dot, kAccent, 0);
    lv_obj_align(recognition_dot, LV_ALIGN_BOTTOM_LEFT, 0, -24);
    artwork_image = lv_image_create(artwork);
    lv_obj_center(artwork_image);
    lv_obj_add_flag(artwork_image, LV_OBJ_FLAG_HIDDEN);

    settings_panel = lv_obj_create(screen);
    lv_obj_set_size(settings_panel, 1024, 600);
    lv_obj_set_style_bg_color(settings_panel, kBackground, 0);
    lv_obj_set_style_bg_opa(settings_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(settings_panel, 0, 0);
    lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(settings_panel, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *settings_title = make_label(settings_panel, "Settings", &lv_font_montserrat_48,
                                          kForeground);
    lv_obj_align(settings_title, LV_ALIGN_TOP_LEFT, 42, 30);

    lv_obj_t *close = lv_button_create(settings_panel);
    lv_obj_set_size(close, 120, 52);
    lv_obj_set_style_bg_color(close, kPanel, 0);
    lv_obj_set_style_border_color(close, kAccent, 0);
    lv_obj_set_style_border_width(close, 1, 0);
    lv_obj_set_style_radius(close, 9, 0);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -42, 26);
    lv_obj_add_event_cb(close, on_close_settings, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *close_text = make_label(close, "Done", &lv_font_montserrat_16, kForeground);
    lv_obj_center(close_text);

    owner_credentials_ui_create(screen, settings_panel);

    lv_obj_t *profile_heading = make_label(settings_panel, "PROFILE", &lv_font_montserrat_16,
                                           kAccent);
    lv_obj_align(profile_heading, LV_ALIGN_TOP_LEFT, 48, 125);
    lv_obj_t *profile = make_label(settings_panel, "Name: Simon",
                                   &lv_font_montserrat_20, kForeground);
    lv_obj_set_style_text_line_space(profile, 12, 0);
    lv_obj_align(profile, LV_ALIGN_TOP_LEFT, 48, 160);

    lv_obj_t *style_heading = make_label(settings_panel, "LED STYLE", &lv_font_montserrat_16,
                                         kAccent);
    lv_obj_align(style_heading, LV_ALIGN_TOP_LEFT, 48, 255);
    constexpr const char *style_names[] = {"Classic", "Mirrored", "Waterfall"};
    for (int index = 0; index < 3; ++index) {
        style_buttons[index] = lv_button_create(settings_panel);
        lv_obj_set_size(style_buttons[index], 130, 60);
        lv_obj_set_style_bg_color(style_buttons[index], kPanel, 0);
        lv_obj_set_style_border_color(style_buttons[index], kLine, 0);
        lv_obj_set_style_border_width(style_buttons[index], 2, 0);
        lv_obj_set_style_radius(style_buttons[index], 9, 0);
        lv_obj_align(style_buttons[index], LV_ALIGN_TOP_LEFT, 48 + index * 145, 290);
        lv_obj_add_event_cb(style_buttons[index], on_style, LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(static_cast<intptr_t>(index)));
        lv_obj_t *label = make_label(style_buttons[index], style_names[index],
                                     &lv_font_montserrat_16, kForeground);
        lv_obj_center(label);
    }
    create_microphone_controls(screen);

    style_status = make_label(settings_panel, "Waiting for LED controller…",
                              &lv_font_montserrat_16, kMuted);
    lv_obj_align(style_status, LV_ALIGN_TOP_LEFT, 48, 355);

    lv_obj_t *appearance_heading = make_label(settings_panel, "APPEARANCE", &lv_font_montserrat_16, kAccent);
    lv_obj_align(appearance_heading, LV_ALIGN_TOP_LEFT, 245, 125);
    constexpr const char *appearance_names[] = {"Album colours", "Classic black"};
    for (unsigned i=0; i<2; ++i) {
        appearance_buttons[i] = lv_button_create(settings_panel);
        lv_obj_set_size(appearance_buttons[i], 135, 52);
        lv_obj_set_style_bg_color(appearance_buttons[i], kPanel, 0);
        lv_obj_set_style_border_width(appearance_buttons[i], 2, 0);
        lv_obj_set_style_border_color(appearance_buttons[i], i==0 ? kAccent : kLine, 0);
        lv_obj_align(appearance_buttons[i], LV_ALIGN_TOP_LEFT, 245+int(i)*140, 158);
        lv_obj_add_event_cb(appearance_buttons[i], on_appearance, LV_EVENT_CLICKED,
            reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_center(make_label(appearance_buttons[i], appearance_names[i], &lv_font_montserrat_16, kForeground));
    }

    lv_obj_t *display_heading = make_label(settings_panel, "DISPLAY", &lv_font_montserrat_16,
                                           kAccent);
    lv_obj_align(display_heading, LV_ALIGN_TOP_LEFT, 535, 125);
    lv_obj_t *brightness_text = make_label(settings_panel, "Brightness", &lv_font_montserrat_20,
                                           kForeground);
    lv_obj_align(brightness_text, LV_ALIGN_TOP_LEFT, 535, 162);
    brightness_label = make_label(settings_panel, "100%", &lv_font_montserrat_20, kAccent);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_RIGHT, -60, 162);
    brightness_slider = lv_slider_create(settings_panel);
    lv_obj_set_size(brightness_slider, 425, 54);
    lv_slider_set_range(brightness_slider, 10, 100);
    lv_slider_set_value(brightness_slider, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, kLine, LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, kAccent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, kForeground, LV_PART_KNOB);
    lv_obj_set_style_pad_all(brightness_slider, 12, LV_PART_MAIN);
    lv_obj_set_style_width(brightness_slider, 34, LV_PART_KNOB);
    lv_obj_set_style_height(brightness_slider, 34, LV_PART_KNOB);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_LEFT, 535, 205);
    lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *wifi_heading = make_label(settings_panel, "HOME WI-FI",
                                         &lv_font_montserrat_16, kAccent);
    lv_obj_align(wifi_heading, LV_ALIGN_TOP_LEFT, 535, 285);

    wifi_dropdown = lv_dropdown_create(settings_panel);
    lv_dropdown_set_options(wifi_dropdown, "Scanning…");
    lv_obj_set_size(wifi_dropdown, 315, 54);
    lv_obj_set_style_bg_color(wifi_dropdown, kPanel, 0);
    lv_obj_set_style_text_color(wifi_dropdown, kForeground, 0);
    lv_obj_set_style_border_color(wifi_dropdown, kLine, 0);
    lv_obj_align(wifi_dropdown, LV_ALIGN_TOP_LEFT, 535, 315);

    lv_obj_t *refresh = lv_button_create(settings_panel);
    lv_obj_set_size(refresh, 95, 54);
    lv_obj_set_style_bg_color(refresh, kPanel, 0);
    lv_obj_set_style_border_color(refresh, kLine, 0);
    lv_obj_set_style_border_width(refresh, 1, 0);
    lv_obj_align(refresh, LV_ALIGN_TOP_LEFT, 865, 315);
    lv_obj_add_event_cb(refresh, on_wifi_refresh, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *refresh_text = make_label(refresh, "Scan", &lv_font_montserrat_16, kForeground);
    lv_obj_center(refresh_text);

    lv_obj_t *password_button = lv_button_create(settings_panel);
    lv_obj_set_size(password_button, 205, 54);
    lv_obj_set_style_bg_color(password_button, kPanel, 0);
    lv_obj_set_style_border_color(password_button, kAccent, 0);
    lv_obj_set_style_border_width(password_button, 1, 0);
    lv_obj_align(password_button, LV_ALIGN_TOP_LEFT, 535, 382);
    lv_obj_add_event_cb(password_button, on_wifi_password, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *password_text = make_label(password_button, "Enter password",
                                         &lv_font_montserrat_16, kForeground);
    lv_obj_center(password_text);

    wifi_status = make_label(settings_panel, "Scanning for networks…",
                             &lv_font_montserrat_16, kMuted);
    lv_obj_set_width(wifi_status, 425);
    lv_label_set_long_mode(wifi_status, LV_LABEL_LONG_WRAP);
    lv_obj_align(wifi_status, LV_ALIGN_TOP_LEFT, 535, 455);

    network_heading = make_label(settings_panel, "PHONE REMOTE",
                                 &lv_font_montserrat_16, kAccent);
    lv_obj_align(network_heading, LV_ALIGN_TOP_LEFT, 48, 390);
    network_qr = lv_qrcode_create(settings_panel);
    lv_qrcode_set_size(network_qr, 145);
    lv_qrcode_set_dark_color(network_qr, kBackground);
    lv_qrcode_set_light_color(network_qr, kForeground);
    lv_qrcode_set_quiet_zone(network_qr, true);
    lv_obj_align(network_qr, LV_ALIGN_TOP_LEFT, 48, 420);
    lv_obj_add_flag(network_qr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(network_qr, on_open_setup, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(network_qr, LV_OBJ_FLAG_HIDDEN);
    network_qr_placeholder = lv_obj_create(settings_panel);
    lv_obj_set_size(network_qr_placeholder, 145, 145);
    lv_obj_set_style_bg_color(network_qr_placeholder, kPanel, 0);
    lv_obj_set_style_border_color(network_qr_placeholder, kLine, 0);
    lv_obj_set_style_border_width(network_qr_placeholder, 1, 0);
    lv_obj_set_style_radius(network_qr_placeholder, 8, 0);
    lv_obj_clear_flag(network_qr_placeholder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(network_qr_placeholder, LV_ALIGN_TOP_LEFT, 48, 420);
    lv_obj_add_flag(network_qr_placeholder, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(network_qr_placeholder, on_open_setup, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *qr_waiting = make_label(network_qr_placeholder, "APP QR\nAFTER WI-FI",
                                      &lv_font_montserrat_16, kMuted);
    lv_obj_set_style_text_align(qr_waiting, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(qr_waiting);
    network_instructions = make_label(settings_panel, "Starting Wi-Fi…",
                                      &lv_font_montserrat_20, kForeground);
    lv_obj_set_style_text_line_space(network_instructions, 8, 0);
    lv_obj_align(network_instructions, LV_ALIGN_TOP_LEFT, 215, 430);
    lv_obj_add_flag(network_instructions, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(network_instructions, on_open_setup, LV_EVENT_CLICKED, nullptr);

    forget_phones_button = lv_button_create(settings_panel);
    lv_obj_set_size(forget_phones_button, 285, 44);
    style_update_button(forget_phones_button);
    lv_obj_align(forget_phones_button, LV_ALIGN_TOP_LEFT, 215, 525);
    forget_phones_text = make_label(forget_phones_button, "Forget paired phones", &lv_font_montserrat_16, kForeground);
    lv_obj_center(forget_phones_text);
    lv_obj_add_event_cb(forget_phones_button, on_forget_phones, LV_EVENT_CLICKED, nullptr);

    wifi_entry_panel = lv_obj_create(settings_panel);
    lv_obj_set_size(wifi_entry_panel, 1024, 600);
    lv_obj_set_style_bg_color(wifi_entry_panel, kBackground, 0);
    lv_obj_set_style_bg_opa(wifi_entry_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifi_entry_panel, 0, 0);
    lv_obj_clear_flag(wifi_entry_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(wifi_entry_panel, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *entry_title = make_label(wifi_entry_panel, "Connect to home Wi-Fi",
                                       &lv_font_montserrat_32, kForeground);
    lv_obj_align(entry_title, LV_ALIGN_TOP_LEFT, 42, 25);
    wifi_entry_network = make_label(wifi_entry_panel, "Network:",
                                    &lv_font_montserrat_20, kAccent);
    lv_obj_align(wifi_entry_network, LV_ALIGN_TOP_LEFT, 48, 82);

    wifi_password = lv_textarea_create(wifi_entry_panel);
    lv_textarea_set_one_line(wifi_password, true);
    lv_textarea_set_password_mode(wifi_password, true);
    lv_textarea_set_placeholder_text(wifi_password, "Wi-Fi password");
    lv_obj_set_size(wifi_password, 535, 58);
    lv_obj_set_style_bg_color(wifi_password, kPanel, 0);
    lv_obj_set_style_text_color(wifi_password, kForeground, 0);
    lv_obj_set_style_border_color(wifi_password, kAccent, 0);
    lv_obj_align(wifi_password, LV_ALIGN_TOP_LEFT, 48, 116);

    lv_obj_t *show_password = lv_button_create(wifi_entry_panel);
    lv_obj_set_size(show_password, 105, 58);
    lv_obj_set_style_bg_color(show_password, kPanel, 0);
    lv_obj_set_style_border_color(show_password, kLine, 0);
    lv_obj_set_style_border_width(show_password, 1, 0);
    lv_obj_align(show_password, LV_ALIGN_TOP_LEFT, 595, 116);
    lv_obj_add_event_cb(show_password, on_wifi_password_visibility, LV_EVENT_CLICKED, nullptr);
    wifi_entry_show_label = make_label(show_password, "Show",
                                       &lv_font_montserrat_16, kForeground);
    lv_obj_center(wifi_entry_show_label);

    wifi_entry_cancel = lv_button_create(wifi_entry_panel);
    lv_obj_set_size(wifi_entry_cancel, 115, 52);
    lv_obj_set_style_bg_color(wifi_entry_cancel, kPanel, 0);
    lv_obj_set_style_border_color(wifi_entry_cancel, kLine, 0);
    lv_obj_set_style_border_width(wifi_entry_cancel, 1, 0);
    lv_obj_align(wifi_entry_cancel, LV_ALIGN_TOP_RIGHT, -170, 115);
    lv_obj_add_event_cb(wifi_entry_cancel, on_wifi_modal_cancel, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *entry_cancel_text = make_label(wifi_entry_cancel, "Cancel",
                                             &lv_font_montserrat_16, kForeground);
    lv_obj_center(entry_cancel_text);

    wifi_entry_connect = lv_button_create(wifi_entry_panel);
    lv_obj_set_size(wifi_entry_connect, 115, 52);
    lv_obj_set_style_bg_color(wifi_entry_connect, kPanel, 0);
    lv_obj_set_style_border_color(wifi_entry_connect, kAccent, 0);
    lv_obj_set_style_border_width(wifi_entry_connect, 1, 0);
    lv_obj_align(wifi_entry_connect, LV_ALIGN_TOP_RIGHT, -42, 115);
    lv_obj_add_event_cb(wifi_entry_connect, on_wifi_connect, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *entry_connect_text = make_label(wifi_entry_connect, "Connect",
                                              &lv_font_montserrat_16, kForeground);
    lv_obj_center(entry_connect_text);

    wifi_entry_status = make_label(wifi_entry_panel, "Enter the network password.",
                                   &lv_font_montserrat_16, kMuted);
    lv_obj_align(wifi_entry_status, LV_ALIGN_TOP_LEFT, 48, 180);

    wifi_entry_spinner = lv_spinner_create(wifi_entry_panel);
    lv_obj_set_size(wifi_entry_spinner, 34, 34);
    lv_spinner_set_anim_params(wifi_entry_spinner, 900, 220);
    lv_obj_set_style_arc_color(wifi_entry_spinner, kAccent, LV_PART_INDICATOR);
    lv_obj_align(wifi_entry_spinner, LV_ALIGN_TOP_LEFT, 6, 173);
    lv_obj_add_flag(wifi_entry_spinner, LV_OBJ_FLAG_HIDDEN);

    wifi_keyboard = lv_keyboard_create(wifi_entry_panel);
    lv_obj_set_size(wifi_keyboard, 1024, 380);
    lv_obj_align(wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(wifi_keyboard, on_wifi_connect, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(wifi_keyboard, on_wifi_modal_cancel, LV_EVENT_CANCEL, nullptr);
    lv_keyboard_set_textarea(wifi_keyboard, wifi_password);
    lv_obj_add_flag(wifi_entry_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(update_clock, 1000, nullptr);
    create_display_preferences(screen);
    create_updates_screen(screen);
    create_setup_guide(screen);
}

void create_display_preferences(lv_obj_t *screen)
{
    auto *open = lv_button_create(settings_panel);
    lv_obj_set_size(open, 235, 38);
    style_update_button(open);
    lv_obj_align(open, LV_ALIGN_TOP_LEFT, 725, 115);
    lv_obj_center(make_label(open, "Clock & night dimming", &lv_font_montserrat_16, kForeground));
    display_panel = lv_obj_create(screen);
    lv_obj_set_size(display_panel, 1024, 600);
    lv_obj_center(display_panel);
    lv_obj_set_style_pad_all(display_panel, 0, 0);
    lv_obj_set_style_bg_color(display_panel, kBackground, 0);
    lv_obj_set_style_bg_opa(display_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(display_panel, 0, 0);
    lv_obj_clear_flag(display_panel, LV_OBJ_FLAG_SCROLLABLE);
    auto *title = make_label(display_panel, "Clock & night dimming", &lv_font_montserrat_32, kForeground);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 48, 35);
    auto *close = lv_button_create(display_panel);
    lv_obj_set_size(close, 120, 52); style_update_button(close);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -42, 26);
    lv_obj_center(make_label(close, "Done", &lv_font_montserrat_16, kForeground));
    lv_obj_add_event_cb(close, [](lv_event_t *) { lv_obj_add_flag(display_panel, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, nullptr);
    night_enabled = lv_checkbox_create(display_panel);
    lv_checkbox_set_text(night_enabled, "Dim touchscreen at night while idle");
    lv_obj_set_style_text_color(night_enabled, kForeground, 0);
    lv_obj_set_style_text_font(night_enabled, display_font(&lv_font_montserrat_20), 0);
    lv_obj_set_style_bg_color(night_enabled, kPanel, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(night_enabled, kAccent, static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(night_enabled, kAccent, LV_PART_INDICATOR);
    lv_obj_align(night_enabled, LV_ALIGN_TOP_LEFT, 70, 115);
    lv_obj_align(make_label(display_panel, "Starts", &lv_font_montserrat_20, kForeground), LV_ALIGN_TOP_LEFT, 70, 180);
    lv_obj_align(make_label(display_panel, "Ends", &lv_font_montserrat_20, kForeground), LV_ALIGN_TOP_LEFT, 360, 180);
    std::string times;
    char value[8];
    for (int minute = 0; minute < 1440; minute += 15) {
        std::snprintf(value, sizeof(value), "%02d:%02d", minute / 60, minute % 60);
        if (!times.empty()) times += '\n';
        times += value;
    }
    night_start = lv_dropdown_create(display_panel);
    night_end = lv_dropdown_create(display_panel);
    for (auto *dropdown : {night_start, night_end}) {
        lv_dropdown_set_options(dropdown, times.c_str());
        lv_obj_set_size(dropdown, 220, 52);
        lv_obj_set_style_bg_color(dropdown, kPanel, 0);
        lv_obj_set_style_text_color(dropdown, kForeground, 0);
        lv_obj_set_style_border_color(dropdown, kLine, 0);
    }
    lv_obj_align(night_start, LV_ALIGN_TOP_LEFT, 70, 215);
    lv_obj_align(night_end, LV_ALIGN_TOP_LEFT, 360, 215);
    night_level_label = make_label(display_panel, "Night brightness: 10%", &lv_font_montserrat_20, kForeground);
    lv_obj_align(night_level_label, LV_ALIGN_TOP_LEFT, 70, 305);
    night_level = lv_slider_create(display_panel);
    lv_obj_set_size(night_level, 510, 40);
    lv_slider_set_range(night_level, 10, 100);
    lv_obj_set_style_bg_color(night_level, kLine, LV_PART_MAIN);
    lv_obj_set_style_bg_color(night_level, kAccent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(night_level, kForeground, LV_PART_KNOB);
    lv_obj_align(night_level, LV_ALIGN_TOP_LEFT, 70, 345);
    lv_obj_add_event_cb(night_level, [](lv_event_t *) {
        lv_label_set_text_fmt(night_level_label, "Night brightness: %d%%", int(lv_slider_get_value(night_level)));
    }, LV_EVENT_VALUE_CHANGED, nullptr);
    auto *help = make_label(display_panel,
        "The idle clock uses device local time (24-hour).\n"
        "Touch or music restores your selected brightness.\n"
        "Automatic dimming waits 60 seconds after interaction.\n"
        "Without valid time, automatic dimming stays off.", &lv_font_montserrat_16, kMuted);
    lv_obj_align(help, LV_ALIGN_TOP_LEFT, 70, 405);
    auto *save = lv_button_create(display_panel);
    lv_obj_set_size(save, 220, 52); style_update_button(save);
    lv_obj_align(save, LV_ALIGN_BOTTOM_LEFT, 70, -25);
    lv_obj_center(make_label(save, "Save display settings", &lv_font_montserrat_16, kForeground));
    night_status = make_label(display_panel, "", &lv_font_montserrat_16, kMuted);
    lv_obj_set_width(night_status, 620);
    lv_obj_align(night_status, LV_ALIGN_BOTTOM_LEFT, 320, -40);
    lv_obj_add_event_cb(save, [](lv_event_t *) {
        display_schedule::Config candidate;
        candidate.enabled = lv_obj_has_state(night_enabled, LV_STATE_CHECKED);
        candidate.start = lv_dropdown_get_selected(night_start) * 15;
        candidate.end = lv_dropdown_get_selected(night_end) * 15;
        candidate.percent = lv_slider_get_value(night_level);
        if (!candidate.valid()) {
            lv_label_set_text(night_status, "Choose different start and end times.");
        } else if (!display_schedule::save(candidate)) {
            lv_label_set_text(night_status, "Could not save. Previous settings remain active.");
        } else {
            night_config = candidate; night_storage_ready = true;
            lv_label_set_text(night_status, "Saved. Applies when idle and the clock is set.");
        }
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(open, [](lv_event_t *) {
        if (night_config.enabled) lv_obj_add_state(night_enabled, LV_STATE_CHECKED);
        else lv_obj_remove_state(night_enabled, LV_STATE_CHECKED);
        lv_dropdown_set_selected(night_start, night_config.start / 15);
        lv_dropdown_set_selected(night_end, night_config.end / 15);
        lv_slider_set_value(night_level, night_config.percent, LV_ANIM_OFF);
        lv_label_set_text_fmt(night_level_label, "Night brightness: %d%%", int(night_config.percent));
        lv_label_set_text(night_status, night_storage_ready ? "" : "Settings could not be read. Dimming is off until saved.");
        lv_obj_remove_flag(display_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(display_panel);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(display_panel, LV_OBJ_FLAG_HIDDEN);
}

[[gnu::noinline]] void update_cover_palette(const RecognitionTrack *track, const RecognitionArtwork *image,
                                            bool same_cover)
{
    if (!same_cover) cover_palette = {};
    if (track != nullptr && image != nullptr && image->pixels != nullptr
        && (!same_cover || cover_palette == album_palette::Palette{}))
        cover_palette = album_palette::extract(image->pixels, image->width, image->height);
    network_set_palette(cover_palette);
}

bool update_recognition(RecognitionStatus status, const RecognitionTrack *track,
                        const RecognitionArtwork *image)
{
    if (microphone_privacy::muted()) { status = RecognitionStatus::Waiting; track = nullptr; image = nullptr; }
    network_set_recognition(status, track);
    // The recognition/session mutex is acquired before this GUI lock everywhere.
    if (!bsp_display_lock(10)) {
        ESP_LOGW(kTag, "Recognition display update deferred: GUI busy");
        return false;
    }
    if (track || recognition_feedback::searching(status)) lv_obj_add_flag(idle_clock, LV_OBJ_FLAG_HIDDEN);
    const bool same_cover = track != nullptr
        && std::strcmp(displayed_track.artwork_url, track->artwork_url) == 0;
    const bool same_track = track != nullptr
        && std::strcmp(displayed_track.title, track->title) == 0
        && std::strcmp(displayed_track.artist, track->artist) == 0
        && std::strcmp(displayed_track.artwork_url, track->artwork_url) == 0;
    if (track != nullptr) {
        set_label_text(headline, track->title);
        set_label_text(artist, track->artist);
        set_label_text(album, track->album);
        displayed_track = *track;
    } else {
        clear_recognition_track();
    }
    if (!same_track || image == nullptr || image->pixels != artwork_source) {
        lv_obj_add_flag(artwork_image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(artwork_mark, LV_OBJ_FLAG_HIDDEN);
        lv_image_cache_drop(&artwork_descriptor);
        heap_caps_free(artwork_pixels);
        artwork_pixels = nullptr; artwork_source = nullptr;
        if (image != nullptr && image->pixels != nullptr && image->width > 0 && image->width <= 1024
            && image->height > 0 && image->height <= 1024) {
            const size_t bytes = image->width * image->height * 2;
            artwork_pixels = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (artwork_pixels != nullptr) {
                std::memcpy(artwork_pixels, image->pixels, bytes);
                artwork_descriptor = {};
                artwork_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
                artwork_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
                artwork_descriptor.header.w = image->width;
                artwork_descriptor.header.h = image->height;
                artwork_descriptor.header.stride = image->width * 2;
                artwork_descriptor.data_size = bytes;
                artwork_descriptor.data = artwork_pixels;
                lv_image_set_src(artwork_image, &artwork_descriptor);
                const unsigned scale = std::min(358U * 256 / image->width, 358U * 256 / image->height);
                lv_image_set_scale(artwork_image, scale);
                lv_obj_center(artwork_image);
                artwork_source = image->pixels;
                lv_obj_remove_flag(artwork_image, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(artwork_mark, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    update_cover_palette(track, image, same_cover);
    apply_palette();
    set_searching(status == RecognitionStatus::Capturing || status == RecognitionStatus::Identifying,
                  track != nullptr);
    set_recognition_feedback(status, track != nullptr);
    bsp_display_unlock();
    return true;
}

} // namespace

extern "C" void app_main()
{
    microphone_privacy_start();
    ESP_ERROR_CHECK(xTaskCreate(update_boot_guard, "ota_boot", 4096, nullptr, 5, nullptr) == pdPASS
        ? ESP_OK : ESP_ERR_NO_MEM);
    log_board_identity();
    night_storage_ready = display_schedule::load(night_config);
    if (!night_storage_ready) {
        night_config.enabled = 0;
        ESP_LOGW(kTag, "Display schedule unavailable; automatic dimming disabled until saved");
    }

    bsp_display_cfg_t config = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };

    lv_display_t *display = bsp_display_start_with_config(&config);
    ESP_ERROR_CHECK(display != nullptr ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    ESP_ERROR_CHECK(bsp_display_lock(-1) ? ESP_OK : ESP_ERR_TIMEOUT);
    create_product_screen();
    {
        // LVGL invokes timers under the display lock, including credential-free boots.
        lv_timer_create([](lv_timer_t *) {
            set_recognition_feedback(theme_status, displayed_track.title[0] != '\0');
        }, 100, nullptr);
    }
    bsp_display_unlock();

    ESP_LOGI(kTag, "Display and touch initialized");
    recognition_start(update_recognition);
    music_detector_start();
    if (p4_led_enabled()) p4_led_start(update_controller_status);
    else usb_controller_start(update_controller_status);
    RemoteCallbacks remote{};
    remote.phone_pairing_forgotten = phone_pairing_forgotten;
    remote.set_appearance = remote_set_appearance;
    remote.set_style = remote_set_style;
    remote.set_brightness = remote_set_brightness;
    remote.set_name = remote_set_name;
    remote.set_time = remote_set_time;
    remote.set_network = remote_set_network;
    remote.set_networks = remote_set_networks;
    remote.set_wifi_configuration = remote_set_wifi_configuration;
    network_start(remote);
}
