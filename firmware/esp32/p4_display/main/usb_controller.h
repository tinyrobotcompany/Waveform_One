#pragma once

#include <cstddef>
#include <cstdint>

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

using AudioActivityCallback = void (*)(bool active);
void usb_controller_start(LedControllerStatusCallback callback, AudioActivityCallback activity = nullptr);
StyleRequestResult usb_controller_set_style(LedStyle style);

enum class AudioCaptureStatus {
    Complete, Busy, AudioLost, Invalid, TimedOut, Disconnected, NoMemory, TransportError,
};
enum class AudioCaptureRequestResult { Queued, Busy, Disconnected, Unavailable, QueueFull };
// Called on the controller task. Do not block or perform HTTP here. On success
// ownership of the 256,000-byte PCM buffer passes to the callback; release it
// with heap_caps_free. Failure always supplies nullptr and zero bytes.
// capture_id is returned unchanged, including on failure and disconnect.
using AudioCaptureCallback = void (*)(AudioCaptureStatus status, uint8_t *pcm, size_t bytes, uint64_t capture_id);
AudioCaptureRequestResult usb_controller_capture(AudioCaptureCallback callback, uint64_t capture_id = 0);
