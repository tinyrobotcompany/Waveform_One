#pragma once

#include <ctime>

#include "control_policy.h"
#include "usb_controller.h"
#include "recognition.h"

enum class NetworkMode {
    Unconfigured,
    Reconnecting,
    HomeWifi,
};

enum class WifiConfigurationState {
    Connected,
    Failed,
    StorageRecoveryRequired,
};

struct RemoteCallbacks {
    bool (*set_style)(LedStyle style) = nullptr;
    bool (*set_brightness)(int percent) = nullptr;
    void (*set_name)(const char *name) = nullptr;
    void (*set_time)(std::time_t epoch) = nullptr;
    void (*set_network)(NetworkMode mode, const char *address) = nullptr;
    void (*set_networks)(const control_policy::WifiNetworkList &networks) = nullptr;
    void (*set_wifi_configuration)(WifiConfigurationState state) = nullptr;
};

void network_start(const RemoteCallbacks &callbacks);
void network_request_scan();
void network_set_recognition(RecognitionStatus status, const RecognitionTrack *track);
void network_set_brightness(int percent);
void network_set_name(const char *name);
void network_set_style(LedStyle style);
bool network_configure_home(const control_policy::WifiNetwork &network, const char *password);
