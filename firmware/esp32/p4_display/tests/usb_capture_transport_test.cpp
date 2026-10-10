// Compile the actual controller adapter with fake SDK I/O, not a second adapter.
#include "../main/usb_controller.cpp"
#include <cassert>
#include <thread>
#include <vector>

namespace {
unsigned completions = 0;
AudioCaptureStatus last_status = AudioCaptureStatus::Invalid;
uint8_t *completed_pcm = nullptr;
size_t completed_bytes = 0;
unsigned state_updates = 0;
unsigned activity_updates = 0;
bool music_active = false;
void activity_status(bool active) { ++activity_updates; music_active = active; }
void controller_status(LedControllerState) { ++state_updates; }
void completed(AudioCaptureStatus status, uint8_t *pcm, size_t bytes) {
    ++completions; last_status = status; completed_pcm = pcm; completed_bytes = bytes;
}
void receive(std::string_view text) {
    test_cdc_config.data_cb(reinterpret_cast<const uint8_t *>(text.data()), text.size(), nullptr);
}
void drain() { Event event{}; while (xQueueReceive(events, &event, 0) == pdTRUE) process_event(event); }
unsigned begin() {
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::Queued);
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::Busy);
    drain(); return request_id;
}
void failed(AudioCaptureStatus expected) {
    assert(last_status == expected && completed_pcm == nullptr && completed_bytes == 0);
    assert(!capture_reserved.load() && test_allocations.empty());
}
}

int main()
{
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::Unavailable);
    usb_controller_start(controller_status, activity_status);
    assert(usb_controller_capture(nullptr) == AudioCaptureRequestResult::Unavailable);
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::Disconnected);
    open_controller(0x303a, 0x1001);
    // Existing S3 diagnostics drive recognition without changing its firmware.
    receive("BEAT OPEN RMS=0.01 GATE_RMS=0.01 DISPLAY=BARS BANDS=8 |123|\n");
    poll_activity(); assert(music_active && activity_updates == 1);
    // Invalid and truncated diagnostics cannot change activity.
    receive("WF1 4 PCM 0 SHUT RMS=0 GATE_RMS=0 DISPLAY=GATE_CLOSED |0|\n");
    receive("     SHUT RMS=0 GATE_RMS=0 DISPLAY=GATE_CLOSED\n");
    poll_activity(); assert(music_active && activity_updates == 1);
    const std::string quiet = "     SHUT RMS=0.0001 GATE_RMS=0 DISPLAY=GATE_CLOSED BANDS=0 |000|\n";
    receive(quiet); receive(quiet); poll_activity(); assert(music_active);
    receive(quiet); poll_activity(); assert(!music_active && activity_updates == 2);
    const std::string playing = "     OPEN RMS=0.001 GATE_RMS=0.001 DISPLAY=BARS BANDS=8 |123|\n";
    // Fragmented diagnostics coexist with PCM and style acknowledgements.
    receive(playing.substr(0, 13)); receive(playing.substr(13));
    poll_activity(); assert(music_active);
    test_time_us += 3000000; poll_activity(); assert(!music_active);
    receive(playing); poll_activity(); assert(music_active);
    const unsigned id = begin();
    const std::string prefix = "WF1 " + std::to_string(id);
    receive(prefix + " AUDIO 16000 128000\n");
    // Style requests and acknowledgements share the same physical USB owner.
    assert(usb_controller_set_style(LedStyle::Waterfall) == StyleRequestResult::Queued);
    drain();
    receive("WF1 " + std::to_string(request_id) + " OK MODE waterfall\n"); drain();
    for (unsigned i = 0; i < 1000; ++i)
        receive(prefix + " PCM " + std::to_string(i) + " " + std::string(512, '0') + " e6a1d1c5\n");
    receive(prefix + " END 1000\n");
    poll_capture();
    assert(completions == 1 && last_status == AudioCaptureStatus::Complete);
    assert(completed_bytes == audio_capture::kBytes && completed_pcm != nullptr);
    assert(test_allocations.size() == 1); heap_caps_free(completed_pcm); completed_pcm = nullptr;
    poll_capture(); assert(completions == 1);

    auto next = begin(); receive("WF1 " + std::to_string(next) + " ERR BUSY\n");
    poll_capture(); failed(AudioCaptureStatus::Busy);
    next = begin(); receive("WF1 " + std::to_string(next) + " ERR AUDIO_LOST\n");
    poll_capture(); failed(AudioCaptureStatus::AudioLost);
    next = begin(); receive("WF1 " + std::to_string(next) + " AUDIO 16000 128000\n");
    receive("WF1 " + std::to_string(next) + " END 1000\n");
    poll_capture(); failed(AudioCaptureStatus::Invalid);
    begin(); test_time_us += audio_capture::kTimeoutMs * 1000LL - 1;
    const unsigned previous = completions; poll_capture(); assert(completions == previous);
    ++test_time_us; poll_capture(); failed(AudioCaptureStatus::TimedOut);

    test_no_memory = true; begin(); failed(AudioCaptureStatus::NoMemory); test_no_memory = false;
    test_tx_result = -2; begin(); failed(AudioCaptureStatus::TransportError); test_tx_result = ESP_OK;

    begin();
    Event event{}; event.type = EventType::SetStyle;
    for (unsigned i = 0; i < 8; ++i) assert(xQueueSend(events, &event, 0) == pdTRUE);
    cdc_acm_host_dev_event_data_t disconnected{}; disconnected.type = CDC_ACM_HOST_DEVICE_DISCONNECTED;
    test_cdc_config.event_cb(&disconnected, nullptr);
    poll_disconnect(); poll_activity(); assert(!music_active); failed(AudioCaptureStatus::Disconnected);
    assert(controller == nullptr && !controller_available.load()); drain();
    const unsigned reported = state_updates;
    event.type = EventType::Reply; event.reply.ok = true; event.reply.mode = requested_mode;
    event.generation = controller_generation.load(); process_event(event);
    assert(state_updates == reported);

    // A capture accepted on a previous USB session must not start on a new S3.
    open_controller(0x303a, 0x1001); poll_activity(); assert(!music_active);
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::Queued);
    test_cdc_config.event_cb(&disconnected, nullptr); poll_disconnect();
    open_controller(0x303a, 0x1001); drain(); failed(AudioCaptureStatus::Disconnected);
    const unsigned current_updates = state_updates;
    event.type = EventType::Reply; event.reply.ok = true; event.reply.mode = requested_mode;
    event.generation = controller_generation.load() - 1; process_event(event);
    assert(state_updates == current_updates);

    event.type = EventType::SetStyle;
    for (unsigned i = 0; i < 8; ++i) assert(xQueueSend(events, &event, 0) == pdTRUE);
    assert(usb_controller_capture(completed) == AudioCaptureRequestResult::QueueFull);
    assert(!capture_reserved.load()); drain();

    std::atomic_uint accepted{0};
    std::vector<std::thread> callers;
    for (unsigned i = 0; i < 16; ++i) callers.emplace_back([&] {
        if (usb_controller_capture(completed) == AudioCaptureRequestResult::Queued) ++accepted;
    });
    for (auto &caller : callers) caller.join();
    assert(accepted == 1); drain();
    test_time_us += audio_capture::kTimeoutMs * 1000LL;
    poll_capture(); failed(AudioCaptureStatus::TimedOut);
    close_controller();
}
