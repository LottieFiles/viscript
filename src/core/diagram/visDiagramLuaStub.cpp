#include <cstdio>

#include "vis_diagram.h"

namespace vis::diagram
{

Result Lua::load(const char*, uint32_t, const char*, Scene** scene, char* error,
                 uint32_t errorSize, vis::Lua::AssetResolver, void*, const Theme*,
                 bool* adaptiveThemeUsed) noexcept
{
    if (scene) *scene = nullptr;
    if (adaptiveThemeUsed) *adaptiveThemeUsed = false;
    if (error && errorSize) std::snprintf(error, errorSize, "Lua support is disabled");
    return scene ? Result::NonSupport : Result::InvalidArguments;
}

}  // namespace vis::diagram
