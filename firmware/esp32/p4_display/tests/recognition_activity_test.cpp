#include <cassert>
#include <cstdio>
#include "recognition_activity.h"
#include "recognition_state.h"

int main() {
    recognition_policy::Sessions sessions;
    sessions.set_network(true, "http://192.168.1.26");
    sessions.set_controller(true);
    audio_presence::Snapshot state;
    assert(recognition_activity::received(sessions, state, 0, 1, false));
    const auto first = sessions.snapshot();
    assert(first.ready && recognition_policy::request_allowed(true, first, false));
    // Zero-valued PCM stays eligible: local loudness does not gate this mode.
    int16_t silence[128]{};
    audio_presence::Tracker detector;
    recognition_policy::Planner planner({3, 5, 8}, 313, 313);
    planner.sound_started(0);
    for (unsigned hop = 0; hop < 1000; ++hop) {
        detector.feed(silence, 128, hop * 128, 1, [](auto, auto) {});
        assert(!recognition_activity::received(sessions, state, hop * 128, 1, false));
    }
    assert(!detector.snapshot().sound && sessions.snapshot().epoch == first.epoch);
    recognition_policy::Window window;
    assert(planner.due(1000, window));
    const auto transport = sessions.transport_epoch();
    recognition_state::Context context{true, sessions.snapshot().ready, first.epoch,
        sessions.snapshot().epoch, transport, transport, 1, 1, 1, 0, state.start_sample};
    assert(recognition_state::evaluate(recognition_state::Outcome::Matched, {true, 0, 48000}, context)
        == recognition_state::Reason::Current);
    recognition_activity::missing(sessions, state);
    assert(!sessions.snapshot().ready && !state.sound);
    assert(recognition_activity::received(sessions, state, 128000, 2, false));
    assert(state.start_sample == 128000 && sessions.transport_epoch() != transport);
    sessions.set_controller(false);
    assert(!recognition_policy::request_allowed(true, sessions.snapshot(), true));
    std::puts("PASS: continuous PCM bypasses loudness for requests and matches; loss/disconnect still invalidate");
}
