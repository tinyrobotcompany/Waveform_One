#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Decides when sound starts and stops from the recognition audio itself.
// Levels are compared with the room's own background, learnt while it is
// quiet, so the same thresholds work in a silent study and a noisy kitchen.
// Times are counted in 128-sample hops of 16 kHz audio (125 per second).
namespace listening {

inline constexpr unsigned kHopsPerSecond = 125;
inline constexpr unsigned kBlockHops = 25;           // 0.2 s level blocks
inline constexpr double kStartAboveDb = 6;           // sound: 6 dB over background...
inline constexpr unsigned kStartBlocks = 3;          // ...for 0.6 s
inline constexpr double kStopAboveDb = 4;            // silence: within 4 dB of background...
inline constexpr unsigned kStopBlocks = 10;          // ...for 2 s
inline constexpr unsigned kCalibrationBlocks = 10;   // first 2 s set the background
// Background may rise quickly while quiet (a fan turns on) but only slowly
// while sound plays, so a long song cannot become the new background. Sound
// that recognition has repeatedly failed to identify is treated like quiet:
// a fan or a dishwasher must not be "music" for ten minutes.
inline constexpr double kQuietRiseDbPerBlock = 0.02;   // 6 dB a minute
inline constexpr double kSoundRiseDbPerBlock = 0.0033; // 1 dB a minute
inline constexpr double kLowestDb = -90;

enum class Event { None, SoundStarted, SilenceStarted };

class Detector {
public:
    // Feed every hop in order. Returns an event when the state changes; a
    // SoundStarted event reports the hop the sound most likely began at.
    Event push(const int16_t *hop, unsigned samples, uint32_t hop_number, uint32_t &start_hop)
    {
        for (unsigned i = 0; i < samples; ++i) energy_ += double(hop[i]) * hop[i];
        count_ += samples;
        if (++block_hops_ < kBlockHops) return Event::None;
        const double level = 10 * std::log10(energy_ / count_ / (32768.0 * 32768.0) + 1e-12);
        energy_ = 0; count_ = 0; block_hops_ = 0;
        last_level_ = level;
        return block(level, hop_number, start_hop);
    }

    // Audio stopped arriving or skipped: what follows is a new recording.
    void discontinuity()
    {
        energy_ = 0; count_ = 0; block_hops_ = 0;
        loud_ = quiet_ = 0;
        sound_ = unrecognised_ = false;
    }

    // Recognition has tried and failed to identify the current sound.
    void set_unrecognised(bool value) { unrecognised_ = value; }

    bool sound() const { return sound_; }
    bool calibrated() const { return calibration_ >= kCalibrationBlocks; }
    double background_db() const { return background_; }
    double level_db() const { return last_level_; }

private:
    Event block(double level, uint32_t hop_number, uint32_t &start_hop)
    {
        level = std::max(level, kLowestDb);
        if (calibration_ < kCalibrationBlocks) {
            background_ = calibration_ == 0 ? level : std::min(background_, level);
            ++calibration_;
            return Event::None;
        }
        const bool slow = sound_ && !unrecognised_;
        background_ = std::min(level, background_ + (slow ? kSoundRiseDbPerBlock : kQuietRiseDbPerBlock));
        if (!sound_) {
            loud_ = level >= background_ + kStartAboveDb ? loud_ + 1 : 0;
            if (loud_ < kStartBlocks) return Event::None;
            sound_ = true; quiet_ = 0;
            // Include the blocks that proved the sound plus one before them.
            const uint32_t back = (kStartBlocks + 1) * kBlockHops;
            start_hop = hop_number + 1 > back ? hop_number + 1 - back : 0;
            return Event::SoundStarted;
        }
        quiet_ = level < background_ + kStopAboveDb ? quiet_ + 1 : 0;
        if (quiet_ < kStopBlocks) return Event::None;
        sound_ = unrecognised_ = false; loud_ = 0;
        return Event::SilenceStarted;
    }

    double energy_ = 0;
    unsigned count_ = 0, block_hops_ = 0;
    unsigned calibration_ = 0, loud_ = 0, quiet_ = 0;
    double background_ = kLowestDb, last_level_ = kLowestDb;
    bool sound_ = false, unrecognised_ = false;
};

} // namespace listening
