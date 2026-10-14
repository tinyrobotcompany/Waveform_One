#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include "recognition.h"

namespace recognition_evidence {
struct Coverage {
    bool valid = false;
    uint64_t first = 0, last = 0;
};
// The service's match coordinates belong to the uploaded clip, not to the
// time the HTTP response arrives. Invalid/missing coordinates prove nothing
// about whether music is still playing.
inline Coverage coverage(const RecognitionTrack &track, uint64_t first, uint64_t last) {
    if (last <= first || !std::isfinite(track.sample_begin_ms) || !std::isfinite(track.sample_end_ms)
        || track.sample_begin_ms < 0 || track.sample_end_ms <= track.sample_begin_ms) return {};
    const double begin = track.sample_begin_ms * 16;
    const double end = track.sample_end_ms * 16;
    if (end > 8 * 16000 || end > double(last - first)) return {};
    return {true, first + uint64_t(begin), first + uint64_t(end)};
}
inline bool retained_after_activity_change(Coverage region, bool ready, bool same_transport,
                                          bool same_segment, uint64_t resumed_at) {
    return ready && same_transport && same_segment && region.valid && region.last > resumed_at;
}

}
