#include <cassert>
#include "recognition_policy.h"

int main()
{
    using namespace recognition_policy;
    Sessions sessions;
    assert(!sessions.snapshot().ready);
    assert(sessions.set_controller(true));
    assert(!sessions.snapshot().ready);
    assert(sessions.set_network(true, "http://192.168.1.10/pair?code=first"));
    assert(!sessions.snapshot().ready);
    assert(sessions.set_activity(true));
    const auto original = sessions.snapshot();
    assert(original.ready);
    assert(!sessions.set_network(true, "http://192.168.1.10/pair?code=rotated"));
    assert(sessions.snapshot().epoch == original.epoch);
    assert(!sessions.set_controller(true));
    assert(sessions.set_network(false, ""));
    assert(!sessions.snapshot().ready && sessions.snapshot().epoch != original.epoch);
    assert(sessions.set_network(true, "http://192.168.1.10/pair?code=new"));
    assert(sessions.snapshot().ready && sessions.snapshot().epoch != original.epoch);
    auto before = sessions.snapshot();
    assert(sessions.set_network(true, "http://192.168.1.11/pair?code=changed"));
    assert(sessions.snapshot().epoch != before.epoch);
    before = sessions.snapshot();
    assert(sessions.set_controller(false));
    assert(!sessions.snapshot().ready && sessions.snapshot().epoch != before.epoch);
    assert(sessions.set_controller(true));
    assert(sessions.snapshot().ready);
    auto music = sessions.snapshot();
    assert(sessions.set_activity(false));
    assert(!sessions.snapshot().ready && sessions.snapshot().epoch != music.epoch);
    assert(!sessions.set_activity(false));
    assert(sessions.set_activity(true));
    assert(sessions.snapshot().ready && sessions.snapshot().epoch != music.epoch);
    assert(sessions.set_network(true, ""));
    assert(!sessions.snapshot().ready);

    // Availability and queue failures back off; Busy is transient only within
    // the maximum outstanding capture window, then becomes a failure too.
    for (auto result : {AudioCaptureRequestResult::Disconnected,
                        AudioCaptureRequestResult::Unavailable,
                        AudioCaptureRequestResult::QueueFull}) {
        Retries request;
        assert(!request.capture_requested(result, 100));
        request.activity_changed(200);
        assert(!request.due(60000099) && request.due(60000100));
        assert(!request.capture_requested(result, 60000100));
        assert(!request.due(180000099) && request.due(180000100));
    }
    Retries busy;
    assert(!busy.capture_requested(AudioCaptureRequestResult::Busy, 0));
    assert(!busy.due(999999) && busy.due(1000000));
    busy.activity_changed(1000000);
    assert(!busy.capture_requested(AudioCaptureRequestResult::Busy, kCaptureCompletionTimeoutUs - 1));
    assert(!busy.capture_requested(AudioCaptureRequestResult::Busy, kCaptureCompletionTimeoutUs));
    busy.activity_changed(kCaptureCompletionTimeoutUs + 1);
    assert(!busy.due(kCaptureCompletionTimeoutUs + 59999999));
    assert(busy.due(kCaptureCompletionTimeoutUs + 60000000));
    Retries transient;
    assert(!transient.capture_requested(AudioCaptureRequestResult::Busy, 0));
    assert(transient.capture_requested(AudioCaptureRequestResult::Queued, 1000000));
    assert(transient.due(1000000));
    // A later independent Busy period gets a new, bounded grace window.
    assert(!transient.capture_requested(AudioCaptureRequestResult::Busy, 20000000));
    assert(!transient.due(20999999) && transient.due(21000000));

    assert(kIntervalUs == 1000000);
    Retries retries;
    assert(retries.due(0));
    retries.begin(0);
    assert(!retries.due(kIntervalUs - 1) && retries.due(kIntervalUs));
    retries.failed(0);
    assert(!retries.due(60000000 - 1) && retries.due(60000000));
    // Failure cooldowns survive session changes; normal pauses may be removed.
    sessions.set_network(false, ""); sessions.set_network(true, "http://192.168.1.11/");
    assert(!retries.due(1000000));
    retries.failed(0); assert(!retries.due(120000000 - 1));
    retries.failed(0); assert(!retries.due(240000000 - 1));
    retries.failed(0); assert(!retries.due(300000000 - 1) && retries.due(300000000));
    retries.failed(0); assert(retries.due(300000000));
    retries.succeeded(0, false); assert(!retries.due(14999999) && retries.due(15000000));
    retries.activity_changed(100); assert(retries.due(100));
    retries.succeeded(0, true); assert(!retries.due(29999999) && retries.due(30000000));
    retries.activity_changed(100); assert(retries.due(100));
    retries.failed(0, 180000000); retries.activity_changed(100);
    assert(!retries.due(179999999) && retries.due(180000000));
    // Reproduce the worker's epoch-change branch on actual session transitions.
    Sessions playback;
    playback.set_network(true, "http://192.168.1.12/");
    playback.set_controller(true); playback.set_activity(true);
    auto observed_epoch = playback.snapshot().epoch;
    auto observe_session_change = [&](int64_t now) {
        const auto current = playback.snapshot();
        if (observed_epoch != current.epoch) retries.activity_changed(now);
        observed_epoch = current.epoch;
    };
    for (int transition = 0; transition < 3; ++transition) {
        Retries fresh;
        retries = fresh;
        retries.failed(100, 180000000);
        if (transition == 0) { playback.set_activity(false); playback.set_activity(true); }
        if (transition == 1) { playback.set_network(false, ""); playback.set_network(true, "http://192.168.1.12/"); }
        if (transition == 2) { playback.set_controller(false); playback.set_controller(true); }
        observe_session_change(200);
        assert(!retries.due(180000099) && retries.due(180000100));
        // Only after the failure deadline may the worker start and succeed.
        retries.begin(180000100);
        retries.succeeded(180000200, true);
        assert(!retries.due(180000201));
        playback.set_activity(false); playback.set_activity(true);
        observe_session_change(180000300);
        assert(retries.due(180000300));
    }
    assert(retry_after("Sat, 10 Oct 2026 12:03:00 GMT", 1791633600) == 180000000);
    assert(retry_after("Sat, 31 Feb 2026 12:00:00 GMT", 1) == 0);
    assert(retry_after("Sun, 29 Feb 2026 12:00:00 GMT", 1) == 0);
    assert(retry_after("Fri, 31 Apr 2026 12:00:00 GMT", 1) == 0);
    assert(retry_after("Sat, 00 Oct 2026 12:00:00 GMT", 1) == 0);
    assert(retry_after("Sat, 10 Oct 2026 24:00:00 GMT", 1) == 0);
    assert(retry_after("Sat, 10 Oct 2026 12:60:00 GMT", 1) == 0);
    assert(retry_after("Sat, 10 Oct 2026 12:00:60 GMT", 1) == 0);
    assert(retry_after("Thu, 29 Feb 2024 12:00:00 GMT", 1709207940) == 60000000);
    assert(retry_after("Mon, 29 Feb 2100 12:00:00 GMT", 1) == 0);
    assert(retry_after("180") == 180000000);
    assert(retry_after("0") == 0 && retry_after("invalid") == 0);
    assert(retry_after("9999999999999999999999999999999") == 0);
    retries.succeeded(0, false);
    retries.failed(0); assert(retries.due(60000000));
    retries.failed(1000000, std::numeric_limits<int64_t>::max());
    assert(!retries.due(std::numeric_limits<int64_t>::max() - 1));
    assert(!expired(100, 100 + kTrackLifetimeUs - 1));
    assert(expired(100, 100 + kTrackLifetimeUs));
}
