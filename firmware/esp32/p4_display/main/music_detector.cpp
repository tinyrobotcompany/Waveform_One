#include "music_detector.h"
#include "music_features.h"
#include "music_policy.h"
#include "recognition.h"
#include "recognition_input.h"

#include <atomic>
#include <cstring>
#include <new>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
constexpr char kTag[] = "waveform_music";
// firmware/esp32/common/music_model/provenance.json
constexpr size_t kModelBytes = 4046312, kArenaBytes = 1536 * 1024; // P4 ESP-NN kernels need ~1.06 MB of scratch
constexpr uint8_t kModelSha[32] = {0x41,0x17,0xec,0x5b,0x22,0xde,0x5e,0x61,0x87,0xc9,0xc2,0x8f,0x6a,0xa8,0x37,0x83,
                                   0xde,0xbb,0x53,0x07,0x6f,0x94,0xfd,0x69,0x3f,0x4e,0x6f,0x3d,0x73,0xf6,0x4c,0x70};

std::atomic<MusicVerdict> verdict{MusicVerdict::Unavailable};

struct Work {
    music_detection::FeatureWorkspace features;
    int16_t pcm[music_detection::kSamples];
};

void yield_frontend() { vTaskDelay(1); }

void detector_task(void *model)
{
    auto *arena = static_cast<uint8_t *>(heap_caps_malloc(kArenaBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    void *work_memory = heap_caps_malloc(sizeof(Work), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    tflite::MicroMutableOpResolver<5> resolver;
    const bool registered = resolver.AddConv2D() == kTfLiteOk && resolver.AddDepthwiseConv2D() == kTfLiteOk
        && resolver.AddMean() == kTfLiteOk && resolver.AddFullyConnected() == kTfLiteOk && resolver.AddLogistic() == kTfLiteOk;
    if (!arena || !work_memory || !registered) {
        ESP_LOGE(kTag, "Classifier memory unavailable; LED and lookups use the level detector alone");
        heap_caps_free(arena); heap_caps_free(work_memory); heap_caps_free(model);
        vTaskDelete(nullptr); return;
    }
    auto *work = new (work_memory) Work{};
    tflite::MicroInterpreter interpreter(tflite::GetModel(model), resolver, arena, kArenaBytes);
    const auto allocated = interpreter.AllocateTensors();
    if (allocated != kTfLiteOk ||
        interpreter.input(0)->type != kTfLiteInt8 || interpreter.input(0)->bytes != 6144 ||
        interpreter.output(0)->type != kTfLiteInt8 || interpreter.output(0)->bytes != 521) {
        ESP_LOGE(kTag, "Classifier initialization failed allocate=%d input=%d/%u output=%d/%u; "
            "LED and lookups use the level detector alone", int(allocated),
            allocated == kTfLiteOk ? int(interpreter.input(0)->type) : -1,
            allocated == kTfLiteOk ? unsigned(interpreter.input(0)->bytes) : 0,
            allocated == kTfLiteOk ? int(interpreter.output(0)->type) : -1,
            allocated == kTfLiteOk ? unsigned(interpreter.output(0)->bytes) : 0);
        vTaskDelete(nullptr); return;
    }
    ESP_LOGI(kTag, "Classifier ready arena=%u", unsigned(interpreter.arena_used_bytes()));
    verdict.store(MusicVerdict::Undecided);
    music_detection::Presence presence;
    uint32_t segment = UINT32_MAX;
    int64_t last_log = 0;
    while (true) {
        const auto *audio = recognition_input_audio();
        // Only sound needs a verdict: silence resets it, and costs no CPU.
        if (!audio || !recognition_sound_heard()) {
            presence.reset();
            verdict.store(MusicVerdict::Undecided);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const uint64_t end = audio->written();
        if (end < music_detection::kSamples) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        // The newest 0.975 s; consecutive windows overlap when inference is fast.
        const auto read = audio->read(end - music_detection::kSamples, work->pcm, music_detection::kSamples);
        if (read.segment != segment) { presence.reset(); segment = read.segment; }
        if (read.skipped || read.count != music_detection::kSamples) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        const int64_t started = esp_timer_get_time();
        const auto *bytes = reinterpret_cast<const uint8_t *>(work->pcm);
        constexpr size_t size = music_detection::kSamples * 2;
        if (!music_detection::features(bytes, size, work->features, interpreter.input(0)->data.int8, 6144,
                yield_frontend, music_detection::input_gain(bytes, size)) || interpreter.Invoke() != kTfLiteOk) {
            presence.reset();
            verdict.store(MusicVerdict::Undecided);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        const int64_t now = esp_timer_get_time();
        const float score = music_detection::music_score(interpreter.output(0)->data.int8);
        presence.update(score, now);
        const auto state = presence.music(now) ? MusicVerdict::Music
            : presence.not_music(now) ? MusicVerdict::NotMusic : MusicVerdict::Undecided;
        verdict.store(state);
        if (now - last_log >= 2000000 || score >= music_detection::Presence::kWeak) {
            last_log = now;
            ESP_LOGI(kTag, "score=%.3f verdict=%s processing_ms=%lld", score,
                state == MusicVerdict::Music ? "music" : state == MusicVerdict::NotMusic ? "not_music" : "undecided",
                static_cast<long long>((now - started) / 1000));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
}

void music_detector_start()
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY, "music_model");
    // Copied into PSRAM: faster to run from than flash, and no large mapping.
    auto *model = static_cast<uint8_t *>(heap_caps_malloc(kModelBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    uint8_t digest[32];
    size_t digest_bytes = 0;
    if (!partition || partition->size < kModelBytes || !model
        || esp_partition_read(partition, 0, model, kModelBytes) != ESP_OK
        || psa_crypto_init() != PSA_SUCCESS
        || psa_hash_compute(PSA_ALG_SHA_256, model, kModelBytes, digest, sizeof(digest), &digest_bytes) != PSA_SUCCESS
        || digest_bytes != sizeof(digest) || std::memcmp(digest, kModelSha, sizeof(digest)) != 0) {
        ESP_LOGE(kTag, "Music model missing or corrupt; LED and lookups use the level detector alone");
        heap_caps_free(model);
        return;
    }
    flatbuffers::Verifier verifier(model, kModelBytes);
    if (!tflite::VerifyModelBuffer(verifier) || tflite::GetModel(model)->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(kTag, "Music model schema check failed; LED and lookups use the level detector alone");
        heap_caps_free(model);
        return;
    }
    // Lowest priority on the core the recognition tasks do not use: it only
    // takes otherwise idle time.
    if (xTaskCreatePinnedToCore(detector_task, "music_detect", 8192, model, 1, nullptr, 0) != pdPASS) {
        ESP_LOGE(kTag, "Classifier task unavailable; LED and lookups use the level detector alone");
        heap_caps_free(model);
    }
}

MusicVerdict music_detector_verdict() { return verdict.load(); }
