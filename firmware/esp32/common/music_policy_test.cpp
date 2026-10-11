#include "music_policy.h"
#include <cassert>
#include <limits>
using music_detection::State;
int main() {
    music_detection::Policy policy;
    assert(policy.update(.9f, 1000000) == State::Unknown);
    assert(policy.update(.8f, 2000000) == State::Music);
    // Voices/noise interrupting known music do not immediately clear the album.
    assert(policy.update(.01f, 3000000) == State::Music);
    assert(policy.update(.01f, 4000000) == State::Music);
    assert(policy.update(.9f, 5000000) == State::Music);
    for (int second = 6; second < 11; ++second) assert(policy.update(.01f, second * 1000000LL) == State::Music);
    assert(policy.update(.01f, 11000000) == State::Quiet);
    policy.reset();
    assert(policy.update(.01f, 1000000) == State::Quiet);
    assert(policy.update(.4f, 2000000) == State::Unknown);
    assert(policy.update(.9f, 3000000) == State::Unknown);
    assert(policy.update(.9f, 4000000) == State::Music);
    assert(policy.update(.4f, 8000000) == State::Unknown); // Stale inference resets evidence.
    assert(policy.update(.9f, 9000000) == State::Unknown);
    assert(policy.update(.9f, 10000000) == State::Music);
    for (int second = 11; second <= 17; ++second) assert(policy.update(second % 2 ? .1f : .3f, second * 1000000LL) == State::Music);
    assert(policy.update(.3f, 18000000) == State::Unknown);
    assert(policy.update(std::numeric_limits<float>::quiet_NaN(), 19000000) == State::Unknown);
    int8_t scores[521];
    for (auto &score : scores) score = -128;
    scores[0] = 127; // Speech never vetoes music/singing/rap.
    scores[24] = 100;
    assert(music_detection::music_score(scores) == 228 / 256.f);
    scores[24] = -128; scores[31] = 110;
    assert(music_detection::music_score(scores) == 238 / 256.f);
}
