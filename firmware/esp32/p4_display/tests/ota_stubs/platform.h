#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_NVS_NOT_FOUND 3
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define WIFI_STORAGE_RAM 0
#define WIFI_MODE_STA 0
#define WIFI_IF_STA 0
#define WIFI_AUTH_OPEN 0
#define WIFI_SCAN_TYPE_ACTIVE 0
#define WIFI_EVENT_STA_DISCONNECTED 1
#define WIFI_EVENT_SCAN_DONE 2
#define IP_EVENT_STA_GOT_IP 3
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
typedef int portMUX_TYPE;
typedef struct test_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned count, size_t size);
int xQueueSend(QueueHandle_t queue, const void *item, unsigned ticks);
int xQueueReceive(QueueHandle_t queue, void *item, unsigned ticks);
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *handle);
void test_log(const char *tag, const char *format, ...);
#define ESP_LOGI test_log
#define ESP_LOGW test_log
const char *esp_err_to_name(esp_err_t err);
int64_t esp_timer_get_time(void);
typedef int nvs_handle_t;
int nvs_open(const char *, int, nvs_handle_t *);
void nvs_close(nvs_handle_t);
int nvs_get_u8(nvs_handle_t, const char *, uint8_t *);
int nvs_get_str(nvs_handle_t, const char *, char *, size_t *);
int nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
int nvs_set_u8(nvs_handle_t, const char *, uint8_t);
int nvs_set_str(nvs_handle_t, const char *, const char *);
int nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
int nvs_erase_key(nvs_handle_t, const char *);
int nvs_commit(nvs_handle_t);
typedef const char *esp_event_base_t;
extern const char *WIFI_EVENT, *IP_EVENT;
typedef void (*test_event_handler_t)(void *,esp_event_base_t,int32_t,void *);
int esp_event_handler_register(esp_event_base_t, int32_t,test_event_handler_t,void *);
int esp_event_loop_create_default(void);
int esp_netif_init(void);
void *esp_netif_create_default_wifi_sta(void);
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct { struct { unsigned char ssid[32], password[64]; struct {bool capable,required;} pmf_cfg;} sta;} wifi_config_t;
typedef struct {unsigned status;} wifi_event_sta_scan_done_t;
typedef struct {unsigned char ssid[33]; int8_t rssi; int authmode;} wifi_ap_record_t;
typedef struct {bool show_hidden; int scan_type; struct {struct {unsigned min,max;} active;} scan_time;} wifi_scan_config_t;
int esp_wifi_init(wifi_init_config_t *);
int esp_wifi_set_storage(int);
int esp_wifi_set_mode(int);
int esp_wifi_disconnect(void);
int esp_wifi_stop(void);
int esp_wifi_start(void);
int esp_wifi_connect(void);
int esp_wifi_set_config(int,wifi_config_t *);
int esp_wifi_scan_start(wifi_scan_config_t *,bool);
int esp_wifi_scan_stop(void);
int esp_wifi_clear_ap_list(void);
int esp_wifi_scan_get_ap_records(uint16_t *,wifi_ap_record_t *);
typedef struct {bool start; void (*sync_cb)(struct timeval *);} esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(...) ((esp_sntp_config_t){0})
int esp_netif_sntp_init(esp_sntp_config_t *);
void esp_netif_sntp_deinit(void);
int esp_netif_sntp_start(void);
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

#define ESP_HTTP_CLIENT_TLS_VER_TLS_1_3 2

int esp_http_client_set_timeout_ms(void *,int);

typedef struct { uint8_t reason; } wifi_event_sta_disconnected_t;

int esp_http_client_set_url(void *,const char *);
