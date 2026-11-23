#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NO_MEM -3
#define NVS_READONLY 0
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
void nvs_close(nvs_handle_t);

#define ESP_ERR_INVALID_STATE 0x103
