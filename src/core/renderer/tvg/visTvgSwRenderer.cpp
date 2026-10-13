#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

#include <thorvg.h>

#include "vis.h"
#include "visDisplayList.h"
#include "visRendererBackend.h"
#include "visRendererData.h"
#include "visScene.h"
#include "visTvgRenderer.h"

namespace vis
{

struct SwRenderer::Impl
{
    tvg::SwCanvas* canvas = nullptr;
    RetainedTree* tree = nullptr;
    uint32_t* buffer = nullptr;
    uint32_t stride = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    bool initialized = false;
    bool antialiasing = true;
    renderer::TvgDiagnostic diagnostic;
};

SwRenderer::SwRenderer() noexcept
{
    pImpl = new (std::nothrow) Impl;
    if (!pImpl) return;
    pImpl->initialized = renderer::tvgInit();
    if (!pImpl->initialized) return;
    pImpl->canvas = tvg::SwCanvas::gen();
    pImpl->tree = renderer::tvgRetainedGen();
    if (!pImpl->tree) {
        delete pImpl->canvas;
        pImpl->canvas = nullptr;
    }
}

SwRenderer::~SwRenderer()
{
    auto initialized = pImpl && pImpl->initialized;
    if (pImpl) {
        renderer::tvgRetainedFree(pImpl->tree);
        delete pImpl->canvas;
    }
    delete pImpl;
    if (initialized) renderer::tvgTerm();
}

SwRenderer* SwRenderer::gen() noexcept
{
    auto renderer = new (std::nothrow) SwRenderer;
    if (!renderer || (renderer->pImpl && renderer->pImpl->canvas)) return renderer;
    delete renderer;
    return nullptr;
}

Result SwRenderer::font(const char* path) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    return renderer::tvgFont(pImpl->diagnostic, path);
}

Result SwRenderer::font(const char* name, const void* data, uint32_t size, const char* mime) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    return renderer::tvgFont(pImpl->diagnostic, name, data, size, mime);
}

const char* SwRenderer::diagnostic() const noexcept
{
    return pImpl ? pImpl->diagnostic.text() : nullptr;
}

static bool _family(const Object* object, const Object* ancestor)
{
    while (object) {
        if (object == ancestor) return true;
        object = object->parent();
    }
    return false;
}

struct SamplePaint
{
    tvg::Paint* paint = nullptr;
    uint32_t order = 0u;
};

static Result _sampleFamily(tvg::SwCanvas* canvas, Surface& surface,
                            const DisplayList& list, const Object* candidate,
                            uint32_t pixelRatio, uint32_t pixelX, uint32_t pixelY,
                            SamplePaint* paints, Color& color, uint32_t& order,
                            bool& hit)
{
    hit = false;
    color = {0, 0, 0, 0};
    auto root = tvg::Scene::gen();
    if (!root) return Result::OutOfMemory;
    auto paintCount = 0u;
    for (auto i = 0u; i < list.count; i++) {
        auto& command = list.commands[i];
        if (!_family(command.object, candidate) || !renderer::tvgVisible(command) || list.masking(command)) {
            continue;
        }
        auto paint = renderer::tvgPaint(command);
        if (!paint) {
            renderer::tvgRelease(root);
            return Result::NonSupport;
        }
        paints[paintCount++] = {paint, i};
        if (renderer::tvgAdd(root, paint)) continue;
        renderer::tvgRelease(root);
        return Result::Unknown;
    }
    if (!paintCount) {
        renderer::tvgRelease(root);
        return Result::Success;
    }

    auto scale = static_cast<float>(pixelRatio);
    tvg::Matrix matrix = {
        scale, 0.0f, -static_cast<float>(pixelX),
        0.0f, scale, -static_cast<float>(pixelY),
        0.0f, 0.0f, 1.0f};
    if (!renderer::tvgSuccess(root->transform(matrix))) {
        renderer::tvgRelease(root);
        return Result::Unknown;
    }
    surface.data()[0] = 0u;
    if (!renderer::tvgSuccess(canvas->add(root))) {
        renderer::tvgRelease(root);
        return Result::Unknown;
    }
    auto result = Result::Success;
    if (!renderer::tvgSuccess(canvas->update()) || !renderer::tvgSuccess(canvas->draw(true)) || !renderer::tvgSuccess(canvas->sync())) {
        result = Result::Unknown;
    } else {
        auto rgba = reinterpret_cast<const uint8_t*>(surface.data());
        color = {rgba[0], rgba[1], rgba[2], rgba[3]};
        if (color.a) {
            if (paintCount == 1u) {
                order = paints[0].order;
                hit = true;
            } else {
                for (auto i = 0u; i < paintCount; i++) {
                    if (!renderer::tvgSuccess(paints[i].paint->visible(false))) {
                        result = Result::Unknown;
                        break;
                    }
                }
                for (auto i = paintCount; result == Result::Success && i > 0u; i--) {
                    auto paint = paints[i - 1u].paint;
                    if (!paint->intersects(0, 0, 1, 1)) continue;
                    surface.data()[0] = 0u;
                    if (!renderer::tvgSuccess(paint->visible(true)) || !renderer::tvgSuccess(canvas->update())
                        || !renderer::tvgSuccess(canvas->draw(true)) || !renderer::tvgSuccess(canvas->sync())) {
                        result = Result::Unknown;
                        break;
                    }
                    auto isolated = reinterpret_cast<const uint8_t*>(surface.data());
                    if (isolated[3]) {
                        order = paints[i - 1u].order;
                        hit = true;
                        break;
                    }
                    if (!renderer::tvgSuccess(paint->visible(false))) {
                        result = Result::Unknown;
                        break;
                    }
                }
            }
        }
    }
    if (!renderer::tvgSuccess(canvas->remove()) && result == Result::Success) result = Result::Unknown;
    return result;
}

Result SwRenderer::sample(const Scene* scene, float time, const Vec2& position,
                          const Object* const* candidates, uint32_t count,
                          PixelSample& output, uint32_t pixelRatio) noexcept
{
    output = {};
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!pImpl->initialized || !scene || !scene->pImpl || !candidates
        || !count || count > 64u || !pixelRatio || !std::isfinite(time) || time < 0.0f
        || !std::isfinite(position.x) || !std::isfinite(position.y)) {
        return Result::InvalidArguments;
    }
    auto& config = scene->pImpl->cfg;
    if (config.width > UINT32_MAX / pixelRatio || config.height > UINT32_MAX / pixelRatio
        || position.x < 0.0f || position.y < 0.0f
        || position.x >= static_cast<float>(config.width)
        || position.y >= static_cast<float>(config.height)) {
        return Result::InvalidArguments;
    }
    for (auto i = 0u; i < count; i++) {
        if (!candidates[i] || !scene->pImpl->entry(candidates[i])) {
            return Result::InvalidArguments;
        }
        for (auto j = 0u; j < i; j++) {
            if (_family(candidates[i], candidates[j]) || _family(candidates[j], candidates[i])) {
                return Result::InvalidArguments;
            }
        }
    }

    DisplayList list;
    auto result = scene->pImpl->build(time, list);
    if (result != Result::Success) return result;
    auto paints = new (std::nothrow) SamplePaint[list.count ? list.count : 1u];
    if (!paints) return Result::OutOfMemory;
    auto canvas = tvg::SwCanvas::gen(config.antialiasing ? tvg::EngineOption::Default
                                                         : tvg::EngineOption::Aliased);
    Surface surface;
    if (!canvas) {
        delete[] paints;
        return Result::OutOfMemory;
    }
    result = surface.resize(1u, 1u);
    if (result == Result::Success
        && !renderer::tvgSuccess(canvas->target(surface.data(), surface.stride(), 1u, 1u,
                                    tvg::ColorSpace::ABGR8888S))) {
        result = Result::NonSupport;
    }

    auto found = false;
    auto selectedOrder = 0u;
    if (result == Result::Success) {
        auto pixelX = static_cast<uint32_t>(std::floor(position.x * pixelRatio));
        auto pixelY = static_cast<uint32_t>(std::floor(position.y * pixelRatio));
        for (auto i = 0u; i < count; i++) {
            Color color;
            uint32_t order = 0u;
            bool hit = false;
            result = _sampleFamily(canvas, surface, list, candidates[i], pixelRatio,
                                   pixelX, pixelY, paints, color, order, hit);
            if (result != Result::Success) break;
            if (!hit || (found && order <= selectedOrder)) continue;
            found = true;
            selectedOrder = order;
            output.object = candidates[i];
            output.position = position;
            output.color = color;
        }
    }
    delete canvas;
    delete[] paints;
    if (result != Result::Success) return result;
    return found ? Result::Success : Result::InsufficientCondition;
}

struct RasterPresence
{
    tvg::SwCanvas* canvas = nullptr;
    Surface surface;
    uint32_t width;
    uint32_t height;
    bool antialiasing;

    explicit RasterPresence(const Config& config)
        : width(config.width), height(config.height), antialiasing(config.antialiasing) {}
    ~RasterPresence() { delete canvas; }

    bool visible(const DrawCommand& command, const tvg::Matrix* transform = nullptr)
    {
        if (command.type != CommandType::Path || !command.points || command.count < 2u
            || !command.object
            || (command.object->type() != Type::Arrow
                && command.object->type() != Type::Vector)) return true;
        auto matrix = transform ? *transform : tvg::Matrix{1, 0, 0, 0, 1, 0, 0, 0, 1};
        Vec2 minimum = command.points[0];
        Vec2 maximum = minimum;
        for (auto i = 1u; i < command.count; i++) {
            minimum.x = std::fmin(minimum.x, command.points[i].x);
            minimum.y = std::fmin(minimum.y, command.points[i].y);
            maximum.x = std::fmax(maximum.x, command.points[i].x);
            maximum.y = std::fmax(maximum.y, command.points[i].y);
        }
        if ((maximum.x - minimum.x) * matrix.e11 >= 1.0f
            || (maximum.y - minimum.y) * matrix.e22 >= 1.0f) return true;

        // Bounds expand a collapsed shaft by its stroke width. For subpixel arrow pieces,
        // query prepared coverage at the original canvas origin and physical transform.
        // Moving the probe to the paint bounds can change float rounding at a raster edge.
        if (static_cast<uint64_t>(width) * height > 4u * 1024u * 1024u) return true;
        auto paint = renderer::tvgPaint(command);
        if (!paint) return true;
        float x, y, columns, rows;
        auto valid = renderer::tvgSuccess(paint->transform(matrix))
                  && renderer::tvgSuccess(paint->bounds(&x, &y, &columns, &rows));
        // Do not confuse canvas clipping with empty geometry; failures retain the paint.
        if (!valid || !(x >= 0.0f && y >= 0.0f && x + columns <= width && y + rows <= height)) {
            renderer::tvgRelease(paint);
            return true;
        }
        if (!canvas) canvas = tvg::SwCanvas::gen(antialiasing ? tvg::EngineOption::Default
                                                            : tvg::EngineOption::Aliased);
        if (!canvas || surface.resize(width, height) != Result::Success
            || !renderer::tvgSuccess(canvas->target(surface.data(), surface.stride(),
                                                   surface.width(), surface.height(),
                                                   tvg::ColorSpace::ABGR8888S))
            || !renderer::tvgSuccess(canvas->add(paint))) {
            renderer::tvgRelease(paint);
            return true;
        }
        auto visible = !renderer::tvgSuccess(canvas->update())
                    || paint->intersects(0, 0, surface.width(), surface.height());
        if (!renderer::tvgSuccess(canvas->remove())) {
            delete canvas;
            canvas = nullptr;
            visible = true;
        }
        return visible;
    }
};

static Result _paintBounds(const DisplayList& list, const Object* object, bool family,
                           RasterPresence& presence, BBox& output)
{
    auto root = tvg::Scene::gen();
    if (!root) return Result::OutOfMemory;
    auto found = false;
    for (auto i = 0u; i < list.count; i++) {
        auto& command = list.commands[i];
        if ((family ? !_family(command.object, object) : command.object != object)
            || !renderer::tvgVisible(command) || !presence.visible(command)) {
            continue;
        }
        auto paint = renderer::tvgPaint(command);
        if (!paint) {
            renderer::tvgRelease(root);
            return Result::NonSupport;
        }
        if (!renderer::tvgAdd(root, paint)) {
            renderer::tvgRelease(root);
            return Result::Unknown;
        }
        found = true;
    }
    if (!found) {
        renderer::tvgRelease(root);
        return Result::InsufficientCondition;
    }
    float x;
    float y;
    float width;
    float height;
    auto status = root->bounds(&x, &y, &width, &height);
    renderer::tvgRelease(root);
    if (!renderer::tvgSuccess(status) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width)
        || !std::isfinite(height) || width <= 0.0f || height <= 0.0f) {
        return Result::InsufficientCondition;
    }
    output = {x, y, width, height};
    return Result::Success;
}

Result SwRenderer::bounds(const Scene* scene, const Object* object, float time, BBox& output) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!pImpl->canvas || !scene || !scene->pImpl || !object || !std::isfinite(time)
        || time < 0.0f || !scene->pImpl->entry(object)) {
        return Result::InvalidArguments;
    }
    DisplayList list;
    auto result = scene->pImpl->build(time, list, false);
    if (result != Result::Success) return result;
    RasterPresence presence(scene->config());
    return _paintBounds(list, object, true, presence, output);
}

Result SwRenderer::intersects(const Scene* scene, const Object* first, const Object* second,
                              float time, bool& output, float padding) noexcept
{
    output = false;
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!first || !second || first == second || !std::isfinite(padding) || padding < 0.0f) {
        return Result::InvalidArguments;
    }
    BBox firstBounds;
    BBox secondBounds;
    auto result = bounds(scene, first, time, firstBounds);
    if (result != Result::Success) return result;
    result = bounds(scene, second, time, secondBounds);
    if (result != Result::Success) return result;
    output = firstBounds.intersects(secondBounds, padding);
    return Result::Success;
}

struct LayoutTransform
{
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
};

static BBox _layoutMap(const BBox& bounds, const LayoutTransform& transform)
{
    return {
        transform.x + bounds.x * transform.scaleX,
        transform.y + bounds.y * transform.scaleY,
        bounds.width * transform.scaleX,
        bounds.height * transform.scaleY,
    };
}

static Vec2 _layoutMap(const Vec2& point, const LayoutTransform& transform)
{
    return {
        transform.x + point.x * transform.scaleX,
        transform.y + point.y * transform.scaleY,
    };
}

// The record of one contour of a path command, the points from first up to end.
static Result _layoutPath(const DrawCommand& command, uint32_t visual,
                          const LayoutTransform& transform, LayoutPath& output,
                          uint32_t first, uint32_t end)
{
    if (command.type != CommandType::Path || !command.points || end > command.count
        || end < first + 2u) {
        return Result::InvalidArguments;
    }
    auto count = end - first;
    auto points = new (std::nothrow) Vec2[count];
    if (!points) return Result::OutOfMemory;
    auto minimum = _layoutMap(command.points[first], transform);
    auto maximum = minimum;
    points[0] = minimum;
    for (auto i = 1u; i < count; i++) {
        points[i] = _layoutMap(command.points[first + i], transform);
        minimum.x = std::fmin(minimum.x, points[i].x);
        minimum.y = std::fmin(minimum.y, points[i].y);
        maximum.x = std::fmax(maximum.x, points[i].x);
        maximum.y = std::fmax(maximum.y, points[i].y);
    }
    auto stroked = command.style.stroke.a && command.style.width > 0.0f;
    auto halfX = stroked ? command.style.width * transform.scaleX * 0.5f : 0.0f;
    auto halfY = stroked ? command.style.width * transform.scaleY * 0.5f : 0.0f;
    output.points = points;
    output.paintBounds = {
        minimum.x - halfX,
        minimum.y - halfY,
        maximum.x - minimum.x + halfX * 2.0f,
        maximum.y - minimum.y + halfY * 2.0f,
    };
    output.visual = visual;
    output.count = count;
    output.strokeWidth = stroked ? command.style.width
                                   * std::fmax(transform.scaleX, transform.scaleY)
                                 : 0.0f;
    output.closed = command.closed;
    output.filled = command.style.fill.a;
    output.stroked = stroked;
    return Result::Success;
}

static bool _layoutClip(const BBox& bounds, const BBox& clip, BBox& output)
{
    auto left = std::fmax(bounds.x, clip.x);
    auto top = std::fmax(bounds.y, clip.y);
    auto right = std::fmin(bounds.x + bounds.width, clip.x + clip.width);
    auto bottom = std::fmin(bounds.y + bounds.height, clip.y + clip.height);
    if (right <= left || bottom <= top) {
        output = {};
        return false;
    }
    output = {left, top, right - left, bottom - top};
    return true;
}

static bool _layoutDifferent(const BBox& first, const BBox& second)
{
    static constexpr auto Epsilon = 1.0e-3f;
    return std::fabs(first.x - second.x) > Epsilon
        || std::fabs(first.y - second.y) > Epsilon
        || std::fabs(first.width - second.width) > Epsilon
        || std::fabs(first.height - second.height) > Epsilon;
}

static void _layoutUnion(BBox& output, const BBox& bounds)
{
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    if (output.width <= 0.0f || output.height <= 0.0f) {
        output = bounds;
        return;
    }
    auto left = std::fmin(output.x, bounds.x);
    auto top = std::fmin(output.y, bounds.y);
    auto right = std::fmax(output.x + output.width, bounds.x + bounds.width);
    auto bottom = std::fmax(output.y + output.height, bounds.y + bounds.height);
    output = {left, top, right - left, bottom - top};
}

static uint8_t _layoutOpacity(uint8_t first, uint8_t second)
{
    return static_cast<uint8_t>((static_cast<uint32_t>(first) * second + 127u) / 255u);
}

static uint8_t _layoutMix(uint8_t first, uint8_t second, float progress)
{
    auto value = static_cast<float>(first)
               + (static_cast<float>(second) - first) * progress;
    if (value < 0.0f) value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    return static_cast<uint8_t>(value + 0.5f);
}

static bool _layoutContains(const BBox& outer, const BBox& inner)
{
    return outer.width > 0.0f && outer.height > 0.0f
        && inner.width > 0.0f && inner.height > 0.0f
        && outer.x <= inner.x && outer.y <= inner.y
        && outer.x + outer.width >= inner.x + inner.width
        && outer.y + outer.height >= inner.y + inner.height;
}

static bool _layoutVisual(const DrawCommand& first, const DrawCommand& second)
{
    return first.object == second.object && first.counterpart == second.counterpart
        && first.visualKind == second.visualKind
        && first.visualOpacity == second.visualOpacity;
}

static Result _layoutVisualBounds(const DisplayList& list, const DrawCommand& visual,
                                  RasterPresence& presence, const tvg::Matrix& transform,
                                  BBox& output)
{
    auto root = tvg::Scene::gen();
    if (!root) return Result::OutOfMemory;
    auto found = false;
    for (auto i = 0u; i < list.count; i++) {
        auto& command = list.commands[i];
        if (!_layoutVisual(command, visual) || !renderer::tvgVisible(command)
            || !presence.visible(command, &transform)) continue;
        auto paint = renderer::tvgPaint(command);
        if (!paint) {
            renderer::tvgRelease(root);
            return Result::NonSupport;
        }
        if (!renderer::tvgAdd(root, paint)) {
            renderer::tvgRelease(root);
            return Result::Unknown;
        }
        found = true;
    }
    if (!found) {
        renderer::tvgRelease(root);
        return Result::InsufficientCondition;
    }
    float x;
    float y;
    float width;
    float height;
    auto status = root->bounds(&x, &y, &width, &height);
    renderer::tvgRelease(root);
    if (!renderer::tvgSuccess(status) || !std::isfinite(x) || !std::isfinite(y)
        || !std::isfinite(width) || !std::isfinite(height)
        || width <= 0.0f || height <= 0.0f) {
        return Result::InsufficientCondition;
    }
    output = {x, y, width, height};
    return Result::Success;
}

static float _layoutClearance(float first, float firstSize, float second, float secondSize)
{
    auto firstEnd = first + firstSize;
    auto secondEnd = second + secondSize;
    if (firstEnd <= second) return second - firstEnd;
    if (secondEnd <= first) return first - secondEnd;
    return -std::fmin(firstEnd, secondEnd) + std::fmax(first, second);
}

static char* _layoutPath(const char* parent, const char* kind, uint32_t index)
{
    auto parentSize = std::strlen(parent);
    char suffix[48];
    auto suffixSize = std::snprintf(suffix, sizeof(suffix), "/%s:%u", kind, index);
    if (suffixSize < 0 || static_cast<size_t>(suffixSize) >= sizeof(suffix)
        || parentSize > std::numeric_limits<size_t>::max() - static_cast<size_t>(suffixSize) - 1u) {
        return nullptr;
    }
    auto path = new (std::nothrow) char[parentSize + static_cast<size_t>(suffixSize) + 1u];
    if (!path) return nullptr;
    std::memcpy(path, parent, parentSize);
    std::memcpy(path + parentSize, suffix, static_cast<size_t>(suffixSize) + 1u);
    return path;
}

Result SwRenderer::layout(const Scene* scene, float time, LayoutReport& output,
                          float padding) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!pImpl->canvas || !scene || !scene->pImpl || !output.pImpl
        || !std::isfinite(time) || time < 0.0f || !std::isfinite(padding) || padding < 0.0f) {
        return Result::InvalidArguments;
    }

    auto report = output.pImpl;
    report->reset();
    report->sampleTime = time;
    report->samplePadding = padding;
    RasterPresence presence(scene->config());

    auto occlude = [&](const BBox& bounds, uint8_t opacity,
                       uint32_t scene) {
        if (opacity < 255u) return;
        for (auto i = 0u; i < report->visualCnt; i++) {
            auto& visual = report->visuals[i];
            if (!visual.visible || visual.occluded
                || !_layoutContains(bounds, visual.visibleBounds)) {
                continue;
            }
            visual.occluded = true;
            visual.occluder = scene;
        }
    };

    auto addVisuals = [&](const DisplayList& list, const LayoutTransform& transform,
                          const BBox& clipBounds, uint8_t groupOpacity,
                          LayoutVisualKind stableKind) -> Result {
        if (!list.count) return Result::Success;
        auto representatives = new (std::nothrow) uint32_t[list.count];
        if (!representatives) return Result::OutOfMemory;
        auto representativeCnt = 0u;
        auto result = Result::Success;
        tvg::Matrix matrix = {
            transform.scaleX, 0, transform.x,
            0, transform.scaleY, transform.y,
            0, 0, 1};
        for (auto i = 0u; i < list.count; i++) {
            auto& command = list.commands[i];
            // A mask target shows nothing of its own.
            if (!command.object || list.masking(command)) continue;
            auto duplicate = false;
            for (auto j = 0u; j < representativeCnt; j++) {
                if (!_layoutVisual(command, list.commands[representatives[j]])) continue;
                duplicate = true;
                break;
            }
            if (duplicate) continue;
            representatives[representativeCnt++] = i;

            LayoutVisual visual;
            visual.clipBounds = clipBounds;
            visual.layer = command.layer;
            visual.kind = command.visualKind == LayoutVisualKind::Stable
                        ? stableKind : command.visualKind;
            auto opacity = _layoutOpacity(groupOpacity, command.visualOpacity);
            visual.opacity = static_cast<float>(opacity) / 255.0f;
            if (opacity) {
                BBox localBounds;
                result = _layoutVisualBounds(list, command, presence, matrix, localBounds);
                if (result == Result::Success) {
                    visual.paintBounds = _layoutMap(localBounds, transform);
                    visual.visible = _layoutClip(visual.paintBounds, clipBounds,
                                                 visual.visibleBounds);
                    visual.clipped = !visual.visible
                                  || _layoutDifferent(visual.paintBounds,
                                                      visual.visibleBounds);
                } else if (result != Result::InsufficientCondition) {
                    break;
                }
                result = Result::Success;
            }
            auto visualIndex = report->visualCnt;
            if (!report->add(visual, command.object, command.counterpart)) {
                result = Result::OutOfMemory;
                break;
            }
            if (!opacity) continue;
            for (auto j = 0u; j < list.count; j++) {
                auto& candidate = list.commands[j];
                if (!_layoutVisual(candidate, command)
                    || candidate.type != CommandType::Path
                    || !renderer::tvgVisible(candidate) || !presence.visible(candidate, &matrix)) {
                    continue;
                }
                // Each contour of a path is a record of its own.
                auto contours = candidate.ends ? candidate.contours : 1u;
                auto first = 0u;
                for (auto contour = 0u; contour < contours && result == Result::Success; contour++) {
                    auto end = candidate.ends ? candidate.ends[contour] : candidate.count;
                    if (end < first + 2u) {
                        first = end;
                        continue;
                    }
                    LayoutPath path;
                    result = _layoutPath(candidate, visualIndex, transform, path, first, end);
                    first = end;
                    if (result != Result::Success) break;
                    if (report->add(path)) continue;
                    delete[] path.points;
                    result = Result::OutOfMemory;
                }
                if (result != Result::Success) break;
            }
            if (result != Result::Success) break;
        }
        delete[] representatives;
        return result;
    };

    auto visit = [&](auto&& self, const Scene* current, float sampleTime,
                     const LayoutTransform& authoredTransform,
                     const LayoutTransform& physicalTransform,
                     const BBox& sceneBounds, const BBox& clipBounds, char* path,
                     uint32_t parent, uint32_t depth, LayoutSceneKind kind,
                     bool stretched, const DisplayList* prepared,
                     const Config* coordinateConfig, uint8_t backgroundAlpha,
                     uint8_t directOpacity,
                     uint8_t descendantsOpacity, LayoutVisualKind directKind,
                     LayoutVisualKind descendantsKind, bool measureDirect,
                     bool measureViewports, bool measureSequence,
                     uint32_t resumeScene) -> Result {
        if (!current || (resumeScene == UINT32_MAX && !path)) {
            delete[] path;
            return current ? Result::OutOfMemory : Result::InvalidArguments;
        }
        auto sceneIndex = resumeScene;
        const char* scenePath = nullptr;
        // The visuals of the scene's own objects, which its views may cover.
        auto directBegin = report->visualCnt;
        auto directEnd = directBegin;
        if (sceneIndex == UINT32_MAX) {
            sceneIndex = report->sceneCnt;
            LayoutScene sceneEntry;
            sceneEntry.scene = current;
            sceneEntry.path = path;
            sceneEntry.bounds = sceneBounds;
            sceneEntry.clipBounds = clipBounds;
            sceneEntry.scaleX = authoredTransform.scaleX;
            sceneEntry.scaleY = authoredTransform.scaleY;
            sceneEntry.parent = parent;
            sceneEntry.depth = depth;
            sceneEntry.kind = kind;
            sceneEntry.clipped = _layoutDifferent(sceneBounds, clipBounds);
            sceneEntry.stretched = stretched;
            sceneEntry.time = sampleTime;
            if (!report->add(sceneEntry)) {
                delete[] path;
                return Result::OutOfMemory;
            }
            scenePath = path;
            occlude(clipBounds, _layoutOpacity(directOpacity, backgroundAlpha),
                    sceneIndex);

            auto objectBegin = report->objectCnt;
            for (auto i = 0u; i < current->count(); i++) {
                auto object = current->objectAt(i);
                LayoutObject objectEntry;
                objectEntry.object = object;
                objectEntry.scene = sceneIndex;
                objectEntry.layer = current->pImpl->drawLayer(object, sampleTime);
                objectEntry.clipBounds = clipBounds;
                if (object->parent()) {
                    for (auto parentIndex = 0u; parentIndex < i; parentIndex++) {
                        if (current->objectAt(parentIndex) != object->parent()) continue;
                        objectEntry.parent = objectBegin + parentIndex;
                        break;
                    }
                }
                if (!report->add(objectEntry)) return Result::OutOfMemory;
            }

            DisplayList generated;
            auto list = prepared;
            if (measureDirect && !list) {
                auto result = current->pImpl->build(sampleTime, generated, false);
                if (result != Result::Success) return result;
                list = &generated;
            }
            if (measureDirect) {
                directBegin = report->visualCnt;
                auto result = addVisuals(*list, physicalTransform, clipBounds,
                                         directOpacity, directKind);
                if (result != Result::Success) return result;
                directEnd = report->visualCnt;
            }
        } else {
            if (sceneIndex >= report->sceneCnt
                || report->scenes[sceneIndex].scene != current) {
                return Result::Unknown;
            }
            scenePath = report->scenes[sceneIndex].path;
        }

        auto& authoredConfig = current->config();
        auto& physicalConfig = coordinateConfig ? *coordinateConfig : authoredConfig;
        auto mount = [&](const Scene* child, const Viewport& viewport, char* childPath,
                         LayoutSceneKind childKind, float childTime,
                         const DisplayList* childList, const Config* childSpace,
                         uint8_t childBackgroundAlpha,
                         uint8_t childDirectOpacity, uint8_t childDescendantsOpacity,
                         LayoutVisualKind childDirectKind,
                         LayoutVisualKind childDescendantsKind,
                         bool childMeasureDirect, bool childMeasureViewports,
                         bool childMeasureSequence,
                         uint32_t childResumeScene,
                         const Viewport* frame = nullptr) -> Result {
            if (!child) {
                delete[] childPath;
                return Result::InvalidArguments;
            }
            auto& childAuthoredConfig = child->config();
            auto& childPhysicalConfig = childSpace ? *childSpace : childAuthoredConfig;

            auto authoredX = viewport.x * static_cast<float>(authoredConfig.width);
            auto authoredY = viewport.y * static_cast<float>(authoredConfig.height);
            auto authoredWidth = viewport.width * static_cast<float>(authoredConfig.width);
            auto authoredHeight = viewport.height * static_cast<float>(authoredConfig.height);
            LayoutTransform childAuthoredTransform = {
                authoredTransform.scaleX * authoredWidth
                    / static_cast<float>(childAuthoredConfig.width),
                authoredTransform.scaleY * authoredHeight
                    / static_cast<float>(childAuthoredConfig.height),
                authoredTransform.x + authoredTransform.scaleX * authoredX,
                authoredTransform.y + authoredTransform.scaleY * authoredY,
            };
            BBox childBounds = {
                childAuthoredTransform.x,
                childAuthoredTransform.y,
                authoredTransform.scaleX * authoredWidth,
                authoredTransform.scaleY * authoredHeight,
            };

            auto physicalX = viewport.x * static_cast<float>(physicalConfig.width);
            auto physicalY = viewport.y * static_cast<float>(physicalConfig.height);
            auto physicalWidth = viewport.width * static_cast<float>(physicalConfig.width);
            auto physicalHeight = viewport.height * static_cast<float>(physicalConfig.height);
            LayoutTransform childPhysicalTransform = {
                physicalTransform.scaleX * physicalWidth
                    / static_cast<float>(childPhysicalConfig.width),
                physicalTransform.scaleY * physicalHeight
                    / static_cast<float>(childPhysicalConfig.height),
                physicalTransform.x + physicalTransform.scaleX * physicalX,
                physicalTransform.y + physicalTransform.scaleY * physicalY,
            };
            BBox physicalBounds = {
                childPhysicalTransform.x,
                childPhysicalTransform.y,
                physicalTransform.scaleX * physicalWidth,
                physicalTransform.scaleY * physicalHeight,
            };
            BBox childClip;
            _layoutClip(physicalBounds, clipBounds, childClip);
            if (frame) {
                // A scene moving through a frame shows only where it overlaps the frame.
                BBox frameBounds = {
                    physicalTransform.x + physicalTransform.scaleX
                        * frame->x * static_cast<float>(physicalConfig.width),
                    physicalTransform.y + physicalTransform.scaleY
                        * frame->y * static_cast<float>(physicalConfig.height),
                    physicalTransform.scaleX * frame->width
                        * static_cast<float>(physicalConfig.width),
                    physicalTransform.scaleY * frame->height
                        * static_cast<float>(physicalConfig.height),
                };
                auto visible = childClip;
                _layoutClip(visible, frameBounds, childClip);
            }
            auto relativeX = authoredWidth
                           / static_cast<float>(childAuthoredConfig.width);
            auto relativeY = authoredHeight
                           / static_cast<float>(childAuthoredConfig.height);
            auto tolerance = std::fmax(std::fabs(relativeX), std::fabs(relativeY)) * 1.0e-3f;
            auto childStretched = std::fabs(relativeX - relativeY) > tolerance;
            return self(self, child, childTime, childAuthoredTransform,
                        childPhysicalTransform, childBounds, childClip, childPath,
                        sceneIndex, depth + 1u, childKind, childStretched, childList,
                        childSpace, childBackgroundAlpha, childDirectOpacity,
                        childDescendantsOpacity,
                        childDirectKind, childDescendantsKind, childMeasureDirect,
                        childMeasureViewports, childMeasureSequence,
                        childResumeScene);
        };

        if (measureViewports) {
            // A view is a scene of the report that shows the same objects again, through its
            // camera, in its rectangle: the report gives where it covers the frame, not a second
            // set of objects. Opaque, it hides the scene's own visuals on its layer and below.
            auto spaceX = static_cast<float>(physicalConfig.width) / static_cast<float>(authoredConfig.width);
            auto spaceY = static_cast<float>(physicalConfig.height) / static_cast<float>(authoredConfig.height);
            for (auto i = 0u; i < current->pImpl->viewCnt; i++) {
                ViewFrame frame;
                auto result = current->pImpl->frame(i, sampleTime, frame);
                if (result != Result::Success) return result;
                auto opacity = _layoutOpacity(descendantsOpacity,
                                              static_cast<uint8_t>(frame.opacity * 255.0f + 0.5f));
                if (!frame.shown || !opacity) continue;
                LayoutScene viewEntry;
                viewEntry.scene = current;
                viewEntry.path = _layoutPath(scenePath, "view", i);
                if (!viewEntry.path) return Result::OutOfMemory;
                viewEntry.bounds = {
                    physicalTransform.x + physicalTransform.scaleX * spaceX * frame.rect[0],
                    physicalTransform.y + physicalTransform.scaleY * spaceY * frame.rect[1],
                    physicalTransform.scaleX * spaceX * frame.rect[2],
                    physicalTransform.scaleY * spaceY * frame.rect[3],
                };
                _layoutClip(viewEntry.bounds, clipBounds, viewEntry.clipBounds);
                viewEntry.scaleX = authoredTransform.scaleX * frame.scale;
                viewEntry.scaleY = authoredTransform.scaleY * frame.scale;
                viewEntry.parent = sceneIndex;
                viewEntry.depth = depth + 1u;
                viewEntry.kind = LayoutSceneKind::View;
                viewEntry.clipped = _layoutDifferent(viewEntry.bounds, viewEntry.clipBounds);
                viewEntry.time = sampleTime;
                auto viewIndex = report->sceneCnt;
                if (!report->add(viewEntry)) {
                    delete[] viewEntry.path;
                    return Result::OutOfMemory;
                }
                if (_layoutOpacity(opacity, frame.background.a) < 255u) continue;
                for (auto j = directBegin; j < directEnd; j++) {
                    auto& visual = report->visuals[j];
                    if (!visual.visible || visual.occluded || visual.layer > frame.layer
                        || !_layoutContains(viewEntry.clipBounds, visual.visibleBounds)) {
                        continue;
                    }
                    visual.occluded = true;
                    visual.occluder = viewIndex;
                }
            }
            for (auto i = 0u; i < current->viewportCount(); i++) {
                Viewport viewport;
                if (!current->viewportAt(i, viewport)) return Result::Unknown;
                auto childPath = _layoutPath(scenePath, "viewport", i);
                auto child = current->sceneAt(i);
                auto result = mount(child, viewport, childPath,
                                    LayoutSceneKind::Viewport, sampleTime, nullptr,
                                    nullptr, child->config().background.a,
                                    descendantsOpacity, descendantsOpacity,
                                    descendantsKind, descendantsKind, true, true,
                                    true, UINT32_MAX);
                if (result != Result::Success) return result;
            }
        }

        if (!measureSequence) return Result::Success;
        auto sequence = current->pImpl->sequence;
        if (!sequence || sampleTime < sequence->begin) return Result::Success;
        for (auto i = 0u; i < sequence->count; i++) {
            auto& stage = sequence->stages[i];
            if (sampleTime < stage.end || i + 1u >= sequence->count) {
                auto childPath = _layoutPath(scenePath, "transition", i);
                return mount(stage.scene, sequence->viewport, childPath,
                             LayoutSceneKind::Transition, sequence->time(i, sampleTime),
                             nullptr, nullptr, stage.scene->config().background.a,
                             descendantsOpacity, descendantsOpacity,
                             descendantsKind, descendantsKind, true, true, true,
                             UINT32_MAX);
            }
            auto& transition = sequence->transitions[i];
            if (sampleTime >= transition.end) continue;
            auto timelineProgress = (sampleTime - transition.begin)
                                  / (transition.end - transition.begin);
            if (timelineProgress <= 0.0f) {
                auto childPath = _layoutPath(scenePath, "transition", i);
                return mount(stage.scene, sequence->viewport, childPath,
                             LayoutSceneKind::Transition, sequence->time(i, sampleTime),
                             nullptr, nullptr, stage.scene->config().background.a,
                             descendantsOpacity, descendantsOpacity,
                             descendantsKind, descendantsKind, true, true, true,
                             UINT32_MAX);
            }

            auto from = stage.scene;
            auto to = sequence->stages[i + 1u].scene;
            auto fromTime = sequence->time(i, sampleTime);
            auto toTime = sequence->time(i + 1u, sampleTime);
            if (transition.kind != TransitionKind::Match) {
                // Each scene shows whole where the frame of the style places it, leaving or
                // arriving with everything it holds, or with matching everything but the
                // objects that pair, which travel above both in the viewport of the sequence.
                auto progress = ease(transition.curve, timelineProgress);
                TransitionFrame frame;
                transition.frame(sequence->viewport, progress, from->config().background.a,
                                 to->config().background.a, frame);
                DisplayList parts[3];
                Config spaces[3];
                if (transition.match) {
                    auto result = Scene::Impl::matched(from, to, transition, progress, fromTime, toTime,
                                                       parts, spaces);
                    if (result != Result::Success) return result;
                }
                auto place = [&](const Scene* child, uint32_t index, const Viewport& viewport,
                                 float share, LayoutVisualKind childKind) {
                    auto opacity = _layoutOpacity(descendantsOpacity,
                                                  static_cast<uint8_t>(share * 255.0f + 0.5f));
                    auto part = index - i;
                    return mount(child, viewport, _layoutPath(scenePath, "transition", index),
                                 LayoutSceneKind::Transition, sequence->time(index, sampleTime),
                                 transition.match ? &parts[part] : nullptr,
                                 transition.match ? &spaces[part] : nullptr,
                                 child->config().background.a, opacity, opacity,
                                 childKind, childKind, true, true, true, UINT32_MAX,
                                 &sequence->viewport);
                };
                auto outgoing = [&]() {
                    return place(from, i, frame.outgoing, frame.outgoingOpacity,
                                 LayoutVisualKind::FadeOut);
                };
                auto incoming = [&]() {
                    return place(to, i + 1u, frame.incoming, frame.incomingOpacity,
                                 LayoutVisualKind::FadeIn);
                };
                auto result = frame.outgoingAbove ? incoming() : outgoing();
                if (result != Result::Success) return result;
                result = frame.outgoingAbove ? outgoing() : incoming();
                if (result != Result::Success || !transition.match) return result;
                // The pairs lie in the space of the outgoing scene, stretched over the viewport.
                auto& fromConfig = from->config();
                auto x = sequence->viewport.x * static_cast<float>(physicalConfig.width);
                auto y = sequence->viewport.y * static_cast<float>(physicalConfig.height);
                auto width = sequence->viewport.width * static_cast<float>(physicalConfig.width);
                auto height = sequence->viewport.height * static_cast<float>(physicalConfig.height);
                LayoutTransform pairs = {
                    physicalTransform.scaleX * width / static_cast<float>(fromConfig.width),
                    physicalTransform.scaleY * height / static_cast<float>(fromConfig.height),
                    physicalTransform.x + physicalTransform.scaleX * x,
                    physicalTransform.y + physicalTransform.scaleY * y,
                };
                BBox pairBounds = {pairs.x, pairs.y, physicalTransform.scaleX * width,
                                   physicalTransform.scaleY * height};
                BBox pairClip;
                _layoutClip(pairBounds, clipBounds, pairClip);
                return addVisuals(parts[2], pairs, pairClip, descendantsOpacity,
                                  LayoutVisualKind::Stable);
            }
            DisplayList first;
            DisplayList second;
            DisplayList blended;
            auto result = from->pImpl->build(fromTime, first, false);
            if (result != Result::Success) return result;
            result = to->pImpl->build(toTime, second, false);
            if (result != Result::Success) return result;
            auto& fromConfig = from->config();
            auto& toConfig = to->config();
            auto progress = ease(transition.curve, timelineProgress);
            result = blended.blend(first, second, transition.matches,
                                   from->count(), to->count(),
                                   {static_cast<float>(fromConfig.width),
                                    static_cast<float>(fromConfig.height)},
                                   {static_cast<float>(toConfig.width),
                                    static_cast<float>(toConfig.height)}, progress);
            if (result != Result::Success) return result;
            auto bounded = std::fmax(0.0f, std::fmin(1.0f, progress));
            auto fromOpacity = static_cast<uint8_t>(
                (1.0f - bounded) * 255.0f + 0.5f);
            auto toOpacity = static_cast<uint8_t>(bounded * 255.0f + 0.5f);
            auto fromDescendantsOpacity = _layoutOpacity(descendantsOpacity,
                                                         fromOpacity);
            auto toDescendantsOpacity = _layoutOpacity(descendantsOpacity,
                                                       toOpacity);
            auto backgroundAlpha = _layoutMix(fromConfig.background.a,
                                              toConfig.background.a, bounded);

            auto fromScene = report->sceneCnt;
            auto fromPath = _layoutPath(scenePath, "transition", i);
            result = mount(from, sequence->viewport, fromPath,
                           LayoutSceneKind::Transition, fromTime, &blended,
                           &fromConfig, backgroundAlpha, descendantsOpacity,
                           fromDescendantsOpacity,
                           descendantsKind, LayoutVisualKind::FadeOut, true, true,
                           false, UINT32_MAX);
            if (result != Result::Success) return result;
            auto toScene = report->sceneCnt;
            auto toPath = _layoutPath(scenePath, "transition", i + 1u);
            result = mount(to, sequence->viewport, toPath,
                           LayoutSceneKind::Transition, toTime, &blended,
                           &fromConfig, 0u, descendantsOpacity,
                           toDescendantsOpacity, descendantsKind,
                           LayoutVisualKind::FadeIn, false, true, false,
                           UINT32_MAX);
            if (result != Result::Success) return result;
            result = mount(from, sequence->viewport, nullptr,
                           LayoutSceneKind::Transition, fromTime, &blended,
                           &fromConfig, backgroundAlpha, descendantsOpacity,
                           fromDescendantsOpacity, descendantsKind,
                           LayoutVisualKind::FadeOut, false, false, true,
                           fromScene);
            if (result != Result::Success) return result;
            return mount(to, sequence->viewport, nullptr,
                         LayoutSceneKind::Transition, toTime, &blended,
                         &fromConfig, 0u, descendantsOpacity, toDescendantsOpacity,
                         descendantsKind, LayoutVisualKind::FadeIn, false, false,
                         true, toScene);
        }
        return Result::Unknown;
    };

    auto rootPath = new (std::nothrow) char[5];
    if (!rootPath) return Result::OutOfMemory;
    std::memcpy(rootPath, "root", 5);
    auto rootBounds = BBox{0.0f, 0.0f, static_cast<float>(scene->config().width),
                           static_cast<float>(scene->config().height)};
    auto result = visit(visit, scene, time, {}, {}, rootBounds, rootBounds, rootPath,
                        0xffffffffu, 0u, LayoutSceneKind::Root, false, nullptr,
                        nullptr, scene->config().background.a, 255u, 255u,
                        LayoutVisualKind::Stable,
                        LayoutVisualKind::Stable, true, true, true, UINT32_MAX);
    if (result != Result::Success) {
        report->reset();
        return result;
    }

    auto findObject = [&](const Object* object) {
        for (auto i = 0u; i < report->objectCnt; i++) {
            if (report->objects[i].object == object) return i;
        }
        return UINT32_MAX;
    };
    auto applyVisual = [&](const LayoutVisual& visual, uint32_t index) {
        auto& identity = report->objects[index];
        _layoutUnion(identity.paintBounds, visual.paintBounds);
        if (visual.visible) {
            _layoutUnion(identity.visibleBounds, visual.visibleBounds);
            identity.visible = true;
        }
        identity.clipped = identity.clipped || visual.clipped;
        for (auto i = 0u; i < report->objectCnt; i++) {
            auto& family = report->objects[i];
            if (family.scene != identity.scene
                || !_family(identity.object, family.object)) {
                continue;
            }
            _layoutUnion(family.familyBounds, visual.paintBounds);
            if (visual.visible) {
                _layoutUnion(family.visibleFamilyBounds, visual.visibleBounds);
                family.familyVisible = true;
            }
            family.familyClipped = family.familyClipped || visual.clipped;
        }
    };
    for (auto i = 0u; i < report->visualCnt; i++) {
        auto& visual = report->visuals[i];
        visual.object = findObject(report->visualObjects[i]);
        if (visual.object == UINT32_MAX) {
            report->reset();
            return Result::Unknown;
        }
        if (report->visualCounterparts[i]) {
            visual.counterpart = findObject(report->visualCounterparts[i]);
            if (visual.counterpart == UINT32_MAX) {
                report->reset();
                return Result::Unknown;
            }
        }
        applyVisual(visual, visual.object);
        if (visual.kind == LayoutVisualKind::Morph
            && visual.counterpart != UINT32_MAX
            && visual.counterpart != visual.object) {
            applyVisual(visual, visual.counterpart);
        }
    }

    for (auto i = 0u; i < report->objectCnt; i++) {
        auto& first = report->objects[i];
        for (auto j = i + 1u; j < report->objectCnt; j++) {
            auto& second = report->objects[j];
            auto firstContainsSecond = first.scene == second.scene
                                    && _family(second.object, first.object);
            auto secondContainsFirst = first.scene == second.scene
                                    && _family(first.object, second.object);
            if (!firstContainsSecond && !secondContainsFirst) continue;
            auto containerIndex = firstContainsSecond ? i : j;
            auto contentIndex = firstContainsSecond ? j : i;
            auto& container = report->objects[containerIndex];
            auto& content = report->objects[contentIndex];
            if (container.paintBounds.width <= 0.0f || container.paintBounds.height <= 0.0f
                || content.familyBounds.width <= 0.0f || content.familyBounds.height <= 0.0f) {
                continue;
            }
            LayoutContainment containment;
            containment.container = containerIndex;
            containment.content = contentIndex;
            containment.inset = padding;
            for (auto ancestor = content.object; ancestor && ancestor != container.object;
                 ancestor = ancestor->parent()) {
                containment.depth++;
            }
            auto innerLeft = container.paintBounds.x + padding;
            auto innerTop = container.paintBounds.y + padding;
            auto innerRight = container.paintBounds.x + container.paintBounds.width - padding;
            auto innerBottom = container.paintBounds.y + container.paintBounds.height - padding;
            containment.overflowLeft = std::fmax(0.0f, innerLeft - content.familyBounds.x);
            containment.overflowTop = std::fmax(0.0f, innerTop - content.familyBounds.y);
            containment.overflowRight = std::fmax(
                0.0f, content.familyBounds.x + content.familyBounds.width - innerRight);
            containment.overflowBottom = std::fmax(
                0.0f, content.familyBounds.y + content.familyBounds.height - innerBottom);
            containment.contained = containment.overflowLeft <= 0.0f
                                 && containment.overflowTop <= 0.0f
                                 && containment.overflowRight <= 0.0f
                                 && containment.overflowBottom <= 0.0f;
            if (!report->add(containment)) {
                report->reset();
                return Result::OutOfMemory;
            }
        }
    }

    auto related = [&](const LayoutVisual& first, const LayoutVisual& second) {
        uint32_t firstObjects[] = {first.object, first.counterpart};
        uint32_t secondObjects[] = {second.object, second.counterpart};
        auto firstCount = first.kind == LayoutVisualKind::Morph
                       && first.counterpart != UINT32_MAX ? 2u : 1u;
        auto secondCount = second.kind == LayoutVisualKind::Morph
                        && second.counterpart != UINT32_MAX ? 2u : 1u;
        auto comparable = false;
        for (auto i = 0u; i < firstCount; i++) {
            auto& firstObject = report->objects[firstObjects[i]];
            for (auto j = 0u; j < secondCount; j++) {
                auto& secondObject = report->objects[secondObjects[j]];
                if (firstObject.scene != secondObject.scene) continue;
                comparable = true;
                if (!_family(firstObject.object, secondObject.object)
                    && !_family(secondObject.object, firstObject.object)) return false;
            }
        }
        return comparable;
    };
    for (auto i = 0u; i < report->visualCnt; i++) {
        auto& first = report->visuals[i];
        if (!first.visible || first.occluded) continue;
        for (auto j = i + 1u; j < report->visualCnt; j++) {
            auto& second = report->visuals[j];
            if (!second.visible || second.occluded || related(first, second)
                || !first.visibleBounds.intersects(second.visibleBounds, padding)) {
                continue;
            }

            LayoutCollision collision;
            collision.first = first.object;
            collision.second = second.object;
            collision.clearance = {
                _layoutClearance(first.visibleBounds.x, first.visibleBounds.width,
                                 second.visibleBounds.x, second.visibleBounds.width),
                _layoutClearance(first.visibleBounds.y, first.visibleBounds.height,
                                 second.visibleBounds.y, second.visibleBounds.height),
            };
            collision.overlapping = collision.clearance.x < 0.0f
                                 && collision.clearance.y < 0.0f;
            if (collision.overlapping) {
                auto left = std::fmax(first.visibleBounds.x, second.visibleBounds.x);
                auto top = std::fmax(first.visibleBounds.y, second.visibleBounds.y);
                auto right = std::fmin(first.visibleBounds.x + first.visibleBounds.width,
                                       second.visibleBounds.x + second.visibleBounds.width);
                auto bottom = std::fmin(first.visibleBounds.y + first.visibleBounds.height,
                                        second.visibleBounds.y + second.visibleBounds.height);
                collision.overlap = {left, top, right - left, bottom - top};
            }
            auto horizontal = padding - collision.clearance.x;
            auto vertical = padding - collision.clearance.y;
            if (horizontal <= vertical) {
                collision.separation.x = second.visibleBounds.center().x
                                       < first.visibleBounds.center().x
                                       ? -horizontal : horizontal;
            } else {
                collision.separation.y = second.visibleBounds.center().y
                                       < first.visibleBounds.center().y
                                       ? -vertical : vertical;
            }
            if (!report->add(collision)) {
                report->reset();
                return Result::OutOfMemory;
            }
        }
    }
    return Result::Success;
}

Result SwRenderer::render(const Scene* scene, float time, Surface& surface) noexcept
{
    return render(scene, time, surface, 1u);
}

Result SwRenderer::render(const Scene* scene, float time, Surface& surface,
                          uint32_t pixelRatio) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!pImpl->canvas || !scene || !scene->pImpl || !std::isfinite(time)
        || time < 0.0f || !pixelRatio) {
        return Result::InvalidArguments;
    }
    auto& config = scene->pImpl->cfg;
    if (config.width > UINT32_MAX / pixelRatio || config.height > UINT32_MAX / pixelRatio) {
        return Result::InvalidArguments;
    }
    auto result = surface.resize(config.width * pixelRatio, config.height * pixelRatio);
    if (result != Result::Success) return result;

    if (pImpl->antialiasing != config.antialiasing) {
        renderer::tvgRetainedReset(pImpl->tree);
        delete pImpl->canvas;
        pImpl->canvas = tvg::SwCanvas::gen(config.antialiasing ? tvg::EngineOption::Default
                                                               : tvg::EngineOption::Aliased);
        pImpl->antialiasing = config.antialiasing;
        pImpl->buffer = nullptr;
        if (!pImpl->canvas) return Result::NonSupport;
    }
    // Retargeting damages every retained paint, so only do it when the buffer changes.
    if (pImpl->buffer != surface.data() || pImpl->stride != surface.stride()
        || pImpl->width != surface.width() || pImpl->height != surface.height()) {
        pImpl->buffer = nullptr;
        if (!renderer::tvgSuccess(pImpl->canvas->target(surface.data(), surface.stride(), surface.width(),
                                            surface.height(), tvg::ColorSpace::ABGR8888S))) {
            return Result::NonSupport;
        }
        pImpl->buffer = surface.data();
        pImpl->stride = surface.stride();
        pImpl->width = surface.width();
        pImpl->height = surface.height();
    }

    result = renderer::tvgRetainedSync(pImpl->tree, pImpl->canvas, scene, time,
                                       static_cast<float>(pixelRatio));
    if (result != Result::Success) return result;
    if (!renderer::tvgSuccess(pImpl->canvas->update())) return Result::Unknown;
    if (!renderer::tvgSuccess(pImpl->canvas->draw(true))) return Result::Unknown;
    if (!renderer::tvgSuccess(pImpl->canvas->sync())) return Result::Unknown;
    return Result::Success;
}

namespace renderer
{

bool cpuEnabled() noexcept
{
    return true;
}

SwRenderer* genCpu() noexcept
{
    return SwRenderer::gen();
}

}  // namespace renderer

}  // namespace vis
