#ifndef VIS_AUDIO_LUA_EXTENSION_H
#define VIS_AUDIO_LUA_EXTENSION_H

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
