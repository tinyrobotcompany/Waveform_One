#!/bin/sh
# Run hardware-independent firmware tests on the development computer.
set -eu
esp_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/waveform-firmware-tests.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

sh "$esp_dir/mic_test/tests/run.sh"

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$esp_dir/led_test/tests/patterns_test.cpp" -o "$build_dir/patterns"
"$build_dir/patterns"
printf '%s\n' 'PASS: LED diagnostic patterns'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/visualizer/main" -I "$esp_dir/mic_test/main" \
    "$esp_dir/visualizer/tests/bars_test.cpp" -o "$build_dir/bars"
"$build_dir/bars"
printf '%s\n' 'PASS: frequency-bar rendering'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/visualizer/main" -I "$esp_dir/mic_test/main" \
    "$esp_dir/visualizer/tests/sensitivity_test.cpp" \
    "$esp_dir/mic_test/main/audio_pipeline.cpp" -o "$build_dir/sensitivity"
"$build_dir/sensitivity"
printf '%s\n' 'PASS: all four firmware host test suites'
for suite in styles control capture update; do
    "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -I "$esp_dir/visualizer/main" -I "$esp_dir/mic_test/main" \
        "$esp_dir/visualizer/tests/${suite}_test.cpp" -o "$build_dir/$suite"
    "$build_dir/$suite"
    printf 'PASS: %s\n' "$suite"
done

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/visualizer/tests/stubs" \
    -I "$esp_dir/visualizer/main" -I "$esp_dir/mic_test/main" \
    "$esp_dir/visualizer/tests/capture_transport_test.cpp" -o "$build_dir/capture_transport"
"$build_dir/capture_transport"
printf '%s\n' 'PASS: production capture/control transport, overflow recovery and failed headers'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/visualizer/tests/stubs" -I "$esp_dir/visualizer/main" \
    "$esp_dir/visualizer/tests/update_flash_test.cpp" -o "$build_dir/update_flash"
"$build_dir/update_flash"
printf '%s\n' 'PASS: production flash adapter failure cleanup and retry'
