// The states module's hooks for the core's Lua loader (see visLuaExtension.h).
#ifndef _VIS_STATES_LUA_EXTENSION_H_
#define _VIS_STATES_LUA_EXTENSION_H_

#include "../lua/visLuaExtension.h"

namespace vis::states
{

struct Controller;

namespace lua_extension
{

struct LuaContext
{
    Controller* controller = nullptr;
    Scene* scene = nullptr;
};

vis::detail::LuaHooks luaHooks(LuaContext& context) noexcept;

}  // namespace lua_extension

}  // namespace vis::states

#endif
