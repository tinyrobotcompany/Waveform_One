// Compile the actual controller adapter with fake SDK I/O, not a second adapter.
#include "../main/usb_controller.cpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace {
unsigned state_updates = 0;
unsigned activity_updates = 0;
bool music_active = false;
audio_activity::State music_state = audio_activity::State::Unknown;
void activity_status(audio_activity::State state) {
    ++activity_updates;
    music_state = state;
    music_active = state == audio_activity::State::Playing;
}
void controller_status(LedControllerState) { ++state_updates; }
void receive(std::string_view text) {
    test_cdc_config.data_cb(reinterpret_cast<const uint8_t *>(text.data()), text.size(), nullptr);
}
void drain() { Event event{}; while (xQueueReceive(events, &event, 0) == pdTRUE) process_event(event); }
std::string packet(unsigned id, unsigned sequence, int16_t value)
{
    std::string hex;
    uint32_t hash = 2166136261u;
    char pair[3];
    for (unsigned i = 0; i < audio_stream::kPacketBytes; ++i) {
        const uint8_t byte = i % 2 == 0 ? uint8_t(uint16_t(value)) : uint8_t(uint16_t(value) >> 8);
        hash = (hash ^ byte) * 16777619u;
        std::snprintf(pair, sizeof(pair), "%02x", byte);
        hex += pair;
    }
    char tail[16];
    std::snprintf(tail, sizeof(tail), " %08x\n", static_cast<unsigned>(hash));
    return "WF1 " + std::to_string(id) + " PCM " + std::to_string(sequence) + " " + hex + tail;
}
// The id of the most recent STREAM request sent to the S3.
unsigned stream_id()
{
    for (auto it = test_transmissions.rbegin(); it != test_transmissions.rend(); ++it) {
        if (it->find(" STREAM\n") != std::string::npos) return unsigned(std::stoul(it->substr(4)));
    }
    return 0;
}
unsigned stream_requests()
{
    unsigned count = 0;
    for (const auto &sent : test_transmissions) count += sent.find(" STREAM\n") != std::string::npos;
    return count;
}
}

int main()
{
    assert(usb_controller_audio() == nullptr);
    usb_controller_start(controller_status, activity_status);
    const auto *audio = usb_controller_audio();
    assert(audio != nullptr && audio->written() == 0);
    open_controller(0x303a, 0x1001);
    // Connecting asks for the LED style and then the audio stream.
    assert(stream_requests() == 1 && test_transmissions.back() == audio_stream::request(stream_id()));
    // Existing S3 diagnostics drive recognition without changing its firmware.
    receive("BEAT OPEN RMS=0.01 GATE_RMS=0.01 DISPLAY=BARS BANDS=8 |123|\n");
    poll_activity(); assert(music_active);
    const unsigned initial_updates = activity_updates;
    poll_activity();
    assert(music_active && activity_updates == initial_updates + 1); // retention heartbeat
    // Invalid and truncated diagnostics cannot change activity.
    receive("WF1 4 PCM 0 SHUT RMS=0 GATE_RMS=0 DISPLAY=GATE_CLOSED |0|\n");
    receive("     SHUT RMS=0 GATE_RMS=0 DISPLAY=GATE_CLOSED\n");
    poll_activity(); assert(music_active);
    const std::string quiet = "     SHUT RMS=0.0001 GATE_RMS=0 DISPLAY=GATE_CLOSED BANDS=0 |000|\n";
    receive(quiet); receive(quiet); poll_activity(); assert(music_active);
    receive(quiet); poll_activity(); assert(music_state == audio_activity::State::Quiet);
    const std::string playing = "     OPEN RMS=0.001 GATE_RMS=0.001 DISPLAY=BARS BANDS=8 |123|\n";
    // Fragmented diagnostics coexist with PCM and style acknowledgements.
    receive(playing.substr(0, 13)); receive(playing.substr(13));
    poll_activity(); assert(music_active);
    test_time_us += 3000000; poll_activity(); assert(music_state == audio_activity::State::Unknown);
    const unsigned unknown_updates = activity_updates;
    poll_activity(); assert(activity_updates == unknown_updates); // no fake quiet heartbeat
    receive(playing); poll_activity(); assert(music_active);
    // The stream fills the recognition ring, alongside diagnostics and replies.
    unsigned id = stream_id();
    const auto first_segment = audio->read(0, nullptr, 0).segment;
    receive("WF1 " + std::to_string(id) + " AUDIO 16000 STREAM\n");
    assert(usb_controller_set_style(LedStyle::Waterfall) == StyleRequestResult::Queued);
    drain();
    receive("WF1 " + std::to_string(request_id) + " OK MODE waterfall\n"); drain();
    for (unsigned i = 0; i < 250; ++i) {
        const std::string line = packet(id, i, int16_t(i));
        receive(line.substr(0, 100)); receive(line.substr(100)); // fragmented
        if (i % 50 == 0) receive(playing);
        test_time_us += 8000;
    }
    assert(audio->written() == 250 * 128);
    int16_t samples[256];
    auto read = audio->read(249 * 128, samples, 256);
    assert(read.count == 128 && samples[0] == 249 && read.segment == first_segment);
    // A healthy stream is left alone.
    const unsigned requests = stream_requests();
    poll_stream();
    assert(stream_requests() == requests);

    // Two seconds without audio: restart it as a new recording.
    test_time_us += 2000000;
    poll_stream();
    assert(stream_requests() == requests + 1 && stream_id() != id);
    assert(audio->read(0, samples, 0).segment != first_segment);
    // Packets for the old stream are ignored.
    receive(packet(id, 250, 1));
    assert(audio->written() == 250 * 128);
    id = stream_id();
    receive("WF1 " + std::to_string(id) + " AUDIO 16000 STREAM\n");
    receive(packet(id, 0, 7));
    assert(audio->written() == 251 * 128);

    // A gap in the sequence, refusal and lost audio all restart it, at most
    // once a second.
    receive(packet(id, 2, 7));
    poll_stream();
    assert(stream_requests() == requests + 1);
    test_time_us += 1000000;
    poll_stream();
    assert(stream_requests() == requests + 2);
    id = stream_id();
    receive("WF1 " + std::to_string(id) + " ERR BUSY\n");
    test_time_us += 1000000; poll_stream();
    assert(stream_requests() == requests + 3);
    id = stream_id();
    receive("WF1 " + std::to_string(id) + " AUDIO 16000 STREAM\n");
    receive("WF1 " + std::to_string(id) + " ERR AUDIO_LOST\n");
    test_time_us += 1000000; poll_stream();
    assert(stream_requests() == requests + 4);
    // An unanswered request is repeated after two seconds; a failed one is
    // retried a second later.
    test_tx_result = -2;
    test_time_us += 1000000; poll_stream();
    assert(stream_requests() == requests + 4);
    test_time_us += 1000000; poll_stream();
    assert(stream_requests() == requests + 5);
    test_tx_result = ESP_OK;
    test_time_us += 1000000; poll_stream();
    assert(stream_requests() == requests + 6);

    // Disconnecting ends the stream; nothing is requested until reconnected.
    Event event{}; event.type = EventType::SetStyle;
    for (unsigned i = 0; i < 8; ++i) assert(xQueueSend(events, &event, 0) == pdTRUE);
    cdc_acm_host_dev_event_data_t disconnected{}; disconnected.type = CDC_ACM_HOST_DEVICE_DISCONNECTED;
    test_cdc_config.event_cb(&disconnected, nullptr);
    const auto connected_segment = audio->read(0, samples, 0).segment;
    poll_disconnect(); poll_activity(); assert(!music_active);
    assert(controller == nullptr && !controller_available.load()); drain();
    assert(audio->read(0, samples, 0).segment != connected_segment);
    const unsigned disconnected_requests = stream_requests();
    test_time_us += 5000000; poll_stream();
    assert(stream_requests() == disconnected_requests);
    const unsigned reported = state_updates;
    event.type = EventType::Reply; event.reply.ok = true; event.reply.mode = requested_mode;
    event.generation = controller_generation.load(); process_event(event);
    assert(state_updates == reported);
    open_controller(0x303a, 0x1001);
    assert(stream_requests() == disconnected_requests + 1);
    const unsigned current_updates = state_updates;
    event.type = EventType::Reply; event.reply.ok = true; event.reply.mode = requested_mode;
    event.generation = controller_generation.load() - 1; process_event(event);
    assert(state_updates == current_updates);
    close_controller();
}
