#pragma once
#include "platform.h"
typedef struct {char version[32],project_name[32];} esp_app_desc_t;
const esp_app_desc_t *esp_app_get_description(void);
typedef struct {unsigned size;} esp_partition_t;
typedef unsigned esp_ota_handle_t;
typedef int esp_ota_img_states_t;
#define ESP_OTA_IMG_PENDING_VERIFY 1
#define ESP_OTA_IMG_VALID 2
#define ESP_OTA_IMG_INVALID 3
#define ESP_OTA_IMG_ABORTED 4
#define ESP_ERR_NOT_FOUND -20
#define OTA_WITH_SEQUENTIAL_WRITES ((unsigned)-2)
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const void *);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *);
esp_err_t esp_ota_begin(const esp_partition_t *,unsigned,esp_ota_handle_t *);
esp_err_t esp_ota_write(esp_ota_handle_t,const void *,size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *);
void esp_restart(void);
uint32_t esp_random(void);
void vQueueDelete(QueueHandle_t);
typedef struct {int unused;} mbedtls_md_context_t;
typedef struct {int unused;} mbedtls_pk_context;
#define MBEDTLS_MD_SHA256 1
void mbedtls_md_init(mbedtls_md_context_t *);
void mbedtls_md_free(mbedtls_md_context_t *);
void *mbedtls_md_info_from_type(int);
int mbedtls_md_setup(mbedtls_md_context_t *,const void *,int);
int mbedtls_md_starts(mbedtls_md_context_t *);
int mbedtls_md_update(mbedtls_md_context_t *,const void *,size_t);
int mbedtls_md_finish(mbedtls_md_context_t *,unsigned char *);
int mbedtls_md(const void *,const unsigned char *,size_t,unsigned char *);
void mbedtls_pk_init(mbedtls_pk_context *);
void mbedtls_pk_free(mbedtls_pk_context *);
int mbedtls_pk_parse_public_key(mbedtls_pk_context *,const unsigned char *,size_t);
int mbedtls_pk_verify(mbedtls_pk_context *,int,const unsigned char *,size_t,const unsigned char *,size_t);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
