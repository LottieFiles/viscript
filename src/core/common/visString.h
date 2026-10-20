// C strings: a copy in new storage, whether one ends in a file extension, and the length of a
// UTF-8 sequence.
#ifndef _VIS_STRING_H_
#define _VIS_STRING_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace vis
{

// A copy of text in new storage, or nullptr.
static inline char* _duplicate(const char* text)
{
    if (!text) return nullptr;
    auto size = std::strlen(text) + 1u;
    auto copy = new (std::nothrow) char[size];
    if (copy) std::memcpy(copy, text, size);
    return copy;
}

// Whether text ends in suffix, a lower case extension such as ".png", which its letters match in
// either case.
static inline bool _ends(const char* text, const char* suffix)
{
    auto textSize = std::strlen(text);
    auto suffixSize = std::strlen(suffix);
    if (textSize < suffixSize) return false;
    text += textSize - suffixSize;
    for (auto i = 0u; i < suffixSize; i++) {
        auto c = text[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != suffix[i]) return false;
    }
    return true;
}

// The length of the well-formed UTF-8 sequence of two to four bytes at value, or 0. The lead byte
// gives the length and the bytes after it run from 0x80 to 0xbf, but four leads narrow the second
// byte's range to keep out overlong forms, surrogates and code points past U+10FFFF.
static inline uint32_t utf8Sequence(const uint8_t* value, size_t remaining)
{
    if (!remaining || value[0] < 0xc2 || value[0] > 0xf4) return 0;
    auto lead = value[0];
    auto length = lead < 0xe0 ? 2u : lead < 0xf0 ? 3u : 4u;
    if (remaining < length) return 0;
    uint8_t low = lead == 0xe0 ? 0xa0 : lead == 0xf0 ? 0x90 : 0x80;
    uint8_t high = lead == 0xed ? 0x9f : lead == 0xf4 ? 0x8f : 0xbf;
    if (value[1] < low || value[1] > high) return 0;
    for (auto i = 2u; i < length; i++) {
        if (value[i] < 0x80 || value[i] > 0xbf) return 0;
    }
    return length;
}

}  // namespace vis

#endif
