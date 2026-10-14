#include "hosted_rx_ownership.h"
#include <cassert>
#include <cstdlib>
#include <initializer_list>

int main() {
    for (int result : {0, -1, 1}) {
        auto *buffer = std::malloc(128);
        assert(buffer);
        int releases = 0;
#if ESP_WIFI_REMOTE_VERSION >= ESP_WIFI_REMOTE_VERSION_VAL(1,3,1)
        // The actual IDF 6.1 callback consumes even rejected/disconnected RX.
        std::free(buffer); ++releases;
#else
        if (result == 0) { std::free(buffer); ++releases; }
#endif
        if (esp_hosted_rx_caller_frees(result)) { std::free(buffer); ++releases; }
        assert(releases == 1);
    }
}
