#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace capture {
inline uint32_t checksum(const uint8_t *bytes, size_t count) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < count; i++)
    h = (h ^ bytes[i]) * 16777619u;
  return h;
}
// 31-tap Hamming-window low-pass, 6.5 kHz cutoff, then 48 -> 16 kHz.
// The recognition path never changes samples used by the visualizer.
class Samples {
  float history_[31]{};
  size_t position_ = 0;
  unsigned phase_ = 0;

public:
  template <class Sink> void push(int32_t word, Sink sink) {
    history_[position_] = float(word) / 65536.0f;
    position_ = (position_ + 1) % 31;
    if (++phase_ != 3)
      return;
    phase_ = 0;
    static constexpr float coefficients[31] = {
        0.0003318424f,  -0.0012488505f, -0.0029320274f, -0.0031544833f,
        0.0004403169f,  0.0078437878f,  0.0138277227f,  0.0098054889f,
        -0.0086126607f, -0.0334996351f, -0.0440503165f, -0.0174955331f,
        0.0538741492f,  0.1518139709f,  0.2373750892f,  0.2713622771f,
        0.2373750892f,  0.1518139709f,  0.0538741492f,  -0.0174955331f,
        -0.0440503165f, -0.0334996351f, -0.0086126607f, 0.0098054889f,
        0.0138277227f,  0.0078437878f,  0.0004403169f,  -0.0031544833f,
        -0.0029320274f, -0.0012488505f, 0.0003318424f};
    float sample = 0;
    for (size_t i = 0; i < 31; i++)
      sample += coefficients[i] * history_[(position_ + 30 - i) % 31];
    sink(int16_t(std::clamp(sample, -32768.0f, 32767.0f)));
  }
};
} // namespace capture
