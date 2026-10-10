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

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/wf1_protocol_test.cpp" -o "$build_dir/p4_wf1_protocol"
"$build_dir/p4_wf1_protocol"
printf '%s\n' 'PASS: ESP32-P4 WF1 client protocol'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/audio_capture_test.cpp" -o "$build_dir/p4_audio_capture"
"$build_dir/p4_audio_capture"
printf '%s\n' 'PASS: ESP32-P4 bounded PCM capture, sequence and checksum validation'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/tests/stubs" -I "$esp_dir/visualizer/tests/stubs" \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/usb_capture_transport_test.cpp" -o "$build_dir/p4_usb_capture"
"$build_dir/p4_usb_capture"
printf '%s\n' 'PASS: production P4 USB capture, shared controls, deadlines, disconnects and cleanup'

"${CXX:-c++}" -O2 -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/fingerprint_test.cpp" \
    "$esp_dir/p4_display/main/fingerprint.cpp" -o "$build_dir/p4_fingerprint"
"$build_dir/p4_fingerprint"
printf '%s\n' 'PASS: native fingerprints against upstream reference and binary encoding'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/capture_completion_test.cpp" -o "$build_dir/p4_completion"
"$build_dir/p4_completion"
printf '%s\n' 'PASS: recognition completion deadlines, cancellation, stale callbacks and PCM ownership'

"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-deprecated-declarations -DCJSON_NESTING_LIMIT=32 \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/third_party" \
    -c "$esp_dir/p4_display/third_party/cJSON.c" -o "$build_dir/cJSON.o"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" -I "$esp_dir/p4_display/third_party" \
    "$esp_dir/p4_display/tests/recognition_metadata_test.cpp" \
    "$build_dir/cJSON.o" -o "$build_dir/p4_metadata"
"$build_dir/p4_metadata"
printf '%s\n' 'PASS: recognition metadata, UTF-8 and artwork origin validation'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/recognition_policy_test.cpp" -o "$build_dir/p4_recognition_policy"
"$build_dir/p4_recognition_policy"
printf '%s\n' 'PASS: recognition session invalidation, pairing rotation, backoff and expiry'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/control_policy_test.cpp" -o "$build_dir/p4_control_policy"
"$build_dir/p4_control_policy"
printf '%s\n' 'PASS: ESP32-P4 remote, Wi-Fi and UI control policies'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/remote_auth_test.cpp" -o "$build_dir/p4_remote_auth"
"$build_dir/p4_remote_auth"
printf '%s\n' 'PASS: ESP32-P4 phone remote pairing and session lifecycle'
