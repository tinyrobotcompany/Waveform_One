#include "recognition_input.h"
#include "sdkconfig.h"
#include "esp_log.h"

#if CONFIG_WAVEFORM_P4_MIC
#include "microphone_pcm.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <new>

namespace {
constexpr char kTag[] = "waveform_microphone";
RecognitionAudio *local_audio = nullptr; // allocated before readers start; lives until reboot
AudioReadyCallback notify = nullptr;

void capture(void *) {
    auto codec = bsp_audio_codec_microphone_init();
    esp_codec_dev_sample_info_t format{};
    format.sample_rate = audio_stream::kSampleRate;
    format.bits_per_sample = 16;
    format.channel = 2;
    if (!codec) {
        ESP_LOGE(kTag, "Onboard ADC initialization failed; no USB fallback");
        notify(false); vTaskDelete(nullptr); return;
    }
    constexpr size_t frames = 256; // 16 ms at 16 kHz
    int16_t stereo[frames * 2], mono[frames];
    while (true) {
        const int opened = esp_codec_dev_open(codec, &format);
        const int gain = opened == ESP_CODEC_DEV_OK
            ? esp_codec_dev_set_in_gain(codec, CONFIG_WAVEFORM_P4_MIC_GAIN_DB) : opened;
        if (opened != ESP_CODEC_DEV_OK || gain != ESP_CODEC_DEV_OK) {
            notify(false);
            ESP_LOGE(kTag, "ADC open/gain failed open=%d gain=%d", opened, gain);
            esp_codec_dev_close(codec);
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        ESP_LOGI(kTag, "source=p4_es7210 rate=16000 bits=16 channels=2 selected=%d gain_db=%d",
            CONFIG_WAVEFORM_P4_MIC_CHANNEL, CONFIG_WAVEFORM_P4_MIC_GAIN_DB);
        bool ready = false;
        uint64_t samples = 0;
        microphone_pcm::Levels levels[2];
        int64_t report = esp_timer_get_time();
        while (true) {
            // The installed codec adapter bounds reads to one second. Failed
            // reads (including partial timeouts) never enter the sample ring.
            const int result = esp_codec_dev_read(codec, stereo, sizeof(stereo));
            if (result != ESP_CODEC_DEV_OK) {
                local_audio->discontinuity(); notify(false);
                ESP_LOGE(kTag, "ADC read failed=%d; stream invalidated", result);
                break;
            }
            microphone_pcm::select(stereo, frames, CONFIG_WAVEFORM_P4_MIC_CHANNEL, mono);
            for (size_t i = 0; i < frames; ++i) {
                levels[0].add(stereo[2 * i]);
                levels[1].add(stereo[2 * i + 1]);
            }
            local_audio->write(mono, frames);
            if (!ready) { notify(true); ready = true; }
            samples += frames;
            const int64_t now = esp_timer_get_time();
            if (now - report >= 5000000) {
                ESP_LOGI(kTag, "Capture samples=%llu elapsed_us=%lld",
                    static_cast<unsigned long long>(samples), static_cast<long long>(now - report));
                for (unsigned ch = 0; ch < 2; ++ch) {
                    ESP_LOGI(kTag, "Input channel=%u ac_dbfs=%.1f dc=%.2f clipped_pct=%.4f interval_ms=%lld",
                        ch, levels[ch].dbfs(), levels[ch].dc(), levels[ch].clipped_percent(),
                        static_cast<long long>((now - report) / 1000));
                    levels[ch] = {};
                }
                report = now; samples = 0;
            }
        }
        esp_codec_dev_close(codec);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}
#endif

bool recognition_input_is_local() {
#if CONFIG_WAVEFORM_P4_MIC
    return true;
#else
    return false;
#endif
}

void recognition_input_start(AudioReadyCallback ready) {
#if CONFIG_WAVEFORM_P4_MIC
    void *memory = heap_caps_malloc(sizeof(RecognitionAudio), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(memory ? ESP_OK : ESP_ERR_NO_MEM);
    local_audio = new (memory) RecognitionAudio;
    notify = ready;
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(capture, "p4_microphone", 8192, nullptr, 6,
        nullptr, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
#else
    (void)ready;
    ESP_LOGI("waveform_microphone", "source=s3_usb");
#endif
}

const RecognitionAudio *recognition_input_audio() {
#if CONFIG_WAVEFORM_P4_MIC
    return local_audio;
#else
    return usb_controller_audio();
#endif
}
