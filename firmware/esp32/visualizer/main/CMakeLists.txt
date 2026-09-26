idf_component_register(
    SRCS "main.cpp" "panel.cpp" "../../mic_test/main/audio_pipeline.cpp"
    INCLUDE_DIRS "." "../../mic_test/main"
    REQUIRES esp_driver_i2s esp-dsp
    PRIV_REQUIRES esp_driver_gpio esp_timer esp_rom)
