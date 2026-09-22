idf_component_register(
    SRCS "main.cpp" "audio_pipeline.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_i2s
        esp-dsp
    PRIV_REQUIRES
        esp_timer
)