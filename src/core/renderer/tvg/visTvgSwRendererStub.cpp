#include "vis.h"
#include "visRendererBackend.h"

namespace vis
{

struct SwRenderer::Impl
{
};

SwRenderer::SwRenderer() noexcept = default;

SwRenderer::~SwRenderer()
{
    delete pImpl;
}

SwRenderer* SwRenderer::gen() noexcept
{
    return nullptr;
}

Result SwRenderer::font(const char* path) noexcept
{
    return path ? Result::NonSupport : Result::InvalidArguments;
}

Result SwRenderer::font(const char* name, const void* data, uint32_t size, const char* mime) noexcept
{
    if (!name || !name[0] || !data || !size || !mime || !mime[0]) return Result::InvalidArguments;
    return Result::NonSupport;
}

const char* SwRenderer::diagnostic() const noexcept
{
    return nullptr;
}

Result SwRenderer::bounds(const Scene*, const Object*, float, BBox&) noexcept
{
    return Result::NonSupport;
}

Result SwRenderer::intersects(const Scene*, const Object*, const Object*, float,
                              bool& output, float) noexcept
{
    output = false;
    return Result::NonSupport;
}

Result SwRenderer::layout(const Scene*, float, LayoutReport&, float) noexcept
{
    return Result::NonSupport;
}

Result SwRenderer::sample(const Scene*, float, const Vec2&, const Object* const*,
                          uint32_t, PixelSample&, uint32_t) noexcept
{
    return Result::NonSupport;
}

Result SwRenderer::render(const Scene*, float, Surface&) noexcept
{
    return Result::NonSupport;
}

Result SwRenderer::render(const Scene*, float, Surface&, uint32_t) noexcept
{
    return Result::NonSupport;
}

namespace renderer
{

bool cpuEnabled() noexcept
{
    return false;
}

SwRenderer* genCpu() noexcept
{
    return nullptr;
}

}  // namespace renderer

}  // namespace vis
