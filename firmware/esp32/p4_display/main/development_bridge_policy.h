#pragma once
#include <cstdint>
#include <string_view>
namespace development_bridge {
constexpr int64_t kActivationUs = 600000000, kIdleUs = 30000000;
inline bool authorize(std::string_view expected, std::string_view provided, int64_t issued, int64_t now) {
    if (expected.size() != 64 || provided.size() != 64 || now < issued || now - issued >= kActivationUs) return false;
    unsigned mismatch = 0;
    for (unsigned index = 0; index < 64; ++index) mismatch |= unsigned(expected[index] ^ provided[index]);
    return mismatch == 0;
}
}
