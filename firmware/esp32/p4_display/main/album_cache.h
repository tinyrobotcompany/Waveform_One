#pragma once
#include "album_sequence.h"

namespace album_cache {
// Metadata only: callers must supply a fresh recognition result. No clock or
// track-order prediction can select a song from this cache.
struct Album {
    album_sequence::Sequence sequence;
    album_resolver::Context context;
    char name[513]{};
    char artwork_url[2049]{};

    bool apply(RecognitionTrack &verified, uint64_t collection_id, int64_t now) const {
        if (context.collection_id != collection_id || !context.fresh(now)) return false;
        const auto title = album_resolver::words(verified.title);
        const auto artist = album_resolver::lead_artist(verified.artist);
        if (title.empty() || artist.empty()) return false;
        int found = -1;
        for (unsigned i = 0; i < sequence.count; ++i) {
            const auto &song = sequence.tracks[i];
            // Preserve version qualifiers: a live recording must not reuse
            // the studio recording's cached release just because titles agree.
            if (album_resolver::words(song.title) != title ||
                album_resolver::lead_artist(song.artist) != artist) continue;
            if (found >= 0) return false; // duplicate titles are ambiguous
            found = int(i);
        }
        if (found < 0) return false;
        std::memcpy(verified.album, name, sizeof(name));
        std::memcpy(verified.artwork_url, artwork_url, sizeof(artwork_url));
        verified.duration_s = sequence.tracks[found].duration_s;
        return true;
    }
};
}
