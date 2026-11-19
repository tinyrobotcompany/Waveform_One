idf_component_register(
    SRCS "update_service.c" "update_policy.c" "main.cpp" "display_schedule.cpp" "usb_controller.cpp" "network.cpp" "fingerprint.cpp"
         "recognition.cpp" "recognition_credentials.cpp" "recognition_input.cpp" "p4_led.cpp" "unicode_font.cpp" "../third_party/cJSON.c"
         "music_detector.cpp" "../../common/music_features.cpp"
         "fonts/waveform_font_16.c" "fonts/waveform_font_20.c"
         "fonts/waveform_font_24.c" "fonts/waveform_font_32.c"
         "fonts/waveform_font_40.c" "fonts/waveform_font_48.c"
    INCLUDE_DIRS "." "../third_party" "../../visualizer/main" "../../mic_test/main"
                 "../../common" "../../common/music_model"
    EMBED_FILES "fonts/waveform_unicode.bin"
    EMBED_TXTFILES "ota_public.pem"
    REQUIRES esp32_p4_wifi6_touch_lcd_7b lvgl__lvgl
    PRIV_REQUIRES esp_psram esp_system spi_flash esp_wifi esp_event esp_netif
                  esp_http_server esp_timer nvs_flash esp_driver_sdmmc heap
                  esp_http_client mbedtls esp_new_jpeg esp-dsp esp_codec_dev esp_driver_parlio
                  espressif__esp-tflite-micro esp_partition app_update)

# Optimise the continuously running fingerprint only; retain IEEE floating-point semantics.
set_source_files_properties(fingerprint.cpp PROPERTIES COMPILE_OPTIONS "-O3")
set_source_files_properties("../../common/music_features.cpp" PROPERTIES COMPILE_OPTIONS "-O3")

target_compile_definitions(${COMPONENT_LIB} PRIVATE CJSON_NESTING_LIMIT=32)

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    idf_component_get_property(esp_hosted_lib espressif__esp_hosted COMPONENT_LIB)
    idf_component_get_property(sdmmc_driver_lib esp_driver_sdmmc COMPONENT_LIB)
    target_link_libraries(${esp_hosted_lib} PRIVATE ${sdmmc_driver_lib})
    idf_component_get_property(esp_hosted_dir espressif__esp_hosted COMPONENT_DIR)
    include("${CMAKE_CURRENT_LIST_DIR}/../cmake/hosted_rx_ownership.cmake")
    waveform_fix_hosted_rx(${esp_hosted_lib} "${esp_hosted_dir}" "${CMAKE_CURRENT_LIST_DIR}")
endif()

# The vendor's C aggregate macro intentionally zero-initializes newer optional
# esp_lvgl_adapter fields. Keep C++ warnings strict except for that C interop case.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers)

# Recognition-service credentials from the untracked acrcloud.local at the
# repository root for one-time provisioning. OTA builds load credentials from
# device NVS; public artifacts must never embed the local secrets.
set(acrcloud_file "${CMAKE_CURRENT_LIST_DIR}/../../../../acrcloud.local")
set(ACRCLOUD_HOST "")
set(ACRCLOUD_ACCESS_KEY "")
set(ACRCLOUD_ACCESS_SECRET "")
if(EXISTS "${acrcloud_file}" AND NOT DEFINED ENV{WAVEFORM_RELEASE_VERSION})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${acrcloud_file}")
    file(STRINGS "${acrcloud_file}" acrcloud_lines)
    foreach(line IN LISTS acrcloud_lines)
        if(line MATCHES "^(host|access_key|access_secret)=([A-Za-z0-9._-]+)$")
            string(TOUPPER "${CMAKE_MATCH_1}" acrcloud_name)
            set(ACRCLOUD_${acrcloud_name} "${CMAKE_MATCH_2}")
        endif()
    endforeach()
endif()
if((NOT ACRCLOUD_HOST OR NOT ACRCLOUD_ACCESS_KEY OR NOT ACRCLOUD_ACCESS_SECRET)
   AND NOT DEFINED ENV{WAVEFORM_DEVICE_ACRCLOUD})
    message(FATAL_ERROR "ACRCloud keys missing from ${acrcloud_file}. Copy acrcloud.local "
        "from the main checkout (needs host=, access_key= and access_secret= lines), then run "
        "idf.py reconfigure. Set WAVEFORM_DEVICE_ACRCLOUD=1 only to build firmware that loads provisioned device credentials.")
endif()
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/acrcloud_config.h" CONTENT
"#pragma once
// Generated from acrcloud.local; never commit.
#define ACRCLOUD_HOST \"@ACRCLOUD_HOST@\"
#define ACRCLOUD_ACCESS_KEY \"@ACRCLOUD_ACCESS_KEY@\"
#define ACRCLOUD_ACCESS_SECRET \"@ACRCLOUD_ACCESS_SECRET@\"
" @ONLY)
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# Audit the experimental task stack before any hardware trial.
set_source_files_properties(p4_led.cpp PROPERTIES COMPILE_OPTIONS "-fstack-usage")

# Shazam startup calls the FFT self-test on the recognition task stack.
set_source_files_properties(fingerprint.cpp PROPERTIES COMPILE_OPTIONS "-O3;-Werror=frame-larger-than=4096;-fstack-usage")
