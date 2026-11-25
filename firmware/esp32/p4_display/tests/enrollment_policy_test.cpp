#include "enrollment_policy.h"
#include <cassert>
int main() {
    enrollment::Schedule s;
    assert(s.due(100, "192.168.1.2", 1000));
    s.success(100, "192.168.1.2", false, 10000);
    assert(!s.due(101, "192.168.1.2", 10000));
    assert(s.due(101, "192.168.1.3", 10000));
    assert(s.due(10000, "192.168.1.2", 10000));
    s.failure(100, 120);
    assert(!s.due(219, "192.168.1.3", 0));
    assert(s.due(220, "192.168.1.3", 0));
    s.success(220, "192.168.1.3", true, 10000);
    assert(!s.due(221, "192.168.1.3", 10000));
    assert(s.due(250, "192.168.1.3", 10000));
    assert(enrollment::valid_url("https://example.com/v1/device"));
    assert(!enrollment::valid_url("http://example.com/v1/device"));
    assert(!enrollment::valid_url("https://user@example.com/v1/device"));
    assert(!enrollment::valid_url("https://example.com/v1/device?x=1"));
}
