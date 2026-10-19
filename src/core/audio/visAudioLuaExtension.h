#ifndef _VIS_AUDIO_LUA_EXTENSION_H_
#define _VIS_AUDIO_LUA_EXTENSION_H_

#include "../lua/visLuaExtension.h"

namespace vis::audio
{

struct Soundscape;

namespace lua_extension
{

struct LuaContext
{
    Soundscape* soundscape = nullptr;
    Scene* scene = nullptr;
};

vis::detail::LuaHooks luaHooks(LuaContext& context) noexcept;

}  // namespace lua_extension

}  // namespace vis::audio

#endif
