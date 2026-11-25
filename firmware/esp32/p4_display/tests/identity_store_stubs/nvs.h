#pragma once
#include "platform.h"
int nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
int nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
#define ESP_ERR_NVS_NOT_FOUND 99
#define NVS_READWRITE 1
int nvs_commit(nvs_handle_t);
