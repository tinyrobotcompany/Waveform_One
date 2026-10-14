#include <algorithm>
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
            assert(event == listening::Event::None || event == want || event == listening::Event::Gap);
            ++hops;
        }
        return at;
    }
};
} // namespace

int main()
{
    using listening::Event;
    // Independent diagnostic checks: DC energy is not acoustic AC energy;
    // rails and zeros must be measured without changing detector decisions.
    {
        listening::Detector meter;
        int16_t samples[128];
        uint32_t number = 0, begin = 0;
        auto block = [&] {
            for (unsigned h = 0; h < listening::kBlockHops; ++h)
                meter.push(samples, 128, number++, begin);
        };
        std::fill(samples, samples + 128, 1024);
        block();
        assert(meter.quality().mean == 1024 && meter.quality().ac_db <= -119);
        assert(std::abs(meter.level_db() + 30.103) < 0.01);
        assert(meter.quality().rail_percent == 0 && meter.quality().zero_percent == 0);
        for (unsigned i = 0; i < 128; ++i) samples[i] = i % 2 ? INT16_MAX : INT16_MIN;
        block();
        assert(meter.quality().mean == -0.5 && meter.quality().ac_db > -0.001);
        assert(meter.quality().rail_percent == 100);
        std::fill(samples, samples + 128, 0);
        block();
        assert(meter.quality().zero_percent == 100 && meter.quality().rail_percent == 0);
        meter.discontinuity();
        assert(meter.quality().mean == 0 && meter.quality().zero_percent == 0);
    }
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
    // A one-second pause in the music is not silence but a gap: reported
    // when the music returns, from where the pause began.
    const uint32_t pause = room.hops;
    assert(room.play(1, 130, Event::SilenceStarted) < 0);
    uint32_t gap_start = 0;
    const long dip = room.play(5, 520, Event::Gap, &gap_start);
    assert(dip >= 0 && dip - long(pause) <= 175);
    assert(gap_start + 50 >= pause && gap_start <= pause + 50);
    assert(room.detector.gap_end() + 50 >= pause + 125 && room.detector.gap_end() <= pause + 175);
    // A dip shorter than 0.4 s is not a gap.
    assert(room.play(0.2, 130, Event::Gap) < 0 && room.play(2, 520, Event::Gap) < 0);
    // ...but 1.6 s back at the background is.
    const uint32_t stop = room.hops;
    const long silent = room.play(5, 130, Event::SilenceStarted);
    // Mostly quiet (7 of 8 blocks of 0.2 s); the first may straddle the stop.
    assert(silent >= 0 && silent - stop >= 150 && silent - stop <= 225);
    assert(!room.detector.sound());

    // A real room wanders several dB from moment to moment; that is not sound,
    // and returning to it after music is silence.
    Run uneven;
    uint32_t wander = 7;
    auto room_noise = [&](double seconds, listening::Event want) {
        long at = -1;
        for (int second = 0; second < seconds; ++second) {
            wander = 1664525u * wander + 1013904223u;
            const double rms = 90 + (wander >> 24) * 90.0 / 255; // -51..-45 dBFS
            const long event = uneven.play(1, rms, want);
            if (event >= 0 && at < 0) at = event;
        }
        return at;
    };
    assert(room_noise(60, Event::SoundStarted) < 0 && !uneven.detector.sound());
    assert(uneven.play(3, 520, Event::SoundStarted) >= 0);
    uneven.play(60, 520, Event::SilenceStarted);
    assert(room_noise(5, Event::SilenceStarted) >= 0 && !uneven.detector.sound());

    // Sound that recognition cannot identify stays sound: music the service
    // misses at first must never become the background.
    Run missed;
    missed.play(10, 130, Event::SoundStarted);
    assert(missed.play(3, 520, Event::SoundStarted) >= 0);
    missed.detector.set_unrecognised(true);
    assert(missed.play(120, 520, Event::SilenceStarted) < 0 && missed.detector.sound());
    assert(missed.detector.background_db() < -44);

    // A discontinuity forgets the half-built state but keeps the background.
    Run gap;
    gap.play(5, 130, Event::SoundStarted);
    gap.play(5, 520, Event::SoundStarted);
    assert(gap.detector.sound());
    const double background = gap.detector.background_db();
    gap.detector.discontinuity();
    assert(!gap.detector.sound() && gap.detector.background_db() == background);
    assert(gap.play(2, 520, Event::SoundStarted) >= 0);

    // A stop in a room with the odd louder moment (a creak every second)
    // is still found within about two seconds.
    {
        Run creaky;
        creaky.play(5, 130, Event::SoundStarted);
        assert(creaky.play(5, 520, Event::SoundStarted) >= 0);
        long found = -1;
        const uint32_t stopped = creaky.hops;
        for (int second = 0; second < 6 && found < 0; ++second) {
            found = creaky.play(0.8, 130, Event::SilenceStarted);
            if (found < 0) found = creaky.play(0.2, 400, Event::SilenceStarted);
        }
        assert(found >= 0 && found - long(stopped) <= 3 * 125);
        float levels[listening::Detector::kRecentBlocks];
        assert(creaky.detector.recent(levels) == listening::Detector::kRecentBlocks);
        assert(levels[49] < -40 && *std::max_element(levels, levels + 50) > -40);
    }

    // While a song is shown, a 2 s quiet passage is a gap, not a stop.
    room.detector.set_patient(true);
    assert(room.play(5, 520, Event::SoundStarted) >= 0);
    assert(room.play(2, 130, Event::SilenceStarted) < 0);
    assert(room.play(2, 520, Event::Gap) >= 0 && room.detector.sound());
    const long patient = room.play(5, 130, Event::SilenceStarted);
    assert(patient >= 0 && room.detector.stop_blocks() == 15);
    room.detector.set_patient(false);

    // Switched on while music plays: the music is heard as sound, not learnt
    // as the background.
    Run playing;
    assert(playing.play(4, 520, Event::SoundStarted) >= 0);
    assert(playing.detector.background_db() <= -48);

    // Digital silence is the quietest possible background, not an error.
    Run zero;
    assert(zero.play(5, 0, Event::SoundStarted) < 0);
    assert(zero.detector.background_db() <= -89);
    assert(zero.play(2, 130, Event::SoundStarted) >= 0);
    // Votes from a previous quiet passage must not combine with a new dip
    // after music has been confidently heard again.
    Run resumed;
    resumed.play(5, 130, Event::SoundStarted);
    resumed.play(3, 520, Event::SoundStarted);
    resumed.detector.set_patient(true);
    assert(resumed.play(2.2, 130, Event::SilenceStarted) < 0);
    resumed.play(0.6, 520, Event::Gap);
    assert(resumed.play(0.4, 130, Event::SilenceStarted) < 0);
    assert(resumed.detector.sound());
}
