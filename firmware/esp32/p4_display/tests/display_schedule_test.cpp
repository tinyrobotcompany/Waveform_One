#include "display_schedule.h"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <vector>
std::vector<unsigned char> stored, pending;
int error = 0, commits = 0, init_error = 0, open_error = 0, read_error = 0, write_error = 0;
int nvs_flash_init() { return init_error; }
int nvs_open(const char *name, int, nvs_handle_t *handle) {
    assert(std::strcmp(name, "wf_display") == 0); *handle = 1; return open_error;
}
int nvs_get_blob(nvs_handle_t, const char *key, void *out, size_t *size) {
    assert(std::strcmp(key, "schedule") == 0);
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
    using namespace display_schedule;
    Config c;
    assert(load(c) && c.start == 1380 && commits == 0);
    open_error = ESP_ERR_NVS_NOT_FOUND; assert(load(c));
    open_error = -1; assert(!load(c)); open_error = 0;
    init_error = -1; assert(!load(c)); init_error = 0;
    read_error = -1; assert(!load(c)); read_error = 0;
    assert(!in_window(c, 1379) && in_window(c, 1380));
    assert(in_window(c, 0) && in_window(c, 419) && !in_window(c, 420));
    assert(brightness(c, 0, true, false, 80) == 10);
    assert(brightness(c, -1, true, false, 80) == 80); // No valid clock.
    assert(brightness(c, 0, false, false, 80) == 80); // Music restores brightness.
    assert(brightness(c, 0, true, true, 80) == 80); // Touch/manual override.
    c.percent = 50;
    assert(brightness(c, 0, true, false, 20) == 20); // Never brighten at night.
    c.enabled = 0;
    assert(brightness(c, 0, true, false, 80) == 80);
    c.enabled = 1; c.start = 540; c.end = 1020;
    assert(in_window(c, 540) && !in_window(c, 1020) && !in_window(c, 0));
    assert(save(c));
    Config rebooted;
    assert(load(rebooted) && rebooted.start == 540 && rebooted.percent == 50);
    write_error = -1; c.percent = 20;
    assert(!save(c));
    assert(load(rebooted) && rebooted.percent == 50);
    write_error = 0;
    error = -1; c.percent = 20;
    assert(!save(c));
    assert(load(rebooted) && rebooted.percent == 50);
    error = 0; c.start = c.end;
    assert(!save(c));
    stored.resize(3);
    assert(!load(rebooted) && rebooted.start == 1380);
    c = Config{}; c.end = 1440; assert(!c.valid());
    c = Config{}; c.start = 1; assert(!c.valid());
    c = Config{}; c.version = 2; assert(!c.valid());
}
