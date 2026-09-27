#pragma once
#include "esp_err.h"
using esp_ota_handle_t=unsigned;
struct esp_partition_t { unsigned size; };
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*);
int esp_ota_begin(const esp_partition_t*,unsigned,esp_ota_handle_t*);
int esp_ota_write(esp_ota_handle_t,const void*,unsigned);
int esp_ota_end(esp_ota_handle_t);
int esp_ota_abort(esp_ota_handle_t);
int esp_ota_set_boot_partition(const esp_partition_t*);
