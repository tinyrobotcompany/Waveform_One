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
inline constexpr double kStopAboveDb = 5;            // silence: within 5 dB of background...
inline constexpr unsigned kStopBlocks = 8;           // ...for 1.6 s
inline constexpr unsigned kCalibrationBlocks = 10;   // first 2 s set the background
// The background is the room's average quiet level, not its quietest moment:
// ordinary room noise wanders a few dB from block to block. It follows quieter
// levels quickly. It follows louder ones gradually while quiet (a fan turns
// on) but only very slowly while sound plays, so a long song cannot become the
// new background. Sound that recognition has repeatedly failed to identify is
// treated like quiet: a fan or a dishwasher must not be "music" for minutes.
inline constexpr double kFallFraction = 0.2;           // per block, towards quieter levels
inline constexpr double kQuietRiseFraction = 0.05;     // per block, towards louder levels...
inline constexpr double kQuietRiseDbPerBlock = 0.25;   // ...at most 1.25 dB a second
inline constexpr double kSoundRiseDbPerBlock = 0.0033; // 1 dB a minute
inline constexpr double kLowestDb = -90;

inline constexpr unsigned kGapBlocks = 2;            // a 0.4 s dip is a gap between tracks

// Gap: sound dipped to the background briefly (at least 0.4 s, less than
// silence) and has resumed; start_hop is where the dip began and
// gap_end() where sound returned.
enum class Event { None, SoundStarted, SilenceStarted, Gap };

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

    // Quietest and loudest block since the last call, for tuning logs.
    struct Range { double low, high; };
    Range take_range() { const Range range{low_, high_}; low_ = 0; high_ = kLowestDb; return range; }

    uint32_t gap_end() const { return gap_end_; }
    bool sound() const { return sound_; }
    bool calibrated() const { return calibration_ >= kCalibrationBlocks; }
    double background_db() const { return background_; }
    double level_db() const { return last_level_; }

private:
    Event block(double level, uint32_t hop_number, uint32_t &start_hop)
    {
        level = std::max(level, kLowestDb);
        low_ = std::min(low_, level); high_ = std::max(high_, level);
        if (calibration_ < kCalibrationBlocks) {
            // Average of the first blocks.
            background_ += (level - background_) / (calibration_ + 1);
            ++calibration_;
            return Event::None;
        }
        if (level < background_) background_ += (level - background_) * kFallFraction;
        else if (sound_ && !unrecognised_) background_ = std::min(level, background_ + kSoundRiseDbPerBlock);
        else background_ += std::min(kQuietRiseDbPerBlock, (level - background_) * kQuietRiseFraction);
        if (!sound_) {
            loud_ = level >= background_ + kStartAboveDb ? loud_ + 1 : 0;
            if (loud_ < kStartBlocks) return Event::None;
            sound_ = true; quiet_ = 0;
            // Include the blocks that proved the sound plus one before them.
            const uint32_t back = (kStartBlocks + 1) * kBlockHops;
            start_hop = hop_number + 1 > back ? hop_number + 1 - back : 0;
            return Event::SoundStarted;
        }
        if (level >= background_ + kStopAboveDb) {
            const unsigned dip = quiet_;
            quiet_ = 0;
            if (dip < kGapBlocks) return Event::None;
            const uint32_t resumed = hop_number + 1 - kBlockHops;
            const uint32_t back = dip * kBlockHops;
            start_hop = resumed > back ? resumed - back : 0;
            gap_end_ = resumed;
            return Event::Gap;
        }
        if (++quiet_ < kStopBlocks) return Event::None;
        sound_ = unrecognised_ = false; loud_ = 0;
        return Event::SilenceStarted;
    }

    double energy_ = 0;
    unsigned count_ = 0, block_hops_ = 0;
    unsigned calibration_ = 0, loud_ = 0, quiet_ = 0;
    double background_ = kLowestDb, last_level_ = kLowestDb;
    double low_ = 0, high_ = kLowestDb;
    uint32_t gap_end_ = 0;
    bool sound_ = false, unrecognised_ = false;
};

} // namespace listening
