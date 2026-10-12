#include <cassert>
#include "recognition_policy.h"

int main()
{
    using namespace recognition_policy;
    constexpr uint32_t s = Planner::kHopsPerSecond;

    // Sessions: network and controller changes invalidate work.
    Sessions sessions;
    assert(!sessions.snapshot().ready);
    assert(sessions.set_controller(true));
    assert(!sessions.snapshot().ready);
    assert(sessions.set_network(true, "http://192.168.1.10/pair?code=first"));
    const auto original = sessions.snapshot();
    assert(original.ready);
    assert(!sessions.set_network(true, "http://192.168.1.10/pair?code=rotated"));
    assert(sessions.snapshot().epoch == original.epoch);
    assert(!sessions.set_controller(true));
    assert(sessions.set_network(false, ""));
    assert(!sessions.snapshot().ready && sessions.snapshot().epoch != original.epoch);
    assert(sessions.set_network(true, "http://192.168.1.10/pair?code=new"));
    auto before = sessions.snapshot();
    assert(sessions.set_network(true, "http://192.168.1.11/pair?code=changed"));
    assert(sessions.snapshot().epoch != before.epoch);
    before = sessions.snapshot();
    assert(sessions.set_controller(false));
    assert(!sessions.snapshot().ready && sessions.snapshot().epoch != before.epoch);
    assert(sessions.set_network(true, ""));
    assert(!sessions.snapshot().ready);

    // Planner: nothing happens without sound.
    Planner planner;
    Window window;
    assert(!planner.due(10 * s, window));
    // Sound at 10 s: lookups at 4 and 10 s of it, each from its start.
    planner.sound_started(10 * s);
    assert(!planner.due(14 * s - 1, window));
    assert(planner.due(14 * s, window) && window.first == 10 * s && window.last == 14 * s);
    planner.attempted(14 * s);
    assert(!planner.missed() && !planner.unidentified());
    assert(!planner.due(20 * s - 1, window));
    assert(planner.due(20 * s, window) && window.first == 12 * s && window.last == 20 * s);
    planner.attempted(20 * s);
    assert(planner.unidentified());
    // Then the latest eight seconds every 30 s.
    assert(!planner.due(50 * s - 1, window));
    assert(planner.due(50 * s, window) && window.first == 42 * s && window.last == 50 * s);
    planner.attempted(50 * s);
    // A late lookup (service limits) still takes at most eight seconds.
    assert(planner.due(90 * s, window) && window.last - window.first == 8 * s);

    // A paid service allows three quick tries: 3, 5 and 8 s.
    Planner fast{3, 5, 8};
    fast.sound_started(0);
    assert(!fast.due(3 * s - 1, window) && fast.due(3 * s, window) && window.last == 3 * s);
    fast.attempted(3 * s);
    assert(fast.due(5 * s, window) && window.last == 5 * s);
    fast.attempted(5 * s);
    assert(!fast.unidentified() && fast.due(8 * s, window) && window.first == 0);
    fast.attempted(8 * s);
    assert(fast.unidentified() && !fast.due(37 * s, window) && fast.due(38 * s, window));
    // Stopping keeps the service's tries.
    fast.stopped(); fast.sound_started(100 * s);
    assert(fast.due(103 * s, window));

    // Cautious after repeated unidentified sound: one try at 6 s, then a minute.
    Planner careful{3, 5, 8};
    careful.set_cautious(true);
    careful.sound_started(0);
    assert(!careful.due(3 * s, window) && !careful.due(6 * s - 1, window) && careful.due(6 * s, window));
    careful.attempted(6 * s);
    assert(careful.unidentified() && !careful.due(66 * s - 1, window) && careful.due(66 * s, window));
    // stopped() keeps the caution; the caller lifts it after a match.
    careful.stopped(); careful.sound_started(100 * s);
    assert(careful.cautious() && !careful.due(103 * s, window));
    careful.set_cautious(false);
    assert(careful.due(103 * s, window));

    // A match with a known length waits for the end of the song.
    Planner song;
    song.sound_started(0);
    assert(song.due(4 * s, window));
    song.attempted(4 * s);
    // Clip started 20 s into a 200 s song: it ends 180 s after the clip start.
    song.matched(window, 4 * s, 20, 200);
    assert(song.matched() && !song.unidentified() && song.expected_end() == 180 * s);
    assert(!song.due(184 * s - 1, window));
    // Four seconds into whatever follows, identify it from its start.
    assert(song.due(184 * s, window) && window.first == 180 * s && window.last == 184 * s);
    song.attempted(184 * s);
    // The next song is found: a new end is set.
    song.matched(window, 184 * s, 1, 241);
    assert(song.expected_end() == 420 * s);
    // Past that end nothing is heard: the old song must go.
    assert(song.due(424 * s, window));
    song.attempted(424 * s);
    assert(song.missed() && !song.matched());
    // ...and identification carries on for the next one.
    assert(song.due(430 * s, window) && window.first == 422 * s);

    // Without a length, check once a minute; two misses clear the song.
    Planner unknown;
    unknown.sound_started(0);
    assert(unknown.due(4 * s, window));
    unknown.attempted(4 * s);
    unknown.matched(window, 4 * s, -1, 0);
    assert(unknown.expected_end() == 0);
    assert(!unknown.due(64 * s - 1, window));
    assert(unknown.due(64 * s, window) && window.last - window.first == 8 * s);
    unknown.attempted(64 * s);
    assert(!unknown.missed() && unknown.matched());
    assert(unknown.due(124 * s, window));
    unknown.attempted(124 * s);
    assert(unknown.missed() && !unknown.matched());
    // Implausible lengths or offsets count as unknown.
    Planner odd;
    odd.sound_started(0);
    window = {0, 3 * s};
    odd.matched(window, 3 * s, 50, 40);
    assert(odd.expected_end() == 0);
    odd.matched(window, 3 * s, 0, 7200);
    assert(odd.expected_end() == 0);
    // Almost at the end: check again in five seconds.
    odd.matched(window, 3 * s, 37.5, 40);
    assert(odd.expected_end() == 0 && !odd.due(8 * s - 1, window) && odd.due(8 * s, window));

    // Silence stops everything and reports whether a song was showing.
    assert(song.stopped() == false);
    assert(!song.active() && !song.due(1000 * s, window));
    Planner showing;
    showing.sound_started(0);
    window = {0, 3 * s};
    showing.matched(window, 3 * s, 10, 200);
    assert(showing.stopped() && !showing.matched());

    // Failures back off from 60 s to five minutes, or as the service asks.
    Retries retries;
    assert(retries.due(0) && retries.remaining_us(0) == 0);
    retries.failed(0);
    assert(!retries.due(60000000 - 1) && retries.due(60000000));
    retries.failed(0); assert(!retries.due(120000000 - 1));
    retries.failed(0); assert(!retries.due(240000000 - 1));
    retries.failed(0); assert(!retries.due(300000000 - 1) && retries.due(300000000));
    retries.failed(0); assert(retries.due(300000000));
    retries.succeeded();
    retries.failed(0); assert(retries.due(60000000));
    retries.failed(0, 180000000);
    assert(!retries.due(179999999) && retries.due(180000000));
    retries.failed(1000000, std::numeric_limits<int64_t>::max());
    assert(!retries.due(std::numeric_limits<int64_t>::max() - 1));

    // Budget: three in any rolling minute.
    Budget budget;
    for (int i = 0; i < 3; ++i) { assert(budget.available(i * 1000000)); budget.spend(i * 1000000); }
    assert(!budget.available(3000000) && !budget.available(59999999));
    assert(budget.available(60000000));
    budget.spend(60000000);
    assert(!budget.available(60999999) && budget.available(61000000));
    // Long quiet periods do not bank extra lookups.
    for (int i = 0; i < 3; ++i) { assert(budget.available(3600000000)); budget.spend(3600000000); }
    assert(!budget.available(3600000000));

    Budget paid(12);
    for (int i = 0; i < 12; ++i) { assert(paid.available(i)); paid.spend(i); }
    assert(!paid.available(12) && paid.available(60000000));

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
}
