#pragma once
#include "test_platform.h"
#include <algorithm>
#include <cstring>
constexpr int ESP_ERR_HTTP_EAGAIN = 0x7007;
enum { HTTP_EVENT_ON_HEADER, HTTP_METHOD_GET, HTTP_METHOD_POST };
struct esp_http_client_event_t { int event_id; const char *header_key; const char *header_value; void *user_data; };
struct esp_http_client_config_t {
    const char *url; void *user_data; int (*event_handler)(esp_http_client_event_t *);
    int (*crt_bundle_attach)(void *); int timeout_ms; bool disable_auto_redirect;
    int method; const char *user_agent;
};
struct TestHttp {
    int64_t declared = 0;
    int status = 200, reads = 0, closed = 0, cleaned = 0, eagain = 0;
    int open_result = ESP_OK;
    bool complete = true, read_error = false;
    std::string body, retry_after;
    size_t offset = 0, fragment = 3;
    int64_t read_duration_us = 1000;
    esp_http_client_config_t config{};
};
inline TestHttp test_http;
using esp_http_client_handle_t = TestHttp *;
inline auto esp_http_client_init(const esp_http_client_config_t *config) {
    test_http.config = *config;
    assert(config->crt_bundle_attach != nullptr && config->disable_auto_redirect);
    return &test_http;
}
inline int esp_http_client_close(esp_http_client_handle_t client) { ++client->closed; return ESP_OK; }
inline int esp_http_client_cleanup(esp_http_client_handle_t client) { ++client->cleaned; return ESP_OK; }
inline int esp_http_client_set_header(esp_http_client_handle_t, const char *, const char *) { return ESP_OK; }
inline int esp_http_client_open(esp_http_client_handle_t client, int) { return client->open_result; }
inline int esp_http_client_write(esp_http_client_handle_t, const char *, int length) { return length; }
inline int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client) {
    if (!client->retry_after.empty()) {
        esp_http_client_event_t event{HTTP_EVENT_ON_HEADER, "Retry-After", client->retry_after.c_str(), client->config.user_data};
        client->config.event_handler(&event);
    }
    return client->declared;
}
inline int esp_http_client_get_status_code(esp_http_client_handle_t client) { return client->status; }
inline int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int room) {
    ++client->reads; test_time_us += client->read_duration_us;
    if (client->eagain > 0) { --client->eagain; return -ESP_ERR_HTTP_EAGAIN; }
    if (client->read_error) return -1;
    const size_t count = std::min({client->fragment, size_t(room), client->body.size() - client->offset});
    std::memcpy(buffer, client->body.data() + client->offset, count); client->offset += count;
    return static_cast<int>(count);
}
inline bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client) { return client->complete; }
