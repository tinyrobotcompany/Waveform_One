#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Background clients hold a lease until their connection is destroyed.
 * Only the OTA worker owns pause/resume. No persisted settings are changed. */
bool waveform_network_acquire(void);
void waveform_network_release(void);
void waveform_network_pause(void);
bool waveform_network_paused(void);
bool waveform_network_drained(void);
void waveform_network_resume(void);
#ifdef __cplusplus
}
#endif
