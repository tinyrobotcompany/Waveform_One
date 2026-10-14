#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include "cJSON.h"
#include "recognition.h"
#include "recognition_metadata.h"

// Audio identifies a recording, not a release: the same recording appears on
// the original album, singles, compilations and reissues, and the recognition
// service picks one of them. This chooses the release a listener expects:
// the album already playing if it contains the song; else the catalogue's
// most relevant regular album, avoiding singles, EPs, compilations,
// soundtracks and other versions. Recognition services often answer with a
// compilation, so their album is used only when nothing better is listed.
// Catalogue release dates belong to the song, not the album (a compilation
// carries the song's original date), so they cannot tell albums apart.
namespace album_resolver {

inline constexpr char kCountry[] = "GB";
inline constexpr int64_t kContextLifetimeUs = 20LL * 60 * 1000000;

// The album of the last song identified. A record usually plays one album.
struct Context {
    uint64_t collection_id = 0;
    int64_t at_us = 0;
    bool fresh(int64_t now) const { return collection_id != 0 && now - at_us < kContextLifetimeUs; }
};

struct Choice {
    uint64_t collection_id = 0;
    double duration_s = 0;
    char album[513]{};
    char artwork_url[2049]{};
};

// Lower-case ASCII words, without bracketed or dashed qualifiers such as
// "(feat. X)", "[Live]" or " - Remastered 2011". Other UTF-8 is kept as is.
inline std::string normalise(std::string_view text)
{
    std::string output;
    int depth = 0;
    bool space = true;
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '(' || c == '[') { ++depth; continue; }
        if ((c == ')' || c == ']') && depth > 0) { --depth; continue; }
        if (depth > 0) continue;
        if (text.substr(i, 3) == " - ") break;
        if (c >= 'A' && c <= 'Z') { output += char(c - 'A' + 'a'); space = false; }
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80) { output += char(c); space = false; }
        else if (c == '\'') continue;  // "Don't" and "Dont" agree
        else if (!space) { output += ' '; space = true; }
    }
    while (!output.empty() && output.back() == ' ') output.pop_back();
    return output;
}

// The lead artist: "Beyonce feat. JAY-Z" and "Simon & Garfunkel" credit
// differently between services, so compare the first name only.
inline std::string lead_artist(std::string_view artist)
{
    std::string text = " " + normalise(artist) + " ";
    for (const char *joiner : {" feat ", " featuring ", " ft ", " with ", " x ", " vs "}) {
        const size_t at = text.find(joiner);
        if (at != std::string::npos) text.resize(at + 1);
    }
    while (!text.empty() && text.front() == ' ') text.erase(text.begin());
    while (!text.empty() && text.back() == ' ') text.pop_back();
    return text;
}

inline bool has_word(const std::string &words, std::string_view word)
{
    const std::string padded = " " + words + " ";
    return padded.find(" " + std::string(word) + " ") != std::string::npos;
}

// Lower-case words of text, keeping bracketed qualifiers.
inline std::string words(std::string_view text)
{
    std::string output;
    for (unsigned char c : text) {
        if (c >= 'A' && c <= 'Z') output += char(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80) output += char(c);
        else if (!output.empty() && output.back() != ' ') output += ' ';
    }
    return output;
}

// Penalty for a release that is not the regular album the song comes from.
// 50 or more means it is not a regular album at all.
inline int penalty(const char *collection, const char *collection_artist, const std::string &artist,
                   const char *version, const char *heard)
{
    std::string name = collection;
    int score = 0;
    if (name.size() > 9 && name.compare(name.size() - 9, 9, " - Single") == 0) score += 100;
    if (name.size() > 5 && name.compare(name.size() - 5, 5, " - EP") == 0) score += 60;
    // "Various Artists", or a soundtrack credited to the film.
    if (collection_artist != nullptr && collection_artist[0] != '\0') {
        const std::string credited = lead_artist(collection_artist);
        if (credited.find(artist) == std::string::npos && artist.find(credited) == std::string::npos) score += 80;
    }
    const std::string album = words(name);
    for (const char *word : {"hits", "greatest", "best", "essential", "essentials", "anthology", "collection",
                             "compilation", "classic", "classics", "definitive", "ultimate", "legends", "decade",
                             "years", "retrospective", "karaoke", "tribute", "playlist", "singles", "gold", "platinum"}) {
        if (has_word(album, word)) { score += 50; break; }
    }
    // A different recording of the song than the one heard.
    const std::string song = words(version), original = words(heard);
    for (const char *word : {"live", "remix", "mix", "demo", "instrumental", "karaoke", "acoustic", "session"}) {
        if (has_word(song, word) && !has_word(original, word)) { score += 40; break; }
    }
    if (has_word(song, "version") && !has_word(original, "version")) score += 20;
    if (has_word(album, "live") && !has_word(original, "live")) score += 40;
    // "1962 - 1966", "1967-1970": a career span is a compilation.
    for (size_t i = 0; i + 9 <= name.size(); ++i) {
        const auto year = [&](size_t at) {
            return at + 4 <= name.size() && (name.compare(at, 2, "19") == 0 || name.compare(at, 2, "20") == 0)
                && std::isdigit(static_cast<unsigned char>(name[at + 2])) && std::isdigit(static_cast<unsigned char>(name[at + 3]));
        };
        if (!year(i)) continue;
        size_t at = i + 4;
        while (at < name.size() && (name[at] == ' ' || name[at] == '-' || name[at] == '/')) ++at;
        if (at > i + 4 && year(at)) { score += 50; break; }
    }
    for (const char *word : {"deluxe", "remastered", "remaster", "expanded", "anniversary", "edition"}) {
        if (has_word(album, word)) { score += 5; break; }
    }
    return score;
}

inline std::string search_url(const RecognitionTrack &track)
{
    const std::string term = lead_artist(track.artist) + " " + normalise(track.title);
    std::string url = "https://itunes.apple.com/search?media=music&entity=song&limit=25&country=";
    url += kCountry;
    url += "&term=";
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : term) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) url += char(c);
        else if (c == ' ') url += '+';
        else { url += '%'; url += hex[c >> 4]; url += hex[c & 15]; }
    }
    return url;
}

// Picks a release from an iTunes Search response for this recording.
// Returns false when no result is clearly the same song by the same artist.
inline bool choose(const cJSON *root, const RecognitionTrack &track, const Context &context,
                   int64_t now, Choice &output)
{
    using recognition_metadata::field;
    output = {};
    const std::string title = normalise(track.title);
    const std::string artist = lead_artist(track.artist);
    if (title.empty() || artist.empty()) return false;
    const std::string service_album = normalise(track.album);
    const cJSON *best = nullptr, *service = nullptr;
    int best_penalty = 0, service_penalty = 0;
    const cJSON *result = nullptr;
    // Results arrive most relevant first; ties keep the earlier one.
    cJSON_ArrayForEach(result, field(root, "results")) {
        const cJSON *kind = field(result, "kind");
        const cJSON *name = field(result, "trackName");
        const cJSON *by = field(result, "artistName");
        const cJSON *collection = field(result, "collectionName");
        const cJSON *id = field(result, "collectionId");
        if (!cJSON_IsString(kind) || std::strcmp(kind->valuestring, "song") != 0
            || !cJSON_IsString(name) || !cJSON_IsString(by) || !cJSON_IsString(collection)
            || !cJSON_IsNumber(id) || id->valuedouble <= 0) continue;
        if (normalise(name->valuestring) != title) continue;
        const std::string credited = lead_artist(by->valuestring);
        if (credited.find(artist) == std::string::npos && artist.find(credited) == std::string::npos) continue;
        // The album already playing wins outright.
        if (context.fresh(now) && uint64_t(id->valuedouble) == context.collection_id) { best = result; service = nullptr; break; }
        const cJSON *collection_artist = field(result, "collectionArtistName");
        const int score = penalty(collection->valuestring,
            cJSON_IsString(collection_artist) ? collection_artist->valuestring : nullptr, artist,
            name->valuestring, track.title);
        if (best == nullptr || score < best_penalty) { best = result; best_penalty = score; }
        if (!service_album.empty() && normalise(collection->valuestring) == service_album
            && (service == nullptr || score < service_penalty)) { service = result; service_penalty = score; }
    }
    // The service's album stands only as good as the best listed one.
    if (service != nullptr && service_penalty <= best_penalty) best = service;
    // Nothing better than a compilation: keep whatever the service said.
    if (best == nullptr || (best != service && best_penalty >= 50 && !service_album.empty())) return false;
    // Keep the service's own wording for its album, e.g. without "(Deluxe Edition)".
    if (best == service) std::memcpy(output.album, track.album, sizeof(output.album));
    else if (!recognition_metadata::text(field(best, "collectionName"), output.album)) { output = {}; return false; }
    output.collection_id = uint64_t(field(best, "collectionId")->valuedouble);
    const cJSON *length = field(best, "trackTimeMillis");
    if (cJSON_IsNumber(length) && length->valuedouble > 0) output.duration_s = length->valuedouble / 1000;
    char artwork[2049]{};
    if (recognition_metadata::text(field(best, "artworkUrl100"), artwork)) {
        std::string url = artwork;
        const size_t size = url.find("100x100bb");
        if (size != std::string::npos) url.replace(size, 9, "400x400bb");
        if (url.size() < sizeof(output.artwork_url) && recognition_metadata::artwork_url(url)) {
            std::memcpy(output.artwork_url, url.c_str(), url.size() + 1);
        }
    }
    return true;
}

} // namespace album_resolver
