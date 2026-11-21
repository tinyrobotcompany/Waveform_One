#include "idle_display.h"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <vector>
std::vector<unsigned char> stored, pending;
bool namespace_exists = false;
int error = 0, commits = 0, init_error = 0, open_error = 0, read_error = 0, write_error = 0;
int nvs_flash_init() { return init_error; }
int nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    assert(std::strcmp(name, "wf_display") == 0);
    if (open_error) return open_error;
    if (!namespace_exists && mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
    if (mode == NVS_READWRITE) namespace_exists = true;
    *handle = 1;
    return 0;
}
int nvs_get_blob(nvs_handle_t, const char *key, void *out, size_t *size) {
    assert(std::strcmp(key, "idle_clock") == 0);
    if (read_error) return read_error;
    if (stored.empty()) return ESP_ERR_NVS_NOT_FOUND;
    assert(stored.size() <= *size);
    std::memcpy(out, stored.data(), stored.size()); *size = stored.size(); return 0;
}
int nvs_set_blob(nvs_handle_t, const char *, const void *value, size_t size) {
    if (write_error) return write_error;
    const auto *data = static_cast<const unsigned char *>(value);
    pending.assign(data, data + size); return 0;
}
int nvs_commit(nvs_handle_t) { ++commits; if (error) return error; stored = pending; return 0; }
void nvs_close(nvs_handle_t) { pending.clear(); }
int main() {
    using namespace idle_display;
    Config c;
    assert(load(c) && !c.enabled && commits==0);
    c.enabled=1; c.minutes=37; c.twelve_hour=1; c.large=0; c.accent=1;
    assert(save(c));
    Config reboot;
    assert(load(reboot) && reboot.minutes==37 && reboot.twelve_hour && !reboot.large && reboot.accent);
    error=-1; c.minutes=10; assert(!save(c));
    assert(load(reboot) && reboot.minutes==37);
    error=0; write_error=-1; assert(!save(c)); write_error=0;
    c.minutes=0; assert(!save(c));
    stored.resize(3); assert(!load(reboot) && !reboot.enabled);
    read_error=-1; assert(!load(reboot)); read_error=0;
    init_error=-1; assert(!load(reboot)); init_error=0;
    open_error=-1; assert(!load(reboot));
}
