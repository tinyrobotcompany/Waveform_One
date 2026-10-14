#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "album_resolver.h"
#include "cJSON.h"
#include "recognition_metadata.h"

// A record plays its tracks in order. Once one song on it is identified, the
// album's track list and lengths say what plays next and when, so the next
// title can be shown without asking the recognition service. The listener
// can change the music at any moment, so the prediction is only trusted while
// the audio keeps agreeing with it; anything else returns to live listening.
// Times are 128-sample hops of 16 kHz audio (125 per second).
namespace album_sequence {

inline constexpr unsigned kMaxTracks = 64;

struct Track {
    char title[513]{};
    char artist[513]{};
    double duration_s = 0;
};

// Allocate in PSRAM: about 66 KB.
struct Sequence {
    unsigned count = 0;
    Track tracks[kMaxTracks];

    // The catalogue lookup URL for an album's songs.
    static std::string url(uint64_t collection_id)
    {
        char text[160];
        std::snprintf(text, sizeof(text),
            "https://itunes.apple.com/lookup?id=%llu&entity=song&limit=200&country=%s",
            static_cast<unsigned long long>(collection_id), album_resolver::kCountry);
        return text;
    }

    // Reads an iTunes lookup response in disc and track order. Returns the
    // index of the track titled like `title`, or -1 if the album is unusable.
    int load(const cJSON *root, const char *title)
    {
        using recognition_metadata::field;
        count = 0;
        struct Entry { int disc, number; const cJSON *song; };
        Entry entries[kMaxTracks];
        unsigned found = 0;
        const cJSON *result = nullptr;
        cJSON_ArrayForEach(result, field(root, "results")) {
            const cJSON *kind = field(result, "kind");
            const cJSON *number = field(result, "trackNumber");
            const cJSON *disc = field(result, "discNumber");
            const cJSON *length = field(result, "trackTimeMillis");
            if (!cJSON_IsString(kind) || std::strcmp(kind->valuestring, "song") != 0
                || !cJSON_IsNumber(number) || !cJSON_IsNumber(length) || length->valuedouble <= 0) continue;
            if (found == kMaxTracks) return -1;
            entries[found++] = {cJSON_IsNumber(disc) ? disc->valueint : 1, number->valueint, result};
        }
        std::sort(entries, entries + found, [](const Entry &a, const Entry &b) {
            return a.disc != b.disc ? a.disc < b.disc : a.number < b.number;
        });
        const std::string wanted = album_resolver::normalise(title);
        int index = -1;
        for (unsigned i = 0; i < found; ++i) {
            Track &track = tracks[count];
            track = {};
            if (!recognition_metadata::text(field(entries[i].song, "trackName"), track.title)) continue;
            recognition_metadata::text(field(entries[i].song, "artistName"), track.artist);
            track.duration_s = field(entries[i].song, "trackTimeMillis")->valuedouble / 1000;
            if (index < 0 && album_resolver::normalise(track.title) == wanted) index = int(count);
            ++count;
        }
        if (index < 0) count = 0;
        return index;
    }
};

enum class Action {
    None,
    Advance,  // the next track has started: show it now
    Verify,   // ask the service what is playing (window from verify_window)
    Lost,     // the audio no longer fits the album: clear and listen live
};

// Follows the album through the audio's gaps and silences.
class Follower {
public:
    static constexpr uint32_t kHz = 125;
    static constexpr uint32_t kBoundaryHops = 6 * kHz;   // gap must be this close to a track end
    static constexpr uint32_t kGapHoldHops = 10 * kHz;   // longest silence between tracks
    static constexpr uint32_t kVerifyAfterHops = 6 * kHz;
    static constexpr uint32_t kRetryVerifyHops = 10 * kHz;

    bool active() const { return sequence_ != nullptr; }
    unsigned index() const { return index_; }
    const Track &track() const { return sequence_->tracks[index_]; }

    // Follow `sequence` from track `index`, which started at hop `start`.
    void start(const Sequence &sequence, unsigned index, uint32_t start)
    {
        *this = Follower{};
        sequence_ = &sequence; index_ = index; start_ = start;
    }
    void stop() { *this = Follower{}; }

    // A verified match of the shown track resets its timing.
    void confirmed(uint32_t start)
    {
        start_ = start; verify_at_ = 0; misses_ = 0; trusted_ = true;
    }

    // A check heard nothing usable. Two in a row mean the album is gone.
    Action unconfirmed(uint32_t heard)
    {
        if (++misses_ >= 2) { stop(); return Action::Lost; }
        verify_at_ = heard + kRetryVerifyHops;
        return Action::None;
    }

    // A short dip (shorter than silence) between hops start and end.
    Action gap(uint32_t start, uint32_t end)
    {
        if (!active() || holding_) return Action::None;
        // Quiet moments inside a track are music, not a change: only a gap
        // where the track should end counts. Real changes show up as silence
        // or as no gap at the expected end.
        if (near_boundary(start)) return advance(end, !trusted_);
        return Action::None;
    }

    // Silence began at hop `at`. Between tracks, wait for the next one.
    Action silence(uint32_t at)
    {
        if (!active()) return Action::None;
        if (!holding_ && near_boundary(at) && index_ + 1 < sequence_->count) {
            holding_ = true; hold_until_ = at + kGapHoldHops;
            return Action::None;
        }
        stop();
        return Action::Lost;
    }

    // Sound returned at hop `at` after silence.
    Action sound(uint32_t at)
    {
        if (!active()) return Action::None;
        if (!holding_) { stop(); return Action::Lost; }
        holding_ = false;
        return advance(at, !trusted_);
    }

    // Time passing. heard is the newest hop.
    Action tick(uint32_t heard)
    {
        if (!active()) return Action::None;
        if (holding_) {
            if (heard < hold_until_) return Action::None;
            stop();
            return Action::Lost;
        }
        if (heard > boundary() + kBoundaryHops) {
            // No gap where the track should end: gapless, a playlist or a
            // different version. Move on, but check.
            if (index_ + 1 >= sequence_->count) { stop(); return Action::Lost; }
            return advance(boundary(), true);
        }
        if (verify_at_ != 0 && heard >= verify_at_) { verify_at_ = 0; return Action::Verify; }
        return Action::None;
    }

    uint32_t track_start() const { return start_; }
    uint32_t boundary() const { return start_ + uint32_t(track().duration_s * kHz); }

private:
    bool near_boundary(uint32_t at) const
    {
        const uint32_t end = boundary();
        return (at > end ? at - end : end - at) <= kBoundaryHops;
    }

    Action advance(uint32_t at, bool verify)
    {
        if (index_ + 1 >= sequence_->count) { stop(); return Action::Lost; }
        ++index_; start_ = at; misses_ = 0;
        verify_at_ = verify ? at + kVerifyAfterHops : 0;
        return Action::Advance;
    }

    const Sequence *sequence_ = nullptr;
    unsigned index_ = 0, misses_ = 0;
    uint32_t start_ = 0, verify_at_ = 0, hold_until_ = 0;
    bool holding_ = false, trusted_ = false;
};

} // namespace album_sequence
