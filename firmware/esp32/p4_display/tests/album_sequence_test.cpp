#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include "album_sequence.h"
#include "album_cache.h"
#include "album_cache_retry.h"

namespace {
std::string song(int disc, int number, const char *title, long ms)
{
    return "{\"wrapperType\":\"track\",\"kind\":\"song\",\"discNumber\":" + std::to_string(disc)
        + ",\"trackNumber\":" + std::to_string(number) + ",\"trackName\":\"" + title
        + "\",\"artistName\":\"The Dave Brubeck Quartet\",\"trackTimeMillis\":" + std::to_string(ms) + "}";
}
} // namespace

int main()
{
    using namespace album_sequence;
    constexpr uint32_t s = Follower::kHz;
    assert(Sequence::url(1440857781) ==
           "https://itunes.apple.com/lookup?id=1440857781&entity=song&limit=200&country=GB");

    // Tracks arrive in any order; disc then track number decides.
    const std::string text = "{\"results\":[{\"wrapperType\":\"collection\",\"collectionName\":\"Time Out\"},"
        + song(2, 1, "Bonus Take", 100000) + "," + song(1, 3, "Take Five", 328832) + ","
        + song(1, 1, "Blue Rondo \\u00c0 la Turk", 409359) + "," + song(1, 2, "Strange Meadow Lark", 446314)
        + ",{\"kind\":\"music-video\",\"trackNumber\":4,\"trackTimeMillis\":5}]}";
    cJSON *root = cJSON_Parse(text.c_str());
    auto album = std::make_unique<Sequence>();
    assert(album->load(root, "Take Five (Remastered)") == 2);
    assert(album->count == 4);
    assert(std::strcmp(album->tracks[0].title, "Blue Rondo \xc3\x80 la Turk") == 0);
    assert(std::strcmp(album->tracks[3].title, "Bonus Take") == 0);
    assert(album->tracks[2].duration_s > 328 && album->tracks[2].duration_s < 329);
    assert(album->load(root, "Not On This Album") == -1 && album->count == 0);
    cJSON_Delete(root);

    // Box sets must not silently lose their cache at the old 64-track limit.
    std::string box = "{\"results\":[";
    for (unsigned i = 0; i < 200; ++i) {
        if (i) box += ",";
        const auto name = "Track " + std::to_string(i);
        box += song(1, i + 1, name.c_str(), 180000);
    }
    box += "]}";
    root = cJSON_Parse(box.c_str());
    assert(album->load(root, "Track 199") == 199 && album->count == 200);
    cJSON_Delete(root);
    box.insert(box.size() - 2, "," + song(1, 201, "Overflow", 180000));
    root = cJSON_Parse(box.c_str());
    assert(album->load(root, "Track 0") == -1 && album->count == 0);
    cJSON_Delete(root);

    // A cover already loaded does not affect the independent cache retries.
    album_cache::Retry retry;
    const auto first = retry.request();
    assert(retry.due(0));
    assert(!retry.complete(first, false, 100, 0));
    assert(!retry.due(30000099) && retry.due(30000100));
    assert(!retry.complete(first, false, 30000100, 120000000));
    assert(!retry.due(150000099) && retry.due(150000100));
    assert(retry.complete(first, true, 150000100, 0));
    assert(!retry.pending());
    const auto second = retry.request();
    assert(!retry.complete(first, true, 0, 0)); // stale success cannot replace new album
    assert(!retry.complete(first, false, 0, 999999999)); // nor delay it
    assert(retry.due(0));
    for (int i = 0; i < 3; ++i) retry.complete(second, false, i * 60000000LL, 0);
    assert(!retry.pending()); // broken catalogue cannot loop indefinitely

    // Verified matches can reuse an album in any order, never by time alone.
    auto cache = std::make_unique<album_cache::Album>();
    cache->context = {123, 100};
    std::strcpy(cache->name, "Time Out");
    cache->sequence.count = 2;
    std::strcpy(cache->sequence.tracks[0].title, "Take Five");
    std::strcpy(cache->sequence.tracks[1].title, "Strange Meadow Lark");
    for (unsigned i = 0; i < 2; ++i) {
        std::strcpy(cache->sequence.tracks[i].artist, "The Dave Brubeck Quartet");
        cache->sequence.tracks[i].duration_s = 300 + i;
    }
    RecognitionTrack match{};
    std::strcpy(match.title, "Strange Meadow Lark");
    std::strcpy(match.artist, "The Dave Brubeck Quartet");
    match.offset_s = 12;
    assert(cache->apply(match, 123, 200));
    assert(std::strcmp(match.album, "Time Out") == 0 && match.duration_s == 301 && match.offset_s == 12);
    assert(!cache->apply(match, 456, 200));
    assert(!cache->apply(match, 123, 100 + album_resolver::kContextLifetimeUs));
    std::strcpy(match.artist, "Another Artist");
    assert(!cache->apply(match, 123, 200));
    std::strcpy(match.artist, "The Dave Brubeck Quartet");
    std::strcpy(match.title, "Take Five (Live)");
    assert(!cache->apply(match, 123, 200));
    std::strcpy(match.title, "Take Five");
    assert(cache->apply(match, 123, 200)); // skipping backwards is supported
    cache->sequence.tracks[1] = cache->sequence.tracks[0];
    assert(!cache->apply(match, 123, 200)); // ambiguous duplicate

    // A synthetic side: four 200 s tracks.
    album->count = 4;
    for (auto &track : album->tracks) { track = {}; track.duration_s = 200; }
    std::strcpy(album->tracks[1].title, "Two");

    // A gap where the track ends: next track at once, checked the first time.
    Follower follow;
    follow.start(*album, 0, 0);
    assert(follow.tick(100 * s) == Action::None);
    assert(follow.gap(199 * s, 201 * s) == Action::Advance && follow.index() == 1);
    assert(std::strcmp(follow.track().title, "Two") == 0 && follow.track_start() == 201 * s);
    assert(follow.tick(207 * s - 1) == Action::None);
    assert(follow.tick(207 * s) == Action::Verify);
    assert(follow.tick(208 * s) == Action::None);  // asked once
    follow.confirmed(201 * s);
    // Once confirmed, matching gaps need no lookups.
    assert(follow.gap(400 * s, 402 * s) == Action::Advance && follow.index() == 2);
    for (uint32_t t = 402 * s; t < 590 * s; t += s) assert(follow.tick(t) == Action::None);

    // Silence between tracks holds the title until sound returns.
    assert(follow.silence(601 * s) == Action::None);
    assert(follow.tick(605 * s) == Action::None);
    assert(follow.sound(605 * s) == Action::Advance && follow.index() == 3);
    // Past the last track there is nothing to predict.
    assert(follow.gap(805 * s, 806 * s) == Action::Lost && !follow.active());

    // Silence in the middle of a track: the music stopped or changed.
    follow.start(*album, 0, 0);
    assert(follow.silence(90 * s) == Action::Lost && !follow.active());
    // Silence at a track end that never resumes: stopped.
    follow.start(*album, 0, 0);
    assert(follow.silence(200 * s) == Action::None);
    assert(follow.tick(209 * s) == Action::None && follow.tick(210 * s) == Action::Lost);
    // Sound after a silence that was not between tracks starts afresh.
    follow.start(*album, 0, 0);
    assert(follow.sound(50 * s) == Action::Lost);

    // Quiet moments inside a track cost nothing: no advance, no check.
    follow.start(*album, 0, 0);
    for (uint32_t t = 10; t < 190; t += 3) assert(follow.gap(t * s, (t + 1) * s) == Action::None);
    for (uint32_t t = 10; t < 190; ++t) assert(follow.tick(t * s) == Action::None);
    assert(follow.index() == 0);
    // Two inconclusive checks in a row give up on the album.
    assert(follow.gap(199 * s, 201 * s) == Action::Advance);
    assert(follow.tick(207 * s) == Action::Verify);
    assert(follow.unconfirmed(207 * s) == Action::None);
    assert(follow.tick(217 * s) == Action::Verify);
    assert(follow.unconfirmed(217 * s) == Action::Lost && !follow.active());

    // No gap where the track should end (gapless or a playlist): move on, check.
    follow.start(*album, 0, 0);
    follow.confirmed(0);
    assert(follow.tick(206 * s) == Action::None);
    assert(follow.tick(207 * s) == Action::Advance && follow.index() == 1 && follow.track_start() == 200 * s);
    assert(follow.tick(206 * s) == Action::Verify);
    // A gap far from where the next track ends is not an advance.
    assert(follow.gap(300 * s, 301 * s) == Action::None && follow.index() == 1);
}
