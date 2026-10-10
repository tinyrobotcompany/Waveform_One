idf_component_register(
    SRCS "main.cpp" "usb_controller.cpp" "network.cpp" "fingerprint.cpp"
         "recognition.cpp" "../third_party/cJSON.c"
         "fonts/waveform_font_16.c" "fonts/waveform_font_20.c"
         "fonts/waveform_font_24.c" "fonts/waveform_font_32.c"
         "fonts/waveform_font_40.c" "fonts/waveform_font_48.c"
    INCLUDE_DIRS "." "../third_party"
    REQUIRES esp32_p4_wifi6_touch_lcd_7b lvgl__lvgl
    PRIV_REQUIRES esp_psram esp_system spi_flash esp_wifi esp_event esp_netif
                  esp_http_server esp_timer nvs_flash esp_driver_sdmmc heap
                  esp_http_client mbedtls esp_new_jpeg)

target_compile_definitions(${COMPONENT_LIB} PRIVATE CJSON_NESTING_LIMIT=32)

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    idf_component_get_property(esp_hosted_lib espressif__esp_hosted COMPONENT_LIB)
    idf_component_get_property(sdmmc_driver_lib esp_driver_sdmmc COMPONENT_LIB)
    target_link_libraries(${esp_hosted_lib} PRIVATE ${sdmmc_driver_lib})
endif()

# The vendor's C aggregate macro intentionally zero-initializes newer optional
# esp_lvgl_adapter fields. Keep C++ warnings strict except for that C interop case.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers)
