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

struct Body {
    const void *data = nullptr;
    size_t size = 0;
    const char *content_type = nullptr;
    bool shazam = false;  // the recognition service's own request headers
};

// Keeps one HTTPS connection open to a single host between requests, saving
// a TCP and TLS handshake on every lookup. Any failure drops it.
struct Connection {
    esp_http_client_handle_t client = nullptr;
    Response *response = nullptr;  // the request in progress, for header events
    ~Connection() { drop(); }
    void drop()
    {
        if (client == nullptr) return;
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        client = nullptr;
    }
};

enum class Exchange { Ok, Stale, Failed };

// One request on an open client. Stale means nothing came back: on a reused
// connection the server may simply have closed it.
inline Exchange exchange(esp_http_client_handle_t client, const Body &body, size_t limit, Response &response)
{
    const int64_t deadline = esp_timer_get_time() + kRequestTimeoutUs;
    const size_t length = body.data != nullptr ? body.size : 0;
    const char *bytes = static_cast<const char *>(body.data);
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    if (body.data != nullptr) {
        esp_http_client_set_header(client, "Content-Type", body.content_type);
        if (body.shazam) {
            esp_http_client_set_header(client, "X-Shazam-Platform", "IPHONE");
            esp_http_client_set_header(client, "X-Shazam-AppVersion", "14.1.0");
            esp_http_client_set_header(client, "Accept-Language", "en-US");
        }
    }
    // An eight-second 8 kHz clip is 128 KB; nothing legitimate is larger.
    if (length > 256 * 1024) return Exchange::Failed;
    const esp_err_t opened = esp_http_client_open(client, static_cast<int>(length));
    if (opened != ESP_OK) {
        ESP_LOGW(kTag, "HTTP open failed: %s", esp_err_to_name(opened));
        return Exchange::Stale;
    }
    size_t written = 0;
    while (written < length && esp_timer_get_time() < deadline) {
        const int count = esp_http_client_write(client, bytes + written, length - written);
        if (count <= 0) return Exchange::Stale;
        written += count;
    }
    if (written != length || esp_timer_get_time() >= deadline) return Exchange::Failed;
    const int64_t declared = esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (declared < 0 && status == 0) return Exchange::Stale;
    if (declared < 0 || declared > static_cast<int64_t>(limit) || status != 200) {
        ESP_LOGW(kTag, "HTTP response rejected: status=%d declared_bytes=%lld limit=%u retry_after_s=%lld",
            status, static_cast<long long>(declared), static_cast<unsigned>(limit), static_cast<long long>(response.retry_after_us / 1000000));
        return Exchange::Failed;
    }
    while (esp_timer_get_time() < deadline) {
        // Read one extra byte to detect bodies exceeding the maximum, including chunked bodies.
        const size_t room = limit + 1 - response.size;
        if (room == 0) return Exchange::Failed;
        const int count = esp_http_client_read(client, response.bytes.get() + response.size,
            std::min<size_t>(room, 4096));
        if (count == -ESP_ERR_HTTP_EAGAIN) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        if (count < 0) return Exchange::Failed;
        if (count == 0) {
            if (!esp_http_client_is_complete_data_received(client)) return Exchange::Failed;
            response.bytes.get()[response.size] = '\0';
            return response.size <= limit ? Exchange::Ok : Exchange::Failed;
        }
        response.size += count;
        if (response.size > limit) return Exchange::Failed;
    }
    return Exchange::Failed;
}

// With a connection, it stays open for the next request to the same host.
inline bool fetch(const char *url, const Body &body, size_t limit, Response &response,
                  Connection *connection = nullptr)
{
    response = {};
    response.bytes.reset(static_cast<char *>(heap_caps_malloc(limit + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!response.bytes) return false;
    Connection local;
    Connection &link = connection != nullptr ? *connection : local;
    const bool reused = link.client != nullptr;
    link.response = &response;
    const auto method = body.data == nullptr ? HTTP_METHOD_GET : HTTP_METHOD_POST;
    if (reused) {
        esp_http_client_set_url(link.client, url);
        esp_http_client_set_method(link.client, method);
    } else {
        esp_http_client_config_t config{};
        config.url = url;
        config.user_data = &link;
        config.event_handler = [](esp_http_client_event_t *event) -> esp_err_t {
            auto *owner = static_cast<Connection *>(event->user_data);
            if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key != nullptr && owner->response != nullptr
                && event->header_value != nullptr && strcasecmp(event->header_key, "Retry-After") == 0) {
                owner->response->retry_after_us = recognition_policy::retry_after(event->header_value, std::time(nullptr));
            }
            return ESP_OK;
        };
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.timeout_ms = 5000;
        config.disable_auto_redirect = true;
        config.method = method;
        config.user_agent = body.shazam ? "Dalvik/2.1.0 (Linux; U; Android 5.0.2; VS980 4G Build/LRX22G)"
            : "WaveformOne/1.0";
        link.client = esp_http_client_init(&config);
        if (link.client == nullptr) return false;
    }
    const Exchange result = exchange(link.client, body, limit, response);
    link.response = nullptr;
    if (result == Exchange::Ok && connection != nullptr) return true;
    link.drop();
    // A kept connection the server had closed: try once on a fresh one.
    if (result == Exchange::Stale && reused) return fetch(url, body, limit, response, connection);
    return result == Exchange::Ok;
}

inline bool fetch(const char *url, const char *json, size_t limit, Response &response)
{
    Body body;
    if (json != nullptr) body = {json, std::strlen(json), "application/json", true};
    return fetch(url, body, limit, response);
}

} // namespace recognition_http
