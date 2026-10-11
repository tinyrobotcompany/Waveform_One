#pragma once
#include "esp_wifi_remote_version.h"

// Wi-Fi remote >=1.3.1 consumes RX buffers on every return path, including
// rejection before a netif callback is installed. ESP-Hosted 1.4.7 predates
// that contract and otherwise frees the same buffer again on failure.
static inline int esp_hosted_rx_caller_frees(int result) {
#if ESP_WIFI_REMOTE_VERSION >= ESP_WIFI_REMOTE_VERSION_VAL(1,3,1)
    (void)result;
    return 0;
#else
    return result != 0;
#endif
}
