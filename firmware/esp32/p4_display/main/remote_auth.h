#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "control_policy.h"

namespace remote_auth {

constexpr int64_t kPairingLifetimeUs = 10LL * 60LL * 1000000LL;
constexpr int64_t kSessionLifetimeUs = 60LL * 60LL * 1000000LL;
constexpr std::size_t kMaxSessions = 4;
constexpr std::size_t kTokenSize = 33;

using RandomFill = void (*)(void *buffer, std::size_t size);

// Phone-remote authorization state. It is bound to the address the device
// currently holds on home Wi-Fi: losing or changing that address revokes every
// session and pairing code, so a remote can only act while the QR it was
// paired from is still the one on screen. The class is not thread-safe; the
// caller serializes access.
class RemoteAuth {
public:
    explicit RemoteAuth(RandomFill fill) : fill_(fill) {}

    // Binds the remote to a newly acquired home-network address. Any previous
    // binding is revoked first, even when the address is unchanged.
    void connect(std::string_view host, int64_t now_us)
    {
        disconnect();
        if (host.empty() || host.size() >= sizeof(host_)) return;
        host.copy(host_, host.size());
        host_[host.size()] = '\0';
        issue_pairing_code(now_us);
    }

    void disconnect()
    {
        *this = RemoteAuth(fill_);
    }

    bool connected() const { return host_[0] != '\0'; }

    // Replaces an expired pairing code. Returns true when the code shown in the
    // QR must be refreshed.
    bool refresh_pairing(int64_t now_us)
    {
        if (!connected() || now_us <= pairing_expires_at_us_) return false;
        issue_pairing_code(now_us);
        return true;
    }

    // The URL the QR presents, or false while the remote is unavailable.
    bool pairing_url(char *output, std::size_t capacity) const
    {
        if (!connected() || capacity == 0) return false;
        const int written = std::snprintf(output, capacity, "http://%s/pair?code=%s", host_,
                                          pairing_code_);
        return written > 0 && static_cast<std::size_t>(written) < capacity;
    }

    bool host_valid(std::string_view host) const
    {
        return connected() && control_policy::valid_http_host(host, host_);
    }

    // Exchanges the on-screen pairing code for a session and rotates the code so
    // the QR cannot be replayed.
    bool pair(std::string_view host, std::string_view code, int64_t now_us,
              char (&session_id)[kTokenSize])
    {
        if (!host_valid(host) || now_us > pairing_expires_at_us_ ||
            !control_policy::remote_token_matches(pairing_code_, code)) {
            return false;
        }
        Session &session = sessions_[next_session_++ % kMaxSessions];
        random_token(session.id);
        random_token(session.csrf);
        session.expires_at_us = now_us + kSessionLifetimeUs;
        std::memcpy(session_id, session.id, kTokenSize);
        issue_pairing_code(now_us);
        return true;
    }

    // Returns the CSRF token for the session named in the Cookie header.
    bool session_csrf(std::string_view host, std::string_view cookies, int64_t now_us,
                      char (&csrf)[kTokenSize]) const
    {
        const Session *session = find_session(cookies, now_us);
        if (!host_valid(host) || session == nullptr) return false;
        std::memcpy(csrf, session->csrf, kTokenSize);
        return true;
    }

    bool authorize(std::string_view host, std::string_view origin, std::string_view cookies,
                   std::string_view csrf, int64_t now_us) const
    {
        const Session *session = find_session(cookies, now_us);
        return connected() && control_policy::same_http_origin(host, origin, host_) &&
               session != nullptr && control_policy::remote_token_matches(session->csrf, csrf);
    }

private:
    struct Session {
        char id[kTokenSize]{};
        char csrf[kTokenSize]{};
        int64_t expires_at_us = 0;
    };

    void random_token(char (&output)[kTokenSize]) const
    {
        static constexpr char kDigits[] = "0123456789abcdef";
        uint8_t random[(kTokenSize - 1) / 2]{};
        fill_(random, sizeof(random));
        for (std::size_t index = 0; index < sizeof(random); ++index) {
            output[index * 2] = kDigits[random[index] >> 4];
            output[index * 2 + 1] = kDigits[random[index] & 0x0f];
        }
        output[kTokenSize - 1] = '\0';
    }

    void issue_pairing_code(int64_t now_us)
    {
        random_token(pairing_code_);
        pairing_expires_at_us_ = now_us + kPairingLifetimeUs;
    }

    const Session *find_session(std::string_view cookies, int64_t now_us) const
    {
        if (!connected()) return nullptr;
        const std::string_view id = control_policy::cookie_value(cookies, "wf1_session");
        for (const Session &session : sessions_) {
            if (session.expires_at_us >= now_us &&
                control_policy::remote_token_matches(session.id, id)) {
                return &session;
            }
        }
        return nullptr;
    }

    RandomFill fill_;
    char host_[16]{};
    char pairing_code_[kTokenSize]{};
    int64_t pairing_expires_at_us_ = 0;
    Session sessions_[kMaxSessions]{};
    std::size_t next_session_ = 0;
};

} // namespace remote_auth
