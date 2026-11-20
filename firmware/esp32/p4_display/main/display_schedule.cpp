#include "display_schedule.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace display_schedule {
bool load(Config &output) {
    output = Config{};
    if (nvs_flash_init() != ESP_OK) return false;
    nvs_handle_t handle;
    const auto opened = nvs_open("wf_display", NVS_READONLY, &handle);
    if (opened == ESP_ERR_NVS_NOT_FOUND) return true;
    if (opened != ESP_OK) return false;
    Config value{};
    size_t size = sizeof(value);
    const auto read = nvs_get_blob(handle, "schedule", &value, &size);
    nvs_close(handle);
    if (read == ESP_ERR_NVS_NOT_FOUND) return true;
    if (read != ESP_OK || size != sizeof(value) || !value.valid()) return false;
    output = value;
    return true;
}
bool save(const Config &value) {
    if (!value.valid()) return false;
    nvs_handle_t handle;
    if (nvs_open("wf_display", NVS_READWRITE, &handle) != ESP_OK) return false;
    const bool saved = nvs_set_blob(handle, "schedule", &value, sizeof(value)) == ESP_OK &&
        nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return saved;
}
}
