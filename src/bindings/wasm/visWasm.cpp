// The C functions the WebAssembly runtime exports to client.js: engines that load scenes from
// Lua, render frames into pixels, report layouts and errors as JSON, and take input, assets
// and audio.
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
#include "../../core/renderer/visLayoutJson.h"
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
    // Fonts the scene's texts name that are not loaded, one per line (see vis_missing_fonts).
    char missingFonts[1024] = {};
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

// The fonts the loaded scene's texts need that are not loaded, one per line: other fonts stand
// in for them until the host loads fonts under those names and loads the scene again.
extern "C" VIS_KEEPALIVE const char* vis_missing_fonts(VisEngine handle)
{
    auto engine = _engine(handle);
    if (!engine) return "";
    auto& list = engine->missingFonts;
    list[0] = '\0';
    const char* fonts[64];
    auto count = engine->scene ? engine->scene->missingFonts(fonts, 64u) : 0u;
    auto used = size_t{0};
    for (auto i = 0u; i < count; i++) {
        auto length = std::strlen(fonts[i]);
        if (std::strchr(fonts[i], '\n') || used + length + 2u > sizeof(list)) continue;
        std::memcpy(list + used, fonts[i], length);
        used += length;
        list[used++] = '\n';
        list[used] = '\0';
    }
    return list;
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
    auto index = engine->assetCount;
    for (auto i = 0u; i < engine->assetCount; i++) {
        if (std::strcmp(engine->assets[i].name, name) == 0) {
            index = i;
            break;
        }
    }
    // Assets count by their encoded source; the RGBA pixels they decode to on render are not
    // counted, which keeps the accounting stable.
    auto replaced = index < engine->assetCount ? engine->assets[index].asset : nullptr;
    auto retained = engine->assetBytes - (replaced ? replaced->encodedSize() : 0u);
    if (asset->encodedSize() > VIS_ASSET_MEMORY_LIMIT - retained || (!replaced && engine->assetCount >= 64u)) {
        delete asset;
        return _error(engine, vis::Result::OutOfMemory);
    }
    if (!replaced) engine->assetCount++;
    delete replaced;
    std::memcpy(engine->assets[index].name, name, nameSize + 1u);
    engine->assets[index].asset = asset;
    engine->assetBytes = retained + asset->encodedSize();
    engine->error[0] = '\0';
    return VIS_RESULT_SUCCESS;
}

extern "C" VIS_KEEPALIVE VisResult vis_render(VisEngine handle, float time, uint32_t pixelRatio)
{
    auto engine = _engine(handle);
    if (!engine) return VIS_RESULT_INVALID_ARGUMENTS;
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
    vis::JsonBuffer json;
    if (!vis::layoutJson(report, engine->scene, json)) {
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
    auto result = engine->scene->cameraInput({static_cast<vis::CameraAction>(action), {x, y}});
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
                                                  uint32_t pointer, uint32_t key, float time)
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
    (void) buttons;
    (void) modifiers;
    (void) key;
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
    engine->error[0] = '\0';
    return flags;
}
#endif

#if defined(VIS_LUA_RUNTIME)
// Runtime exports exist only when compiled in; construction-only scenes remain inactive.
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
