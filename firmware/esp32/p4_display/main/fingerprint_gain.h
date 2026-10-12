#pragma once
#include <algorithm>
#include <cstdint>

namespace fingerprint {
// Exact equivalent of int(sample * max(1, min(32, 26000.0 / peak))).
// Avoid software double arithmetic for every sample on the P4: integer gains
// are exact, and fractional gains cache each magnitude once. Input magnitude
// must not exceed the peak supplied to prepare(). Allocate with Workspace.
class Normalisation {
public:
    bool prepare(unsigned peak) {
        if (peak > 32768) return false;
        factor_ = peak >= 26000 ? 1 : peak <= 812 ? 32 : 0;
        if (!factor_) {
            const double gain = 26000.0 / peak;
            for (unsigned i = 0; i <= peak; ++i) magnitudes_[i] = uint16_t(int(i * gain));
        }
        return true;
    }
    int apply(int value) const {
        if (factor_) return value * factor_;
        return value < 0 ? -int(magnitudes_[unsigned(-value)]) : int(magnitudes_[unsigned(value)]);
    }
private:
    uint16_t magnitudes_[26000];
    int factor_ = 1;
};
}
