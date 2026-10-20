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
    return handle->controller;
}

// The keys a script names in a word; a letter or a digit names its own key.
static constexpr LuaName<Key> KEYS[] = {
    {"ArrowLeft", Key::ArrowLeft}, {"ArrowRight", Key::ArrowRight}, {"ArrowUp", Key::ArrowUp},
    {"ArrowDown", Key::ArrowDown}, {"Space", Key::Space}, {"Enter", Key::Enter},
    {"Escape", Key::Escape}, {"Plus", Key::Plus}, {"Minus", Key::Minus}, {"Shift", Key::Shift},
    {"Tab", Key::Tab},
};

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
    binding.region = _fieldPointerRegion(lua, 2);
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
    binding.key = lua_extension::luaKey(lua, -1);
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
    detail::luaMetatable(lua, CONTROLLER_METATABLE, nullptr, {methods});

    lua_getglobal(lua, "vis");
    lua_newtable(lua);
    lua_pushlightuserdata(lua, data);
    lua_pushcclosure(lua, _newController, 1);
    lua_setfield(lua, -2, "controller");
    lua_setfield(lua, -2, "input");
    lua_pop(lua, 1);
    return 0;
}

Key lua_extension::luaKey(lua_State* lua, int index)
{
    size_t size;
    auto name = _checkString(lua, index, "key", &size);
    if (auto found = _findName(name, size, KEYS)) return found->value;
    if (size == 1u) {
        auto c = name[0] >= 'a' && name[0] <= 'z' ? name[0] - 'a' + 'A' : name[0];
        if (c >= 'A' && c <= 'Z') {
            return static_cast<Key>(static_cast<uint8_t>(Key::A) + c - 'A');
        }
        if (c >= '0' && c <= '9') {
            return static_cast<Key>(static_cast<uint8_t>(Key::Number0) + c - '0');
        }
    }
    luaL_error(lua, "unsupported input key '%s'", name);
    return Key::Unknown;
}

vis::detail::LuaHooks lua_extension::luaHooks(lua_extension::LuaContext& context) noexcept
{
    return {_open, detail::luaLoadedScene<LuaContext, &LuaContext::controller>, &context};
}

}  // namespace vis::input
