#pragma once

#include <ctime>

#include "usb_controller.h"

enum class NetworkMode {
    Unconfigured,
    HomeWifi,
};

struct RemoteCallbacks {
    void (*set_style)(LedStyle style) = nullptr;
    void (*set_brightness)(int percent) = nullptr;
    void (*set_name)(const char *name) = nullptr;
    void (*set_time)(std::time_t epoch) = nullptr;
    void (*set_network)(NetworkMode mode, const char *address) = nullptr;
    void (*set_networks)(const char *options) = nullptr;
};

void network_start(const RemoteCallbacks &callbacks);
void network_request_scan();
bool network_configure_home(const char *ssid, const char *password);
