#ifndef VIS_INPUT_LUA_EXTENSION_H
#define VIS_INPUT_LUA_EXTENSION_H

#include "../lua/visLuaExtension.h"

namespace vis::input
{
struct Controller;
}

namespace vis::input::lua_extension
{

struct LuaContext
{
    Controller* controller = nullptr;
    Scene* scene = nullptr;
};

vis::detail::LuaHooks luaHooks(LuaContext& context) noexcept;

}  // namespace vis::input::lua_extension

#endif
