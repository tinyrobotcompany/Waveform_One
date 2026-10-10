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
#include "src/misc/cache/instance/lv_image_cache.h"
#include "network.h"
#include "recognition.h"
#include "recognition_policy.h"
#include "usb_controller.h"

namespace {

constexpr char kTag[] = "waveform_p4";
constexpr lv_color_t kBackground = LV_COLOR_MAKE(0x08, 0x0d, 0x0a);
constexpr lv_color_t kPanel = LV_COLOR_MAKE(0x12, 0x1a, 0x13);
constexpr lv_color_t kForeground = LV_COLOR_MAKE(0xed, 0xf3, 0xe8);
constexpr lv_color_t kAccent = LV_COLOR_MAKE(0xa8, 0xd5, 0x7a);
constexpr lv_color_t kMuted = LV_COLOR_MAKE(0x84, 0x91, 0x84);
constexpr lv_color_t kLine = LV_COLOR_MAKE(0x37, 0x48, 0x38);

lv_obj_t *artwork = nullptr;
lv_obj_t *artwork_mark = nullptr;
lv_obj_t *eyebrow = nullptr;
lv_obj_t *headline = nullptr;
lv_obj_t *artist = nullptr;
lv_obj_t *album = nullptr;
lv_obj_t *recognition_status = nullptr;
lv_obj_t *artwork_image = nullptr;
lv_image_dsc_t artwork_descriptor{};
uint8_t *artwork_pixels = nullptr;
const uint8_t *artwork_source = nullptr;
RecognitionTrack displayed_track{};
lv_obj_t *settings_panel = nullptr;
lv_obj_t *brightness_label = nullptr;
lv_obj_t *brightness_slider = nullptr;
lv_obj_t *welcome_label = nullptr;
lv_obj_t *clock_label = nullptr;
lv_obj_t *wifi_indicator = nullptr;
lv_obj_t *wifi_header_button = nullptr;
lv_obj_t *network_qr = nullptr;
lv_obj_t *network_heading = nullptr;
lv_obj_t *network_instructions = nullptr;
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

void clear_recognition_track()
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

lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                     lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
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
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
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
    const StyleRequestResult result = usb_controller_set_style(style);
    lv_label_set_text(style_status, result == StyleRequestResult::Queued
                                        ? "Applying…"
                                        : "LED controller is not available.");
}

bool remote_set_style(LedStyle style)
{
    const bool queued = usb_controller_set_style(style) == StyleRequestResult::Queued;
    if (bsp_display_lock(1000)) {
        lv_label_set_text(style_status, queued ? "Applying…" : "LED controller is not available.");
        bsp_display_unlock();
    }
    return queued;
}

bool remote_set_brightness(int percent)
{
    if (!control_policy::valid_brightness(percent)) return false;
    const esp_err_t result = bsp_display_brightness_set(percent);
    if (result != ESP_OK) {
        ESP_LOGW(kTag, "Remote brightness failed: %s", esp_err_to_name(result));
        return false;
    }
    current_brightness.store(percent);
    if (bsp_display_lock(1000)) {
        lv_label_set_text_fmt(brightness_label, "%d%%", percent);
        lv_slider_set_value(brightness_slider, percent, LV_ANIM_OFF);
        bsp_display_unlock();
    }
    return true;
}

void remote_set_name(const char *name)
{
    if (bsp_display_lock(1000)) {
        lv_label_set_text_fmt(welcome_label, "Welcome, %s", name);
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
    if (displayed_track.title[0] != '\0'
        && recognition_policy::expired(displayed_track.matched_at_us, esp_timer_get_time())) {
        clear_recognition_track();
        lv_label_set_text(recognition_status, "Listening");
    }
    const std::time_t now = std::time(nullptr);
    if (now < 1700000000) return;
    tm local{};
    localtime_r(&now, &local);
    lv_label_set_text_fmt(clock_label, "%02d:%02d", local.tm_hour, local.tm_min);
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

void remote_set_network(NetworkMode mode, const char *address)
{
    recognition_set_network(mode == NetworkMode::HomeWifi, address);
    if (!bsp_display_lock(1000)) return;
    if (mode == NetworkMode::Unconfigured) {
        lv_obj_set_style_text_color(wifi_indicator, kMuted, 0);
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
        lv_obj_set_style_text_color(wifi_indicator, kMuted, 0);
        lv_obj_align(wifi_indicator, LV_ALIGN_TOP_RIGHT, -260, 28);
        lv_obj_add_flag(network_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(network_qr_placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(wifi_header_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(network_heading, "PHONE REMOTE");
        lv_label_set_text(network_instructions,
                          "Reconnecting to home Wi-Fi.\nPhone remotes must scan the\nnew QR once it appears.");
        lv_label_set_text(wifi_status, "Reconnecting to home Wi-Fi…");
    } else {
        lv_obj_set_style_text_color(wifi_indicator, kAccent, 0);
        lv_obj_align(wifi_indicator, LV_ALIGN_TOP_RIGHT, -260, 28);
        lv_qrcode_set_data(network_qr, address);
        lv_obj_remove_flag(network_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(network_qr_placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(wifi_header_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(network_heading, "PHONE REMOTE");
        lv_label_set_text(network_instructions,
                          "Connected to home Wi-Fi.\nScan to open the remote\non this network.");
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

void create_product_screen()
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, kBackground, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *brand = make_label(screen, "WAVEFORM ONE", &lv_font_montserrat_20, kAccent);
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
    lv_obj_t *wifi_header_text = make_label(wifi_header_button, "Connect Wi-Fi",
                                            &lv_font_montserrat_16, kForeground);
    lv_obj_center(wifi_header_text);

    lv_obj_t *settings_button = lv_button_create(screen);
    lv_obj_set_size(settings_button, 120, 46);
    lv_obj_set_style_bg_color(settings_button, kPanel, 0);
    lv_obj_set_style_border_color(settings_button, kLine, 0);
    lv_obj_set_style_border_width(settings_button, 1, 0);
    lv_obj_set_style_radius(settings_button, 9, 0);
    lv_obj_align(settings_button, LV_ALIGN_TOP_RIGHT, -42, 15);
    lv_obj_add_event_cb(settings_button, on_open_settings, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *settings_text = make_label(settings_button, "Settings", &lv_font_montserrat_16,
                                         kForeground);
    lv_obj_center(settings_text);

    lv_obj_t *rule = lv_obj_create(screen);
    lv_obj_set_size(rule, 940, 2);
    lv_obj_set_style_border_width(rule, 0, 0);
    lv_obj_set_style_bg_color(rule, kLine, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_MID, 0, 70);

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

    lv_obj_t *information = lv_obj_create(screen);
    lv_obj_set_size(information, 510, 390);
    lv_obj_set_style_border_width(information, 0, 0);
    lv_obj_set_style_bg_opa(information, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(information, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(information, LV_ALIGN_TOP_RIGHT, -42, 98);

    lv_obj_set_style_pad_all(information, 0, 0);
    lv_obj_set_style_pad_top(information, 14, 0);
    lv_obj_set_style_pad_row(information, 12, 0);
    lv_obj_set_flex_flow(information, LV_FLEX_FLOW_COLUMN);

    eyebrow = make_label(information, "NOW PLAYING", &lv_font_montserrat_16, kAccent);


    headline = make_label(information, "Listening for music", &lv_font_montserrat_32, kForeground);
    lv_obj_set_width(headline, 490);
    lv_label_set_long_mode(headline, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(headline, 128, 0);


    artist = make_label(information, "Play a song to bring this screen to life.",
                        &lv_font_montserrat_24, kForeground);
    lv_obj_set_width(artist, 490);
    lv_label_set_long_mode(artist, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(artist, 60, 0);


    album = make_label(information, "",
                       &lv_font_montserrat_20, kMuted);
    lv_obj_set_width(album, 490);
    lv_label_set_long_mode(album, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(album, 48, 0);


    recognition_status = make_label(information,
        "Listening",
        &lv_font_montserrat_16, kMuted);
    lv_obj_add_flag(recognition_status, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(recognition_status, LV_ALIGN_BOTTOM_LEFT, 2, -28);
    lv_obj_set_width(recognition_status, 490);
    lv_label_set_long_mode(recognition_status, LV_LABEL_LONG_WRAP);
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
    style_status = make_label(settings_panel, "Waiting for LED controller…",
                              &lv_font_montserrat_16, kMuted);
    lv_obj_align(style_status, LV_ALIGN_TOP_LEFT, 48, 355);

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
    lv_obj_add_flag(network_qr, LV_OBJ_FLAG_HIDDEN);
    network_qr_placeholder = lv_obj_create(settings_panel);
    lv_obj_set_size(network_qr_placeholder, 145, 145);
    lv_obj_set_style_bg_color(network_qr_placeholder, kPanel, 0);
    lv_obj_set_style_border_color(network_qr_placeholder, kLine, 0);
    lv_obj_set_style_border_width(network_qr_placeholder, 1, 0);
    lv_obj_set_style_radius(network_qr_placeholder, 8, 0);
    lv_obj_clear_flag(network_qr_placeholder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(network_qr_placeholder, LV_ALIGN_TOP_LEFT, 48, 420);
    lv_obj_t *qr_waiting = make_label(network_qr_placeholder, "APP QR\nAFTER WI-FI",
                                      &lv_font_montserrat_16, kMuted);
    lv_obj_set_style_text_align(qr_waiting, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(qr_waiting);
    network_instructions = make_label(settings_panel, "Starting Wi-Fi…",
                                      &lv_font_montserrat_20, kForeground);
    lv_obj_set_style_text_line_space(network_instructions, 8, 0);
    lv_obj_align(network_instructions, LV_ALIGN_TOP_LEFT, 215, 430);

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
}

void update_recognition(RecognitionStatus status, const RecognitionTrack *track,
                        const RecognitionArtwork *image)
{
    // The recognition/session mutex is acquired before this GUI lock everywhere.
    if (!bsp_display_lock(1000)) return;
    if (track != nullptr && recognition_policy::expired(track->matched_at_us, esp_timer_get_time())) {
        track = nullptr; image = nullptr;
        if (status == RecognitionStatus::Matched) status = RecognitionStatus::Waiting;
    }
    const bool same_track = track != nullptr
        && std::strcmp(displayed_track.title, track->title) == 0
        && std::strcmp(displayed_track.artist, track->artist) == 0
        && std::strcmp(displayed_track.artwork_url, track->artwork_url) == 0;
    if (track != nullptr) {
        lv_label_set_text(headline, track->title);
        lv_label_set_text(artist, track->artist);
        lv_label_set_text(album, track->album);
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
    const char *text = "Listening";
    switch (status) {
    case RecognitionStatus::Waiting: break;
    case RecognitionStatus::Capturing: text = "Listening"; break;
    case RecognitionStatus::Identifying: text = "Finding your song..."; break;
    case RecognitionStatus::Matched: text = ""; break;
    case RecognitionStatus::NoMatch: text = "Listening for your next song"; break;
    case RecognitionStatus::Unavailable: text = "Trying again shortly"; break;
    }
    lv_label_set_text(recognition_status, text);
    bsp_display_unlock();
}

} // namespace

extern "C" void app_main()
{
    log_board_identity();

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
    bsp_display_unlock();

    ESP_LOGI(kTag, "Display and touch initialized");
    recognition_start(update_recognition);
    usb_controller_start(update_controller_status, recognition_set_activity);
    RemoteCallbacks remote{};
    remote.set_style = remote_set_style;
    remote.set_brightness = remote_set_brightness;
    remote.set_name = remote_set_name;
    remote.set_time = remote_set_time;
    remote.set_network = remote_set_network;
    remote.set_networks = remote_set_networks;
    remote.set_wifi_configuration = remote_set_wifi_configuration;
    network_start(remote);
}
