#include "microphone_privacy.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <mutex>
namespace microphone_privacy {
namespace { std::mutex storage_mutex; }
bool initialize() {
    std::lock_guard<std::mutex> lock(storage_mutex);
    set_muted(true); // Fail closed until the saved setting has been checked.
    storage_error.store(true);
    if (nvs_flash_init() != ESP_OK) return false; // Never erase settings.
    nvs_handle_t handle;
    if (nvs_open("mic_privacy", NVS_READWRITE, &handle) != ESP_OK) return false;
    uint8_t record[2]{}; size_t length = sizeof(record);
    const auto result = nvs_get_blob(handle, "mute_v1", record, &length);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) set_muted(false); // New device: normal listening.
    else if (result == ESP_OK && length == sizeof(record) && record[0] == 1 && record[1] <= 1)
        set_muted(record[1] != 0);
    else return false;
    storage_error.store(false);
    return true;
}
bool request(bool mute) {
    std::lock_guard<std::mutex> lock(storage_mutex);
    if (mute) set_muted(true); // Inhibit immediately even if persistence fails.
    nvs_handle_t handle;
    bool saved = false;
    if (nvs_flash_init() == ESP_OK && nvs_open("mic_privacy", NVS_READWRITE, &handle) == ESP_OK) {
        const uint8_t record[]{1, static_cast<uint8_t>(mute)};
        saved = nvs_set_blob(handle, "mute_v1", record, sizeof(record)) == ESP_OK && nvs_commit(handle) == ESP_OK;
        nvs_close(handle);
    }
    storage_error.store(!saved);
    if (saved) set_muted(mute); // Never resume before the unmute is durable.
    else set_muted(true);
    return saved;
}
}
void microphone_privacy_start() { microphone_privacy::initialize(); }
