#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "remote_auth.h"
#include "remote_pairing.h"

namespace {

uint8_t next_random = 1;

void deterministic_random(void *output, std::size_t size)
{
    auto *buffer = static_cast<uint8_t *>(output);
    for (std::size_t index = 0; index < size; ++index) buffer[index] = next_random++;
}

std::string pairing_code(const remote_auth::RemoteAuth &auth)
{
    char url[160]{};
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
    constexpr std::string_view kHost = "device-a.waveform.tinyrobotcompany.io";
    constexpr std::string_view kOrigin = "https://device-a.waveform.tinyrobotcompany.io";
    char session[kTokenSize]{};
    char csrf[kTokenSize]{};
    char url[160]{};

    RemoteAuth auth(deterministic_random);

    // Nothing is reachable before the device holds a home-network address.
    assert(!auth.connected());
    assert(!auth.host_valid(kHost));
    assert(!auth.pairing_url(url, sizeof(url)));
    assert(!auth.pair(kHost, "", 0, session));

    auth.connect(kHost, 0);
    assert(auth.connected());
    assert(auth.pairing_url(url, sizeof(url)));
    assert(std::string_view(url).rfind("https://device-a.waveform.tinyrobotcompany.io/pair?code=", 0) == 0);
    assert(auth.host_valid(kHost));
    assert(auth.host_valid("device-a.waveform.tinyrobotcompany.io:443"));
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
    assert(!auth.authorize(kHost, "https://evil.example", cookie(session), csrf, 3));
    assert(!auth.authorize("evil.example", kOrigin, cookie(session), csrf, 3));
    assert(!auth.authorize(kHost, kOrigin, cookie(session), csrf, kSessionLifetimeUs + 2));

    assert(!auth.authorize(kHost, "http://device-a.waveform.tinyrobotcompany.io", cookie(session), csrf, 3));
    assert(!auth.host_valid("device-a.waveform.tinyrobotcompany.io:80"));
    // An unused pairing code expires and is replaced for the QR.
    const std::string waiting_code = pairing_code(auth);
    assert(!auth.refresh_pairing(1 + kPairingLifetimeUs));
    assert(!auth.pair(kHost, waiting_code, 2 + kPairingLifetimeUs, replay));
    assert(auth.refresh_pairing(2 + kPairingLifetimeUs));
    assert(pairing_code(auth) != waiting_code);

    // Radio interruptions suspend access without losing pairing at the same IP.
    auth.suspend();
    assert(!auth.authorize(kHost, kOrigin, cookie(session), csrf, 3));
    auth.connect(kHost, 4);
    assert(auth.authorize(kHost, kOrigin, cookie(session), csrf, 5));

    // A reboot restores only unexpired sessions at the same address after clock sync.
    StoredSessions stored{};
    constexpr int64_t epoch = 1800000000;
    assert(!auth.export_sessions(stored, 5, 0));
    assert(auth.export_sessions(stored, 5, epoch));
    RemoteAuth rebooted(deterministic_random);
    rebooted.connect(kHost, 100);
    assert(!rebooted.restore_sessions(stored, 100, 0));
    assert(rebooted.restore_sessions(stored, 100, epoch + 60));
    assert(rebooted.authorize(kHost, kOrigin, cookie(session), csrf, 101));
    assert(!rebooted.authorize(kHost, kOrigin, cookie(session), "forged", 101));
    assert(!rebooted.authorize(kHost, "https://evil.example", cookie(session), csrf, 101));
    assert(!rebooted.authorize(kHost, kOrigin, cookie(session), csrf, kSessionLifetimeUs));
    rebooted.disconnect();rebooted.connect("192.168.1.21", 100);
    assert(!rebooted.restore_sessions(stored, 100, epoch + 60));
    rebooted.disconnect();rebooted.connect(kHost, 100);
    assert(rebooted.restore_sessions(stored, 100, epoch + kSessionLifetimeSeconds + 1));
    assert(!rebooted.authorize(kHost, kOrigin, cookie(session), csrf, 101));
    stored.entries[0].id[0] = '!';
    assert(!rebooted.restore_sessions(stored, 100, epoch + 60));

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
    assert(auth.authorize("192.168.1.21", "https://192.168.1.21", cookie(session), csrf, 6));
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

    // Durable pairing: later-day/reboot access, legacy expiry, failure rollback,
    // and device-only revocation that also invalidates an already scanned QR.
    RemoteAuth durable(deterministic_random);
    durable.connect(kHost, 0);
    StoredSessions disk{};
    bool write_ok = true;
    unsigned writes = 0;
    auto save = [&](const StoredSessions &value) {
        ++writes;
        if (!write_ok) return false;
        disk = value;
        return true;
    };
    const auto initial_code = pairing_code(durable);
    char persistent_id[kTokenSize]{}, persistent_csrf[kTokenSize]{};
    assert(pair_saved(durable, kHost, initial_code, 1, 0, persistent_id, save) == PairResult::StorageUnavailable);
    assert(writes == 0 && pairing_code(durable) == initial_code);
    write_ok = false;
    assert(pair_saved(durable, kHost, initial_code, 1, epoch, persistent_id, save) == PairResult::StorageUnavailable);
    assert(!persistent_id[0] && pairing_code(durable) == initial_code);
    write_ok = true;
    assert(pair_saved(durable, kHost, initial_code, 1, epoch, persistent_id, save) == PairResult::Paired);
    assert(durable.session_csrf(kHost, cookie(persistent_id), 2, persistent_csrf));
    char response_cookie[112]{};
    assert(session_cookie(response_cookie, sizeof(response_cookie), persistent_id));
    const std::string cookie_header(response_cookie);
    assert(cookie_header.find("Max-Age=7776000; HttpOnly; Secure; SameSite=Strict") != std::string::npos);
    assert(!session_cookie(response_cookie, 8, persistent_id));
    assert(pair_saved(durable, kHost, initial_code, 2, epoch, replay, save) == PairResult::InvalidCode);
    RemoteAuth next_day(deterministic_random);
    next_day.connect(kHost, 0);
    assert(next_day.restore_sessions(disk, 0, epoch + 86400));
    assert(next_day.authorize(kHost, kOrigin, cookie(persistent_id), persistent_csrf, 1));
    auto legacy = disk;
    legacy.entries[0].expires_epoch = epoch + 3600;
    assert(next_day.restore_sessions(legacy, 0, epoch + 3601));
    assert(!next_day.authorize(kHost, kOrigin, cookie(persistent_id), persistent_csrf, 1));
    assert(next_day.restore_sessions(disk, 0, epoch + 86400));
    const auto before_forget = pairing_code(next_day);
    write_ok = false;
    assert(!forget_saved(next_day, 2, save));
    assert(pairing_code(next_day) == before_forget);
    assert(next_day.authorize(kHost, kOrigin, cookie(persistent_id), persistent_csrf, 3));
    write_ok = true;
    assert(forget_saved(next_day, 4, save));
    assert(!next_day.authorize(kHost, kOrigin, cookie(persistent_id), persistent_csrf, 5));
    assert(!next_day.pair(kHost, before_forget, 5, replay));
    RemoteAuth after_forget(deterministic_random);
    after_forget.connect(kHost, 0);
    after_forget.restore_sessions(disk, 0, epoch + 86401);
    assert(!after_forget.authorize(kHost, kOrigin, cookie(persistent_id), persistent_csrf, 1));

    // An address that does not fit is never bound.
    auth.connect(std::string(97, 'a'), 8);
    assert(!auth.connected());
}
