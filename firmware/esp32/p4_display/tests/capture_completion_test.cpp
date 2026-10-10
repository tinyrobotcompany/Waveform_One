#include <cassert>
#include <cstdlib>
#include <set>
#include <thread>
#include "capture_completion.h"

std::set<void *> allocations;
uint8_t *allocate() {
    auto *pcm = static_cast<uint8_t *>(std::malloc(8));
    assert(pcm); allocations.insert(pcm); return pcm;
}
void release(void *pcm) {
    if (!pcm) return;
    assert(allocations.erase(pcm) == 1); std::free(pcm);
}
int main()
{
    using namespace capture_completion;
    Mailbox mailbox(release);
    int64_t now = 0;
    uint64_t epoch = 1;
    Clip clip{};
    auto clock = [&] { return now; };
    auto session = [&] { return epoch; };
    auto tick = [&] { now += 100000; };

    // A lost callback has a fixed deadline; the next capture can succeed.
    auto id = mailbox.begin();
    assert(wait(mailbox, id, 1, 17000000, clip, clock, session, tick) == WaitResult::TimedOut);
    assert(now == 17000000);
    const auto abandoned = id;
    id = mailbox.begin();
    mailbox.complete(abandoned, AudioCaptureStatus::Complete, allocate());
    assert(allocations.empty());
    auto *expected = allocate();
    mailbox.complete(id, AudioCaptureStatus::Complete, expected);
    assert(wait(mailbox, id, 1, now + 17000000, clip, clock, session, tick) == WaitResult::Complete);
    assert(clip.pcm == expected); release(clip.pcm);

    // Session changes interrupt a wait promptly; late callbacks release PCM.
    id = mailbox.begin();
    auto change_session = [&] { now += 100000; ++epoch; };
    assert(wait(mailbox, id, 1, now + 17000000, clip, clock, session, change_session) == WaitResult::Cancelled);
    mailbox.complete(id, AudioCaptureStatus::Complete, allocate());
    assert(allocations.empty());

    // A completion already pending when a session is cancelled is also freed.
    id = mailbox.begin(); mailbox.complete(id, AudioCaptureStatus::Complete, allocate());
    assert(wait(mailbox, id, 1, now + 17000000, clip, clock, session, tick) == WaitResult::Cancelled);
    assert(allocations.empty());

    // No full-queue drop: first completion wins, duplicates/stale entries free.
    id = mailbox.begin(); expected = allocate();
    mailbox.complete(id, AudioCaptureStatus::Complete, expected);
    mailbox.complete(id, AudioCaptureStatus::Complete, allocate());
    assert(allocations.size() == 1);
    assert(mailbox.take(id, clip) && clip.pcm == expected); release(clip.pcm);
    mailbox.complete(id, AudioCaptureStatus::Complete, allocate());
    assert(allocations.empty());
    id = mailbox.begin(); mailbox.complete(id, AudioCaptureStatus::Complete, allocate());
    const auto newer = mailbox.begin(); assert(allocations.empty());
    mailbox.complete(id, AudioCaptureStatus::Complete, allocate());
    assert(allocations.empty() && !mailbox.take(newer, clip));
    mailbox.cancel(newer);

    // Controller callback and worker cancellation may run on different cores.
    id = mailbox.begin(); expected = allocate();
    std::thread callback([&] { mailbox.complete(id, AudioCaptureStatus::Complete, expected); });
    mailbox.cancel(id); callback.join(); assert(allocations.empty());
}
