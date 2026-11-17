#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { bool connected, time_ready; } waveform_update_network_t;
bool waveform_update_local_ready(void);
void waveform_update_network(waveform_update_network_t *out);
#ifdef __cplusplus
}
#endif
