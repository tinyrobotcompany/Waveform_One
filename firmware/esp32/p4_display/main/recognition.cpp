#include "recognition.h"
#include "recognition_metadata.h"
#include "recognition_http.h"
#include "recognition_policy.h"
#include "audio_capture.h"
#include "capture_completion.h"
#include "fingerprint.h"
#include "usb_controller.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <strings.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char kTag[] = "waveform_recognition";

struct HeapFree { void operator()(void *pointer) const { heap_caps_free(pointer); } };
template<class T> using Heap = std::unique_ptr<T, HeapFree>;
struct JsonFree { void operator()(cJSON *value) const { cJSON_Delete(value); } };
using Json = std::unique_ptr<cJSON, JsonFree>;

using recognition_policy::Session;
std::mutex session_mutex;
recognition_policy::Sessions sessions;
RecognitionCallback show = nullptr;
std::atomic<int64_t> last_activity_us{0};

capture_completion::Mailbox clips(heap_caps_free);

Session session()
{
    std::lock_guard<std::mutex> lock(session_mutex);
    return sessions.snapshot();
}

void publish(uint64_t expected_epoch, RecognitionStatus status,
             const RecognitionTrack *track = nullptr, const RecognitionArtwork *artwork = nullptr)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (expected_epoch == sessions.snapshot().epoch && show != nullptr) show(status, track, artwork);
}

void captured(AudioCaptureStatus status, uint8_t *pcm, size_t, uint64_t capture_id)
{
    clips.complete(capture_id, status, pcm);
}

using recognition_http::Response;
using recognition_http::fetch;

std::string uuid()
{
    uint8_t bytes[16]; esp_fill_random(bytes, sizeof(bytes));
    bytes[6] = (bytes[6] & 0x0f) | 0x40; bytes[8] = (bytes[8] & 0x3f) | 0x80;
    char output[37];
    std::snprintf(output, sizeof(output),
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return output;
}

enum class Lookup { Matched, NoMatch, Failed };
Lookup lookup(const fingerprint::Signature &signature, RecognitionTrack &track, int64_t &service_delay)
{
    Json root(cJSON_CreateObject());
    if (!root) return Lookup::Failed;
    const std::string uri = fingerprint::uri(signature);
    cJSON *sig = cJSON_AddObjectToObject(root.get(), "signature");
    if (uri.empty() || sig == nullptr || !cJSON_AddStringToObject(sig, "uri", uri.c_str())
        || !cJSON_AddNumberToObject(sig, "samplems", signature.samples / 16)
        || !cJSON_AddStringToObject(root.get(), "timezone", "UTC")
        || !cJSON_AddNumberToObject(root.get(), "timestamp", double(std::time(nullptr)) * 1000)
        || !cJSON_AddObjectToObject(root.get(), "context")
        || !cJSON_AddObjectToObject(root.get(), "geolocation")) return Lookup::Failed;
    Heap<char> body(cJSON_PrintUnformatted(root.get()));
    if (!body) return Lookup::Failed;
    const std::string url = "https://amp.shazam.com/discovery/v5/en-US/GB/iphone/-/tag/"
        + uuid() + "/" + uuid()
        + "?sync=true&webv3=true&sampling=true&connected=&shazamapiversion=v3"
          "&sharehub=true&hubv5minorversion=v5.1&hidelb=true&video=v3";
    Response response;
    const bool received = fetch(url.c_str(), body.get(), 128 * 1024, response);
    service_delay = response.retry_after_us;
    if (!received) return Lookup::Failed;
    Json result(cJSON_ParseWithLengthOpts(response.bytes.get(), response.size + 1, nullptr, true));
    if (!result || !cJSON_IsObject(result.get())) {
        ESP_LOGW(kTag, "Recognition response invalid JSON: bytes=%u", static_cast<unsigned>(response.size));
        return Lookup::Failed;
    }
    if (recognition_metadata::parse(result.get(), track)) return Lookup::Matched;
    const auto *matches = recognition_metadata::field(result.get(), "matches");
    if (cJSON_IsArray(matches) && cJSON_GetArraySize(matches) == 0) return Lookup::NoMatch;
    ESP_LOGW(kTag, "Recognition response missing usable track: matches=%d bytes=%u",
        cJSON_IsArray(matches) ? cJSON_GetArraySize(matches) : -1, static_cast<unsigned>(response.size));
    return Lookup::Failed;
}

struct Image {
    Heap<uint8_t> pixels;
    unsigned width = 0, height = 0;
    RecognitionArtwork view() const { return {pixels.get(), width, height}; }
};

Image download_artwork(const char *url)
{
    Image image;
    if (!recognition_metadata::artwork_url(url)) return image;
    Response response;
    if (!fetch(url, nullptr, 512 * 1024, response)) return image;
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    jpeg_dec_handle_t decoder = nullptr;
    if (jpeg_dec_open(&config, &decoder) != JPEG_ERR_OK) return image;
    struct Close { jpeg_dec_handle_t decoder; ~Close() { jpeg_dec_close(decoder); } } close{decoder};
    jpeg_dec_io_t io{};
    io.inbuf = reinterpret_cast<uint8_t *>(response.bytes.get());
    io.inbuf_len = response.size;
    jpeg_dec_header_info_t header{};
    if (jpeg_dec_parse_header(decoder, &io, &header) != JPEG_ERR_OK
        || header.width == 0 || header.height == 0 || header.width > 1024 || header.height > 1024) return image;
    image.width = header.width; image.height = header.height;
    const size_t bytes = image.width * image.height * 2;
    image.pixels.reset(static_cast<uint8_t *>(heap_caps_aligned_alloc(16, bytes,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!image.pixels) return {};
    io.outbuf = image.pixels.get(); io.out_size = bytes;
    if (jpeg_dec_process(decoder, &io) != JPEG_ERR_OK) return {};
    return image;
}

void recognition_task(void *)
{
    RecognitionTrack track{};
    Image image;
    uint64_t track_epoch = 0;
    int64_t matched_at = 0;
    recognition_policy::Retries retries;
    RecognitionStatus last_status = RecognitionStatus::Waiting;
    while (true) {
        const Session start = session();
        const int64_t now = esp_timer_get_time();
        if (track_epoch != start.epoch || recognition_policy::expired(matched_at, now, recognition_last_activity_us())) {
            if (track_epoch != start.epoch) retries.activity_changed(now);
            track = {}; image = {}; track_epoch = start.epoch;
        }
        const bool has_track = track.title[0] != '\0';
        auto picture = image.view();
        if (!start.ready || std::time(nullptr) < 1704067200) {
            publish(start.epoch, RecognitionStatus::Waiting, has_track ? &track : nullptr,
                image.pixels ? &picture : nullptr);
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        if (!retries.due(now)) {
            publish(start.epoch, last_status,
                has_track ? &track : nullptr, image.pixels ? &picture : nullptr);
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        retries.begin(now);
        const uint64_t capture_id = clips.begin();
        const int64_t capture_deadline = esp_timer_get_time()
            + recognition_policy::kCaptureCompletionTimeoutUs;
        const auto queued = usb_controller_capture(captured, capture_id);
        if (!retries.capture_requested(queued, esp_timer_get_time())) {
            clips.cancel(capture_id);
            last_status = RecognitionStatus::Unavailable;
            publish(start.epoch, last_status, has_track ? &track : nullptr,
                image.pixels ? &picture : nullptr);
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        publish(start.epoch, RecognitionStatus::Capturing, has_track ? &track : nullptr,
            image.pixels ? &picture : nullptr);
        capture_completion::Clip clip{};
        const auto completion = capture_completion::wait(clips, capture_id, start.epoch,
            capture_deadline, clip, esp_timer_get_time, [] { return session().epoch; },
            [] { vTaskDelay(pdMS_TO_TICKS(100)); });
        if (completion == capture_completion::WaitResult::Cancelled) continue;
        if (completion == capture_completion::WaitResult::TimedOut) {
            ESP_LOGW(kTag, "Audio capture completion timed out");
            retries.failed(esp_timer_get_time());
            last_status = RecognitionStatus::Unavailable;
            publish(start.epoch, last_status, has_track ? &track : nullptr,
                image.pixels ? &picture : nullptr);
            continue;
        }
        Heap<uint8_t> pcm(clip.pcm);
        if (session().epoch != start.epoch) continue;
        Lookup outcome = Lookup::Failed;
        int64_t service_delay = 0;
        RecognitionTrack next{};
        if (clip.status == AudioCaptureStatus::Complete) {
            const int64_t processing_started = esp_timer_get_time();
            void *memory = heap_caps_malloc(sizeof(fingerprint::Workspace), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            Heap<fingerprint::Workspace> workspace(memory != nullptr
                ? new (memory) fingerprint::Workspace : nullptr);
            fingerprint::Signature signature;
            if (workspace && fingerprint::generate(pcm.get(), audio_capture::kBytes, *workspace, signature,
                    [] { vTaskDelay(pdMS_TO_TICKS(1)); })) {
                pcm.reset(); workspace.reset();
                ESP_LOGI(kTag, "Fingerprint samples=%u peaks=%u duration_ms=%lld",
                    signature.samples, static_cast<unsigned>(signature.peaks()),
                    static_cast<long long>((esp_timer_get_time() - processing_started) / 1000));
                if (session().epoch != start.epoch) continue;
                if (signature.peaks() < 5) outcome = Lookup::NoMatch;
                else {
                    publish(start.epoch, RecognitionStatus::Identifying, has_track ? &track : nullptr,
                        image.pixels ? &picture : nullptr);
                    const int64_t lookup_started = esp_timer_get_time();
                    outcome = lookup(signature, next, service_delay);
                    ESP_LOGI(kTag, "Recognition result=%u duration_ms=%lld",
                        static_cast<unsigned>(outcome),
                        static_cast<long long>((esp_timer_get_time() - lookup_started) / 1000));
                }
            }
        }
        if (session().epoch != start.epoch) continue;
        if (outcome == Lookup::Matched) {
            next.matched_at_us = esp_timer_get_time();
            const bool same = std::strcmp(track.title, next.title) == 0
                && std::strcmp(track.artist, next.artist) == 0
                && std::strcmp(track.artwork_url, next.artwork_url) == 0;
            if (!same) image = {};
            track = next; track_epoch = start.epoch; matched_at = next.matched_at_us;
            last_status = RecognitionStatus::Matched;
            picture = image.view();
            // Show metadata immediately; an unavailable cover must not delay it.
            publish(start.epoch, RecognitionStatus::Matched, &track, image.pixels ? &picture : nullptr);
            if (!image.pixels && track.artwork_url[0] != '\0') {
                const int64_t artwork_started = esp_timer_get_time();
                image = download_artwork(track.artwork_url);
                ESP_LOGI(kTag, "Artwork result=%s duration_ms=%lld", image.pixels ? "ready" : "unavailable",
                    static_cast<long long>((esp_timer_get_time() - artwork_started) / 1000));
                if (session().epoch != start.epoch) continue;
                picture = image.view();
                publish(start.epoch, RecognitionStatus::Matched, &track, image.pixels ? &picture : nullptr);
            }
            retries.succeeded(esp_timer_get_time(), true);
            ESP_LOGI(kTag, "Recognition matched");
        } else {
            if (recognition_policy::expired(matched_at, esp_timer_get_time(), recognition_last_activity_us())) { track = {}; image = {}; }
            picture = image.view();
            if (outcome == Lookup::Failed) {
                retries.failed(esp_timer_get_time(), service_delay);
            } else { retries.succeeded(esp_timer_get_time(), false); }
            last_status = outcome == Lookup::Failed ? RecognitionStatus::Unavailable : RecognitionStatus::NoMatch;
            publish(start.epoch, last_status,
                track.title[0] != '\0' ? &track : nullptr, image.pixels ? &picture : nullptr);
            ESP_LOGI(kTag, "%s", outcome == Lookup::Failed ? "Recognition unavailable; retry scheduled" : "Recognition: no match");
        }
    }
}
}

void recognition_start(RecognitionCallback callback)
{
    {
        std::lock_guard<std::mutex> lock(session_mutex);
        show = callback;
    }
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(recognition_task, "recognition", 24576,
        nullptr, 4, nullptr, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

void recognition_set_network(bool available, const char *address)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (!sessions.set_network(available, address != nullptr ? address : "")) return;
    if (show != nullptr) show(RecognitionStatus::Waiting, nullptr, nullptr);
}

void recognition_set_controller(bool available)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (!sessions.set_controller(available)) return;
    if (show != nullptr) show(RecognitionStatus::Waiting, nullptr, nullptr);
}

int64_t recognition_last_activity_us() { return last_activity_us.load(); }

void recognition_set_activity(audio_activity::State state)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (state == audio_activity::State::Playing) last_activity_us.store(esp_timer_get_time());
    if (!sessions.set_activity(state)) return;
    // Epoch change invalidates a capture/lookup spanning stopped playback.
    if (show != nullptr) show(RecognitionStatus::Waiting, nullptr, nullptr);
}
