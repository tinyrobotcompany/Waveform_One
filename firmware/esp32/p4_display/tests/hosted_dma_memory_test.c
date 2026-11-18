#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "hosted_dma_memory.h"
static _Alignas(128) unsigned char blocks[8][65536];
static unsigned allocations, frees;
static size_t requested;
static int fail;
void *heap_caps_aligned_alloc(size_t alignment, size_t size, unsigned caps)
{
    assert(alignment == 128 && size % 128 == 0);
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    requested = size;
    if (fail) return NULL;
    assert(allocations < 8 && size <= sizeof(blocks[0]));
    return blocks[allocations++];
}
void heap_caps_free(void *memory) { if (memory) ++frees; }
int main(void)
{
    assert(!waveform_hosted_buffer_alloc(0));
    assert(!waveform_hosted_buffer_alloc(SIZE_MAX));
    assert(allocations == 0);
    assert(waveform_hosted_buffer_alloc(1536) && requested == 1536);
    assert(waveform_hosted_buffer_alloc(17) && requested == 128);
    uint8_t *buffer = NULL;
    uint32_t capacity = 0;
    assert(waveform_hosted_stream_buffer(&buffer, &capacity, 8192));
    uint8_t *original = buffer;
    fail = 1;
    assert(!waveform_hosted_stream_buffer(&buffer, &capacity, 32768));
    assert(buffer == original && capacity == 8192 && frees == 0);
    // Recovery must retry growth, rather than use a freed/undersized buffer.
    fail = 0;
    assert(waveform_hosted_stream_buffer(&buffer, &capacity, 32768));
    assert(buffer != original && capacity == 32768 && frees == 1);
    unsigned before = allocations;
    assert(waveform_hosted_stream_buffer(&buffer, &capacity, 1536) == buffer);
    assert(allocations == before && frees == 1);
    puts("PASS: hosted PSRAM alignment, bounded allocation and growth failure recovery");
}
