// What the viscript command and its audit share: parsing of numeric options, JSON strings and
// boxes on stdout, plain words for a Result, loading fonts and a scene with the context of the
// optional modules, and the session that deletes the renderer and the scene of a command.
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
#include "../core/common/visString.h"

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

// Whether argv[i] names the option and the argument after it is a value for it; i then moves to
// the value.
static inline bool _option(int argc, char** argv, int& i, const char* name, uint32_t& output, bool allowZero = false)
{
    if (std::strcmp(argv[i], name) != 0 || i + 1 >= argc || !_uint(argv[i + 1], output, allowZero)) return false;
    i++;
    return true;
}

static inline bool _option(int argc, char** argv, int& i, const char* name, float& output)
{
    if (std::strcmp(argv[i], name) != 0 || i + 1 >= argc || !_float(argv[i + 1], output)) return false;
    i++;
    return true;
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
                } else if (auto size = vis::utf8Sequence(cursor, static_cast<size_t>(end - cursor))) {
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

// A JSON string, or null for nullptr.
static inline void _json(const char* value)
{
    if (value) _jsonString(value, std::strlen(value));
    else std::fputs("null", stdout);
}

static inline void _bbox(const vis::BBox& bounds)
{
    std::printf("{\"x\":%.6g,\"y\":%.6g,\"width\":%.6g,\"height\":%.6g}", bounds.x, bounds.y, bounds.width,
                bounds.height);
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

// Other fonts stand in for a font that is not loaded, the next loaded one of a text's list or
// the default font, which may lack its letters and differ in shape and width; names those fonts.
static inline void _missingFonts(const vis::Scene* scene)
{
    const char* fonts[16];
    auto count = scene->missingFonts(fonts, 16u);
    for (auto i = 0u; i < count; i++) {
        std::fprintf(stderr, "viscript: the font '%s' is not loaded, so other fonts stand in for it; pass its "
                     "file, such as %s.ttf, with --font\n", fonts[i], fonts[i]);
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

// Loads font files in order; they load before a script runs, which may measure text in them.
static inline bool _loadFonts(vis::Renderer* renderer, const char* const* paths, uint32_t count)
{
    for (auto i = 0u; i < count; i++) {
        if (renderer->font(paths[i]) == vis::Result::Success) continue;
        auto reason = renderer->diagnostic();
        if (reason) std::fprintf(stderr, "viscript: %s\n", reason);
        else std::fprintf(stderr, "viscript: could not load the font file '%s'\n", paths[i]);
        return false;
    }
    return true;
}

// The renderer and the scene a command works on, deleted in that order when it returns.
struct Session
{
    vis::Renderer* renderer = nullptr;
    vis::Scene* scene = nullptr;

    ~Session()
    {
        delete renderer;
        delete scene;
    }

    // Makes the renderer, loads the fonts into it and then the scene, into context when one is
    // given. Returns 0, or after reporting the failure the exit code: 3 when the renderer or a
    // font fails, 2 when the scene does.
    int open(const char* source, const char* const* fonts, uint32_t fontCount,
             vis::detail::LuaHostContext* context = nullptr)
    {
        renderer = vis::Renderer::gen();
        if (!renderer) {
            std::fprintf(stderr, "viscript: the renderer is unavailable\n");
            return 3;
        }
        if (!_loadFonts(renderer, fonts, fontCount)) return 3;
        scene = context ? _load(source, *context) : _load(source);
        return scene ? 0 : 2;
    }
};

#endif
