#include <cassert>
#include <array>
#include "audio_presence.h"
#include "recognition_policy.h"

int main() {
    audio_presence::Tracker tracker;
    recognition_policy::Sessions sessions;
    sessions.set_network(true, "http://192.168.1.10"); sessions.set_controller(true);
    uint64_t position = 0;
    auto event = [&](const audio_presence::Snapshot &state, listening::Event) { sessions.set_sound(state.sound); };
    auto feed = [&](unsigned hops, int16_t amplitude, uint32_t segment = 0) {
        std::array<int16_t, 128> pcm;
        for (unsigned i = 0; i < pcm.size(); ++i) pcm[i] = i % 2 ? amplitude : -amplitude;
        for (unsigned i = 0; i < hops; ++i) {
            tracker.feed(pcm.data(), pcm.size(), position, segment, event); position += pcm.size();
        }
    };
    feed(625, 130); assert(!sessions.snapshot().ready);
    feed(375, 520); assert(tracker.snapshot().sound && sessions.snapshot().ready);
    const auto in_flight = sessions.snapshot();
    const auto start = tracker.snapshot().start_sample;
    assert(start < position && start >= 600 * 128);
    // Audio presence continues while the recognition/network worker is blocked.
    feed(375, 130);
    assert(!tracker.snapshot().sound && !sessions.snapshot().ready);
    assert(sessions.snapshot().epoch != in_flight.epoch);
    feed(375, 520);
    assert(tracker.snapshot().sound && sessions.snapshot().epoch != in_flight.epoch);
    assert(tracker.snapshot().start_sample > start);
    // A stream interruption invalidates a pending match immediately.
    const auto pending = sessions.snapshot();
    tracker.reset(event);
    assert(!tracker.snapshot().sound && sessions.snapshot().epoch != pending.epoch);
    feed(375, 520, 1); assert(tracker.snapshot().sound);
}
