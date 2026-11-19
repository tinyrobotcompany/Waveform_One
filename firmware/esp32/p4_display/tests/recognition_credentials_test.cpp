#include "recognition_credentials.h"
#include "recognition_startup.h"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <vector>
using recognition_credentials::Record;
std::vector<unsigned char> stored, pending, owner, pending_owner;
int writes = 0, init_error = 0, write_error = 0, commit_error = 0, open_error = 0, read_error = 0;
int nvs_flash_init() { return init_error; }
int nvs_open(const char *name, int, nvs_handle_t *handle) {
    assert(std::strcmp(name, "recognition") == 0); *handle = 1; return open_error;
}
int nvs_get_blob(nvs_handle_t, const char *key, void *value, size_t *size) {
    const auto &data = std::strcmp(key, "owner_acr") == 0 ? owner : stored;
    if (read_error) return read_error;
    if (data.empty()) return ESP_ERR_NVS_NOT_FOUND;
    if (data.size() > *size) return -1;
    std::memcpy(value, data.data(), data.size()); *size = data.size(); return 0;
}
int nvs_set_blob(nvs_handle_t, const char *key, const void *value, size_t size) {
    ++writes; if (write_error) return write_error;
    const auto *bytes = static_cast<const unsigned char *>(value);
    auto &target = std::strcmp(key, "owner_acr") == 0 ? pending_owner : pending;
    target.assign(bytes, bytes + size); return 0;
}
int nvs_commit(nvs_handle_t) { if (commit_error) return commit_error; if (!pending.empty()) stored = pending; if (!pending_owner.empty()) owner = pending_owner; return 0; }
void nvs_close(nvs_handle_t) { pending.clear(); pending_owner.clear(); }
int main() {
    Record output; bool persisted;
    using recognition_credentials::State;
    State state;
    auto ota = [&] {
        const bool configured = recognition_credentials::load(output, "", "", "", persisted, &state);
        std::vector<int> started;
        recognition_startup::start(configured, [&] { started.push_back(1); },
            [&] { started.push_back(2); }, [&] { started.push_back(3); });
        assert(started == (configured ? std::vector<int>{1, 2, 3} : std::vector<int>{1, 2}));
        assert(!recognition_startup::trial_ready(false, state));
        return configured;
    };
    assert(!ota() && !persisted); // Missing credentials never select another provider.
    assert(state == State::Missing && recognition_startup::trial_ready(true, state));
    open_error = ESP_ERR_NVS_NOT_FOUND;
    assert(!ota() && state == State::Missing); // No namespace on a new device.
    open_error = -1;
    assert(!ota() && state == State::Error && !recognition_startup::trial_ready(true, state));
    open_error = 0;
    read_error = -1;
    assert(!ota() && state == State::Error);
    read_error = 0;
    for (auto requested : {RecognitionStatus::Waiting, RecognitionStatus::Capturing,
            RecognitionStatus::Identifying, RecognitionStatus::Unavailable}) {
        assert(recognition_startup::status(false, requested) == RecognitionStatus::ConfigurationRequired);
        assert(recognition_startup::status(true, requested) == requested);
    }
    assert(recognition_credentials::load(output, "identify.example.com", "test-key", "test-secret", persisted));
    assert(persisted && writes == 1);
    assert(ota() && persisted && std::strcmp(output.key, "test-key") == 0);
    assert(state == State::Ready && recognition_startup::trial_ready(true, state));
    assert(recognition_credentials::load(output, "identify.example.com", "test-key", "test-secret", persisted));
    assert(writes == 1); // No flash wear on repeated bootstrap boots.
    assert(!recognition_credentials::load(output, "identify.example.com", "", "test-secret", persisted, &state));
    assert(state == State::Error);
    assert(ota() && persisted); // Invalid provisioning did not destroy stored credentials.
    commit_error = -1;
    assert(recognition_credentials::load(output, "identify.example.com", "new-key", "new-secret", persisted, &state));
    assert(state == State::Unpersisted && !recognition_startup::trial_ready(true, state));
    assert(!persisted && std::strcmp(output.key, "new-key") == 0);
    assert(ota() && std::strcmp(output.key, "test-key") == 0);
    commit_error = 0; write_error = -1;
    assert(recognition_credentials::load(output, "identify.example.com", "new-key", "new-secret", persisted, &state));
    assert(state == State::Unpersisted && !recognition_startup::trial_ready(true, state));
    assert(!persisted);
    write_error = 0;
    const auto saved = stored;
    stored.resize(12); assert(!ota() && state == State::Error);
    stored = saved; stored[0] = 2; assert(!ota() && state == State::Error);
    stored = saved; std::memset(stored.data() + 4, 'a', 256); assert(!ota() && state == State::Error);
    stored = saved; init_error = -1; assert(!ota() && state == State::Error);
    assert(stored == saved); // Storage errors must never erase existing credentials.
    // Cannot safely fall back to an embedded key if an owner override cannot be read.
    assert(!recognition_credentials::load(output, "identify.example.com", "new-key", "new-secret", persisted, &state));
    assert(state == State::Error && stored == saved);
    init_error = 0; open_error = -1;
    assert(!recognition_credentials::load(output, "identify.example.com", "new-key", "new-secret", persisted, &state));
    open_error = 0;
    assert(ota() && state == State::Ready);
    using recognition_credentials::OwnerState;
    assert(recognition_credentials::owner_state() == OwnerState::Unset);
    Record custom;
    assert(!recognition_credentials::owner_record(custom, "acrcloud.com.attacker.test", "key", "secret"));
    assert(!recognition_credentials::owner_record(custom, "https://identify-eu-west-1.acrcloud.com", "key", "secret"));
    assert(!recognition_credentials::owner_record(custom, "identify-eu-west-1.acrcloud.com", "", "secret"));
    assert(recognition_credentials::owner_record(custom, "identify-eu-west-1.acrcloud.com", "owner-key", "owner-secret"));
    assert(recognition_credentials::save_owner(custom));
    assert(recognition_credentials::owner_state() == OwnerState::Configured);
    assert(ota() && persisted && std::strcmp(output.key, "owner-key") == 0);
    assert(recognition_credentials::load(output, "identify.example.com", "old-key", "old-secret", persisted, &state));
    assert(std::strcmp(output.key, "owner-key") == 0); // Old bootstrap must not overwrite owner choice.
    Record replacement;
    assert(recognition_credentials::owner_record(replacement, "identify-us-west-2.acrcloud.com", "replacement", "replacement-secret"));
    assert(recognition_credentials::save_owner(replacement));
    assert(ota() && std::strcmp(output.key, "replacement") == 0);
    assert(recognition_credentials::save_owner(custom));
    Record invalid = custom;
    std::memset(invalid.secret, 'a', sizeof(invalid.secret));
    assert(!recognition_credentials::save_owner(invalid));
    invalid = custom; invalid.version = 2;
    assert(!recognition_credentials::save_owner(invalid));
    const auto owner_saved = owner;
    commit_error = -1;
    assert(!recognition_credentials::disable_owner() && owner == owner_saved);
    assert(!recognition_credentials::save_owner(custom) && owner == owner_saved);
    commit_error = 0; write_error = -1;
    assert(!recognition_credentials::disable_owner() && owner == owner_saved);
    write_error = 0;
    assert(recognition_credentials::disable_owner());
    assert(recognition_credentials::owner_state() == OwnerState::Disabled);
    assert(!ota() && state == State::Missing && recognition_startup::trial_ready(true, state));
    assert(!recognition_credentials::load(output, "identify.example.com", "old-key", "old-secret", persisted, &state));
    assert(state == State::Missing && !output.valid());
    assert(recognition_credentials::save_owner(custom)); // Re-enable after removal.
    owner.resize(12);
    assert(!ota() && state == State::Error); // Corrupt override never restores legacy key.
    owner = owner_saved; owner[0] = 99;
    assert(!ota() && state == State::Error);
    owner = owner_saved;
    assert(ota() && std::strcmp(output.key, "owner-key") == 0);
}
