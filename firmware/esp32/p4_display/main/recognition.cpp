#include "recognition.h"
#include "recognition_metadata.h"
#include "recognition_policy.h"
#include "audio_capture.h"
#include "fingerprint.h"
#include "usb_controller.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <new>
#include <string>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace {
constexpr char kTag[] = "waveform_recognition";
constexpr int64_t kRequestTimeoutUs = 25000000;

struct HeapFree { void operator()(void *pointer) const { heap_caps_free(pointer); } };
template<class T> using Heap = std::unique_ptr<T, HeapFree>;
struct JsonFree { void operator()(cJSON *value) const { cJSON_Delete(value); } };
using Json = std::unique_ptr<cJSON, JsonFree>;

using recognition_policy::Session;
std::mutex session_mutex;
recognition_policy::Sessions sessions;
RecognitionCallback show = nullptr;

struct Clip { AudioCaptureStatus status; uint8_t *pcm; };
QueueHandle_t clips = nullptr;

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

void captured(AudioCaptureStatus status, uint8_t *pcm, size_t)
{
    Clip clip{status, pcm};
    if (xQueueSend(clips, &clip, 0) != pdTRUE) heap_caps_free(pcm);
}

struct Response {
    Heap<char> bytes;
    size_t size = 0;
};

bool fetch(const char *url, const char *body, size_t limit, Response &response)
{
    response = {};
    response.bytes.reset(static_cast<char *>(heap_caps_malloc(limit + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!response.bytes) return false;
    esp_http_client_config_t config{};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 5000;
    config.disable_auto_redirect = true;
    config.method = body == nullptr ? HTTP_METHOD_GET : HTTP_METHOD_POST;
    config.user_agent = "Dalvik/2.1.0 (Linux; U; Android 5.0.2; VS980 4G Build/LRX22G)";
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) return false;
    struct Cleanup {
        esp_http_client_handle_t client;
        ~Cleanup() { esp_http_client_close(client); esp_http_client_cleanup(client); }
    } cleanup{client};
    const int64_t deadline = esp_timer_get_time() + kRequestTimeoutUs;
    const size_t length = body != nullptr ? std::strlen(body) : 0;
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    if (body != nullptr) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "X-Shazam-Platform", "IPHONE");
        esp_http_client_set_header(client, "X-Shazam-AppVersion", "14.1.0");
        esp_http_client_set_header(client, "Accept-Language", "en-US");
    }
    if (length > 65536 || esp_http_client_open(client, static_cast<int>(length)) != ESP_OK) return false;
    size_t written = 0;
    while (written < length && esp_timer_get_time() < deadline) {
        const int count = esp_http_client_write(client, body + written, length - written);
        if (count <= 0) return false;
        written += count;
    }
    if (written != length || esp_timer_get_time() >= deadline) return false;
    const int64_t declared = esp_http_client_fetch_headers(client);
    if (declared < 0 || declared > static_cast<int64_t>(limit)
        || esp_http_client_get_status_code(client) != 200) return false;
    while (esp_timer_get_time() < deadline) {
        // Read one extra byte to detect bodies exceeding the maximum, including chunked bodies.
        const size_t room = limit + 1 - response.size;
        if (room == 0) return false;
        const int count = esp_http_client_read(client, response.bytes.get() + response.size,
            std::min<size_t>(room, 4096));
        if (count == -ESP_ERR_HTTP_EAGAIN) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        if (count < 0) return false;
        if (count == 0) {
            if (!esp_http_client_is_complete_data_received(client)) return false;
            response.bytes.get()[response.size] = '\0';
            return response.size <= limit;
        }
        response.size += count;
        if (response.size > limit) return false;
    }
    return false;
}

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
Lookup lookup(const fingerprint::Signature &signature, RecognitionTrack &track)
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
    if (!fetch(url.c_str(), body.get(), 128 * 1024, response)) return Lookup::Failed;
    Json result(cJSON_ParseWithLengthOpts(response.bytes.get(), response.size + 1, nullptr, true));
    if (!result || !cJSON_IsObject(result.get())) return Lookup::Failed;
    if (recognition_metadata::parse(result.get(), track)) return Lookup::Matched;
    const auto *matches = recognition_metadata::field(result.get(), "matches");
    return cJSON_IsArray(matches) && cJSON_GetArraySize(matches) == 0 ? Lookup::NoMatch : Lookup::Failed;
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
        if (track_epoch != start.epoch || recognition_policy::expired(matched_at, now)) {
            track = {}; image = {}; track_epoch = start.epoch;
        }
        const bool has_track = track.title[0] != '\0';
        auto picture = image.view();
        if (!start.ready || std::time(nullptr) < 1704067200) {
            publish(start.epoch, RecognitionStatus::Waiting);
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        if (!retries.due(now)) {
            publish(start.epoch, last_status,
                has_track ? &track : nullptr, image.pixels ? &picture : nullptr);
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        retries.begin(now);
        const auto queued = usb_controller_capture(captured);
        if (queued != AudioCaptureRequestResult::Queued) {
            publish(start.epoch, RecognitionStatus::Unavailable, has_track ? &track : nullptr,
                image.pixels ? &picture : nullptr);
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        publish(start.epoch, RecognitionStatus::Capturing, has_track ? &track : nullptr,
            image.pixels ? &picture : nullptr);
        Clip clip{};
        // One request in flight. Always consume its completion and release PCM,
        // even after an epoch change. The controller enforces the capture deadline.
        while (xQueueReceive(clips, &clip, pdMS_TO_TICKS(1000)) != pdTRUE) {}
        Heap<uint8_t> pcm(clip.pcm);
        if (session().epoch != start.epoch) continue;
        Lookup outcome = Lookup::Failed;
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
                    outcome = lookup(signature, next);
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
            retries.succeeded(esp_timer_get_time());
            ESP_LOGI(kTag, "Recognition matched");
        } else {
            if (recognition_policy::expired(matched_at, esp_timer_get_time())) { track = {}; image = {}; }
            picture = image.view();
            if (outcome == Lookup::Failed) {
                retries.failed(esp_timer_get_time());
            } else { retries.succeeded(esp_timer_get_time()); }
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
    clips = xQueueCreate(1, sizeof(Clip));
    ESP_ERROR_CHECK(clips != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
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

void recognition_set_activity(bool active)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (!sessions.set_activity(active)) return;
    // Epoch change invalidates a capture/lookup spanning stopped playback.
    if (show != nullptr) show(RecognitionStatus::Waiting, nullptr, nullptr);
}
