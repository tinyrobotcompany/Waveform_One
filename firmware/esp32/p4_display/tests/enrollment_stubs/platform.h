#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../identity_stubs/platform.h"
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,void *);
#define HTTP_EVENT_ON_DATA 1
#define HTTP_EVENT_ON_HEADER 2
typedef struct {int event_id, data_len; void *user_data,*data;const char *header_key,*header_value;} esp_http_client_event_t;
typedef struct {const char *url; int timeout_ms; int buffer_size_tx; int buffer_size; int tls_version; void (*crt_bundle_attach)(void); int (*event_handler)(esp_http_client_event_t *);void *user_data;bool disable_auto_redirect;} esp_http_client_config_t;
typedef void *esp_http_client_handle_t;
void esp_crt_bundle_attach(void);
void *esp_http_client_init(esp_http_client_config_t *);
int esp_http_client_perform(void *);
int esp_http_client_get_status_code(void *);
void esp_http_client_cleanup(void *);

void vTaskDelay(unsigned ticks);

int esp_http_client_set_header(void *,const char *,const char *);
int esp_http_client_open(void *,int);
int64_t esp_http_client_fetch_headers(void *);
int esp_http_client_read(void *,char *,int);
bool esp_http_client_is_complete_data_received(void *);
void esp_http_client_close(void *);
void vTaskDelete(void *);
