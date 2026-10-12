#include "recognition.h"
#include "recognition_metadata.h"
#include "recognition_http.h"
#include "recognition_policy.h"
#include "acrcloud.h"
#include "acrcloud_config.h"
#include "album_resolver.h"
#include "album_sequence.h"
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

// Loads the album's track list; returns the index of track, or -1.
int load_album(uint64_t collection_id, const RecognitionTrack &track, album_sequence::Sequence &album)
{
    const int64_t started = esp_timer_get_time();
    Response response;
    int index = -1;
    if (fetch(album_sequence::Sequence::url(collection_id).c_str(), nullptr, 256 * 1024, response)) {
        Json result(cJSON_ParseWithLengthOpts(response.bytes.get(), response.size + 1, nullptr, true));
        if (result) index = album.load(result.get(), track.title);
    }
    ESP_LOGI(kTag, "Album tracks=%u position=%d duration_ms=%lld", album.count, index + 1,
        static_cast<long long>((esp_timer_get_time() - started) / 1000));
    return index;
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

// Where the song began, from a match of window at offset_s; 0 if unknown.
uint32_t song_start(const recognition_policy::Window &window, double offset_s)
{
    if (offset_s < 0) return window.first;
    const double back = offset_s * listening::kHopsPerSecond;
    return back < window.first ? uint32_t(window.first - back) : 0;
}

void recognition_task(void *)
{
    void *memory = heap_caps_aligned_alloc(16, sizeof(fingerprint::Analyzer), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(memory != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    Heap<fingerprint::Analyzer> analyzer(new (memory) fingerprint::Analyzer);
    const double fft_error = fingerprint::self_test();
    ESP_LOGI(kTag, "Fingerprint FFT self-test relative_error=%.2e", fft_error);
    ESP_ERROR_CHECK(fft_error < 1e-4 ? ESP_OK : ESP_ERR_INVALID_STATE);
    memory = heap_caps_malloc(sizeof(album_sequence::Sequence), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(memory != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    Heap<album_sequence::Sequence> album(new (memory) album_sequence::Sequence);

    // A paid service allows quick retries; the free fallback allows three a minute.
    const bool paid = kAcrcloud.valid();
    ESP_LOGI(kTag, "Recognition service=%s", paid ? "acrcloud" : "shazam");
    listening::Detector detector;
    recognition_policy::Planner planner = paid ? recognition_policy::Planner{3, 5, 8} : recognition_policy::Planner{4, 10};
    recognition_policy::Retries retries;
    recognition_policy::Budget budget(paid ? 12 : 3);
    // Audio for a lookup: up to eight seconds of 16 kHz samples.
    Heap<int16_t> clip(static_cast<int16_t *>(heap_caps_malloc(
        recognition_policy::Planner::kMaxWindow * fingerprint::kHopSamples * sizeof(int16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    ESP_ERROR_CHECK(clip ? ESP_OK : ESP_ERR_NO_MEM);
    uint64_t analysis_base = 0;  // stream position of analysed hop zero
    album_resolver::Context album_context;
    album_sequence::Follower follower;
    RecognitionTrack track{};
    Image image;
    RecognitionStatus status = RecognitionStatus::Waiting;
    uint64_t epoch = 0, position = 0;
    uint32_t segment = UINT32_MAX, sound_hop = 0;
    int64_t level_logged = 0, analysis_us = 0;
    uint32_t analysed = 0;
    unsigned lookups = 0;
    bool verify_due = false;
    // The song cleared most recently, to restore it if it is simply heard again.
    RecognitionTrack recent{};
    Image recent_image;
    int recent_index = -1;
    int64_t recent_at = 0;
    // Consecutive sounds the service could not identify (air conditioning, talk).
    unsigned unidentified_streak = 0;
    int64_t unidentified_at = 0;
    constexpr size_t kReadSamples = fingerprint::kHopSamples * 32;
    Heap<int16_t> buffer(static_cast<int16_t *>(heap_caps_malloc(kReadSamples * sizeof(int16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    ESP_ERROR_CHECK(buffer ? ESP_OK : ESP_ERR_NO_MEM);
    int16_t *samples = buffer.get();

    const auto show_now = [&](RecognitionStatus next) {
        status = next;
        auto picture = image.view();
        publish(epoch, status, track.title[0] != '\0' ? &track : nullptr, image.pixels ? &picture : nullptr);
    };
    const auto clear = [&](const char *reason) {
        if (track.title[0] != '\0') {
            ESP_LOGI(kTag, "Display cleared reason=%s", reason);
            recent = track; recent_image = std::move(image);
            recent_index = follower.active() ? int(follower.index()) : -1;
            recent_at = esp_timer_get_time();
        }
        track = {}; image = {};
        follower.stop(); verify_due = false;
    };
    // Back to identifying whatever is playing now.
    const auto listen_live = [&](const char *reason, uint32_t from) {
        clear(reason);
        planner.stopped();
        if (detector.sound()) planner.sound_started(from);
        show_now(detector.sound() ? RecognitionStatus::Capturing : RecognitionStatus::Waiting);
    };
    // The album's next track has started: show it without a lookup.
    const auto show_next = [&](uint32_t heard) {
        const auto &next = follower.track();
        std::memcpy(track.title, next.title, sizeof(track.title));
        if (next.artist[0] != '\0') std::memcpy(track.artist, next.artist, sizeof(track.artist));
        track.duration_s = next.duration_s;
        track.offset_s = 0;
        track.matched_at_us = esp_timer_get_time();
        ESP_LOGI(kTag, "Album advanced position=%u title=\"%s\" since_start_ms=%lld",
            follower.index() + 1, track.title,
            static_cast<long long>(ms(heard - follower.track_start())));
        show_now(RecognitionStatus::Matched);
    };
    const auto follow = [&](album_sequence::Action action, uint32_t heard, const char *reason) {
        switch (action) {
        case album_sequence::Action::Advance: show_next(heard); break;
        case album_sequence::Action::Verify: verify_due = true; break;
        case album_sequence::Action::Lost:
            ESP_LOGI(kTag, "Album left reason=%s", reason);
            listen_live(reason, heard);
            break;
        case album_sequence::Action::None: break;
        }
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
            auto read = audio->read(position, samples, kReadSamples);
            if (read.segment != segment || read.skipped) {
                ESP_LOGI(kTag, "Audio %s; analysis restarted", segment == UINT32_MAX ? "started"
                    : read.skipped ? "fell behind" : "interrupted");
                segment = read.segment;
                analyzer->reset(); detector.discontinuity();
                analysis_base = read.position;
                const bool showing = track.title[0] != '\0';
                planner.stopped(); clear("audio_interrupted");
                if (showing) show_now(RecognitionStatus::Waiting);
            }
            const int64_t started = esp_timer_get_time();
            detector.set_patient(track.title[0] != '\0');
            hops = read.count / fingerprint::kHopSamples;
            for (uint32_t i = 0; i < hops; ++i) {
                const int16_t *hop = samples + i * fingerprint::kHopSamples;
                analyzer->push(hop);
                uint32_t start = 0;
                const auto event = detector.push(hop, fingerprint::kHopSamples, analyzer->hops() - 1, start);
                if (event == listening::Event::SoundStarted) {
                    sound_hop = start;
                    log_levels("Sound started", detector);
                    if (follower.active()) {
                        follow(follower.sound(start), start, "sound_after_silence");
                    } else {
                        // After repeated sound nobody could identify, spend lookups sparingly.
                        planner.set_cautious(unidentified_streak >= 2
                            && esp_timer_get_time() - unidentified_at < 180000000);
                        planner.sound_started(start);
                        if (current.ready) show_now(RecognitionStatus::Capturing);
                    }
                } else if (event == listening::Event::SilenceStarted) {
                    log_levels("Silence", detector);
                    const int following = follower.active() ? int(follower.index()) : -1;
                    const auto action = follower.silence(analyzer->hops() - detector.stop_blocks() * listening::kBlockHops);
                    if (action == album_sequence::Action::None && follower.active()) {
                        ESP_LOGI(kTag, "Album gap: holding for the next track");
                    } else {
                        planner.stopped(); clear("silence");
                        if (following >= 0) recent_index = following;
                        show_now(RecognitionStatus::Waiting);
                    }
                } else if (event == listening::Event::Gap) {
                    ESP_LOGI(kTag, "Gap ms=%lld", static_cast<long long>(ms(detector.gap_end() - start)));
                    follow(follower.gap(start, detector.gap_end()), detector.gap_end(), "gap");
                }
            }
            position = read.position + uint64_t(hops) * fingerprint::kHopSamples;
            analysis_us += esp_timer_get_time() - started; analysed += hops;
        }
        const uint32_t heard = analyzer->hops();
        follow(follower.tick(heard), heard, "track_end_without_gap");
        const int64_t now = esp_timer_get_time();
        if (now - level_logged >= 10000000) {
            const auto range = detector.take_range();
            ESP_LOGI(kTag, "Listening sound=%d level=%.1fdB range=%.1f..%.1fdB background=%.1fdB "
                "analysis_cpu=%.1f%% internal_free=%u largest=%u lowest=%u dma_free=%u",
                detector.sound(), detector.level_db(), range.low, range.high, detector.background_db(),
                analysed ? 100.0 * analysis_us / (ms(analysed) * 1000) : 0.0,
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));
            level_logged = now; analysis_us = 0; analysed = 0;
        }

        // 2. Look up audio when the planner, or a check on the album, asks.
        recognition_policy::Window window;
        bool due = false;
        if (current.ready && std::time(nullptr) >= 1704067200 && retries.due(now) && budget.available(now)) {
            if (follower.active()) {
                due = verify_due;
                const uint32_t first = std::max(follower.track_start(),
                    heard > recognition_policy::Planner::kMaxWindow ? heard - recognition_policy::Planner::kMaxWindow : 0);
                window = {first, heard};
            } else {
                due = planner.due(heard, window);
            }
        }
        if (due) {
            const bool checking = follower.active();
            verify_due = false;
            if (!checking) planner.attempted(window.last);
            const unsigned id = ++lookups;
            fingerprint::Signature signature;
            const bool signed_ok = analyzer->signature(window.first, window.last, signature);
            ESP_LOGI(kTag, "Lookup id=%u%s audio_ms=%lld since_sound_ms=%lld peaks=%u%s",
                id, checking ? " album_check" : "", static_cast<long long>(ms(window.last - window.first)),
                static_cast<long long>(ms(window.last - sound_hop)),
                static_cast<unsigned>(signature.peaks()), signed_ok ? "" : " signature_unavailable");
            Lookup outcome = Lookup::NoMatch;
            int64_t service_delay = 0;
            RecognitionTrack next{};
            if (signed_ok && signature.peaks() >= 5) {
                budget.spend(now);
                if (track.title[0] == '\0') show_now(RecognitionStatus::Identifying);
                const int64_t lookup_started = esp_timer_get_time();
                if (paid) {
                    // The same audio the planner chose, straight from the stream.
                    const size_t count = size_t(window.last - window.first) * fingerprint::kHopSamples;
                    const auto read = audio->read(analysis_base + uint64_t(window.first) * fingerprint::kHopSamples,
                        clip.get(), count);
                    outcome = read.count == count && !read.skipped && read.segment == segment
                        ? identify(clip.get(), count, next, service_delay) : Lookup::Failed;
                } else {
                    outcome = lookup(signature, next, service_delay);
                }
                ESP_LOGI(kTag, "Lookup id=%u result=%s title=\"%s\" artist=\"%s\" offset_s=%.1f duration_ms=%lld", id,
                    outcome == Lookup::Matched ? "matched" : outcome == Lookup::NoMatch ? "no_match" : "failed",
                    next.title, next.artist, next.offset_s,
                    static_cast<long long>((esp_timer_get_time() - lookup_started) / 1000));
            }
            if (session().epoch != epoch) continue;
            detector.set_unrecognised(!checking && outcome == Lookup::NoMatch && planner.unidentified());
            if (outcome == Lookup::Matched) unidentified_streak = 0;
            else if (!checking && outcome == Lookup::NoMatch && planner.unidentified()) {
                if (esp_timer_get_time() - unidentified_at > 180000000) unidentified_streak = 0;
                ++unidentified_streak; unidentified_at = esp_timer_get_time();
            }
            if (outcome == Lookup::Matched) {
                retries.succeeded();
                if (same_song(track, next)) {
                    // Still the shown song: keep its album, artwork and length.
                    track.offset_s = next.offset_s;
                    if (follower.active()) {
                        follower.confirmed(song_start(window, next.offset_s));
                        ESP_LOGI(kTag, "Album check confirmed position=%u", follower.index() + 1);
                    }
                } else if (track.title[0] == '\0' && same_song(recent, next)
                           && esp_timer_get_time() - recent_at < 300000000) {
                    // Heard again after a pause: bring back its album, cover and place.
                    track = recent; track.offset_s = next.offset_s;
                    track.matched_at_us = esp_timer_get_time();
                    image = std::move(recent_image);
                    planner.stopped(); planner.sound_started(sound_hop);
                    planner.attempted(window.last);
                    if (recent_index >= 0 && unsigned(recent_index) < album->count) {
                        follower.start(*album, unsigned(recent_index), song_start(window, next.offset_s));
                        follower.confirmed(song_start(window, next.offset_s));
                    }
                    recent = {}; recent_index = -1;
                    ESP_LOGI(kTag, "Restored recent song following_album=%d", follower.active());
                    show_now(RecognitionStatus::Matched);
                } else {
                    if (checking) ESP_LOGI(kTag, "Album left reason=different_song");
                    clear("new_song");
                    planner.stopped(); planner.sound_started(sound_hop);
                    planner.attempted(window.last);
                    // Title first; the album and cover follow a moment later.
                    track = next;
                    track.album[0] = '\0'; track.artwork_url[0] = '\0';
                    track.matched_at_us = esp_timer_get_time();
                    show_now(RecognitionStatus::Matched);
                    const uint64_t collection = resolve_album(next, album_context);
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
                    // Follow the album from here when its track list knows this song.
                    const int index = collection != 0 && track.offset_s >= 0 ? load_album(collection, track, *album) : -1;
                    if (session().epoch != epoch) continue;
                    if (index >= 0) {
                        // The first predicted track is checked once; later ones are trusted.
                        follower.start(*album, unsigned(index), song_start(window, track.offset_s));
                        if (track.duration_s <= 0) track.duration_s = album->tracks[index].duration_s;
                    }
                }
                planner.matched(window, analyzer->hops(), track.offset_s, track.duration_s);
                ESP_LOGI(kTag, "Recognition matched id=%u since_sound_ms=%lld following_album=%d next_song_in_ms=%lld",
                    id, static_cast<long long>(ms(window.last - sound_hop)), follower.active(),
                    static_cast<long long>(follower.active() ? (int64_t(follower.boundary()) - analyzer->hops()) * 8
                    : planner.expected_end() ? (int64_t(planner.expected_end()) - analyzer->hops()) * 8 : -1));
            } else if (outcome == Lookup::NoMatch) {
                retries.succeeded();
                if (checking) {
                    follow(follower.unconfirmed(analyzer->hops()), analyzer->hops(), "checks_heard_nothing");
                } else {
                    if (planner.missed()) clear("song_no_longer_heard");
                    // Keep "finding your song" up while the first tries continue.
                    show_now(track.title[0] != '\0' ? RecognitionStatus::Matched
                        : planner.active() && !planner.unidentified() ? RecognitionStatus::Capturing
                        : RecognitionStatus::NoMatch);
                }
            } else {
                retries.failed(esp_timer_get_time(), service_delay);
                // An album check retries once the service allows.
                if (checking) verify_due = true;
                ESP_LOGI(kTag, "Recognition unavailable; retry_ms=%lld",
                    static_cast<long long>(retries.remaining_us(esp_timer_get_time()) / 1000));
                show_now(RecognitionStatus::Unavailable);
            }
            continue;
        }
        // Caught up: wait for more audio. Otherwise yield and keep going.
        vTaskDelay(pdMS_TO_TICKS(hops == kReadSamples / fingerprint::kHopSamples ? 1 : 20));
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
