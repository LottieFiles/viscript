#include <cstdio>

#include "vis.h"

namespace vis
{

static Result _unsupported(Scene** scene, char* error, uint32_t errorSize) noexcept
{
    if (scene) *scene = nullptr;
    if (error && errorSize) std::snprintf(error, errorSize, "Lua support is disabled");
    return scene ? Result::NonSupport : Result::InvalidArguments;
}

Result Lua::load(const char*, Scene** scene, char* error, uint32_t errorSize) noexcept
{
    return _unsupported(scene, error, errorSize);
}

Result Lua::load(const char*, uint32_t, const char*, Scene** scene, char* error, uint32_t errorSize,
                 AssetResolver, void*, const Theme*, bool* adaptiveThemeUsed) noexcept
{
    if (adaptiveThemeUsed) *adaptiveThemeUsed = false;
    return _unsupported(scene, error, errorSize);
}

}  // namespace vis
