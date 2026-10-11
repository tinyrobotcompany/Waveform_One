cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(waveform_visualizer)
esptool_py_flash_to_partition(flash music_model
    "${CMAKE_CURRENT_LIST_DIR}/../common/music_model/yamnet_int8.tflite")
