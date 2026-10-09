#include "usb_controller.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "wf1_protocol.h"

namespace {

constexpr char kTag[] = "waveform_usb";
constexpr uint32_t kTxTimeoutMs = 1000;

enum class EventType { DeviceFound, Disconnected, Reply, SetStyle };

struct Event {
    EventType type;
    uint16_t vid = 0;
    uint16_t pid = 0;
    wf1::Reply reply{};
    LedStyle style = LedStyle::Mirrored;
};

QueueHandle_t events = nullptr;
LedControllerStatusCallback report_status = nullptr;
cdc_acm_dev_hdl_t controller = nullptr;
std::atomic_bool controller_available{false};
unsigned request_id = 0;
wf1::Mode requested_mode = wf1::Mode::Mirrored;
wf1::Replies replies(1);

void report(LedControllerState state)
{
    if (report_status != nullptr) report_status(state);
}

void enqueue(const Event &event)
{
    if (events == nullptr || xQueueSend(events, &event, 0) != pdTRUE) {
        ESP_LOGW(kTag, "Dropped USB controller event");
    }
}

bool handle_rx(const uint8_t *data, size_t size, void *)
{
    for (size_t index = 0; index < size; ++index) {
        replies.push(static_cast<char>(data[index]), [](const wf1::Reply &reply) {
            Event event{};
            event.type = EventType::Reply;
            event.reply = reply;
            enqueue(event);
        });
    }
    return true;
}

void handle_device_event(const cdc_acm_host_dev_event_data_t *event, void *)
{
    if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
        Event message{};
        message.type = EventType::Disconnected;
        enqueue(message);
    } else if (event->type == CDC_ACM_HOST_ERROR) {
        ESP_LOGE(kTag, "CDC error %d", event->data.error);
    }
}

void handle_new_device(usb_device_handle_t device)
{
    const usb_device_desc_t *descriptor = nullptr;
    if (usb_host_get_device_descriptor(device, &descriptor) != ESP_OK || descriptor == nullptr) {
        ESP_LOGW(kTag, "Could not read USB device descriptor");
        return;
    }
    Event event{};
    event.type = EventType::DeviceFound;
    event.vid = descriptor->idVendor;
    event.pid = descriptor->idProduct;
    enqueue(event);
}

void usb_library_task(void *)
{
    usb_host_config_t host_config{};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LOWMED;
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    cdc_acm_host_driver_config_t driver_config{};
    driver_config.driver_task_stack_size = 4096;
    driver_config.driver_task_priority = 19;
    driver_config.xCoreID = 0;
    driver_config.new_dev_cb = handle_new_device;
    ESP_ERROR_CHECK(cdc_acm_host_install(&driver_config));
    ESP_LOGI(kTag, "USB host waiting for ESP32-S3");

    while (true) {
        uint32_t flags = 0;
        const esp_err_t result = usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (result != ESP_OK) ESP_LOGE(kTag, "USB host event error: %s", esp_err_to_name(result));
    }
}

void close_controller()
{
    controller_available.store(false);
    if (controller != nullptr) {
        const esp_err_t result = cdc_acm_host_close(controller);
        if (result != ESP_OK) ESP_LOGW(kTag, "CDC close failed: %s", esp_err_to_name(result));
        controller = nullptr;
    }
}

wf1::Mode protocol_mode(LedStyle style)
{
    switch (style) {
    case LedStyle::Classic: return wf1::Mode::Classic;
    case LedStyle::Mirrored: return wf1::Mode::Mirrored;
    case LedStyle::Waterfall: return wf1::Mode::Waterfall;
    }
    return wf1::Mode::Mirrored;
}

LedControllerState connected_state(wf1::Mode mode)
{
    switch (mode) {
    case wf1::Mode::Classic: return LedControllerState::ConnectedClassic;
    case wf1::Mode::Mirrored: return LedControllerState::ConnectedMirrored;
    case wf1::Mode::Waterfall: return LedControllerState::ConnectedWaterfall;
    }
    return LedControllerState::ConnectedMirrored;
}

void send_style(LedStyle style)
{
    if (controller == nullptr) {
        ESP_LOGW(kTag, "Cannot change LED style while controller is disconnected");
        return;
    }
    requested_mode = protocol_mode(style);
    request_id = request_id == 65535 ? 1 : request_id + 1;
    replies.expect(request_id);
    const std::string request = wf1::mode_request(request_id, requested_mode);
    const esp_err_t sent = cdc_acm_host_data_tx_blocking(
        controller, reinterpret_cast<const uint8_t *>(request.data()), request.size(), kTxTimeoutMs);
    if (sent != ESP_OK) {
        ESP_LOGE(kTag, "WF1 style request failed: %s", esp_err_to_name(sent));
        report(LedControllerState::ProtocolError);
    }
}

void open_controller(uint16_t vid, uint16_t pid)
{
    if (controller != nullptr) return;
    report(LedControllerState::Connecting);
    ESP_LOGI(kTag, "Opening CDC device VID=0x%04x PID=0x%04x", vid, pid);

    cdc_acm_host_device_config_t config{};
    config.connection_timeout_ms = 5000;
    config.out_buffer_size = 256;
    config.in_buffer_size = 512;
    config.event_cb = handle_device_event;
    config.data_cb = handle_rx;

    const esp_err_t result = cdc_acm_host_open(vid, pid, 0, &config, &controller);
    if (result != ESP_OK) {
        controller = nullptr;
        ESP_LOGE(kTag, "CDC open failed: %s", esp_err_to_name(result));
        report(LedControllerState::ProtocolError);
        return;
    }

    controller_available.store(true);
    send_style(LedStyle::Mirrored);
}

void controller_task(void *)
{
    report(LedControllerState::Waiting);
    while (true) {
        Event event{};
        if (xQueueReceive(events, &event, portMAX_DELAY) != pdTRUE) continue;
        switch (event.type) {
        case EventType::DeviceFound:
            open_controller(event.vid, event.pid);
            break;
        case EventType::Disconnected:
            close_controller();
            report(LedControllerState::Disconnected);
            ESP_LOGW(kTag, "ESP32-S3 disconnected");
            break;
        case EventType::Reply:
            if (event.reply.ok && event.reply.mode == requested_mode) {
                report(connected_state(event.reply.mode));
                ESP_LOGI(kTag, "ESP32-S3 style acknowledged: %.*s",
                         static_cast<int>(wf1::name(event.reply.mode).size()),
                         wf1::name(event.reply.mode).data());
            } else {
                report(LedControllerState::ProtocolError);
                ESP_LOGW(kTag, "ESP32-S3 returned an invalid WF1 reply");
            }
            break;
        case EventType::SetStyle:
            send_style(event.style);
            break;
        }
    }
}

StyleRequestResult enqueue_style(LedStyle style)
{
    if (events == nullptr) return StyleRequestResult::Unavailable;
    if (!controller_available.load()) return StyleRequestResult::Disconnected;
    Event event{};
    event.type = EventType::SetStyle;
    event.style = style;
    return xQueueSend(events, &event, 0) == pdTRUE
               ? StyleRequestResult::Queued
               : StyleRequestResult::QueueFull;
}

} // namespace

void usb_controller_start(LedControllerStatusCallback callback)
{
    report_status = callback;
    events = xQueueCreate(8, sizeof(Event));
    ESP_ERROR_CHECK(events != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(controller_task, "s3_control", 6144, nullptr, 10,
        nullptr, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(usb_library_task, "usb_host", 4096, nullptr, 20,
        nullptr, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

StyleRequestResult usb_controller_set_style(LedStyle style)
{
    return enqueue_style(style);
}
