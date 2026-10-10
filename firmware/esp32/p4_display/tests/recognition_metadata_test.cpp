#include <cassert>
#include <cstring>
#include "recognition_metadata.h"

int main()
{
    using namespace recognition_metadata;
    auto check = [](const char *json, RecognitionTrack &track) {
        cJSON *root = cJSON_Parse(json);
        assert(root != nullptr);
        const bool matched = parse(root, track);
        cJSON_Delete(root);
        return matched;
    };
    RecognitionTrack track{};
    assert(check(R"({"track":{"title":"Seconds","subtitle":"U2","sections":[{"metadata":[{"title":"Album","text":"War"}]}],"images":{"coverart":"https://is1-ssl.mzstatic.com/image/thumb/cover/400x400bb.jpg"}}})", track));
    assert(std::strcmp(track.title, "Seconds") == 0 && std::strcmp(track.artist, "U2") == 0);
    assert(std::strcmp(track.album, "War") == 0 && track.artwork_url[0] != '\0');
    assert(!check(R"({"matches":[]})", track) && track.title[0] == '\0');
    assert(!check(R"({"track":{"title":"   "}})", track));
    assert(!check(R"({"track":{"title":"bad\ntext"}})", track));
    assert(check(R"({"track":{"title":"Björk","images":{"coverart":"https://127.0.0.1/private"}}})", track));
    assert(track.artwork_url[0] == '\0');
    // The leading dot in the suffix requires a real DNS label boundary.
    assert(artwork_url("https://is1-ssl.mzstatic.com/image"));
    assert(artwork_url("https://a.b.mzstatic.com/image"));
    assert(!artwork_url("https://evil-mzstatic.com/image"));
    assert(!artwork_url("https://evilmzstatic.com/image"));
    assert(!artwork_url("https://mzstatic.com.evil.test/image"));
    // Policy permits subdomains only, not the bare root domain.
    assert(!artwork_url("https://mzstatic.com/image"));
    assert(!artwork_url("http://is1-ssl.mzstatic.com/image"));
    assert(!artwork_url("https://is1-ssl.mzstatic.com.evil.test/image"));
    assert(!artwork_url("https://user@is1-ssl.mzstatic.com/image"));
    assert(!artwork_url("https://is1-ssl.mzstatic.com:443/image"));
    assert(!artwork_url("https://is1-ssl.mzstatic.com/image\n"));
    assert(!check(R"({"track":{"title":"\u202ehidden"}})", track));
}
