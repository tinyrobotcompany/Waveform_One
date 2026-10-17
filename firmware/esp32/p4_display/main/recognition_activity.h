#pragma once
#include "audio_presence.h"
#include "recognition_policy.h"

namespace recognition_activity {
// Continuous recognition treats arriving PCM as activity. Acoustic level
// votes remain diagnostic; network and stream continuity still invalidate work.
inline bool received(recognition_policy::Sessions &sessions, audio_presence::Snapshot &state,
                     uint64_t position, uint32_t segment, bool skipped) {
    const bool reset = state.segment != segment || skipped;
    if (reset) sessions.invalidate_audio();
    if (reset || !state.sound) state.start_sample = position;
    state.segment = segment; state.sound = true; state.quiet_sample = 0;
    const bool started = sessions.set_sound(true);
    return reset || started;
}
inline void missing(recognition_policy::Sessions &sessions, audio_presence::Snapshot &state) {
    sessions.invalidate_audio();
    state.sound = false;
}
}
