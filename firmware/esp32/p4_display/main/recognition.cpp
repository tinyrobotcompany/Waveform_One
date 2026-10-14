#include "recognition.h"
#include "recognition_metadata.h"
#include "recognition_http.h"
#include "recognition_policy.h"
#include "recognition_evidence.h"
#include "recognition_state.h"
#include "recognition_delivery.h"
#include "acrcloud.h"
#include "acrcloud_config.h"
#include "album_resolver.h"
#include "album_cache.h"
#include "audio_presence.h"
#include "fingerprint.h"
#include "listening.h"
#include "usb_controller.h"

#include <algorithm>
#include <cmath>
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
audio_presence::Snapshot sound_state;
std::atomic_bool track_visible{false};
struct SoundSession { Session session; audio_presence::Snapshot audio; uint64_t transport_epoch; };
SoundSession sound_session() {
    std::lock_guard<std::mutex> lock(session_mutex);
    return {sessions.snapshot(), sound_state, sessions.transport_epoch()};
}

Session session()
{
    std::lock_guard<std::mutex> lock(session_mutex);
    return sessions.snapshot();
}

// Protected by session_mutex. A busy GUI must not silently lose a stop.
recognition_delivery::EmptyUpdate pending_clear;
void flush_clear_locked()
{
    if (pending_clear.flush(sessions.snapshot().epoch, [](RecognitionStatus status) {
        return show && show(status, nullptr, nullptr);
    })) {
        ESP_LOGI(kTag, "Display empty update applied epoch=%llu status=%d",
            static_cast<unsigned long long>(pending_clear.epoch()), int(pending_clear.status()));
    }
}

void clear_display_locked(RecognitionStatus status)
{
    track_visible.store(false);
    pending_clear.request(sessions.snapshot().epoch, status);
    flush_clear_locked();
}

bool publish(uint64_t expected_epoch, RecognitionStatus status,
             const RecognitionTrack *track = nullptr, const RecognitionArtwork *artwork = nullptr)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (expected_epoch == sessions.snapshot().epoch && show != nullptr) {
        if (track == nullptr) { clear_display_locked(status); return true; }
        pending_clear.cancel(); // a newer track supersedes an older clear
        if (!show(status, track, artwork)) return false;
        track_visible.store(track->title[0] != '\0');
    }
    return true; // stale epochs are intentionally discarded, never retried
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

using Lookup = recognition_state::Outcome;
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

constexpr acrcloud::Credentials kAcrcloud{ACRCLOUD_HOST, ACRCLOUD_ACCESS_KEY, ACRCLOUD_ACCESS_SECRET};

// Identifies a stretch of audio with ACRCloud. pcm holds 16 kHz samples.
Lookup identify(const int16_t *pcm, size_t count, RecognitionTrack &track, int64_t &service_delay)
{
    track = {};
    Heap<int16_t> reduced(static_cast<int16_t *>(heap_caps_malloc((count / 2) * sizeof(int16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!reduced) return Lookup::Failed;
    acrcloud::downsample(pcm, count, reduced.get());
    const std::string timestamp = std::to_string(static_cast<long long>(std::time(nullptr)));
    const std::string boundary = "waveform" + uuid();
    const auto body = acrcloud::body(kAcrcloud, timestamp, reduced.get(), count / 2, boundary);
    reduced.reset();
    const std::string type = "multipart/form-data; boundary=" + boundary;
    const std::string url = std::string("https://") + kAcrcloud.host + "/v1/identify";
    Response response;
    // One connection kept open: no TCP/TLS handshake per lookup.
    static recognition_http::Connection connection;
    if (!fetch(url.c_str(), recognition_http::Body{body.data(), body.size(), type.c_str(), false}, 64 * 1024,
               response, &connection)) {
        service_delay = response.retry_after_us;
        return Lookup::Failed;
    }
    Json result(cJSON_ParseWithLengthOpts(response.bytes.get(), response.size + 1, nullptr, true));
    if (!result) return Lookup::Failed;
    switch (acrcloud::parse(result.get(), track)) {
    case acrcloud::Result::Matched: return Lookup::Matched;
    case acrcloud::Result::NoMatch: return Lookup::NoMatch;
    case acrcloud::Result::Limited:
        ESP_LOGW(kTag, "Recognition quota or rate limit reached");
        service_delay = 60000000;
        return Lookup::Failed;
    case acrcloud::Result::Failed: break;
    }
    const cJSON *status = recognition_metadata::field(result.get(), "status");
    const cJSON *code = recognition_metadata::field(status, "code");
    ESP_LOGW(kTag, "Recognition service error code=%d", cJSON_IsNumber(code) ? code->valueint : -1);
    return Lookup::Failed;
}

struct Image {
    std::shared_ptr<uint8_t> pixels;
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
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)), HeapFree{});
    if (!image.pixels) return {};
    io.outbuf = image.pixels.get(); io.out_size = bytes;
    if (jpeg_dec_process(decoder, &io) != JPEG_ERR_OK) return {};
    return image;
}

// One completed album and one pending request. A slow catalogue request
// runs independently of recognition and never advances the displayed song.
struct CachedAlbum {
    album_cache::Album metadata;
    Image image;
    char seed_title[513]{};
};
std::mutex album_cache_mutex;
std::shared_ptr<CachedAlbum> cached_album, pending_album;
uint64_t album_request_generation = 0;

void prefetch_album(uint64_t id, const RecognitionTrack &track, const Image &image)
{
    if (!id) return;
    void *memory = heap_caps_malloc(sizeof(CachedAlbum), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) return;
    auto item = std::shared_ptr<CachedAlbum>(new (memory) CachedAlbum{}, [](CachedAlbum *value) {
        value->~CachedAlbum(); heap_caps_free(value);
    });
    item->metadata.context = {id, esp_timer_get_time()};
    std::memcpy(item->metadata.name, track.album, sizeof(track.album));
    std::memcpy(item->metadata.artwork_url, track.artwork_url, sizeof(track.artwork_url));
    std::memcpy(item->seed_title, track.title, sizeof(track.title));
    item->image = image;
    std::lock_guard<std::mutex> lock(album_cache_mutex);
    pending_album = std::move(item);
    ++album_request_generation;
}

bool reuse_album(RecognitionTrack &verified, const album_resolver::Context &context, Image &image)
{
    std::shared_ptr<CachedAlbum> item;
    {
        std::lock_guard<std::mutex> lock(album_cache_mutex);
        item = cached_album;
    }
    if (!context.fresh(esp_timer_get_time()) || !item ||
        !item->metadata.apply(verified, context.collection_id, esp_timer_get_time())) return false;
    image = item->image;
    ESP_LOGI(kTag, "Album cache hit collection=%llu title=\"%s\" artwork=%d",
        static_cast<unsigned long long>(context.collection_id), verified.title, bool(image.pixels));
    return true;
}

void album_cache_task(void *)
{
    while (true) {
        std::shared_ptr<CachedAlbum> item;
        uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> lock(album_cache_mutex);
            item = std::move(pending_album);
            generation = album_request_generation;
        }
        if (!item) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        Response response;
        const auto id = item->metadata.context.collection_id;
        const auto url = album_sequence::Sequence::url(id);
        bool loaded = false;
        if (fetch(url.c_str(), nullptr, 256 * 1024, response)) {
            Json root(cJSON_ParseWithLengthOpts(response.bytes.get(), response.size + 1, nullptr, true));
            loaded = root && item->metadata.sequence.load(root.get(), item->seed_title) >= 0;
        }
        ESP_LOGI(kTag, "Album cache %s collection=%llu tracks=%u", loaded ? "ready" : "unavailable",
            static_cast<unsigned long long>(id), item->metadata.sequence.count);
        if (loaded) {
            std::lock_guard<std::mutex> lock(album_cache_mutex);
            if (generation == album_request_generation) cached_album = std::move(item);
        }
    }
}

// Picks the release the listener expects; keeps the service's on failure.
// Returns the chosen catalogue album, or zero.
uint64_t resolve_album(RecognitionTrack &track, album_resolver::Context &context)
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
    if (!chosen) return 0;
    std::memcpy(track.album, choice.album, sizeof(track.album));
    if (choice.artwork_url[0] != '\0') std::memcpy(track.artwork_url, choice.artwork_url, sizeof(track.artwork_url));
    track.duration_s = choice.duration_s;
    context = {choice.collection_id, started};
    return choice.collection_id;
}

// The same recording, even when services word it differently
// ("Whole Lotta Love" and "Whole Lotta Love (1990 Remaster)").
bool same_song(const RecognitionTrack &a, const RecognitionTrack &b)
{
    if (a.title[0] == '\0' || b.title[0] == '\0') return false;
    if (album_resolver::normalise(a.title) != album_resolver::normalise(b.title)) return false;
    const std::string x = album_resolver::lead_artist(a.artist), y = album_resolver::lead_artist(b.artist);
    return x.empty() || y.empty() || x.find(y) != std::string::npos || y.find(x) != std::string::npos;
}

// The last 10 s of levels relative to the background, for tuning start/stop.
void log_levels(const char *event, const listening::Detector &detector)
{
    float levels[listening::Detector::kRecentBlocks];
    const unsigned count = detector.recent(levels);
    char text[4 * listening::Detector::kRecentBlocks + 1];
    size_t used = 0;
    for (unsigned i = 0; i < count && used + 5 < sizeof(text); ++i) {
        used += std::snprintf(text + used, sizeof(text) - used, "%s%d", i ? "," : "",
            int(std::lround(levels[i] - detector.background_db())));
    }
    text[used] = '\0';
    ESP_LOGI(kTag, "%s background=%.1fdB last_10s_vs_background=[%s]", event, detector.background_db(), text);
}

int64_t ms(uint32_t hops) { return int64_t(hops) * 1000 / listening::kHopsPerSecond; }

// Level detection owns a separate reader and never waits for HTTPS, catalogue
// resolution or artwork. A stop changes the epoch and clears both UIs immediately.
void presence_task(void *)
{
    constexpr size_t capacity = 4096;
    Heap<int16_t> buffer(static_cast<int16_t *>(heap_caps_malloc(capacity * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    ESP_ERROR_CHECK(buffer ? ESP_OK : ESP_ERR_NO_MEM);
    audio_presence::Tracker tracker;
    uint64_t position = 0;
    int64_t last_audio = esp_timer_get_time();
    bool missing = false;
    int64_t last_diagnostic = 0;
    const auto changed = [&](const audio_presence::Snapshot &state, listening::Event event) {
        {
            std::lock_guard<std::mutex> lock(session_mutex);
            sound_state = state;
            if (event == listening::Event::None) {
                sessions.invalidate_audio();
                clear_display_locked(RecognitionStatus::Waiting);
            } else if (sessions.set_sound(state.sound)) {
                clear_display_locked(state.sound ? RecognitionStatus::Capturing : RecognitionStatus::Waiting);
            }
        }
        if (event != listening::Event::None)
            log_levels(state.sound ? "Sound started" : "Silence", tracker.detector());
    };
    while (true) {
        const auto *audio = usb_controller_audio();
        size_t count = 0;
        if (audio) {
            const auto read = audio->read(position, buffer.get(), capacity);
            count = read.count;
            if (count) {
                tracker.set_patient(track_visible.load());
                tracker.feed(buffer.get(), count, read.position, read.segment, changed);
                position = read.position + count;
                last_audio = esp_timer_get_time(); missing = false;
            }
        }
        if (!missing && esp_timer_get_time() - last_audio >= 3000000) {
            tracker.reset(changed); missing = true;
            ESP_LOGW(kTag, "Audio presence unavailable: no new samples");
        }
        {
            std::lock_guard<std::mutex> lock(session_mutex);
            flush_clear_locked();
        }
        const int64_t diagnostic_now = esp_timer_get_time();
        if (diagnostic_now - last_diagnostic >= 5000000) {
            last_diagnostic = diagnostic_now;
            ESP_LOGI(kTag, "Audio presence sound=%d patient=%d level_db=%.1f background_db=%.1f position=%llu",
                tracker.snapshot().sound, track_visible.load(), tracker.detector().level_db(),
                tracker.detector().background_db(), static_cast<unsigned long long>(position));
            log_levels("Audio levels", tracker.detector());
            const auto quality = tracker.detector().quality();
            ESP_LOGI(kTag, "Audio quality dc_counts=%.2f ac_dbfs=%.1f rails_pct=%.3f zeros_pct=%.3f block_ms=200",
                quality.mean, quality.ac_db, quality.rail_percent, quality.zero_percent);
        }
        vTaskDelay(pdMS_TO_TICKS(count == capacity ? 1 : 20));
    }
}

void recognition_task(void *)
{
    const bool paid = kAcrcloud.valid();
    ESP_LOGI(kTag, "Recognition service=%s", paid ? "acrcloud" : "shazam");
    // ACRCloud uses PCM directly. No Shazam FFT, peak gate or fingerprint heap.
    Heap<fingerprint::Analyzer> analyzer;
    if (!paid) {
        void *memory = heap_caps_aligned_alloc(16, sizeof(fingerprint::Analyzer), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ESP_ERROR_CHECK(memory ? ESP_OK : ESP_ERR_NO_MEM);
        analyzer.reset(new (memory) fingerprint::Analyzer);
        ESP_ERROR_CHECK(fingerprint::self_test() < 1e-4 ? ESP_OK : ESP_ERR_INVALID_STATE);
    }
    constexpr uint32_t hz = listening::kHopsPerSecond;
    // Round up to the 8 ms audio hop: 313 hops = 2.504 s.
    constexpr uint32_t acr_check_hops = (5 * hz + 1) / 2;
    recognition_policy::Planner planner = paid
        ? recognition_policy::Planner({3, 5, 8}, acr_check_hops, acr_check_hops)
        : recognition_policy::Planner{4, 10};
    recognition_policy::Retries retries;
    recognition_policy::Budget budget(paid ? 24 : 3);
    Heap<int16_t> clip(static_cast<int16_t *>(heap_caps_malloc(
        recognition_policy::Planner::kMaxWindow * fingerprint::kHopSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    constexpr size_t capacity = 4096;
    Heap<int16_t> buffer(static_cast<int16_t *>(heap_caps_malloc(capacity * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    ESP_ERROR_CHECK(clip && buffer ? ESP_OK : ESP_ERR_NO_MEM);
    RecognitionTrack track{}, recent{};
    Image image, recent_image;
    album_resolver::Context album_context;
    uint64_t epoch = 0, position = 0, analysis_base = 0, transport_epoch = 0;
    uint32_t segment = UINT32_MAX, heard = 0, sound_hop = 0;
    unsigned lookups = 0;
    int64_t recent_at = 0;
    int64_t last_request_at = -2500000;
    recognition_delivery::StatusUpdate pending_display;
    const auto flush_display = [&] {
        auto picture = image.view();
        pending_display.flush(epoch, [&](RecognitionStatus status) {
            return publish(epoch, status, track.title[0] ? &track : nullptr, image.pixels ? &picture : nullptr);
        });
    };
    const auto show_now = [&](RecognitionStatus status) {
        pending_display.request(epoch, status);
        flush_display();
    };
    const auto clear = [&] {
        pending_display.cancel();
        if (track.title[0]) { recent = track; recent_image = std::move(image); recent_at = esp_timer_get_time(); }
        track = {}; image = {};
    };
    while (true) {
        const auto current = sound_session();
        const auto *audio = usb_controller_audio();
        size_t count = 0;
        bool caught_up = false;
        if (audio) {
            const auto read = audio->read(position, buffer.get(), capacity);
            caught_up = read.remaining == 0;
            if (read.segment != segment || read.skipped) {
                segment = read.segment; analysis_base = read.position; heard = 0;
                if (analyzer) analyzer->reset();
                planner.stopped(); clear(); epoch = 0;
            }
            count = read.count;
            if (analyzer) for (size_t i = 0; i < count; i += fingerprint::kHopSamples) analyzer->push(buffer.get() + i);
            heard += uint32_t(count / fingerprint::kHopSamples);
            position = read.position + count;
        }
        const uint32_t quiet_hop = current.audio.quiet_sample > analysis_base
            ? uint32_t((current.audio.quiet_sample - analysis_base) / fingerprint::kHopSamples) : 0;
        if (current.session.epoch != epoch) {
            epoch = current.session.epoch;
            if (!paid || transport_epoch != current.transport_epoch) planner.stopped();
            transport_epoch = current.transport_epoch;
            clear();
            if (current.audio.sound && current.audio.segment == segment) {
                sound_hop = current.audio.start_sample > analysis_base
                    ? uint32_t((current.audio.start_sample - analysis_base) / fingerprint::kHopSamples) : 0;
                if (paid) planner.resume_sound(sound_hop, quiet_hop);
                else planner.sound_started(sound_hop);
            }
            show_now(current.audio.sound ? RecognitionStatus::Capturing : RecognitionStatus::Waiting);
        }
        if (paid && !current.audio.sound) planner.observe_quiet(heard, quiet_hop);
        // Retry a busy display using still-owned track/artwork. A session
        // change above cancels/replaces this before any retry can publish.
        flush_display();
        const int64_t now = esp_timer_get_time();
        recognition_policy::Window window;
        // HTTPS/catalogue work may leave seconds of queued audio. Drain it
        // before choosing the next window; otherwise each slow lookup can
        // trigger another lookup on old samples before reaching current audio.
        if (recognition_policy::request_allowed(paid, current.session, planner.acquiring(), caught_up)
            && std::time(nullptr) >= 1704067200 && retries.due(now)
            && (!paid || now - last_request_at >= 2500000)
            && budget.available(now) && planner.due(heard, window)) {
            planner.attempted(window.last);
            const unsigned id = ++lookups;
            fingerprint::Signature signature;
            const bool signed_ok = !paid && analyzer->signature(window.first, window.last, signature);
            const size_t wanted = size_t(window.last - window.first) * fingerprint::kHopSamples;
            bool pcm_ok = false;
            if (paid && audio) {
                const auto read = audio->read(analysis_base + uint64_t(window.first) * fingerprint::kHopSamples, clip.get(), wanted);
                pcm_ok = read.count == wanted && !read.skipped && read.segment == segment;
            }
            ESP_LOGI(kTag, "Lookup id=%u audio_ms=%lld since_sound_ms=%lld source=%s valid=%d level_sound=%d first=%llu last=%llu", id,
                static_cast<long long>(ms(window.last - window.first)),
                static_cast<long long>(ms(window.last - sound_hop)), paid ? "pcm" : "signature", paid ? pcm_ok : signed_ok,
                current.audio.sound,
                static_cast<unsigned long long>(analysis_base + uint64_t(window.first) * fingerprint::kHopSamples),
                static_cast<unsigned long long>(analysis_base + uint64_t(window.last) * fingerprint::kHopSamples));
            Lookup outcome = Lookup::NoMatch;
            int64_t delay = 0;
            RecognitionTrack next{};
            if (recognition_policy::lookup_ready(paid, pcm_ok, signed_ok, signature.peaks())) {
                budget.spend(now);
                last_request_at = now;
                if (current.session.ready && !track.title[0]) show_now(RecognitionStatus::Identifying);
                const int64_t started = esp_timer_get_time();
                outcome = paid ? identify(clip.get(), wanted, next, delay) : lookup(signature, next, delay);
                ESP_LOGI(kTag, "Lookup id=%u result=%s title=\"%s\" artist=\"%s\" duration_ms=%lld", id,
                    outcome == Lookup::Matched ? "matched" : outcome == Lookup::NoMatch ? "no_match" : "failed",
                    next.title, next.artist, static_cast<long long>((esp_timer_get_time() - started) / 1000));
            }
            if (paid && outcome == Lookup::Matched) {
                const auto region = recognition_evidence::coverage(next,
                    analysis_base + uint64_t(window.first) * fingerprint::kHopSamples,
                    analysis_base + uint64_t(window.last) * fingerprint::kHopSamples);
                ESP_LOGI(kTag, "Match evidence id=%u valid=%d sample_begin_ms=%.0f sample_end_ms=%.0f score=%.0f first=%llu last=%llu clip_last=%llu",
                    id, region.valid, next.sample_begin_ms, next.sample_end_ms, next.match_score,
                    static_cast<unsigned long long>(region.first), static_cast<unsigned long long>(region.last),
                    static_cast<unsigned long long>(analysis_base + uint64_t(window.last) * fingerprint::kHopSamples));
            }
            // Account for the request before presentation/session checks. A
            // quiet/disconnect transition must not discard service backoff.
            if (outcome == Lookup::Failed) {
                retries.failed(esp_timer_get_time(), delay);
                ESP_LOGI(kTag, "Recognition unavailable; retry_ms=%lld", static_cast<long long>(retries.remaining_us(esp_timer_get_time()) / 1000));
            } else {
                retries.succeeded();
            }
            const auto completed = sound_session();
            // USB advances the ring generation before the presence task has
            // necessarily observed a discontinuity. Check the stream itself
            // as well as the presentation session before accepting a result.
            const uint32_t current_segment = audio ? audio->read(position, nullptr, 0).segment : UINT32_MAX;
            const uint64_t clip_first = analysis_base + uint64_t(window.first) * fingerprint::kHopSamples;
            const auto region = recognition_evidence::coverage(next, clip_first,
                analysis_base + uint64_t(window.last) * fingerprint::kHopSamples);
            const recognition_state::Context context{paid, completed.session.ready,
                epoch, completed.session.epoch, transport_epoch, completed.transport_epoch,
                segment, current_segment, completed.audio.segment, clip_first, completed.audio.start_sample};
            const auto reason = recognition_state::evaluate(outcome, region, context);
            if (reason != recognition_state::Reason::Current) {
                ESP_LOGI(kTag, "Lookup not displayed id=%u reason=%s transport_ready=%d level_sound=%d",
                    id, recognition_state::name(reason), completed.session.transport_ready, completed.audio.sound);
                continue;
            }
            if (completed.session.epoch != epoch) {
                epoch = completed.session.epoch;
                ESP_LOGI(kTag, "Lookup retained id=%u: verified resumed audio", id);
            }
            if (outcome == Lookup::Matched) {
                if (same_song(track, next)) {
                    track.offset_s = next.offset_s; track.matched_at_us = esp_timer_get_time();
                } else if (!track.title[0] && same_song(recent, next) && now - recent_at < 300000000) {
                    track = recent; track.offset_s = next.offset_s; track.matched_at_us = esp_timer_get_time();
                    image = std::move(recent_image); recent = {};
                } else if (Image cached_image; reuse_album(next, album_context, cached_image)) {
                    clear(); track = next; track.matched_at_us = esp_timer_get_time();
                    image = std::move(cached_image);
                } else {
                    clear(); track = next; track.matched_at_us = esp_timer_get_time();
                    show_now(RecognitionStatus::Matched);
                    const uint64_t collection = resolve_album(next, album_context);
                    if (session().epoch != epoch) continue;
                    std::memcpy(track.album, next.album, sizeof(track.album));
                    std::memcpy(track.artwork_url, next.artwork_url, sizeof(track.artwork_url));
                    track.duration_s = next.duration_s;
                    show_now(RecognitionStatus::Matched);
                    if (track.artwork_url[0]) {
                        image = download_artwork(track.artwork_url);
                        if (session().epoch != epoch) continue;
                    }
                    prefetch_album(collection, track, image);
                }
                planner.matched(window, heard, track.offset_s, track.duration_s);
                show_now(RecognitionStatus::Matched);
            } else if (outcome == Lookup::NoMatch) {
                // A single failed lookup is not proof of a stopped or changed song.
                // The independent audio task clears confirmed silence immediately.
                show_now(track.title[0] ? RecognitionStatus::NoMatch : RecognitionStatus::Capturing);
            } else {
                show_now(RecognitionStatus::Unavailable);
            }
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(count == capacity ? 1 : 20));
    }
}
}

void recognition_start(RecognitionCallback callback)
{
    // Catalogue responses parse into hundreds of small nodes; keep them out of
    // the internal RAM that TLS and DMA depend on.
    static cJSON_Hooks hooks{
        [](size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); },
        heap_caps_free};
    cJSON_InitHooks(&hooks);
    {
        std::lock_guard<std::mutex> lock(session_mutex);
        show = callback;
    }
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(presence_task, "audio_presence", 8192,
        nullptr, 5, nullptr, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(album_cache_task, "album_cache", 16384,
        nullptr, 3, nullptr, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(recognition_task, "recognition", 24576,
        nullptr, 4, nullptr, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

void recognition_set_network(bool available, const char *address)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (!sessions.set_network(available, address != nullptr ? address : "")) return;
    clear_display_locked(RecognitionStatus::Waiting);
}

void recognition_set_controller(bool available)
{
    std::lock_guard<std::mutex> lock(session_mutex);
    if (!sessions.set_controller(available)) return;
    clear_display_locked(RecognitionStatus::Waiting);
}
