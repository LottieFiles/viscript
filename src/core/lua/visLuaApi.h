// Names of the Lua metatables and registry tables that the binding and the modules share.
#ifndef _VIS_LUA_API_H_
#define _VIS_LUA_API_H_

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace vis
{

static constexpr const char* SCENE_METATABLE = "vis.Scene";
static constexpr const char* VIEW_METATABLE = "vis.View";
static constexpr const char* OBJECT_METATABLE = "vis.Object";
static constexpr const char* SPACE_METATABLE = "vis.Space";
static constexpr const char* GROUP_METATABLE = "vis.Group";
// Registry table of the handles given out for attached objects, keyed by Object, with weak values.
static constexpr const char* HANDLE_REGISTRY = "vis.handles";
// Registry table of the sources of code blocks, keyed by scene with weak keys: for each scene a
// table of {text, options} keyed by the block's Object, which search and edit read.
static constexpr const char* CODE_REGISTRY = "vis.code";
// Registry table of what plots keep of their cuts, keyed by scene with weak keys: for each scene a
// LuaPlotRecord keyed by the plot's Object, which the states that give the plot fn or points read.
static constexpr const char* PLOT_REGISTRY = "vis.plots";
// The tracker prelude's flush, which plays the segments of the tracks that wait for it.
static constexpr const char* TRACKER_FLUSH = "vis.tracker.flush";
// Registry table of the makers modules give spans parts: the keys that mark the parts in the
// order the modules added them, from 1, and each key's maker under the key.
static constexpr const char* SPAN_PART_REGISTRY = "vis.spanParts";

namespace detail
{

struct LuaApi
{
    const luaL_Reg* factories;
    const luaL_Reg* sceneMethods;
    const luaL_Reg* objectMethods;
    const luaL_Reg* groupMethods;
    const luaL_Reg* spaceMethods;
    const luaL_Reg* viewMethods;
    lua_CFunction sceneGc;
    lua_CFunction objectGc;
    lua_CFunction sceneGen;
    lua_CFunction springResponse;
    lua_CFunction trackerEase;
    lua_CFunction trackerSettle;
    lua_CFunction trackerFps;
    lua_CFunction trackerUnknown;
    lua_CFunction trackerWhere;
    lua_CFunction trackerPending;
};

int luaOpenApi(lua_State* lua, const LuaApi& api);

}  // namespace detail

}  // namespace vis

#endif
