#pragma once
#include "audio_pipeline.h"

// Room-listening profile. Keep mic_test's reference defaults unchanged.
inline audio::Config visualizerConfig()
{
    audio::Config config;
    config.minimumOpenRms = 0.00010f;
    config.minimumCloseRms = 0.00007f;
    config.openNoiseRatio = 1.25f;
    config.closeNoiseRatio = 1.10f;
    config.closeHoldSeconds = 0.40f;
    config.displayFloorDb = -80.0f;
    config.displayCeilingDb = -35.0f;
    config.displayReleaseSeconds = 0.25f;
    // Retain spectral noise subtraction and raw-spectrum flux detection.
    // No adaptive gain; quiet-room noise must not grow towards full height.
    return config;
}
