#include "recognition_credentials.h"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <vector>
using recognition_credentials::Record;
std::vector<unsigned char> stored, pending;
int writes = 0, init_error = 0, write_error = 0, commit_error = 0;
int nvs_flash_init() { return init_error; }
int nvs_open(const char *name, int, nvs_handle_t *handle) {
    assert(std::strcmp(name, "recognition") == 0); *handle = 1; return 0;
}
int nvs_get_blob(nvs_handle_t, const char *key, void *value, size_t *size) {
    assert(std::strcmp(key, "acrcloud") == 0);
    if (stored.empty() || stored.size() > *size) return -1;
    std::memcpy(value, stored.data(), stored.size()); *size = stored.size(); return 0;
}
int nvs_set_blob(nvs_handle_t, const char *, const void *value, size_t size) {
    ++writes; if (write_error) return write_error;
    const auto *bytes = static_cast<const unsigned char *>(value);
    pending.assign(bytes, bytes + size); return 0;
}
int nvs_commit(nvs_handle_t) { if (commit_error) return commit_error; stored = pending; return 0; }
void nvs_close(nvs_handle_t) { pending.clear(); }
int main() {
    Record output; bool persisted;
    auto ota = [&] { return recognition_credentials::load(output, "", "", "", persisted); };
    assert(!ota() && !persisted); // Missing credentials never select another provider.
    assert(recognition_credentials::load(output, "identify.example.com", "test-key", "test-secret", persisted));
    assert(persisted && writes == 1);
    assert(ota() && persisted && std::strcmp(output.key, "test-key") == 0);
    assert(recognition_credentials::load(output, "identify.example.com", "test-key", "test-secret", persisted));
    assert(writes == 1); // No flash wear on repeated bootstrap boots.
    assert(!recognition_credentials::load(output, "identify.example.com", "", "test-secret", persisted));
    assert(ota() && persisted); // Invalid provisioning did not destroy stored credentials.
    commit_error = -1;
    assert(recognition_credentials::load(output, "identify.example.com", "new-key", "new-secret", persisted));
    assert(!persisted && std::strcmp(output.key, "new-key") == 0);
    assert(ota() && std::strcmp(output.key, "test-key") == 0);
    commit_error = 0; write_error = -1;
    assert(recognition_credentials::load(output, "identify.example.com", "new-key", "new-secret", persisted));
    assert(!persisted);
    write_error = 0;
    const auto saved = stored;
    stored.resize(12); assert(!ota());
    stored = saved; stored[0] = 2; assert(!ota());
    stored = saved; std::memset(stored.data() + 4, 'a', 256); assert(!ota());
    stored = saved; init_error = -1; assert(!ota());
    assert(stored == saved); // Storage errors must never erase existing credentials.
}
