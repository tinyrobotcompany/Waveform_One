#include "music_policy.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
    using music_detection::Presence;
    constexpr int64_t s = 1000000;
    int8_t scores[521]{};
    for (auto &value : scores) value = -128;
    assert(music_detection::music_score(scores) == 0);
    scores[24] = 127;
    assert(music_detection::music_score(scores) > 0.99f);

    // Talk scores near zero: never music, and after 10 s it is called not music.
    Presence talk;
    for (int64_t t = 0; t <= 30 * s; t += s / 2) talk.update(0.06f, t);
    assert(!talk.music(30 * s) && talk.not_music(30 * s));
    Presence early;
    early.update(0.0f, 0);
    assert(!early.music(s) && !early.not_music(9 * s));

    // A confident window enters at once; two weak ones within 4 s enter too.
    Presence pop;
    pop.update(0.7f, s);
    assert(pop.music(s));
    Presence quiet;
    quiet.update(0.25f, s);
    assert(!quiet.music(s));
    quiet.update(0.1f, 2 * s);
    quiet.update(0.25f, 4 * s);
    assert(quiet.music(4 * s) && !quiet.not_music(4 * s));
    // Weak windows further apart do not.
    Presence sparse;
    sparse.update(0.25f, 0);
    sparse.update(0.25f, 5 * s);
    assert(!sparse.music(5 * s));

    // Quiet music holds through 18 s without a positive window, then lapses
    // after 20 s and must re-enter.
    quiet.update(0.05f, 20 * s);
    assert(quiet.music(22 * s));
    assert(!quiet.music(24 * s + 1));
    quiet.update(0.05f, 25 * s);
    assert(!quiet.music(25 * s));
    quiet.update(0.3f, 26 * s);
    assert(!quiet.music(26 * s));

    // Silence resets: talk after a song does not inherit its decision.
    pop.reset();
    assert(!pop.music(2 * s) && !pop.not_music(60 * s));

    // Invalid scores are ignored.
    Presence invalid;
    invalid.update(std::numeric_limits<float>::quiet_NaN(), 0);
    invalid.update(2.0f, 0);
    assert(!invalid.music(0) && !invalid.not_music(20 * s));
    std::puts("PASS: music presence enters on music-like windows, holds quiet music, never on talk");
}
