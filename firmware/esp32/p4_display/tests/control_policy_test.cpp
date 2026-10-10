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
    assert(cookie_value("theme=dark; wf1_session=abc123; language=en", "wf1_session") ==
           "abc123");
    assert(cookie_value("wf1_session_extra=wrong; wf1_session=right", "wf1_session") ==
           "right");
    assert(cookie_value("theme=dark", "wf1_session").empty());

    assert(valid_brightness(10));
    assert(valid_brightness(100));
    assert(!valid_brightness(9));
    assert(!valid_brightness(101));

    assert(valid_wifi_password(std::string(63, 'p')));
    assert(valid_wifi_password(
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    assert(!valid_wifi_password(std::string(64, 'z')));
    assert(!valid_wifi_password(std::string(65, 'a')));

    assert(valid_browser_time(1800000000LL, 0));
    assert(valid_browser_time(1800000300LL, 1800000000LL));
    assert(!valid_browser_time(1800000301LL, 1800000000LL));
    assert(!valid_browser_time(4102444800LL, 1800000000LL));

    assert(valid_http_host("192.168.1.20", "192.168.1.20"));
    assert(valid_http_host("192.168.1.20:80", "192.168.1.20"));
    assert(!valid_http_host("", "192.168.1.20"));
    assert(!valid_http_host("evil.example", "192.168.1.20"));
    assert(!valid_http_host("192.168.1.20:8080", "192.168.1.20"));
    assert(same_http_origin("192.168.1.20", "http://192.168.1.20", "192.168.1.20"));
    assert(same_http_origin("192.168.1.20:80", "http://192.168.1.20", "192.168.1.20"));
    assert(same_http_origin("192.168.1.20:80", "http://192.168.1.20:80", "192.168.1.20"));
    assert(same_http_origin("192.168.1.20", "http://192.168.1.20:80", "192.168.1.20"));
    assert(!same_http_origin("192.168.1.20:8080", "http://192.168.1.20:8080",
                             "192.168.1.20"));
    assert(!same_http_origin("192.168.1.200", "http://192.168.1.20", "192.168.1.20"));
    assert(!same_http_origin("192.168.1.20", "http://evil.example", "192.168.1.20"));

    WifiNetworkList networks{};
    const uint8_t home[] = {'H', 'o', 'm', 'e'};
    const uint8_t cafe[] = {'C', 'a', 'f', 0xc3, 0xa9};
    const uint8_t injected[] = {'H', 'o', 'm', 'e', '\n', 'E', 'v', 'i', 'l'};
    const uint8_t malformed[] = {0xc3, 0x28};
    const uint8_t embedded_nul[] = {'H', 'o', 'm', 'e', 0, 'E', 'v', 'i', 'l'};
    const uint8_t padded[] = {'H', 'o', 'm', 'e', 0, 0, 0, 0};
    assert(add_wifi_network(networks, home, sizeof(home)));
    assert(add_wifi_network(networks, cafe, sizeof(cafe)));
    assert(!add_wifi_network(networks, injected, sizeof(injected)));
    assert(!add_wifi_network(networks, malformed, sizeof(malformed)));
    assert(scanned_ssid_size(embedded_nul, sizeof(embedded_nul)) == 0);
    assert(scanned_ssid_size(padded, sizeof(padded)) == 4);
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
