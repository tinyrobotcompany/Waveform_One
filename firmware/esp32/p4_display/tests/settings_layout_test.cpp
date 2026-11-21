#include "settings_layout.h"
#include <cassert>
int main() {
    using namespace settings_layout;
    for (unsigned i = 0; i < count; ++i) {
        const auto a = tile(i);
        assert(a.x >= 32 && a.y >= 120 && a.x+a.w <= 992 && a.y+a.h <= 570);
        assert(a.w >= 250 && a.h >= 64);
        for (unsigned j = 0; j < i; ++j) {
            const auto b = tile(j);
            assert(a.x+a.w <= b.x || b.x+b.w <= a.x || a.y+a.h <= b.y || b.y+b.h <= a.y);
        }
    }
    assert(tile(Microphone).y == 130);
}
