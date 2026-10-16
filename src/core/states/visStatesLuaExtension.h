#ifndef VIS_STATES_LUA_EXTENSION_H
#define VIS_STATES_LUA_EXTENSION_H

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
