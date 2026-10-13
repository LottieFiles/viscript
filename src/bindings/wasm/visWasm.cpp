#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define VIS_KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define VIS_KEEPALIVE
#endif

#include "vis.h"
#include "vis_capi.h"
#include "visLuaHost.h"
#include "visSaver.h"
#if defined(VIS_AUDIO)
#include "vis_audio.h"

static_assert(static_cast<uint32_t>(vis::audio::Bus::Music) == 0u);
static_assert(static_cast<uint32_t>(vis::audio::Bus::Effect) == 1u);
static_assert(static_cast<uint32_t>(vis::audio::Bus::Ui) == 2u);
#endif
#if defined(VIS_UI)
#include "vis_ui.h"

static_assert(static_cast<uint32_t>(vis::ui::InputKind::PointerDown) == 0u);
static_assert(static_cast<uint32_t>(vis::ui::InputKind::PointerMove) == 1u);
static_assert(static_cast<uint32_t>(vis::ui::InputKind::PointerUp) == 2u);
static_assert(static_cast<uint32_t>(vis::ui::InputKind::PointerCancel) == 3u);
static_assert(static_cast<uint32_t>(vis::ui::InputKind::Wheel) == 4u);
static_assert(static_cast<uint32_t>(vis::ui::InputKind::KeyDown) == 5u);
#endif
#if defined(VIS_INPUT)
#include "vis_input.h"

static_assert(static_cast<uint32_t>(vis::input::InputKind::PointerDown) == 0u);
static_assert(static_cast<uint32_t>(vis::input::InputKind::KeyDown) == 5u);
static_assert(static_cast<uint32_t>(vis::input::InputKind::KeyUp) == 6u);
static_assert(static_cast<uint32_t>(vis::input::Key::ArrowLeft) == 1u);
static_assert(static_cast<uint32_t>(vis::input::Key::Space) == 5u);
static_assert(static_cast<uint32_t>(vis::input::Key::A) == 8u);
static_assert(static_cast<uint32_t>(vis::input::Key::Z) == 33u);
static_assert(static_cast<uint32_t>(vis::input::Key::Number0) == 34u);
static_assert(static_cast<uint32_t>(vis::input::Key::Number9) == 43u);
static_assert(static_cast<uint32_t>(vis::input::Key::Plus) == 44u);
static_assert(static_cast<uint32_t>(vis::input::Key::Minus) == 45u);
static_assert(static_cast<uint32_t>(vis::input::Key::Shift) == 46u);
static_assert(static_cast<uint32_t>(vis::input::Key::Tab) == 47u);
#endif
#if defined(VIS_LUA_RUNTIME)
#include "vis_lua_runtime.h"

static_assert(static_cast<uint32_t>(vis::lua_runtime::SoundBus::Music) == 0u);
static_assert(static_cast<uint32_t>(vis::lua_runtime::SoundBus::Effect) == 1u);
static_assert(static_cast<uint32_t>(vis::lua_runtime::SoundBus::Ui) == 2u);
#endif

struct Engine
{
    struct AssetSlot
    {
        char name[257] = {};
        vis::Asset* asset = nullptr;
    };

    vis::Scene* scene = nullptr;
    vis::Renderer* renderer = nullptr;
    vis::Surface surface;
    char error[1024] = {};
    float renderTime = 0.0f;
    bool rendered = false;
    uint8_t* layoutReport = nullptr;
    uint32_t layoutReportSize = 0;
    AssetSlot assets[64];
    uint32_t assetCount = 0;
    size_t assetBytes = 0;
    vis::Theme hostTheme;
    bool hasHostTheme = false;
    bool adaptiveTheme = false;
#if defined(VIS_AUDIO)
    struct AudioVoiceSlot
    {
        vis::audio::Voice voice;
        uint32_t serial = 0u;
        bool mapped = false;
    };

    vis::audio::Player* audioPlayer = nullptr;
    vis::audio::Soundscape* soundscape = nullptr;
    AudioVoiceSlot audioVoices[64];
#endif
#if defined(VIS_UI)
    vis::ui::Panel* panel = nullptr;
    vis::PixelSample uiSample;
    vis::Vec2 uiSampleValue;
    uint32_t renderPixelRatio = 1u;
    bool uiSampled = false;
#endif
#if defined(VIS_INPUT)
    vis::input::Controller* input = nullptr;
    vis::input::DigitalState inputKey;
    vis::input::PointerState inputPointer;
#endif
#if defined(VIS_LUA_RUNTIME)
    vis::lua_runtime::Runtime* luaRuntime = nullptr;
#endif
};

static constexpr uint32_t VIS_PIXEL_LIMIT = 67108864;
static constexpr uint32_t VIS_FONT_LIMIT = 32u * 1024u * 1024u;
static constexpr size_t VIS_ASSET_MEMORY_LIMIT = 64u * 1024u * 1024u;
static constexpr uint32_t VIS_ASSET_SOURCE_LIMIT = 32u * 1024u * 1024u;
#if defined(VIS_AUDIO)
static constexpr uint32_t VIS_AUDIO_VOICE_LIMIT = 64u;
static constexpr uint32_t VIS_AUDIO_VOICE_SLOT_BITS = 6u;
static constexpr uint32_t VIS_AUDIO_VOICE_SLOT_MASK = VIS_AUDIO_VOICE_LIMIT - 1u;
static constexpr uint32_t VIS_AUDIO_VOICE_SERIAL_MASK =
    UINT32_MAX >> VIS_AUDIO_VOICE_SLOT_BITS;
static_assert(VIS_AUDIO_VOICE_LIMIT == (1u << VIS_AUDIO_VOICE_SLOT_BITS));
#endif

static Engine* _engine(VisEngine engine)
{
    return static_cast<Engine*>(engine);
}

#if defined(VIS_AUDIO)
static vis::Result _audioPlayer(Engine* engine, vis::audio::Player*& output)
{
    if (engine->audioPlayer) {
        output = engine->audioPlayer;
        return vis::Result::Success;
    }
    auto config = vis::audio::PlayerConfig{};
    config.voiceLimit = VIS_AUDIO_VOICE_LIMIT;
    auto player = vis::audio::Player::gen(config);
    if (!player) return vis::Result::OutOfMemory;
    auto result = player->bind(engine->soundscape);
    if (result != vis::Result::Success) {
        delete player;
        return result;
    }
    engine->audioPlayer = player;
    output = player;
    return vis::Result::Success;
}

static uint32_t _audioVoice(Engine::AudioVoiceSlot& slot, uint32_t index)
{
    slot.serial++;
    slot.mapped = true;
    return (slot.serial << VIS_AUDIO_VOICE_SLOT_BITS) | index;
}

static vis::Result _audioVoice(Engine* engine, uint32_t handle,
                                Engine::AudioVoiceSlot*& output)
{
    auto serial = handle >> VIS_AUDIO_VOICE_SLOT_BITS;
    if (!serial) return vis::Result::InvalidArguments;
    auto index = handle & VIS_AUDIO_VOICE_SLOT_MASK;
    auto& slot = engine->audioVoices[index];
    if (!slot.mapped || slot.serial != serial) {
        return vis::Result::InsufficientCondition;
    }
    output = &slot;
    return vis::Result::Success;
}

static Engine::AudioVoiceSlot* _audioVoiceSlot(Engine* engine,
                                               vis::audio::Player* player,
                                               uint32_t& output)
{
    for (auto i = 0u; i < VIS_AUDIO_VOICE_LIMIT; i++) {
        auto& slot = engine->audioVoices[i];
        if (slot.mapped && player->active(slot.voice)) continue;
        slot.mapped = false;
        if (slot.serial == VIS_AUDIO_VOICE_SERIAL_MASK) continue;
        output = i;
        return &slot;
    }
    return nullptr;
}

static bool _audioBus(uint32_t value, vis::audio::Bus& output)
{
    if (value > static_cast<uint32_t>(vis::audio::Bus::Ui)) return false;
    output = static_cast<vis::audio::Bus>(value);
    return true;
}
#endif

static bool _renderEngine(VisRenderEngine value, vis::RenderEngine& output)
{
    switch (value) {
        case VIS_RENDER_ENGINE_CPU: output = vis::RenderEngine::Cpu; return true;
        case VIS_RENDER_ENGINE_GL: output = vis::RenderEngine::Gl; return true;
    }
    return false;
}

#if defined(VIS_UI)
static vis::Result _sample(const vis::Scene* scene, float time,
                             const vis::Vec2& position,
                             const vis::Object* const* candidates, uint32_t count,
                             vis::PixelSample& output, void* data) noexcept
{
    auto engine = static_cast<Engine*>(data);
    if (!engine || !engine->renderer || engine->scene != scene) {
        return vis::Result::InvalidArguments;
    }
    return engine->renderer->sample(scene, time, position, candidates, count, output,
                                    engine->renderPixelRatio);
}
#endif

static void _clearLayoutReport(Engine* engine)
{
    delete[] engine->layoutReport;
    engine->layoutReport = nullptr;
    engine->layoutReportSize = 0;
}

struct JsonBuffer
{
    ~JsonBuffer()
    {
        delete[] data;
    }

    bool reserve(uint32_t additional)
    {
        if (additional > UINT32_MAX - size - 1u) return false;
        auto required = size + additional + 1u;
        if (required <= capacity) return true;
        auto next = capacity ? capacity * 2u : 4096u;
        if (next < capacity || next < required) next = required;
        auto grown = new (std::nothrow) uint8_t[next];
        if (!grown) return false;
        if (size) std::memcpy(grown, data, size);
        delete[] data;
        data = grown;
        capacity = next;
        return true;
    }

    bool append(const char* value)
    {
        auto length = std::strlen(value);
        if (length > UINT32_MAX || !reserve(static_cast<uint32_t>(length))) return false;
        std::memcpy(data + size, value, length);
        size += static_cast<uint32_t>(length);
        data[size] = 0;
        return true;
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

    bool string(const char* value)
    {
        if (!append("\"")) return false;
        if (value) {
            for (auto cursor = reinterpret_cast<const uint8_t*>(value); *cursor; cursor++) {
                char escaped[7];
                switch (*cursor) {
                    case '\"': if (!append("\\\"")) return false; break;
                    case '\\': if (!append("\\\\")) return false; break;
                    case '\n': if (!append("\\n")) return false; break;
                    case '\r': if (!append("\\r")) return false; break;
                    case '\t': if (!append("\\t")) return false; break;
                    default:
                        if (*cursor < 0x20u) {
                            std::snprintf(escaped, sizeof(escaped), "\\u%04x", *cursor);
                            if (!append(escaped)) return false;
                        } else {
                            if (!reserve(1u)) return false;
                            data[size++] = *cursor;
                            data[size] = 0;
                        }
                        break;
                }
            }
        }
        return append("\"");
    }

    uint8_t* release()
    {
        auto output = data;
        data = nullptr;
        size = 0;
        capacity = 0;
        return output;
    }

    uint8_t* data = nullptr;
    uint32_t size = 0;
    uint32_t capacity = 0;
};

static bool _json(JsonBuffer& json, const vis::BBox& bounds)
{
    return json.format("{\"x\":%.9g,\"y\":%.9g,\"width\":%.9g,\"height\":%.9g}",
                       bounds.x, bounds.y, bounds.width, bounds.height);
}

static const char* _layoutKind(vis::LayoutSceneKind kind)
{
    switch (kind) {
        case vis::LayoutSceneKind::Root: return "root";
        case vis::LayoutSceneKind::Viewport: return "viewport";
        case vis::LayoutSceneKind::Transition: return "transition";
        case vis::LayoutSceneKind::View: return "view";
    }
    return "unknown";
}

static const char* _layoutVisualKind(vis::LayoutVisualKind kind)
{
    switch (kind) {
        case vis::LayoutVisualKind::Stable: return "stable";
        case vis::LayoutVisualKind::Morph: return "morph";
        case vis::LayoutVisualKind::FadeOut: return "fade-out";
        case vis::LayoutVisualKind::FadeIn: return "fade-in";
    }
    return "unknown";
}

static bool _serialize(const vis::LayoutReport& report, const vis::Scene* root,
                       JsonBuffer& json)
{
    if (!json.format("{\"time\":%.9g,\"padding\":%.9g,\"width\":%u,\"height\":%u,\"scenes\":[",
                     report.time(), report.padding(), root->config().width, root->config().height)) {
        return false;
    }
    for (auto i = 0u; i < report.sceneCount(); i++) {
        auto scene = report.sceneAt(i);
        if ((i && !json.append(",")) || !json.format("{\"index\":%u,\"path\":", i)
            || !json.string(scene->path) || !json.append(",\"kind\":")
            || !json.string(_layoutKind(scene->kind))) {
            return false;
        }
        if (scene->parent == UINT32_MAX) {
            if (!json.append(",\"parent\":null")) return false;
        } else if (!json.format(",\"parent\":%u", scene->parent)) {
            return false;
        }
        if (!json.format(",\"depth\":%u,\"bounds\":", scene->depth)
            || !_json(json, scene->bounds) || !json.append(",\"clipBounds\":")
            || !_json(json, scene->clipBounds)
            || !json.format(",\"scale\":{\"x\":%.9g,\"y\":%.9g},\"clipped\":%s,\"stretched\":%s}",
                            scene->scaleX, scene->scaleY, scene->clipped ? "true" : "false",
                            scene->stretched ? "true" : "false")) {
            return false;
        }
    }
    if (!json.append("],\"objects\":[")) return false;
    for (auto i = 0u; i < report.objectCount(); i++) {
        auto item = report.objectAt(i);
        auto object = item->object;
        auto scene = report.sceneAt(item->scene);
        if ((i && !json.append(","))
            || !json.format("{\"index\":%u,\"scene\":%u,\"scenePath\":", i, item->scene)
            || !json.string(scene->path)
            || !json.format(",\"handle\":%u,\"id\":", object->id())) {
            return false;
        }
        if (object->tag()) {
            if (!json.string(object->tag())) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (item->parent == UINT32_MAX) {
            if (!json.append(",\"parent\":null")) return false;
        } else if (!json.format(",\"parent\":%u", item->parent)) {
            return false;
        }
        if (!json.append(",\"type\":") || !json.string(vis::type(object->type()))
            || !json.format(",\"layer\":%d,\"paintBounds\":", item->layer)) {
            return false;
        }
        auto painted = item->paintBounds.width > 0.0f && item->paintBounds.height > 0.0f;
        if (painted) {
            if (!_json(json, item->paintBounds)) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (!json.append(",\"familyBounds\":")) return false;
        auto familyPainted = item->familyBounds.width > 0.0f
                          && item->familyBounds.height > 0.0f;
        if (familyPainted) {
            if (!_json(json, item->familyBounds)) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (!json.append(",\"visibleBounds\":")) return false;
        if (item->visible) {
            if (!_json(json, item->visibleBounds)) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (!json.append(",\"visibleFamilyBounds\":")) return false;
        if (item->familyVisible) {
            if (!_json(json, item->visibleFamilyBounds)) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (!json.append(",\"clipBounds\":") || !_json(json, item->clipBounds)
            || !json.format(",\"visible\":%s,\"familyVisible\":%s,\"clipped\":%s,\"familyClipped\":%s,\"outside\":%s,\"familyOutside\":%s}",
                            item->visible ? "true" : "false",
                            item->familyVisible ? "true" : "false",
                            item->clipped ? "true" : "false",
                            item->familyClipped ? "true" : "false",
                            painted && !item->visible ? "true" : "false",
                            familyPainted && !item->familyVisible ? "true" : "false")) {
            return false;
        }
    }
    if (!json.append("],\"visuals\":[")) return false;
    for (auto i = 0u; i < report.visualCount(); i++) {
        auto visual = report.visualAt(i);
        if ((i && !json.append(","))
            || !json.format("{\"index\":%u,\"object\":%u,\"counterpart\":", i,
                            visual->object)) {
            return false;
        }
        if (visual->counterpart == UINT32_MAX) {
            if (!json.append("null")) return false;
        } else if (!json.format("%u", visual->counterpart)) {
            return false;
        }
        if (!json.append(",\"occluder\":")) return false;
        if (visual->occluder == UINT32_MAX) {
            if (!json.append("null")) return false;
        } else if (!json.format("%u", visual->occluder)) {
            return false;
        }
        if (!json.append(",\"kind\":") || !json.string(_layoutVisualKind(visual->kind))
            || !json.format(",\"layer\":%d,\"paintBounds\":", visual->layer)) {
            return false;
        }
        auto painted = visual->paintBounds.width > 0.0f && visual->paintBounds.height > 0.0f;
        if (painted) {
            if (!_json(json, visual->paintBounds)) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (!json.append(",\"visibleBounds\":")) return false;
        if (visual->visible) {
            if (!_json(json, visual->visibleBounds)) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (!json.append(",\"clipBounds\":") || !_json(json, visual->clipBounds)
            || !json.format(",\"opacity\":%.9g,\"visible\":%s,\"clipped\":%s,\"occluded\":%s}",
                            visual->opacity, visual->visible ? "true" : "false",
                            visual->clipped ? "true" : "false",
                            visual->occluded ? "true" : "false")) {
            return false;
        }
    }
    if (!json.append("],\"paths\":[")) return false;
    for (auto i = 0u; i < report.pathCount(); i++) {
        auto path = report.pathAt(i);
        if ((i && !json.append(","))
            || !json.format("{\"index\":%u,\"visual\":%u,\"paintBounds\":", i,
                            path->visual)
            || !_json(json, path->paintBounds)
            || !json.format(",\"strokeWidth\":%.9g,\"closed\":%s,\"filled\":%s,"
                            "\"stroked\":%s,\"points\":[",
                            path->strokeWidth, path->closed ? "true" : "false",
                            path->filled ? "true" : "false",
                            path->stroked ? "true" : "false")) {
            return false;
        }
        for (auto j = 0u; j < path->count; j++) {
            if ((j && !json.append(","))
                || !json.format("{\"x\":%.9g,\"y\":%.9g}", path->points[j].x,
                                path->points[j].y)) {
                return false;
            }
        }
        if (!json.append("]}")) return false;
    }
    if (!json.append("],\"collisions\":[")) return false;
    for (auto i = 0u; i < report.collisionCount(); i++) {
        auto collision = report.collisionAt(i);
        if ((i && !json.append(","))
            || !json.format("{\"first\":%u,\"second\":%u,\"kind\":\"%s\",\"overlap\":",
                            collision->first, collision->second,
                            collision->overlapping ? "overlap" : "insufficient-gap")) {
            return false;
        }
        if (collision->overlapping) {
            if (!_json(json, collision->overlap)) return false;
        } else if (!json.append("null")) {
            return false;
        }
        if (!json.format(",\"clearance\":{\"x\":%.9g,\"y\":%.9g},\"separation\":{\"x\":%.9g,\"y\":%.9g}}",
                         collision->clearance.x, collision->clearance.y,
                         collision->separation.x, collision->separation.y)) {
            return false;
        }
    }
    if (!json.append("],\"containments\":[")) return false;
    for (auto i = 0u; i < report.containmentCount(); i++) {
        auto containment = report.containmentAt(i);
        if ((i && !json.append(","))
            || !json.format("{\"container\":%u,\"content\":%u,\"depth\":%u,\"inset\":%.9g,\"contained\":%s,\"overflow\":{\"left\":%.9g,\"top\":%.9g,\"right\":%.9g,\"bottom\":%.9g}}",
                            containment->container, containment->content, containment->depth,
                            containment->inset, containment->contained ? "true" : "false",
                            containment->overflowLeft, containment->overflowTop,
                            containment->overflowRight, containment->overflowBottom)) {
            return false;
        }
    }
    return json.append("]}");
}

static VisResult _error(Engine* engine, vis::Result result)
{
    if (engine) std::snprintf(engine->error, sizeof(engine->error), "%s", vis::result(result));
    return static_cast<VisResult>(result);
}

// Like _error after a renderer call, but keeps the renderer's reason when it gave one, such
// as the font a text needs.
static VisResult _rendererError(Engine* engine, vis::Result result)
{
    auto reason = engine->renderer ? engine->renderer->diagnostic() : nullptr;
    if (!reason) return _error(engine, result);
    std::snprintf(engine->error, sizeof(engine->error), "%s", reason);
    return static_cast<VisResult>(result);
}

#if defined(VIS_AUDIO)
static VisResult _audioResult(Engine* engine, vis::Result result)
{
    if (result != vis::Result::Success) return _error(engine, result);
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}
#endif

// Retained bytes: the encoded source for decoded-on-render assets, RGBA pixels for raw-pixel assets.
// The lazy RGBA cache that Cell textures request is not counted, which keeps the accounting stable.
static size_t _assetBytes(const vis::Asset* asset)
{
    if (!asset->encoded()) return _assetBytes(asset);
    return asset->encodedSize();
}

static const vis::Asset* _asset(const char* name, void* data)
{
    auto engine = static_cast<Engine*>(data);
    for (auto i = 0u; i < engine->assetCount; i++) {
        if (std::strcmp(engine->assets[i].name, name) == 0) return engine->assets[i].asset;
    }
    return nullptr;
}

static vis::Color _rgba(uint32_t value)
{
    return {
        static_cast<uint8_t>(value >> 24u),
        static_cast<uint8_t>(value >> 16u),
        static_cast<uint8_t>(value >> 8u),
        static_cast<uint8_t>(value),
    };
}

static_assert(static_cast<VisResult>(vis::Result::Success) == VIS_RESULT_SUCCESS);
static_assert(static_cast<VisResult>(vis::Result::InvalidArguments) == VIS_RESULT_INVALID_ARGUMENTS);
static_assert(static_cast<VisResult>(vis::Result::InsufficientCondition) == VIS_RESULT_INSUFFICIENT_CONDITION);
static_assert(static_cast<VisResult>(vis::Result::NonSupport) == VIS_RESULT_NON_SUPPORT);
static_assert(static_cast<VisResult>(vis::Result::OutOfMemory) == VIS_RESULT_OUT_OF_MEMORY);
static_assert(static_cast<VisResult>(vis::Result::IoError) == VIS_RESULT_IO_ERROR);
static_assert(static_cast<VisResult>(vis::Result::ScriptError) == VIS_RESULT_SCRIPT_ERROR);
static_assert(static_cast<VisResult>(vis::Result::Unknown) == VIS_RESULT_UNKNOWN);
static_assert(static_cast<uint8_t>(vis::RenderEngine::Cpu) == VIS_RENDER_ENGINE_CPU);
static_assert(static_cast<uint8_t>(vis::RenderEngine::Gl) == VIS_RENDER_ENGINE_GL);
static_assert(static_cast<uint8_t>(vis::CameraMode::Fixed) == VIS_CAMERA_FIXED);
static_assert(static_cast<uint8_t>(vis::CameraMode::Interactive) == VIS_CAMERA_INTERACTIVE);
static_assert(static_cast<uint8_t>(vis::CameraView::TwoD) == VIS_CAMERA_VIEW_2D);
static_assert(static_cast<uint8_t>(vis::CameraView::ThreeD) == VIS_CAMERA_VIEW_3D);
static_assert(static_cast<uint8_t>(vis::CameraAction::Pan) == VIS_CAMERA_PAN);
static_assert(static_cast<uint8_t>(vis::CameraAction::View3D) == VIS_CAMERA_SET_3D);

static VisEngine _create(vis::Renderer* renderer)
{
    if (!renderer) return nullptr;
    auto engine = new (std::nothrow) Engine;
    if (engine) {
        engine->renderer = renderer;
        return engine;
    }
    delete renderer;
    return nullptr;
}

extern "C" VIS_KEEPALIVE VisEngine vis_create(void)
{
    return _create(vis::Renderer::gen());
}

extern "C" VIS_KEEPALIVE VisEngine vis_create_with_engine(VisRenderEngine value)
{
    vis::RenderEngine engine;
    if (!_renderEngine(value, engine)) return nullptr;
    return _create(vis::Renderer::gen(engine));
}

extern "C" VIS_KEEPALIVE VisResult vis_default_render_engine_set(VisRenderEngine value)
{
    vis::RenderEngine engine;
    if (!_renderEngine(value, engine)) return VIS_RESULT_INVALID_ARGUMENTS;
    return static_cast<VisResult>(vis::Renderer::defaultEngine(engine));
}

extern "C" VIS_KEEPALIVE VisRenderEngine vis_default_render_engine(void)
{
    return static_cast<VisRenderEngine>(vis::Renderer::defaultEngine());
}

extern "C" VIS_KEEPALIVE uint8_t vis_render_engine_enabled(VisRenderEngine value)
{
    vis::RenderEngine engine;
    return _renderEngine(value, engine) && vis::Renderer::enabled(engine) ? 1u : 0u;
}

extern "C" VIS_KEEPALIVE VisRenderEngine vis_render_engine(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine || !engine->renderer) return VIS_RENDER_ENGINE_CPU;
    return static_cast<VisRenderEngine>(engine->renderer->engine());
}

extern "C" VIS_KEEPALIVE void vis_destroy(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine) return;
#if defined(VIS_AUDIO)
    delete engine->audioPlayer;
    for (auto& slot : engine->audioVoices) slot = {};
    engine->soundscape = nullptr;
#endif
    delete engine->scene;
    delete engine->renderer;
    _clearLayoutReport(engine);
    for (auto i = 0u; i < engine->assetCount; i++)
        delete engine->assets[i].asset;
    delete engine;
}

extern "C" VIS_KEEPALIVE VisResult vis_host_theme(VisEngine handle, uint8_t dark,
                                                          uint32_t background, uint32_t foreground,
                                                          uint32_t muted, uint32_t accent,
                                                          uint32_t secondary, uint32_t success,
                                                          uint32_t warning, uint32_t danger,
                                                          uint32_t info, uint32_t surface,
                                                          uint32_t line, uint32_t result,
                                                          uint32_t focus, const uint32_t* objects,
                                                          uint32_t objectCount)
{
    auto engine = _engine(handle);
    if (!engine || !objects || !objectCount || objectCount > vis::Theme::ColorLimit) {
        return VIS_RESULT_INVALID_ARGUMENTS;
    }
    auto theme = vis::Theme::preset(dark ? vis::ThemePreset::ProBlack
                                           : vis::ThemePreset::ProWhite);
    theme.h1.font = "Source Serif 4";
    theme.h2.font = "Source Serif 4";
    theme.h3.font = "Source Serif 4";
    theme.text.font = "Source Serif 4";
    theme.code.font = "JetBrainsMono";
    theme.background = _rgba(background);
    theme.h1.color = _rgba(foreground);
    theme.h2.color = _rgba(foreground);
    theme.h3.color = _rgba(foreground);
    theme.text.color = _rgba(muted);
    theme.code.color = _rgba(accent);
    for (auto i = 0u; i < objectCount; i++) theme.objects[i] = _rgba(objects[i]);
    theme.objectCount = objectCount;
    theme.endGradientStop = _rgba(accent);
    theme.axis = {_rgba(line), _rgba(line), _rgba(line), _rgba(line), _rgba(muted)};
    theme.colors = {
        _rgba(foreground),
        _rgba(muted),
        _rgba(accent),
        _rgba(secondary),
        _rgba(success),
        _rgba(warning),
        _rgba(danger),
        _rgba(info),
        _rgba(surface),
        _rgba(line),
        _rgba(result),
        _rgba(focus),
    };
    engine->hostTheme = theme;
    engine->hasHostTheme = true;
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE uint8_t vis_adaptive_theme(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->adaptiveTheme;
}

extern "C" VIS_KEEPALIVE VisResult vis_load_lua(VisEngine handle, const char* source, uint32_t size,
                                                      const char* name)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!source || !size) return _error(engine, vis::Result::InvalidArguments);
    vis::Scene* scene = nullptr;
    engine->error[0] = '\0';
    bool adaptiveTheme = false;
    vis::detail::LuaHostContext modules;
    auto result = vis::detail::luaHostLoad(
        source, size, name ? name : "scene.lua", &scene, engine->error,
        sizeof(engine->error), _asset, engine,
        engine->hasHostTheme ? &engine->hostTheme : nullptr, &adaptiveTheme, modules);
#if defined(VIS_AUDIO)
    auto soundscape = modules.audio.soundscape;
#endif
#if defined(VIS_UI)
    auto panel = modules.ui.panel;
    if (result == vis::Result::Success && !panel) panel = vis::ui::Panel::gen(scene);
    if (result == vis::Result::Success && !panel) {
        delete scene;
        return _error(engine, vis::Result::OutOfMemory);
    }
    if (result == vis::Result::Success
        && panel->sampler(_sample, engine) != vis::Result::Success) {
        delete scene;
        return _error(engine, vis::Result::Unknown);
    }
#endif
#if defined(VIS_INPUT)
    auto input = modules.input.controller;
    if (result == vis::Result::Success && !input) {
        input = vis::input::Controller::gen(scene);
    }
    if (result == vis::Result::Success && !input) {
        delete scene;
        return _error(engine, vis::Result::OutOfMemory);
    }
#endif
#if defined(VIS_LUA_RUNTIME)
    auto luaRuntime = modules.runtime.runtime;
#if defined(VIS_INPUT)
    if (result == vis::Result::Success && luaRuntime
        && luaRuntime->input(input->state()) != vis::Result::Success) {
        delete scene;
        return _error(engine, vis::Result::Unknown);
    }
#endif
#endif
    if (result != vis::Result::Success) return static_cast<VisResult>(result);
    auto& config = scene->config();
    if (config.height > VIS_PIXEL_LIMIT / config.width) {
        delete scene;
        return _error(engine, vis::Result::InvalidArguments);
    }
#if defined(VIS_AUDIO)
    if (engine->audioPlayer) {
        result = engine->audioPlayer->bind(soundscape);
        if (result != vis::Result::Success) {
            delete scene;
            return _error(engine, result);
        }
    }
#endif
    delete engine->scene;
    engine->scene = scene;
#if defined(VIS_AUDIO)
    engine->soundscape = soundscape;
#endif
#if defined(VIS_UI)
    engine->panel = panel;
    engine->uiSample = {};
    engine->uiSampleValue = {};
    engine->renderPixelRatio = 1u;
    engine->uiSampled = false;
#endif
#if defined(VIS_INPUT)
    engine->input = input;
    engine->inputKey = {};
    engine->inputPointer = {};
#endif
#if defined(VIS_LUA_RUNTIME)
    engine->luaRuntime = luaRuntime;
#endif
    engine->adaptiveTheme = adaptiveTheme;
    engine->rendered = false;
    _clearLayoutReport(engine);
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE VisResult vis_load_font(VisEngine handle, const char* name, const uint8_t* data,
                                                       uint32_t size, const char* mime)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!engine->renderer || !name || !name[0] || !data || !size || size > VIS_FONT_LIMIT || !mime || !mime[0]) {
        return _error(engine, vis::Result::InvalidArguments);
    }
    auto result = engine->renderer->font(name, data, size, mime);
    if (result != vis::Result::Success) return _rendererError(engine, result);
    _clearLayoutReport(engine);
    engine->error[0] = '\0';
    return static_cast<VisResult>(result);
}

extern "C" VIS_KEEPALIVE VisResult vis_load_asset(VisEngine handle, const char* name,
                                                        const uint8_t* data, uint32_t size,
                                                        const char* mime)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    auto nameSize = name ? std::strlen(name) : 0u;
    if (!nameSize || nameSize > 256u || !data || !size || size > VIS_ASSET_SOURCE_LIMIT || !mime || !mime[0]) {
        return _error(engine, vis::Result::InvalidArguments);
    }
    vis::Asset* asset = nullptr;
    auto result = vis::AssetLoader::load(data, size, mime, asset);
    if (result != vis::Result::Success) return _error(engine, result);
    auto bytes = _assetBytes(asset);
    auto index = engine->assetCount;
    for (auto i = 0u; i < engine->assetCount; i++) {
        if (std::strcmp(engine->assets[i].name, name) == 0) {
            index = i;
            break;
        }
    }
    auto retained = engine->assetBytes;
    if (index < engine->assetCount) {
        retained -= _assetBytes(engine->assets[index].asset);
    }
    if (bytes > VIS_ASSET_MEMORY_LIMIT - retained || (index == engine->assetCount && engine->assetCount >= 64u)) {
        delete asset;
        return _error(engine, vis::Result::OutOfMemory);
    }
    if (index == engine->assetCount) engine->assetCount++;
    else {
        engine->assetBytes -= _assetBytes(engine->assets[index].asset);
        delete engine->assets[index].asset;
    }
    std::memcpy(engine->assets[index].name, name, nameSize + 1u);
    engine->assets[index].asset = asset;
    engine->assetBytes += _assetBytes(asset);
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

static VisResult _render(Engine* engine, float time, uint32_t pixelRatio)
{
    if (!engine->scene || !engine->renderer) return _error(engine, vis::Result::InsufficientCondition);
    auto& config = engine->scene->config();
    if (!pixelRatio || config.width > UINT32_MAX / pixelRatio
        || config.height > UINT32_MAX / pixelRatio) {
        return _error(engine, vis::Result::InvalidArguments);
    }
    auto width = config.width * pixelRatio;
    auto height = config.height * pixelRatio;
    if (height > VIS_PIXEL_LIMIT / width) {
        return _error(engine, vis::Result::InvalidArguments);
    }
    engine->rendered = false;
    auto result = engine->renderer->render(engine->scene, time, engine->surface, pixelRatio);
    if (result != vis::Result::Success) return _rendererError(engine, result);
    engine->rendered = true;
    engine->renderTime = time;
#if defined(VIS_UI)
    engine->renderPixelRatio = pixelRatio;
#endif
    engine->error[0] = '\0';
    return static_cast<VisResult>(result);
}

extern "C" VIS_KEEPALIVE VisResult vis_render(VisEngine handle, float time)
{
    auto engine = _engine(handle);
    return engine ? _render(engine, time, 1u) : VIS_RESULT_INVALID_ARGUMENTS;
}

extern "C" VIS_KEEPALIVE VisResult vis_render_scaled(VisEngine handle, float time,
                                                            uint32_t pixelRatio)
{
    auto engine = _engine(handle);
    return engine ? _render(engine, time, pixelRatio) : VIS_RESULT_INVALID_ARGUMENTS;
}

extern "C" VIS_KEEPALIVE VisResult vis_render_gl(VisEngine handle, float time,
                                                         const VisGlTarget* target)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!target) return _error(engine, vis::Result::InvalidArguments);
    if (!engine->scene || !engine->renderer) {
        return _error(engine, vis::Result::InsufficientCondition);
    }
    vis::GlTarget output = {
        target->display,
        target->surface,
        target->context,
        target->id,
        target->width,
        target->height,
    };
    engine->rendered = false;
    auto result = engine->renderer->render(engine->scene, time, output);
    if (result != vis::Result::Success) return _rendererError(engine, result);
    engine->rendered = true;
    engine->renderTime = time;
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE VisResult vis_bounds(VisEngine handle, const char* object, float time,
                                                     VisBBox* output)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!engine->scene || !engine->renderer || !object || !object[0] || !output) {
        return _error(engine, vis::Result::InvalidArguments);
    }
    auto target = engine->scene->object(object);
    if (!target) return _error(engine, vis::Result::InvalidArguments);
    vis::BBox bounds;
    auto result = engine->renderer->bounds(engine->scene, target, time, bounds);
    if (result != vis::Result::Success) return _rendererError(engine, result);
    *output = {bounds.x, bounds.y, bounds.width, bounds.height};
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE VisResult vis_intersects(VisEngine handle, const char* first,
                                                         const char* second, float time, float padding,
                                                         uint8_t* output)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!engine->scene || !engine->renderer || !first || !first[0] || !second || !second[0] || !output) {
        return _error(engine, vis::Result::InvalidArguments);
    }
    auto firstObject = engine->scene->object(first);
    auto secondObject = engine->scene->object(second);
    if (!firstObject || !secondObject) return _error(engine, vis::Result::InvalidArguments);
    auto intersects = false;
    auto result = engine->renderer->intersects(engine->scene, firstObject, secondObject, time,
                                                intersects, padding);
    if (result != vis::Result::Success) return _rendererError(engine, result);
    *output = intersects ? 1u : 0u;
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE VisResult vis_layout_report(VisEngine handle, float time,
                                                             float padding)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!engine->scene || !engine->renderer) {
        return _error(engine, vis::Result::InsufficientCondition);
    }
    vis::LayoutReport report;
    auto result = engine->renderer->layout(engine->scene, time, report, padding);
    if (result != vis::Result::Success) return _rendererError(engine, result);
    JsonBuffer json;
    if (!_serialize(report, engine->scene, json)) {
        return _error(engine, vis::Result::OutOfMemory);
    }
    auto size = json.size;
    auto data = json.release();
    _clearLayoutReport(engine);
    engine->layoutReport = data;
    engine->layoutReportSize = size;
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE const uint8_t* vis_layout_report_data(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->layoutReport : nullptr;
}

extern "C" VIS_KEEPALIVE uint32_t vis_layout_report_size(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->layoutReportSize : 0u;
}

extern "C" VIS_KEEPALIVE VisResult vis_resize(VisEngine handle, uint32_t width, uint32_t height)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!engine->scene || !width || !height || height > VIS_PIXEL_LIMIT / width) {
        return _error(engine, vis::Result::InvalidArguments);
    }
    engine->scene->config().width = width;
    engine->scene->config().height = height;
    engine->rendered = false;
    _clearLayoutReport(engine);
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

#if !defined(__EMSCRIPTEN__) || defined(VIS_UI) || defined(VIS_INPUT)
extern "C" VIS_KEEPALIVE VisResult vis_camera(VisEngine handle, VisCameraAction action, float x, float y)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!engine->scene || action < VIS_CAMERA_PAN || action > VIS_CAMERA_SET_3D) {
        return _error(engine, vis::Result::InvalidArguments);
    }
#if defined(VIS_INPUT)
    if (!engine->input || engine->scene->config().cameraMode != vis::CameraMode::Interactive) {
        return _error(engine, vis::Result::InsufficientCondition);
    }
    vis::Result result;
    switch (action) {
        case VIS_CAMERA_PAN: result = engine->input->cameraMove({x, y}); break;
        case VIS_CAMERA_ORBIT: result = engine->input->cameraOrbit({x, y}); break;
        case VIS_CAMERA_ZOOM: result = engine->input->cameraZoom(y); break;
        case VIS_CAMERA_RESET: result = engine->input->cameraReset(); break;
        case VIS_CAMERA_SET_2D: result = engine->input->cameraView(vis::CameraView::TwoD); break;
        case VIS_CAMERA_SET_3D: result = engine->input->cameraView(vis::CameraView::ThreeD); break;
    }
#elif defined(VIS_UI)
    if (!engine->panel || engine->scene->config().cameraMode != vis::CameraMode::Interactive) {
        return _error(engine, vis::Result::InsufficientCondition);
    }
    vis::Result result;
    switch (action) {
        case VIS_CAMERA_PAN: result = engine->panel->cameraMove({x, y}); break;
        case VIS_CAMERA_ORBIT: result = engine->panel->cameraOrbit({x, y}); break;
        case VIS_CAMERA_ZOOM: result = engine->panel->cameraZoom(y); break;
        case VIS_CAMERA_RESET: result = engine->panel->cameraReset(); break;
        case VIS_CAMERA_SET_2D: result = engine->panel->cameraView(vis::CameraView::TwoD); break;
        case VIS_CAMERA_SET_3D: result = engine->panel->cameraView(vis::CameraView::ThreeD); break;
    }
#else
    auto result = engine->scene->cameraInput({static_cast<vis::CameraAction>(action), {x, y}});
#endif
    if (result != vis::Result::Success) return _error(engine, result);
    engine->rendered = false;
    _clearLayoutReport(engine);
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}
#endif

#if defined(VIS_UI) || defined(VIS_INPUT)
// Dispatches UI first, then unhandled input to Input; cancellation reaches both.
extern "C" VIS_KEEPALIVE uint32_t vis_input(VisEngine handle, uint32_t type,
                                                  float x, float y, float dx, float dy,
                                                  float wheelX, float wheelY, int32_t button,
                                                  uint32_t buttons, uint32_t modifiers,
                                                  uint32_t pointer, uint32_t key,
                                                  uint8_t repeat, float time)
{
    auto engine = _engine(handle);
#if defined(VIS_UI)
    if (engine) engine->uiSampled = false;
#endif
    if (!engine || !engine->scene || type > 6u
        || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dx)
        || !std::isfinite(dy) || !std::isfinite(wheelX) || !std::isfinite(wheelY)
        || !std::isfinite(time) || time < 0.0f) {
        return 0u;
    }
    constexpr uint32_t Handled = 1u;
    constexpr uint32_t Redraw = 2u;
    constexpr uint32_t Capture = 4u;
    constexpr uint32_t Release = 8u;
#if defined(VIS_UI)
    constexpr uint32_t Sampled = 16u;
#endif
    constexpr uint32_t Failed = 32u;
    uint32_t flags = 0u;
#if !defined(VIS_INPUT)
    (void) key;
    (void) repeat;
#endif
#if defined(VIS_UI)
    if (engine->panel && type <= 5u
#if defined(VIS_INPUT)
        && type != static_cast<uint32_t>(vis::ui::InputKind::KeyDown)
#endif
    ) {
        vis::ui::InputEvent event;
        event.kind = static_cast<vis::ui::InputKind>(type);
        event.pointer = pointer;
        event.position = {x, y};
        event.delta = {dx, dy};
        event.wheel = {wheelX, wheelY};
        event.button = button;
        event.buttons = buttons;
        event.modifiers = modifiers;
        event.time = time;
        auto input = engine->panel->input(event);
        if (input.handled) flags |= Handled;
        if (input.redraw) flags |= Redraw;
        if (input.capture) flags |= Capture;
        if (input.release) flags |= Release;
        if (input.status != vis::Result::Success) {
            _error(engine, input.status);
            flags |= Failed;
        }
        if (input.sample && input.sample->selected()) {
            engine->uiSample = input.sample->selection();
            engine->uiSampleValue = input.sample->value();
            engine->uiSampled = true;
            flags |= Sampled;
        }
    }
#endif
#if defined(VIS_INPUT)
    if ((!(flags & Handled)
         || type == static_cast<uint32_t>(vis::input::InputKind::PointerCancel))
        && engine->input) {
        vis::input::InputEvent event;
        event.kind = static_cast<vis::input::InputKind>(type);
        event.key = static_cast<vis::input::Key>(key);
        event.pointer = pointer;
        event.position = {x, y};
        event.delta = {dx, dy};
        event.wheel = {wheelX, wheelY};
        event.button = button;
        event.buttons = buttons;
        event.modifiers = modifiers;
        event.time = time;
        event.repeat = repeat != 0u;
        auto input = engine->input->input(event);
        if (input.handled) flags |= Handled;
        if (input.redraw) flags |= Redraw;
        if (input.status != vis::Result::Success) {
            _error(engine, input.status);
            flags |= Failed;
        }
    }
#if defined(VIS_LUA_RUNTIME)
    if (engine->luaRuntime) {
        auto claimed = engine->luaRuntime->claimsPointer(pointer);
        if ((type == static_cast<uint32_t>(vis::input::InputKind::KeyDown)
             || type == static_cast<uint32_t>(vis::input::InputKind::KeyUp))
            && engine->luaRuntime->claimsKey(key)) {
            flags |= Handled;
        } else if (type == static_cast<uint32_t>(vis::input::InputKind::PointerDown)
                   && button >= 0 && button < 32
                   && (claimed & (1u << button))) {
            flags |= Handled | Capture;
        } else if (type == static_cast<uint32_t>(vis::input::InputKind::PointerMove)
                   && (claimed & buttons)) {
            flags |= Handled;
        } else if (type == static_cast<uint32_t>(vis::input::InputKind::PointerUp)
                   && button >= 0 && button < 32
                   && (claimed & (1u << button))) {
            flags |= Handled | Release;
        } else if (type == static_cast<uint32_t>(vis::input::InputKind::PointerCancel)
                   && claimed) {
            flags |= Handled | Release;
        }
    }
#endif
#endif
    if (flags & Redraw) {
        engine->rendered = false;
        _clearLayoutReport(engine);
    }
    return flags;
}
#endif

#if defined(VIS_INPUT)
extern "C" VIS_KEEPALIVE VisResult vis_input_begin(VisEngine handle, float time)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene || !engine->input || !std::isfinite(time)
        || time < 0.0f) {
        return VIS_RESULT_INVALID_ARGUMENTS;
    }
    auto result = engine->input->state()->begin(time);
    if (result != vis::Result::Success) return _error(engine, result);
    engine->inputKey = {};
    engine->inputPointer = {};
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE uint32_t vis_input_release(VisEngine handle, float time)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene || !engine->input || !std::isfinite(time)
        || time < 0.0f) {
        return 32u;
    }
    auto result = engine->input->release(time);
    auto flags = (result.handled ? 1u : 0u) | (result.redraw ? 2u : 0u);
    if (result.status != vis::Result::Success) {
        _error(engine, result.status);
        return flags | 32u;
    }
    if (result.redraw) {
        engine->rendered = false;
        _clearLayoutReport(engine);
    }
    engine->inputKey = {};
    engine->inputPointer = {};
    engine->error[0] = '\0';
    return flags;
}

extern "C" VIS_KEEPALIVE uint32_t vis_input_key_state(VisEngine handle,
                                                            uint32_t key)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene || !engine->input || key == 0u
        || key > static_cast<uint32_t>(vis::input::Key::Tab)) {
        return UINT32_MAX;
    }
    auto result = engine->input->state()->key(static_cast<vis::input::Key>(key),
                                               engine->inputKey);
    if (result != vis::Result::Success) {
        _error(engine, result);
        return UINT32_MAX;
    }
    engine->error[0] = '\0';
    return (engine->inputKey.previous ? 1u : 0u)
         | (engine->inputKey.down ? 2u : 0u)
         | (engine->inputKey.pressed ? 4u : 0u)
         | (engine->inputKey.released ? 8u : 0u);
}

extern "C" VIS_KEEPALIVE float vis_input_key_begin(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputKey.begin : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_input_key_end(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputKey.end : 0.0f;
}

extern "C" VIS_KEEPALIVE uint32_t vis_input_pointer_state(VisEngine handle,
                                                                uint32_t pointer)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene || !engine->input) return UINT32_MAX;
    auto result = engine->input->state()->pointer(pointer, engine->inputPointer);
    if (result == vis::Result::InsufficientCondition) {
        engine->inputPointer = {};
        engine->error[0] = '\0';
        return 0u;
    }
    if (result != vis::Result::Success) {
        _error(engine, result);
        return UINT32_MAX;
    }
    engine->error[0] = '\0';
    return 1u | (engine->inputPointer.active ? 2u : 0u);
}

extern "C" VIS_KEEPALIVE float vis_input_pointer_x(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.position.x : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_input_pointer_y(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.position.y : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_input_pointer_dx(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.delta.x : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_input_pointer_dy(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.delta.y : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_input_pointer_wheel_x(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.wheel.x : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_input_pointer_wheel_y(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.wheel.y : 0.0f;
}

extern "C" VIS_KEEPALIVE uint32_t vis_input_pointer_previous_buttons(
    VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.previousButtons : 0u;
}

extern "C" VIS_KEEPALIVE uint32_t vis_input_pointer_buttons(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.buttons : 0u;
}

extern "C" VIS_KEEPALIVE uint32_t vis_input_pointer_pressed(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.pressed : 0u;
}

extern "C" VIS_KEEPALIVE uint32_t vis_input_pointer_released(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->inputPointer.released : 0u;
}
#endif

#if defined(VIS_LUA_RUNTIME)
// Runtime exports exist only when compiled in; construction-only scenes remain inactive.
// Hosts feature-detect scheduling and sound exports independently.
extern "C" VIS_KEEPALIVE uint8_t vis_runtime_active(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->luaRuntime ? 1u : 0u;
}

extern "C" VIS_KEEPALIVE VisResult vis_runtime_advance(VisEngine handle,
                                                              float elapsed)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene || !engine->luaRuntime) {
        return VIS_RESULT_INSUFFICIENT_CONDITION;
    }
    auto result = engine->luaRuntime->advance(elapsed);
    if (engine->luaRuntime->steps()) {
        engine->rendered = false;
        _clearLayoutReport(engine);
    }
    if (result != vis::Result::Success) {
        auto message = engine->luaRuntime->error();
        if (message && message[0]) {
            std::snprintf(engine->error, sizeof(engine->error), "%s", message);
        } else {
            _error(engine, result);
        }
        return static_cast<VisResult>(result);
    }
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE double vis_runtime_time(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->luaRuntime ? engine->luaRuntime->time() : 0.0;
}

extern "C" VIS_KEEPALIVE double vis_runtime_dropped(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->luaRuntime ? engine->luaRuntime->dropped() : 0.0;
}

extern "C" VIS_KEEPALIVE double vis_runtime_tick(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->luaRuntime
               ? static_cast<double>(engine->luaRuntime->tick()) : 0.0;
}

extern "C" VIS_KEEPALIVE uint32_t vis_runtime_steps(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->luaRuntime ? engine->luaRuntime->steps() : 0u;
}

extern "C" VIS_KEEPALIVE float vis_runtime_interpolation(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->luaRuntime
               ? engine->luaRuntime->interpolation() : 0.0f;
}

// Queries the last advance's committed sound events without consuming or playing them.
extern "C" VIS_KEEPALIVE uint32_t vis_runtime_sound_count(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->luaRuntime ? engine->luaRuntime->soundCount() : 0u;
}

extern "C" VIS_KEEPALIVE VisResult vis_runtime_sound(
    VisEngine handle, uint32_t index, char* asset, uint32_t assetSize,
    uint32_t* bus, float* gain, float* rate, uint8_t* loop)
{
    auto engine = _engine(handle);
    if (!engine || !engine->luaRuntime || !asset || !assetSize || !bus
        || !gain || !rate || !loop) {
        return VIS_RESULT_INVALID_ARGUMENTS;
    }
    vis::lua_runtime::SoundEvent event;
    if (!engine->luaRuntime->soundAt(index, event)) {
        return VIS_RESULT_INSUFFICIENT_CONDITION;
    }
    auto length = std::strlen(event.asset);
    if (length >= assetSize) return VIS_RESULT_INVALID_ARGUMENTS;
    std::memcpy(asset, event.asset, length + 1u);
    *bus = static_cast<uint32_t>(event.bus);
    *gain = event.gain;
    *rate = event.rate;
    *loop = event.loop ? 1u : 0u;
    return VIS_RESULT_SUCCESS;
}
#endif

#if defined(VIS_AUDIO)
extern "C" VIS_KEEPALIVE uint8_t vis_audio_supported(VisEngine handle)
{
    return _engine(handle) ? 1u : 0u;
}

extern "C" VIS_KEEPALIVE VisResult vis_audio_load(VisEngine handle,
                                                          const char* name,
                                                          const uint8_t* data,
                                                          uint32_t size)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    vis::audio::Player* player = nullptr;
    auto result = _audioPlayer(engine, player);
    if (result == vis::Result::Success) result = player->load(name, data, size);
    return _audioResult(engine, result);
}

extern "C" VIS_KEEPALIVE VisResult vis_audio_start(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    vis::audio::Player* player = nullptr;
    auto result = _audioPlayer(engine, player);
    if (result == vis::Result::Success) result = player->start();
    return _audioResult(engine, result);
}

extern "C" VIS_KEEPALIVE VisResult vis_audio_play(VisEngine handle,
                                                          const char* asset,
                                                          uint32_t bus,
                                                          float gain,
                                                          float rate,
                                                          uint8_t loop,
                                                          uint32_t* output)
{
    auto engine = _engine(handle);
    if (output) *output = 0u;
    vis::audio::Bus targetBus;
    if (!engine || !output || loop > 1u || !_audioBus(bus, targetBus)) {
        return engine ? _error(engine, vis::Result::InvalidArguments)
                      : VIS_RESULT_INVALID_ARGUMENTS;
    }
    vis::audio::Player* player = nullptr;
    auto result = _audioPlayer(engine, player);
    if (result != vis::Result::Success) return _error(engine, result);
    auto index = 0u;
    auto slot = _audioVoiceSlot(engine, player, index);
    if (!slot) return _error(engine, vis::Result::InsufficientCondition);
    auto playback = vis::audio::Playback{};
    playback.asset = asset;
    playback.bus = targetBus;
    playback.gain = gain;
    playback.rate = rate;
    playback.loop = loop != 0u;
    result = player->play(playback, &slot->voice);
    if (result != vis::Result::Success) return _error(engine, result);
    *output = _audioVoice(*slot, index);
    return _audioResult(engine, result);
}

extern "C" VIS_KEEPALIVE VisResult vis_audio_stop(VisEngine handle,
                                                          uint32_t voice)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    if (!engine->audioPlayer) {
        return _error(engine, vis::Result::InsufficientCondition);
    }
    Engine::AudioVoiceSlot* slot = nullptr;
    auto result = _audioVoice(engine, voice, slot);
    if (result != vis::Result::Success) return _error(engine, result);
    result = engine->audioPlayer->stop(slot->voice);
    slot->mapped = false;
    return _audioResult(engine, result);
}

extern "C" VIS_KEEPALIVE uint8_t vis_audio_active(VisEngine handle,
                                                        uint32_t voice)
{
    auto engine = _engine(handle);
    if (!engine || !engine->audioPlayer) return 0u;
    Engine::AudioVoiceSlot* slot = nullptr;
    if (_audioVoice(engine, voice, slot) != vis::Result::Success) return 0u;
    if (engine->audioPlayer->active(slot->voice)) return 1u;
    slot->mapped = false;
    return 0u;
}

extern "C" VIS_KEEPALIVE VisResult vis_audio_master_gain(VisEngine handle,
                                                                 float gain)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
    vis::audio::Player* player = nullptr;
    auto result = _audioPlayer(engine, player);
    if (result == vis::Result::Success) result = player->gain(gain);
    return _audioResult(engine, result);
}

extern "C" VIS_KEEPALIVE VisResult vis_audio_bus_gain(VisEngine handle,
                                                              uint32_t bus,
                                                              float gain)
{
    auto engine = _engine(handle);
    vis::audio::Bus targetBus;
    if (!engine || !_audioBus(bus, targetBus)) {
        return engine ? _error(engine, vis::Result::InvalidArguments)
                      : VIS_RESULT_INVALID_ARGUMENTS;
    }
    vis::audio::Player* player = nullptr;
    auto result = _audioPlayer(engine, player);
    if (result == vis::Result::Success) result = player->gain(targetBus, gain);
    return _audioResult(engine, result);
}

extern "C" VIS_KEEPALIVE VisResult vis_audio_transport(VisEngine handle,
                                                               float time,
                                                               float rate,
                                                               uint8_t playing,
                                                               uint8_t seek)
{
    auto engine = _engine(handle);
    if (!engine || playing > 1u || seek > 1u) {
        return engine ? _error(engine, vis::Result::InvalidArguments)
                      : VIS_RESULT_INVALID_ARGUMENTS;
    }
    vis::audio::Player* player = nullptr;
    auto result = _audioPlayer(engine, player);
    if (result != vis::Result::Success) return _error(engine, result);
    auto transport = vis::audio::Transport{};
    transport.time = time;
    transport.rate = rate;
    transport.playing = playing != 0u;
    transport.seek = seek != 0u;
    result = player->sync(transport);
    return _audioResult(engine, result);
}
#endif

#if defined(VIS_UI)
extern "C" VIS_KEEPALIVE uint32_t vis_ui_input(VisEngine handle, uint32_t type,
                                                     float x, float y, float dx, float dy,
                                                     float wheelX, float wheelY, int32_t button,
                                                     uint32_t buttons, uint32_t modifiers,
                                                     uint32_t pointer, float time)
{
    auto engine = _engine(handle);
    if (engine) engine->uiSampled = false;
    if (!engine || !engine->scene || !engine->panel || type > 5u
        || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dx)
        || !std::isfinite(dy) || !std::isfinite(wheelX) || !std::isfinite(wheelY)
        || !std::isfinite(time) || time < 0.0f) {
        return 0u;
    }
    constexpr uint32_t Handled = 1u;
    constexpr uint32_t Redraw = 2u;
    constexpr uint32_t Capture = 4u;
    constexpr uint32_t Release = 8u;
    constexpr uint32_t Sampled = 16u;
    constexpr uint32_t Failed = 32u;
    uint32_t flags = 0u;
    vis::ui::InputEvent event;
    event.kind = static_cast<vis::ui::InputKind>(type);
    event.pointer = pointer;
    event.position = {x, y};
    event.delta = {dx, dy};
    event.wheel = {wheelX, wheelY};
    event.button = button;
    event.buttons = buttons;
    event.modifiers = modifiers;
    event.time = time;
    auto input = engine->panel->input(event);
    if (input.handled) flags |= Handled;
    if (input.redraw) flags |= Redraw;
    if (input.capture) flags |= Capture;
    if (input.release) flags |= Release;
    if (input.status != vis::Result::Success) {
        _error(engine, input.status);
        flags |= Failed;
    }
    if (input.sample && input.sample->selected()) {
        engine->uiSample = input.sample->selection();
        engine->uiSampleValue = input.sample->value();
        engine->uiSampled = true;
        flags |= Sampled;
    }
    if (!(flags & Handled) && type == 5u && !(modifiers & (2u | 4u | 8u))
        && engine->scene->config().cameraMode == vis::CameraMode::Interactive) {
        vis::Result result = vis::Result::InvalidArguments;
        switch (button) {
            case 0: result = engine->panel->cameraMove({0.04f, 0.0f}); break;
            case 1: result = engine->panel->cameraMove({-0.04f, 0.0f}); break;
            case 2: result = engine->panel->cameraMove({0.0f, -0.04f}); break;
            case 3: result = engine->panel->cameraMove({0.0f, 0.04f}); break;
            case 4: result = engine->panel->cameraZoom(-0.12f); break;
            case 5: result = engine->panel->cameraZoom(0.12f); break;
            case 6: result = engine->panel->cameraReset(); break;
            case 7: result = engine->panel->cameraView(vis::CameraView::TwoD); break;
            case 8: result = engine->panel->cameraView(vis::CameraView::ThreeD); break;
        }
        if (result == vis::Result::Success) flags = Handled | Redraw;
    }
    if (flags & Redraw) {
        engine->rendered = false;
        _clearLayoutReport(engine);
    }
    return flags;
}

extern "C" VIS_KEEPALIVE uint32_t vis_ui_sample_handle(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->uiSampled && engine->uiSample.object
               ? engine->uiSample.object->id() : 0u;
}

extern "C" VIS_KEEPALIVE const char* vis_ui_sample_tag(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->uiSampled && engine->uiSample.object
               ? engine->uiSample.object->tag() : nullptr;
}

extern "C" VIS_KEEPALIVE const char* vis_ui_sample_type(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->uiSampled && engine->uiSample.object
               ? vis::type(engine->uiSample.object->type()) : nullptr;
}

extern "C" VIS_KEEPALIVE float vis_ui_sample_x(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->uiSampled ? engine->uiSample.position.x : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_ui_sample_y(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->uiSampled ? engine->uiSample.position.y : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_ui_sample_u(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->uiSampled ? engine->uiSampleValue.x : 0.0f;
}

extern "C" VIS_KEEPALIVE float vis_ui_sample_v(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->uiSampled ? engine->uiSampleValue.y : 0.0f;
}

extern "C" VIS_KEEPALIVE uint32_t vis_ui_sample_rgba(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine || !engine->uiSampled) return 0u;
    auto& color = engine->uiSample.color;
    return static_cast<uint32_t>(color.r) << 24u
         | static_cast<uint32_t>(color.g) << 16u
         | static_cast<uint32_t>(color.b) << 8u
         | color.a;
}
#endif

extern "C" VIS_KEEPALIVE VisCameraMode vis_camera_mode(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene) return VIS_CAMERA_FIXED;
    return static_cast<VisCameraMode>(engine->scene->config().cameraMode);
}

extern "C" VIS_KEEPALIVE VisCameraView vis_camera_view(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene) return VIS_CAMERA_VIEW_2D;
    auto time = engine->rendered ? engine->renderTime : 0.0f;
    return static_cast<VisCameraView>(engine->scene->view(time));
}

extern "C" VIS_KEEPALIVE const uint8_t* vis_pixels(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->rendered ? reinterpret_cast<const uint8_t*>(engine->surface.data()) : nullptr;
}

extern "C" VIS_KEEPALIVE uint32_t vis_pixels_size(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine || !engine->rendered || !engine->surface.data() || !engine->surface.width()) return 0;
    auto limit = std::numeric_limits<uint32_t>::max() / 4u;
    if (engine->surface.height() > limit / engine->surface.width()) return 0;
    return engine->surface.width() * engine->surface.height() * 4u;
}

extern "C" VIS_KEEPALIVE uint32_t vis_width(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->scene ? engine->scene->config().width : 0;
}

extern "C" VIS_KEEPALIVE uint32_t vis_height(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->scene ? engine->scene->config().height : 0;
}

extern "C" VIS_KEEPALIVE float vis_duration(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine || !engine->scene) return 0.0f;
    auto duration = engine->scene->duration();
#if defined(VIS_AUDIO)
    auto audioDuration = engine->soundscape ? engine->soundscape->duration() : 0.0f;
    if (audioDuration > duration) duration = audioDuration;
#endif
    return duration;
}

extern "C" VIS_KEEPALIVE uint8_t vis_loop(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine && engine->scene && engine->scene->config().loop ? 1u : 0u;
}

extern "C" VIS_KEEPALIVE const char* vis_last_error(VisEngine handle)
{
    auto engine = _engine(handle);
    return engine ? engine->error : "invalid engine";
}
