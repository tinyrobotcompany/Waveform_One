#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "control_policy.h"

namespace remote_auth {

constexpr int64_t kPairingLifetimeUs = 10LL * 60LL * 1000000LL;
constexpr int64_t kSessionLifetimeSeconds = 90LL * 24LL * 60LL * 60LL;
constexpr int64_t kSessionLifetimeUs = kSessionLifetimeSeconds * 1000000LL;
constexpr std::size_t kMaxSessions = 4;
constexpr std::size_t kTokenSize = 33;

inline bool session_cookie(char *output, std::size_t capacity, const char *id)
{
    const int size = std::snprintf(output, capacity,
        "wf1_session=%s; Path=/; Max-Age=%lld; HttpOnly; Secure; SameSite=Strict", id,
        static_cast<long long>(kSessionLifetimeSeconds));
    return size > 0 && static_cast<std::size_t>(size) < capacity;
}

using RandomFill = void (*)(void *buffer, std::size_t size);

// Sessions remain bound to the provisioned device hostname. Wi-Fi interruptions
// suspend access; hostname changes and explicit network changes revoke access.
struct StoredSessions {
    uint32_t schema = 2;
    uint32_t next_session = 0;
    char host[97]{};
    struct Entry { char id[kTokenSize]{}; char csrf[kTokenSize]{}; int64_t expires_epoch = 0; } entries[kMaxSessions];
};

class RemoteAuth {
public:
    explicit RemoteAuth(RandomFill fill) : fill_(fill) {}

    // A same-hostname reconnect preserves unexpired pairing sessions.
    void connect(std::string_view host, int64_t now_us)
    {
        if (host == host_ && !host.empty()) { online_ = true; issue_pairing_code(now_us); return; }
        disconnect();
        if (host.empty() || host.size() >= sizeof(host_)) return;
        host.copy(host_, host.size());
        host_[host.size()] = '\0';
        online_ = true;
        issue_pairing_code(now_us);
    }

    void disconnect()
    {
        *this = RemoteAuth(fill_);
    }

    void forget_sessions(int64_t now_us)
    {
        for (auto &session : sessions_) session = Session{};
        next_session_ = 0;
        if (connected()) issue_pairing_code(now_us);
    }

    bool matches_address(std::string_view host) const { return host == host_; }
    void suspend() { online_ = false; }
    bool connected() const { return online_ && host_[0] != '\0'; }

    // Store absolute expiry, never reboot-relative time. No pairing code is persisted.
    bool export_sessions(StoredSessions &out, int64_t now_us, int64_t epoch) const
    {
        if (!connected() || epoch < 1700000000) return false;
        out = StoredSessions{};
        out.next_session = next_session_ % kMaxSessions;
        std::memcpy(out.host, host_, sizeof(host_));
        for (std::size_t i = 0; i < kMaxSessions; ++i) {
            if (!sessions_[i].id[0] || sessions_[i].expires_at_us <= now_us) continue;
            std::memcpy(out.entries[i].id, sessions_[i].id, kTokenSize);
            std::memcpy(out.entries[i].csrf, sessions_[i].csrf, kTokenSize);
            out.entries[i].expires_epoch = epoch + (sessions_[i].expires_at_us - now_us) / 1000000;
        }
        return true;
    }

    bool restore_sessions(const StoredSessions &in, int64_t now_us, int64_t epoch)
    {
        if (!connected() || epoch < 1700000000 || in.schema != 2 || in.next_session >= kMaxSessions ||
            !std::memchr(in.host, 0, sizeof(in.host)) || std::string_view(in.host) != host_) return false;
        for (const auto &entry : in.entries) {
            if (!entry.id[0]) continue;
            if (!valid_token(entry.id) || !valid_token(entry.csrf) ||
                entry.expires_epoch > epoch + kSessionLifetimeUs / 1000000) return false;
        }
        next_session_ = in.next_session;
        for (std::size_t i = 0; i < kMaxSessions; ++i) {
            sessions_[i] = Session{};
            if (!in.entries[i].id[0] || in.entries[i].expires_epoch <= epoch) continue;
            std::memcpy(sessions_[i].id, in.entries[i].id, kTokenSize);
            std::memcpy(sessions_[i].csrf, in.entries[i].csrf, kTokenSize);
            sessions_[i].expires_at_us = now_us + (in.entries[i].expires_epoch - epoch) * 1000000;
        }
        return true;
    }

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
        const int written = std::snprintf(output, capacity, "https://%s/pair?code=%s", host_,
                                          pairing_code_);
        return written > 0 && static_cast<std::size_t>(written) < capacity;
    }

    bool host_valid(std::string_view host) const
    {
        return connected() && (host == host_ || host == std::string(host_) + ":443");
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
        return connected() && host_valid(host) && (origin == std::string("https://") + host_ || origin == std::string("https://") + host_ + ":443") &&
               session != nullptr && control_policy::remote_token_matches(session->csrf, csrf);
    }

private:
    static bool valid_token(const char (&value)[kTokenSize])
    {
        if (value[kTokenSize - 1] != 0) return false;
        for (std::size_t i = 0; i < kTokenSize - 1; ++i)
            if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
        return true;
    }
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

    bool online_ = false;
    RandomFill fill_;
    char host_[97]{};
    char pairing_code_[kTokenSize]{};
    int64_t pairing_expires_at_us_ = 0;
    Session sessions_[kMaxSessions]{};
    std::size_t next_session_ = 0;
};

} // namespace remote_auth
