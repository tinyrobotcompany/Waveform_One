#pragma once

#include <mutex>
#include <cstdio>
#include <string>
#include "recognition_metadata.h"
#include "recognition.h"
#include "album_palette.h"

// Owns the worker's borrowed metadata. HTTP never takes a GUI/session lock.
class RemoteState {
public:
    void recognition(RecognitionStatus status, const RecognitionTrack *track) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (track == nullptr || std::strcmp(track_.artwork_url, track->artwork_url) != 0) palette_ = {};
        status_ = status;
        track_ = track == nullptr ? RecognitionTrack{} : *track;
    }
    void palette(const album_palette::Palette &value) {
        const std::lock_guard<std::mutex> lock(mutex_); palette_ = value;
    }
    void appearance(bool value) {
        const std::lock_guard<std::mutex> lock(mutex_); album_colours_ = value;
    }
    void brightness(int value) {
        const std::lock_guard<std::mutex> lock(mutex_);
        brightness_ = value;
    }
    void name(const char *value) {
        const std::lock_guard<std::mutex> lock(mutex_);
        name_ = value;
    }
    void style(const char *value) {
        const std::lock_guard<std::mutex> lock(mutex_);
        style_ = value;
    }
    // Empty means allocation failed; never publish a partial JSON object.
    std::string json() {
        const std::lock_guard<std::mutex> lock(mutex_);
        cJSON *root = cJSON_CreateObject();
        if (root == nullptr) return {};
        // Recognition clears the track itself when the music stops.
        const bool visible = track_.title[0] != '\0';
        const auto status = !visible && status_ == RecognitionStatus::Matched
            ? RecognitionStatus::Waiting : status_;
        bool ok = cJSON_AddStringToObject(root, "status", status_name(status)) != nullptr
            && cJSON_AddStringToObject(root, "name", name_.c_str()) != nullptr
            && cJSON_AddNumberToObject(root, "brightness", brightness_) != nullptr
            && cJSON_AddStringToObject(root, "style", style_.c_str()) != nullptr;
        ok = ok && cJSON_AddStringToObject(root, "appearance", album_colours_ ? "album" : "classic") != nullptr;
        const auto colours = visible && album_colours_ ? palette_ : album_palette::Palette{};
        cJSON *palette = cJSON_AddObjectToObject(root, "palette");
        ok = ok && palette != nullptr;
        if (palette != nullptr) {
            const uint32_t values[] = {colours.background, colours.panel, colours.accent, colours.search, colours.line};
            const char *keys[] = {"background", "panel", "accent", "search", "line"};
            for (unsigned i=0; i<5; ++i) {
                char hex[8]; std::snprintf(hex, sizeof(hex), "#%06x", static_cast<unsigned>(values[i]));
                ok = ok && cJSON_AddStringToObject(palette, keys[i], hex) != nullptr;
            }
        }
        cJSON *track = visible ? cJSON_AddObjectToObject(root, "track") : nullptr;
        if (visible) {
            ok = ok && track != nullptr;
            if (track != nullptr) {
                const char *url = recognition_metadata::artwork_url(track_.artwork_url)
                    ? track_.artwork_url : "";
                ok = ok && cJSON_AddStringToObject(track, "title", track_.title) != nullptr
                    && cJSON_AddStringToObject(track, "artist", track_.artist) != nullptr
                    && cJSON_AddStringToObject(track, "album", track_.album) != nullptr
                    && cJSON_AddStringToObject(track, "artwork", url) != nullptr;
            }
        } else {
            ok = ok && cJSON_AddNullToObject(root, "track") != nullptr;
        }
        char *encoded = ok ? cJSON_PrintUnformatted(root) : nullptr;
        std::string result = encoded == nullptr ? "" : encoded;
        cJSON_free(encoded);
        cJSON_Delete(root);
        return result;
    }
private:
    static const char *status_name(RecognitionStatus status) {
        switch (status) {
        case RecognitionStatus::Capturing: return "capturing";
        case RecognitionStatus::Identifying: return "identifying";
        case RecognitionStatus::Matched: return "matched";
        case RecognitionStatus::NoMatch: return "no_match";
        case RecognitionStatus::Unavailable: return "unavailable";
        case RecognitionStatus::Waiting: return "waiting";
        }
        return "waiting";
    }
    std::mutex mutex_;
    RecognitionStatus status_ = RecognitionStatus::Waiting;
    RecognitionTrack track_{};
    std::string name_ = "Simon", style_ = "";
    int brightness_ = 100;
    bool album_colours_ = true;
    album_palette::Palette palette_{};
};
