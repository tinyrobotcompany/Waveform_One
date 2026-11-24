#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_heap_caps.h"

/* ESP32-P4 SDMMC in IDF 6.1 accepts PSRAM and performs cache synchronization.
 * Align both address and capacity to the P4's largest cache line (128 bytes).
 * Short CMD53 tails use the existing SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF bounce
 * buffer. Do not apply this allocator to older chips/SDMMC implementations. */
static inline void *waveform_hosted_buffer_alloc(size_t size)
{
    if (!size || size > SIZE_MAX - 127) return NULL;
    return heap_caps_aligned_alloc(128, (size + 127) & ~(size_t)127,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

/* Keep the previous allocation and capacity valid when growth fails. */
static inline uint8_t *waveform_hosted_stream_buffer(uint8_t **buffer,
                                                    uint32_t *capacity, uint32_t size)
{
    if (size > *capacity) {
        uint8_t *next = (uint8_t *)waveform_hosted_buffer_alloc(size);
        if (!next) return NULL;
        heap_caps_free(*buffer);
        *buffer = next;
        *capacity = size;
    }
    return *buffer;
}
