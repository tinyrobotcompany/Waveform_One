#include "music_detector.h"
#include "music_features.h"
#include "music_windows.h"
#include "capture_samples.h"
#include <atomic>
#include <new>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
constexpr const char *kTag = "waveform_music";
constexpr size_t kModelBytes = 4046312, kArenaBytes = 320 * 1024;
constexpr uint8_t kModelSha[32] = {0x41,0x17,0xec,0x5b,0x22,0xde,0x5e,0x61,0x87,0xc9,0xc2,0x8f,0x6a,0xa8,0x37,0x83,0xde,0xbb,0x53,0x07,0x6f,0x94,0xfd,0x69,0x3f,0x4e,0x6f,0x3d,0x73,0xf6,0x4c,0x70};
music_detection::Windows windows;
uint8_t *pcm_buffers[2]{};
std::atomic_bool running{false};
std::atomic_uint epoch{1};
capture::Samples decimator;
uint8_t *arena = nullptr;
music_detection::FeatureWorkspace *workspace = nullptr;
const void *model_memory = nullptr;
esp_partition_mmap_handle_t model_mapping = 0;
portMUX_TYPE result_lock = portMUX_INITIALIZER_UNLOCKED;
music_detection::State result = music_detection::State::Unknown;
int64_t result_at = 0;
unsigned result_epoch = 0;
int64_t quiet_since = 0; // Owned by the audio task.
bool quiet_confirmed = false;

void yield_frontend() { vTaskDelay(1); }
void cleanup();
void publish(music_detection::State value, unsigned generation) {
    portENTER_CRITICAL(&result_lock);
    result = value; result_at = esp_timer_get_time(); result_epoch = generation;
    portEXIT_CRITICAL(&result_lock);
}
void detector_task(void *) {
    tflite::MicroMutableOpResolver<5> resolver;
    const bool registered = resolver.AddConv2D() == kTfLiteOk && resolver.AddDepthwiseConv2D() == kTfLiteOk
        && resolver.AddMean() == kTfLiteOk && resolver.AddFullyConnected() == kTfLiteOk && resolver.AddLogistic() == kTfLiteOk;
    tflite::MicroInterpreter interpreter(tflite::GetModel(model_memory), resolver, arena, kArenaBytes);
    if (!registered || interpreter.AllocateTensors() != kTfLiteOk ||
        interpreter.input(0)->type != kTfLiteInt8 || interpreter.input(0)->bytes != 6144 ||
        interpreter.output(0)->type != kTfLiteInt8 || interpreter.output(0)->bytes != 521) {
        ESP_LOGE(kTag, "Classifier initialization failed; music remains unknown");
        cleanup();
        vTaskDelete(nullptr); return;
    }
    ESP_LOGI(kTag, "Native classifier ready; arena=%u, internal free=%u, external free=%u",
        unsigned(interpreter.arena_used_bytes()), unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    running.store(true);
    music_detection::Policy policy;
    unsigned policy_epoch = epoch.load();
    while (true) {
        auto *pending = windows.acquire();
        if (pending) {
            auto &slot = *pending;
            const unsigned generation = slot.epoch;
            if (generation != policy_epoch) { policy.reset(); policy_epoch = generation; }
            const int64_t start = esp_timer_get_time();
            if (generation == epoch.load() && start - slot.captured_at <= 2000000 &&
                music_detection::features(slot.pcm, music_detection::kSamples * 2, *workspace,
                    interpreter.input(0)->data.int8, 6144, yield_frontend,
                    music_detection::input_gain(slot.pcm, music_detection::kSamples * 2)) && interpreter.Invoke() == kTfLiteOk) {
                if (generation == epoch.load() && esp_timer_get_time() - slot.captured_at <= 3000000) {
                    const float score = music_detection::music_score(interpreter.output(0)->data.int8);
                    const auto state = policy.update(score, esp_timer_get_time());
                    publish(state, generation);
                    ESP_LOGI(kTag, "score=%.3f state=%s processing_ms=%lld age_ms=%lld",
                        score, music_detection::name(state), (esp_timer_get_time() - start) / 1000,
                        (esp_timer_get_time() - slot.captured_at) / 1000);
                }
            } else { policy.reset(); publish(music_detection::State::Unknown, generation); }
            windows.release(slot);
        }
        if (!pending) vTaskDelay(pdMS_TO_TICKS(5));
        else vTaskDelay(1);
    }
}
void cleanup() {
    for (auto &pcm : pcm_buffers) { heap_caps_free(pcm); pcm = nullptr; }
    heap_caps_free(arena); arena = nullptr;
    if (workspace) { workspace->~FeatureWorkspace(); heap_caps_free(workspace); workspace = nullptr; }
    if (model_memory) { esp_partition_munmap(model_mapping); model_memory = nullptr; }
}
void *allocate(size_t bytes) {
    // Keep internal SRAM available for I2S DMA, USB queues and task stacks.
    // A board without usable PSRAM reports unavailable instead of exhausting
    // the heap and rebooting while initializing the audio driver.
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
}

bool music_detector_start() {
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "music_model");
    if (!partition || partition->size < kModelBytes || esp_partition_mmap(partition, 0, kModelBytes,
            ESP_PARTITION_MMAP_DATA, &model_memory, &model_mapping) != ESP_OK) {
        ESP_LOGE(kTag, "Music model partition unavailable"); return false;
    }
    uint8_t digest[32];
    size_t digest_bytes = 0;
    flatbuffers::Verifier verifier(static_cast<const uint8_t *>(model_memory), kModelBytes);
    if (psa_crypto_init() != PSA_SUCCESS || psa_hash_compute(PSA_ALG_SHA_256,
        static_cast<const uint8_t *>(model_memory), kModelBytes, digest, sizeof(digest), &digest_bytes) != PSA_SUCCESS ||
        digest_bytes != sizeof(digest) ||
        std::memcmp(digest, kModelSha, sizeof(digest)) != 0 || !tflite::VerifyModelBuffer(verifier) ||
        tflite::GetModel(model_memory)->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(kTag, "Music model integrity/schema check failed"); cleanup(); return false;
    }
    arena = static_cast<uint8_t *>(allocate(kArenaBytes));
    void *feature_memory = allocate(sizeof(music_detection::FeatureWorkspace));
    if (feature_memory) workspace = new (feature_memory) music_detection::FeatureWorkspace{};
    for (auto &pcm : pcm_buffers) pcm = static_cast<uint8_t *>(allocate(music_detection::kSamples * 2));
    if (!arena || !workspace || !pcm_buffers[0] || !pcm_buffers[1]) {
        ESP_LOGE(kTag, "Insufficient classifier memory; music remains unknown"); cleanup(); return false;
    }
    windows.initialize(pcm_buffers[0], pcm_buffers[1]);
    if (xTaskCreatePinnedToCore(detector_task, "music_detect", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
        ESP_LOGE(kTag, "Classifier task unavailable"); cleanup(); return false;
    }
    return true;
}
bool music_detector_ready() { return running.load(); }
void music_detector_audio(const int32_t *stereo, size_t frames) {
    if (!running.load()) return;
    const int64_t now = esp_timer_get_time();
    const unsigned generation = epoch.load();
    for (size_t frame = 0; frame < frames; ++frame) {
        decimator.push(stereo[frame * 2], [generation, now](int16_t sample) {
            windows.push(sample, generation, now);
        });
    }
}
void music_detector_discontinuity() {
    epoch.fetch_add(1);
    windows.discontinuity(); decimator = {}; quiet_since = 0; quiet_confirmed = false;
}
music_detection::State music_detector_state(bool audible, int64_t now) {
    if (!running.load()) return music_detection::State::Unknown;
    if (audible) { quiet_since = 0; quiet_confirmed = false; }
    else if (!quiet_since) quiet_since = now;
    if (!audible && now - quiet_since >= 1500000) {
        // Silence invalidates old positive evidence, so speech after a pause
        // cannot inherit the preceding song's decision.
        if (!quiet_confirmed) { epoch.fetch_add(1); windows.discontinuity(); quiet_confirmed = true; }
        return music_detection::State::Quiet;
    }
    portENTER_CRITICAL(&result_lock);
    const auto state = result;
    const bool valid = result_epoch == epoch.load() && now - result_at < 3000000;
    portEXIT_CRITICAL(&result_lock);
    return valid ? state : music_detection::State::Unknown;
}
