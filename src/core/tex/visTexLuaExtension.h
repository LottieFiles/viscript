#ifndef VIS_TEX_LUA_EXTENSION_H
#define VIS_TEX_LUA_EXTENSION_H

#include "../lua/visLuaExtension.h"

namespace vis::tex::lua_extension
{

vis::detail::LuaHooks luaHooks() noexcept;

}  // namespace vis::tex::lua_extension

#endif
