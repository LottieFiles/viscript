#include <cmath>
#include <cstring>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "vis_motion.h"
#include "visMotionLuaExtension.h"

namespace vis::motion
{

namespace
{

static constexpr const char* MotionMetatable = "vis.motion.Controller";

struct LuaController
{
    Controller* controller = nullptr;
};

static Controller* _controller(lua_State* lua)
{
    auto handle = static_cast<LuaController*>(luaL_checkudata(lua, 1, MotionMetatable));
    if (!handle->controller || !handle->controller->scene()) {
        luaL_error(lua, "motion controller is no longer available");
    }
    return handle->controller;
}

static void _options(lua_State* lua, int index, const char* kind, const char* const* names)
{
    index = lua_absindex(lua, index);
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) != LUA_TSTRING) {
            luaL_error(lua, "%s keys must be strings", kind);
        }
        auto key = lua_tostring(lua, -2);
        auto known = false;
        for (auto cursor = names; *cursor; cursor++) {
            if (std::strcmp(key, *cursor) != 0) continue;
            known = true;
            break;
        }
        if (!known) luaL_error(lua, "unknown %s field '%s'", kind, key);
        lua_pop(lua, 1);
    }
}

static float _number(lua_State* lua, int index, const char* name)
{
    auto value = static_cast<float>(luaL_checknumber(lua, index));
    if (!std::isfinite(value)) luaL_error(lua, "%s must be finite", name);
    return value;
}

static float _fieldNumber(lua_State* lua, int index, const char* name, float fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    auto value = _number(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

static Vec3 _vec3(lua_State* lua, int index, const char* name,
                  float z = 0.0f)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    auto count = lua_rawlen(lua, index);
    if (count != 2u && count != 3u) {
        luaL_error(lua, "%s must contain two or three numbers", name);
    }
    Vec3 value = {0.0f, 0.0f, z};
    lua_rawgeti(lua, index, 1);
    value.x = _number(lua, -1, name);
    lua_pop(lua, 1);
    lua_rawgeti(lua, index, 2);
    value.y = _number(lua, -1, name);
    lua_pop(lua, 1);
    if (count == 3u) {
        lua_rawgeti(lua, index, 3);
        value.z = _number(lua, -1, name);
        lua_pop(lua, 1);
    }
    return value;
}

static Vec3 _fieldVec3(lua_State* lua, int index, const char* name, const Vec3& fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    auto value = _vec3(lua, -1, name, fallback.z);
    lua_pop(lua, 1);
    return value;
}

static const char* _text(lua_State* lua, int index, const char* name)
{
    size_t size = 0u;
    auto value = luaL_checklstring(lua, index, &size);
    if (!size || std::strlen(value) != size) {
        luaL_error(lua, "%s must be non-empty text without zero bytes", name);
    }
    return value;
}

static State _state(lua_State* lua, int index)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    static const char* const options[] = {
        "origin", "shift", "rotation", "scale", "opacity", "progress", nullptr,
    };
    _options(lua, index, "motion state", options);
    State state;
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
    return state;
}

static float _duration(lua_State* lua, int index, float fallback)
{
    if (lua_isnoneornil(lua, index)) return fallback;
    auto value = _number(lua, index, "motion duration");
    if (value < 0.0f) luaL_error(lua, "motion duration must be non-negative");
    return value;
}

static AnimCurve _curve(lua_State* lua, int index)
{
    return lua_isnoneornil(lua, index) ? AnimCurve::preset(AnimCurvePreset::Smooth)
                                       : detail::luaAnimCurve(lua, index);
}

static int _result(lua_State* lua, Result result, const char* operation)
{
    if (result != Result::Success) {
        return luaL_error(lua, "motion %s failed: %s", operation, vis::result(result));
    }
    lua_settop(lua, 1);
    return 1;
}

static int _define(lua_State* lua)
{
    auto controller = _controller(lua);
    if (!detail::luaAuthoring(lua)) {
        return luaL_error(lua, "motion definitions are immutable after loading");
    }
    auto object = detail::luaObject(lua, controller->scene(), 2);
    auto name = _text(lua, 3, "motion state name");
    auto state = _state(lua, 4);
    return _result(lua, controller->define(object, name, state), "define");
}

static int _transition(lua_State* lua)
{
    auto controller = _controller(lua);
    auto object = detail::luaObject(lua, controller->scene(), 2);
    auto name = _text(lua, 3, "motion state name");
    return _result(lua,
                   controller->transition(object, name, _duration(lua, 4, 0.2f), _curve(lua, 5)),
                   "transition");
}

static int _transitionMany(lua_State* lua)
{
    auto controller = _controller(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    auto count = lua_rawlen(lua, 2);
    if (!count || count > Controller::ObjectLimit) {
        return luaL_error(lua, "motion targets must contain 1 to %u entries",
                          Controller::ObjectLimit);
    }
    // A state given as a number turns into a string that only the stack holds, so the states
    // wait in a table of their own until the controller has read them. The table is stored above
    // the arguments, so absent ones must read as nil.
    lua_settop(lua, 4);
    lua_createtable(lua, static_cast<int>(count), 0);
    auto states = lua_gettop(lua);
    Target targets[Controller::ObjectLimit];
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, 2, i + 1u);
        luaL_checktype(lua, -1, LUA_TTABLE);
        static const char* const options[] = {"object", "state", nullptr};
        _options(lua, -1, "motion target", options);
        lua_getfield(lua, -1, "object");
        targets[i].object = detail::luaObject(lua, controller->scene(), -1);
        lua_pop(lua, 1);
        lua_getfield(lua, -1, "state");
        targets[i].state = _text(lua, -1, "motion target state");
        lua_rawseti(lua, states, i + 1u);
        lua_pop(lua, 1);
    }
    return _result(lua,
                   controller->transition(targets, static_cast<uint32_t>(count),
                                          _duration(lua, 3, 0.2f), _curve(lua, 4)),
                   "transition");
}

static int _advance(lua_State* lua)
{
    auto controller = _controller(lua);
    return _result(lua, controller->advance(_number(lua, 2, "motion elapsed")), "advance");
}

static void _vec3Table(lua_State* lua, const Vec3& value)
{
    lua_createtable(lua, 3, 0);
    lua_pushnumber(lua, value.x);
    lua_rawseti(lua, -2, 1);
    lua_pushnumber(lua, value.y);
    lua_rawseti(lua, -2, 2);
    lua_pushnumber(lua, value.z);
    lua_rawseti(lua, -2, 3);
}

static int _sample(lua_State* lua)
{
    auto controller = _controller(lua);
    auto object = detail::luaObject(lua, controller->scene(), 2);
    State state;
    auto result = controller->sample(object, state);
    if (result != Result::Success) {
        return luaL_error(lua, "motion sample failed: %s", vis::result(result));
    }
    lua_createtable(lua, 0, 6);
    _vec3Table(lua, state.origin);
    lua_setfield(lua, -2, "origin");
    _vec3Table(lua, state.shift);
    lua_setfield(lua, -2, "shift");
    _vec3Table(lua, state.rotation);
    lua_setfield(lua, -2, "rotation");
    _vec3Table(lua, state.scale);
    lua_setfield(lua, -2, "scale");
    lua_pushnumber(lua, state.opacity);
    lua_setfield(lua, -2, "opacity");
    lua_pushnumber(lua, state.progress);
    lua_setfield(lua, -2, "progress");
    return 1;
}

static int _time(lua_State* lua)
{
    lua_pushnumber(lua, _controller(lua)->time());
    return 1;
}

static int _active(lua_State* lua)
{
    lua_pushboolean(lua, _controller(lua)->active());
    return 1;
}

static int _definitionCount(lua_State* lua)
{
    lua_pushinteger(lua, _controller(lua)->definitionCount());
    return 1;
}

static int _eventCount(lua_State* lua)
{
    lua_pushinteger(lua, _controller(lua)->eventCount());
    return 1;
}

static const char* _preset(AnimCurvePreset preset) noexcept
{
    switch (preset) {
        case AnimCurvePreset::Linear: return "linear";
        case AnimCurvePreset::Smooth: return "smooth";
        case AnimCurvePreset::EaseIn: return "ease_in";
        case AnimCurvePreset::EaseOut: return "ease_out";
        case AnimCurvePreset::EaseInOut: return "ease_in_out";
        case AnimCurvePreset::Gentle: return "gentle";
        case AnimCurvePreset::Snappy: return "snappy";
        case AnimCurvePreset::Back: return "back";
        case AnimCurvePreset::Bounce: return "bounce";
        case AnimCurvePreset::Elastic: return "elastic";
        case AnimCurvePreset::Custom: return "custom";
    }
    return "unknown";
}

static void _curveTable(lua_State* lua, const AnimCurve& curve)
{
    lua_createtable(lua, 0, curve.kind == AnimCurvePreset::Custom ? 4 : 3);
    lua_pushstring(lua, _preset(curve.kind));
    lua_setfield(lua, -2, "preset");
    lua_pushnumber(lua, curve.strength);
    lua_setfield(lua, -2, "strength");
    lua_pushboolean(lua, curve.reverse);
    lua_setfield(lua, -2, "reverse");
    if (curve.kind != AnimCurvePreset::Custom) return;
    lua_createtable(lua, 4, 0);
    lua_pushnumber(lua, curve.control1.x);
    lua_rawseti(lua, -2, 1);
    lua_pushnumber(lua, curve.control1.y);
    lua_rawseti(lua, -2, 2);
    lua_pushnumber(lua, curve.control2.x);
    lua_rawseti(lua, -2, 3);
    lua_pushnumber(lua, curve.control2.y);
    lua_rawseti(lua, -2, 4);
    lua_setfield(lua, -2, "bezier");
}

static void _transaction(lua_State* lua, uint64_t value)
{
    char buffer[21] = {};
    auto cursor = sizeof(buffer) - 1u;
    do {
        buffer[--cursor] = static_cast<char>('0' + value % 10u);
        value /= 10u;
    } while (value);
    lua_pushstring(lua, buffer + cursor);
}

static int _event(lua_State* lua)
{
    auto controller = _controller(lua);
    auto index = luaL_checkinteger(lua, 2);
    if (index < 1 || static_cast<lua_Unsigned>(index) > UINT32_MAX) {
        lua_pushnil(lua);
        return 1;
    }
    Event event;
    if (!controller->eventAt(static_cast<uint32_t>(index - 1), event)) {
        lua_pushnil(lua);
        return 1;
    }
    lua_createtable(lua, 0, 7);
    lua_pushnumber(lua, event.time);
    lua_setfield(lua, -2, "time");
    _transaction(lua, event.transaction);
    lua_setfield(lua, -2, "transaction");
    lua_pushinteger(lua, event.object);
    lua_setfield(lua, -2, "object");
    lua_pushstring(lua, event.state);
    lua_setfield(lua, -2, "state");
    lua_pushnumber(lua, event.duration);
    lua_setfield(lua, -2, "duration");
    _curveTable(lua, event.curve);
    lua_setfield(lua, -2, "curve");
    return 1;
}

static int _clearEvents(lua_State* lua)
{
    _controller(lua)->clearEvents();
    lua_settop(lua, 1);
    return 1;
}

static int _controllerGc(lua_State* lua)
{
    auto handle = static_cast<LuaController*>(lua_touserdata(lua, 1));
    if (handle) handle->controller = nullptr;
    return 0;
}

static int _newController(lua_State* lua)
{
    if (!detail::luaAuthoring(lua)) {
        return luaL_error(lua, "motion authoring is unavailable after loading");
    }
    auto context =
        static_cast<lua_extension::LuaContext*>(lua_touserdata(lua, lua_upvalueindex(1)));
    if (context->controller) {
        return luaL_error(lua, "a scene can have only one motion controller");
    }
    auto scene = detail::luaScene(lua, 1);
    auto controller = Controller::gen(scene);
    if (!controller) return luaL_error(lua, "could not create motion controller");
    context->controller = controller;
    context->scene = scene;
    auto handle = static_cast<LuaController*>(lua_newuserdatauv(lua, sizeof(LuaController), 1));
    handle->controller = controller;
    luaL_setmetatable(lua, MotionMetatable);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

static int _open(lua_State* lua, void* data)
{
    static const luaL_Reg methods[] = {
        {"define", _define},
        {"transition", _transition},
        {"transition_many", _transitionMany},
        {"advance", _advance},
        {"sample", _sample},
        {"time", _time},
        {"active", _active},
        {"definition_count", _definitionCount},
        {"event_count", _eventCount},
        {"event", _event},
        {"clear_events", _clearEvents},
        {nullptr, nullptr},
    };
    luaL_newmetatable(lua, MotionMetatable);
    lua_pushcfunction(lua, _controllerGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "vis.motion.Controller");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, methods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    lua_getglobal(lua, "vis");
    lua_pushlightuserdata(lua, data);
    lua_pushcclosure(lua, _newController, 1);
    lua_setfield(lua, -2, "motion");
    lua_pop(lua, 1);
    return 0;
}

static Result _loaded(lua_State*, Scene* scene, void* data) noexcept
{
    auto context = static_cast<lua_extension::LuaContext*>(data);
    if (context->controller && context->scene != scene) return Result::InvalidArguments;
    return Result::Success;
}

}  // namespace

Result Lua::load(const char* source, uint32_t size, const char* name, Scene** scene,
                 Controller** controller, char* error, uint32_t errorSize,
                 vis::Lua::AssetResolver resolver, void* resolverData, const Theme* adaptiveTheme,
                 bool* adaptiveThemeUsed) noexcept
{
    if (controller) *controller = nullptr;
    lua_extension::LuaContext context;
    auto hook = lua_extension::luaHooks(context);
    detail::LuaHookSpan hooks = {&hook, 1u};
    auto result =
        detail::luaLoad(source, size, name, scene, error, errorSize, resolver, resolverData,
                        nullptr, 0u, adaptiveTheme, adaptiveThemeUsed, hooks);
    if (result == Result::Success && controller) *controller = context.controller;
    return result;
}

vis::detail::LuaHooks lua_extension::luaHooks(LuaContext& context) noexcept
{
    return {_open, _loaded, &context};
}

}  // namespace vis::motion
