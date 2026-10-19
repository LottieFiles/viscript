// The Lua binding of the audio module: cues on a scene's soundscape and their gains.
#include <cmath>
#include <cstring>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "vis_audio.h"
#include "visAudioLuaExtension.h"
#include "../lua/visLuaValues.h"

namespace vis::audio
{

namespace
{

static constexpr const char* CUE_METATABLE = "vis.audio.Cue";

struct LuaCue
{
    Soundscape* soundscape = nullptr;
    Cue cue;
};

static void _fieldAsset(lua_State* lua, int index,
                        char (&asset)[Soundscape::AssetNameLimit + 1u])
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "asset");
    if (lua_isnil(lua, -1)) luaL_error(lua, "audio cue requires asset");
    size_t size = 0u;
    auto value = luaL_checklstring(lua, -1, &size);
    if (!size || size > Soundscape::AssetNameLimit || std::memchr(value, '\0', size)) {
        luaL_error(lua, "audio asset must be 1 to %d bytes without zero bytes",
                   static_cast<int>(Soundscape::AssetNameLimit));
    }
    std::memcpy(asset, value, size);
    asset[size] = '\0';
    lua_pop(lua, 1);
}

static Bus _fieldBus(lua_State* lua, int index)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "bus");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return Bus::Effect;
    }
    auto value = luaL_checkstring(lua, -1);
    auto bus = Bus::Effect;
    if (std::strcmp(value, "music") == 0) bus = Bus::Music;
    else if (std::strcmp(value, "effect") == 0) bus = Bus::Effect;
    else if (std::strcmp(value, "ui") == 0) bus = Bus::Ui;
    else luaL_error(lua, "audio bus must be music, effect or ui");
    lua_pop(lua, 1);
    return bus;
}

static LuaCue* _cue(lua_State* lua)
{
    auto cue = static_cast<LuaCue*>(luaL_checkudata(lua, 1, CUE_METATABLE));
    if (!cue->soundscape || !cue->cue) luaL_error(lua, "audio cue is unavailable");
    return cue;
}

static int _gain(lua_State* lua)
{
    detail::luaTiming(lua, 4);
    if (!detail::luaAuthoring(lua)) {
        return luaL_error(lua, "audio authoring is unavailable after loading");
    }
    auto cue = _cue(lua);
    auto value = _number(lua, 2, "audio gain");
    auto begin = _number(lua, 3, "audio gain begin");
    auto duration = lua_isnoneornil(lua, 4) ? 1.0f : _number(lua, 4, "audio gain duration");
    auto curve = detail::luaAnimCurve(lua, 5);
    auto result = cue->soundscape->gain(cue->cue, value, begin, duration, curve);
    if (result != Result::Success) {
        return luaL_error(lua, "could not animate audio gain: %s", vis::result(result));
    }
    lua_settop(lua, 1);
    return 1;
}

static int _newCue(lua_State* lua)
{
    if (!detail::luaAuthoring(lua)) {
        return luaL_error(lua, "audio authoring is unavailable after loading");
    }
    auto context = static_cast<lua_extension::LuaContext*>(
        lua_touserdata(lua, lua_upvalueindex(1)));
    auto scene = detail::luaScene(lua, 1);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "asset", "bus", "begin", "end", "gain", "loop", nullptr,
    };
    _options(lua, 2, "audio cue", options);
    char asset[Soundscape::AssetNameLimit + 1u];
    _fieldAsset(lua, 2, asset);
    CueConfig config;
    config.asset = asset;
    config.bus = _fieldBus(lua, 2);
    config.begin = _fieldNumber(lua, 2, "begin", 0.0f);
    config.end = _fieldNumber(lua, 2, "end", 0.0f);
    config.gain = _fieldNumber(lua, 2, "gain", 1.0f);
    config.loop = _fieldBool(lua, 2, "loop", false);

    if (context->scene && context->scene != scene) {
        return luaL_error(lua, "audio cues must belong to one scene");
    }
    if (!context->soundscape) {
        context->soundscape = Soundscape::gen(scene);
        if (!context->soundscape) return luaL_error(lua, "could not create audio soundscape");
        context->scene = scene;
    }

    Cue cue;
    auto result = context->soundscape->cue(config, cue);
    if (result != Result::Success) {
        return luaL_error(lua, "could not create audio cue: %s", vis::result(result));
    }
    auto handle = static_cast<LuaCue*>(lua_newuserdatauv(lua, sizeof(LuaCue), 1));
    handle->soundscape = context->soundscape;
    handle->cue = cue;
    luaL_setmetatable(lua, CUE_METATABLE);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

static int _open(lua_State* lua, void* data)
{
    static const luaL_Reg methods[] = {
        {"gain", _gain},
        {nullptr, nullptr},
    };
    luaL_newmetatable(lua, CUE_METATABLE);
    lua_pushliteral(lua, "vis.audio.Cue");
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
    lua_pushcclosure(lua, _newCue, 1);
    lua_setfield(lua, -2, "cue");
    lua_setfield(lua, -2, "audio");
    lua_pop(lua, 1);
    return 0;
}

static Result _loaded(lua_State*, Scene* scene, void* data) noexcept
{
    auto context = static_cast<lua_extension::LuaContext*>(data);
    if (context->soundscape && context->scene != scene) return Result::InvalidArguments;
    return Result::Success;
}

}  // namespace

vis::detail::LuaHooks lua_extension::luaHooks(LuaContext& context) noexcept
{
    return {_open, _loaded, &context};
}

}  // namespace vis::audio
