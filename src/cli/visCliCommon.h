// What the viscript command and its audit share: parsing of numeric arguments, JSON strings on
// stdout, plain words for a Result, and loading a scene with the context of the optional modules.
#ifndef _VIS_CLI_COMMON_H_
#define _VIS_CLI_COMMON_H_

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "vis.h"
#include "visLuaHost.h"

// A positive decimal that fits uint32_t, or with allowZero a non-negative one.
static inline bool _uint(const char* value, uint32_t& output, bool allowZero = false)
{
    if (!value || !value[0]) return false;
    for (auto cursor = value; *cursor; cursor++) {
        if (*cursor < '0' || *cursor > '9') return false;
    }
    errno = 0;
    char* end = nullptr;
    auto number = std::strtoull(value, &end, 10);
    if (errno == ERANGE || !end || *end || (!allowZero && !number) || number > UINT32_MAX) return false;
    output = static_cast<uint32_t>(number);
    return true;
}

// A finite non-negative decimal.
static inline bool _float(const char* value, float& output)
{
    if (!value || !value[0]) return false;
    char* end = nullptr;
    auto number = std::strtof(value, &end);
    if (!end || *end || !std::isfinite(number) || number < 0.0f) return false;
    output = number;
    return true;
}

// The length of the well-formed UTF-8 sequence at value, or 0.
static inline uint32_t _utf8(const unsigned char* value, size_t remaining)
{
    auto continuation = [](unsigned char byte) {
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

// Writes size bytes of text to stdout as a JSON string: quoted, with control characters and
// malformed UTF-8 escaped.
static inline void _jsonString(const char* value, size_t size)
{
    std::putchar('"');
    auto cursor = reinterpret_cast<const unsigned char*>(value);
    auto end = cursor + size;
    while (cursor < end) {
        auto byte = *cursor;
        switch (byte) {
            case '"':
                std::fputs("\\\"", stdout);
                cursor++;
                break;
            case '\\':
                std::fputs("\\\\", stdout);
                cursor++;
                break;
            case '\n':
                std::fputs("\\n", stdout);
                cursor++;
                break;
            case '\r':
                std::fputs("\\r", stdout);
                cursor++;
                break;
            case '\t':
                std::fputs("\\t", stdout);
                cursor++;
                break;
            default:
                if (byte < 0x20) {
                    std::printf("\\u%04x", byte);
                    cursor++;
                } else if (byte < 0x80) {
                    std::putchar(byte);
                    cursor++;
                } else if (auto size = _utf8(cursor, static_cast<size_t>(end - cursor))) {
                    std::fwrite(cursor, 1, size, stdout);
                    cursor += size;
                } else {
                    std::printf("\\u%04x", byte);
                    cursor++;
                }
                break;
        }
    }
    std::putchar('"');
}


// Plain words for a failure that nothing explained more precisely.
static inline const char* _describe(vis::Result status)
{
    switch (status) {
        case vis::Result::InvalidArguments: return "an argument is out of range";
        case vis::Result::InsufficientCondition: return "the scene is not ready for it";
        case vis::Result::NonSupport: return "this build does not support it";
        case vis::Result::OutOfMemory: return "out of memory";
        case vis::Result::IoError: return "a file could not be read or written";
        case vis::Result::ScriptError: return "the script failed";
        default: return "an unexpected engine error occurred";
    }
}

// Text in a font that is not loaded is drawn in the default font, which may lack its letters
// and differs in shape and width; names those fonts.
static inline void _missingFonts(const vis::Scene* scene)
{
    const char* fonts[16];
    auto count = scene->missingFonts(fonts, 16u);
    for (auto i = 0u; i < count; i++) {
        std::fprintf(stderr, "viscript: the font '%s' is not loaded, so its text is drawn in the default "
                     "font; pass its file, such as %s.ttf, with --font\n", fonts[i], fonts[i]);
    }
}

// Loads a scene; context keeps what the optional modules collected, such as audio cues.
static inline vis::Scene* _load(const char* path, vis::detail::LuaHostContext& context)
{
    char error[2048] = {};
    vis::Scene* scene = nullptr;
    auto status = vis::detail::luaHostLoadFile(path, &scene, error, sizeof(error), context);
    if (status == vis::Result::Success) {
        _missingFonts(scene);
        return scene;
    }
    if (error[0]) std::fprintf(stderr, "viscript: %s\n", error);
    else std::fprintf(stderr, "viscript: could not load %s: %s\n", path, _describe(status));
    return nullptr;
}

static inline vis::Scene* _load(const char* path)
{
    vis::detail::LuaHostContext context;
    return _load(path, context);
}

#endif
