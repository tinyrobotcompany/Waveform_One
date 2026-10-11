#define WAVEFORM_DEV_FLASH_BRIDGE
#include "../main/usb_controller.cpp"
#include <cassert>
#include <vector>
std::vector<uint8_t> raw_received;
bool observing = false;
bool development_bridge_observing() { return observing; }
void development_bridge_receive(const uint8_t *data, size_t size) { raw_received.insert(raw_received.end(), data, data + size); }
void completed(AudioCaptureStatus, uint8_t *pcm, size_t, uint64_t) { if (pcm) heap_caps_free(pcm); }
int main() {
    assert(!usb_controller_bridge_begin());
    usb_controller_start(nullptr);
    open_controller(0x303a, 0x1001);
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::Queued);
    assert(!usb_controller_bridge_begin()); // Never take over an accepted clip.
    Event event{}; assert(xQueueReceive(events, &event, 0) == pdTRUE); process_event(event);
    finish_capture(AudioCaptureStatus::Busy);
    assert(test_allocations.empty());
    assert(usb_controller_bridge_begin());
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::Unavailable);
    assert(usb_controller_set_style(LedStyle::Classic) == StyleRequestResult::Unavailable);
    assert(usb_controller_bridge_reset(true));
    const std::vector<std::pair<bool,bool>> expected{{false,false},{true,false},{true,true},{false,true},{false,false}};
    assert(test_control_lines == expected);
    const uint8_t packet[]{0xc0,0,0xff,0xdb,0xc0};
    test_cdc_config.data_cb(packet, sizeof(packet), nullptr);
    assert(raw_received == std::vector<uint8_t>(packet, packet+sizeof(packet)));
    assert(usb_controller_bridge_write(packet, sizeof(packet)));
    assert(!usb_controller_bridge_write(packet, 4097));
    const cdc_acm_host_dev_event_data_t disconnected{CDC_ACM_HOST_DEVICE_DISCONNECTED,{0}};
    handle_device_event(&disconnected, nullptr); poll_disconnect();
    assert(!usb_controller_bridge_write(packet, sizeof(packet)));
    const size_t sent = test_transmissions.size();
    open_controller(0x303a, 0x1001);
    assert(test_transmissions.size() == sent); // ROM must not receive WF1 style text.
    test_control_line_result = 1;
    assert(!usb_controller_bridge_reset(true));
    test_control_line_result = ESP_OK;
    usb_controller_bridge_end();
    assert(!bridge_active.load());
    observing = true;
    raw_received.clear();
    const char heartbeat[] = "WF1 MUSIC MUSIC\n";
    test_cdc_config.data_cb(reinterpret_cast<const uint8_t *>(heartbeat), sizeof(heartbeat)-1, nullptr);
    assert(!raw_received.empty());
    assert(activity.state(esp_timer_get_time()) == audio_activity::State::Playing);
    observing = false;
    assert(test_transmissions.size() == sent + 1);
    assert(usb_controller_bridge_begin());
    const size_t controls = test_control_lines.size();
    usb_controller_bridge_end(false);
    assert(test_control_lines.size() == controls); // Live diagnostics never reboot S3.
    assert(!bridge_active.load());
    assert(test_allocations.empty());
}
