#include <cassert>
#include <cstring>
#include <string>
#include "album_resolver.h"

namespace {
std::string song(const char *title, const char *artist, const char *album, long id, const char *date,
                 const char *collection_artist = nullptr, long length_ms = 236000)
{
    std::string text = std::string("{\"wrapperType\":\"track\",\"kind\":\"song\",\"trackName\":\"") + title
        + "\",\"artistName\":\"" + artist + "\",\"collectionName\":\"" + album
        + "\",\"collectionId\":" + std::to_string(id) + ",\"releaseDate\":\"" + date
        + "\",\"trackTimeMillis\":" + std::to_string(length_ms)
        + ",\"artworkUrl100\":\"https://is1-ssl.mzstatic.com/image/thumb/Music/" + std::to_string(id)
        + "/100x100bb.jpg\"";
    if (collection_artist) text += std::string(",\"collectionArtistName\":\"") + collection_artist + "\"";
    return text + "}";
}

cJSON *results(std::initializer_list<std::string> items)
{
    std::string text = "{\"resultCount\":" + std::to_string(items.size()) + ",\"results\":[";
    bool first = true;
    for (const auto &item : items) { if (!first) text += ","; text += item; first = false; }
    cJSON *root = cJSON_Parse((text + "]}").c_str());
    assert(root != nullptr);
    return root;
}

RecognitionTrack track(const char *title, const char *artist)
{
    RecognitionTrack value{};
    std::strcpy(value.title, title);
    std::strcpy(value.artist, artist);
    return value;
}
} // namespace

int main()
{
    using namespace album_resolver;
    assert(normalise("Crazy In Love (feat. JAY-Z)") == "crazy in love");
    assert(normalise("Whole Lotta Love - 1990 Remaster") == "whole lotta love");
    assert(normalise("Don't Stop Me Now [Live]") == "dont stop me now");
    assert(normalise("  Café   del Mar! ") == "café del mar");
    assert(lead_artist("Beyoncé Feat. JAY-Z") == "beyoncé");
    assert(lead_artist("Simon & Garfunkel") == "simon garfunkel");
    assert(lead_artist("Calvin Harris x Dua Lipa") == "calvin harris");
    const auto url = search_url(track("Crazy In Love (feat. JAY-Z)", "Beyoncé Feat. JAY-Z"));
    assert(url == "https://itunes.apple.com/search?media=music&entity=song&limit=25&country=GB"
                  "&term=beyonc%C3%A9+crazy+in+love");

    Context none;
    Choice choice;
    // The single came out first, but the album is what a listener expects.
    cJSON *root = results({
        song("Crazy In Love (feat. Jay-Z)", "Beyoncé", "Crazy In Love (feat. Jay-Z) - Single", 11, "2003-05-18T07:00:00Z"),
        song("Crazy In Love (feat. Jay-Z)", "Beyoncé", "Dangerously In Love", 22, "2003-06-23T07:00:00Z"),
        song("Crazy In Love (feat. Jay-Z)", "Beyoncé", "Now That's What I Call Music! 55", 33, "2003-07-21T07:00:00Z", "Various Artists"),
        song("Crazy In Love", "Beyoncé", "Crazy In Love (Remixes) - EP", 44, "2003-06-01T07:00:00Z"),
    });
    assert(choose(root, track("Crazy In Love (feat. JAY-Z)", "Beyoncé Feat. JAY-Z"), none, 0, choice));
    assert(std::strcmp(choice.album, "Dangerously In Love") == 0 && choice.collection_id == 22);
    assert(choice.duration_s == 236);
    assert(std::strcmp(choice.artwork_url, "https://is1-ssl.mzstatic.com/image/thumb/Music/22/400x400bb.jpg") == 0);
    cJSON_Delete(root);

    // The original album beats later compilations and live albums.
    root = results({
        song("Inception (Live)", "McCoy Tyner", "Live at Newport", 504, "1963-07-05T07:00:00Z"),
        song("Inception", "McCoy Tyner", "Jazz Essentials: Piano", 502, "2010-01-01T08:00:00Z", "Various Artists"),
        song("Inception", "McCoy Tyner Trio", "Inception", 503, "1962-01-01T08:00:00Z"),
        song("Inception", "McCoy Tyner", "The Impulse Story", 501, "2006-01-01T08:00:00Z"),
    });
    auto inception = track("Inception", "McCoy Tyner");
    std::strcpy(inception.album, "The Definitive McCoy Tyner");
    assert(choose(root, inception, none, 0, choice));
    assert(std::strcmp(choice.album, "Inception") == 0);
    // A compilation from the service is replaced when its name gives it
    // away. ("The Impulse Story" does not, and would be kept.)
    std::strcpy(inception.album, "Jazz Essentials: Piano");
    assert(choose(root, inception, none, 0, choice));
    assert(std::strcmp(choice.album, "Inception") == 0);
    cJSON_Delete(root);

    // Song dates are useless (compilations carry the original date), so the
    // most relevant regular album wins over compilations and soundtracks.
    root = results({
        song("Take Five", "The Dave Brubeck Quartet", "Time Out", 601, "1959-12-14T08:00:00Z"),
        song("Take Five", "The Dave Brubeck Quartet", "The Absolutely Essential Dave Brubeck", 602, "1959-09-21T07:00:00Z", "Dave Brubeck"),
        song("Take Five", "The Dave Brubeck Quartet", "Summer Jazz", 603, "1959-09-21T07:00:00Z", "Various Artists"),
        song("Take Five", "The Dave Brubeck Quartet", "Pleasantville (Music from the Motion Picture)", 604, "1959-01-01T08:00:00Z", "Pleasantville"),
    });
    assert(choose(root, track("Take Five", "The Dave Brubeck Quartet"), none, 0, choice));
    assert(std::strcmp(choice.album, "Time Out") == 0);
    cJSON_Delete(root);
    root = results({
        song("Girls On Film", "Duran Duran", "Greatest", 701, "1981-06-15T07:00:00Z"),
        song("Girls On Film (Night Version)", "Duran Duran", "The Singles 81-85", 702, "2003-03-03T08:00:00Z"),
        song("Girls On Film (AIR Studio Version)", "Duran Duran", "Duran Duran (Deluxe Edition)", 703, "1981-06-15T07:00:00Z"),
        song("Girls On Film", "Duran Duran", "Decade", 704, "1981-06-15T07:00:00Z"),
        song("Girls On Film (Live)", "Duran Duran", "As the Lights Go Down", 705, "2010-03-26T07:00:00Z"),
        song("Girls On Film (Remastered)", "Duran Duran", "Duran Duran", 706, "1981-06-15T07:00:00Z"),
    });
    assert(choose(root, track("Girls On Film", "Duran Duran"), none, 0, choice));
    assert(std::strcmp(choice.album, "Duran Duran") == 0 && choice.collection_id == 706);
    cJSON_Delete(root);

    // A regular album from the service stands, in the service's own words...
    root = results({
        song("Whole Lotta Love", "Led Zeppelin", "Mothership (Remastered)", 801, "2007-11-12T08:00:00Z"),
        song("Whole Lotta Love", "Led Zeppelin", "Led Zeppelin II (Deluxe Edition)", 802, "1969-10-22T07:00:00Z"),
        song("Whole Lotta Love", "Led Zeppelin", "Early Days: The Best of Led Zeppelin, Vol. 1", 803, "1999-11-23T08:00:00Z"),
    });
    auto heard = track("Whole Lotta Love", "Led Zeppelin");
    std::strcpy(heard.album, "Led Zeppelin II");
    assert(choose(root, heard, none, 0, choice));
    assert(std::strcmp(choice.album, "Led Zeppelin II") == 0 && choice.collection_id == 802);
    // ...but a compilation from the service is replaced.
    std::strcpy(heard.album, "Early Days: The Best of Led Zeppelin, Vol. 1");
    assert(choose(root, heard, none, 0, choice));
    assert(choice.collection_id == 801 || choice.collection_id == 802);
    cJSON_Delete(root);
    // Only compilations on offer: the service's answer is kept.
    root = results({
        song("Whole Lotta Love", "Led Zeppelin", "Early Days: The Best of Led Zeppelin, Vol. 1", 803, "1999-11-23T08:00:00Z"),
    });
    std::strcpy(heard.album, "Some Album");
    assert(!choose(root, heard, none, 0, choice));
    cJSON_Delete(root);

    // The album already playing wins when it has the song, even over the
    // service's own choice...
    Context playing{904, 1000000};
    root = results({
        song("Whole Lotta Love", "Led Zeppelin", "Led Zeppelin II", 903, "1969-10-22T07:00:00Z"),
        song("Whole Lotta Love", "Led Zeppelin", "Early Days: The Best of Led Zeppelin, Vol. 1", 904, "1999-11-23T08:00:00Z"),
    });
    std::strcpy(heard.album, "Led Zeppelin II");
    assert(choose(root, heard, playing, 2000000, choice));
    assert(choice.collection_id == 904);
    // ...but not after twenty minutes.
    assert(choose(root, heard, playing, 1000000 + kContextLifetimeUs, choice));
    assert(choice.collection_id == 903);
    cJSON_Delete(root);

    // Band credits differ between services; the lead name must still agree.
    root = results({
        song("Take Five", "The Dave Brubeck Quartet", "Time Out", 701, "1959-12-14T08:00:00Z"),
        song("Take Five", "Someone Else", "Take Five Covers", 702, "1950-01-01T08:00:00Z"),
    });
    assert(choose(root, track("Take Five", "Dave Brubeck"), none, 0, choice));
    assert(std::strcmp(choice.album, "Time Out") == 0);
    cJSON_Delete(root);

    // A live recording keeps its live album.
    root = results({
        song("In My Time of Dying (Live from Earls Court, 1975)", "Led Zeppelin", "Earls Court 1975 (Live)", 801, "2020-01-01T08:00:00Z"),
        song("In My Time of Dying (Live from Earls Court, 1975)", "Led Zeppelin", "Physical Graffiti (Deluxe Edition)", 802, "1975-02-24T08:00:00Z"),
    });
    assert(choose(root, track("In My Time of Dying (Live from Earls Court, 1975)", "Led Zeppelin"), none, 0, choice));
    assert(choice.collection_id == 801 || choice.collection_id == 802);
    cJSON_Delete(root);

    // Nothing clearly the same song: keep the service's answer.
    root = results({
        song("Popular Song", "Mika", "The Boy Who Knew Too Much", 901, "2009-01-01T08:00:00Z"),
        song("Popular", "Somebody Else", "Wicked Covers", 902, "2004-01-01T08:00:00Z"),
    });
    assert(!choose(root, track("Popular", "Kristin Chenoweth"), none, 0, choice));
    cJSON_Delete(root);
    root = cJSON_Parse("{\"results\":[{\"kind\":\"music-video\",\"trackName\":\"Popular\"}]}");
    assert(!choose(root, track("Popular", "Kristin Chenoweth"), none, 0, choice));
    cJSON_Delete(root);
    root = cJSON_Parse("{\"error\":\"rate limited\"}");
    assert(!choose(root, track("Popular", "Kristin Chenoweth"), none, 0, choice));
    cJSON_Delete(root);
    // Untrusted artwork hosts are dropped but the album is still used.
    root = cJSON_Parse("{\"results\":[{\"kind\":\"song\",\"trackName\":\"Popular\",\"artistName\":\"Kristin Chenoweth\","
                       "\"collectionName\":\"Wicked\",\"collectionId\":5,\"artworkUrl100\":\"https://evil.test/100x100bb.jpg\"}]}");
    assert(choose(root, track("Popular", "Kristin Chenoweth"), none, 0, choice));
    assert(std::strcmp(choice.album, "Wicked") == 0 && choice.artwork_url[0] == '\0' && choice.duration_s == 0);
    cJSON_Delete(root);
}
