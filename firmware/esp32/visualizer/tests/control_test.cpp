#include <cassert>
#include <string>
#include <vector>
#include "control_protocol.h"
int main() {
    control::Command command{};
    assert(control::parse("WF1 42 MODE mirrored", command));
    assert(command.id == 42 && command.changeMode && command.mode == visual::Mode::Mirrored);
    assert(control::parse("WF1 1 STATUS", command) && !command.changeMode);
    for (const char* bad : {"WF2 1 STATUS", "WF1 0 STATUS", "WF1 -1 STATUS", "WF1 65536 STATUS",
            "WF1 1 MODE unknown", "WF1 1 MODE classic extra", "WF1 2 STATUS extra"})
        assert(!control::parse(bad, command));
    assert(control::parse("WF1 12 CAPTURE", command) && command.capture && !command.changeMode);
    assert(!control::parse("WF1 12 CAPTURE extra", command));
    control::Lines lines;
    std::vector<std::string> received;
    auto feed = [&](const std::string& input) {
        for (char c : input) lines.push(c, [&](std::string_view s) { received.emplace_back(s); });
    };
    feed("WF1 7 MO"); assert(received.empty());
    feed("DE waterfall\r\nWF1 8 STATUS\n");
    assert(received.size() == 2 && received[0] == "WF1 7 MODE waterfall");
    feed(std::string(200, 'x') + "WF1 9 MODE classic\nWF1 10 STATUS\n");
    assert(received.back() == "WF1 10 STATUS");
    assert(received.size() == 3); // Overlong line, including suffix, is discarded.
}
