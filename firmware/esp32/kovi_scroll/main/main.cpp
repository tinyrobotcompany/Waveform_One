#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "message.h"

namespace {
constexpr const char* kTag = "waveform_led";
// Agreed breadboard wiring, NOT the Waveshare example's GPIO assignment.
constexpr gpio_num_t kRgb[] = {
    GPIO_NUM_4, GPIO_NUM_8, GPIO_NUM_9,
    GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_12
};
constexpr gpio_num_t kAddress[] = {
    GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_16
};
constexpr gpio_num_t kClock = GPIO_NUM_17;
constexpr gpio_num_t kLatch = GPIO_NUM_18;
constexpr gpio_num_t kOutputEnable = GPIO_NUM_21;

// Retain the row timing of the working panel diagnostic.
constexpr uint32_t kOnUs = 500;
portMUX_TYPE outputLock = portMUX_INITIALIZER_UNLOCKED;

void initialise()
{
    // Blank the panel before enabling the other GPIO outputs.
    ESP_ERROR_CHECK(gpio_set_level(kOutputEnable, 1));
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << kOutputEnable;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_set_level(kOutputEnable, 1));

    uint64_t mask = (1ULL << kClock) | (1ULL << kLatch);
    for (auto pin : kRgb) mask |= 1ULL << pin;
    for (auto pin : kAddress) mask |= 1ULL << pin;
    for (int pin = 0; pin < 22; ++pin) {
        if (mask & (1ULL << pin))
            ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pin), 0));
    }
    config.pin_bit_mask = mask;
    ESP_ERROR_CHECK(gpio_config(&config));
}

// Retain the startup sequence used by the working diagnostic.
void initialisePanelDriver()
{
    constexpr uint8_t kBrightnessRegister[16] = {
        0, 0, 0, 0, 0, 1, 1, 1,
        1, 1, 1, 0, 0, 0, 0, 0,
    };
    constexpr uint8_t kOutputEnableRegister[16] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
    };

    gpio_set_level(kOutputEnable, 1);
    auto shiftRegister = [](const uint8_t (&bits)[16]) {
        for (int clock = 0; clock < panel::kWidth; ++clock) {
            const int bit = clock % 16;
            for (auto pin : kRgb)
                gpio_set_level(pin, bits[bit]);
            if (clock > panel::kWidth - 12)
                gpio_set_level(kLatch, 1);
            gpio_set_level(kClock, 1);
            gpio_set_level(kClock, 0);
        }
        gpio_set_level(kLatch, 0);
    };

    shiftRegister(kBrightnessRegister);
    shiftRegister(kOutputEnableRegister);

    for (auto pin : kRgb)
        gpio_set_level(pin, 0);
    for (int clock = 0; clock < panel::kWidth; ++clock) {
        gpio_set_level(kClock, 1);
        gpio_set_level(kClock, 0);
    }
    gpio_set_level(kLatch, 1);
    gpio_set_level(kClock, 1);
    gpio_set_level(kClock, 0);
    gpio_set_level(kLatch, 0);
    gpio_set_level(kOutputEnable, 0);
    ESP_LOGI(kTag, "Initialized FM6124/FM6126A-compatible panel driver");
}

void scan(int offset)
{
    for (int row = 0; row < panel::kScanRows; ++row) {
        gpio_set_level(kOutputEnable, 1);
        for (int x = 0; x < panel::kWidth; ++x) {
            const uint8_t bits = panel::rowPair(offset, x, row);
            for (int channel = 0; channel < 6; ++channel)
                gpio_set_level(kRgb[channel], (bits >> channel) & 1);
            // Keep the clock timing that worked with the direct rainbow cable.
            esp_rom_delay_us(1);
            gpio_set_level(kClock, 1);
            esp_rom_delay_us(1);
            gpio_set_level(kClock, 0);
        }
        for (int bit = 0; bit < 4; ++bit)
            gpio_set_level(kAddress[bit], (row >> bit) & 1);
        esp_rom_delay_us(1);
        gpio_set_level(kLatch, 1);
        esp_rom_delay_us(1);
        gpio_set_level(kLatch, 0);
        esp_rom_delay_us(1);

        // Prevent ordinary task/interrupt preemption from extending a lit
        // row. Everything slow (logging, scheduler waits) runs while blank.
        portENTER_CRITICAL(&outputLock);
        gpio_set_level(kOutputEnable, 0);
        esp_rom_delay_us(kOnUs);
        gpio_set_level(kOutputEnable, 1);
        portEXIT_CRITICAL(&outputLock);
    }
}
} // namespace

extern "C" void app_main()
{
    initialise();
    initialisePanelDriver();
    ESP_LOGI(kTag, "Scrolling: %s", panel::kMessage);
    const int64_t started = esp_timer_get_time();
    while (true) {
        const auto steps = (esp_timer_get_time() - started) / 40000;
        const int offset = panel::kWidth - static_cast<int>(
            steps % (panel::kWidth + panel::kTextWidth + 16));
        scan(offset);
        vTaskDelay(1);
    }
}
