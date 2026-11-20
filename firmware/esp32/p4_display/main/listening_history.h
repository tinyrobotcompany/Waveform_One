#pragma once
#include "recognition.h"
#include "cJSON.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

// Optional session history only: no audio, flash writes, or external requests.
class ListeningHistory {
    struct Entry {
        unsigned id = 0;
        std::string title, artist, album;
        int64_t started = 0, last_match = 0;
        double observed = 0, duration = 0, offset = -1;
    };
    std::mutex mutex_;
    std::deque<Entry> entries_;
    Entry candidate_;
    unsigned next_id_ = 1;
    bool enabled_ = false;
    std::string session_;
public:
    void begin(std::string session) {
        const std::lock_guard<std::mutex> lock(mutex_);
        session_ = std::move(session); enabled_ = false; entries_.clear(); candidate_ = {};
    }
    void enable(bool value) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (enabled_ != value) candidate_ = {};
        enabled_ = value;
        if (!value) { entries_.clear(); candidate_ = {}; }
    }
    void clear() {
        const std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear(); candidate_ = {};
        // Never reuse IDs: exported/deleted entries must not collide with later ones.
    }
    void observe(const RecognitionTrack *track, int64_t epoch) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        if (!track || !track->title[0] || !track->artist[0] || track->matched_at_us <= 0) {
            candidate_ = {}; return;
        }
        if (track->matched_at_us == candidate_.last_match) return; // UI/artwork repeats are not new evidence.
        const double gap = double(track->matched_at_us - candidate_.last_match) / 1000000;
        const bool same = candidate_.title == track->title && candidate_.artist == track->artist;
        // Never infer play time across a network outage, missing evidence or rewind/restart.
        if (!same || gap <= 0 || gap > 15 ||
            (track->offset_s >= 0 && candidate_.offset >= 0 && track->offset_s + 5 < candidate_.offset)) {
            candidate_ = {};
            candidate_.title = std::string(track->title, strnlen(track->title, sizeof(track->title)));
            candidate_.artist = std::string(track->artist, strnlen(track->artist, sizeof(track->artist)));
            candidate_.album = std::string(track->album, strnlen(track->album, sizeof(track->album)));
            candidate_.started = epoch >= 1700000000 ? epoch : 0;
        } else candidate_.observed += gap;
        candidate_.offset = track->offset_s;
        candidate_.last_match = track->matched_at_us;
        candidate_.duration = std::isfinite(track->duration_s) && track->duration_s > 0 ? track->duration_s : 0;
        if (candidate_.observed < 20) return;
        if (!candidate_.id) {
            candidate_.id = next_id_++;
            if (entries_.size() == 50) entries_.pop_front();
            entries_.push_back(candidate_);
        } else if (!entries_.empty() && entries_.back().id == candidate_.id) entries_.back() = candidate_;
    }
    std::string json() {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto *root = cJSON_CreateObject();
        if (!root) return {};
        bool ok = cJSON_AddNumberToObject(root, "schema", 1) &&
            cJSON_AddStringToObject(root, "session", session_.c_str()) &&
            cJSON_AddBoolToObject(root, "enabled", enabled_);
        auto *items = cJSON_AddArrayToObject(root, "entries");
        ok = ok && items;
        for (auto it = entries_.rbegin(); ok && it != entries_.rend(); ++it) {
            auto *item = cJSON_CreateObject();
            if (!item) { ok = false; break; }
            ok = cJSON_AddNumberToObject(item, "id", it->id) &&
                cJSON_AddStringToObject(item, "title", it->title.c_str()) &&
                cJSON_AddStringToObject(item, "artist", it->artist.c_str()) &&
                cJSON_AddStringToObject(item, "album", it->album.c_str()) &&
                cJSON_AddNumberToObject(item, "started", double(it->started)) &&
                cJSON_AddNumberToObject(item, "observed_seconds", it->observed) &&
                cJSON_AddNumberToObject(item, "duration_seconds", it->duration);
            if (!ok || !cJSON_AddItemToArray(items, item)) { cJSON_Delete(item); ok = false; }
        }
        char *encoded = ok ? cJSON_PrintUnformatted(root) : nullptr;
        std::string result = encoded ? encoded : "";
        cJSON_free(encoded); cJSON_Delete(root);
        return result;
    }
};
