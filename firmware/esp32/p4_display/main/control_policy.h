#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "usb_controller.h"

namespace control_policy {

constexpr std::size_t kMaxWifiNetworks = 16;

struct WifiNetwork {
    char ssid[33]{};
};

struct WifiNetworkList {
    WifiNetwork items[kMaxWifiNetworks]{};
    std::size_t count = 0;
};

inline bool remote_token_matches(std::string_view expected, std::string_view presented)
{
    uint8_t difference = static_cast<uint8_t>(expected.size() ^ presented.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const uint8_t actual = index < presented.size()
                                   ? static_cast<uint8_t>(presented[index])
                                   : 0;
        difference |= static_cast<uint8_t>(expected[index]) ^ actual;
    }
    return difference == 0 && !expected.empty();
}

inline bool valid_brightness(int percent)
{
    return percent >= 10 && percent <= 100;
}

inline bool safe_codepoint(uint32_t value)
{
    if (value < 0x20 || (value >= 0x7f && value <= 0x9f)) return false;
    if (value == 0x2028 || value == 0x2029) return false;
    if ((value >= 0x202a && value <= 0x202e) || (value >= 0x2066 && value <= 0x2069)) {
        return false;
    }
    return value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
}

inline bool safe_utf8_ssid(const uint8_t *bytes, std::size_t size)
{
    if (bytes == nullptr || size == 0 || size > 32) return false;
    for (std::size_t index = 0; index < size;) {
        const uint8_t first = bytes[index++];
        uint32_t codepoint = 0;
        std::size_t continuation_count = 0;
        if (first <= 0x7f) {
            codepoint = first;
        } else if (first >= 0xc2 && first <= 0xdf) {
            codepoint = first & 0x1f;
            continuation_count = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            codepoint = first & 0x0f;
            continuation_count = 2;
        } else if (first >= 0xf0 && first <= 0xf4) {
            codepoint = first & 0x07;
            continuation_count = 3;
        } else {
            return false;
        }
        if (index + continuation_count > size) return false;
        for (std::size_t count = 0; count < continuation_count; ++count) {
            const uint8_t next = bytes[index++];
            if ((next & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if ((continuation_count == 2 && codepoint < 0x800) ||
            (continuation_count == 3 && codepoint < 0x10000) ||
            !safe_codepoint(codepoint)) {
            return false;
        }
    }
    return true;
}

inline bool scanned_ssid_size(const uint8_t *bytes, std::size_t capacity,
                              std::size_t &size)
{
    if (bytes == nullptr || capacity == 0 || capacity > 32) return false;
    size = 0;
    while (size < capacity && bytes[size] != 0) ++size;
    if (size == 0) return false;
    for (std::size_t index = size + (size < capacity ? 1 : 0); index < capacity; ++index) {
        if (bytes[index] != 0) return false;
    }
    return safe_utf8_ssid(bytes, size);
}

inline bool add_wifi_network(WifiNetworkList &networks, const uint8_t *ssid, std::size_t size)
{
    if (networks.count >= kMaxWifiNetworks || !safe_utf8_ssid(ssid, size)) return false;
    WifiNetwork &network = networks.items[networks.count++];
    std::memcpy(network.ssid, ssid, size);
    network.ssid[size] = '\0';
    return true;
}

inline const WifiNetwork *selected_wifi_network(const WifiNetworkList &networks,
                                                std::size_t index)
{
    return index < networks.count ? &networks.items[index] : nullptr;
}

inline bool acknowledged_style(LedControllerState state, LedStyle &style)
{
    switch (state) {
    case LedControllerState::ConnectedClassic:
        style = LedStyle::Classic;
        return true;
    case LedControllerState::ConnectedMirrored:
        style = LedStyle::Mirrored;
        return true;
    case LedControllerState::ConnectedWaterfall:
        style = LedStyle::Waterfall;
        return true;
    default:
        return false;
    }
}

} // namespace control_policy
