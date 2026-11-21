#include "update_feedback.h"
#include <cassert>
int main() {
    update_feedback::Pending pending;
    assert(!pending.active(0, false, "Update available"));
    pending.begin(100, "Update available");
    assert(pending.active(100, false, "Update available"));
    assert(pending.active(200, true, "Downloading"));
    assert(!pending.active(300, false, "Failed validation"));
    pending.begin(400, "Up to date");
    assert(pending.active(500, false, "Up to date"));
    assert(!pending.active(8000400, false, "Up to date"));
    pending.begin(9000000, "Update available");
    assert(!pending.active(9000001, false, "Connect to Wi-Fi"));
}
