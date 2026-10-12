#pragma once

#include <cstddef>
#include <cstdint>
#include "audio_activity.h"
#include "audio_stream.h"

enum class LedControllerState {
    Waiting,
    Connecting,
    ConnectedClassic,
    ConnectedMirrored,
    ConnectedWaterfall,
    ProtocolError,
    Disconnected,
};

enum class LedStyle { Classic, Mirrored, Waterfall };

enum class StyleRequestResult {
    Queued,
    Disconnected,
    Unavailable,
    QueueFull,
};

using LedControllerStatusCallback = void (*)(LedControllerState state);

using AudioActivityCallback = void (*)(audio_activity::State state);
void usb_controller_start(LedControllerStatusCallback callback, AudioActivityCallback activity = nullptr);
StyleRequestResult usb_controller_set_style(LedStyle style);

// Continuous recognition audio from the S3: about 16 seconds of history.
// Owned by the controller; readable from any task once started.
using RecognitionAudio = audio_stream::Ring<audio_stream::kSampleRate * 16>;
const RecognitionAudio *usb_controller_audio();
