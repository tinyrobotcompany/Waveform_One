idf_component_register(
    SRCS "main.cpp" "panel.cpp" "control.cpp" "capture.cpp" "update.cpp" "music_detector.cpp"
         "../../common/music_features.cpp" "../../mic_test/main/audio_pipeline.cpp"
    INCLUDE_DIRS "." "../../mic_test/main" "../../common" "../../common/music_model"
    REQUIRES esp_driver_i2s esp-dsp
    PRIV_REQUIRES app_update mbedtls esp_driver_gpio esp_timer esp_rom esp_driver_usb_serial_jtag
                  espressif__esp-tflite-micro esp_partition)
set_source_files_properties("../../common/music_features.cpp" PROPERTIES COMPILE_OPTIONS "-O3")
