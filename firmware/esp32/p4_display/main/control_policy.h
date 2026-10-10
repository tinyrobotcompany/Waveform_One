#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
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

inline std::string_view cookie_value(std::string_view cookies, std::string_view name)
{
    for (std::size_t start = 0; start < cookies.size();) {
        while (start < cookies.size() && (cookies[start] == ' ' || cookies[start] == ';')) {
            ++start;
        }
        const std::size_t end = cookies.find(';', start);
        const std::size_t item_end = end == std::string_view::npos ? cookies.size() : end;
        const std::size_t equals = cookies.find('=', start);
        if (equals < item_end) {
            std::size_t key_end = equals;
            while (key_end > start && cookies[key_end - 1] == ' ') --key_end;
            if (cookies.substr(start, key_end - start) == name) {
                std::size_t value_start = equals + 1;
                while (value_start < item_end && cookies[value_start] == ' ') ++value_start;
                std::size_t value_end = item_end;
                while (value_end > value_start && cookies[value_end - 1] == ' ') --value_end;
                return cookies.substr(value_start, value_end - value_start);
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return {};
}

inline bool valid_brightness(int percent)
{
    return percent >= 10 && percent <= 100;
}

inline bool valid_wifi_password(std::string_view password)
{
    if (password.size() <= 63) return true;
    if (password.size() != 64) return false;
    for (const char value : password) {
        const bool hex = (value >= '0' && value <= '9') ||
                         (value >= 'a' && value <= 'f') ||
                         (value >= 'A' && value <= 'F');
        if (!hex) return false;
    }
    return true;
}

inline bool valid_browser_time(int64_t proposed, int64_t current)
{
    constexpr int64_t kMinimum = 1700000000LL;
    constexpr int64_t kMaximum = 2145916800LL; // 2038-01-01 UTC
    constexpr int64_t kMaximumCorrection = 300LL;
    if (proposed < kMinimum || proposed > kMaximum) return false;
    if (current < kMinimum || current > kMaximum) return true;
    const int64_t difference = proposed > current ? proposed - current : current - proposed;
    return difference <= kMaximumCorrection;
}

inline int form_hex_digit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline bool form_decode(std::string_view encoded, std::string &decoded, std::size_t max_size)
{
    decoded.clear();
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        char value = encoded[index];
        if (value == '+') {
            value = ' ';
        } else if (value == '%') {
            if (index + 2 >= encoded.size()) return false;
            const int high = form_hex_digit(encoded[index + 1]);
            const int low = form_hex_digit(encoded[index + 2]);
            if (high < 0 || low < 0) return false;
            value = static_cast<char>((high << 4) | low);
            index += 2;
        }
        const auto byte = static_cast<unsigned char>(value);
        if (byte < 0x20 || byte == 0x7f || decoded.size() >= max_size) return false;
        decoded.push_back(value);
    }
    return true;
}

// Accepts application/x-www-form-urlencoded, case-insensitively, optionally
// followed by parameters such as "; charset=UTF-8".
inline bool form_content_type(std::string_view content_type)
{
    constexpr std::string_view kType = "application/x-www-form-urlencoded";
    if (content_type.size() < kType.size()) return false;
    for (std::size_t index = 0; index < kType.size(); ++index) {
        char value = content_type[index];
        if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
        if (value != kType[index]) return false;
    }
    std::string_view rest = content_type.substr(kType.size());
    while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
    return rest.empty() || rest.front() == ';';
}

// Looks up one field of an application/x-www-form-urlencoded body. Keys are
// decoded before comparison, and malformed encoding, duplicate keys or an
// oversized value reject the whole body rather than choosing one reading of it.
inline bool form_field(std::string_view body, std::string_view key, std::string &value,
                       std::size_t max_size)
{
    bool found = false;
    std::string decoded_key;
    std::string decoded_value;
    for (std::size_t start = 0; start <= body.size();) {
        const std::size_t end = std::min(body.find('&', start), body.size());
        const std::string_view pair = body.substr(start, end - start);
        const std::size_t equals = pair.find('=');
        const std::string_view raw_key = pair.substr(0, equals);
        const std::string_view raw_value =
            equals == std::string_view::npos ? std::string_view{} : pair.substr(equals + 1);
        if (pair.empty() || !form_decode(raw_key, decoded_key, 32)) return false;
        if (decoded_key == key) {
            if (found || !form_decode(raw_value, decoded_value, max_size)) return false;
            value = decoded_value;
            found = true;
        }
        start = end + 1;
    }
    return found;
}

inline bool valid_http_host(std::string_view host, std::string_view expected_host)
{
    if (expected_host.empty()) return false;
    return host == expected_host ||
           (host.size() == expected_host.size() + 3 &&
            host.substr(0, expected_host.size()) == expected_host &&
            host.substr(expected_host.size()) == ":80");
}

inline bool same_http_origin(std::string_view host, std::string_view origin,
                             std::string_view expected_host)
{
    const bool valid_host = valid_http_host(host, expected_host);
    const std::string expected_origin = std::string("http://") + std::string(expected_host);
    const bool valid_origin = origin == expected_origin ||
                              (origin.size() == expected_origin.size() + 3 &&
                               origin.substr(0, expected_origin.size()) == expected_origin &&
                               origin.substr(expected_origin.size()) == ":80");
    return valid_host && valid_origin;
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

inline bool safe_utf8_text(const uint8_t *bytes, std::size_t size, std::size_t max_size)
{
    if (bytes == nullptr || size == 0 || size > max_size) return false;
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

inline bool safe_utf8_ssid(const uint8_t *bytes, std::size_t size)
{
    return safe_utf8_text(bytes, size, 32);
}

constexpr std::size_t kMaxDisplayNameBytes = 120;

inline bool safe_display_name(std::string_view name)
{
    if (name.empty() || name.front() == ' ' || name.back() == ' ') return false;
    return safe_utf8_text(reinterpret_cast<const uint8_t *>(name.data()), name.size(),
                          kMaxDisplayNameBytes);
}

inline std::size_t scanned_ssid_size(const uint8_t *bytes, std::size_t capacity)
{
    if (bytes == nullptr || capacity == 0) return 0;
    for (std::size_t index = 0; index < capacity; ++index) {
        if (bytes[index] != 0) continue;
        for (std::size_t remainder = index + 1; remainder < capacity; ++remainder) {
            if (bytes[remainder] != 0) return 0;
        }
        return index;
    }
    return capacity;
}

inline bool add_wifi_network(WifiNetworkList &networks, const uint8_t *ssid, std::size_t size)
{
    if (networks.count >= kMaxWifiNetworks || !safe_utf8_ssid(ssid, size)) return false;
    const std::string_view name(reinterpret_cast<const char *>(ssid), size);
    for (std::size_t index = 0; index < networks.count; ++index) {
        if (name == networks.items[index].ssid) return false;
    }
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
