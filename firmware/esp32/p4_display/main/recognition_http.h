#pragma once
#include <algorithm>
#include <cstring>
#include <ctime>
#include <memory>
#include <strings.h>
#include "recognition_policy.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace recognition_http {
inline constexpr char kTag[] = "waveform_recognition";
inline constexpr int64_t kRequestTimeoutUs = 25000000;
struct Free { void operator()(void *pointer) const { heap_caps_free(pointer); } };
struct Response {
    std::unique_ptr<char, Free> bytes;
    size_t size = 0;
    int64_t retry_after_us = 0;
};

inline bool fetch(const char *url, const char *body, size_t limit, Response &response)
{
    response = {};
    response.bytes.reset(static_cast<char *>(heap_caps_malloc(limit + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!response.bytes) return false;
    esp_http_client_config_t config{};
    config.url = url;
    config.user_data = &response;
    config.event_handler = [](esp_http_client_event_t *event) -> esp_err_t {
        if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key != nullptr
            && event->header_value != nullptr && strcasecmp(event->header_key, "Retry-After") == 0) {
            auto *reply = static_cast<Response *>(event->user_data);
            reply->retry_after_us = recognition_policy::retry_after(event->header_value, std::time(nullptr));
        }
        return ESP_OK;
    };
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 5000;
    config.disable_auto_redirect = true;
    config.method = body == nullptr ? HTTP_METHOD_GET : HTTP_METHOD_POST;
    config.user_agent = "Dalvik/2.1.0 (Linux; U; Android 5.0.2; VS980 4G Build/LRX22G)";
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) return false;
    struct Cleanup {
        esp_http_client_handle_t client;
        ~Cleanup() { esp_http_client_close(client); esp_http_client_cleanup(client); }
    } cleanup{client};
    const int64_t deadline = esp_timer_get_time() + kRequestTimeoutUs;
    const size_t length = body != nullptr ? std::strlen(body) : 0;
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    if (body != nullptr) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "X-Shazam-Platform", "IPHONE");
        esp_http_client_set_header(client, "X-Shazam-AppVersion", "14.1.0");
        esp_http_client_set_header(client, "Accept-Language", "en-US");
    }
    if (length > 65536) return false;
    const esp_err_t opened = esp_http_client_open(client, static_cast<int>(length));
    if (opened != ESP_OK) {
        ESP_LOGW(kTag, "HTTP open failed: %s", esp_err_to_name(opened));
        return false;
    }
    size_t written = 0;
    while (written < length && esp_timer_get_time() < deadline) {
        const int count = esp_http_client_write(client, body + written, length - written);
        if (count <= 0) return false;
        written += count;
    }
    if (written != length || esp_timer_get_time() >= deadline) return false;
    const int64_t declared = esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (declared < 0 || declared > static_cast<int64_t>(limit) || status != 200) {
        ESP_LOGW(kTag, "HTTP response rejected: status=%d declared_bytes=%lld limit=%u retry_after_s=%lld",
            status, static_cast<long long>(declared), static_cast<unsigned>(limit), static_cast<long long>(response.retry_after_us / 1000000));
        return false;
    }
    while (esp_timer_get_time() < deadline) {
        // Read one extra byte to detect bodies exceeding the maximum, including chunked bodies.
        const size_t room = limit + 1 - response.size;
        if (room == 0) return false;
        const int count = esp_http_client_read(client, response.bytes.get() + response.size,
            std::min<size_t>(room, 4096));
        if (count == -ESP_ERR_HTTP_EAGAIN) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        if (count < 0) return false;
        if (count == 0) {
            if (!esp_http_client_is_complete_data_received(client)) return false;
            response.bytes.get()[response.size] = '\0';
            return response.size <= limit;
        }
        response.size += count;
        if (response.size > limit) return false;
    }
    return false;
}

} // namespace recognition_http
