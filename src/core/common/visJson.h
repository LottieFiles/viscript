// JSON text that grows as it is written, for the reports the engine hands its hosts.
#ifndef _VIS_JSON_H_
#define _VIS_JSON_H_

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "visGrow.h"

namespace vis
{

// The length of the well-formed UTF-8 sequence at value, or 0.
static inline uint32_t utf8Sequence(const uint8_t* value, size_t remaining)
{
    auto continuation = [](uint8_t byte) {
        return byte >= 0x80 && byte <= 0xbf;
    };
    if (remaining >= 2 && value[0] >= 0xc2 && value[0] <= 0xdf && continuation(value[1])) return 2;
    if (remaining >= 3 && value[0] == 0xe0 && value[1] >= 0xa0 && value[1] <= 0xbf && continuation(value[2])) return 3;
    if (remaining < 3) return 0;
    if (((value[0] >= 0xe1 && value[0] <= 0xec) || (value[0] >= 0xee && value[0] <= 0xef)) && continuation(value[1]) && continuation(value[2])) {
        return 3;
    }
    if (value[0] == 0xed && value[1] >= 0x80 && value[1] <= 0x9f && continuation(value[2])) return 3;
    if (remaining < 4) return 0;
    if (value[0] == 0xf0 && value[1] >= 0x90 && value[1] <= 0xbf && continuation(value[2]) && continuation(value[3])) {
        return 4;
    }
    if (value[0] >= 0xf1 && value[0] <= 0xf3 && continuation(value[1]) && continuation(value[2]) && continuation(value[3])) {
        return 4;
    }
    if (value[0] == 0xf4 && value[1] >= 0x80 && value[1] <= 0x8f && continuation(value[2]) && continuation(value[3])) {
        return 4;
    }
    return 0;
}

struct JsonBuffer
{
    ~JsonBuffer()
    {
        delete[] data;
    }

    // Makes room for additional bytes and the terminator after them.
    bool reserve(uint32_t additional)
    {
        if (additional > UINT32_MAX - size - 1u) return false;
        return grow(data, size, capacity, additional + 1u, 4096u);
    }

    bool append(const void* value, size_t length)
    {
        if (length > UINT32_MAX || !reserve(static_cast<uint32_t>(length))) return false;
        std::memcpy(data + size, value, length);
        size += static_cast<uint32_t>(length);
        data[size] = 0;
        return true;
    }

    bool append(const char* value)
    {
        return append(value, std::strlen(value));
    }

    bool format(const char* format, ...)
    {
        va_list arguments;
        va_start(arguments, format);
        va_list countArguments;
        va_copy(countArguments, arguments);
        auto length = std::vsnprintf(nullptr, 0, format, countArguments);
        va_end(countArguments);
        if (length < 0 || !reserve(static_cast<uint32_t>(length))) {
            va_end(arguments);
            return false;
        }
        std::vsnprintf(reinterpret_cast<char*>(data + size), static_cast<size_t>(length) + 1u,
                       format, arguments);
        va_end(arguments);
        size += static_cast<uint32_t>(length);
        return true;
    }

    // A JSON string, or null for nullptr: quoted, with control characters and the bytes of
    // malformed UTF-8 escaped, so that the text stays UTF-8.
    bool string(const char* value)
    {
        if (!value) return append("null");
        if (!append("\"")) return false;
        auto cursor = reinterpret_cast<const uint8_t*>(value);
        auto end = cursor + std::strlen(value);
        while (cursor < end) {
            auto length = 1u;
            auto written = false;
            switch (*cursor) {
                case '\"': written = append("\\\""); break;
                case '\\': written = append("\\\\"); break;
                case '\n': written = append("\\n"); break;
                case '\r': written = append("\\r"); break;
                case '\t': written = append("\\t"); break;
                default:
                    if (*cursor >= 0x80u) length = utf8Sequence(cursor, static_cast<size_t>(end - cursor));
                    if (*cursor >= 0x20u && length) {
                        written = append(cursor, length);
                    } else {
                        length = 1u;
                        written = format("\\u%04x", *cursor);
                    }
                    break;
            }
            if (!written) return false;
            cursor += length;
        }
        return append("\"");
    }

    // Hands over the text, which the caller then deletes, and leaves the buffer empty.
    uint8_t* release()
    {
        auto output = data;
        data = nullptr;
        size = 0;
        capacity = 0;
        return output;
    }

    // size bytes of text and a terminator after them.
    uint8_t* data = nullptr;
    uint32_t size = 0;
    uint32_t capacity = 0;
};

}  // namespace vis

#endif
