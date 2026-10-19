// GlRenderer in a build without the GL engine: glEnabled() is false.
#include "vis.h"
#include "visRendererBackend.h"

namespace vis
{

struct GlRenderer::Impl
{
};

GlRenderer::GlRenderer() noexcept = default;

GlRenderer::~GlRenderer()
{
    delete pImpl;
}

GlRenderer* GlRenderer::gen() noexcept
{
    return nullptr;
}

Result GlRenderer::font(const char* path) noexcept
{
    return path ? Result::NonSupport : Result::InvalidArguments;
}

Result GlRenderer::font(const char* name, const void* data, uint32_t size, const char* mime) noexcept
{
    if (!name || !name[0] || !data || !size || !mime || !mime[0]) return Result::InvalidArguments;
    return Result::NonSupport;
}

const char* GlRenderer::diagnostic() const noexcept
{
    return nullptr;
}

Result GlRenderer::render(const Scene*, float, const GlTarget&) noexcept
{
    return Result::NonSupport;
}

namespace renderer
{

bool glEnabled() noexcept
{
    return false;
}

GlRenderer* genGl() noexcept
{
    return nullptr;
}

}  // namespace renderer

}  // namespace vis
