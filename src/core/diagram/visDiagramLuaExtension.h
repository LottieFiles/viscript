// The diagram module's hooks for the core's Lua loader (see visLuaExtension.h).
#ifndef _VIS_DIAGRAM_LUA_EXTENSION_H_
#define _VIS_DIAGRAM_LUA_EXTENSION_H_

#include "../lua/visLuaExtension.h"

namespace vis::diagram::lua_extension
{

vis::detail::LuaHooks luaHooks() noexcept;

}  // namespace vis::diagram::lua_extension

#endif
