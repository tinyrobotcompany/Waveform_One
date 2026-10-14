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
    for (const char *sample : {"Don't you want me", "Don’t you want me · Beyoncé", "Music 🎵\nNext",
            "Donʼt · 東京 · 서울 · العربية · שלום · हिन्दी", "\xf4\x8f\xbf\xbf"}) {
        assert(display_text::renderable(sample) == sample);
    }
    assert(display_text::renderable(std::string("bad\xc0\xaf", 5)) == "bad��");
    assert(display_text::renderable(std::string("\xed\xa0\x80", 3)) == "���");
    assert(display_text::renderable(std::string("\xf0\x9f", 2)) == "��");
    assert(display_text::renderable("").empty());
}
