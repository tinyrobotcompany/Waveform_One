#pragma once
#include "esp_err.h"
#include "waveform_update.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t waveform_update_start(void);
void waveform_update_status(waveform_update_status_t *out);
bool waveform_update_request(const char *action);
bool waveform_update_configure(unsigned hours,bool automatic,unsigned start,unsigned end);
bool waveform_update_validate_trial(void);

#ifdef __cplusplus
}
#endif
