#pragma once
#include "recognition.h"

namespace recognition_delivery {
// Owned under the session mutex. Payload is empty, so retries never retain
// borrowed track strings or artwork pixels past their lifetime.
class EmptyUpdate {
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
}
