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

using LedControllerStatusCallback = void (*)(LedControllerState state);

void usb_controller_start(LedControllerStatusCallback callback);
bool usb_controller_set_style(LedStyle style);
