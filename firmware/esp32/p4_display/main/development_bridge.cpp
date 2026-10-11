#ifdef WAVEFORM_DEV_FLASH_BRIDGE
#include "usb_controller.h"
#include "development_bridge_policy.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

namespace {
constexpr size_t kBufferBytes = 1024 * 1024;
constexpr uint16_t kPort = 8765;
StreamBufferHandle_t received = nullptr;
StaticStreamBuffer_t stream_storage{};
std::atomic_bool overflowed{false};
std::atomic_bool observing{false};
char key[65]{};
int64_t issued = 0;
bool send_all(int client, const uint8_t *bytes, size_t size) {
    while (size) {
        const int sent = send(client, bytes, size, 0);
        if (sent <= 0) return false;
        bytes += sent; size -= sent;
    }
    return true;
}
bool authenticate(int client, char &mode) {
    char header[66];
    size_t used = 0;
    const int64_t deadline = esp_timer_get_time() + 3000000;
    while (used < sizeof(header)) {
        const int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) return false;
        timeval timeout{long(remaining / 1000000), long(remaining % 1000000)};
        fd_set descriptors; FD_ZERO(&descriptors); FD_SET(client, &descriptors);
        if (select(client + 1, &descriptors, nullptr, nullptr, &timeout) <= 0) return false;
        const int count = recv(client, header + used, sizeof(header) - used, 0);
        if (count <= 0) return false;
        used += count;
    }
    mode = header[65];
    return header[64] == '\n' && (mode == 'L' || mode == 'R' || mode == 'T') &&
        development_bridge::authorize(key, {header, 64}, issued, esp_timer_get_time());
}
void session(int client) {
    // No USB takeover/reset before physical-access authentication succeeds.
    char mode = 0;
    if (!authenticate(client, mode)) return;
    const bool live = mode != 'R', observe = mode == 'T';
    if (!observe && !usb_controller_bridge_begin()) {
        const uint8_t busy[] = "BUSY!\n";
        send_all(client, busy, sizeof(busy) - 1); return;
    }
    uint8_t bytes[4096];
    while (xStreamBufferReceive(received, bytes, sizeof(bytes), 0)) {}
    overflowed.store(false);
    if (observe) observing.store(true);
    if (!live && !usb_controller_bridge_reset(true)) { usb_controller_bridge_end(); return; }
    const uint8_t ready[] = "READY\n";
    if (!send_all(client, ready, sizeof(ready) - 1)) {
        if (observe) observing.store(false); else usb_controller_bridge_end(!live);
        return;
    }
    int64_t activity = esp_timer_get_time();
    while (!overflowed.load() && esp_timer_get_time() - activity < development_bridge::kIdleUs) {
        fd_set descriptors; FD_ZERO(&descriptors); FD_SET(client, &descriptors);
        timeval timeout{0, 5000};
        const int readable = select(client + 1, &descriptors, nullptr, nullptr, &timeout);
        if (readable < 0) break;
        if (readable > 0) {
            const int count = recv(client, bytes, sizeof(bytes), 0);
            if (count <= 0 || observe || !usb_controller_bridge_write(bytes, count)) break;
            activity = esp_timer_get_time();
        }
        const size_t count = xStreamBufferReceive(received, bytes, sizeof(bytes), 0);
        if (count) {
            if (!send_all(client, bytes, count)) break;
            activity = esp_timer_get_time();
        }
    }
    // Return to the ordinary S3 application even after a failed/disconnected client.
    if (observe) observing.store(false); else usb_controller_bridge_end(!live);
}
void bridge_task(void *) {
    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_port = htons(kPort); address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (server < 0 || bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(server, 1)) {
        if (server >= 0) close(server);
        ESP_LOGE("waveform_bridge", "Development bridge could not listen");
        vTaskDelete(nullptr); return;
    }
    while (esp_timer_get_time() - issued < development_bridge::kActivationUs) {
        fd_set descriptors; FD_ZERO(&descriptors); FD_SET(server, &descriptors);
        timeval wait{1, 0};
        if (select(server + 1, &descriptors, nullptr, nullptr, &wait) <= 0) continue;
        const int client = accept(server, nullptr, nullptr);
        if (client < 0) continue;
        const timeval timeout{3, 0};
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        session(client);
        shutdown(client, SHUT_RDWR); close(client);
    }
    close(server);
    vTaskDelete(nullptr);
}
}
bool development_bridge_observing() { return observing.load(); }
void development_bridge_receive(const uint8_t *data, size_t size) {
    if (!received || xStreamBufferSend(received, data, size, 0) != size) overflowed.store(true);
}
void development_bridge_start() {
    auto *memory = static_cast<uint8_t *>(heap_caps_malloc(kBufferBytes + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!memory) { ESP_LOGE("waveform_bridge", "Development bridge buffer unavailable"); return; }
    received = xStreamBufferCreateStatic(kBufferBytes + 1, 1, memory, &stream_storage);
    if (!received) { heap_caps_free(memory); return; }
    uint8_t random[32]; esp_fill_random(random, sizeof(random));
    constexpr char hex[] = "0123456789abcdef";
    for (size_t index = 0; index < sizeof(random); ++index) {
        key[index * 2] = hex[random[index] >> 4]; key[index * 2 + 1] = hex[random[index] & 15];
    }
    issued = esp_timer_get_time();
    // Key available only on the physically connected debug console, never HTTP.
    printf("DEVELOPMENT_FLASH_KEY=%s\n", key);
    if (xTaskCreatePinnedToCore(bridge_task, "dev_flash_bridge", 8192, nullptr, 5, nullptr, 1) != pdPASS)
        ESP_LOGE("waveform_bridge", "Development bridge task unavailable");
}
#endif
