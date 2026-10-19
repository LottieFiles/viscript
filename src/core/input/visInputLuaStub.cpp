// The input module's Lua::load in a build without Lua: reports NonSupport.
#include <cstdio>

#include "vis_input.h"

namespace vis::input
{

Result Lua::load(const char*, uint32_t, const char*, Scene** scene,
                 Controller** controller, char* error, uint32_t errorSize,
                 vis::Lua::AssetResolver, void*, const Theme*,
                 bool* adaptiveThemeUsed) noexcept
{
    if (scene) *scene = nullptr;
    if (controller) *controller = nullptr;
    if (adaptiveThemeUsed) *adaptiveThemeUsed = false;
    if (error && errorSize) std::snprintf(error, errorSize, "Lua support is disabled");
    return scene ? Result::NonSupport : Result::InvalidArguments;
}

}  // namespace vis::input
