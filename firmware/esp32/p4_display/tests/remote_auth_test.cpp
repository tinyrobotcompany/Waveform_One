#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "remote_auth.h"

namespace {

uint8_t next_random = 1;

void deterministic_random(void *output, std::size_t size)
{
    auto *buffer = static_cast<uint8_t *>(output);
    for (std::size_t index = 0; index < size; ++index) buffer[index] = next_random++;
}

std::string pairing_code(const remote_auth::RemoteAuth &auth)
{
    char url[96]{};
    assert(auth.pairing_url(url, sizeof(url)));
    const std::string_view value(url);
    const std::string_view marker = "?code=";
    return std::string(value.substr(value.find(marker) + marker.size()));
}

std::string cookie(const char (&session)[remote_auth::kTokenSize])
{
    return std::string("theme=dark; wf1_session=") + session;
}

} // namespace

int main()
{
    using namespace remote_auth;
    constexpr std::string_view kHost = "192.168.1.20";
    constexpr std::string_view kOrigin = "http://192.168.1.20";
    char session[kTokenSize]{};
    char csrf[kTokenSize]{};
    char url[96]{};

    RemoteAuth auth(deterministic_random);

    // Nothing is reachable before the device holds a home-network address.
    assert(!auth.connected());
    assert(!auth.host_valid(kHost));
    assert(!auth.pairing_url(url, sizeof(url)));
    assert(!auth.pair(kHost, "", 0, session));

    auth.connect(kHost, 0);
    assert(auth.connected());
    assert(auth.pairing_url(url, sizeof(url)));
    assert(std::string_view(url).rfind("http://192.168.1.20/pair?code=", 0) == 0);
    assert(auth.host_valid(kHost));
    assert(auth.host_valid("192.168.1.20:80"));
    // A DNS-rebinding page reaches the device under its own name.
    assert(!auth.host_valid("evil.example"));

    // Pairing needs the on-screen code at the device's own address.
    const std::string first_code = pairing_code(auth);
    assert(!auth.pair("evil.example", first_code, 1, session));
    assert(!auth.pair(kHost, "0123456789abcdef0123456789abcdef", 1, session));
    assert(auth.pair(kHost, first_code, 1, session));

    // The code rotates on use, so the QR cannot be replayed.
    char replay[kTokenSize]{};
    assert(!auth.pair(kHost, first_code, 2, replay));
    assert(pairing_code(auth) != first_code);

    // The session cookie yields the CSRF token only at the device's address.
    assert(auth.session_csrf(kHost, cookie(session), 2, csrf));
    assert(!auth.session_csrf("evil.example", cookie(session), 2, csrf));
    assert(!auth.session_csrf(kHost, "wf1_session=forged", 2, csrf));

    // Mutations need session, CSRF token, Host and Origin together.
    assert(auth.authorize(kHost, kOrigin, cookie(session), csrf, 3));
    assert(!auth.authorize(kHost, kOrigin, cookie(session), "", 3));
    assert(!auth.authorize(kHost, kOrigin, "", csrf, 3));
    assert(!auth.authorize(kHost, "http://evil.example", cookie(session), csrf, 3));
    assert(!auth.authorize("evil.example", kOrigin, cookie(session), csrf, 3));
    assert(!auth.authorize(kHost, kOrigin, cookie(session), csrf, kSessionLifetimeUs + 2));

    // An unused pairing code expires and is replaced for the QR.
    const std::string waiting_code = pairing_code(auth);
    assert(!auth.refresh_pairing(1 + kPairingLifetimeUs));
    assert(!auth.pair(kHost, waiting_code, 2 + kPairingLifetimeUs, replay));
    assert(auth.refresh_pairing(2 + kPairingLifetimeUs));
    assert(pairing_code(auth) != waiting_code);

    // Losing Wi-Fi revokes every session and code.
    assert(auth.authorize(kHost, kOrigin, cookie(session), csrf, 3));
    const std::string before_drop = pairing_code(auth);
    auth.disconnect();
    assert(!auth.connected());
    assert(!auth.authorize(kHost, kOrigin, cookie(session), csrf, 3));
    assert(!auth.pair(kHost, before_drop, 3, replay));
    assert(!auth.refresh_pairing(10 * kPairingLifetimeUs));

    // Rejoining, even at the same address, does not restore old sessions.
    auth.connect(kHost, 4);
    assert(!auth.authorize(kHost, kOrigin, cookie(session), csrf, 5));
    assert(!auth.pair(kHost, before_drop, 5, replay));

    // A new address moves the remote with it.
    auth.connect("192.168.1.21", 6);
    assert(!auth.host_valid(kHost));
    assert(auth.host_valid("192.168.1.21"));
    assert(auth.pair("192.168.1.21", pairing_code(auth), 6, session));
    assert(auth.session_csrf("192.168.1.21", cookie(session), 6, csrf));
    assert(auth.authorize("192.168.1.21", "http://192.168.1.21", cookie(session), csrf, 6));
    assert(!auth.authorize(kHost, kOrigin, cookie(session), csrf, 6));

    // At most four phones stay paired; the oldest is evicted.
    char sessions[kMaxSessions + 1][kTokenSize]{};
    for (auto &each : sessions) {
        assert(auth.pair("192.168.1.21", pairing_code(auth), 7, each));
    }
    char evicted_csrf[kTokenSize]{};
    assert(!auth.session_csrf("192.168.1.21", cookie(sessions[0]), 7, evicted_csrf));
    for (std::size_t index = 1; index <= kMaxSessions; ++index) {
        assert(auth.session_csrf("192.168.1.21", cookie(sessions[index]), 7, evicted_csrf));
    }

    // An address that does not fit is never bound.
    auth.connect("this-address-is-too-long", 8);
    assert(!auth.connected());
}
