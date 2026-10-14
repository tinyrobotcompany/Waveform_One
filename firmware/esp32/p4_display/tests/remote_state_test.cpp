#include <cassert>
#include <cstring>
#include <thread>
#include "remote_state.h"

int main()
{
    RemoteState state;
    RecognitionTrack track{};
    track.matched_at_us = 1000000;
    std::strcpy(track.title, "In My Time of Dying (Live from Earls Court, 1975)");
    std::strcpy(track.artist, "Led Zeppelin");
    std::strcpy(track.album, "Physical Graffiti");
    std::strcpy(track.artwork_url, "https://is1-ssl.mzstatic.com/image.jpg");
    state.recognition(RecognitionStatus::Matched, &track);
    // The source belongs to the worker and may be reused immediately.
    track.title[0] = '\0';
    auto read = [&] {
        const std::string json = state.json();
        cJSON *root = cJSON_Parse(json.c_str());
        assert(root != nullptr);
        return root;
    };
    cJSON *root = read();
    cJSON *saved = cJSON_GetObjectItemCaseSensitive(root, "track");
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(saved, "artist")->valuestring, "Led Zeppelin") == 0);
    assert(cJSON_GetObjectItemCaseSensitive(saved, "title")->valuestring[0] == 'I');
    cJSON_Delete(root);
    album_palette::Palette palette{}; palette.background=0x302010;
    state.palette(palette);
    root=read();
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"palette"),"background")->valuestring,"#302010")==0);
    cJSON_Delete(root);
    track.title[0] = 'I';
    state.recognition(RecognitionStatus::Identifying,&track);
    root=read();
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"palette"),"background")->valuestring,"#302010")==0);
    cJSON_Delete(root);
    state.appearance(false);
    root=read();
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(root,"appearance")->valuestring,"classic")==0);
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"palette"),"background")->valuestring,"#080d0a")==0);
    cJSON_Delete(root); state.appearance(true);
    state.recognition(RecognitionStatus::Matched, &track);
    root = read();
    assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "track")));
    cJSON_Delete(root);
    // Recognition clears the song itself when the music stops.
    state.recognition(RecognitionStatus::Waiting, nullptr);
    root = read();
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(root, "track")));
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(root, "status")->valuestring, "waiting") == 0);
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"palette"),"background")->valuestring,"#080d0a")==0);
    cJSON_Delete(root);

    std::strcpy(track.title, "Quotes \" and newline\n</script>");
    std::strcpy(track.artwork_url, "https://mzstatic.com.evil.test/cover.jpg");
    state.recognition(RecognitionStatus::Identifying, &track);
    state.name("Zoë \" Simon");
    state.brightness(42);
    state.style("mirrored");
    root = read();
    saved = cJSON_GetObjectItemCaseSensitive(root, "track");
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(saved, "title")->valuestring, track.title) == 0);
    assert(cJSON_GetObjectItemCaseSensitive(saved, "artwork")->valuestring[0] == '\0');
    assert(cJSON_GetObjectItemCaseSensitive(root, "brightness")->valueint == 42);
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(root, "name")->valuestring, "Zoë \" Simon") == 0);
    cJSON_Delete(root);

    // Repeated HTTP reads cannot see mixed fields from concurrent callbacks.
    std::thread writer([&] {
        for (int i = 0; i < 1000; ++i) {
            state.recognition(RecognitionStatus::Identifying, &track);
            state.recognition(RecognitionStatus::Waiting, nullptr);
        }
    });
    for (int i = 0; i < 1000; ++i) {
        root = read();
        saved = cJSON_GetObjectItemCaseSensitive(root, "track");
        if (cJSON_IsObject(saved)) {
            assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(saved, "title")->valuestring, track.title) == 0);
            assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(root, "status")->valuestring, "identifying") == 0);
        }
        cJSON_Delete(root);
    }
    writer.join();
    root = read();
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(root, "track")));
    cJSON_Delete(root);
}
