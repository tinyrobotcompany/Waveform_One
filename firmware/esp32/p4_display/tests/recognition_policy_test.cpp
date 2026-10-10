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

    assert(kIntervalUs == 1000000);
    Retries retries;
    assert(retries.due(0));
    retries.begin(0);
    assert(!retries.due(kIntervalUs - 1) && retries.due(kIntervalUs));
    retries.failed(0);
    assert(!retries.due(60000000 - 1) && retries.due(60000000));
    // Session changes never reset the worker's independent retry schedule.
    sessions.set_network(false, ""); sessions.set_network(true, "http://192.168.1.11/");
    assert(!retries.due(1000000));
    retries.failed(0); assert(!retries.due(120000000 - 1));
    retries.failed(0); assert(!retries.due(240000000 - 1));
    retries.failed(0); assert(!retries.due(300000000 - 1) && retries.due(300000000));
    retries.failed(0); assert(retries.due(300000000));
    retries.succeeded(0); assert(retries.due(kIntervalUs));
    retries.failed(0); assert(retries.due(60000000));
    assert(!expired(100, 100 + kTrackLifetimeUs - 1));
    assert(expired(100, 100 + kTrackLifetimeUs));
}
