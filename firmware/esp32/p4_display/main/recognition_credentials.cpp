#include "recognition_credentials.h"
#include <cstring>
#include <mutex>
#include "nvs.h"
#include "nvs_flash.h"

namespace recognition_credentials {
namespace {
std::mutex storage_mutex;
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
namespace {
OwnerState read_owner(Record &output) {
    output = Record{};
    if (nvs_flash_init() != ESP_OK) return OwnerState::Error;
    nvs_handle_t handle;
    const auto opened = nvs_open("recognition", NVS_READONLY, &handle);
    if (opened == ESP_ERR_NVS_NOT_FOUND) return OwnerState::Unset;
    if (opened != ESP_OK) return OwnerState::Error;
    size_t size = sizeof(output);
    const auto read = nvs_get_blob(handle, "owner_acr", &output, &size);
    nvs_close(handle);
    if (read == ESP_ERR_NVS_NOT_FOUND) return OwnerState::Unset;
    if (read != ESP_OK || size != sizeof(output)) return OwnerState::Error;
    Record disabled;
    disabled.version = 0;
    if (std::memcmp(&output, &disabled, sizeof(output)) == 0) return OwnerState::Disabled;
    return output.valid() ? OwnerState::Configured : OwnerState::Error;
}
bool write_owner(const Record &record) {
    if (nvs_flash_init() != ESP_OK) return false;
    nvs_handle_t handle;
    if (nvs_open("recognition", NVS_READWRITE, &handle) != ESP_OK) return false;
    const bool saved = nvs_set_blob(handle, "owner_acr", &record, sizeof(record)) == ESP_OK &&
        nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return saved;
}
}
OwnerState owner_state() {
    const std::lock_guard<std::mutex> lock(storage_mutex);
    Record value;
    return read_owner(value);
}
bool owner_record(Record &output, const char *host, const char *key, const char *secret) {
    output = Record{};
    Record value;
    if (!copy_field(value.host, sizeof(value.host), host) ||
        !copy_field(value.key, sizeof(value.key), key) ||
        !copy_field(value.secret, sizeof(value.secret), secret)) return false;
    // Only ACRCloud regional identification hosts; no arbitrary upload destination.
    constexpr char suffix[] = ".acrcloud.com";
    const size_t length = std::strlen(value.host);
    if (length <= sizeof(suffix) - 1 ||
        std::strcmp(value.host + length - (sizeof(suffix) - 1), suffix) != 0) return false;
    for (const char *c = value.host; *c; ++c) {
        if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '.')) return false;
        if (*c == '.' && (c == value.host || c[-1] == '.')) return false;
    }
    output = value;
    return true;
}
bool save_owner(const Record &record) {
    Record checked;
    if (record.version != 1 || !record.valid() ||
        !owner_record(checked, record.host, record.key, record.secret)) return false;
    const std::lock_guard<std::mutex> lock(storage_mutex);
    return write_owner(checked);
}
bool disable_owner() {
    const std::lock_guard<std::mutex> lock(storage_mutex);
    Record disabled;
    disabled.version = 0;
    return write_owner(disabled);
}
bool load(Record &output, const char *host, const char *key, const char *secret,
          bool &persisted, State *state) {
    const std::lock_guard<std::mutex> lock(storage_mutex);
    if (state) *state = State::Error;
    output = Record{};
    persisted = false;
    Record owner;
    const auto selected = read_owner(owner);
    if (selected == OwnerState::Configured) {
        output = owner;
        persisted = true;
        if (state) *state = State::Ready;
        return true;
    }
    if (selected == OwnerState::Disabled) {
        if (state) *state = State::Missing;
        return false;
    }
    // A failed read cannot safely choose a potentially superseded bootstrap key.
    if (selected == OwnerState::Error) return false;
    Record embedded;
    const bool has_embedded = copy_field(embedded.host, sizeof(embedded.host), host) &&
        copy_field(embedded.key, sizeof(embedded.key), key) &&
        copy_field(embedded.secret, sizeof(embedded.secret), secret);
    // A partial bootstrap must not silently retain a different configuration.
    const bool any_embedded = (host && *host) || (key && *key) || (secret && *secret);
    if (any_embedded && !has_embedded) return false;
    if (has_embedded) {
        output = embedded;
        if (state) *state = State::Unpersisted;
    }
    if (nvs_flash_init() != ESP_OK) return has_embedded; // Never erase NVS.
    nvs_handle_t handle;
    const auto opened = nvs_open("recognition", has_embedded ? NVS_READWRITE : NVS_READONLY, &handle);
    if (opened != ESP_OK) {
        if (!has_embedded && opened == ESP_ERR_NVS_NOT_FOUND && state) *state = State::Missing;
        return has_embedded;
    }
    Record saved;
    size_t size = sizeof(saved);
    const auto read = nvs_get_blob(handle, "acrcloud", &saved, &size);
    const bool found = read == ESP_OK &&
        size == sizeof(saved) && saved.valid();
    if (has_embedded) {
        persisted = found && std::memcmp(&saved, &embedded, sizeof(saved)) == 0;
        if (!persisted) persisted = nvs_set_blob(handle, "acrcloud", &embedded, sizeof(embedded)) == ESP_OK &&
            nvs_commit(handle) == ESP_OK;
    } else if (found) {
        output = saved;
        persisted = true;
    } else if (read == ESP_ERR_NVS_NOT_FOUND && state) {
        *state = State::Missing;
    }
    if (persisted && state) *state = State::Ready;
    nvs_close(handle);
    return output.valid();
}
}
