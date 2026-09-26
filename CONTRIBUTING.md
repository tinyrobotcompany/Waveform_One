# Contributing

Install Git, Python 3.9+, Node.js 22+ and a C++17 compiler with AddressSanitizer
and UndefinedBehaviorSanitizer (Xcode command-line tools on macOS, Clang on Linux),
plus Rust/Cargo (tested toolchain 1.98.1).

```sh
sh scripts/setup-hooks.sh
sh scripts/test.sh
```

The pre-commit hook runs every host unit-test suite: Codex review helpers, release
and packaging tooling, FFT/audio processing, LED patterns, frequency bars and
low-volume sensitivity, visual styles, USB commands and the Rust Pi controller.
A failing suite prevents the commit. Cargo may fetch locked dependencies on its
first run; it does not install a compiler/toolchain. It installs no other
dependencies and does not flash hardware. Hooks need installing once per clone;
the local Git setting also applies to worktrees. Hooks test working-tree files:
stage the complete change, avoiding partially staged source files. CI checks the
committed PR content independently, even if someone bypasses their local hook.

Use TDD for new processing logic, visual styles and Pi behaviour: write a failing
test, implement it, then refactor. Microcontroller code can be unit tested on the
host when hardware-independent logic is separated from GPIO/I2S adapters. The
existing firmware suites cover that logic; physical wiring, electrical levels,
display refresh quality and actual audio capture still require board testing.
The scrolling demos are built in CI but do not yet have rendering unit tests.

ESP-IDF v6.1 builds all five programs in CI. To build locally, activate ESP-IDF,
then run `idf.py build` inside the relevant `firmware/esp32/<program>` directory.
See the [firmware README](firmware/esp32/README.md) for installation and wiring.

Add a [changeset](.changeset/README.md) with each firmware, Pi, protocol or
automation PR. PR checks run all host tests and firmware builds. Codex reviews
non-draft PRs except Dependabot and changeset-release PRs, matching Voxa. Inspect
the actual review body: a successful check can represent a skip or fallback.

Changes merge into `main` only when approved. A successful main pipeline publishes
pending changesets as a GitHub release with firmware assets; it does not install
anything on a Pi or ESP32. Do not manually create `v*` tags or edit released assets.
