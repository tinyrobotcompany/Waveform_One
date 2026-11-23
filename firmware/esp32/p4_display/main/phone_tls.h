#pragma once
#include "esp_https_server.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Identity is provisioned over USB into a dedicated partition, never in firmware. */
esp_err_t waveform_phone_tls_start(httpd_handle_t *server, const httpd_config_t *config);
void waveform_phone_tls_stop(httpd_handle_t server);
const char *waveform_phone_tls_hostname(void);

#ifdef __cplusplus
}
#endif
