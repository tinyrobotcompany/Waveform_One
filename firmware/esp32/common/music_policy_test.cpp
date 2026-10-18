#include "music_policy.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
    using music_detection::Presence;
    constexpr int64_t s = 1000000;
    constexpr uint64_t window = Presence::kWindowSamples;
    int8_t scores[521]{};
    for (auto &value : scores) value = -128;
    assert(music_detection::music_score(scores) == 0);
    scores[24] = 127;
    assert(music_detection::music_score(scores) > 0.99f);

    // Talk scores near zero: never music.
    Presence talk;
    for (int64_t t = 0; t <= 30 * s; t += s / 2) talk.update(0.06f, t, uint64_t(t) * 16 / 1000);
    assert(!talk.music(30 * s));

    // Real talk on the device, 16:06:05: one burst scoring 0.67, 0.41, 0.33,
    // 0.33 in windows 0.4 s apart. Overlapping windows are one piece of
    // evidence, so the burst is at most two, and is not music.
    Presence burst;
    const float scored[] = {0.668f, 0.414f, 0.332f, 0.332f};
    for (unsigned i = 0; i < 4; ++i) burst.update(scored[i], 5 * s + i * 400000, 80000 + i * 6400);
    assert(!burst.music(7 * s));

    // Music: three separate music-like windows within 6 s.
    Presence song;
    song.update(0.3f, 1 * s, 1 * window);
    song.update(0.25f, 2 * s, 2 * window);
    assert(!song.music(2 * s));
    song.update(0.05f, 3 * s, 3 * window);
    song.update(0.4f, 4 * s, 4 * window);
    assert(song.music(4 * s));
    // Three spread over more than 6 s do not enter.
    Presence slow;
    slow.update(0.3f, 0, 1 * window);
    slow.update(0.3f, 4 * s, 5 * window);
    slow.update(0.3f, 8 * s, 9 * window);
    assert(!slow.music(8 * s));
    slow.update(0.3f, 9 * s, 10 * window);
    assert(slow.music(9 * s));

    // Quiet passages hold for 15 s without a music-like window, then lapse.
    song.update(0.0f, 15 * s, 15 * window);
    assert(song.music(19 * s) && !song.music(19 * s + 1));
    song.update(0.0f, 20 * s, 20 * window);
    assert(!song.music(20 * s));

    // Silence resets.
    slow.reset();
    assert(!slow.music(9 * s));

    // Invalid scores are ignored.
    Presence invalid;
    invalid.update(std::numeric_limits<float>::quiet_NaN(), 0, window);
    invalid.update(2.0f, 0, window);
    assert(!invalid.music(0));
    std::puts("PASS: music presence needs three separate music-like windows; talk bursts and overlaps do not count");
}
