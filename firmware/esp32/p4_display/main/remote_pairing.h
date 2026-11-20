#pragma once
#include "remote_auth.h"

namespace remote_auth {
enum class PairResult { Paired, InvalidCode, StorageUnavailable };
// Commit the candidate before granting access. Failure leaves existing phones
// and the displayed QR intact. The caller serializes access to auth and storage.
template<class Save>
PairResult pair_saved(RemoteAuth &auth, std::string_view host, std::string_view code,
                      int64_t now_us, int64_t epoch, char (&id)[kTokenSize], Save save)
{
    auto candidate = auth;
    char candidate_id[kTokenSize]{};
    if (!candidate.pair(host, code, now_us, candidate_id)) return PairResult::InvalidCode;
    StoredSessions stored{};
    if (!candidate.export_sessions(stored, now_us, epoch) || !save(stored))
        return PairResult::StorageUnavailable;
    auth = candidate;
    std::memcpy(id, candidate_id, kTokenSize);
    return PairResult::Paired;
}
template<class Save>
bool forget_saved(RemoteAuth &auth, int64_t now_us, Save save)
{
    // An empty record revokes all phones even when offline or without a clock.
    if (!save(StoredSessions{})) return false;
    auth.forget_sessions(now_us);
    return true;
}
}
