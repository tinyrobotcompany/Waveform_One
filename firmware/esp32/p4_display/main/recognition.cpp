#include "recognition.h"
#include "recognition_metadata.h"
#include "recognition_http.h"
#include "recognition_policy.h"
#include "album_resolver.h"
#include "fingerprint.h"
#include "listening.h"
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

// Picks the release the listener expects; keeps the service's on failure.
void resolve_album(RecognitionTrack &track, album_resolver::Context &context)
{
    const int64_t started = esp_timer_get_time();
    Response response;
    album_resolver::Choice choice;
    bool chosen = false;
    if (fetch(album_resolver::search_url(track).c_str(), nullptr, 256 * 1024, response)) {
        Json result(cJSON_ParseWithLengthOpts(response.bytes.get(), response.size + 1, nullptr, true));
        chosen = result && album_resolver::choose(result.get(), track, context, started, choice);
    }
    ESP_LOGI(kTag, "Album result=%s service_album=\"%s\" chosen_album=\"%s\" length_s=%.0f duration_ms=%lld",
        chosen ? "chosen" : "kept", track.album, chosen ? choice.album : track.album, choice.duration_s,
        static_cast<long long>((esp_timer_get_time() - started) / 1000));
    if (!chosen) return;
    std::memcpy(track.album, choice.album, sizeof(track.album));
    if (choice.artwork_url[0] != '\0') std::memcpy(track.artwork_url, choice.artwork_url, sizeof(track.artwork_url));
    track.duration_s = choice.duration_s;
    context = {choice.collection_id, started};
}

bool same_song(const RecognitionTrack &a, const RecognitionTrack &b)
{
    return a.title[0] != '\0' && std::strcmp(a.title, b.title) == 0 && std::strcmp(a.artist, b.artist) == 0;
}

int64_t ms(uint32_t hops) { return int64_t(hops) * 1000 / listening::kHopsPerSecond; }

void recognition_task(void *)
{
    void *memory = heap_caps_aligned_alloc(16, sizeof(fingerprint::Analyzer), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(memory != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    Heap<fingerprint::Analyzer> analyzer(new (memory) fingerprint::Analyzer);
    const double fft_error = fingerprint::self_test();
    ESP_LOGI(kTag, "Fingerprint FFT self-test relative_error=%.2e", fft_error);
    ESP_ERROR_CHECK(fft_error < 1e-4 ? ESP_OK : ESP_ERR_INVALID_STATE);

    listening::Detector detector;
    recognition_policy::Planner planner;
    recognition_policy::Retries retries;
    recognition_policy::Budget budget;
    album_resolver::Context album_context;
    RecognitionTrack track{};
    Image image;
    RecognitionStatus status = RecognitionStatus::Waiting;
    uint64_t epoch = 0, position = 0;
    uint32_t segment = UINT32_MAX, sound_hop = 0;
    int64_t level_logged = 0, analysis_us = 0;
    uint32_t analysed = 0;
    unsigned lookups = 0;
    static int16_t samples[fingerprint::kHopSamples * 32];

    const auto show_now = [&](RecognitionStatus next) {
        status = next;
        auto picture = image.view();
        publish(epoch, status, track.title[0] != '\0' ? &track : nullptr, image.pixels ? &picture : nullptr);
    };
    const auto clear = [&](const char *reason) {
        if (track.title[0] != '\0') ESP_LOGI(kTag, "Display cleared reason=%s", reason);
        track = {}; image = {};
    };

    while (true) {
        const Session current = session();
        if (current.epoch != epoch) {
            // Network or controller changed: drop anything shown or pending.
            epoch = current.epoch;
            planner.stopped(); clear("session_changed");
            // Music that is still playing is identified afresh from now on.
            if (detector.sound()) planner.sound_started(analyzer->hops());
            show_now(RecognitionStatus::Waiting);
        }

        // 1. Analyse every new hop of audio as it arrives.
        const auto *audio = usb_controller_audio();
        uint32_t hops = 0;
        if (audio != nullptr) {
            auto read = audio->read(position, samples, std::size(samples));
            if (read.segment != segment || read.skipped) {
                ESP_LOGI(kTag, "Audio %s; analysis restarted", segment == UINT32_MAX ? "started"
                    : read.skipped ? "fell behind" : "interrupted");
                segment = read.segment;
                analyzer->reset(); detector.discontinuity();
                if (planner.stopped()) { clear("audio_interrupted"); show_now(RecognitionStatus::Waiting); }
            }
            const int64_t started = esp_timer_get_time();
            hops = read.count / fingerprint::kHopSamples;
            for (uint32_t i = 0; i < hops; ++i) {
                const int16_t *hop = samples + i * fingerprint::kHopSamples;
                analyzer->push(hop);
                uint32_t start = 0;
                const auto event = detector.push(hop, fingerprint::kHopSamples, analyzer->hops() - 1, start);
                if (event == listening::Event::SoundStarted) {
                    sound_hop = start;
                    planner.sound_started(start);
                    ESP_LOGI(kTag, "Sound started level=%.1fdB background=%.1fdB",
                        detector.level_db(), detector.background_db());
                    if (current.ready) show_now(RecognitionStatus::Capturing);
                } else if (event == listening::Event::SilenceStarted) {
                    ESP_LOGI(kTag, "Silence level=%.1fdB background=%.1fdB",
                        detector.level_db(), detector.background_db());
                    planner.stopped(); clear("silence");
                    show_now(RecognitionStatus::Waiting);
                }
            }
            position = read.position + uint64_t(hops) * fingerprint::kHopSamples;
            analysis_us += esp_timer_get_time() - started; analysed += hops;
        }
        const int64_t now = esp_timer_get_time();
        if (now - level_logged >= 10000000) {
            ESP_LOGI(kTag, "Listening sound=%d level=%.1fdB background=%.1fdB analysis_cpu=%.1f%%",
                detector.sound(), detector.level_db(), detector.background_db(),
                analysed ? 100.0 * analysis_us / (ms(analysed) * 1000) : 0.0);
            level_logged = now; analysis_us = 0; analysed = 0;
        }

        // 2. Look up the audio the planner asks for, within the service's limits.
        recognition_policy::Window window;
        if (current.ready && std::time(nullptr) >= 1704067200 && retries.due(now)
            && budget.available(now) && planner.due(analyzer->hops(), window)) {
            planner.attempted(window.last);
            const unsigned id = ++lookups;
            fingerprint::Signature signature;
            const bool signed_ok = analyzer->signature(window.first, window.last, signature);
            ESP_LOGI(kTag, "Lookup id=%u audio_ms=%lld since_sound_ms=%lld peaks=%u%s",
                id, static_cast<long long>(ms(window.last - window.first)),
                static_cast<long long>(ms(window.last - sound_hop)),
                static_cast<unsigned>(signature.peaks()), signed_ok ? "" : " signature_unavailable");
            Lookup outcome = Lookup::NoMatch;
            int64_t service_delay = 0;
            RecognitionTrack next{};
            if (signed_ok && signature.peaks() >= 5) {
                budget.spend(now);
                if (track.title[0] == '\0') show_now(RecognitionStatus::Identifying);
                const int64_t lookup_started = esp_timer_get_time();
                outcome = lookup(signature, next, service_delay);
                ESP_LOGI(kTag, "Lookup id=%u result=%s offset_s=%.1f duration_ms=%lld", id,
                    outcome == Lookup::Matched ? "matched" : outcome == Lookup::NoMatch ? "no_match" : "failed",
                    next.offset_s, static_cast<long long>((esp_timer_get_time() - lookup_started) / 1000));
            }
            if (session().epoch != epoch) continue;
            detector.set_unrecognised(outcome == Lookup::NoMatch && planner.unidentified());
            if (outcome == Lookup::Matched) {
                retries.succeeded();
                if (same_song(track, next)) {
                    // Still the same song: keep its album, artwork and length.
                    track.offset_s = next.offset_s;
                } else {
                    clear("new_song");
                    // Title first; the album and cover follow a moment later.
                    track = next;
                    track.album[0] = '\0'; track.artwork_url[0] = '\0';
                    track.matched_at_us = esp_timer_get_time();
                    show_now(RecognitionStatus::Matched);
                    resolve_album(next, album_context);
                    if (session().epoch != epoch) continue;
                    std::memcpy(track.album, next.album, sizeof(track.album));
                    std::memcpy(track.artwork_url, next.artwork_url, sizeof(track.artwork_url));
                    track.duration_s = next.duration_s;
                    show_now(RecognitionStatus::Matched);
                    if (track.artwork_url[0] != '\0') {
                        const int64_t artwork_started = esp_timer_get_time();
                        image = download_artwork(track.artwork_url);
                        ESP_LOGI(kTag, "Artwork result=%s duration_ms=%lld", image.pixels ? "ready" : "unavailable",
                            static_cast<long long>((esp_timer_get_time() - artwork_started) / 1000));
                        if (session().epoch != epoch) continue;
                        show_now(RecognitionStatus::Matched);
                    }
                }
                planner.matched(window, analyzer->hops(), track.offset_s, track.duration_s);
                ESP_LOGI(kTag, "Recognition matched id=%u since_sound_ms=%lld next_song_in_ms=%lld",
                    id, static_cast<long long>(ms(window.last - sound_hop)),
                    planner.expected_end() ? static_cast<long long>(ms(planner.expected_end() - analyzer->hops())) : -1LL);
            } else if (outcome == Lookup::NoMatch) {
                retries.succeeded();
                if (planner.missed()) clear("song_no_longer_heard");
                // Keep "finding your song" up while the first tries continue.
                show_now(track.title[0] != '\0' ? RecognitionStatus::Matched
                    : planner.active() && !planner.unidentified() ? RecognitionStatus::Capturing
                    : RecognitionStatus::NoMatch);
            } else {
                retries.failed(esp_timer_get_time(), service_delay);
                ESP_LOGI(kTag, "Recognition unavailable; retry_ms=%lld",
                    static_cast<long long>(retries.remaining_us(esp_timer_get_time()) / 1000));
                show_now(RecognitionStatus::Unavailable);
            }
            continue;
        }
        // Caught up: wait for more audio. Otherwise yield and keep going.
        vTaskDelay(pdMS_TO_TICKS(hops == std::size(samples) / fingerprint::kHopSamples ? 1 : 20));
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
