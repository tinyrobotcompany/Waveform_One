#pragma once

#include <cstring>
#include <string_view>
#include "cJSON.h"
#include "recognition.h"

namespace recognition_metadata {

inline bool artwork_url(std::string_view url)
{
    constexpr std::string_view prefix = "https://";
    if (url.size() > 2048 || url.substr(0, prefix.size()) != prefix) return false;
    url.remove_prefix(prefix.size());
    const size_t slash = url.find('/');
    if (slash == std::string_view::npos) return false;
    const auto host = url.substr(0, slash);
    constexpr std::string_view suffix = ".mzstatic.com";
    if (host.size() <= suffix.size() || host.substr(host.size() - suffix.size()) != suffix) return false;
    for (char c : host) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.')) return false;
    for (unsigned char c : url) if (c <= 32 || c >= 127 || c == '\\') return false;
    return true;
}

inline const cJSON *field(const cJSON *object, const char *name)
{
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

// Reject malformed UTF-8, display-control characters and overlong metadata.
template<size_t N> bool text(const cJSON *value, char (&output)[N])
{
    output[0] = '\0';
    if (!cJSON_IsString(value) || value->valuestring == nullptr) return false;
    const auto *bytes = reinterpret_cast<const unsigned char *>(value->valuestring);
    const size_t length = std::strlen(value->valuestring);
    if (length >= N) return false;
    for (size_t i = 0; i < length;) {
        const unsigned char first = bytes[i++];
        uint32_t code = first;
        unsigned continuation = 0;
        if (first >= 0xc2 && first <= 0xdf) { continuation = 1; code &= 0x1f; }
        else if (first >= 0xe0 && first <= 0xef) { continuation = 2; code &= 0x0f; }
        else if (first >= 0xf0 && first <= 0xf4) { continuation = 3; code &= 0x07; }
        else if (first >= 128) return false;
        if (i + continuation > length) return false;
        for (unsigned j = 0; j < continuation; ++j) {
            if ((bytes[i] & 0xc0) != 0x80) return false;
            code = (code << 6) | (bytes[i++] & 0x3f);
        }
        if ((continuation == 1 && code < 128) || (continuation == 2 && code < 2048)
            || (continuation == 3 && code < 65536) || code > 0x10ffff
            || (code >= 0xd800 && code <= 0xdfff) || code < 32 || (code >= 127 && code <= 159)
            || (code >= 0x202a && code <= 0x202e) || (code >= 0x2066 && code <= 0x2069)) return false;
    }
    std::memcpy(output, bytes, length + 1);
    return true;
}

inline bool parse(const cJSON *root, RecognitionTrack &output)
{
    output = {};
    const auto *track = field(root, "track");
    if (!cJSON_IsObject(track) || !text(field(track, "title"), output.title)) return false;
    bool visible = false;
    for (unsigned char c : std::string_view(output.title)) if (c > 32) visible = true;
    if (!visible) { output = {}; return false; }
    text(field(track, "subtitle"), output.artist);
    const cJSON *section = nullptr;
    cJSON_ArrayForEach(section, field(track, "sections")) {
        const cJSON *item = nullptr;
        cJSON_ArrayForEach(item, field(section, "metadata")) {
            const auto *label = field(item, "title");
            if (cJSON_IsString(label) && (std::strcmp(label->valuestring, "Album") == 0
                || std::strcmp(label->valuestring, "album") == 0)) text(field(item, "text"), output.album);
        }
    }
    char url[2049]{};
    if (text(field(field(track, "images"), "coverart"), url) && artwork_url(url)) {
        std::memcpy(output.artwork_url, url, std::strlen(url) + 1);
    }
    return true;
}
} // namespace recognition_metadata
