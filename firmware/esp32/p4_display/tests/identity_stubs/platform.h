#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_NVS_NOT_FOUND 3
#define ESP_ERR_INVALID_STATE 4
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open_from_partition(const char *,const char *,int,nvs_handle_t *);
void nvs_close(nvs_handle_t);
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
int nvs_set_str(nvs_handle_t,const char *,const char *);
int nvs_commit(nvs_handle_t);
void test_log(const char *,const char *,...);
#define ESP_LOGI test_log
