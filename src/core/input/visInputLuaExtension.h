// The input module's hooks for the core's Lua loader (see visLuaExtension.h).
#ifndef _VIS_INPUT_LUA_EXTENSION_H_
#define _VIS_INPUT_LUA_EXTENSION_H_

#include "../lua/visLuaExtension.h"

namespace vis::input
{
struct Controller;
enum struct Key : uint8_t;
}

namespace vis::input::lua_extension
{

struct LuaContext
{
    Controller* controller = nullptr;
    Scene* scene = nullptr;
};

vis::detail::LuaHooks luaHooks(LuaContext& context) noexcept;
// The key the name at index stands for, in key_move and the runtime's key bindings: an arrow,
// Space, Enter, Escape, Plus, Minus, Shift, Tab, a letter or a digit.
Key luaKey(lua_State* lua, int index);

}  // namespace vis::input::lua_extension

#endif
