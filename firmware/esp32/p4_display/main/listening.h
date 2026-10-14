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
inline constexpr unsigned kStopBlocks = 8;           // ...for 1.6 s, or
inline constexpr unsigned kShowingStopBlocks = 15;   // 3 s while a song is shown: quiet passages are not stops
inline constexpr unsigned kCalibrationBlocks = 10;   // first 2 s set the background...
// ...but never louder than a quiet room: music already playing at power-on
// must not become the background. A noisy room is learnt within seconds.
inline constexpr double kLoudestStartDb = -50;
// The background is the room's average quiet level, not its quietest moment:
// ordinary room noise wanders a few dB from block to block. It follows quieter
// levels quickly. It follows louder ones gradually while quiet (a fan turns
// on) but only very slowly while sound plays, so a long song cannot become the
// new background. (Unidentified sound is not absorbed either: music the
// service misses at first must not turn into "silence". Lookups for noise
// are limited by the planner's caution instead.)
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
        for (unsigned i = 0; i < samples; ++i) {
            energy_ += double(hop[i]) * hop[i];
            sum_ += hop[i];
            rails_ += hop[i] == INT16_MIN || hop[i] == INT16_MAX;
            zeros_ += hop[i] == 0;
        }
        count_ += samples;
        if (++block_hops_ < kBlockHops) return Event::None;
        const double level = 10 * std::log10(energy_ / count_ / (32768.0 * 32768.0) + 1e-12);
        quality_.mean = sum_ / count_;
        const double ac_power = std::max(0.0, energy_ / count_ - quality_.mean * quality_.mean);
        quality_.ac_db = 10 * std::log10(ac_power / (32768.0 * 32768.0) + 1e-12);
        quality_.rail_percent = 100.0 * rails_ / count_;
        quality_.zero_percent = 100.0 * zeros_ / count_;
        sum_ = 0; rails_ = zeros_ = 0;
        energy_ = 0; count_ = 0; block_hops_ = 0;
        last_level_ = level;
        return block(level, hop_number, start_hop);
    }

    // Audio stopped arriving or skipped: what follows is a new recording.
    void discontinuity()
    {
        energy_ = 0; count_ = 0; block_hops_ = 0;
        sum_ = 0; rails_ = zeros_ = 0; quality_ = {};
        loud_ = quiet_ = resumed_loud_ = 0; quiet_history_ = 0;
        sound_ = unrecognised_ = false;
    }

    // A song is on screen: a longer quiet is needed before it is cleared.
    void set_patient(bool value) { patient_ = value; }
    unsigned stop_blocks() const { return patient_ ? kShowingStopBlocks : kStopBlocks; }

    // Recognition has tried and failed to identify the current sound.
    void set_unrecognised(bool value) { unrecognised_ = value; }

    // The last kRecentBlocks block levels in dB, oldest first, for logs.
    static constexpr unsigned kRecentBlocks = 50;  // 10 s
    unsigned recent(float *out) const
    {
        const unsigned count = recent_count_ < kRecentBlocks ? recent_count_ : kRecentBlocks;
        for (unsigned i = 0; i < count; ++i) out[i] = recent_[(recent_count_ - count + i) % kRecentBlocks];
        return count;
    }

    // Quietest and loudest block since the last call, for tuning logs.
    struct Range { double low, high; };
    Range take_range() { const Range range{low_, high_}; low_ = 0; high_ = kLowestDb; return range; }

    uint32_t gap_end() const { return gap_end_; }
    bool sound() const { return sound_; }
    bool calibrated() const { return calibration_ >= kCalibrationBlocks; }
    double background_db() const { return background_; }
    double level_db() const { return last_level_; }
    // Diagnostics only: changing the detector's input requires a separate
    // validation. dBFS is digital level, not calibrated acoustic dB SPL.
    struct Quality { double mean = 0, ac_db = -120, rail_percent = 0, zero_percent = 0; };
    Quality quality() const { return quality_; }

private:
    Event block(double level, uint32_t hop_number, uint32_t &start_hop)
    {
        level = std::max(level, kLowestDb);
        low_ = std::min(low_, level); high_ = std::max(high_, level);
        recent_[recent_count_++ % kRecentBlocks] = float(level);
        if (calibration_ < kCalibrationBlocks) {
            // Average of the first blocks.
            background_ += (level - background_) / (calibration_ + 1);
            if (++calibration_ == kCalibrationBlocks) background_ = std::min(background_, kLoudestStartDb);
            return Event::None;
        }
        if (level < background_) background_ += (level - background_) * kFallFraction;
        else if (sound_) background_ = std::min(level, background_ + kSoundRiseDbPerBlock);
        else background_ += std::min(kQuietRiseDbPerBlock, (level - background_) * kQuietRiseFraction);
        if (!sound_) {
            loud_ = level >= background_ + kStartAboveDb ? loud_ + 1 : 0;
            if (loud_ < kStartBlocks) return Event::None;
            sound_ = true; quiet_ = resumed_loud_ = 0; quiet_history_ = 0;
            // Include the blocks that proved the sound plus one before them.
            const uint32_t back = (kStartBlocks + 1) * kBlockHops;
            start_hop = hop_number + 1 > back ? hop_number + 1 - back : 0;
            return Event::SoundStarted;
        }
        const bool quiet = level < background_ + kStopAboveDb;
        quiet_history_ = (quiet_history_ << 1) | (quiet ? 1u : 0u);
        if (!quiet) {
            // A confidently resumed passage starts a fresh quiet-vote window.
            // A single cough still does not erase genuine stop evidence.
            resumed_loud_ = std::min(resumed_loud_ + 1, kStartBlocks);
            if (resumed_loud_ == kStartBlocks) quiet_history_ = 0;
            const unsigned dip = quiet_;
            quiet_ = 0;
            if (dip < kGapBlocks) return Event::None;
            const uint32_t resumed = hop_number + 1 - kBlockHops;
            const uint32_t back = dip * kBlockHops;
            start_hop = resumed > back ? resumed - back : 0;
            gap_end_ = resumed;
            return Event::Gap;
        }
        resumed_loud_ = 0;
        ++quiet_;
        // Mostly quiet for the stop time: a creak or a cough in the room must
        // not restart the count.
        const unsigned window = stop_blocks();
        const uint32_t mask = window >= 32 ? 0xffffffffu : (1u << window) - 1;
        const unsigned quiet_blocks = unsigned(__builtin_popcount(quiet_history_ & mask));
        if (quiet_blocks * 5 < window * 4) return Event::None;
        sound_ = unrecognised_ = false; loud_ = 0; quiet_ = 0; quiet_history_ = 0;
        return Event::SilenceStarted;
    }

    double energy_ = 0;
    double sum_ = 0;
    unsigned rails_ = 0, zeros_ = 0;
    Quality quality_;
    unsigned count_ = 0, block_hops_ = 0;
    unsigned calibration_ = 0, loud_ = 0, quiet_ = 0, resumed_loud_ = 0;
    double background_ = kLowestDb, last_level_ = kLowestDb;
    double low_ = 0, high_ = kLowestDb;
    uint32_t gap_end_ = 0, quiet_history_ = 0;
    float recent_[kRecentBlocks]{};
    unsigned recent_count_ = 0;
    bool sound_ = false, unrecognised_ = false, patient_ = false;
};

} // namespace listening
