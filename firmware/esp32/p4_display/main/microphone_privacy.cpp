#include "microphone_privacy.h"
#include "sdkconfig.h"
#if CONFIG_WAVEFORM_PHYSICAL_MUTE
#include "driver/gpio.h"
#include "esp_log.h"
namespace {
constexpr gpio_num_t sense = GPIO_NUM_36; // 7B P3 pin 1; separate switch contact only.
bool read_switch() { return gpio_get_level(sense) != 0; }
void changed(void *) { microphone_privacy::set_muted(read_switch()); }
}
#endif
void microphone_privacy_start() {
#if CONFIG_WAVEFORM_PHYSICAL_MUTE
    microphone_privacy::installed.store(true);
    microphone_privacy::set_muted(true); // Open/disconnected sense defaults to muted.
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << sense;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.intr_type = GPIO_INTR_ANYEDGE;
    if (gpio_config(&config) != ESP_OK) { ESP_LOGE("microphone_privacy", "Mute input failed; capture remains disabled"); return; }
    auto result = gpio_install_isr_service(0);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return;
    if (gpio_isr_handler_add(sense, changed, nullptr) != ESP_OK) return;
    microphone_privacy::read_muted = read_switch;
    changed(nullptr);
#endif
}
