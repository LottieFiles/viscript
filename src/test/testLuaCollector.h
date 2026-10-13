#ifndef VIS_TEST_LUA_COLLECTOR_H
#define VIS_TEST_LUA_COLLECTOR_H

#include <cstring>

extern "C" {
#include <lua.h>
}

#include "visLuaExtension.h"

// An open hook under which a binding that reads memory the collector may free fails every time,
// instead of only when a cycle happens to end at the wrong moment, as a Lua built with
// HARDMEMTESTS would make it. The collector finishes a whole cycle wherever it may run once
// anything has been allocated, and fills what it frees with 0xff, a NaN in every float and no
// end to a string, so a pointer kept into a value that left the stack reads garbage at once.

// The allocator the hook wraps, of the last state it opened. The core reads back only its data,
// the memory budget of the state, which the wrapper keeps.
inline lua_Alloc collectedAllocator = nullptr;

inline void* collectedAllocate(void* data, void* block, size_t oldSize, size_t size)
{
    if (block && !size) std::memset(block, 0xff, oldSize);
    return collectedAllocator(data, block, oldSize, size);
}

inline int openCollecting(lua_State* lua, void*)
{
    void* data = nullptr;
    collectedAllocator = lua_getallocf(lua, &data);
    lua_setallocf(lua, collectedAllocate, data);
    // A pause of 0 starts a cycle as soon as anything is allocated, and a step size past the
    // largest finishes it in that step. lua_gc leaves a parameter of 0 as it was and keeps the
    // pause in quarters, so 1 sets it to 0. The pause counts from the end of a cycle, so one
    // runs now; the last one waited for the memory in use to double.
    lua_gc(lua, LUA_GCINC, 1, 0, 63);
    lua_gc(lua, LUA_GCCOLLECT);
    return 0;
}

// The hook to pass to detail::luaLoad, after any module's own.
inline vis::detail::LuaHooks collectingHooks()
{
    return {openCollecting, nullptr, nullptr};
}

#endif
