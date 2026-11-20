#include "microphone_privacy.h"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <vector>
std::vector<unsigned char> saved, pending;
int init_error=0, open_error=0, read_error=0, write_error=0, commit_error=0;
int nvs_flash_init() { return init_error; }
int nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    assert(std::strcmp(name,"mic_privacy")==0 && mode==NVS_READWRITE); *handle=1; return open_error;
}
int nvs_get_blob(nvs_handle_t, const char *key, void *data, size_t *size) {
    assert(std::strcmp(key,"mute_v1")==0);
    if(read_error) return read_error;
    if(saved.empty()) return ESP_ERR_NVS_NOT_FOUND;
    if(saved.size()>*size) return -1;
    std::memcpy(data,saved.data(),saved.size());*size=saved.size();return ESP_OK;
}
int nvs_set_blob(nvs_handle_t, const char *key, const void *data, size_t size) {
    assert(std::strcmp(key,"mute_v1")==0);
    if(write_error) return write_error;
    auto *bytes=static_cast<const unsigned char *>(data);pending.assign(bytes,bytes+size);return ESP_OK;
}
int nvs_commit(nvs_handle_t) { if(commit_error)return commit_error;saved=pending;return ESP_OK; }
void nvs_close(nvs_handle_t) { pending.clear(); }
int main() {
    using namespace microphone_privacy;
    assert(initialize() && !muted() && !storage_error);
    Permit captured;
    assert(request(true) && muted() && !captured.valid());
    assert(initialize() && muted()); // Restart/OTA loads the same durable record.
    assert(request(false) && !muted() && !captured.valid());
    assert(initialize() && !muted());
    for(int *error : {&init_error,&open_error,&write_error,&commit_error}) {
        assert(request(false));
        *error=-1;
        assert(!request(true) && muted() && storage_error);
        assert(!request(false) && muted()); // Failed unmute cannot resume capture.
        *error=0;
        assert(request(true) && initialize() && muted() && !storage_error);
        assert(request(false) && initialize() && !muted());
    }
    for(int *error : {&init_error,&open_error,&read_error}) {
        *error=-1;assert(!initialize() && muted() && storage_error);*error=0;
    }
    for(auto invalid : {std::vector<unsigned char>{1},std::vector<unsigned char>{2,0},std::vector<unsigned char>{1,2}}) {
        saved=invalid;assert(!initialize() && muted());
    }
    assert(request(true) && initialize() && muted()); // Explicit repair without erasing NVS.
}
