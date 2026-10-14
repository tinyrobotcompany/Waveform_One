#include <cassert>
#include <cstdio>
#include "recognition_state.h"
#include "recognition_policy.h"

int main() {
    using namespace recognition_state;
    using recognition_evidence::Coverage;
    recognition_policy::Sessions sessions;
    sessions.set_network(true, "http://192.168.1.10");
    sessions.set_controller(true); sessions.set_sound(true);
    const auto request = sessions.snapshot();
    const uint64_t transport = sessions.transport_epoch();
    Context current{true, true, request.epoch, request.epoch, transport, transport, 4, 4, 4, 0, 0};
    Coverage old_song{true, 0, 48000};
    assert(evaluate(Outcome::Matched, old_song, current) == Reason::Current);

    // Continuous music, including quiet passages with no local stop event:
    // neither a provider miss nor an outage invalidates the session's track.
    assert(evaluate(Outcome::NoMatch, {}, current) == Reason::Current);
    assert(evaluate(Outcome::Failed, {}, current) == Reason::Current);
    // A gapless next-song result is admissible without manufacturing silence.
    assert(evaluate(Outcome::Matched, {true, 48000, 96000}, current) == Reason::Current);

    // Stop while the network is in flight: even a valid old match is hidden.
    sessions.set_sound(false);
    auto stopped = current;
    stopped.ready = sessions.snapshot().ready; stopped.current_epoch = sessions.snapshot().epoch;
    assert(evaluate(Outcome::Matched, old_song, stopped) == Reason::Inactive);
    assert(evaluate(Outcome::NoMatch, {}, stopped) == Reason::Inactive);
    assert(evaluate(Outcome::Failed, {}, stopped) == Reason::Inactive);

    // Restart: only coverage extending into the resumed audio can survive.
    sessions.set_sound(true);
    auto resumed = current;
    resumed.current_epoch = sessions.snapshot().epoch; resumed.sound_start = 64000;
    assert(evaluate(Outcome::Matched, old_song, resumed) != Reason::Current);
    assert(evaluate(Outcome::Matched, {true, 32000, 96000}, resumed) == Reason::Current);
    assert(evaluate(Outcome::Matched, {}, resumed) != Reason::Current);
    assert(evaluate(Outcome::NoMatch, {}, resumed) != Reason::Current);
    assert(evaluate(Outcome::Failed, {}, resumed) != Reason::Current);
    // An overlapping request begun in the new session can still contain old audio.
    resumed.request_epoch = resumed.current_epoch;
    assert(evaluate(Outcome::Matched, old_song, resumed) == Reason::OldAudio);

    // USB resets its ring immediately; the independent presence task may not
    // yet have observed it or advanced the UI epoch. Reject the old response.
    auto broken_stream = current;
    broken_stream.stream_segment = 5;
    assert(evaluate(Outcome::Matched, old_song, broken_stream) == Reason::StreamChanged);
    broken_stream = current; broken_stream.presence_segment = 3;
    assert(evaluate(Outcome::Matched, old_song, broken_stream) == Reason::StreamChanged);

    sessions.set_controller(false); sessions.set_controller(true);
    auto reconnected = current;
    reconnected.current_epoch = sessions.snapshot().epoch;
    reconnected.current_transport = sessions.transport_epoch();
    assert(evaluate(Outcome::Matched, old_song, reconnected) == Reason::TransportChanged);
    std::puts("PASS: production response policy for continuous play, gapless change, stop/restart and stream races");
}
