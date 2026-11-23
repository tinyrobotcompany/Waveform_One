#pragma once
#include "esp_https_server.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Identity is provisioned over USB into a dedicated partition, never in firmware. */
esp_err_t waveform_phone_tls_start(httpd_handle_t *server, const httpd_config_t *config);
/* ESP_OK means the server has drained and identity buffers have been released.
 * On failure the caller must retain its handle and retry; starting another server is refused. */
esp_err_t waveform_phone_tls_stop(httpd_handle_t server);
bool waveform_phone_tls_time_valid(void);
const char *waveform_phone_tls_hostname(void);

#ifdef __cplusplus
}
#endif
