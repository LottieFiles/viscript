// The vis.* library functions (merge, keys, format, tostring, the random seed) and luaOpenApi,
// which registers the scene API, the module methods and the global guard.
#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include <lualib.h>
}

#include "visLuaApi.h"
#include "visLuaBudget.h"
#include "visLuaExtension.h"
#include "visLuaTracker.h"

namespace vis
{

static int _trackTiming(lua_State* lua)
{
    detail::luaTiming(lua, 1);
    lua_createtable(lua, 2, 0);
    lua_pushvalue(lua, 1);
    lua_rawseti(lua, -2, 1);
    lua_pushvalue(lua, 2);
    lua_rawseti(lua, -2, 2);
    return 1;
}

static int _toString(lua_State* lua)
{
    switch (lua_type(lua, 1)) {
        case LUA_TNIL: lua_pushliteral(lua, "nil"); break;
        case LUA_TBOOLEAN: lua_pushstring(lua, lua_toboolean(lua, 1) ? "true" : "false"); break;
        case LUA_TNUMBER:
        case LUA_TSTRING: luaL_tolstring(lua, 1, nullptr); break;
        default: return luaL_error(lua, "tostring accepts only nil, booleans, numbers and strings");
    }
    return 1;
}

static int _format(lua_State* lua)
{
    size_t size;
    auto format = luaL_checklstring(lua, 1, &size);
    static constexpr auto conversions = "cdiuoxXaAfFeEgGpsq";
    for (auto i = 0u; i < size; i++) {
        if (format[i] != '%') continue;
        if (++i < size && format[i] == '%') continue;
        while (i < size && !std::strchr(conversions, format[i]))
            i++;
        if (i < size && format[i] == 'p') return luaL_error(lua, "string.format %%p is unavailable");
    }
    auto count = lua_gettop(lua);
    for (auto i = 2; i <= count; i++) {
        auto type = lua_type(lua, i);
        if (type != LUA_TNIL && type != LUA_TBOOLEAN && type != LUA_TNUMBER && type != LUA_TSTRING) {
            return luaL_argerror(lua, i, "format values must be nil, booleans, numbers or strings");
        }
    }
    return detail::luaCallWrapped(lua, lua_upvalueindex(1));
}

// vis.merge(a, b, ...): a new table with the fields of every table argument, later tables
// winning; nil arguments are skipped. Nested tables are shared, not copied.
static int _merge(lua_State* lua)
{
    auto count = lua_gettop(lua);
    for (auto i = 1; i <= count; i++) {
        if (!lua_isnil(lua, i) && !lua_istable(lua, i)) {
            return luaL_error(lua, "merge accepts only tables and nil, got %s for argument %d", luaL_typename(lua, i), i);
        }
    }
    lua_newtable(lua);
    for (auto i = 1; i <= count; i++) {
        if (lua_isnil(lua, i)) continue;
        lua_pushnil(lua);
        while (lua_next(lua, i)) {
            lua_pushvalue(lua, -2);
            lua_insert(lua, -2);
            lua_rawset(lua, -4);
        }
    }
    return 1;
}

struct LuaKey
{
    lua_Integer integer;
    lua_Number number;
    const char* string;
    size_t size;
    bool isInteger;
};

static bool _keyBefore(const LuaKey& a, const LuaKey& b)
{
    if (!a.string != !b.string) return !a.string;
    if (a.string) {
        auto common = std::min(a.size, b.size);
        auto order = std::memcmp(a.string, b.string, common);
        return order ? order < 0 : a.size < b.size;
    }
    if (a.isInteger && b.isInteger) return a.integer < b.integer;
    return a.number < b.number;
}

// vis.keys(t): the keys of t in an order that never changes between runs, which pairs and
// next cannot promise: numbers ascending, then strings in byte order.
static int _keys(lua_State* lua)
{
    luaL_checktype(lua, 1, LUA_TTABLE);
    lua_settop(lua, 1);
    lua_Integer count = 0;
    lua_pushnil(lua);
    while (lua_next(lua, 1)) {
        lua_pop(lua, 1);
        auto type = lua_type(lua, -1);
        if (type != LUA_TNUMBER && type != LUA_TSTRING) {
            return luaL_error(lua, "keys orders only number and string keys, got a %s key", luaL_typename(lua, -1));
        }
        count++;
    }
    if (static_cast<lua_Unsigned>(count) > LUA_MEMORY_LIMIT / sizeof(LuaKey)) return luaL_error(lua, "table has too many keys");
    auto keys = static_cast<LuaKey*>(lua_newuserdatauv(lua, sizeof(LuaKey) * static_cast<size_t>(count), 0));
    auto size = 0u;
    lua_pushnil(lua);
    while (lua_next(lua, 1)) {
        lua_pop(lua, 1);
        auto& key = keys[size++];
        key.isInteger = lua_isinteger(lua, -1);
        key.integer = key.isInteger ? lua_tointeger(lua, -1) : 0;
        key.number = lua_type(lua, -1) == LUA_TNUMBER ? lua_tonumber(lua, -1) : 0.0;
        // The table keeps its string keys alive while the result is built.
        key.string = lua_type(lua, -1) == LUA_TSTRING ? lua_tolstring(lua, -1, &key.size) : nullptr;
    }
    std::sort(keys, keys + size, _keyBefore);
    lua_createtable(lua, static_cast<int>(size), 0);
    for (auto i = 0u; i < size; i++) {
        auto& key = keys[i];
        if (key.string) lua_pushlstring(lua, key.string, key.size);
        else if (key.isInteger) lua_pushinteger(lua, key.integer);
        else lua_pushnumber(lua, key.number);
        lua_rawseti(lua, -2, static_cast<lua_Integer>(i) + 1);
    }
    return 1;
}

struct LuaAbsent
{
    // The table the name is missing from: "" for globals, or a library name.
    const char* library;
    const char* name;
    const char* reason;
};

static constexpr auto ABSENT_ORDER =
    "its order can change between runs; use ipairs for arrays or vis.keys for other tables";
static constexpr auto ABSENT_CONSOLE =
    "a scene script has no console; stop with error(...) to see a value, or read the scene with viscript inspect";
static constexpr auto ABSENT_FILE = "a scene script is one file; define shared helpers as local functions in it";
static constexpr auto ABSENT_ERROR = "an error stops the script so that it is reported with its line";
static constexpr auto ABSENT_META = "metatables are reserved for viscript handles; use vis.merge to fill in defaults";
static constexpr auto ABSENT_SYSTEM = "scene scripts cannot reach files, the clock or the system";
static constexpr auto ABSENT_PATTERN =
    "patterns can run without bound; use string.sub, string.byte and plain comparisons";

// Construction scripts exclude filesystem/system access, dynamic code, console output,
// protected calls, metatable changes, unordered iteration and pattern matching.
// Missing reserved APIs report their reason; use ordered keys and plain string operations.
static constexpr LuaAbsent ABSENT[] = {
    {"", "pairs", ABSENT_ORDER},
    {"", "next", ABSENT_ORDER},
    {"", "print", ABSENT_CONSOLE},
    {"", "warn", ABSENT_CONSOLE},
    {"", "require", ABSENT_FILE},
    {"", "dofile", ABSENT_FILE},
    {"", "loadfile", ABSENT_FILE},
    {"", "load", "scripts cannot compile code while they run"},
    {"", "pcall", ABSENT_ERROR},
    {"", "xpcall", ABSENT_ERROR},
    {"", "collectgarbage", "the host manages memory"},
    {"", "getmetatable", ABSENT_META},
    {"", "setmetatable", ABSENT_META},
    {"", "io", ABSENT_SYSTEM},
    {"", "os", ABSENT_SYSTEM},
    {"", "package", ABSENT_SYSTEM},
    {"", "debug", ABSENT_SYSTEM},
    {LUA_STRLIBNAME, "find", ABSENT_PATTERN},
    {LUA_STRLIBNAME, "match", ABSENT_PATTERN},
    {LUA_STRLIBNAME, "gmatch", ABSENT_PATTERN},
    {LUA_STRLIBNAME, "gsub", ABSENT_PATTERN},
};

// The __index of the global, string and math tables. A name the sandbox leaves out raises an
// error that says why, where a plain nil would end in "attempt to call a nil value"; any other
// missing name is still nil.
static int _absent(lua_State* lua)
{
    if (lua_type(lua, 2) != LUA_TSTRING) return 0;
    auto library = lua_tostring(lua, lua_upvalueindex(1));
    auto name = lua_tostring(lua, 2);
    for (auto& absent : ABSENT) {
        if (std::strcmp(absent.library, library) != 0 || std::strcmp(absent.name, name) != 0) continue;
        if (*library) return luaL_error(lua, "%s.%s is not available in viscript scripts: %s", library, name, absent.reason);
        return luaL_error(lua, "%s is not available in viscript scripts: %s", name, absent.reason);
    }
    return 0;
}

// The factories an optional module adds to the scene and object handles. Until the module
// adds one, as in a build without it, calling it stops the script with the build option.
struct LuaModuleMethod
{
    const char* name;
    const char* module;
};

static constexpr LuaModuleMethod MODULE_METHODS[] = {
    {"tex", "tex"},
};

static int _moduleMethod(lua_State* lua)
{
    auto name = lua_tostring(lua, lua_upvalueindex(1));
    auto module = lua_tostring(lua, lua_upvalueindex(2));
    return luaL_error(lua, "%s needs the %s module: build viscript with -Dmodules=%s", name, module, module);
}

// Removes the absent names of library from the table on top of the stack and guards it.
static void _guard(lua_State* lua, const char* library)
{
    for (auto& absent : ABSENT) {
        if (std::strcmp(absent.library, library) != 0) continue;
        lua_pushnil(lua);
        lua_setfield(lua, -2, absent.name);
    }
    lua_createtable(lua, 0, 2);
    lua_pushstring(lua, library);
    lua_pushcclosure(lua, _absent, 1);
    lua_setfield(lua, -2, "__index");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_setmetatable(lua, -2);
}

// math.randomseed(x [, y]) starts math.random over from the seed of the integers x and y, 0 by
// default, as Lua's own does, so a scene picks its numbers and every platform draws the same
// ones. Without a seed Lua's would read the clock, so a call must give one.
static int _randomSeed(lua_State* lua)
{
    if (lua_isnoneornil(lua, 1)) {
        return luaL_error(lua, "math.randomseed needs a seed, such as math.randomseed(42), so that every "
                               "render of a scene matches");
    }
    auto first = luaL_checkinteger(lua, 1);
    auto second = luaL_optinteger(lua, 2, 0);
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_pushinteger(lua, first);
    lua_pushinteger(lua, second);
    lua_call(lua, 2, 2);
    return 2;
}

int detail::luaOpenApi(lua_State* lua, const LuaApi& api)
{
    auto registry = [lua](const char* name, const char* mode) {
        lua_newtable(lua);
        if (mode) {
            lua_createtable(lua, 0, 1);
            lua_pushstring(lua, mode);
            lua_setfield(lua, -2, "__mode");
            lua_setmetatable(lua, -2);
        }
        lua_setfield(lua, LUA_REGISTRYINDEX, name);
    };
    registry(HANDLE_REGISTRY, "v");
    registry(CODE_REGISTRY, "k");
    registry(PLOT_REGISTRY, "k");
    registry(SPAN_PART_REGISTRY, nullptr);

    auto library = [lua](const char* name, lua_CFunction open) {
        luaL_requiref(lua, name, open, 1);
        lua_pop(lua, 1);
    };
    library(LUA_GNAME, luaopen_base);
    library(LUA_TABLIBNAME, luaopen_table);
    library(LUA_STRLIBNAME, luaopen_string);
    library(LUA_MATHLIBNAME, luaopen_math);
    library(LUA_UTF8LIBNAME, luaopen_utf8);

    // math.random starts from the seed 0 in every run, and math.randomseed takes only a seed.
    lua_getglobal(lua, LUA_MATHLIBNAME);
    lua_getfield(lua, -1, "randomseed");
    lua_pushvalue(lua, -1);
    lua_pushinteger(lua, 0);
    lua_call(lua, 1, 0);
    lua_pushcclosure(lua, _randomSeed, 1);
    lua_setfield(lua, -2, "randomseed");
    _guard(lua, LUA_MATHLIBNAME);
    lua_pop(lua, 1);

    lua_pushcfunction(lua, _toString);
    lua_setglobal(lua, "tostring");
    lua_getglobal(lua, LUA_STRLIBNAME);
    lua_getfield(lua, -1, "format");
    lua_pushcclosure(lua, _format, 1);
    lua_setfield(lua, -2, "format");
    _guard(lua, LUA_STRLIBNAME);
    lua_pop(lua, 1);

    luaL_newmetatable(lua, OBJECT_METATABLE);
    lua_pushcfunction(lua, api.objectGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, api.factories, 0);
    luaL_setfuncs(lua, api.objectMethods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    luaL_newmetatable(lua, SPACE_METATABLE);
    lua_pushcfunction(lua, api.objectGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "vis.Space");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, api.factories, 0);
    luaL_setfuncs(lua, api.objectMethods, 0);
    luaL_setfuncs(lua, api.spaceMethods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    luaL_newmetatable(lua, GROUP_METATABLE);
    lua_pushcfunction(lua, api.objectGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "vis.Group");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, api.factories, 0);
    luaL_setfuncs(lua, api.objectMethods, 0);
    luaL_setfuncs(lua, api.groupMethods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    luaL_newmetatable(lua, SCENE_METATABLE);
    lua_pushcfunction(lua, api.sceneGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "vis.Scene");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, api.factories, 0);
    luaL_setfuncs(lua, api.sceneMethods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    luaL_newmetatable(lua, VIEW_METATABLE);
    lua_pushliteral(lua, "vis.View");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, api.viewMethods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    for (auto& method : MODULE_METHODS) {
        for (auto handle : {detail::LuaHandle::Scene, detail::LuaHandle::Object}) {
            lua_pushstring(lua, method.name);
            lua_pushstring(lua, method.module);
            lua_pushcclosure(lua, _moduleMethod, 2);
            detail::luaSetMethod(lua, handle, method.name);
        }
    }

    lua_newtable(lua);
    lua_pushcfunction(lua, api.sceneGen);
    lua_setfield(lua, -2, "scene");
    lua_pushcfunction(lua, api.springResponse);
    lua_setfield(lua, -2, "spring_response");
    lua_pushcfunction(lua, _merge);
    lua_setfield(lua, -2, "merge");
    lua_pushcfunction(lua, _keys);
    lua_setfield(lua, -2, "keys");
    lua_setglobal(lua, "vis");

    if (luaL_loadbuffer(lua, TRACKER_PRELUDE, std::strlen(TRACKER_PRELUDE), "=tracker") != LUA_OK) {
        return lua_error(lua);
    }
    luaL_getmetatable(lua, SCENE_METATABLE);
    lua_getfield(lua, -1, "__index");
    lua_remove(lua, -2);
    lua_pushcfunction(lua, api.trackerEase);
    lua_pushcfunction(lua, api.trackerSettle);
    lua_pushcfunction(lua, api.trackerFps);
    lua_pushcfunction(lua, api.trackerUnknown);
    lua_pushcfunction(lua, api.trackerWhere);
    lua_pushcfunction(lua, api.trackerPending);
    lua_pushcfunction(lua, _trackTiming);
    lua_call(lua, 8, 1);
    lua_setfield(lua, LUA_REGISTRYINDEX, TRACKER_FLUSH);
    // Last, so the tracker prelude above reads globals without the guard.
    lua_pushglobaltable(lua);
    _guard(lua, "");
    lua_pop(lua, 1);
    return 0;
}

}  // namespace vis
