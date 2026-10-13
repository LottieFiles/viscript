#ifndef VIS_UI_LUA_EXTENSION_H
#define VIS_UI_LUA_EXTENSION_H

#include "../lua/visLuaExtension.h"

namespace vis::ui
{
struct Panel;
}

namespace vis::ui::lua_extension
{

struct LuaContext
{
    Panel* panel = nullptr;
    Scene* scene = nullptr;
};

vis::detail::LuaHooks luaHooks(LuaContext& context) noexcept;

}  // namespace vis::ui::lua_extension

#endif
