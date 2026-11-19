#pragma once
#include "recognition.h"
#include "recognition_credentials.h"

namespace recognition_startup {
// Local workers always run, even when cloud configuration is absent or broken.
template<class Audio, class Presence, class Cloud>
void start(bool configured, Audio audio, Presence presence, Cloud cloud) {
    audio();
    presence();
    if (configured) cloud();
}
inline RecognitionStatus status(bool configured, RecognitionStatus requested) {
    return configured ? requested : RecognitionStatus::ConfigurationRequired;
}
inline bool trial_ready(bool local_started, recognition_credentials::State state) {
    // Missing is supported; storage corruption is not evidence of intentional
    // non-provisioning. Preserve the protection against lost credentials.
    return local_started && (state == recognition_credentials::State::Ready ||
        state == recognition_credentials::State::Missing);
}
}
