#ifndef _VIS_TEX_LUA_EXTENSION_H_
#define _VIS_TEX_LUA_EXTENSION_H_

#include "../lua/visLuaExtension.h"

namespace vis::tex::lua_extension
{

vis::detail::LuaHooks luaHooks() noexcept;

}  // namespace vis::tex::lua_extension

#endif
