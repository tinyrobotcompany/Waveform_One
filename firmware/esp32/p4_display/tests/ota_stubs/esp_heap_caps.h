#pragma once
#include <stddef.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
#define MALLOC_CAP_SPIRAM 8
static inline size_t heap_caps_get_free_size(unsigned caps){(void)caps;return 64000;}
static inline size_t heap_caps_get_minimum_free_size(unsigned caps){(void)caps;return 32000;}
static inline size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 16000;}
static inline unsigned uxTaskGetStackHighWaterMark(void *task){(void)task;return 4096;}
