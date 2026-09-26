#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "audio_pipeline.h"
#include "panel.h"
#include "visualizer_config.h"
#include "driver/i2s_std.h"
#include "esp_dsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void control_start();

static const char* TAG = "waveform_fft";
static constexpr gpio_num_t PIN_BCLK = GPIO_NUM_5;
static constexpr gpio_num_t PIN_WS = GPIO_NUM_6;
static constexpr gpio_num_t PIN_DIN = GPIO_NUM_7;
static i2s_chan_handle_t rx_handle = nullptr;
static int32_t i2s_words[audio::kFftSize * 2];
alignas(16) static float fft_data[audio::kFftSize * 2];
static audio::Spectrum spectrum;
static audio::Pipeline pipeline(visualizerConfig());
static portMUX_TYPE overflow_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t overflow_count = 0;

static bool IRAM_ATTR on_receive_overflow(i2s_chan_handle_t, i2s_event_data_t*, void*)
{
    portENTER_CRITICAL_ISR(&overflow_lock);
    ++overflow_count;
    portEXIT_CRITICAL_ISR(&overflow_lock);
    return false;
}

static uint32_t receive_overflows()
{
    portENTER_CRITICAL(&overflow_lock);
    const uint32_t count = overflow_count;
    portEXIT_CRITICAL(&overflow_lock);
    return count;
}

static void initialise_i2s()
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_AUTO,
            I2S_ROLE_MASTER
        );

    ESP_ERROR_CHECK(
        i2s_new_channel(
            &chan_cfg,
            nullptr,
            &rx_handle
        )
    );

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            I2S_STD_CLK_DEFAULT_CONFIG(
                audio::kSampleRate
            ),

        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_32BIT,
                I2S_SLOT_MODE_STEREO
            ),

        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_BCLK,
            .ws   = PIN_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = PIN_DIN,

            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    std_cfg.slot_cfg.slot_mask =
        I2S_STD_SLOT_BOTH;

    std_cfg.slot_cfg.slot_bit_width =
        I2S_SLOT_BIT_WIDTH_32BIT;

    std_cfg.slot_cfg.ws_width = 32;

    std_cfg.slot_cfg.ws_pol = false;
    std_cfg.slot_cfg.bit_shift = true;

    ESP_ERROR_CHECK(
        i2s_channel_init_std_mode(
            rx_handle,
            &std_cfg
        )
    );

    i2s_event_callbacks_t callbacks = {};
    callbacks.on_recv_q_ovf = on_receive_overflow;
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_handle, &callbacks, nullptr));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

static bool read_audio_frame()
{
    size_t bytes_read = 0;
    // ESP-IDF takes milliseconds here, not FreeRTOS ticks.
    const esp_err_t err = i2s_channel_read(
        rx_handle, i2s_words, sizeof(i2s_words), &bytes_read, 1000);
    if (err != ESP_OK || bytes_read != sizeof(i2s_words)) {
        ESP_LOGW(TAG, "I2S frame rejected: %s, %zu/%zu bytes",
                 esp_err_to_name(err), bytes_read, sizeof(i2s_words));
        return false;
    }
    return true;
}

static bool calculate_fft()
{
    esp_err_t err = dsps_fft2r_fc32(fft_data, audio::kFftSize);
    if (err == ESP_OK) err = dsps_bit_rev_fc32(fft_data, audio::kFftSize);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FFT failed: %s", esp_err_to_name(err));
        return false;
    }
    // Ordinary complex FFT of real input; no packed-real conversion needed.
    return true;
}

static void verify_fft_pipeline()
{
    // Verify the actual target ESP-DSP implementation before trusting live
    // measurements. Host tests use an independent reference transform.
    constexpr float amplitude = 0.02f;
    for (int i = 0; i < audio::kFftSize; ++i) {
        const float sample = amplitude * std::sin(2.0f * 3.14159265358979323846f
            * 43.0f * i / audio::kFftSize);
        i2s_words[2 * i] = int32_t(sample * 8388608.0f) * 256;
        i2s_words[2 * i + 1] = 0;
    }
    const float rms = spectrum.prepare(i2s_words, fft_data);
    ESP_ERROR_CHECK(calculate_fft() ? ESP_OK : ESP_FAIL);
    float power = 0.0f;
    for (float band : spectrum.power(fft_data)) power += band;
    const float expected = amplitude * amplitude / 2.0f;
    ESP_ERROR_CHECK(std::isfinite(power) && std::abs(power / expected - 1.0f) < 0.01f
        && std::abs(rms * rms / expected - 1.0f) < 0.01f ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "FFT self-test passed: tone RMS=%.6f, spectral RMS=%.6f", rms, std::sqrt(power));
}

static void print_visualizer(const audio::Frame& frame, bool beat_since_print)
{
    flockfile(stdout); // Keep command acknowledgements outside diagnostic lines.
    int visibleBands = 0;
    int peakPixels = 0;
    float cleanPower = 0;
    for (int b = 0; b < audio::kBandCount; ++b) {
        const int height = int(std::clamp(frame.levels[b], 0.0f, 1.0f) * 32 + 0.5f);
        visibleBands += height > 0;
        peakPixels = std::max(peakPixels, height);
        cleanPower += frame.bandRms[b] * frame.bandRms[b];
    }
    const char* displayState = !frame.calibrated ? "CALIBRATING"
        : visibleBands > 0 ? "BARS"
        : !frame.active ? "GATE_CLOSED"
        : cleanPower == 0 ? "NO_CLEAN_BANDS" : "BELOW_DISPLAY";
    printf("%s %s RMS=%.6f dB=%.1f FLUX=%.5f THR=%.5f OPEN=%.6f CLOSE=%.6f "
           "CLEAN=%.6f GATE_RMS=%.6f DISPLAY=%s BANDS=%d PEAK_PX=%d |",
           beat_since_print ? "BEAT" : "    ",
           !frame.calibrated ? "CAL " : frame.active ? "OPEN" : "SHUT",
           frame.rms, 20.0f * std::log10(std::max(frame.rms, 1e-9f)),
           frame.flux, frame.fluxThreshold, frame.openRms, frame.closeRms,
           std::sqrt(cleanPower), frame.gateRms, displayState, visibleBands, peakPixels);
    for (float level : frame.levels) {
        printf("%d", std::clamp(int(level * 9.0f), 0, 9));
    }
    printf("|\n");
    funlockfile(stdout);
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Waveform One FFT pipeline v2: independent detection / fixed dB display");
    const auto config = visualizerConfig();
    ESP_LOGI(TAG, "Room-listening profile: CLEAN spectrum gate %.2fx/%.2fx noise; hold %.2fs; display %.0f to %.0f dBFS",
             config.openNoiseRatio, config.closeNoiseRatio, config.closeHoldSeconds,
             config.displayFloorDb, config.displayCeilingDb);
    ESP_LOGI(TAG, "INMP441 GPIO5/6/7; %d Hz / %d FFT / %d bands",
             audio::kSampleRate, audio::kFftSize, audio::kBandCount);
    for (int b = 0; b < audio::kBandCount; ++b) {
        ESP_LOGI(TAG, "Band %02d: %.1f–%.1f Hz (%d bins)", b + 1,
                 spectrum.lowerEdge(b), spectrum.upperEdge(b), spectrum.binCount(b));
        ESP_ERROR_CHECK(spectrum.binCount(b) > 0 ? ESP_OK : ESP_ERR_INVALID_STATE);
    }
    ESP_ERROR_CHECK(dsps_fft2r_init_fc32(nullptr, CONFIG_DSP_MAX_FFT_SIZE));
    verify_fft_pipeline();
    panel_start();
    control_start();
    initialise_i2s();
    for (int i = 0; i < 5; ++i) read_audio_frame();

    ESP_LOGW(TAG, "NOISE CALIBRATION: keep room quiet / music paused for 3 seconds.");
    ESP_LOGW(TAG, "If music plays during calibration, pause it and reset the board.");
    uint32_t frame_number = 0;
    bool beat_since_print = false;
    uint32_t last_overflows = receive_overflows();
    while (true) {
        if (!read_audio_frame()) {
            pipeline.discontinuity();
            beat_since_print = false;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        const uint32_t overflows = receive_overflows();
        if (overflows != last_overflows) {
            ESP_LOGW(TAG, "I2S dropped %lu buffers; discarding frame and re-priming detection",
                     static_cast<unsigned long>(overflows - last_overflows));
            last_overflows = overflows;
            pipeline.discontinuity();
            beat_since_print = false;
            continue;
        }
        const float rms = spectrum.prepare(i2s_words, fft_data);
        if (!calculate_fft()) {
            pipeline.discontinuity();
            beat_since_print = false;
            continue;
        }
        const bool was_calibrated = pipeline.calibrated();
        const audio::Frame frame = pipeline.process(rms, spectrum.power(fft_data));
        panel_publish(frame);
        if (!was_calibrated && frame.calibrated) {
            ESP_LOGI(TAG, "Calibration complete: noise RMS=%.6f; open=%.6f; close=%.6f",
                     pipeline.noiseRms(), frame.openRms, frame.closeRms);
        }
        beat_since_print |= frame.beat;
        // ~2 updates/s; full-rate audio and display continue independently.
        if ((frame_number++ % 12) == 0) {
            print_visualizer(frame, beat_since_print);
            beat_since_print = false;
        }
        // Blocking I2S reads yield to FreeRTOS and pace the analysis. Do not
        // insert an extra delay into the audio-consumer path.
    }
}
