#include <cassert>
#include <string>
#include <vector>

#include "wf1_protocol.h"

int main()
{
    assert(wf1::mode_request(7, wf1::Mode::Mirrored) == "WF1 7 MODE mirrored\n");
    assert(wf1::status_request(8) == "WF1 8 STATUS\n");
    assert(wf1::mode_request(0, wf1::Mode::Classic).empty());

    wf1::Replies replies(42);
    std::vector<wf1::Reply> received;
    auto feed = [&](std::string_view bytes) {
        for (const char byte : bytes) {
            replies.push(byte, [&](const wf1::Reply &reply) { received.push_back(reply); });
        }
    };

    feed("SHUT RMS=0.004\nWF1 41 OK MODE classic\nWF1 42 OK MO");
    assert(received.empty());
    feed("DE mirrored\r\n");
    assert(received.size() == 1);
    assert(received[0].id == 42);
    assert(received[0].ok);
    assert(received[0].mode == wf1::Mode::Mirrored);

    feed(std::string(600, 'x'));
    feed("WF1 42 OK MODE classic\n");
    assert(received.size() == 1);
    feed("WF1 42 ERR BUSY\n");
    assert(received.size() == 2);
    assert(!received[1].ok);

    replies.expect(43);
    feed("WF1 42 OK MODE mirrored\nWF1 43 OK MODE waterfall\n");
    assert(received.size() == 3);
    assert(received.back().id == 43);
    assert(received.back().mode == wf1::Mode::Waterfall);
}
