// The retained Lua runtime: keeps a scene's VM alive after loading so that update callbacks
// move overlays, answer input and start sounds each frame, in fixed steps.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "vis_lua_runtime.h"
#include "../common/visFinite.h"
#include "../lua/visLuaExtension.h"
#include "../lua/visLuaValues.h"
#include "visLuaRuntimeExtension.h"
#include "vis_input.h"

namespace vis::lua_runtime
{

namespace
{

constexpr auto RuntimeMetatable = "vis.lua_runtime.Runtime";
constexpr auto InstructionLimit = 1000000;
uint8_t RuntimeKey = 0u;

static bool _valid(const ObjectState& state) noexcept
{
    return _finite(state.origin) && _finite(state.shift)
           && _finite(state.rotation) && _finite(state.scale)
           && std::isfinite(state.opacity) && state.opacity >= 0.0f
           && state.opacity <= 1.0f && std::isfinite(state.progress)
           && state.progress >= 0.0f && state.progress <= 1.0f;
}

static bool _valid(const SoundEvent& event) noexcept
{
    return event.asset[0] != '\0'
           && std::memchr(event.asset, '\0', sizeof(event.asset))
           && static_cast<uint8_t>(event.bus)
                  <= static_cast<uint8_t>(SoundBus::Ui)
           && std::isfinite(event.gain) && event.gain >= 0.0f
           && event.gain <= 4.0f && std::isfinite(event.rate)
           && event.rate >= 0.125f && event.rate <= 8.0f;
}

static void _message(char* output, uint32_t size, const char* message) noexcept
{
    if (!output || !size) return;
    if (!message) message = "unknown retained Lua error";
    auto length = std::strlen(message);
    if (length >= size) length = size - 1u;
    std::memcpy(output, message, length);
    output[length] = '\0';
}

static void _limit(lua_State* lua, lua_Debug* debug)
{
    if (lua_getinfo(lua, "Sl", debug)) {
        luaL_error(lua, "%s:%d: fixed-step instruction limit exceeded",
                   debug->short_src, debug->currentline);
    }
    luaL_error(lua, "fixed-step instruction limit exceeded");
}

}  // namespace

struct Runtime::Impl
{
    struct Entry
    {
        uint32_t object = 0u;
        ObjectState state;
    };

    Scene* scene = nullptr;
    lua_State* lua = nullptr;
    lua_State* prepared = nullptr;
    Entry states[ObjectLimit];
    Entry staged[ObjectLimit];
    SoundEvent sounds[SoundEventLimit];
    double accumulator = 0.0;
    double elapsed = 0.0;
    double dropped = 0.0;
    uint64_t tick = 0u;
    uint32_t stateCount = 0u;
    uint32_t stagedCount = 0u;
    uint32_t steps = 0u;
    uint32_t stepIndex = 0u;
    uint32_t soundCount = 0u;
    int callback = LUA_NOREF;
    int handle = LUA_NOREF;
    Config config;
    char error[1024] = {};
    struct PointerClaim
    {
        uint32_t pointer = 0u;
        uint32_t buttons = 0u;
    };

    const input::State* input = nullptr;
    input::ActionMap* actions = nullptr;
    PointerClaim pointerClaims[input::ActionMap::BindingLimit];
    uint64_t keyClaims = 0u;
    uint32_t pointerClaimCount = 0u;
    bool running = false;
    bool failed = false;

    ~Impl()
    {
        delete actions;
    }

    Entry* find(Entry* entries, uint32_t count, uint32_t object) noexcept
    {
        for (auto i = 0u; i < count; i++) {
            if (entries[i].object == object) return entries + i;
        }
        return nullptr;
    }

    const Entry* find(const Object* object) const noexcept
    {
        auto id = object ? object->id() : 0u;
        for (auto i = 0u; i < stateCount; i++) {
            if (states[i].object == id) return states + i;
        }
        return nullptr;
    }

    void stage() noexcept
    {
        for (auto i = 0u; i < stateCount; i++) staged[i] = states[i];
        stagedCount = stateCount;
    }

    void commit() noexcept
    {
        for (auto i = 0u; i < stagedCount; i++) states[i] = staged[i];
        stateCount = stagedCount;
    }
};

namespace detail
{

struct RuntimeAccess
{
    static Result prepare(Runtime* runtime, lua_State* lua, int callback,
                          int handle) noexcept
    {
        if (!runtime || !runtime->pImpl || !lua || callback == LUA_NOREF
            || handle == LUA_NOREF) {
            return Result::InvalidArguments;
        }
        auto& impl = *runtime->pImpl;
        if (impl.prepared || impl.lua) return Result::InsufficientCondition;
        impl.prepared = lua;
        impl.callback = callback;
        impl.handle = handle;
        return Result::Success;
    }

    static Result retain(Runtime* runtime, lua_State* lua) noexcept
    {
        if (!runtime || !runtime->pImpl || !lua) return Result::InvalidArguments;
        auto& impl = *runtime->pImpl;
        if (impl.lua || impl.prepared != lua || impl.callback == LUA_NOREF
            || impl.handle == LUA_NOREF) {
            return Result::InsufficientCondition;
        }
        impl.lua = lua;
        impl.prepared = nullptr;
        return Result::Success;
    }

    static bool running(const Runtime* runtime) noexcept
    {
        return runtime && runtime->pImpl && runtime->pImpl->running;
    }

    static Result key(Runtime* runtime, const char* action, input::Key key,
                      const Vec2& value) noexcept
    {
        if (!runtime || !runtime->pImpl) return Result::InvalidArguments;
        auto& impl = *runtime->pImpl;
        if (!impl.actions) impl.actions = input::ActionMap::gen();
        if (!impl.actions) return Result::OutOfMemory;
        auto result = impl.actions->key(action, key, value);
        if (result == Result::Success) {
            impl.keyClaims |= uint64_t{1u} << static_cast<uint32_t>(key);
        }
        return result;
    }

    static Result pointer(Runtime* runtime, const char* action, uint32_t button,
                          const Vec2& value, uint32_t pointer) noexcept
    {
        if (!runtime || !runtime->pImpl) return Result::InvalidArguments;
        auto& impl = *runtime->pImpl;
        if (!impl.actions) impl.actions = input::ActionMap::gen();
        if (!impl.actions) return Result::OutOfMemory;
        auto result = impl.actions->pointer(action, button, value, pointer);
        if (result != Result::Success) return result;
        for (auto i = 0u; i < impl.pointerClaimCount; i++) {
            if (impl.pointerClaims[i].pointer != pointer) continue;
            impl.pointerClaims[i].buttons |= 1u << button;
            return result;
        }
        if (impl.pointerClaimCount >= input::ActionMap::BindingLimit) {
            return Result::InsufficientCondition;
        }
        auto& claim = impl.pointerClaims[impl.pointerClaimCount++];
        claim.pointer = pointer;
        claim.buttons = 1u << button;
        return result;
    }

    static Result action(Runtime* runtime, const char* action,
                         input::ActionState& state) noexcept
    {
        if (!runtime || !runtime->pImpl) return Result::InvalidArguments;
        auto& impl = *runtime->pImpl;
        if (!impl.input || !impl.actions) return Result::InsufficientCondition;
        auto result = impl.actions->state(impl.input, action, state);
        if (result != Result::Success || impl.stepIndex == 0u) return result;
        state.previous = state.value;
        state.delta = {};
        state.pressed = false;
        state.released = false;
        return result;
    }

    static Result key(Runtime* runtime, input::Key key,
                      input::DigitalState& state) noexcept
    {
        if (!runtime || !runtime->pImpl || !runtime->pImpl->input) {
            return Result::InsufficientCondition;
        }
        auto result = runtime->pImpl->input->key(key, state);
        if (result != Result::Success || runtime->pImpl->stepIndex == 0u) return result;
        state.previous = state.down;
        state.pressed = false;
        state.released = false;
        return result;
    }

    static Result pointer(Runtime* runtime, uint32_t id,
                          input::PointerState& state) noexcept
    {
        if (!runtime || !runtime->pImpl || !runtime->pImpl->input) {
            return Result::InsufficientCondition;
        }
        auto result = runtime->pImpl->input->pointer(id, state);
        if (result != Result::Success || runtime->pImpl->stepIndex == 0u) return result;
        state.previousButtons = state.buttons;
        state.delta = {};
        state.wheel = {};
        state.pressed = 0u;
        state.released = 0u;
        return result;
    }
};

}  // namespace detail

Runtime::Runtime(Scene* scene, const Config& config) noexcept
    : pImpl(new (std::nothrow) Impl)
{
    if (!pImpl) return;
    pImpl->scene = scene;
    pImpl->config = config;
}

Runtime::~Runtime()
{
    if (pImpl && pImpl->scene) pImpl->scene->runtimeRemove(this);
    if (pImpl && pImpl->lua) {
        auto lua = pImpl->lua;
        pImpl->lua = nullptr;
        vis::detail::luaClose(lua);
    }
    delete pImpl;
}

Runtime* Runtime::gen(Scene* scene, const Config& config) noexcept
{
    if (!scene || !std::isfinite(config.fixedStep) || config.fixedStep <= 0.0f
        || config.fixedStep > 1.0f || !config.maxSteps || config.maxSteps > 64u) {
        return nullptr;
    }
    auto runtime = new (std::nothrow) Runtime(scene, config);
    if (!runtime || !runtime->pImpl) {
        delete runtime;
        return nullptr;
    }
    RuntimeModifier modifier;
    modifier.object = modifyObject;
    modifier.fill = modifyFill;
    modifier.data = runtime;
    modifier.key = &RuntimeKey;
    if (scene->runtimeAdd(&modifier) != Result::Success) {
        delete runtime;
        return nullptr;
    }
    if (scene->add(runtime) == Result::Success) return runtime;
    scene->runtimeRemove(runtime);
    delete runtime;
    return nullptr;
}

Scene* Runtime::scene() const noexcept
{
    return pImpl ? pImpl->scene : nullptr;
}

Result Runtime::update(Object* object, const ObjectState& state) noexcept
{
    if (!pImpl || !pImpl->scene || !object || object == this || !_valid(state)
        || !object->id() || pImpl->scene->object(object->id()) != object) {
        return Result::InvalidArguments;
    }
    auto entries = pImpl->running ? pImpl->staged : pImpl->states;
    auto& count = pImpl->running ? pImpl->stagedCount : pImpl->stateCount;
    auto entry = pImpl->find(entries, count, object->id());
    auto resolved = state;
    if (!resolved.originEnabled) {
        if (entry && entry->state.originEnabled) {
            resolved.origin = entry->state.origin;
        } else {
            Bounds bounds;
            if (!object->bounds(bounds)) return Result::InsufficientCondition;
            resolved.origin = bounds.center();
        }
        resolved.originEnabled = true;
    }
    if (!entry) {
        if (count == ObjectLimit) return Result::InsufficientCondition;
        entry = entries + count++;
        entry->object = object->id();
    }
    entry->state = resolved;
    return Result::Success;
}

Result Runtime::clear(Object* object) noexcept
{
    if (!pImpl || !pImpl->scene || !object || object == this || !object->id()
        || pImpl->scene->object(object->id()) != object) {
        return Result::InvalidArguments;
    }
    auto entries = pImpl->running ? pImpl->staged : pImpl->states;
    auto& count = pImpl->running ? pImpl->stagedCount : pImpl->stateCount;
    for (auto i = 0u; i < count; i++) {
        if (entries[i].object != object->id()) continue;
        entries[i] = entries[--count];
        return Result::Success;
    }
    return Result::InsufficientCondition;
}

void Runtime::clear() noexcept
{
    if (!pImpl) return;
    if (pImpl->running) pImpl->stagedCount = 0u;
    else pImpl->stateCount = 0u;
}

Result Runtime::sound(const SoundEvent& event) noexcept
{
    if (!pImpl || !_valid(event)) return Result::InvalidArguments;
    if (!pImpl->running || pImpl->soundCount >= SoundEventLimit) {
        return Result::InsufficientCondition;
    }
    pImpl->sounds[pImpl->soundCount++] = event;
    return Result::Success;
}

uint32_t Runtime::soundCount() const noexcept
{
    return pImpl ? pImpl->soundCount : 0u;
}

bool Runtime::soundAt(uint32_t index, SoundEvent& output) const noexcept
{
    if (!pImpl || index >= pImpl->soundCount) return false;
    output = pImpl->sounds[index];
    return true;
}

Result Runtime::advance(float elapsed) noexcept
{
    if (!pImpl || !pImpl->lua || pImpl->callback == LUA_NOREF
        || pImpl->handle == LUA_NOREF) {
        return Result::InsufficientCondition;
    }
    if (!std::isfinite(elapsed) || elapsed < 0.0f) return Result::InvalidArguments;
    if (pImpl->failed) return Result::ScriptError;
    if (pImpl->running) return Result::InsufficientCondition;
    if (pImpl->tick >= static_cast<uint64_t>(LUA_MAXINTEGER)) {
        return Result::InsufficientCondition;
    }

    pImpl->steps = 0u;
    pImpl->soundCount = 0u;
    auto fixed = static_cast<double>(pImpl->config.fixedStep);
    auto capacity = fixed * pImpl->config.maxSteps;
    auto available = pImpl->accumulator + static_cast<double>(elapsed);
    if (available > capacity) {
        pImpl->dropped += available - capacity;
        available = capacity;
    }
    auto epsilon = fixed * 1.0e-9;
    while (available + epsilon >= fixed
           && pImpl->steps < pImpl->config.maxSteps) {
        if (pImpl->tick >= static_cast<uint64_t>(LUA_MAXINTEGER)) {
            pImpl->accumulator = available;
            return Result::InsufficientCondition;
        }
        pImpl->stage();
        auto soundCheckpoint = pImpl->soundCount;
        pImpl->stepIndex = pImpl->steps;
        pImpl->running = true;
        lua_sethook(pImpl->lua, _limit, LUA_MASKCOUNT, InstructionLimit);
        lua_rawgeti(pImpl->lua, LUA_REGISTRYINDEX, pImpl->callback);
        lua_rawgeti(pImpl->lua, LUA_REGISTRYINDEX, pImpl->handle);
        lua_pushnumber(pImpl->lua, fixed);
        lua_pushnumber(pImpl->lua, pImpl->elapsed + fixed);
        lua_pushinteger(pImpl->lua, static_cast<lua_Integer>(pImpl->tick + 1u));
        auto status = lua_pcall(pImpl->lua, 4, 0, 0);
        pImpl->running = false;
        if (status != LUA_OK) {
            _message(pImpl->error, sizeof(pImpl->error),
                     lua_tostring(pImpl->lua, -1));
            lua_pop(pImpl->lua, 1);
            pImpl->stagedCount = 0u;
            pImpl->soundCount = soundCheckpoint;
            pImpl->accumulator = available;
            pImpl->failed = true;
            return Result::ScriptError;
        }
        pImpl->commit();
        pImpl->tick++;
        pImpl->elapsed += fixed;
        pImpl->steps++;
        available -= fixed;
        if (available < 0.0 && available > -epsilon) available = 0.0;
    }
    pImpl->accumulator = available;
    return Result::Success;
}

Result Runtime::input(const input::State* state) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->input = state;
    return Result::Success;
}

bool Runtime::claimsKey(uint32_t key) const noexcept
{
    return pImpl && key < 64u && (pImpl->keyClaims & (uint64_t{1u} << key));
}

uint32_t Runtime::claimsPointer(uint32_t pointer) const noexcept
{
    if (!pImpl) return 0u;
    for (auto i = 0u; i < pImpl->pointerClaimCount; i++) {
        if (pImpl->pointerClaims[i].pointer == pointer) {
            return pImpl->pointerClaims[i].buttons;
        }
    }
    return 0u;
}

float Runtime::fixedStep() const noexcept
{
    return pImpl ? pImpl->config.fixedStep : 0.0f;
}

float Runtime::interpolation() const noexcept
{
    if (!pImpl || pImpl->config.fixedStep <= 0.0f) return 0.0f;
    auto value = static_cast<float>(pImpl->accumulator / pImpl->config.fixedStep);
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

double Runtime::time() const noexcept
{
    return pImpl ? pImpl->elapsed : 0.0;
}

double Runtime::dropped() const noexcept
{
    return pImpl ? pImpl->dropped : 0.0;
}

uint64_t Runtime::tick() const noexcept
{
    return pImpl ? pImpl->tick : 0u;
}

uint32_t Runtime::steps() const noexcept
{
    return pImpl ? pImpl->steps : 0u;
}

bool Runtime::failed() const noexcept
{
    return pImpl && pImpl->failed;
}

const char* Runtime::error() const noexcept
{
    return pImpl ? pImpl->error : "retained Lua runtime is unavailable";
}

Result Runtime::modifyObject(const Object* object, float time, Mat4& model,
                             float& opacity, float& progress, void* data) noexcept
{
    auto runtime = static_cast<Runtime*>(data);
    if (!runtime || !runtime->pImpl || !object || !std::isfinite(time)
        || time < 0.0f) {
        return Result::InvalidArguments;
    }
    auto entry = runtime->pImpl->find(object);
    if (!entry) return Result::Success;
    auto& state = entry->state;
    auto local = Mat4::translate(state.origin + state.shift)
               * Mat4::rotateZ(state.rotation.z)
               * Mat4::rotateY(state.rotation.y)
               * Mat4::rotateX(state.rotation.x)
               * Mat4::scale(state.scale)
               * Mat4::translate(state.origin * -1.0f);
    model = local * model;
    opacity *= state.opacity;
    progress *= state.progress;
    return _finite(model) && std::isfinite(opacity) && std::isfinite(progress)
               ? Result::Success : Result::InvalidArguments;
}

Result Runtime::modifyFill(const Object* object, float time, Color& fill,
                           void* data) noexcept
{
    auto runtime = static_cast<Runtime*>(data);
    if (!runtime || !runtime->pImpl || !object || !std::isfinite(time)
        || time < 0.0f) {
        return Result::InvalidArguments;
    }
    for (auto current = object; current; current = current->parent()) {
        auto entry = runtime->pImpl->find(current);
        if (!entry || !entry->state.fillEnabled) continue;
        fill = entry->state.fill;
        break;
    }
    return Result::Success;
}

namespace
{

struct LuaRuntime
{
    Runtime* runtime = nullptr;
};

static Runtime* _runtime(lua_State* lua)
{
    auto handle = static_cast<LuaRuntime*>(
        luaL_checkudata(lua, 1, RuntimeMetatable));
    if (!handle->runtime || !handle->runtime->scene()) {
        luaL_error(lua, "retained Lua runtime is no longer available");
    }
    return handle->runtime;
}

static Runtime* _running(lua_State* lua)
{
    auto runtime = _runtime(lua);
    if (!detail::RuntimeAccess::running(runtime)) {
        luaL_error(lua, "runtime state is available only inside the fixed-step callback");
    }
    return runtime;
}

static Vec2 _vec2(lua_State* lua, int index, const char* name)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    if (lua_rawlen(lua, index) != 2u) {
        luaL_error(lua, "%s must contain exactly two numbers", name);
    }
    lua_rawgeti(lua, index, 1);
    auto x = _number(lua, -1, name);
    lua_pop(lua, 1);
    lua_rawgeti(lua, index, 2);
    auto y = _number(lua, -1, name);
    lua_pop(lua, 1);
    return {x, y};
}

static Vec3 _vec3(lua_State* lua, int index, const char* name)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    auto count = lua_rawlen(lua, index);
    if (count != 2u && count != 3u) {
        luaL_error(lua, "%s must contain two or three numbers", name);
    }
    lua_rawgeti(lua, index, 1);
    auto x = _number(lua, -1, name);
    lua_pop(lua, 1);
    lua_rawgeti(lua, index, 2);
    auto y = _number(lua, -1, name);
    lua_pop(lua, 1);
    auto z = 0.0f;
    if (count == 3u) {
        lua_rawgeti(lua, index, 3);
        z = _number(lua, -1, name);
        lua_pop(lua, 1);
    }
    return {x, y, z};
}

static Vec3 _fieldVec3(lua_State* lua, int index, const char* name,
                       const Vec3& fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    auto value = _vec3(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

static ObjectState _objectState(lua_State* lua, int index, const Theme& theme)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    static const char* const options[] = {
        "origin", "shift", "rotation", "scale", "opacity", "progress", "fill",
        nullptr,
    };
    _options(lua, index, "runtime config", options);
    ObjectState state;
    lua_getfield(lua, index, "origin");
    if (!lua_isnil(lua, -1)) {
        state.origin = _vec3(lua, -1, "origin");
        state.originEnabled = true;
    }
    lua_pop(lua, 1);
    state.shift = _fieldVec3(lua, index, "shift", state.shift);
    state.scale = _fieldVec3(lua, index, "scale", state.scale);
    lua_getfield(lua, index, "rotation");
    if (!lua_isnil(lua, -1)) {
        if (lua_isnumber(lua, -1)) state.rotation.z = _number(lua, -1, "rotation");
        else state.rotation = _vec3(lua, -1, "rotation");
    }
    lua_pop(lua, 1);
    state.opacity = _fieldNumber(lua, index, "opacity", state.opacity);
    state.progress = _fieldNumber(lua, index, "progress", state.progress);
    lua_getfield(lua, index, "fill");
    if (!lua_isnil(lua, -1)) {
        state.fill = _color(lua, -1, "fill", &theme);
        state.fillEnabled = true;
    }
    lua_pop(lua, 1);
    if (!_valid(state)) luaL_error(lua, "runtime Object state is invalid");
    return state;
}

static int _result(lua_State* lua, Result result)
{
    if (result != Result::Success) luaL_error(lua, "%s", vis::result(result));
    lua_settop(lua, 1);
    return 1;
}

static int _update(lua_State* lua)
{
    auto runtime = _running(lua);
    auto object = vis::detail::luaObject(lua, runtime->scene(), 2);
    auto state = _objectState(lua, 3, runtime->scene()->theme());
    return _result(lua, runtime->update(object, state));
}

static int _clear(lua_State* lua)
{
    auto runtime = _running(lua);
    if (lua_gettop(lua) == 1 || lua_isnil(lua, 2)) {
        runtime->clear();
        return _result(lua, Result::Success);
    }
    auto object = vis::detail::luaObject(lua, runtime->scene(), 2);
    return _result(lua, runtime->clear(object));
}

static int _sound(lua_State* lua)
{
    auto runtime = _running(lua);
    size_t assetSize = 0u;
    auto asset = luaL_checklstring(lua, 2, &assetSize);
    if (!assetSize || assetSize > SoundEvent::AssetNameLimit
        || std::strlen(asset) != assetSize) {
        return luaL_error(
            lua, "sound asset must contain 1 to %u bytes without zero bytes",
            SoundEvent::AssetNameLimit);
    }

    SoundEvent event;
    std::memcpy(event.asset, asset, assetSize);
    event.asset[assetSize] = '\0';
    if (!lua_isnoneornil(lua, 3)) {
        luaL_checktype(lua, 3, LUA_TTABLE);
        static const char* const options[] = {
            "bus", "gain", "rate", "loop", nullptr,
        };
        _options(lua, 3, "runtime config", options);
        event.gain = _fieldNumber(lua, 3, "gain", event.gain);
        event.rate = _fieldNumber(lua, 3, "rate", event.rate);
        lua_getfield(lua, 3, "bus");
        if (!lua_isnil(lua, -1)) {
            auto bus = luaL_checkstring(lua, -1);
            if (std::strcmp(bus, "music") == 0) event.bus = SoundBus::Music;
            else if (std::strcmp(bus, "effect") == 0) event.bus = SoundBus::Effect;
            else if (std::strcmp(bus, "ui") == 0) event.bus = SoundBus::Ui;
            else {
                return luaL_error(lua, "sound bus must be music, effect, or ui");
            }
        }
        lua_pop(lua, 1);
        lua_getfield(lua, 3, "loop");
        if (!lua_isnil(lua, -1)) {
            if (!lua_isboolean(lua, -1)) {
                return luaL_error(lua, "sound loop must be boolean");
            }
            event.loop = lua_toboolean(lua, -1);
        }
        lua_pop(lua, 1);
    }
    if (!_valid(event)) {
        return luaL_error(
            lua, "sound gain must be 0..4 and rate must be 0.125..8");
    }
    auto result = runtime->sound(event);
    if (result != Result::Success && result != Result::InsufficientCondition) {
        return luaL_error(lua, "could not queue runtime sound: %s",
                          vis::result(result));
    }
    lua_pushboolean(lua, result == Result::Success);
    return 1;
}

static input::Key _key(lua_State* lua, int index)
{
    auto name = luaL_checkstring(lua, index);
    if (std::strcmp(name, "ArrowLeft") == 0) return input::Key::ArrowLeft;
    if (std::strcmp(name, "ArrowRight") == 0) return input::Key::ArrowRight;
    if (std::strcmp(name, "ArrowUp") == 0) return input::Key::ArrowUp;
    if (std::strcmp(name, "ArrowDown") == 0) return input::Key::ArrowDown;
    if (std::strcmp(name, "Space") == 0) return input::Key::Space;
    if (std::strcmp(name, "Enter") == 0) return input::Key::Enter;
    if (std::strcmp(name, "Escape") == 0) return input::Key::Escape;
    if (std::strcmp(name, "Plus") == 0) return input::Key::Plus;
    if (std::strcmp(name, "Minus") == 0) return input::Key::Minus;
    if (std::strcmp(name, "Shift") == 0) return input::Key::Shift;
    if (std::strcmp(name, "Tab") == 0) return input::Key::Tab;
    if (name[0] && !name[1]) {
        if (name[0] >= 'A' && name[0] <= 'Z') {
            return static_cast<input::Key>(static_cast<uint8_t>(input::Key::A)
                                           + name[0] - 'A');
        }
        if (name[0] >= 'a' && name[0] <= 'z') {
            return static_cast<input::Key>(static_cast<uint8_t>(input::Key::A)
                                           + name[0] - 'a');
        }
        if (name[0] >= '0' && name[0] <= '9') {
            return static_cast<input::Key>(static_cast<uint8_t>(input::Key::Number0)
                                           + name[0] - '0');
        }
    }
    luaL_error(lua, "unsupported input key '%s'", name);
    return input::Key::Unknown;
}

static void _vec2Table(lua_State* lua, const Vec2& value)
{
    lua_createtable(lua, 2, 2);
    lua_pushnumber(lua, value.x);
    lua_rawseti(lua, -2, 1);
    lua_pushnumber(lua, value.y);
    lua_rawseti(lua, -2, 2);
    lua_pushnumber(lua, value.x);
    lua_setfield(lua, -2, "x");
    lua_pushnumber(lua, value.y);
    lua_setfield(lua, -2, "y");
}

static int _bindKey(lua_State* lua)
{
    auto runtime = _runtime(lua);
    if (!vis::detail::luaAuthoring(lua)) {
        return luaL_error(lua, "runtime bindings are immutable after loading");
    }
    auto action = luaL_checkstring(lua, 2);
    auto key = _key(lua, 3);
    auto value = lua_isnoneornil(lua, 4) ? Vec2{1.0f, 0.0f}
                                         : _vec2(lua, 4, "action value");
    return _result(lua, detail::RuntimeAccess::key(runtime, action, key, value));
}

static int _bindPointer(lua_State* lua)
{
    auto runtime = _runtime(lua);
    if (!vis::detail::luaAuthoring(lua)) {
        return luaL_error(lua, "runtime bindings are immutable after loading");
    }
    auto action = luaL_checkstring(lua, 2);
    auto button = luaL_checkinteger(lua, 3);
    if (button < 0 || button >= 32) {
        return luaL_error(lua, "pointer button must be between 0 and 31");
    }
    auto value = lua_isnoneornil(lua, 4) ? Vec2{1.0f, 0.0f}
                                         : _vec2(lua, 4, "action value");
    auto pointer = luaL_optinteger(lua, 5, 0);
    if (pointer < 0 || static_cast<lua_Unsigned>(pointer) > UINT32_MAX) {
        return luaL_error(lua, "pointer ID must be an unsigned 32-bit integer");
    }
    return _result(lua, detail::RuntimeAccess::pointer(
        runtime, action, static_cast<uint32_t>(button), value,
        static_cast<uint32_t>(pointer)));
}

static int _action(lua_State* lua)
{
    auto runtime = _running(lua);
    auto name = luaL_checkstring(lua, 2);
    input::ActionState state;
    auto result = detail::RuntimeAccess::action(runtime, name, state);
    if (result != Result::Success) {
        return luaL_error(lua, "runtime action '%s' is unavailable: %s", name,
                          vis::result(result));
    }
    lua_createtable(lua, 0, 7);
    _vec2Table(lua, state.previous);
    lua_setfield(lua, -2, "previous");
    _vec2Table(lua, state.value);
    lua_setfield(lua, -2, "value");
    _vec2Table(lua, state.delta);
    lua_setfield(lua, -2, "delta");
    lua_pushboolean(lua, state.down);
    lua_setfield(lua, -2, "down");
    lua_pushboolean(lua, state.pressed);
    lua_setfield(lua, -2, "pressed");
    lua_pushboolean(lua, state.released);
    lua_setfield(lua, -2, "released");
    return 1;
}

static int _keyState(lua_State* lua)
{
    auto runtime = _running(lua);
    input::DigitalState state;
    auto result = detail::RuntimeAccess::key(runtime, _key(lua, 2), state);
    if (result != Result::Success) {
        return luaL_error(lua, "runtime key state is unavailable: %s",
                          vis::result(result));
    }
    lua_createtable(lua, 0, 6);
    lua_pushnumber(lua, state.begin);
    lua_setfield(lua, -2, "begin");
    lua_pushnumber(lua, state.end);
    lua_setfield(lua, -2, "end");
    lua_pushboolean(lua, state.previous);
    lua_setfield(lua, -2, "previous");
    lua_pushboolean(lua, state.down);
    lua_setfield(lua, -2, "down");
    lua_pushboolean(lua, state.pressed);
    lua_setfield(lua, -2, "pressed");
    lua_pushboolean(lua, state.released);
    lua_setfield(lua, -2, "released");
    return 1;
}

static int _pointerState(lua_State* lua)
{
    auto runtime = _running(lua);
    auto pointer = luaL_optinteger(lua, 2, 0);
    if (pointer < 0 || static_cast<lua_Unsigned>(pointer) > UINT32_MAX) {
        return luaL_error(lua, "pointer ID must be an unsigned 32-bit integer");
    }
    input::PointerState state;
    auto result = detail::RuntimeAccess::pointer(
        runtime, static_cast<uint32_t>(pointer), state);
    if (result == Result::InsufficientCondition) {
        lua_pushnil(lua);
        return 1;
    }
    if (result != Result::Success) {
        return luaL_error(lua, "runtime pointer state is unavailable: %s",
                          vis::result(result));
    }
    lua_createtable(lua, 0, 10);
    _vec2Table(lua, state.position);
    lua_setfield(lua, -2, "position");
    _vec2Table(lua, state.delta);
    lua_setfield(lua, -2, "delta");
    _vec2Table(lua, state.wheel);
    lua_setfield(lua, -2, "wheel");
    lua_pushinteger(lua, state.pointer);
    lua_setfield(lua, -2, "pointer");
    lua_pushinteger(lua, state.previousButtons);
    lua_setfield(lua, -2, "previous_buttons");
    lua_pushinteger(lua, state.buttons);
    lua_setfield(lua, -2, "buttons");
    lua_pushinteger(lua, state.pressed);
    lua_setfield(lua, -2, "pressed");
    lua_pushinteger(lua, state.released);
    lua_setfield(lua, -2, "released");
    lua_pushboolean(lua, state.active);
    lua_setfield(lua, -2, "active");
    return 1;
}

static int _runtimeGc(lua_State* lua)
{
    auto handle = static_cast<LuaRuntime*>(lua_touserdata(lua, 1));
    if (handle) handle->runtime = nullptr;
    return 0;
}

static int _newRuntime(lua_State* lua)
{
    if (!vis::detail::luaAuthoring(lua)) {
        return luaL_error(lua, "retained runtime authoring is unavailable after loading");
    }
    auto context = static_cast<lua_extension::LuaContext*>(
        lua_touserdata(lua, lua_upvalueindex(1)));
    if (context->runtime) return luaL_error(lua, "a scene can have only one retained runtime");
    auto scene = vis::detail::luaScene(lua, 1);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "fixed_step", "max_steps", "update", nullptr,
    };
    _options(lua, 2, "runtime config", options);
    Config config;
    config.fixedStep = _fieldNumber(lua, 2, "fixed_step", config.fixedStep);
    lua_getfield(lua, 2, "max_steps");
    if (!lua_isnil(lua, -1)) {
        auto steps = luaL_checkinteger(lua, -1);
        if (steps < 1 || steps > 64) {
            return luaL_error(lua, "max_steps must be between 1 and 64");
        }
        config.maxSteps = static_cast<uint32_t>(steps);
    }
    lua_pop(lua, 1);
    if (!std::isfinite(config.fixedStep) || config.fixedStep <= 0.0f
        || config.fixedStep > 1.0f) {
        return luaL_error(lua, "fixed_step must be finite and between 0 and 1 second");
    }
    lua_getfield(lua, 2, "update");
    if (!lua_isfunction(lua, -1)) return luaL_error(lua, "runtime requires an update function");
    auto callback = luaL_ref(lua, LUA_REGISTRYINDEX);

    auto runtime = Runtime::gen(scene, config);
    if (!runtime) {
        luaL_unref(lua, LUA_REGISTRYINDEX, callback);
        return luaL_error(lua, "could not create retained Lua runtime");
    }
    context->runtime = runtime;
    context->scene = scene;
    auto handle = static_cast<LuaRuntime*>(
        lua_newuserdatauv(lua, sizeof(LuaRuntime), 1));
    handle->runtime = runtime;
    luaL_setmetatable(lua, RuntimeMetatable);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    lua_pushvalue(lua, -1);
    auto reference = luaL_ref(lua, LUA_REGISTRYINDEX);
    auto result = detail::RuntimeAccess::prepare(runtime, lua, callback, reference);
    if (result != Result::Success) {
        luaL_unref(lua, LUA_REGISTRYINDEX, callback);
        luaL_unref(lua, LUA_REGISTRYINDEX, reference);
        return luaL_error(lua, "could not prepare retained Lua runtime: %s",
                          vis::result(result));
    }
    return 1;
}

static int _open(lua_State* lua, void* data)
{
    static const luaL_Reg methods[] = {
        {"update", _update},
        {"clear", _clear},
        {"sound", _sound},
        {"bind_key", _bindKey},
        {"bind_pointer", _bindPointer},
        {"action", _action},
        {"key", _keyState},
        {"pointer", _pointerState},
        {nullptr, nullptr},
    };
    luaL_newmetatable(lua, RuntimeMetatable);
    lua_pushcfunction(lua, _runtimeGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "vis.lua_runtime.Runtime");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, methods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    lua_getglobal(lua, "vis");
    lua_pushlightuserdata(lua, data);
    lua_pushcclosure(lua, _newRuntime, 1);
    lua_setfield(lua, -2, "runtime");
    lua_pop(lua, 1);
    return 0;
}

static Result _loaded(lua_State*, Scene* scene, void* data) noexcept
{
    auto context = static_cast<lua_extension::LuaContext*>(data);
    if (context->runtime && context->scene != scene) return Result::InvalidArguments;
    return Result::Success;
}

}  // namespace

Result Lua::load(const char* source, uint32_t size, const char* name,
                 Scene** scene, Runtime** runtime, char* error,
                 uint32_t errorSize, vis::Lua::AssetResolver resolver,
                 void* resolverData, const Theme* adaptiveTheme,
                 bool* adaptiveThemeUsed) noexcept
{
    if (!scene || !runtime) return Result::InvalidArguments;
    *runtime = nullptr;
    lua_extension::LuaContext context;
    auto hook = lua_extension::luaHooks(context);
    vis::detail::LuaHookSpan hooks = {&hook, 1u};
    lua_State* retained = nullptr;
    auto result = vis::detail::luaLoad(
        source, size, name, scene, error, errorSize, resolver, resolverData,
        nullptr, 0u, adaptiveTheme, adaptiveThemeUsed, hooks, &retained);
    if (result != Result::Success) return result;
    if (!context.runtime) {
        vis::detail::luaClose(retained);
        return result;
    }
    result = lua_extension::retain(context, retained);
    if (result == Result::Success) {
        if (runtime) *runtime = context.runtime;
        return result;
    }
    vis::detail::luaClose(retained);
    delete *scene;
    *scene = nullptr;
    return result;
}

vis::detail::LuaHooks lua_extension::luaHooks(LuaContext& context) noexcept
{
    return {_open, _loaded, &context};
}

Result lua_extension::retain(LuaContext& context, lua_State* lua) noexcept
{
    if (!context.runtime || !lua) return Result::InvalidArguments;
    return detail::RuntimeAccess::retain(context.runtime, lua);
}

}  // namespace vis::lua_runtime
