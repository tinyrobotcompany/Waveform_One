#include <cassert>
#include <cstring>
#include "display_text.h"
#include "recognition_feedback.h"

int main()
{
    using namespace recognition_feedback;
    for (auto status : {RecognitionStatus::Capturing, RecognitionStatus::Identifying}) {
        assert(searching(status));
        assert(std::strcmp(text(status, false), "Finding your song…") == 0);
        assert(std::strcmp(text(status, true), "Checking what’s playing…") == 0);
    }
    for (auto status : {RecognitionStatus::Matched, RecognitionStatus::NoMatch, RecognitionStatus::Waiting}) {
        assert(!searching(status));
        assert(std::strcmp(text(status, true), "Enjoy the music") == 0);
    }
    const auto supported = [](uint32_t cp) { return cp < 0x80 || cp == 0x2019 || cp == 0xe9; };
    assert(display_text::renderable("Don't you want me", supported) == "Don't you want me");
    assert(display_text::renderable("Don’t you want me · Beyoncé", supported) == "Don’t you want me ? Beyoncé");
    assert(display_text::renderable("Music 🎵\nNext", supported) == "Music ?\nNext");
    assert(display_text::renderable(std::string("bad\xc0\xaf", 5), supported) == "bad??");
    assert(display_text::renderable(std::string("\xed\xa0\x80", 3), supported) == "???");
    assert(display_text::renderable(std::string("\xf0\x9f", 2), supported) == "??");
    assert(display_text::renderable("", supported).empty());
}
