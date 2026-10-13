#ifndef VIS_LUA_EXTENSION_H
#define VIS_LUA_EXTENSION_H

#include <cstddef>
#include <cstdint>

#include "vis.h"

struct lua_State;

namespace vis::detail
{

struct LuaHooks
{
    using Open = int (*)(lua_State*, void*);
    using Loaded = Result (*)(lua_State*, Scene*, void*) noexcept;

    Open open = nullptr;
    Loaded loaded = nullptr;
    void* data = nullptr;
};

struct LuaHookSpan
{
    const LuaHooks* data = nullptr;
    uint32_t count = 0;
};

Result luaLoad(const char* source, uint32_t size, const char* name, Scene** output,
               char* error, uint32_t errorSize, Lua::AssetResolver resolver,
               void* resolverData, const char* sourcePath, size_t sourceRoot,
               const Theme* adaptiveTheme, bool* adaptiveThemeUsed,
               const LuaHookSpan& hooks = {}, lua_State** retained = nullptr) noexcept;
Result luaLoadFile(const char* path, Scene** output, char* error, uint32_t errorSize,
                   const LuaHookSpan& hooks = {}, lua_State** retained = nullptr) noexcept;
bool luaAuthoring(lua_State* lua) noexcept;
void luaClose(lua_State* lua) noexcept;

Scene* luaScene(lua_State* lua, int index);
Object* luaObject(lua_State* lua, Scene* scene, int index);
AnimCurve luaAnimCurve(lua_State* lua, int index);
using LuaObjectTransfer = void (*)(void* data) noexcept;
// The optional transfer callback runs only after a protected Lua owner exists.
// It lets a module relinquish a detached builder tree without leaking on Lua OOM
// or leaving two native owners during budget/Scene-attachment failures.
// extraNativeBytes accounts for root-owned sidecar data not visited by the Object tree.
int luaAdoptObject(lua_State* lua, int sceneIndex, Object* root, const char* kind,
                   LuaObjectTransfer transfer = nullptr, void* transferData = nullptr,
                   size_t extraNativeBytes = 0u);
int luaPushObject(lua_State* lua, int sceneIndex, Object* object);
// Attaches a detached tree below handle 1, applying styleIndex options and scene budgets.
// The new handle owns root before any Lua error; transfer releases the module's ownership.
// Pushes the root handle on success.
int luaAddObject(lua_State* lua, Object* root, int styleIndex, const char* kind,
                 LuaObjectTransfer transfer = nullptr, void* transferData = nullptr);
// The scene of the scene or object handle at index, whose handle it pushes, so that a module
// method can measure text or push handles before or after it attaches what it makes.
Scene* luaHandleScene(lua_State* lua, int index);

// The handles a method belongs to: the scene's, every object's (spaces and groups too), or
// only groups'.
enum struct LuaHandle : uint8_t
{
    Scene = 0,
    Object,
    Group
};

// Pops the value on top of the stack and makes it the method name of the handles, in place of
// any method of that name, so a module can add a factory such as group:tex{...} or wrap a
// method it got with luaGetMethod. Call it from an open hook.
void luaSetMethod(lua_State* lua, LuaHandle handle, const char* name);
// Pushes the method name of the handles, or nil.
void luaGetMethod(lua_State* lua, LuaHandle handle, const char* name);

// Calls a function as lua_call does, from a method that a script called. A function called
// from C has no line of the script to name, so the message of an error it raises gets the line
// of the script's call here, as an error luaL_error raises in the method itself would.
void luaCall(lua_State* lua, int arguments, int results);
// Gives the values on the stack to the function at index, an upvalue or registry index such as
// the method luaGetMethod pushed for a wrapper to keep, and returns its results, as if the
// script had called it in place of the running function: its errors name the script's line,
// and a bad argument the name the script called.
int luaCallWrapped(lua_State* lua, int index);
// Registers the popped spans-part factory for key from an open hook.
// spans calls maker(handle, part, setting); setting supplies inherited size/font/role
// and row orientation. Parts remain indivisible on the line.
// Maker returns width, ascent, descent, ink top/bottom and place.
// place(row, style, x, y, z) attaches one object at the part's baseline and returns its handle.
void luaSetSpanPart(lua_State* lua, const char* key);

}  // namespace vis::detail

#endif
