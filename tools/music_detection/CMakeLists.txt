cmake_minimum_required(VERSION 3.20)
project(waveform_music_host LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
if(NOT TFLM_ROOT)
    message(FATAL_ERROR "Set TFLM_ROOT to pinned espressif/esp-tflite-micro v1.4.1")
endif()
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${TFLM_ROOT}" OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT revision STREQUAL "5d505b5ffac8e60923bf29a0cc6e6348c84e3763")
    message(FATAL_ERROR "Unexpected TFLM revision: ${revision}")
endif()
set(lite "${TFLM_ROOT}/tensorflow/lite")
file(GLOB micro_sources "${lite}/micro/*.cc" "${lite}/micro/tflite_bridge/*.cc" "${lite}/micro/memory_planner/*.cc" "${lite}/micro/arena_allocator/*.cc")
list(FILTER micro_sources EXCLUDE REGEX "(_test|fake_micro_context|mock_micro_graph|test_helpers|micro_allocation_info_test).cc$")
set(kernel_sources)
foreach(kernel IN ITEMS conv conv_common depthwise_conv depthwise_conv_common fully_connected fully_connected_common reduce reduce_common logistic logistic_common kernel_util)
    list(APPEND kernel_sources "${lite}/micro/kernels/${kernel}.cc")
endforeach()
add_executable(music_micro micro_cli.cpp ${micro_sources} ${kernel_sources}
    "${lite}/core/c/common.cc" "${lite}/core/api/flatbuffer_conversions.cc" "${lite}/core/api/tensor_utils.cc"
    "${lite}/kernels/kernel_util.cc" "${lite}/kernels/internal/common.cc"
    "${lite}/kernels/internal/quantization_util.cc" "${lite}/kernels/internal/portable_tensor_utils.cc"
    "${lite}/kernels/internal/tensor_utils.cc" "${lite}/kernels/internal/tensor_ctypes.cc"
    "${lite}/kernels/internal/reference/portable_tensor_utils.cc"
    "${TFLM_ROOT}/tensorflow/compiler/mlir/lite/core/api/error_reporter.cc"
    "${TFLM_ROOT}/tensorflow/compiler/mlir/lite/schema/schema_utils.cc")
target_include_directories(music_micro PRIVATE "${TFLM_ROOT}" "${TFLM_ROOT}/third_party/flatbuffers/include" "${TFLM_ROOT}/third_party/gemmlowp" "${TFLM_ROOT}/third_party/ruy")
target_compile_definitions(music_micro PRIVATE TF_LITE_STATIC_MEMORY TF_LITE_DISABLE_X86_NEON)
target_compile_options(music_micro PRIVATE -O3 -fno-rtti -fno-exceptions)
add_executable(music_features features_cli.cpp ../../firmware/esp32/common/music_features.cpp)
target_include_directories(music_features PRIVATE ../../firmware/esp32/common ../../firmware/esp32/common/music_model)
target_compile_options(music_features PRIVATE -O2 -Wall -Wextra -Werror -fsanitize=address,undefined)
target_link_options(music_features PRIVATE -fsanitize=address,undefined)
