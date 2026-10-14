#pragma once
#include "recognition.h"

namespace recognition_delivery {
// Queues a status/epoch, never borrowed track strings or artwork pixels.
// The caller supplies its currently owned payload when applying the update.
// Access must be confined to one task or protected by the session mutex.
class StatusUpdate {
public:
    void request(uint64_t epoch, RecognitionStatus status) { epoch_ = epoch; status_ = status; pending_ = true; }
    void cancel() { pending_ = false; }
    bool pending() const { return pending_; }
    uint64_t epoch() const { return epoch_; }
    RecognitionStatus status() const { return status_; }
    template<class Apply> bool flush(uint64_t current_epoch, Apply apply) {
        if (!pending_) return false;
        if (current_epoch != epoch_) { cancel(); return false; }
        if (!apply(status_)) return false;
        cancel(); return true;
    }
private:
    uint64_t epoch_ = 0;
    RecognitionStatus status_ = RecognitionStatus::Waiting;
    bool pending_ = false;
};
using EmptyUpdate = StatusUpdate;
}
