#include <cassert>
#include "../main/patterns.h"

int main()
{
    using namespace panel;
    for (int row = 0; row < 16; ++row) {
        for (int x = 0; x < 64; ++x) {
            assert(rowPair(Pattern::Black, 0, x, row) == 0);
            assert(rowPair(Pattern::Red, 0, x, row) == 0b001001);
            assert(rowPair(Pattern::Green, 0, x, row) == 0b010010);
            assert(rowPair(Pattern::Blue, 0, x, row) == 0b100100);
            assert(rowPair(Pattern::Halves, 0, x, row) == 0b100001);
        }
    }
    // Every moving row must light exactly one physical row, including the
    // boundary between the two RGB outputs and the wrap back to row zero.
    for (int step = 0; step < 66; ++step) {
        int litPixels = 0;
        for (int row = 0; row < 16; ++row) {
            for (int x = 0; x < 64; ++x) {
                const auto bits = rowPair(Pattern::Rows, step, x, row);
                assert(bits == (row == step % 16
                    ? (step % 32 < 16 ? 0b000010 : 0b010000) : 0));
                litPixels += (bits & 7) != 0;
                litPixels += (bits & 56) != 0;
            }
        }
        assert(litPixels == 64);
        int litColumns = 0;
        for (int x = 0; x < 64; ++x) {
            const auto bits = rowPair(Pattern::Columns, step, x, 0);
            assert(bits == (x == step % 64 ? 0b001001 : 0));
            litColumns += bits != 0;
        }
        assert(litColumns == 1);
    }
    assert(pixel(Pattern::Bars, 0, 0, 0) == 1);
    assert(pixel(Pattern::Bars, 0, 21, 0) == 1);
    assert(pixel(Pattern::Bars, 0, 22, 0) == 2);
    assert(pixel(Pattern::Bars, 0, 42, 0) == 2);
    assert(pixel(Pattern::Bars, 0, 43, 0) == 4);
    assert(pixel(Pattern::Bars, 0, 63, 0) == 4);
}
