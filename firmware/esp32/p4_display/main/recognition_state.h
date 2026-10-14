#pragma once
#include "recognition_evidence.h"

namespace recognition_state {
enum class Outcome { Matched, NoMatch, Failed };
enum class Reason { Current, Inactive, TransportChanged, StreamChanged, ActivityChanged, OldAudio };

struct Context {
    bool acr = false, ready = false;
    uint64_t request_epoch = 0, current_epoch = 0;
    uint64_t request_transport = 0, current_transport = 0;
    uint32_t request_segment = 0, stream_segment = 0, presence_segment = 0;
    uint64_t clip_first = 0, sound_start = 0;
};

// Match identity and acoustic activity are different observations. A miss or
// failure in the current session does not prove silence or a changed song.
// The caller handles service backoff before this presentation-only decision.
inline Reason evaluate(Outcome outcome, recognition_evidence::Coverage region, const Context &c) {
    if (!c.ready) return Reason::Inactive;
    if (c.request_transport != c.current_transport) return Reason::TransportChanged;
    if (c.request_segment != c.stream_segment || c.request_segment != c.presence_segment)
        return Reason::StreamChanged;
    if (c.current_epoch != c.request_epoch) {
        if (!c.acr || outcome != Outcome::Matched ||
            !recognition_evidence::retained_after_activity_change(region, c.ready,
                c.request_transport == c.current_transport,
                c.request_segment == c.presence_segment, c.sound_start)) return Reason::ActivityChanged;
    }
    if (c.acr && outcome == Outcome::Matched && c.clip_first < c.sound_start &&
        (!region.valid || region.last <= c.sound_start)) return Reason::OldAudio;
    return Reason::Current;
}

inline const char *name(Reason reason) {
    switch (reason) {
    case Reason::Current: return "current";
    case Reason::Inactive: return "inactive";
    case Reason::TransportChanged: return "transport_changed";
    case Reason::StreamChanged: return "stream_changed";
    case Reason::ActivityChanged: return "activity_changed";
    case Reason::OldAudio: return "old_audio";
    }
    return "invalid";
}
}
