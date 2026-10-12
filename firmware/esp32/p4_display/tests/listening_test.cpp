#include <cassert>
#include <cmath>
#include <vector>
#include "listening.h"

namespace {
uint32_t seed = 1;
// Gaussian-ish noise with the given RMS, one hop at a time.
void hop(double rms, int16_t *out)
{
    for (int i = 0; i < 128; ++i) {
        double sum = 0;
        for (int k = 0; k < 12; ++k) { seed = 1664525u * seed + 1013904223u; sum += (seed >> 8) / double(1 << 24); }
        out[i] = int16_t(std::lround((sum - 6) * rms));
    }
}

struct Run {
    listening::Detector detector;
    uint32_t hops = 0;
    // Plays seconds of sound at rms; returns the hop of the first event, or -1.
    long play(double seconds, double rms, listening::Event want, uint32_t *start = nullptr)
    {
        long at = -1;
        int16_t samples[128];
        for (uint32_t i = 0; i < seconds * 125; ++i) {
            hop(rms, samples);
            uint32_t begin = 0;
            const auto event = detector.push(samples, 128, hops, begin);
            if (event == want && at < 0) { at = hops; if (start) *start = begin; }
            assert(event == listening::Event::None || event == want);
            ++hops;
        }
        return at;
    }
};
} // namespace

int main()
{
    using listening::Event;
    Run room;
    // A quiet room (-48 dBFS) is learnt as background, never as sound.
    assert(room.play(30, 130, Event::SoundStarted) < 0);
    assert(!room.detector.sound() && room.detector.calibrated());
    assert(std::abs(room.detector.background_db() + 48) < 2);
    // Music 12 dB louder starts within a second; the start reaches back to it.
    uint32_t start = 0;
    const uint32_t music = room.hops;
    const long started = room.play(10, 520, Event::SoundStarted, &start);
    assert(started >= 0 && started - music <= 100);
    assert(start <= music + 10 && start + 120 >= music);
    // A four-minute song does not become the background.
    assert(room.play(240, 520, Event::SilenceStarted) < 0 && room.detector.sound());
    // A one-second pause in the music is not silence...
    assert(room.play(1, 130, Event::SilenceStarted) < 0);
    assert(room.play(5, 520, Event::SilenceStarted) < 0);
    // ...but two seconds back at the background is.
    const uint32_t stop = room.hops;
    const long silent = room.play(5, 130, Event::SilenceStarted);
    // Ten 0.2 s blocks; the first may straddle the stop.
    assert(silent >= 0 && silent - stop >= 225 && silent - stop <= 275);
    assert(!room.detector.sound());

    // A fan turning on raises the background within a minute or two.
    Run fan;
    fan.play(10, 130, Event::SoundStarted);
    fan.play(8, 400, Event::SoundStarted);       // heard as sound at first
    // Slowly while recognition might still identify it...
    fan.play(30, 400, Event::SilenceStarted);
    assert(fan.detector.sound() && fan.detector.background_db() < -47);
    // ...quickly once it has failed to.
    fan.detector.set_unrecognised(true);
    assert(fan.play(150, 400, Event::SilenceStarted) >= 0);
    assert(fan.detector.background_db() > -40);
    // Music must then be louder than the fan to count.
    fan.play(10, 400, Event::SilenceStarted);
    assert(!fan.detector.sound());

    // A discontinuity forgets the half-built state but keeps the background.
    Run gap;
    gap.play(5, 130, Event::SoundStarted);
    gap.play(5, 520, Event::SoundStarted);
    assert(gap.detector.sound());
    const double background = gap.detector.background_db();
    gap.detector.discontinuity();
    assert(!gap.detector.sound() && gap.detector.background_db() == background);
    assert(gap.play(2, 520, Event::SoundStarted) >= 0);

    // Digital silence is the quietest possible background, not an error.
    Run zero;
    assert(zero.play(5, 0, Event::SoundStarted) < 0);
    assert(zero.detector.background_db() <= -89);
    assert(zero.play(2, 130, Event::SoundStarted) >= 0);
}
