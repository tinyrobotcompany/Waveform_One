#pragma once
#include <cstdlib>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

using esp_err_t = int;
constexpr unsigned MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2;
constexpr int ESP_INTR_FLAG_LOWMED = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
inline int64_t test_time_us = 0;
inline bool test_no_memory = false;
inline std::set<void *> test_allocations;
inline void *heap_caps_malloc(size_t size, unsigned) {
    if (test_no_memory) return nullptr;
    void *pointer = std::malloc(size); if (pointer != nullptr) test_allocations.insert(pointer); return pointer;
}
inline void heap_caps_free(void *pointer) {
    if (pointer == nullptr) return;
    assert(test_allocations.erase(pointer) == 1); std::free(pointer);
}
inline int64_t esp_timer_get_time() { return test_time_us; }
inline const char *esp_err_to_name(int) { return "test error"; }
template<class... Args> void test_log(Args...) {}
#define ESP_LOGE(...) test_log(__VA_ARGS__)
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define ESP_LOGI(...) test_log(__VA_ARGS__)

using usb_device_handle_t = void *;
struct usb_device_desc_t { uint16_t idVendor = 0x303a, idProduct = 0x1001; };
struct usb_host_config_t { bool skip_phy_setup; unsigned intr_flags; };
inline int usb_host_install(const usb_host_config_t *) { return ESP_OK; }
inline int usb_host_lib_handle_events(TickType_t, uint32_t *) { return ESP_OK; }
inline int usb_host_get_device_descriptor(usb_device_handle_t, const usb_device_desc_t **descriptor) {
    static usb_device_desc_t device; *descriptor = &device; return ESP_OK;
}

using cdc_acm_dev_hdl_t = void *;
enum cdc_event_type { CDC_ACM_HOST_DEVICE_DISCONNECTED, CDC_ACM_HOST_ERROR };
struct cdc_acm_host_dev_event_data_t { cdc_event_type type; struct { int error; } data; };
struct cdc_acm_host_driver_config_t {
    unsigned driver_task_stack_size, driver_task_priority, xCoreID;
    void (*new_dev_cb)(usb_device_handle_t);
};
struct cdc_acm_host_device_config_t {
    unsigned connection_timeout_ms, out_buffer_size, in_buffer_size;
    void (*event_cb)(const cdc_acm_host_dev_event_data_t *, void *);
    bool (*data_cb)(const uint8_t *, size_t, void *);
};
inline cdc_acm_host_device_config_t test_cdc_config{};
inline int test_tx_result = ESP_OK;
inline std::vector<std::string> test_transmissions;
inline void (*test_tx_callback)(std::string_view) = nullptr;
inline int cdc_acm_host_install(const cdc_acm_host_driver_config_t *) { return ESP_OK; }
inline int cdc_acm_host_close(cdc_acm_dev_hdl_t) { return ESP_OK; }
inline int cdc_acm_host_open(uint16_t, uint16_t, unsigned,
    const cdc_acm_host_device_config_t *config, cdc_acm_dev_hdl_t *device) {
    test_cdc_config = *config; *device = reinterpret_cast<void *>(1); return ESP_OK;
}
inline int cdc_acm_host_data_tx_blocking(cdc_acm_dev_hdl_t, const uint8_t *bytes, size_t size, unsigned) {
    test_transmissions.emplace_back(reinterpret_cast<const char *>(bytes), size);
    if (test_tx_callback != nullptr) test_tx_callback(test_transmissions.back());
    return test_tx_result;
}
