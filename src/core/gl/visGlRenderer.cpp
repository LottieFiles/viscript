// gl::Renderer on ThorVG's GL engine: keeps a scene's retained paint tree on a GL canvas and
// draws it into the host's framebuffer (experimental).
#include <cmath>
#include <new>

#include <thorvg.h>

#include "vis_gl.h"
#include "visTvgRenderer.h"

namespace vis::gl
{

struct Renderer::Impl
{
    tvg::GlCanvas* canvas = nullptr;
    RetainedTree* tree = nullptr;
    Target target = {};
    bool targeted = false;
    bool initialized = false;
    bool antialiasing = true;
    renderer::TvgDiagnostic diagnostic;
};

Renderer::Renderer() noexcept
{
    pImpl = new (std::nothrow) Impl;
    if (!pImpl) return;
    pImpl->initialized = renderer::tvgInit();
    pImpl->tree = renderer::tvgRetainedGen();
    if (!pImpl->tree && pImpl->initialized) {
        renderer::tvgTerm();
        pImpl->initialized = false;
    }
}

Renderer::~Renderer()
{
    if (!pImpl) return;
    renderer::tvgRetainedFree(pImpl->tree);
    delete pImpl->canvas;
    if (pImpl->initialized) renderer::tvgTerm();
    delete pImpl;
}

Renderer* Renderer::gen() noexcept
{
    auto renderer = new (std::nothrow) Renderer;
    if (!renderer || (renderer->pImpl && renderer->pImpl->initialized)) return renderer;
    delete renderer;
    return nullptr;
}

Result Renderer::font(const char* path) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    return renderer::tvgFont(pImpl->diagnostic, path);
}

Result Renderer::font(const char* name, const void* data, uint32_t size, const char* mime) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    return renderer::tvgFont(pImpl->diagnostic, name, data, size, mime);
}

const char* Renderer::diagnostic() const noexcept
{
    return pImpl ? pImpl->diagnostic.text() : nullptr;
}

Result Renderer::render(const Scene* scene, float time, const Target& target) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!pImpl->initialized || !scene || !std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    // A scene is never empty, so neither is a target of its size.
    auto& config = scene->config();
    if (!target.context || target.id < 0 || target.width != config.width || target.height != config.height) {
        return Result::InvalidArguments;
    }
    if (!pImpl->canvas || pImpl->antialiasing != config.antialiasing) {
        renderer::tvgRetainedReset(pImpl->tree);
        delete pImpl->canvas;
        pImpl->canvas = tvg::GlCanvas::gen(config.antialiasing ? tvg::EngineOption::Default
                                                               : tvg::EngineOption::Aliased);
        pImpl->antialiasing = config.antialiasing;
        pImpl->targeted = false;
        if (!pImpl->canvas) return Result::NonSupport;
    }
    // Retargeting damages every retained paint, so only do it when the target changes.
    auto& last = pImpl->target;
    if (!pImpl->targeted || last.display != target.display || last.surface != target.surface
        || last.context != target.context || last.id != target.id || last.width != target.width
        || last.height != target.height) {
        pImpl->targeted = false;
        if (!renderer::tvgSuccess(pImpl->canvas->target(target.display, target.surface, target.context, target.id, target.width, target.height, tvg::ColorSpace::ABGR8888S))) {
            return Result::NonSupport;
        }
        pImpl->target = target;
        pImpl->targeted = true;
    }
    return renderer::tvgRetainedDraw(pImpl->tree, pImpl->canvas, scene, time, 1.0f);
}

}  // namespace vis::gl
