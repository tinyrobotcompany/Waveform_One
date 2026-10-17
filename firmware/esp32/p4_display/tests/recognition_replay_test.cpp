#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <memory>
#include "audio_stream.h"
#include "recognition_policy.h"

// Deterministic transport/scheduler replay, not an acoustic recognition test.
// Constant sample labels distinguish a previous song from its replacement.
int main() {
    using namespace recognition_policy;
    constexpr unsigned hz = audio_stream::kSampleRate;
    constexpr unsigned hop = audio_stream::kPacketSamples;
    auto audio = std::make_unique<audio_stream::Ring<16 * hz>>();
    auto write = [&](unsigned seconds, int16_t label) {
        std::array<int16_t, hop> packet;
        packet.fill(label);
        for (unsigned n = 0; n < seconds * hz / hop; ++n)
            audio->write(packet.data(), packet.size());
    };
    std::array<int16_t, 4096> buffer{};
    auto clip = std::make_unique<std::array<int16_t, 10 * hz>>();
    Sessions sessions;
    sessions.set_network(true, "http://192.168.1.10");
    sessions.set_controller(true); sessions.set_sound(true);
    Planner planner({3, 5, 8}, 313, 313);
    planner.sound_started(0);
    write(3, 111);
    uint64_t position = audio->written();
    Window window;
    assert(planner.due(position / hop, window));
    planner.attempted(window.last);
    planner.matched(window, window.last, 0, 300);

    // Twelve seconds of input accumulate while HTTPS/catalogue/artwork blocks.
    // After each completed lookup the next request must reach the stream's
    // current tail, not launch partway through draining its queued samples.
    for (unsigned cycle = 0; cycle < 5; ++cycle) {
        const int16_t current_song = int16_t(222 + cycle);
        write(12, current_song);
        const uint64_t tail = audio->written();
        unsigned reads = 0;
        uint64_t first_due = 0;
        bool requested = false;
        while (position < tail) {
            const auto read = audio->read(position, buffer.data(), buffer.size());
            assert(!read.skipped && read.segment == 0);
            position = read.position + read.count;
            assert(read.remaining == tail - position);
            ++reads;
            Window earlier;
            if (first_due == 0 && planner.due(position / hop, earlier)) first_due = position;
            if (request_allowed(true, sessions.snapshot(), planner.acquiring(), read.remaining == 0)
                && planner.due(position / hop, window)) {
                assert(position == tail); // fails with the old worker admission
                assert(!requested);
                const auto chosen = audio->read(uint64_t(window.first) * hop, clip->data(), clip->size());
                assert(!chosen.skipped && chosen.count == clip->size());
                assert(std::all_of(clip->begin(), clip->end(), [&](int16_t value) { return value == current_song; }));
                planner.attempted(window.last);
                planner.matched(window, window.last, 0, 300);
                requested = true;
            }
        }
        assert(requested && reads == 47);
        if (cycle == 0) assert(tail - first_due == 151040); // old admission: 9.440 s stale
    }
    // Transport loss still takes precedence even when the reader is caught up.
    sessions.set_controller(false);
    assert(!request_allowed(true, sessions.snapshot(), true, true));
    std::puts("PASS: five slow-service replays use current audio without backlog drift");
}
