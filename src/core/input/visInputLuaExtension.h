// The input module's hooks for the core's Lua loader (see visLuaExtension.h).
#ifndef _VIS_INPUT_LUA_EXTENSION_H_
#define _VIS_INPUT_LUA_EXTENSION_H_

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
