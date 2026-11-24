#include "network_activity.h"
#include <assert.h>
#include <stdint.h>
#define PAUSED (UINT32_C(1) << 31)
static uint32_t state;
bool waveform_network_acquire(void)
{
    uint32_t current = __atomic_load_n(&state, __ATOMIC_ACQUIRE);
    do {
        if (current & PAUSED) return false;
        assert(current < PAUSED - 1);
    } while (!__atomic_compare_exchange_n(&state, &current, current + 1, false,
                                         __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    return true;
}
void waveform_network_release(void)
{
    uint32_t previous = __atomic_fetch_sub(&state, 1, __ATOMIC_ACQ_REL);
    assert((previous & ~PAUSED) > 0);
    (void)previous;
}
void waveform_network_pause(void) { __atomic_fetch_or(&state, PAUSED, __ATOMIC_ACQ_REL); }
bool waveform_network_paused(void) { return (__atomic_load_n(&state, __ATOMIC_ACQUIRE) & PAUSED) != 0; }
bool waveform_network_drained(void) { return __atomic_load_n(&state, __ATOMIC_ACQUIRE) == PAUSED; }
void waveform_network_resume(void) { __atomic_fetch_and(&state, ~PAUSED, __ATOMIC_ACQ_REL); }
