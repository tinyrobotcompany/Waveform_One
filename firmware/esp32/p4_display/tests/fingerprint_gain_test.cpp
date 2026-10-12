#include <algorithm>
#include <cassert>
#include "fingerprint_gain.h"

int main() {
    fingerprint::Normalisation gain;
    // Boundaries of exact 32x, fractional gain, unity gain and signed PCM.
    for (unsigned peak : {0u, 1u, 511u, 812u, 813u, 1000u, 3276u, 25999u, 26000u, 32768u}) {
        assert(gain.prepare(peak));
        const double original = peak ? std::max(1.0, std::min(32.0, 26000.0 / peak)) : 32.0;
        for (int value = -int(peak); value <= int(peak); ++value)
            assert(gain.apply(value) == int(value * original));
    }
    assert(!gain.prepare(32769));
    // Reuse must replace a fractional table and either integer fast path.
    assert(gain.prepare(1000) && gain.apply(-1000) == -26000);
    assert(gain.prepare(32768) && gain.apply(-32768) == -32768);
    assert(gain.prepare(1) && gain.apply(-1) == -32);
}
