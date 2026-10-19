// The Lua binding of the input module: controllers with pointer follows and key moves.
#include <cmath>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "vis_input.h"
#include "../lua/visLuaExtension.h"
#include "../lua/visLuaValues.h"
#include "visInputLuaExtension.h"

namespace vis::input
{

static constexpr const char* CONTROLLER_METATABLE = "vis.input.Controller";

struct LuaController
{
    Controller* controller = nullptr;
};

static Controller* _controller(lua_State* lua)
{
    if (!detail::luaAuthoring(lua)) {
        luaL_error(lua, "Input authoring is unavailable after loading");
    }
    auto handle = static_cast<LuaController*>(
        luaL_checkudata(lua, 1, CONTROLLER_METATABLE));
    if (!handle->controller) luaL_error(lua, "input controller is no longer available");
    return handle->controller;
}

static Vec3 _vec3(lua_State* lua, int index, const char* name)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    Vec3 value;
    if (_has(lua, index, "x") || _has(lua, index, "y") || _has(lua, index, "z")) {
        if (!_has(lua, index, "x") || !_has(lua, index, "y") || !_has(lua, index, "z")) {
            luaL_error(lua, "%s must contain x, y and z", name);
        }
        lua_getfield(lua, index, "x");
        value.x = _number(lua, -1, name);
        lua_pop(lua, 1);
        lua_getfield(lua, index, "y");
        value.y = _number(lua, -1, name);
        lua_pop(lua, 1);
        lua_getfield(lua, index, "z");
        value.z = _number(lua, -1, name);
        lua_pop(lua, 1);
        return value;
    }
    auto count = lua_rawlen(lua, index);
    if (count != 2u && count != 3u) {
        luaL_error(lua, "%s must contain two or three numbers", name);
    }
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

static Vec3 _fieldVec3(lua_State* lua, int index, const char* name,
                       const Vec3& fallback = {})
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

static BBox _fieldRegion(lua_State* lua, int index)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "region");
    if (lua_isnil(lua, -1)) luaL_error(lua, "missing field 'region'");
    luaL_checktype(lua, -1, LUA_TTABLE);
    auto region = lua_absindex(lua, -1);
    BBox bounds;
    if (_has(lua, region, "x") || _has(lua, region, "y")
        || _has(lua, region, "width") || _has(lua, region, "height")) {
        const char* names[] = {"x", "y", "width", "height"};
        float* values[] = {&bounds.x, &bounds.y, &bounds.width, &bounds.height};
        for (auto i = 0u; i < 4u; i++) {
            lua_getfield(lua, region, names[i]);
            if (lua_isnil(lua, -1)) luaL_error(lua, "region requires %s", names[i]);
            *values[i] = _number(lua, -1, names[i]);
            lua_pop(lua, 1);
        }
    } else {
        if (lua_rawlen(lua, region) != 4u) {
            luaL_error(lua, "region must contain x, y, width and height");
        }
        float* values[] = {&bounds.x, &bounds.y, &bounds.width, &bounds.height};
        for (auto i = 0u; i < 4u; i++) {
            lua_rawgeti(lua, region, i + 1u);
            *values[i] = _number(lua, -1, "region component");
            lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 1);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        luaL_error(lua, "region width and height must be positive");
    }
    return bounds;
}

static Object* _fieldObject(lua_State* lua, int index, const char* name, Scene* scene)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) luaL_error(lua, "missing field '%s'", name);
    auto object = detail::luaObject(lua, scene, -1);
    lua_pop(lua, 1);
    return object;
}

static Key _key(lua_State* lua, int index)
{
    auto name = luaL_checkstring(lua, index);
    if (std::strcmp(name, "ArrowLeft") == 0) return Key::ArrowLeft;
    if (std::strcmp(name, "ArrowRight") == 0) return Key::ArrowRight;
    if (std::strcmp(name, "ArrowUp") == 0) return Key::ArrowUp;
    if (std::strcmp(name, "ArrowDown") == 0) return Key::ArrowDown;
    if (std::strcmp(name, "Space") == 0) return Key::Space;
    if (std::strcmp(name, "Enter") == 0) return Key::Enter;
    if (std::strcmp(name, "Escape") == 0) return Key::Escape;
    if (std::strcmp(name, "Plus") == 0) return Key::Plus;
    if (std::strcmp(name, "Minus") == 0) return Key::Minus;
    if (std::strcmp(name, "Shift") == 0) return Key::Shift;
    if (std::strcmp(name, "Tab") == 0) return Key::Tab;
    if (name[0] && !name[1]) {
        if (name[0] >= 'A' && name[0] <= 'Z') {
            return static_cast<Key>(static_cast<uint8_t>(Key::A) + name[0] - 'A');
        }
        if (name[0] >= 'a' && name[0] <= 'z') {
            return static_cast<Key>(static_cast<uint8_t>(Key::A) + name[0] - 'a');
        }
        if (name[0] >= '0' && name[0] <= '9') {
            return static_cast<Key>(static_cast<uint8_t>(Key::Number0) + name[0] - '0');
        }
    }
    luaL_error(lua, "unsupported input key '%s'", name);
    return Key::Unknown;
}

static int _result(lua_State* lua, Result result)
{
    if (result != Result::Success) luaL_error(lua, "%s", vis::result(result));
    lua_settop(lua, 1);
    return 1;
}

static int _pointerFollow(lua_State* lua)
{
    auto controller = _controller(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "target", "region", "target_origin", "map_origin", "map_x", "map_y",
        "period", "rotate", "angle_offset", "reset_on_leave", nullptr,
    };
    _options(lua, 2, "Input config", options);
    if (!_has(lua, 2, "map_origin") || !_has(lua, 2, "map_x")
        || !_has(lua, 2, "map_y")) {
        return luaL_error(lua, "pointer_follow requires map_origin, map_x and map_y");
    }
    PointerFollow binding;
    binding.target = _fieldObject(lua, 2, "target", controller->scene());
    binding.region = _fieldRegion(lua, 2);
    binding.targetOrigin = _fieldVec3(lua, 2, "target_origin");
    binding.mapOrigin = _fieldVec3(lua, 2, "map_origin");
    binding.mapX = _fieldVec3(lua, 2, "map_x");
    binding.mapY = _fieldVec3(lua, 2, "map_y");
    binding.period = _fieldNumber(lua, 2, "period", binding.period);
    binding.rotate = _fieldBool(lua, 2, "rotate", binding.rotate);
    binding.angleOffset = _fieldNumber(lua, 2, "angle_offset", binding.angleOffset);
    binding.resetOnLeave = _fieldBool(lua, 2, "reset_on_leave", binding.resetOnLeave);
    return _result(lua, controller->pointerFollow(binding));
}

static int _keyMove(lua_State* lua)
{
    auto controller = _controller(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "target", "key", "shift", "period", nullptr,
    };
    _options(lua, 2, "Input config", options);
    if (!_has(lua, 2, "key") || !_has(lua, 2, "shift")) {
        return luaL_error(lua, "key_move requires key and shift");
    }
    KeyMove binding;
    binding.target = _fieldObject(lua, 2, "target", controller->scene());
    lua_getfield(lua, 2, "key");
    binding.key = _key(lua, -1);
    lua_pop(lua, 1);
    binding.shift = _fieldVec3(lua, 2, "shift");
    binding.period = _fieldNumber(lua, 2, "period", binding.period);
    return _result(lua, controller->keyMove(binding));
}

static int _newController(lua_State* lua)
{
    if (!detail::luaAuthoring(lua)) {
        return luaL_error(lua, "Input authoring is unavailable after loading");
    }
    auto context = static_cast<lua_extension::LuaContext*>(
        lua_touserdata(lua, lua_upvalueindex(1)));
    if (context->controller) return luaL_error(lua, "a scene can have only one Input controller");
    auto scene = detail::luaScene(lua, 1);
    auto controller = Controller::gen(scene);
    if (!controller) return luaL_error(lua, "could not create Input controller");
    context->controller = controller;
    context->scene = scene;
    auto handle = static_cast<LuaController*>(
        lua_newuserdatauv(lua, sizeof(LuaController), 1));
    handle->controller = controller;
    luaL_setmetatable(lua, CONTROLLER_METATABLE);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    lua_pushvalue(lua, -1);
    luaL_ref(lua, LUA_REGISTRYINDEX);
    return 1;
}

static int _open(lua_State* lua, void* data)
{
    static const luaL_Reg methods[] = {
        {"pointer_follow", _pointerFollow},
        {"key_move", _keyMove},
        {nullptr, nullptr},
    };
    luaL_newmetatable(lua, CONTROLLER_METATABLE);
    lua_pushliteral(lua, "vis.input.Controller");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, methods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    lua_getglobal(lua, "vis");
    lua_newtable(lua);
    lua_pushlightuserdata(lua, data);
    lua_pushcclosure(lua, _newController, 1);
    lua_setfield(lua, -2, "controller");
    lua_setfield(lua, -2, "input");
    lua_pop(lua, 1);
    return 0;
}

static Result _loaded(lua_State*, Scene* scene, void* data) noexcept
{
    auto context = static_cast<lua_extension::LuaContext*>(data);
    if (context->controller && context->scene != scene) return Result::InvalidArguments;
    return Result::Success;
}

Result Lua::load(const char* source, uint32_t size, const char* name, Scene** scene,
                 Controller** controller, char* error, uint32_t errorSize,
                 vis::Lua::AssetResolver resolver, void* resolverData,
                 const Theme* adaptiveTheme, bool* adaptiveThemeUsed) noexcept
{
    if (controller) *controller = nullptr;
    lua_extension::LuaContext context;
    auto hooks = lua_extension::luaHooks(context);
    detail::LuaHookSpan extensions = {&hooks, 1u};
    auto result = detail::luaLoad(source, size, name, scene, error, errorSize, resolver,
                                  resolverData, nullptr, 0u, adaptiveTheme,
                                  adaptiveThemeUsed, extensions);
    if (result == Result::Success && controller) *controller = context.controller;
    return result;
}

vis::detail::LuaHooks lua_extension::luaHooks(lua_extension::LuaContext& context) noexcept
{
    return {_open, _loaded, &context};
}

}  // namespace vis::input
