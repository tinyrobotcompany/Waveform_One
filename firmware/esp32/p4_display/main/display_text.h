#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace display_text {
// Keep supported UTF-8 byte-for-byte. Unsupported or malformed characters use
// a visible '?' instead of LVGL's missing-glyph rectangle. Phone metadata stays
// untouched. Glyph availability is supplied by the actual label's font.
template<class Supported>
std::string renderable(std::string_view text, Supported supported)
{
    std::string result;
    result.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        const auto first = static_cast<uint8_t>(text[index]);
        size_t count = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2
            : first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        uint32_t cp = count == 1 ? first : first & (count == 2 ? 0x1f : count == 3 ? 0x0f : 0x07);
        bool valid = count != 0 && index + count <= text.size();
        for (size_t offset = 1; valid && offset < count; ++offset) {
            const auto next = static_cast<uint8_t>(text[index + offset]);
            valid = (next & 0xc0) == 0x80;
            cp = (cp << 6) | (next & 0x3f);
        }
        valid = valid && !(count == 2 && cp < 0x80) && !(count == 3 && cp < 0x800)
            && !(count == 4 && cp < 0x10000) && cp <= 0x10ffff
            && !(cp >= 0xd800 && cp <= 0xdfff);
        if (!valid) { result += '?'; ++index; continue; }
        if (cp == '\n' || cp == '\t' || supported(cp)) result.append(text.substr(index, count));
        else result += '?';
        index += count;
    }
    return result;
}
} // namespace display_text
