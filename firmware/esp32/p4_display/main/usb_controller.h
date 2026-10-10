#pragma once

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

void usb_controller_start(LedControllerStatusCallback callback);
StyleRequestResult usb_controller_set_style(LedStyle style);
