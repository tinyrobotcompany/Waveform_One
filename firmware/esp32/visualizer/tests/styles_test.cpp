#include <cassert>
#include <limits>
#include "styles.h"

int main() {
    visual::Renderer renderer;
    audio::Frame frame{};
    frame.calibrated = true;
    frame.levels[6] = 0.5f;
    renderer.render(frame, 0);
    int defaultLit = 0;
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x) {
        assert(renderer.pixel(x, y) == renderer.pixel(x, 31-y));
        defaultLit += renderer.pixel(x, y) != 0;
    }
    assert(defaultLit > 0);
    renderer.setMode(visual::Mode::Classic);
    renderer.render(frame, 0);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 64; ++x)
            assert(renderer.pixel(x, y) == panel::pixel(frame, x, y));
    renderer.setMode(visual::Mode::Mirrored);
    renderer.render(frame, 0);
    int lit = 0;
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x) {
        assert(renderer.pixel(x, y) == renderer.pixel(x, 31-y));
        lit += renderer.pixel(x, y) != 0;
    }
    assert(lit > 0);
    renderer.setMode(visual::Mode::Waterfall);
    renderer.render(frame, 100);
    assert(renderer.pixel(16, 0) != 0);
    assert(renderer.pixel(16, 1) == 0);
    renderer.render(frame, 120); // Refreshing must not advance history.
    assert(renderer.pixel(16, 1) == 0);
    frame.levels.fill(0);
    renderer.render(frame, 150);
    assert(renderer.pixel(16, 0) == 0 && renderer.pixel(16, 1) != 0);
    renderer.render(frame, 1800);
    for (int y = 0; y < 32; ++y) assert(renderer.pixel(16, y) == 0);
    frame.levels.fill(1);
    renderer.render(frame, 1900);
    renderer.render({}, 1950); // Lost capture/calibration clears history too.
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x)
        assert(renderer.pixel(x, y) == 0);
    frame.levels.fill(std::numeric_limits<float>::quiet_NaN());
    for (auto mode : {visual::Mode::Classic, visual::Mode::Mirrored, visual::Mode::Waterfall}) {
        renderer.setMode(mode);
        renderer.render(frame, 2000);
        for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x)
            assert(renderer.pixel(x, y) == 0);
    }
}
