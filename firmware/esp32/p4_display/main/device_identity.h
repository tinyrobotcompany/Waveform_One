#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* All private-key operations execute on the ESP. Only public identity leaves it. */
esp_err_t waveform_identity_init(void);
const char *waveform_identity_id(void);
const char *waveform_identity_hostname(void);
esp_err_t waveform_identity_csr(char *output, size_t capacity);
esp_err_t waveform_identity_sign(const char *body, char *signature, size_t capacity);
esp_err_t waveform_identity_install(const char *chain, int64_t *renew_after);
bool waveform_identity_changed(void);
int64_t waveform_identity_renew_after(void);
