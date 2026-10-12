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
printf '%s\n' 'PASS: production capture/stream/control transport, overflow recovery and failed headers'

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
    "$esp_dir/p4_display/tests/audio_stream_test.cpp" -o "$build_dir/p4_audio_stream"
"$build_dir/p4_audio_stream"
printf '%s\n' 'PASS: ESP32-P4 continuous PCM stream, sequence/checksum validation and sample ring'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/tests/stubs" -I "$esp_dir/visualizer/tests/stubs" \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/usb_stream_transport_test.cpp" -o "$build_dir/p4_usb_stream"
"$build_dir/p4_usb_stream"
printf '%s\n' 'PASS: production P4 USB audio stream, restarts, shared controls and disconnects'

"${CXX:-c++}" -O1 -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/listening_test.cpp" -o "$build_dir/p4_listening"
"$build_dir/p4_listening"
printf '%s\n' 'PASS: sound start and silence detection against the learnt room background'

"${CXX:-c++}" -O2 -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/fingerprint_test.cpp" \
    "$esp_dir/p4_display/main/fingerprint.cpp" -o "$build_dir/p4_fingerprint"
"$build_dir/p4_fingerprint"
printf '%s\n' 'PASS: streaming fingerprints against upstream reference, recordings and binary encoding'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/tests/stubs" -I "$esp_dir/visualizer/tests/stubs" \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/recognition_http_test.cpp" -o "$build_dir/p4_http"
"$build_dir/p4_http"
printf '%s\n' 'PASS: production recognition HTTP chunked reads, size bounds, errors and Retry-After'

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
    -I "$esp_dir/p4_display/main" -I "$esp_dir/p4_display/third_party" \
    "$esp_dir/p4_display/tests/album_resolver_test.cpp" \
    "$build_dir/cJSON.o" -o "$build_dir/p4_album"
"$build_dir/p4_album"
printf '%s\n' 'PASS: album choice from catalogue releases, current-album preference and fallbacks'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" -I "$esp_dir/p4_display/third_party" \
    "$esp_dir/p4_display/tests/album_sequence_test.cpp" \
    "$build_dir/cJSON.o" -o "$build_dir/p4_album_sequence"
"$build_dir/p4_album_sequence"
printf '%s\n' 'PASS: album track order, gaps, silences, gapless tracks and returning to live listening'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" -I "$esp_dir/p4_display/third_party" \
    "$esp_dir/p4_display/tests/remote_state_test.cpp" \
    "$build_dir/cJSON.o" -o "$build_dir/p4_remote_state"
"$build_dir/p4_remote_state"
printf '%s\n' 'PASS: phone recognition snapshot ownership, expiry, JSON and concurrent updates'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/recognition_policy_test.cpp" -o "$build_dir/p4_recognition_policy"
"$build_dir/p4_recognition_policy"
printf '%s\n' 'PASS: recognition sessions, lookup planning, request budget and backoff'

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

"${CXX:-c++}" -O2 -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/search_waveform_test.cpp" -o "$build_dir/p4_search_waveform"
"$build_dir/p4_search_waveform"
printf '%s\n' 'PASS: fixed search waveform buffer bounds, gutters and animation cycle'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" \
    "$esp_dir/p4_display/tests/display_feedback_test.cpp" -o "$build_dir/p4_feedback"
"$build_dir/p4_feedback"
printf '%s\n' 'PASS: native recognition feedback and UTF-8 display fallback'

for remote_version in 0x010300 0x010302; do
    "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -DTEST_REMOTE_VERSION="$remote_version" \
        -I "$esp_dir/p4_display/tests/stubs" -I "$esp_dir/p4_display/main" \
        "$esp_dir/p4_display/tests/hosted_rx_ownership_test.cpp" -o "$build_dir/p4_rx_ownership"
    "$build_dir/p4_rx_ownership"
done
printf '%s\n' 'PASS: hosted Wi-Fi RX buffer ownership on old and consuming callback contracts'

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$esp_dir/p4_display/main" "$esp_dir/p4_display/tests/album_palette_test.cpp" -o "$build_dir/p4_palette"
"$build_dir/p4_palette"
printf '%s\n' 'PASS: artwork palette bounds, neutral fallbacks, deterministic extraction and readable contrast'
