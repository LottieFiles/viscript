// Text that changes over time: UTF-8 codepoints, the clusters that stay together, and the
// string shown part way between two strings.
#include <cmath>
#include <cstring>
#include <new>

#include "visScene.h"

namespace vis
{

// The codepoint of the UTF-8 sequence at text, and its length in bytes; a byte that starts no
// well-formed sequence stands alone as codepoint zero.
static uint32_t _codepoint(const char* text, uint32_t& length)
{
    auto lead = static_cast<uint8_t>(text[0]);
    uint32_t codepoint;
    length = 1u;
    if (lead < 0x80u) return lead;
    if ((lead & 0xe0u) == 0xc0u) {
        codepoint = lead & 0x1fu;
        length = 2u;
    } else if ((lead & 0xf0u) == 0xe0u) {
        codepoint = lead & 0x0fu;
        length = 3u;
    } else if ((lead & 0xf8u) == 0xf0u) {
        codepoint = lead & 0x07u;
        length = 4u;
    } else {
        return 0u;
    }
    for (auto i = 1u; i < length; i++) {
        auto next = static_cast<uint8_t>(text[i]);
        if ((next & 0xc0u) != 0x80u) {
            length = 1u;
            return 0u;
        }
        codepoint = (codepoint << 6u) | (next & 0x3fu);
    }
    return codepoint;
}

// Codepoints that belong to the one before them: combining marks, the vowels and finals of
// conjoining Hangul, variation selectors, emoji modifiers and tags.
static bool _extends(uint32_t codepoint)
{
    return (codepoint >= 0x300u && codepoint <= 0x36fu) || (codepoint >= 0x1ab0u && codepoint <= 0x1affu)
           || (codepoint >= 0x1dc0u && codepoint <= 0x1dffu) || (codepoint >= 0x20d0u && codepoint <= 0x20ffu)
           || (codepoint >= 0xfe20u && codepoint <= 0xfe2fu) || (codepoint >= 0x3099u && codepoint <= 0x309au)
           || (codepoint >= 0x1160u && codepoint <= 0x11ffu) || (codepoint >= 0xd7b0u && codepoint <= 0xd7ffu)
           || (codepoint >= 0xfe00u && codepoint <= 0xfe0fu) || (codepoint >= 0xe0100u && codepoint <= 0xe01efu)
           || (codepoint >= 0x1f3fbu && codepoint <= 0x1f3ffu) || (codepoint >= 0xe0020u && codepoint <= 0xe007fu);
}

// The bytes of the character at text, which a text change never splits: a codepoint with
// those that belong to it, the two regional indicators of a flag, or emoji that zero width
// joiners join.
static size_t _character(const char* text)
{
    auto indicator = [](uint32_t codepoint) { return codepoint >= 0x1f1e6u && codepoint <= 0x1f1ffu; };
    if (!*text) return 0u;
    uint32_t length;
    auto first = _codepoint(text, length);
    size_t size = length;
    if (indicator(first) && text[size] && indicator(_codepoint(text + size, length))) size += length;
    while (text[size]) {
        auto next = _codepoint(text + size, length);
        if (next == 0x200du) {
            size += length;
            if (!text[size]) break;
            _codepoint(text + size, length);
        } else if (!_extends(next)) {
            break;
        }
        size += length;
    }
    return size;
}

// Writes what a text change from one string to another shows at progress into output, which
// has room for the bytes of both and a terminating zero, and returns its length in bytes.
static size_t _lerpText(const char* from, const char* to, double progress, TextChange change, char* output)
{
    progress = progress < 0.0 ? 0.0 : progress > 1.0 ? 1.0 : progress;
    auto characters = [](const char* text) {
        size_t count = 0u;
        for (auto at = text; *at; at += _character(at)) count++;
        return count;
    };
    auto fromCount = characters(from);
    auto toCount = characters(to);
    auto at = output;
    // Copies the next character of text to output; both move past it.
    auto copy = [&at](const char*& text) {
        auto size = _character(text);
        std::memcpy(at, text, size);
        at += size;
        text += size;
    };
    auto skip = [](const char*& text) { text += _character(text); };
    if (change == TextChange::Retype) {
        // What both strings begin with stays; erasing the rest of from and typing the rest of
        // to take a step per character.
        size_t common = 0u;
        for (auto a = from, b = to; *a && *b; common++) {
            auto size = _character(a);
            if (size != _character(b) || std::memcmp(a, b, size) != 0) break;
            a += size;
            b += size;
        }
        auto erased = fromCount - common;
        auto steps = erased + toCount - common;
        auto done = static_cast<size_t>(std::floor(static_cast<double>(steps) * progress));
        if (done > steps) done = steps;
        auto source = done <= erased ? from : to;
        auto count = done <= erased ? fromCount - done : common + done - erased;
        for (size_t i = 0u; i < count; i++)
            copy(source);
    } else if (toCount >= fromCount) {
        // Sweep replaces the prefix with target characters while interpolating the string length.
        auto fromSize = static_cast<double>(fromCount);
        auto toSize = static_cast<double>(toCount);
        auto current = std::floor(toSize * progress);
        auto reach = std::floor(fromSize - 1.0 + (toSize - fromSize + 1.0) * progress) + 1.0;
        auto length = static_cast<size_t>(std::fmin(toSize, std::fmax(current, std::fmax(fromSize, reach))));
        auto turned = static_cast<size_t>(current);
        for (size_t i = 0u; i < length; i++) {
            if (i < turned || i >= fromCount) {
                copy(to);
                if (i < fromCount) skip(from);
            } else {
                copy(from);
                skip(to);
            }
        }
    } else {
        // Right to left: the characters from current on turn into to's, and those past to's
        // end drop while the length shrinks from from's to to's.
        auto fromSize = static_cast<double>(fromCount);
        auto toSize = static_cast<double>(toCount);
        auto current = std::floor(fromSize * (1.0 - progress) + 0.5);
        auto reach = std::floor(fromSize + 1.0 + (toSize - fromSize - 1.0) * progress);
        auto length = static_cast<size_t>(std::fmin(fromSize, std::fmax(current, std::fmax(toSize, reach))));
        auto kept = static_cast<size_t>(current);
        for (size_t i = 0u; i < length; i++) {
            if (i < kept || i >= toCount) {
                copy(from);
                if (i < toCount) skip(to);
            } else {
                copy(to);
                skip(from);
            }
        }
    }
    *at = '\0';
    return static_cast<size_t>(at - output);
}

float textOpacity(const VisualState& state) noexcept
{
    if (state.textChange != TextChange::Fade || !state.textFrom || !state.text
        || state.textFrom == state.text || std::strcmp(state.textFrom, state.text) == 0) return 1.0f;
    auto progress = std::fmin(1.0f, std::fmax(0.0f, state.textProgress));
    if (!*state.textFrom) return progress;
    if (!*state.text) return 1.0f - progress;
    return std::fabs(1.0f - 2.0f * progress);
}

Result shown(const Text* text, const VisualState& state, const char*& value, char*& written) noexcept
{
    value = text->text();
    written = nullptr;
    if (text->counting()) {
        // A counting text writes the number it shows at this time; its string holds the room
        // any number needs beside the prefix and suffix.
        auto capacity = static_cast<uint32_t>(std::strlen(value)) + 65u;
        written = new (std::nothrow) char[capacity];
        if (!written) return Result::OutOfMemory;
        if (!text->write(state.value, written, capacity)) {
            delete[] written;
            written = nullptr;
            return Result::InvalidArguments;
        }
        value = written;
        return Result::Success;
    }
    if (!state.text) return Result::Success;
    value = state.text;
    if (!state.textFrom || state.textFrom == state.text || state.textProgress >= 1.0f) return Result::Success;
    if (state.textProgress <= 0.0f) {
        value = state.textFrom;
        return Result::Success;
    }
    if (state.textChange == TextChange::Fade) {
        if (*state.textFrom && (!*state.text || state.textProgress < 0.5f)) value = state.textFrom;
        return Result::Success;
    }
    auto fromLength = std::strlen(state.textFrom);
    auto toLength = std::strlen(state.text);
    if (fromLength > SIZE_MAX - 1u - toLength) return Result::OutOfMemory;
    written = new (std::nothrow) char[fromLength + toLength + 1u];
    if (!written) return Result::OutOfMemory;
    _lerpText(state.textFrom, state.text, state.textProgress, state.textChange, written);
    value = written;
    return Result::Success;
}

}  // namespace vis
