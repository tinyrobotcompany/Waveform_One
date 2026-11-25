#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${IDF_PATH:?Activate ESP-IDF v6.1 first}"
source_dir="$IDF_PATH/components/mbedtls/mbedtls"
build="$PWD/build-host/identity-real"
app="$PWD/firmware/esp32/p4_display"
mkdir -p "$build/include/mbedtls" "$build/fixtures"
printf '#include "mbedtls/private/bignum.h"\n' > "$build/include/mbedtls/bignum.h"
printf '#include "mbedtls/private/ecp.h"\n' > "$build/include/mbedtls/ecp.h"
python3 "$app/tests/identity_chain_fixtures.py" "$build/fixtures" prepare
cat > "$build/CMakeLists.txt" <<CMAKE
cmake_minimum_required(VERSION 3.16)
project(waveform_identity_proof C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory("$source_dir" sdk)
add_executable(identity_install_test "$app/tests/identity_install_real_test.cpp" "$build/fixtures/roots.S")
target_include_directories(identity_install_test PRIVATE "$source_dir/include" "$source_dir/tf-psa-crypto/include" "$source_dir/tf-psa-crypto/drivers/builtin/include" "$app/tests/identity_stubs" "$app/main")
target_link_libraries(identity_install_test PRIVATE mbedx509)
CMAKE
cmake -S "$build" -B "$build/host" -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DGEN_FILES=OFF "-DCMAKE_C_FLAGS=-I$build/include" > "$build/configure.log" 2>&1
cmake --build "$build/host" --target identity_install_test -j 4 > "$build/build.log" 2>&1
(cd "$build/fixtures" && "$build/host/identity_install_test" csr)
python3 "$app/tests/identity_chain_fixtures.py" "$build/fixtures" issue
(cd "$build/fixtures" && "$build/host/identity_install_test" verify)
python3 "$app/tests/identity_chain_fixtures.py" "$build/fixtures" check-csr
