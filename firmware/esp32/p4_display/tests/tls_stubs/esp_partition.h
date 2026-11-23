#pragma once
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS 4
typedef struct { int unused; } esp_partition_t;
const esp_partition_t *esp_partition_find_first(int, int, const char *);
