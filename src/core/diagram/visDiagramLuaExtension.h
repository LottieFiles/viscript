#ifndef VIS_DIAGRAM_LUA_EXTENSION_H
#define VIS_DIAGRAM_LUA_EXTENSION_H

#include "../lua/visLuaExtension.h"

namespace vis::diagram::lua_extension
{

vis::detail::LuaHooks luaHooks() noexcept;

}  // namespace vis::diagram::lua_extension

#endif
