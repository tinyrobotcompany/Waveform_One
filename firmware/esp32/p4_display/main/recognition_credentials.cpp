#include "recognition_credentials.h"
#include <cstring>
#include "nvs.h"
#include "nvs_flash.h"

namespace recognition_credentials {
namespace {
bool field_valid(const char *value, size_t capacity) {
    const size_t length = strnlen(value, capacity);
    if (!length || length == capacity) return false;
    for (size_t i = 0; i < length; ++i) {
        const char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}
bool copy_field(char *output, size_t capacity, const char *input) {
    if (!input || !field_valid(input, capacity)) return false;
    std::memcpy(output, input, std::strlen(input) + 1);
    return true;
}
}
bool Record::valid() const {
    return version == 1 && field_valid(host, sizeof(host)) &&
        field_valid(key, sizeof(key)) && field_valid(secret, sizeof(secret));
}
bool load(Record &output, const char *host, const char *key, const char *secret,
          bool &persisted) {
    output = Record{};
    persisted = false;
    Record embedded;
    const bool has_embedded = copy_field(embedded.host, sizeof(embedded.host), host) &&
        copy_field(embedded.key, sizeof(embedded.key), key) &&
        copy_field(embedded.secret, sizeof(embedded.secret), secret);
    // A partial bootstrap must not silently retain a different configuration.
    const bool any_embedded = (host && *host) || (key && *key) || (secret && *secret);
    if (any_embedded && !has_embedded) return false;
    if (has_embedded) output = embedded;
    if (nvs_flash_init() != ESP_OK) return has_embedded; // Never erase NVS.
    nvs_handle_t handle;
    if (nvs_open("recognition", has_embedded ? NVS_READWRITE : NVS_READONLY, &handle) != ESP_OK)
        return has_embedded;
    Record saved;
    size_t size = sizeof(saved);
    const bool found = nvs_get_blob(handle, "acrcloud", &saved, &size) == ESP_OK &&
        size == sizeof(saved) && saved.valid();
    if (has_embedded) {
        persisted = found && std::memcmp(&saved, &embedded, sizeof(saved)) == 0;
        if (!persisted) persisted = nvs_set_blob(handle, "acrcloud", &embedded, sizeof(embedded)) == ESP_OK &&
            nvs_commit(handle) == ESP_OK;
    } else if (found) {
        output = saved;
        persisted = true;
    }
    nvs_close(handle);
    return output.valid();
}
}
