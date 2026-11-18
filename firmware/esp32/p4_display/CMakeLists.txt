cmake_minimum_required(VERSION 3.16)

# Allow a local repair build to retain its base release version so an older
# published image cannot automatically replace the repair. CI always overrides it.
set(PROJECT_VER "0.0.0" CACHE STRING "Local firmware version")
if(DEFINED ENV{WAVEFORM_RELEASE_VERSION})
    set(PROJECT_VER "$ENV{WAVEFORM_RELEASE_VERSION}")
endif()
if(NOT PROJECT_VER MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "Invalid firmware version")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(waveform_p4_display)

# The YAMNet music classifier (4 MB) lives in its own partition; standard
# flash arguments write it. App-only updates leave it in place.
esptool_py_flash_to_partition(flash music_model
    "${CMAKE_CURRENT_LIST_DIR}/../common/music_model/yamnet_int8.tflite")
