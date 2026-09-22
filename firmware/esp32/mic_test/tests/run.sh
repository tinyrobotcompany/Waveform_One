#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/waveform-audio-tests.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$test_dir/../main" \
    "$test_dir/../main/audio_pipeline.cpp" "$test_dir/audio_pipeline_test.cpp" \
    -o "$build_dir/audio_pipeline_test"
"$build_dir/audio_pipeline_test"
