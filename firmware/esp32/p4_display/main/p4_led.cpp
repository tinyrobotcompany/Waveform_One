#include "p4_led.h"
#include "sdkconfig.h"
#if CONFIG_WAVEFORM_P4_LED
#include "p4_hub75.h"
#include "p4_led_spectrum.h"
#include "recognition_input.h"
#include "styles.h"
#include "driver/parlio_tx.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <new>

#if CONFIG_WAVEFORM_P4_LED_PATTERN
constexpr bool diagnostic_pattern = true;
#else
constexpr bool diagnostic_pattern = false;
#endif
namespace {
constexpr char tag[] = "p4_led";
constexpr gpio_num_t oe = GPIO_NUM_51, lat = GPIO_NUM_50, clk = GPIO_NUM_52;
std::atomic<LedStyle> requested{LedStyle::Mirrored};
std::atomic_bool available{false};
std::atomic<unsigned> style_request{0};
LedControllerStatusCallback report;
TaskHandle_t worker;
parlio_tx_unit_handle_t unit;
p4_hub75::Frame *frames[2]{};

bool IRAM_ATTR switched(parlio_tx_unit_handle_t, const parlio_tx_buffer_switched_event_data_t *event, void *) {
    const uint32_t index = event->new_buffer_addr == frames[0] ? 1 :
        event->new_buffer_addr == frames[1] ? 2 : 3;
    BaseType_t awake = pdFALSE;
    xTaskNotifyFromISR(worker, index, eSetValueWithOverwrite, &awake);
    return awake == pdTRUE;
}
void blank_panel() {
    if (unit) parlio_tx_unit_disable(unit);
    // Detach peripheral routing before forcing OE high on a failed driver.
    gpio_reset_pin(oe);
    gpio_set_level(oe, 1);
    gpio_set_direction(oe, GPIO_MODE_OUTPUT);
    gpio_set_level(oe, 1);
}
void fail(const char *stage, esp_err_t error) {
    available.store(false); blank_panel();
    ESP_LOGE(tag, "%s failed: %s; panel blanked", stage, esp_err_to_name(error));
    if (report) report(LedControllerState::ProtocolError);
    // Keep allocated DMA buffers alive until reboot even on a failed handoff.
    vTaskDelete(nullptr);
}
void initialise_panel() {
    gpio_set_level(oe, 1);
    gpio_config_t config{};
    for (int pin : p4_hub75::pins) config.pin_bit_mask |= 1ULL << pin;
    config.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(oe, 1); gpio_set_level(lat, 0); gpio_set_level(clk, 0);
    // Same FM6124/FM6126A sequence as the working S3 panel diagnostic.
    constexpr uint16_t registers[]{0x07e0, 0x0040};
    for (auto value : registers) {
        for (unsigned x = 0; x < 64; ++x) {
            const bool bit = value & (1U << (15 - x % 16));
            for (unsigned c = 0; c < 6; ++c) gpio_set_level(gpio_num_t(p4_hub75::pins[c]), bit);
            gpio_set_level(lat, x > 52);
            esp_rom_delay_us(1); gpio_set_level(clk, 1);
            esp_rom_delay_us(1); gpio_set_level(clk, 0);
        }
        gpio_set_level(lat, 0);
    }
    // OE stays high until the complete, blank-boundary DMA frame starts.
}
void run(void *) {
    worker = xTaskGetCurrentTaskHandle();
    initialise_panel();
    for (auto &frame : frames) {
        void *memory = heap_caps_aligned_alloc(64, sizeof(p4_hub75::Frame), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!memory) { fail("frame allocation", ESP_ERR_NO_MEM); return; }
        frame = new (memory) p4_hub75::Frame{};
    }
    parlio_tx_unit_config_t config{};
    config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    config.clk_in_gpio_num = GPIO_NUM_NC; config.valid_gpio_num = GPIO_NUM_NC;
    config.clk_out_gpio_num = clk; config.output_clk_freq_hz = 1000000;
    config.data_width = 16; config.trans_queue_depth = 2;
    config.max_transfer_size = sizeof(p4_hub75::Frame);
    config.dma_burst_size = 64; config.shift_edge = PARLIO_SHIFT_EDGE_NEG;
    for (auto &pin : config.data_gpio_nums) pin = GPIO_NUM_NC;
    for (unsigned c = 0; c < 6; ++c) config.data_gpio_nums[c] = gpio_num_t(p4_hub75::pins[c]);
    config.data_gpio_nums[6] = lat; config.data_gpio_nums[7] = oe;
    for (unsigned c = 0; c < 4; ++c) config.data_gpio_nums[8+c] = gpio_num_t(p4_hub75::pins[6+c]);
    esp_err_t error = parlio_new_tx_unit(&config, &unit);
    if (error != ESP_OK) { fail("PARLIO allocation", error); return; }
    parlio_tx_event_callbacks_t callbacks{}; callbacks.on_buffer_switched = switched;
    error = parlio_tx_unit_register_event_callbacks(unit, &callbacks, nullptr);
    if (error != ESP_OK) { fail("callback registration", error); return; }
    error = parlio_tx_unit_enable(unit);
    if (error != ESP_OK) { fail("PARLIO enable", error); return; }
    struct Work { p4_led::Spectrum spectrum; visual::Renderer renderer; int16_t samples[512]; };
    void *memory = heap_caps_aligned_alloc(16, sizeof(Work), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) { fail("analysis allocation", ESP_ERR_NO_MEM); return; }
    auto *work = new (memory) Work{};
    p4_hub75::Buffers ownership;
    uint64_t position = 0; uint32_t segment = UINT32_MAX;
    size_t filled = 0;
    int64_t last_audio = 0, last_log = 0;
    unsigned updates = 0;
    unsigned applied_request = 0;
    bool first = true;
    parlio_transmit_config_t tx{};
    tx.idle_value = p4_hub75::blank;
    tx.flags.loop_transmission = true; tx.flags.queue_nonblocking = true;
    ESP_LOGI(tag, "PARLIO 64x32 clock=1000000 frame_bytes=%u mode=%s",
        unsigned(sizeof(p4_hub75::Frame)), diagnostic_pattern ? "diagnostic" : "onboard_audio");
    while (true) {
        const auto request = style_request.load();
        const auto style = requested.load();
        work->renderer.setMode(style == LedStyle::Classic ? visual::Mode::Classic :
            style == LedStyle::Mirrored ? visual::Mode::Mirrored : visual::Mode::Waterfall);
        const auto now = esp_timer_get_time();
        if (!diagnostic_pattern) {
            const auto *audio = recognition_input_audio();
            if (audio) {
                const auto end = audio->written();
                if (end > position + 1024) { position = end - 512; filled = 0; }
                const auto read = audio->read(position, work->samples + filled, 512 - filled);
                if (read.skipped || read.segment != segment) {
                    // Re-read a complete frame after a stream break, never splice.
                    filled = 0; position = read.position; segment = read.segment;
                    work->renderer.render({}, now / 1000);
                } else if (read.count) {
                    filled += read.count; position = read.position + read.count; last_audio = now;
                    if (filled == 512) {
                        work->renderer.render(work->spectrum.process(work->samples), now / 1000);
                        filled = 0;
                    }
                }
            }
            if (now - last_audio > 500000) { filled = 0; work->renderer.render({}, now / 1000); }
        }
        const int index = ownership.writable();
        if (index < 0) { fail("buffer ownership", ESP_ERR_INVALID_STATE); return; }
        p4_hub75::encode(*frames[index], [&](unsigned x, unsigned row) {
            return diagnostic_pattern ? p4_hub75::diagnostic(x, row) : work->renderer.rowPair(x, row);
        });
        if (!ownership.submit(index)) { fail("buffer submit", ESP_ERR_INVALID_STATE); return; }
        error = parlio_tx_unit_transmit(unit, frames[index]->data(), sizeof(p4_hub75::Frame)*8, &tx);
        if (error != ESP_OK) { fail("transmit", error); return; }
        uint32_t notification = 0;
        if (xTaskNotifyWait(0, UINT32_MAX, &notification, pdMS_TO_TICKS(500)) != pdTRUE ||
            !ownership.switched(int(notification)-1)) { fail("DMA handoff", ESP_ERR_TIMEOUT); return; }
        available.store(true);
        if (first || request != applied_request) {
            first = false; applied_request = request;
            if (report) report(style == LedStyle::Classic ? LedControllerState::ConnectedClassic :
                style == LedStyle::Mirrored ? LedControllerState::ConnectedMirrored : LedControllerState::ConnectedWaterfall);
        }
        ++updates;
        if (now - last_log >= 5000000) {
            ESP_LOGI(tag, "updates=%u dma_largest=%u", updates,
                unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)));
            updates = 0; last_log = now;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
}
#endif
bool p4_led_enabled() {
#if CONFIG_WAVEFORM_P4_LED
    return true;
#else
    return false;
#endif
}
void p4_led_start(LedControllerStatusCallback callback) {
#if CONFIG_WAVEFORM_P4_LED
    report = callback;
    if (xTaskCreatePinnedToCore(run, "p4_led", 6144, nullptr, 3, nullptr, 1) != pdPASS) {
        ESP_LOGE("p4_led", "Task allocation failed; LED driver not started");
        if (report) report(LedControllerState::ProtocolError);
    }
#else
    (void)callback;
#endif
}
StyleRequestResult p4_led_set_style(LedStyle style) {
#if CONFIG_WAVEFORM_P4_LED
    if (!available.load()) return StyleRequestResult::Unavailable;
    requested.store(style); ++style_request; return StyleRequestResult::Queued;
#else
    (void)style; return StyleRequestResult::Unavailable;
#endif
}
