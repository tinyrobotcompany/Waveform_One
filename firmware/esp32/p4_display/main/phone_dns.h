#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Blocking resolver: call only from the main maintenance worker, never the UI. */
bool waveform_phone_dns_matches(const char *hostname,uint32_t station_address);

#ifdef __cplusplus
}
#endif
