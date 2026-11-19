#pragma once
#include <cstdint>

namespace recognition_credentials {
enum class State { Missing, Ready, Error };
struct Record {
    uint32_t version = 1;
    char host[256]{};
    char key[256]{};
    char secret[256]{};
    bool valid() const;
};
// Local bootstrap provisions a single atomic NVS record. Public OTA images
// provide empty fields and reuse that record; they never select another service.
bool load(Record &output, const char *host, const char *key, const char *secret,
          bool &persisted, State *state = nullptr);
}
