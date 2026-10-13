#include <cmath>
#include <new>

#include <thorvg.h>

#include "vis.h"
#include "visRendererBackend.h"
#include "visScene.h"
#include "visTvgRenderer.h"

namespace vis
{

struct GlRenderer::Impl
{
    tvg::GlCanvas* canvas = nullptr;
    RetainedTree* tree = nullptr;
    GlTarget target = {};
    bool targeted = false;
    bool initialized = false;
    bool antialiasing = true;
    renderer::TvgDiagnostic diagnostic;
};

GlRenderer::GlRenderer() noexcept
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

GlRenderer::~GlRenderer()
{
    if (pImpl) {
        renderer::tvgRetainedFree(pImpl->tree);
        delete pImpl->canvas;
    }
    if (pImpl && pImpl->initialized) renderer::tvgTerm();
    delete pImpl;
}

GlRenderer* GlRenderer::gen() noexcept
{
    auto renderer = new (std::nothrow) GlRenderer;
    if (!renderer || (renderer->pImpl && renderer->pImpl->initialized)) return renderer;
    delete renderer;
    return nullptr;
}

Result GlRenderer::font(const char* path) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    return renderer::tvgFont(pImpl->diagnostic, path);
}

Result GlRenderer::font(const char* name, const void* data, uint32_t size, const char* mime) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    return renderer::tvgFont(pImpl->diagnostic, name, data, size, mime);
}

const char* GlRenderer::diagnostic() const noexcept
{
    return pImpl ? pImpl->diagnostic.text() : nullptr;
}

Result GlRenderer::render(const Scene* scene, float time, const GlTarget& target) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!pImpl->initialized || !scene || !scene->pImpl
        || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    if (!target.context || target.id < 0 || !target.width || !target.height) {
        return Result::InvalidArguments;
    }
    if (target.width != scene->pImpl->cfg.width || target.height != scene->pImpl->cfg.height) return Result::InvalidArguments;
    if (!pImpl->canvas || pImpl->antialiasing != scene->pImpl->cfg.antialiasing) {
        renderer::tvgRetainedReset(pImpl->tree);
        delete pImpl->canvas;
        pImpl->canvas = tvg::GlCanvas::gen(scene->pImpl->cfg.antialiasing ? tvg::EngineOption::Default
                                                                          : tvg::EngineOption::Aliased);
        pImpl->antialiasing = scene->pImpl->cfg.antialiasing;
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
    auto result = renderer::tvgRetainedSync(pImpl->tree, pImpl->canvas, scene, time, 1.0f);
    if (result != Result::Success) return result;
    if (!renderer::tvgSuccess(pImpl->canvas->update())) return Result::Unknown;
    if (!renderer::tvgSuccess(pImpl->canvas->draw(true))) return Result::Unknown;
    if (!renderer::tvgSuccess(pImpl->canvas->sync())) return Result::Unknown;
    return Result::Success;
}

namespace renderer
{

bool glEnabled() noexcept
{
    return true;
}

GlRenderer* genGl() noexcept
{
    return GlRenderer::gen();
}

}  // namespace renderer

}  // namespace vis
