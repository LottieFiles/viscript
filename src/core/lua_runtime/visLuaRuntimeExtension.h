#ifndef VIS_LUA_RUNTIME_EXTENSION_H
#define VIS_LUA_RUNTIME_EXTENSION_H

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
