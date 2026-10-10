#include <cassert>
#include <cstdint>
#include <string>
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

    std::string field;
    assert(form_field("style=waterfall", "style", field, 32) && field == "waterfall");
    assert(form_field("x=1&style=mirrored", "style", field, 32) && field == "mirrored");
    assert(form_field("name=Caf%C3%A9+Bar", "name", field, 32) && field == "Caf\xc3\xa9 Bar");
    assert(form_field("%73tyle=classic", "style", field, 32) && field == "classic");
    assert(!form_field("x=a%26style%3Dwaterfall", "style", field, 32));
    assert(!form_field("xstyle=classic", "style", field, 32));
    assert(!form_field("style=classic&style=waterfall", "style", field, 32));
    assert(!form_field("style=clas%2", "style", field, 32));
    assert(!form_field("style=clas%zz", "style", field, 32));
    assert(!form_field("style=a%0Ab", "style", field, 32));
    assert(!form_field("style=a%00b", "style", field, 32));
    assert(!form_field("style=classic&", "style", field, 32));
    assert(!form_field("", "style", field, 32));
    assert(!form_field("value=12345", "value", field, 4));
    assert(form_field("value=1234", "value", field, 4) && field == "1234");

    assert(safe_display_name("Simon"));
    assert(safe_display_name("Zo\xc3\xab"));
    assert(!safe_display_name(""));
    assert(!safe_display_name(" Simon"));
    assert(!safe_display_name("Simon\x7f"));
    assert(!safe_display_name("Sim\xc3"));
    assert(!safe_display_name("Si\xe2\x80\xaemon"));
    assert(safe_display_name(std::string(kMaxDisplayNameBytes, 'a')));
    assert(!safe_display_name(std::string(kMaxDisplayNameBytes + 1, 'a')));

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
    assert(!add_wifi_network(networks, home, sizeof(home)));
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

    WifiNetworkList crowded{};
    for (int index = 0; index < 20; ++index) {
        const uint8_t name[] = {'N', static_cast<uint8_t>('a' + index)};
        add_wifi_network(crowded, name, sizeof(name));
    }
    assert(crowded.count == kMaxWifiNetworks);
}
