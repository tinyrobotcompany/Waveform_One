#pragma once
#include <cstdint>

namespace recognition_credentials {
// Unpersisted means valid embedded credentials are usable this boot, but their
// durable write failed. It must never authorize an OTA trial.
enum class State { Missing, Ready, Unpersisted, Error };
struct Record {
    uint32_t version = 1;
    char host[256]{};
    char key[256]{};
    char secret[256]{};
    bool valid() const;
};
// Owner changes are stored atomically and take effect on the next boot.
// Disabled is an explicit override: embedded/bootstrap credentials cannot restore it.
enum class OwnerState { Unset, Configured, Disabled, Error };
OwnerState owner_state();
bool owner_record(Record &output, const char *host, const char *key, const char *secret);
bool save_owner(const Record &record);
bool disable_owner();
// Local bootstrap provisions a single atomic NVS record. Public OTA images
// provide empty fields and reuse that record; they never select another service.
bool load(Record &output, const char *host, const char *key, const char *secret,
          bool &persisted, State *state = nullptr);
}
