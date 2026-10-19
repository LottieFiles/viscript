// Renderer: the engine chosen per instance or by default, forwarding to the CPU or GL backend.
#include <atomic>
#include <new>

#include "vis.h"
#include "visRendererBackend.h"

namespace vis
{

static constexpr uint8_t ENGINE_UNSET = 0xffu;
static std::atomic<uint8_t> _defaultEngine{ENGINE_UNSET};

static constexpr auto CPU_ONLY = "bounds, intersects, layout, sample and surface rendering need the CPU engine";
static constexpr auto GL_ONLY = "rendering to a GL target needs the GL engine";

static bool _valid(RenderEngine engine)
{
    return engine == RenderEngine::Cpu || engine == RenderEngine::Gl;
}

struct Renderer::Impl
{
    SwRenderer* cpu = nullptr;
    GlRenderer* gl = nullptr;
    RenderEngine type = RenderEngine::Cpu;
    // Set when the most recent call needed the other engine.
    const char* mismatch = nullptr;
};

Renderer::Renderer(RenderEngine engine) noexcept
{
    pImpl = new (std::nothrow) Impl;
    if (!pImpl) return;
    pImpl->type = engine;
    if (engine == RenderEngine::Cpu) pImpl->cpu = renderer::genCpu();
    else if (engine == RenderEngine::Gl) pImpl->gl = renderer::genGl();
}

Renderer::~Renderer()
{
    if (pImpl) {
        delete pImpl->cpu;
        delete pImpl->gl;
    }
    delete pImpl;
}

Renderer* Renderer::gen() noexcept
{
    return gen(defaultEngine());
}

Renderer* Renderer::gen(RenderEngine engine) noexcept
{
    if (!_valid(engine) || !enabled(engine)) return nullptr;
    auto renderer = new (std::nothrow) Renderer(engine);
    if (renderer && renderer->pImpl
        && ((engine == RenderEngine::Cpu && renderer->pImpl->cpu)
            || (engine == RenderEngine::Gl && renderer->pImpl->gl))) {
        return renderer;
    }
    delete renderer;
    return nullptr;
}

Result Renderer::defaultEngine(RenderEngine engine) noexcept
{
    if (!_valid(engine)) return Result::InvalidArguments;
    if (!enabled(engine)) return Result::NonSupport;
    _defaultEngine.store(static_cast<uint8_t>(engine), std::memory_order_relaxed);
    return Result::Success;
}

RenderEngine Renderer::defaultEngine() noexcept
{
    auto value = _defaultEngine.load(std::memory_order_relaxed);
    if (value != ENGINE_UNSET) return static_cast<RenderEngine>(value);

    auto engine = renderer::cpuEnabled() ? RenderEngine::Cpu : RenderEngine::Gl;
    auto expected = ENGINE_UNSET;
    if (_defaultEngine.compare_exchange_strong(expected, static_cast<uint8_t>(engine),
                                               std::memory_order_relaxed)) {
        return engine;
    }
    return static_cast<RenderEngine>(expected);
}

bool Renderer::enabled(RenderEngine engine) noexcept
{
    switch (engine) {
        case RenderEngine::Cpu: return renderer::cpuEnabled();
        case RenderEngine::Gl: return renderer::glEnabled();
    }
    return false;
}

RenderEngine Renderer::engine() const noexcept
{
    return pImpl->type;
}

Result Renderer::font(const char* path) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = nullptr;
    if (pImpl->cpu) return pImpl->cpu->font(path);
    if (pImpl->gl) return pImpl->gl->font(path);
    return Result::NonSupport;
}

Result Renderer::font(const char* name, const void* data, uint32_t size,
                      const char* mime) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = nullptr;
    if (pImpl->cpu) return pImpl->cpu->font(name, data, size, mime);
    if (pImpl->gl) return pImpl->gl->font(name, data, size, mime);
    return Result::NonSupport;
}

Result Renderer::bounds(const Scene* scene, const Object* object, float time,
                        BBox& output) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = pImpl->cpu ? nullptr : CPU_ONLY;
    if (!pImpl->cpu) return Result::NonSupport;
    return pImpl->cpu->bounds(scene, object, time, output);
}

Result Renderer::intersects(const Scene* scene, const Object* first, const Object* second,
                            float time, bool& output, float padding) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = pImpl->cpu ? nullptr : CPU_ONLY;
    if (!pImpl->cpu) return Result::NonSupport;
    return pImpl->cpu->intersects(scene, first, second, time, output, padding);
}

Result Renderer::layout(const Scene* scene, float time, LayoutReport& output,
                        float padding) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = pImpl->cpu ? nullptr : CPU_ONLY;
    if (!pImpl->cpu) return Result::NonSupport;
    return pImpl->cpu->layout(scene, time, output, padding);
}

Result Renderer::sample(const Scene* scene, float time, const Vec2& position,
                        const Object* const* candidates, uint32_t count,
                        PixelSample& output, uint32_t pixelRatio) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = pImpl->cpu ? nullptr : CPU_ONLY;
    if (!pImpl->cpu) return Result::NonSupport;
    return pImpl->cpu->sample(scene, time, position, candidates, count, output, pixelRatio);
}

Result Renderer::render(const Scene* scene, float time, Surface& surface) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = pImpl->cpu ? nullptr : CPU_ONLY;
    if (!pImpl->cpu) return Result::NonSupport;
    return pImpl->cpu->render(scene, time, surface);
}

Result Renderer::render(const Scene* scene, float time, Surface& surface,
                        uint32_t pixelRatio) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = pImpl->cpu ? nullptr : CPU_ONLY;
    if (!pImpl->cpu) return Result::NonSupport;
    return pImpl->cpu->render(scene, time, surface, pixelRatio);
}

Result Renderer::render(const Scene* scene, float time, const GlTarget& target) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    pImpl->mismatch = pImpl->gl ? nullptr : GL_ONLY;
    if (!pImpl->gl) return Result::NonSupport;
    return pImpl->gl->render(scene, time, target);
}

const char* Renderer::diagnostic() const noexcept
{
    if (!pImpl) return nullptr;
    if (pImpl->mismatch) return pImpl->mismatch;
    if (pImpl->cpu) return pImpl->cpu->diagnostic();
    if (pImpl->gl) return pImpl->gl->diagnostic();
    return nullptr;
}

SwRenderer* Renderer::cpuBackend() const noexcept
{
    return pImpl ? pImpl->cpu : nullptr;
}

}  // namespace vis
