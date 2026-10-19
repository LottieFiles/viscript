// The viscript command line: render, benchmark, layout, inspect and capabilities, with the
// fonts, assets and audio the arguments name. The audit command is in visAudit.cpp.
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "vis.h"
#ifdef VIS_AUDIT
#include "visAudit.h"
#endif
#include "visLuaHost.h"
#include "visCliCommon.h"
#if defined(VIS_AUDIO) && !defined(_WIN32)
#include <unistd.h>
#include "vis_audio.h"
#include "visSaver.h"
#define VIS_CLI_SOUNDTRACK
#endif

using namespace vis;

static void _usage()
{
    std::fprintf(stderr,
                 "Usage:\n"
                 "  viscript render <scene.lua> [-o output.png|mp4|gif] [--time SEC]\n"
                 "               [--width PX] [--height PX] [--fps N] [--font FILE]... [--audio FILE]\n"
                 "  viscript benchmark <scene.lua> [--frames N] [--warmup N] [--font FILE]...\n"
                 "  viscript layout <scene.lua> [--time SEC] [--padding PX] [--font FILE]...\n"
#ifdef VIS_AUDIT
                 "  viscript audit <scene.lua> --font FILE [--font FILE]... [--format json|text] [audit options]\n"
#endif
                 "  viscript inspect <scene.lua> [--font FILE]...\n"
                 "  viscript capabilities\n"
                 "Repeat --font for each font file; text names a font by its file name without the\n"
                 "extension, so font = \"Pretendard\" needs a file such as Pretendard.ttf. Fonts\n"
                 "load before the script runs, so a script can measure text in them.\n"
                 "Default themes use Pretendard.ttf and JetBrainsMono.ttf (code);\n"
                 "pass both with --font when a script uses both text roles. Text in a font\n"
                 "that is not loaded is drawn in the engine's default font, with a warning.\n"
                 "--audio plays a sound file, such as a narration, from time 0 in an .mp4.\n"
    );
}

static int _capabilities()
{
#ifdef VIS_ENGINE_CPU
    constexpr auto preview = "true";
#else
    constexpr auto preview = "false";
#endif
#if defined(VIS_AUDIT) && defined(VIS_ENGINE_CPU)
    constexpr auto audit = "true";
#else
    constexpr auto audit = "false";
#endif
    std::printf("{\"format\":\"viscript.cli-capabilities\",\"version\":1,"
                "\"nativeInspection\":{\"preview\":%s,\"audit\":%s}}\n",
                preview, audit);
    return 0;
}

static bool _ends(const char* value, const char* suffix)
{
    auto valueSize = std::strlen(value);
    auto suffixSize = std::strlen(suffix);
    if (valueSize < suffixSize) return false;
    return std::strcmp(value + valueSize - suffixSize, suffix) == 0;
}

// A JSON string; nullptr is the empty string.
static void _json(const char* value)
{
    _jsonString(value ? value : "", value ? std::strlen(value) : 0u);
}

// Reports a failed renderer call, preferring the renderer's own reason.
static void _failed(const char* action, Result status, const Renderer* renderer)
{
    auto reason = renderer ? renderer->diagnostic() : nullptr;
    std::fprintf(stderr, "viscript: %s failed: %s\n", action, reason ? reason : _describe(status));
}

// Font files given with --font, in command-line order.
struct Fonts
{
    static constexpr uint32_t Limit = 64u;
    const char* paths[Limit] = {};
    uint32_t count = 0u;

    bool add(const char* path)
    {
        if (count == Limit) {
            std::fprintf(stderr, "viscript: at most %u --font options are supported\n", Limit);
            return false;
        }
        paths[count++] = path;
        return true;
    }
};

static bool _loadFonts(Renderer* renderer, const Fonts& fonts)
{
    for (auto i = 0u; i < fonts.count; i++) {
        if (renderer->font(fonts.paths[i]) == Result::Success) continue;
        auto reason = renderer->diagnostic();
        if (reason) std::fprintf(stderr, "viscript: %s\n", reason);
        else std::fprintf(stderr, "viscript: could not load the font file '%s'\n", fonts.paths[i]);
        return false;
    }
    return true;
}

#ifdef VIS_CLI_SOUNDTRACK
static constexpr uint32_t SOUNDTRACK_RATE = 48000u;
static constexpr uint32_t SOUNDTRACK_BLOCK = 4800u;

// The scene's audio cues mixed into a WAV file next to the output, removed once the export
// ends.
struct Soundtrack
{
    char* path = nullptr;

    ~Soundtrack()
    {
        if (!path) return;
        unlink(path);
        delete[] path;
    }
};

// The cue name under which the file given with --audio joins a scene's own cues.
static constexpr char NARRATION_ASSET[] = "viscript --audio";

// Reads the file at path and loads it into player under name; label names the file in
// messages.
static bool _loadSound(audio::Player* player, const char* name, const char* path, const char* label)
{
    auto file = std::fopen(path, "rb");
    long size = -1;
    if (file && std::fseek(file, 0, SEEK_END) == 0) size = std::ftell(file);
    if (!file || size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fprintf(stderr, "viscript: render failed: could not read %s at %s\n", label, path);
        if (file) std::fclose(file);
        return false;
    }
    if (!size || static_cast<unsigned long>(size) > audio::Player::AssetByteLimit) {
        std::fprintf(stderr, "viscript: render failed: %s must be between 1 byte and %u MiB\n", label,
                     audio::Player::AssetByteLimit / (1024u * 1024u));
        std::fclose(file);
        return false;
    }
    auto bytes = new (std::nothrow) uint8_t[size];
    auto read = bytes ? std::fread(bytes, 1, static_cast<size_t>(size), file) : 0u;
    std::fclose(file);
    auto status = read == static_cast<size_t>(size)
                ? player->load(name, bytes, static_cast<uint32_t>(size)) : Result::IoError;
    delete[] bytes;
    if (status == Result::Success) return true;
    if (status == Result::IoError) {
        std::fprintf(stderr, "viscript: render failed: could not read %s at %s\n", label, path);
    } else {
        std::fprintf(stderr, "viscript: render failed: could not load %s: together the audio files may hold "
                     "at most %u MiB\n", label, audio::Player::AssetTotalByteLimit / (1024u * 1024u));
    }
    return false;
}

// Loads the audio asset a cue names, from the script's folder unless the name is absolute.
static bool _loadAsset(audio::Player* player, const char* script, const char* name)
{
    auto slash = std::strrchr(script, '/');
    auto root = name[0] == '/' || !slash ? 0u : static_cast<size_t>(slash - script) + 1u;
    auto nameSize = std::strlen(name);
    auto path = new (std::nothrow) char[root + nameSize + 1u];
    char label[320];
    if (!path) {
        std::fprintf(stderr, "viscript: render failed: out of memory\n");
        return false;
    }
    std::memcpy(path, script, root);
    std::memcpy(path + root, name, nameSize + 1u);
    std::snprintf(label, sizeof(label), "the audio asset '%s'", name);
    auto loaded = _loadSound(player, name, path, label);
    delete[] path;
    return loaded;
}

static void _le16(uint8_t* output, uint32_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
}

static void _le32(uint8_t* output, uint32_t value)
{
    _le16(output, value);
    _le16(output + 2, value >> 16);
}

// Mixes the cues of soundscape, and the file given with --audio from time 0 on the music bus
// when narration names one, for as long as the exported video lasts into a 16-bit stereo WAV
// file beside output.
static bool _soundtrack(const Scene* scene, audio::Soundscape* soundscape, const char* script,
                        const char* output, uint32_t fps, const char* narration, Soundtrack& soundtrack)
{
    audio::PlayerConfig config;
    config.voiceLimit = 1u;
    config.device = false;
    auto player = audio::Player::gen(config);
    if (!player) {
        std::fprintf(stderr, "viscript: render failed: out of memory\n");
        return false;
    }
    if (narration) {
        audio::CueConfig cue;
        cue.asset = NARRATION_ASSET;
        cue.bus = audio::Bus::Music;
        audio::Cue handle;
        if (soundscape->cue(cue, handle) != Result::Success) {
            std::fprintf(stderr, "viscript: render failed: the scene has no room for the --audio file among its "
                         "%u cues\n", audio::Soundscape::CueLimit);
            delete player;
            return false;
        }
        if (!_loadSound(player, NARRATION_ASSET, narration, "the --audio file")) {
            delete player;
            return false;
        }
    }
    for (auto i = 0u; i < soundscape->count(); i++) {
        audio::CueConfig cue;
        soundscape->cueAt(i, cue);
        auto loaded = narration && std::strcmp(cue.asset, NARRATION_ASSET) == 0;
        for (auto j = 0u; j < i && !loaded; j++) {
            audio::CueConfig earlier;
            soundscape->cueAt(j, earlier);
            loaded = std::strcmp(earlier.asset, cue.asset) == 0;
        }
        if (!loaded && !_loadAsset(player, script, cue.asset)) {
            delete player;
            return false;
        }
    }
    uint32_t frames = 0u;
    if (player->bind(soundscape) != Result::Success || saver::timeline(scene, fps, frames) != Result::Success) {
        std::fprintf(stderr, "viscript: render failed: could not prepare the soundtrack\n");
        delete player;
        return false;
    }
    auto total = (static_cast<uint64_t>(frames) * SOUNDTRACK_RATE + fps - 1u) / fps;
    if (total * 4u > UINT32_MAX - 36u) {
        std::fprintf(stderr, "viscript: render failed: the soundtrack is too long for a WAV file\n");
        delete player;
        return false;
    }

    static constexpr char suffix[] = ".vis-audio-XXXXXX";
    auto size = std::strlen(output);
    soundtrack.path = new (std::nothrow) char[size + sizeof(suffix)];
    auto descriptor = -1;
    if (soundtrack.path) {
        std::memcpy(soundtrack.path, output, size);
        std::memcpy(soundtrack.path + size, suffix, sizeof(suffix));
        descriptor = mkstemp(soundtrack.path);
        if (descriptor < 0) {
            delete[] soundtrack.path;
            soundtrack.path = nullptr;
        }
    }
    auto file = descriptor < 0 ? nullptr : fdopen(descriptor, "wb");
    if (!file) {
        if (descriptor >= 0) close(descriptor);
        std::fprintf(stderr, "viscript: render failed: could not write the soundtrack beside %s\n", output);
        delete player;
        return false;
    }

    auto bytes = static_cast<uint32_t>(total * 4u);
    uint8_t header[44];
    std::memcpy(header, "RIFF", 4);
    _le32(header + 4, 36u + bytes);
    std::memcpy(header + 8, "WAVEfmt ", 8);
    _le32(header + 16, 16u);
    _le16(header + 20, 1u);
    _le16(header + 22, 2u);
    _le32(header + 24, SOUNDTRACK_RATE);
    _le32(header + 28, SOUNDTRACK_RATE * 4u);
    _le16(header + 32, 4u);
    _le16(header + 34, 16u);
    std::memcpy(header + 36, "data", 4);
    _le32(header + 40, bytes);
    auto ok = std::fwrite(header, 1, sizeof(header), file) == sizeof(header);
    static float samples[SOUNDTRACK_BLOCK * 2u];
    static uint8_t pcm[SOUNDTRACK_BLOCK * 4u];
    auto status = Result::Success;
    for (uint64_t first = 0u; ok && first < total; first += SOUNDTRACK_BLOCK) {
        auto count = static_cast<uint32_t>(total - first < SOUNDTRACK_BLOCK ? total - first : SOUNDTRACK_BLOCK);
        status = player->mix(SOUNDTRACK_RATE, first, samples, count);
        if (status != Result::Success) break;
        for (auto i = 0u; i < count * 2u; i++) {
            auto value = static_cast<int32_t>(std::lrint(samples[i] * 32767.0f));
            _le16(pcm + i * 2u, static_cast<uint16_t>(static_cast<int16_t>(value)));
        }
        ok = std::fwrite(pcm, 1, count * 4u, file) == count * 4u;
    }
    if (std::fclose(file) != 0) ok = false;
    delete player;
    if (status != Result::Success) {
        std::fprintf(stderr, "viscript: render failed: could not mix the audio cues: %s\n", _describe(status));
        return false;
    }
    if (!ok) {
        std::fprintf(stderr, "viscript: render failed: could not write the soundtrack beside %s\n", output);
        return false;
    }
    return true;
}
#endif

static void _inspectScene(const Scene* scene, const char* source)
{
    auto& config = scene->config();
    std::putchar('{');
    if (source) {
        std::fputs("\"source\":", stdout);
        _json(source);
        std::putchar(',');
    }
    std::printf("\"width\":%u,\"height\":%u,\"fps\":%u,\"duration\":%.6g,\"loop\":%s,\"camera\":{\"mode\":\"%s\",\"view\":\"%s\",\"projection\":\"%s\"},\"objects\":[",
                config.width, config.height, config.fps, scene->duration(), config.loop ? "true" : "false",
                config.cameraMode == CameraMode::Interactive ? "interactive" : "fixed",
                config.cameraView == CameraView::ThreeD ? "3d" : "2d",
                config.camera.projection == Projection::Perspective ? "perspective" : "orthographic");
    for (auto i = 0u; i < scene->count(); i++) {
        auto object = scene->objectAt(i);
        if (i) std::putchar(',');
        std::printf("{\"handle\":%u,\"id\":", object->id());
        if (object->tag()) _json(object->tag());
        else std::fputs("null", stdout);
        std::printf(",\"type\":");
        _json(type(object->type()));
        std::printf(",\"parent\":");
        if (object->parent()) std::printf("%u", object->parent()->id());
        else std::fputs("null", stdout);
        std::printf(",\"space\":");
        if (object->space()) std::printf("%u", object->space()->id());
        else std::fputs("null", stdout);
        std::putchar('}');
    }
    std::fputs("],\"markers\":[", stdout);
    for (auto i = 0u; i < scene->markerCount(); i++) {
        auto time = 0.0f;
        auto name = scene->markerAt(i, time);
        if (i) std::putchar(',');
        std::fputs("{\"name\":", stdout);
        _json(name);
        std::printf(",\"time\":%.6g}", time);
    }
    std::fputs("],\"viewports\":[", stdout);
    for (auto i = 0u; i < scene->viewportCount(); i++) {
        if (i) std::putchar(',');
        Viewport viewport;
        scene->viewportAt(i, viewport);
        std::printf("{\"x\":%.6g,\"y\":%.6g,\"width\":%.6g,\"height\":%.6g,\"scene\":",
                    viewport.x, viewport.y, viewport.width, viewport.height);
        _inspectScene(scene->sceneAt(i), nullptr);
        std::putchar('}');
    }
    std::fputs("]}", stdout);
}

static int _inspect(int argc, char** argv)
{
    auto path = argv[2];
    Fonts fonts;
    for (auto i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            if (!fonts.add(argv[++i])) return 2;
        } else {
            std::fprintf(stderr, "viscript: invalid option: %s\n", argv[i]);
            return 2;
        }
    }
    // Only a script that measures text needs fonts; they load before it runs.
    Renderer* renderer = nullptr;
    if (fonts.count) {
        renderer = Renderer::gen();
        if (!renderer) {
            std::fprintf(stderr, "viscript: no renderer is available to load fonts\n");
            return 3;
        }
        if (!_loadFonts(renderer, fonts)) {
            delete renderer;
            return 3;
        }
    }
    auto scene = _load(path);
    if (!scene) {
        delete renderer;
        return 2;
    }
    _inspectScene(scene, path);
    std::putchar('\n');
    delete scene;
    delete renderer;
    return 0;
}

static bool _ancestor(const Object* ancestor, const Object* object)
{
    while (object) {
        if (object == ancestor) return true;
        object = object->parent();
    }
    return false;
}

static bool _layoutScene(const Scene* scene, Renderer* renderer, float time, float padding)
{
    auto count = scene->count();
    auto boxes = new (std::nothrow) BBox[count];
    auto defined = new (std::nothrow) bool[count]();
    if ((!boxes || !defined) && count) {
        delete[] boxes;
        delete[] defined;
        return false;
    }
    auto& config = scene->config();
    std::printf("{\"width\":%u,\"height\":%u,\"objects\":[", config.width, config.height);
    for (auto i = 0u; i < count; i++) {
        auto object = scene->objectAt(i);
        if (i) std::putchar(',');
        std::printf("{\"handle\":%u,\"id\":", object->id());
        if (object->tag()) _json(object->tag());
        else std::fputs("null", stdout);
        std::fputs(",\"type\":", stdout);
        _json(type(object->type()));
        if (object->type() == Type::Text) {
            std::fputs(",\"text\":", stdout);
            auto text = static_cast<Text*>(object);
            auto length = scene->text(text, time, nullptr, 0);
            auto shown = new (std::nothrow) char[length + 1u];
            if (!shown) { delete[] boxes; delete[] defined; return false; }
            scene->text(text, time, shown, length + 1u);
            _json(shown);
            delete[] shown;
        }
        auto result = renderer->bounds(scene, object, time, boxes[i]);
        defined[i] = result == Result::Success;
        if (defined[i]) {
            std::printf(",\"bounds\":{\"x\":%.6g,\"y\":%.6g,\"width\":%.6g,\"height\":%.6g}}",
                        boxes[i].x, boxes[i].y, boxes[i].width, boxes[i].height);
        } else {
            std::fputs(",\"bounds\":null}", stdout);
        }
    }
    std::fputs("],\"intersections\":[", stdout);
    auto written = false;
    for (auto i = 0u; i < count; i++) {
        if (!defined[i]) continue;
        auto first = scene->objectAt(i);
        for (auto j = i + 1u; j < count; j++) {
            if (!defined[j]) continue;
            auto second = scene->objectAt(j);
            if (_ancestor(first, second) || _ancestor(second, first)
                || !boxes[i].intersects(boxes[j], padding)) {
                continue;
            }
            if (written) std::putchar(',');
            std::printf("{\"first\":%u,\"second\":%u}", first->id(), second->id());
            written = true;
        }
    }
    std::fputs("],\"viewports\":[", stdout);
    for (auto i = 0u; i < scene->viewportCount(); i++) {
        if (i) std::putchar(',');
        Viewport viewport;
        scene->viewportAt(i, viewport);
        std::printf("{\"x\":%.6g,\"y\":%.6g,\"width\":%.6g,\"height\":%.6g,\"scene\":",
                    viewport.x, viewport.y, viewport.width, viewport.height);
        if (!_layoutScene(scene->sceneAt(i), renderer, time, padding)) {
            delete[] boxes;
            delete[] defined;
            return false;
        }
        std::putchar('}');
    }
    std::fputs("],\"transitions\":[", stdout);
    for (auto i = 0u; i < scene->transitionCount(); i++) {
        if (i) std::putchar(',');
        if (!_layoutScene(scene->transitionAt(i), renderer,
                          scene->transitionAt(i)->duration(), padding)) {
            delete[] boxes;
            delete[] defined;
            return false;
        }
    }
    std::fputs("]}", stdout);
    delete[] boxes;
    delete[] defined;
    return true;
}

static void _bbox(const BBox& bounds)
{
    std::printf("{\"x\":%.6g,\"y\":%.6g,\"width\":%.6g,\"height\":%.6g}",
                bounds.x, bounds.y, bounds.width, bounds.height);
}

static const char* _layoutKind(LayoutSceneKind kind)
{
    switch (kind) {
        case LayoutSceneKind::Root: return "root";
        case LayoutSceneKind::Viewport: return "viewport";
        case LayoutSceneKind::Transition: return "transition";
        case LayoutSceneKind::View: return "view";
    }
    return "unknown";
}

static const char* _layoutVisualKind(LayoutVisualKind kind)
{
    switch (kind) {
        case LayoutVisualKind::Stable: return "stable";
        case LayoutVisualKind::Morph: return "morph";
        case LayoutVisualKind::FadeOut: return "fade-out";
        case LayoutVisualKind::FadeIn: return "fade-in";
    }
    return "unknown";
}

static void _layoutReport(const LayoutReport& report, const Scene* root)
{
    std::printf("{\"width\":%u,\"height\":%u,\"scenes\":[",
                root->config().width, root->config().height);
    for (auto i = 0u; i < report.sceneCount(); i++) {
        auto scene = report.sceneAt(i);
        if (i) std::putchar(',');
        std::printf("{\"index\":%u,\"path\":", i);
        _json(scene->path);
        std::fputs(",\"kind\":", stdout);
        _json(_layoutKind(scene->kind));
        std::fputs(",\"parent\":", stdout);
        if (scene->parent == UINT32_MAX) std::fputs("null", stdout);
        else std::printf("%u", scene->parent);
        std::printf(",\"depth\":%u,\"bounds\":", scene->depth);
        _bbox(scene->bounds);
        std::fputs(",\"clipBounds\":", stdout);
        _bbox(scene->clipBounds);
        std::printf(",\"scale\":{\"x\":%.6g,\"y\":%.6g},\"clipped\":%s,\"stretched\":%s}",
                    scene->scaleX, scene->scaleY, scene->clipped ? "true" : "false",
                    scene->stretched ? "true" : "false");
    }
    std::fputs("],\"objects\":[", stdout);
    for (auto i = 0u; i < report.objectCount(); i++) {
        auto item = report.objectAt(i);
        auto object = item->object;
        auto scene = report.sceneAt(item->scene);
        if (i) std::putchar(',');
        std::printf("{\"index\":%u,\"scene\":%u,\"scenePath\":", i, item->scene);
        _json(scene->path);
        std::printf(",\"handle\":%u,\"id\":", object->id());
        if (object->tag()) _json(object->tag());
        else std::fputs("null", stdout);
        std::fputs(",\"parent\":", stdout);
        if (item->parent == UINT32_MAX) std::fputs("null", stdout);
        else std::printf("%u", item->parent);
        std::fputs(",\"type\":", stdout);
        _json(type(object->type()));
        auto typeset = object->type() == Type::Group && static_cast<const Group*>(object)->typeset;
        std::printf(",\"typeset\":%s,\"layer\":%d,\"paintBounds\":", typeset ? "true" : "false", item->layer);
        if (item->paintBounds.width > 0.0f && item->paintBounds.height > 0.0f) {
            _bbox(item->paintBounds);
        } else {
            std::fputs("null", stdout);
        }
        std::fputs(",\"familyBounds\":", stdout);
        if (item->familyBounds.width > 0.0f && item->familyBounds.height > 0.0f) {
            _bbox(item->familyBounds);
        } else {
            std::fputs("null", stdout);
        }
        std::fputs(",\"visibleBounds\":", stdout);
        if (item->visible) _bbox(item->visibleBounds);
        else std::fputs("null", stdout);
        std::fputs(",\"visibleFamilyBounds\":", stdout);
        if (item->familyVisible) _bbox(item->visibleFamilyBounds);
        else std::fputs("null", stdout);
        std::fputs(",\"clipBounds\":", stdout);
        _bbox(item->clipBounds);
        auto painted = item->paintBounds.width > 0.0f && item->paintBounds.height > 0.0f;
        auto familyPainted = item->familyBounds.width > 0.0f && item->familyBounds.height > 0.0f;
        std::printf(",\"visible\":%s,\"familyVisible\":%s,\"clipped\":%s,\"familyClipped\":%s,\"outside\":%s,\"familyOutside\":%s}",
                    item->visible ? "true" : "false", item->familyVisible ? "true" : "false",
                    item->clipped ? "true" : "false", item->familyClipped ? "true" : "false",
                    painted && !item->visible ? "true" : "false",
                    familyPainted && !item->familyVisible ? "true" : "false");
    }
    std::fputs("],\"visuals\":[", stdout);
    for (auto i = 0u; i < report.visualCount(); i++) {
        auto visual = report.visualAt(i);
        if (i) std::putchar(',');
        std::printf("{\"index\":%u,\"object\":%u,\"counterpart\":", i, visual->object);
        if (visual->counterpart == UINT32_MAX) std::fputs("null", stdout);
        else std::printf("%u", visual->counterpart);
        std::fputs(",\"occluder\":", stdout);
        if (visual->occluder == UINT32_MAX) std::fputs("null", stdout);
        else std::printf("%u", visual->occluder);
        std::fputs(",\"kind\":", stdout);
        _json(_layoutVisualKind(visual->kind));
        std::printf(",\"layer\":%d,\"paintBounds\":", visual->layer);
        auto painted = visual->paintBounds.width > 0.0f && visual->paintBounds.height > 0.0f;
        if (painted) _bbox(visual->paintBounds);
        else std::fputs("null", stdout);
        std::fputs(",\"visibleBounds\":", stdout);
        if (visual->visible) _bbox(visual->visibleBounds);
        else std::fputs("null", stdout);
        std::fputs(",\"clipBounds\":", stdout);
        _bbox(visual->clipBounds);
        std::printf(",\"opacity\":%.6g,\"visible\":%s,\"clipped\":%s,\"occluded\":%s}",
                    visual->opacity, visual->visible ? "true" : "false",
                    visual->clipped ? "true" : "false",
                    visual->occluded ? "true" : "false");
    }
    std::fputs("],\"paths\":[", stdout);
    for (auto i = 0u; i < report.pathCount(); i++) {
        auto path = report.pathAt(i);
        if (i) std::putchar(',');
        std::printf("{\"index\":%u,\"visual\":%u,\"paintBounds\":", i, path->visual);
        _bbox(path->paintBounds);
        std::printf(",\"strokeWidth\":%.6g,\"closed\":%s,\"filled\":%s,"
                    "\"stroked\":%s,\"points\":[",
                    path->strokeWidth, path->closed ? "true" : "false",
                    path->filled ? "true" : "false", path->stroked ? "true" : "false");
        for (auto j = 0u; j < path->count; j++) {
            if (j) std::putchar(',');
            std::printf("{\"x\":%.6g,\"y\":%.6g}", path->points[j].x,
                        path->points[j].y);
        }
        std::putchar(']');
        std::putchar('}');
    }
    std::fputs("],\"collisions\":[", stdout);
    for (auto i = 0u; i < report.collisionCount(); i++) {
        auto collision = report.collisionAt(i);
        if (i) std::putchar(',');
        std::printf("{\"first\":%u,\"second\":%u,\"kind\":\"%s\",\"overlap\":",
                    collision->first, collision->second,
                    collision->overlapping ? "overlap" : "insufficient-gap");
        if (collision->overlapping) _bbox(collision->overlap);
        else std::fputs("null", stdout);
        std::printf(",\"clearance\":{\"x\":%.6g,\"y\":%.6g},\"separation\":{\"x\":%.6g,\"y\":%.6g}}",
                    collision->clearance.x, collision->clearance.y,
                    collision->separation.x, collision->separation.y);
    }
    std::fputs("],\"containments\":[", stdout);
    for (auto i = 0u; i < report.containmentCount(); i++) {
        auto containment = report.containmentAt(i);
        if (i) std::putchar(',');
        std::printf("{\"container\":%u,\"content\":%u,\"depth\":%u,\"inset\":%.6g,\"contained\":%s,\"overflow\":{\"left\":%.6g,\"top\":%.6g,\"right\":%.6g,\"bottom\":%.6g}}",
                    containment->container, containment->content, containment->depth,
                    containment->inset, containment->contained ? "true" : "false",
                    containment->overflowLeft, containment->overflowTop,
                    containment->overflowRight, containment->overflowBottom);
    }
    std::fputs("]}", stdout);
}

static int _layout(int argc, char** argv)
{
    if (argc < 3) {
        _usage();
        return 2;
    }
    auto source = argv[2];
    Fonts fonts;
    auto time = -1.0f;
    auto padding = 0.0f;
    for (auto i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            if (!fonts.add(argv[++i])) return 2;
        } else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc && _float(argv[i + 1], time)) {
            i++;
        } else if (std::strcmp(argv[i], "--padding") == 0 && i + 1 < argc && _float(argv[i + 1], padding)) {
            i++;
        } else {
            std::fprintf(stderr, "viscript: invalid option: %s\n", argv[i]);
            return 2;
        }
    }
    auto renderer = Renderer::gen(RenderEngine::Cpu);
    if (!renderer) {
        std::fprintf(stderr, "viscript: SW renderer is unavailable\n");
        return 3;
    }
    if (!_loadFonts(renderer, fonts)) {
        delete renderer;
        return 3;
    }
    auto scene = _load(source);
    if (!scene) {
        delete renderer;
        return 2;
    }
    if (time < 0.0f) time = scene->duration();
    LayoutReport report;
    auto reportResult = renderer->layout(scene, time, report, padding);
    if (reportResult != Result::Success) {
        _failed("layout report", reportResult, renderer);
        delete renderer;
        delete scene;
        return 4;
    }
    std::fputs("{\"source\":", stdout);
    _json(source);
    std::printf(",\"time\":%.6g,\"padding\":%.6g,\"scene\":", time, padding);
    auto success = _layoutScene(scene, renderer, time, padding);
    if (success) {
        std::fputs(",\"report\":", stdout);
        _layoutReport(report, scene);
        std::fputs("}\n", stdout);
    }
    else std::fprintf(stderr, "viscript: layout inspection failed: out of memory\n");
    delete renderer;
    delete scene;
    return success ? 0 : 4;
}

// Reports a failed GIF or MP4 export; both pipe the rendered frames into ffmpeg.
static void _saveFailed(const char* output, Result status, const Renderer* renderer,
                        const Config& config)
{
    if (renderer->diagnostic()) {
        _failed("render", status, renderer);
    } else if (status == Result::InvalidArguments && _ends(output, ".mp4")
               && ((config.width | config.height) & 1u)) {
        std::fprintf(stderr, "viscript: render failed: mp4 needs an even width and height, got %ux%u\n",
                     config.width, config.height);
    } else if (status == Result::IoError) {
        std::fprintf(stderr, "viscript: render failed: ffmpeg could not write %s; check that ffmpeg is "
                     "installed and the folder is writable\n", output);
    } else if (status == Result::NonSupport) {
        std::fprintf(stderr, "viscript: render failed: this build cannot write .gif or .mp4 files\n");
    } else {
        _failed("render", status, renderer);
    }
}

static int _render(int argc, char** argv)
{
    if (argc < 3) {
        _usage();
        return 2;
    }
    auto source = argv[2];
    auto output = "render.png";
    Fonts fonts;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t fps = 0;
    float time = -1.0f;
    const char* narration = nullptr;
    for (auto i = 3; i < argc; i++) {
        if ((std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            output = argv[++i];
        } else if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            if (!fonts.add(argv[++i])) return 2;
        } else if (std::strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
            if (narration) {
                std::fprintf(stderr, "viscript: --audio may be given only once\n");
                return 2;
            }
            narration = argv[++i];
        } else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc && _uint(argv[i + 1], width)) {
            i++;
        } else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc && _uint(argv[i + 1], height)) {
            i++;
        } else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc && _uint(argv[i + 1], fps)) {
            i++;
        } else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc && _float(argv[i + 1], time)) {
            i++;
        } else {
            std::fprintf(stderr, "viscript: invalid option: %s\n", argv[i]);
            return 2;
        }
    }
    if (narration) {
        if (!_ends(output, ".mp4")) {
            std::fprintf(stderr, "viscript: --audio needs an .mp4 output; %s cannot carry audio\n", output);
            return 2;
        }
        auto file = std::fopen(narration, "rb");
        if (!file) {
            std::fprintf(stderr, "viscript: could not read the --audio file %s\n", narration);
            return 3;
        }
        std::fclose(file);
    }

    // Fonts load before the script runs, which may measure text in them.
    auto renderer = Renderer::gen(RenderEngine::Cpu);
    if (!renderer) {
        std::fprintf(stderr, "viscript: SW renderer is unavailable\n");
        return 3;
    }
    if (!_loadFonts(renderer, fonts)) {
        delete renderer;
        return 3;
    }
    detail::LuaHostContext context;
    auto scene = _load(source, context);
    if (!scene) {
        delete renderer;
        return 2;
    }
    auto& config = scene->config();
    if (width) config.width = width;
    if (height) config.height = height;
    if (fps) config.fps = fps;

    Result status;
    if (_ends(output, ".mp4") || _ends(output, ".gif")) {
        // ffmpeg reads the --audio file as it is unless it joins the scene's cues in a mix.
        const char* soundtrackPath = narration;
#ifdef VIS_CLI_SOUNDTRACK
        // An MP4 carries the audio cues the scene declares; a GIF has no audio.
        Soundtrack soundtrack;
        auto soundscape = context.audio.soundscape;
        if (_ends(output, ".mp4") && soundscape && soundscape->count()) {
            if (!_soundtrack(scene, soundscape, source, output, fps, narration, soundtrack)) {
                delete renderer;
                delete scene;
                return 3;
            }
            soundtrackPath = soundtrack.path;
        }
#endif
        status = Saver::save(scene, *renderer, output, fps, soundtrackPath);
        if (status != Result::Success) _saveFailed(output, status, renderer, config);
    } else if (_ends(output, ".png")) {
        Surface surface;
        if (time < 0.0f) time = scene->duration();
        status = renderer->render(scene, time, surface);
        if (status != Result::Success) {
            _failed("render", status, renderer);
        } else {
            status = Saver::png(surface, output);
            if (status != Result::Success) std::fprintf(stderr, "viscript: render failed: could not write %s\n", output);
        }
    } else {
        std::fprintf(stderr, "viscript: render failed: cannot write %s; the output must end in .png, .gif or .mp4\n",
                     output);
        status = Result::NonSupport;
    }
    if (status == Result::Success) std::printf("%s\n", output);
    delete renderer;
    delete scene;
    return status == Result::Success ? 0 : 4;
}

static uint64_t _objectCount(const Scene* scene)
{
    if (!scene) return 0;
    auto count = static_cast<uint64_t>(scene->count());
    for (auto i = 0u; i < scene->viewportCount(); i++)
        count += _objectCount(scene->sceneAt(i));
    for (auto i = 0u; i < scene->transitionCount(); i++)
        count += _objectCount(scene->transitionAt(i));
    return count;
}

static int _benchmark(int argc, char** argv)
{
    if (argc < 3) {
        _usage();
        return 2;
    }
    auto source = argv[2];
    Fonts fonts;
    auto frames = 120u;
    auto warmup = 5u;
    for (auto i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc && _uint(argv[i + 1], frames)) {
            i++;
        } else if (std::strcmp(argv[i], "--warmup") == 0 && i + 1 < argc && _uint(argv[i + 1], warmup, true)) {
            i++;
        } else if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            if (!fonts.add(argv[++i])) return 2;
        } else {
            std::fprintf(stderr, "viscript: invalid option: %s\n", argv[i]);
            return 2;
        }
    }

    auto renderer = Renderer::gen(RenderEngine::Cpu);
    if (!renderer) {
        std::fprintf(stderr, "viscript: SW renderer is unavailable\n");
        return 3;
    }
    if (!_loadFonts(renderer, fonts)) {
        delete renderer;
        return 3;
    }
    auto scene = _load(source);
    if (!scene) {
        delete renderer;
        return 2;
    }

    Surface surface;
    auto duration = scene->duration();
    auto render = [&](uint32_t frame, uint32_t count) {
        auto time = count > 1u
                  ? static_cast<float>(static_cast<double>(duration) * frame / (count - 1u))
                  : duration;
        return renderer->render(scene, time, surface);
    };
    for (auto i = 0u; i < warmup; i++) {
        auto status = render(i % frames, frames);
        if (status != Result::Success) {
            _failed("benchmark warmup", status, renderer);
            delete renderer;
            delete scene;
            return 4;
        }
    }

    auto begin = std::chrono::steady_clock::now();
    for (auto i = 0u; i < frames; i++) {
        auto status = render(i, frames);
        if (status != Result::Success) {
            _failed("benchmark render", status, renderer);
            delete renderer;
            delete scene;
            return 4;
        }
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(end - begin).count();
    auto frameTime = elapsed / frames;
    std::printf("{\"source\":");
    _json(source);
    std::printf(",\"objects\":%llu,\"frames\":%u,\"warmup\":%u,\"elapsed_ms\":%.6f,\"ms_per_frame\":%.6f,\"fps\":%.6f}\n",
                static_cast<unsigned long long>(_objectCount(scene)), frames, warmup, elapsed,
                frameTime, 1000.0 / frameTime);
    delete renderer;
    delete scene;
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        _usage();
        return 2;
    }
    if (std::strcmp(argv[1], "render") == 0) return _render(argc, argv);
    if (std::strcmp(argv[1], "benchmark") == 0) return _benchmark(argc, argv);
    if (std::strcmp(argv[1], "layout") == 0) return _layout(argc, argv);
#ifdef VIS_AUDIT
    if (std::strcmp(argv[1], "audit") == 0) return visAudit(argc, argv);
#endif
    if (std::strcmp(argv[1], "inspect") == 0 && argc >= 3) return _inspect(argc, argv);
    if (std::strcmp(argv[1], "capabilities") == 0 && argc == 2) return _capabilities();
    _usage();
    return 2;
}
