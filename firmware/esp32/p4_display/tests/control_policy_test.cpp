#include <cassert>
#include <cstdint>
#include <string_view>

#include "control_policy.h"

int main()
{
    using namespace control_policy;

    constexpr std::string_view token = "0123456789abcdef0123456789abcdef";
    assert(remote_token_matches(token, token));
    assert(!remote_token_matches(token, ""));
    assert(!remote_token_matches(token, "0123456789abcdef0123456789abcdee"));
    assert(!remote_token_matches(token, "0123456789abcdef0123456789abcdef0"));

    assert(valid_brightness(10));
    assert(valid_brightness(100));
    assert(!valid_brightness(9));
    assert(!valid_brightness(101));

    WifiNetworkList networks{};
    const uint8_t home[] = {'H', 'o', 'm', 'e'};
    const uint8_t cafe[] = {'C', 'a', 'f', 0xc3, 0xa9};
    const uint8_t injected[] = {'H', 'o', 'm', 'e', '\n', 'E', 'v', 'i', 'l'};
    const uint8_t malformed[] = {0xc3, 0x28};
    const uint8_t embedded_nul[32] = {'H', 'o', 'm', 'e', 0, 'E', 'v', 'i', 'l'};
    const uint8_t padded_home[32] = {'H', 'o', 'm', 'e'};
    assert(add_wifi_network(networks, home, sizeof(home)));
    assert(add_wifi_network(networks, cafe, sizeof(cafe)));
    assert(!add_wifi_network(networks, injected, sizeof(injected)));
    assert(!add_wifi_network(networks, malformed, sizeof(malformed)));
    std::size_t scanned_size = 0;
    assert(!scanned_ssid_size(embedded_nul, sizeof(embedded_nul), scanned_size));
    assert(scanned_ssid_size(padded_home, sizeof(padded_home), scanned_size));
    assert(scanned_size == 4);
    assert(networks.count == 2);
    assert(std::string_view(networks.items[0].ssid) == "Home");
    assert(std::string_view(networks.items[1].ssid) == "Caf\xc3\xa9");
    assert(selected_wifi_network(networks, 0) == &networks.items[0]);
    assert(selected_wifi_network(networks, 2) == nullptr);

    LedStyle style{};
    assert(acknowledged_style(LedControllerState::ConnectedClassic, style));
    assert(style == LedStyle::Classic);
    assert(acknowledged_style(LedControllerState::ConnectedMirrored, style));
    assert(style == LedStyle::Mirrored);
    assert(acknowledged_style(LedControllerState::ConnectedWaterfall, style));
    assert(style == LedStyle::Waterfall);
    assert(!acknowledged_style(LedControllerState::Connecting, style));
    assert(!acknowledged_style(LedControllerState::ProtocolError, style));
}
