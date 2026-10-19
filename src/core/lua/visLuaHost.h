// Loads a scene for a host with every optional module this build has: their Lua hooks, the
// context they fill while loading, and the runtime that may keep the Lua state alive.
#ifndef _VIS_LUA_HOST_H_
#define _VIS_LUA_HOST_H_

#include "visLuaExtension.h"

#if defined(VIS_AUDIO)
#include "../audio/visAudioLuaExtension.h"
#endif
#if defined(VIS_DIAGRAM)
#include "../diagram/visDiagramLuaExtension.h"
#endif
#if defined(VIS_STATES)
#include "../states/visStatesLuaExtension.h"
#endif
#if defined(VIS_TEX)
#include "../tex/visTexLuaExtension.h"
#endif
#if defined(VIS_UI)
#include "../ui/visUiLuaExtension.h"
#endif
#if defined(VIS_INPUT)
#include "../input/visInputLuaExtension.h"
#endif
#if defined(VIS_LUA_RUNTIME)
#include "../lua_runtime/visLuaRuntimeExtension.h"
#endif

namespace vis::detail
{

struct LuaHostContext
{
#if defined(VIS_AUDIO)
    audio::lua_extension::LuaContext audio;
#endif
#if defined(VIS_UI)
    ui::lua_extension::LuaContext ui;
#endif
#if defined(VIS_INPUT)
    input::lua_extension::LuaContext input;
#endif
#if defined(VIS_LUA_RUNTIME)
    lua_runtime::lua_extension::LuaContext runtime;
#endif
#if defined(VIS_STATES)
    states::lua_extension::LuaContext states;
#endif
};

inline void luaHostResetFailedLoad(LuaHostContext& context) noexcept
{
#if defined(VIS_STATES)
    context.states = {};
#else
    (void) context;
#endif
}

// One set of hooks for each optional module.
constexpr uint32_t LUA_HOST_HOOK_LIMIT = 7u;

inline LuaHookSpan luaHostHooks(LuaHostContext& context,
                                LuaHooks (&storage)[LUA_HOST_HOOK_LIMIT]) noexcept
{
    uint32_t count = 0u;
#if defined(VIS_AUDIO)
    storage[count++] = audio::lua_extension::luaHooks(context.audio);
#endif
#if defined(VIS_DIAGRAM)
    storage[count++] = diagram::lua_extension::luaHooks();
#endif
#if defined(VIS_TEX)
    storage[count++] = tex::lua_extension::luaHooks();
#endif
#if defined(VIS_STATES)
    storage[count++] = states::lua_extension::luaHooks(context.states);
#endif
#if defined(VIS_INPUT)
    storage[count++] = input::lua_extension::luaHooks(context.input);
#endif
#if defined(VIS_UI)
    storage[count++] = ui::lua_extension::luaHooks(context.ui);
#endif
#if defined(VIS_LUA_RUNTIME)
    storage[count++] = lua_runtime::lua_extension::luaHooks(context.runtime);
#endif
#if !defined(VIS_AUDIO) && !defined(VIS_UI) && !defined(VIS_INPUT) \
    && !defined(VIS_LUA_RUNTIME) && !defined(VIS_STATES)
    (void) context;
#endif
    return {storage, count};
}

inline Result luaHostLoad(const char* source, uint32_t size, const char* name,
                          Scene** output, char* error, uint32_t errorSize,
                          Lua::AssetResolver resolver, void* resolverData,
                          const Theme* adaptiveTheme, bool* adaptiveThemeUsed,
                          LuaHostContext& context) noexcept
{
    LuaHooks storage[LUA_HOST_HOOK_LIMIT];
    auto hooks = luaHostHooks(context, storage);
#if defined(VIS_LUA_RUNTIME)
    lua_State* retained = nullptr;
    auto result = luaLoad(source, size, name, output, error, errorSize, resolver,
                          resolverData, nullptr, 0u, adaptiveTheme,
                          adaptiveThemeUsed, hooks, &retained);
    if (result != Result::Success) {
        luaHostResetFailedLoad(context);
        return result;
    }
    if (context.runtime.runtime) {
        result = lua_runtime::lua_extension::retain(context.runtime, retained);
        if (result == Result::Success) return result;
        luaClose(retained);
        delete *output;
        *output = nullptr;
        luaHostResetFailedLoad(context);
        return result;
    }
    luaClose(retained);
    return result;
#else
    auto result = luaLoad(source, size, name, output, error, errorSize, resolver,
                          resolverData, nullptr, 0u, adaptiveTheme,
                          adaptiveThemeUsed, hooks);
    if (result != Result::Success) luaHostResetFailedLoad(context);
    return result;
#endif
}

inline Result luaHostLoadFile(const char* path, Scene** output, char* error,
                              uint32_t errorSize, LuaHostContext& context) noexcept
{
    LuaHooks storage[LUA_HOST_HOOK_LIMIT];
    auto hooks = luaHostHooks(context, storage);
#if defined(VIS_LUA_RUNTIME)
    lua_State* retained = nullptr;
    auto result = luaLoadFile(path, output, error, errorSize, hooks, &retained);
    if (result != Result::Success) {
        luaHostResetFailedLoad(context);
        return result;
    }
    if (context.runtime.runtime) {
        result = lua_runtime::lua_extension::retain(context.runtime, retained);
        if (result == Result::Success) return result;
        luaClose(retained);
        delete *output;
        *output = nullptr;
        luaHostResetFailedLoad(context);
        return result;
    }
    luaClose(retained);
    return result;
#else
    auto result = luaLoadFile(path, output, error, errorSize, hooks);
    if (result != Result::Success) luaHostResetFailedLoad(context);
    return result;
#endif
}

}  // namespace vis::detail

#endif
