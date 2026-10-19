// The Lua scenes of the tests, loaded as the CLI and WASM hosts load them: through the host
// loader, with the hooks of each module the test is built with.
#ifndef _VIS_TEST_LUA_LOAD_H_
#define _VIS_TEST_LUA_LOAD_H_

#include <cstdio>
#include <cstring>

#include "visLuaHost.h"
#include "testCheck.h"

// Loads source as the script name. context holds what the modules made once the load succeeds;
// give each load a fresh one.
static inline vis::Result _load(const char* source, const char* name, vis::Scene*& scene, char* error,
                                uint32_t errorSize, vis::detail::LuaHostContext& context,
                                vis::Lua::AssetResolver resolver = nullptr, void* resolverData = nullptr)
{
    return vis::detail::luaHostLoad(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error,
                                    errorSize, resolver, resolverData, nullptr, nullptr, context);
}

// The scene a script makes. A script that does not load fails the check and says why.
static inline vis::Scene* _loaded(const char* source, const char* name, vis::detail::LuaHostContext& context,
                                  vis::Lua::AssetResolver resolver = nullptr, void* resolverData = nullptr)
{
    char error[1024] = {};
    vis::Scene* scene = nullptr;
    auto status = _load(source, name, scene, error, sizeof(error), context, resolver, resolverData);
    CHECK(status == vis::Result::Success && scene);
    if (status != vis::Result::Success) std::fprintf(stderr, "%s: %s\n", name, error);
    return scene;
}

static inline vis::Scene* _loaded(const char* source, const char* name, vis::Lua::AssetResolver resolver = nullptr,
                                  void* resolverData = nullptr)
{
    vis::detail::LuaHostContext context;
    return _loaded(source, name, context, resolver, resolverData);
}

// The scene the script file at path makes, with its assets resolved beside the file.
static inline vis::Scene* _loadedFile(const char* path)
{
    char error[1024] = {};
    vis::Scene* scene = nullptr;
    vis::detail::LuaHostContext context;
    auto status = vis::detail::luaHostLoadFile(path, &scene, error, sizeof(error), context);
    CHECK(status == vis::Result::Success && scene);
    if (status != vis::Result::Success) std::fprintf(stderr, "%s: %s\n", path, error);
    return scene;
}

// The script must fail, and its message must name the script at the line.
static inline void _error(const char* source, const char* name, vis::Lua::AssetResolver resolver = nullptr,
                          void* resolverData = nullptr)
{
    char error[1024] = {};
    vis::Scene* scene = nullptr;
    vis::detail::LuaHostContext context;
    auto status = _load(source, name, scene, error, sizeof(error), context, resolver, resolverData);
    CHECK(status == vis::Result::ScriptError);
    CHECK(scene == nullptr);
    CHECK(error[0] != '\0');
    auto location = std::strstr(error, name);
    CHECK(location != nullptr);
    if (!location) std::fprintf(stderr, "  %s does not name its script: %s\n", name, error);
    if (location) CHECK(location[std::strlen(name)] == ':');
    delete scene;
}

// The script name must fail and the message must explain why.
static inline void _scriptError(const char* source, const char* name, const char* expected)
{
    char error[1024] = {};
    vis::Scene* scene = nullptr;
    vis::detail::LuaHostContext context;
    auto status = _load(source, name, scene, error, sizeof(error), context);
    CHECK(status == vis::Result::ScriptError && scene == nullptr);
    CHECK(std::strstr(error, expected) != nullptr);
    if (!std::strstr(error, expected)) std::fprintf(stderr, "  %s: expected '%s', got: %s\n", name, expected, error);
    delete scene;
}

static inline void _scriptError(const char* source, const char* expected)
{
    _scriptError(source, "diagnostic.lua", expected);
}

// The script must load.
static inline void _scriptLoads(const char* source)
{
    delete _loaded(source, "loads.lua");
}

#endif
