// The runtime module's hooks for the core's Lua loader (see visLuaExtension.h).
#ifndef _VIS_LUA_RUNTIME_EXTENSION_H_
#define _VIS_LUA_RUNTIME_EXTENSION_H_

#include "../lua/visLuaExtension.h"

namespace vis::lua_runtime
{
struct Runtime;
}

namespace vis::lua_runtime::lua_extension
{

struct LuaContext
{
    Runtime* runtime = nullptr;
    Scene* scene = nullptr;
};

vis::detail::LuaHooks luaHooks(LuaContext& context) noexcept;
Result retain(LuaContext& context, lua_State* lua) noexcept;

}  // namespace vis::lua_runtime::lua_extension

#endif
