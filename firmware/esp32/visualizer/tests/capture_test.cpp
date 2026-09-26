#include "capture_samples.h"
#include <cassert>
#include <cmath>
#include <vector>
int main() {
  capture::Samples samples;
  std::vector<int16_t> output;
  for (int i = 0; i < 48000; i++)
    samples.push(int32_t(0.25 * 2147483647.0),
                 [&](int16_t s) { output.push_back(s); });
  assert(output.size() == 16000);
  assert(std::abs(output.back() - 8192) < 3);
  auto rms = [](float hz) {
    capture::Samples s;
    double power = 0;
    int n = 0;
    for (int i = 0; i < 48000; i++)
      s.push(int32_t(0.5 * 2147483647.0 *
                     std::sin(i * 2 * 3.141592653589793 * hz / 48000)),
             [&](int16_t v) {
               if (n++ > 100)
                 power += double(v) * v;
             });
    return std::sqrt(power / (n - 101));
  };
  assert(rms(1000) > 10000);
  assert(rms(12000) < 500);
  const uint8_t hello[] = {'h', 'e', 'l', 'l', 'o'};
  assert(capture::checksum(hello, 5) == 0x4f9f2cab);
}
