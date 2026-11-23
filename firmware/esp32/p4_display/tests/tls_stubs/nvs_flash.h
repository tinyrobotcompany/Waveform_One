#pragma once
#include "platform.h"
int nvs_flash_init_partition(const char *);
int nvs_open_from_partition(const char *,const char *,int,nvs_handle_t *);
#ifdef CONFIG_NVS_ENCRYPTION
#include "esp_partition.h"
typedef struct { unsigned char eky[32], tky[32]; } nvs_sec_cfg_t;
int nvs_flash_read_security_cfg(const esp_partition_t *, nvs_sec_cfg_t *);
int nvs_flash_secure_init_partition(const char *, nvs_sec_cfg_t *);
#endif
