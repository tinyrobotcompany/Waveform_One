#include "capture.h"
#include "capture_samples.h"
#include "esp_err.h"
#include "protocol_write.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <atomic>
#include <cstdio>
#include <mutex>
namespace {
struct Packet {
  unsigned id;
  unsigned sequence;
  uint8_t bytes[256];
};
QueueHandle_t packets = nullptr;
std::atomic<unsigned> active{0}, failed{0};
constexpr unsigned packet_count =
    1000; // eight seconds of mono 16 kHz signed PCM
// Serialize the producer with start/abort so no old frame can refill a reset
// queue. USB writes never hold this mutex.
std::mutex producer_mutex;
capture::Samples samples;
Packet building{};
unsigned position = 0;
} // namespace
void capture_init() {
  packets = xQueueCreate(32, sizeof(Packet));
  ESP_ERROR_CHECK(packets ? ESP_OK : ESP_ERR_NO_MEM);
}
bool capture_start(unsigned id) {
  std::lock_guard<std::mutex> lock(producer_mutex);
  if (!id || active.load())
    return false;
  xQueueReset(packets);
  samples = capture::Samples{};
  building = {};
  building.id = id;
  position = 0;
  failed.store(0);
  active.store(id);
  return true;
}
void capture_abort(unsigned id) {
  std::lock_guard<std::mutex> lock(producer_mutex);
  if (active.load() != id) return;
  active.store(0);
  xQueueReset(packets);
}
void capture_discontinuity() {
  std::lock_guard<std::mutex> lock(producer_mutex);
  const unsigned id = active.load();
  if (id)
    failed.store(id);
}
void capture_audio(const int32_t *stereo, size_t frames) {
  std::lock_guard<std::mutex> lock(producer_mutex);
  const unsigned id = active.load();
  if (!id || failed.load() == id) return;
  auto& packet = building;
  if (packet.sequence >= packet_count)
    return;
  for (size_t i = 0; i < frames; i++) {
    samples.push(stereo[2 * i], [&](int16_t sample) {
      if (packet.sequence >= packet_count)
        return;
      const uint16_t value = uint16_t(sample);
      packet.bytes[position++] = uint8_t(value);
      packet.bytes[position++] = uint8_t(value >> 8);
      if (position == 256) {
        if (xQueueSend(packets, &packet, 0) != pdTRUE)
          failed.store(id);
        ++packet.sequence;
        position = 0;
      }
    });
  }
}
void capture_send() {
  const unsigned id = active.load();
  if (!id)
    return;
  char line[600];
  if (failed.load() == id) {
    const int n = snprintf(line, sizeof(line), "\nWF1 %u ERR AUDIO_LOST\n", id);
    control::write_reply(line, n);
    capture_abort(id);
    return;
  }
  Packet packet{};
  // Bound work per control loop; UART diagnostics and LED tasks keep running.
  for (int i = 0; i < 12 && xQueueReceive(packets, &packet, 0) == pdTRUE; i++) {
    if (packet.id != id)
      continue;
    int n =
        snprintf(line, sizeof(line), "\nWF1 %u PCM %u ", id, packet.sequence);
    constexpr char hex[] = "0123456789abcdef";
    for (uint8_t b : packet.bytes) {
      line[n++] = hex[b >> 4];
      line[n++] = hex[b & 15];
    }
    n += snprintf(
        line + n, sizeof(line) - n, " %08lx\n",
        static_cast<unsigned long>(capture::checksum(packet.bytes, 256)));
    if (!control::write_reply(line, n)) {
      failed.store(id);
      return;
    }
    if (packet.sequence + 1 == packet_count) {
      n = snprintf(line, sizeof(line), "\nWF1 %u END %u\n", id, packet_count);
      control::write_reply(line, n);
      capture_abort(id);
      return;
    }
  }
}
