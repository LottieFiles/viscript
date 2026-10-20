// Renderer on ThorVG's software engine: renders into a Surface, and reads bounds,
// intersections, layout reports and pixel samples from what it drew.
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#include <thorvg.h>

#include "vis.h"
#include "visDisplayList.h"
#include "visLayoutJson.h"
#include "visRendererData.h"
#include "visScene.h"
#include "visTvgRenderer.h"
#include "../../common/visFinite.h"
#include "../../common/visLayout.h"
#include "../../common/visMix.h"
#include "../../common/visString.h"

namespace vis
{

struct Renderer::Impl
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

Renderer::Renderer() noexcept
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

Renderer::~Renderer()
{
    auto initialized = pImpl && pImpl->initialized;
    if (pImpl) {
        renderer::tvgRetainedFree(pImpl->tree);
        delete pImpl->canvas;
    }
    delete pImpl;
    if (initialized) renderer::tvgTerm();
}

Renderer* Renderer::gen() noexcept
{
    auto renderer = new (std::nothrow) Renderer;
    if (!renderer || (renderer->pImpl && renderer->pImpl->canvas)) return renderer;
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
    if (!renderer::tvgDraw(canvas)) {
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
                    if (!renderer::tvgSuccess(paint->visible(true)) || !renderer::tvgDraw(canvas)) {
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

Result Renderer::sample(const Scene* scene, float time, const Vec2& position,
                          const Object* const* candidates, uint32_t count,
                          PixelSample& output, uint32_t pixelRatio) noexcept
{
    output = {};
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!scene || !scene->pImpl || !candidates
        || !count || count > 64u || !pixelRatio || !std::isfinite(time) || time < 0.0f
        || !_finite(position)) {
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
        Vec2 minimum;
        Vec2 maximum;
        extent(command.points, command.count, minimum, maximum);
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

// The bounds of the paints of the commands pick takes, leaving out vector pieces too thin to
// cover a pixel when drawn through transform (identity when null).
template<typename Pick>
static Result _paintBounds(const DisplayList& list, Pick pick, RasterPresence& presence,
                           const tvg::Matrix* transform, BBox& output)
{
    auto root = tvg::Scene::gen();
    if (!root) return Result::OutOfMemory;
    auto found = false;
    for (auto i = 0u; i < list.count; i++) {
        auto& command = list.commands[i];
        if (!pick(command) || !renderer::tvgVisible(command)
            || !presence.visible(command, transform)) continue;
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
    BBox bounds;
    auto measured = found && renderer::tvgSuccess(root->bounds(&bounds.x, &bounds.y,
                                                               &bounds.width, &bounds.height));
    renderer::tvgRelease(root);
    if (!measured || !_finite(bounds) || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return Result::InsufficientCondition;
    }
    output = bounds;
    return Result::Success;
}

// The bounds of what object and its descendants draw.
static Result _familyBounds(const DisplayList& list, const Object* object,
                            RasterPresence& presence, BBox& output)
{
    auto pick = [object](const DrawCommand& command) { return _family(command.object, object); };
    return _paintBounds(list, pick, presence, nullptr, output);
}

Result Renderer::bounds(const Scene* scene, const Object* object, float time, BBox& output) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!scene || !scene->pImpl || !object || !std::isfinite(time) || time < 0.0f
        || !scene->pImpl->entry(object)) {
        return Result::InvalidArguments;
    }
    DisplayList list;
    auto result = scene->pImpl->build(time, list, false);
    if (result != Result::Success) return result;
    RasterPresence presence(scene->config());
    return _familyBounds(list, object, presence, output);
}

Result Renderer::intersects(const Scene* scene, const Object* first, const Object* second,
                              float time, bool& output, float padding) noexcept
{
    output = false;
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!scene || !scene->pImpl || !first || !second || first == second || !std::isfinite(time)
        || time < 0.0f || !std::isfinite(padding) || padding < 0.0f
        || !scene->pImpl->entry(first) || !scene->pImpl->entry(second)) {
        return Result::InvalidArguments;
    }
    DisplayList list;
    auto result = scene->pImpl->build(time, list, false);
    if (result != Result::Success) return result;
    RasterPresence presence(scene->config());
    BBox firstBounds;
    BBox secondBounds;
    result = _familyBounds(list, first, presence, firstBounds);
    if (result == Result::Success) result = _familyBounds(list, second, presence, secondBounds);
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

// Where a scene of the report shows: the transforms from its own coordinates, and from those of
// the display list it is measured from, to root pixels; its bounds there, the part of them its
// parents let show, and whether its viewport stretches it.
struct LayoutPlace
{
    LayoutTransform authored;
    LayoutTransform physical;
    BBox bounds;
    BBox clip;
    bool stretched = false;
};

// A scene the layout walk enters, and how it shows there.
struct LayoutEntry
{
    const Scene* scene;
    // The time of the scene's own timeline it shows.
    float time;
    LayoutSceneKind kind;
    // The display list of the scene's own objects when its parent made one, and the space of
    // its coordinates when that is not the scene's own.
    const DisplayList* list = nullptr;
    const Config* space = nullptr;
    uint8_t background;
    // The opacity and kind of the scene's own visuals, and of those of the scenes it mounts.
    uint8_t opacity;
    uint8_t descendantsOpacity;
    LayoutVisualKind visualKind;
    LayoutVisualKind descendantsKind;
    // The opacity the renderer mounts the scene's views and scenes with inside it: the fade of a
    // match for the two scenes it blends, else 255. descendantsOpacity takes it in, but a view or
    // a stage of a styled transition takes its own share of it, rounded once, as the renderer does.
    uint8_t fade = 255u;
    // False for the incoming scene of a match, whose visuals the outgoing one measures.
    bool direct = true;
    // The walk's: the scene's record, its depth and its place.
    uint32_t index = UINT32_MAX;
    uint32_t depth = 0;
    LayoutPlace place;

    LayoutEntry(const Scene* scene, float time, LayoutSceneKind kind, uint8_t opacity,
                LayoutVisualKind visualKind)
        : scene(scene), time(time), kind(kind), background(scene->config().background.a),
          opacity(opacity), descendantsOpacity(opacity), visualKind(visualKind),
          descendantsKind(visualKind) {}
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

// Places a scene with the size of config in the rectangle viewport covers of a space with the
// size of space, drawn through transform: gives the transform of the scene, and its bounds.
static LayoutTransform _layoutPlace(const LayoutTransform& transform, const Viewport& viewport,
                                    const Config& space, const Config& config, BBox& bounds)
{
    auto x = viewport.x * static_cast<float>(space.width);
    auto y = viewport.y * static_cast<float>(space.height);
    auto width = viewport.width * static_cast<float>(space.width);
    auto height = viewport.height * static_cast<float>(space.height);
    bounds = {transform.x + transform.scaleX * x, transform.y + transform.scaleY * y,
              transform.scaleX * width, transform.scaleY * height};
    return {transform.scaleX * width / static_cast<float>(config.width),
            transform.scaleY * height / static_cast<float>(config.height), bounds.x, bounds.y};
}

// The record of one contour of a path command, the points from first up to end.
static Result _layoutContour(const DrawCommand& command, uint32_t visual,
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
    for (auto i = 0u; i < count; i++) points[i] = _layoutMap(command.points[first + i], transform);
    Vec2 minimum;
    Vec2 maximum;
    extent(points, count, minimum, maximum);
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

static bool _layoutContains(const BBox& outer, const BBox& inner)
{
    return outer.width > 0.0f && outer.height > 0.0f
        && inner.width > 0.0f && inner.height > 0.0f
        && outer.x <= inner.x && outer.y <= inner.y
        && outer.x + outer.width >= inner.x + inner.width
        && outer.y + outer.height >= inner.y + inner.height;
}

// Marks the visuals from begin up to end that bounds wholly covers, on layer or below, as hidden
// by the scene record occluder, unless they show nothing or are hidden already.
static void _layoutOcclude(LayoutVisual* visuals, uint32_t begin, uint32_t end,
                           const BBox& bounds, int32_t layer, uint32_t occluder)
{
    for (auto i = begin; i < end; i++) {
        auto& visual = visuals[i];
        if (!visual.visible || visual.occluded || visual.layer > layer
            || !_layoutContains(bounds, visual.visibleBounds)) {
            continue;
        }
        visual.occluded = true;
        visual.occluder = occluder;
    }
}

static bool _layoutVisual(const DrawCommand& first, const DrawCommand& second)
{
    return first.object == second.object && first.counterpart == second.counterpart
        && first.visualKind == second.visualKind
        && first.visualOpacity == second.visualOpacity;
}

Result Renderer::layout(const Scene* scene, float time, LayoutReport& output,
                          float padding) noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    renderer::TvgDiagnosis diagnosis(pImpl->diagnostic);
    if (!scene || !scene->pImpl || !output.pImpl
        || !std::isfinite(time) || time < 0.0f || !std::isfinite(padding) || padding < 0.0f) {
        return Result::InvalidArguments;
    }

    // The walk down the scenes, inside layout() for its access to their Impl. A scene adds its
    // record, its objects and own visuals, its views, the scenes of its viewports and then those
    // its sequence shows; once all are in, the visuals relate to their objects and each other.
    struct Walk
    {
        LayoutReport::Impl* report;
        RasterPresence& presence;

        // Adds a visual for each object, counterpart, kind and opacity that list draws, with the
        // contours of its paths, drawn through transform and clipped to clip.
        Result visuals(const DisplayList& list, const LayoutTransform& transform, const BBox& clip,
                       uint8_t groupOpacity, LayoutVisualKind stableKind)
        {
            if (!list.count) return Result::Success;
            // The first command of each visual so far.
            std::unique_ptr<uint32_t[]> firsts(new (std::nothrow) uint32_t[list.count]);
            if (!firsts) return Result::OutOfMemory;
            auto firstCnt = 0u;
            tvg::Matrix matrix = {
                transform.scaleX, 0, transform.x,
                0, transform.scaleY, transform.y,
                0, 0, 1};
            for (auto i = 0u; i < list.count; i++) {
                auto& command = list.commands[i];
                // A mask target shows nothing of its own.
                if (!command.object || list.masking(command)) continue;
                auto same = [&command](const DrawCommand& other) {
                    return _layoutVisual(other, command);
                };
                auto seen = false;
                for (auto j = 0u; j < firstCnt && !seen; j++) seen = same(list.commands[firsts[j]]);
                if (seen) continue;
                firsts[firstCnt++] = i;

                LayoutVisual visual;
                visual.clipBounds = clip;
                visual.layer = command.layer;
                visual.kind = command.visualKind == LayoutVisualKind::Stable
                            ? stableKind : command.visualKind;
                auto opacity = _opacity(groupOpacity, command.visualOpacity);
                visual.opacity = static_cast<float>(opacity) / 255.0f;
                BBox bounds;
                auto result = opacity ? _paintBounds(list, same, presence, &matrix, bounds)
                                      : Result::InsufficientCondition;
                if (result == Result::Success) {
                    visual.paintBounds = _layoutMap(bounds, transform);
                    visual.visible = _layoutClip(visual.paintBounds, clip, visual.visibleBounds);
                    visual.clipped = !visual.visible
                                  || _layoutDifferent(visual.paintBounds, visual.visibleBounds);
                } else if (result != Result::InsufficientCondition) {
                    return result;
                }
                auto index = report->visualCnt;
                if (!report->add(visual, command.object, command.counterpart)) return Result::OutOfMemory;
                if (!opacity) continue;
                // Each contour of a path of the visual is a record of its own.
                for (auto j = 0u; j < list.count; j++) {
                    auto& candidate = list.commands[j];
                    if (!same(candidate) || candidate.type != CommandType::Path
                        || !renderer::tvgVisible(candidate) || !presence.visible(candidate, &matrix)) {
                        continue;
                    }
                    auto contours = candidate.ends ? candidate.contours : 1u;
                    for (auto contour = 0u, first = 0u; contour < contours; contour++) {
                        auto end = candidate.ends ? candidate.ends[contour] : candidate.count;
                        if (end >= first + 2u) {
                            LayoutPath record;
                            result = _layoutContour(candidate, index, transform, record, first, end);
                            if (result != Result::Success) return result;
                            if (!report->add(record)) {
                                delete[] record.points;
                                return Result::OutOfMemory;
                            }
                        }
                        first = end;
                    }
                }
            }
            return Result::Success;
        }

        // Adds the record of the scene of entry under the record parent, with path, which the
        // report takes; then its objects, its own visuals, its views and the scenes of its
        // viewports.
        Result enter(LayoutEntry& entry, char* path, uint32_t parent)
        {
            if (!path) return Result::OutOfMemory;
            auto scene = entry.scene;
            auto& place = entry.place;
            LayoutScene record;
            record.scene = scene;
            record.path = path;
            record.bounds = place.bounds;
            record.clipBounds = place.clip;
            record.scaleX = place.authored.scaleX;
            record.scaleY = place.authored.scaleY;
            record.parent = parent;
            record.depth = entry.depth;
            record.kind = entry.kind;
            record.clipped = _layoutDifferent(place.bounds, place.clip);
            record.stretched = place.stretched;
            record.time = entry.time;
            entry.index = report->sceneCnt;
            if (!report->add(record)) {
                delete[] path;
                return Result::OutOfMemory;
            }
            // An opaque scene hides what it covers of those before it.
            if (_opacity(entry.opacity, entry.background) == 255u) {
                _layoutOcclude(report->visuals, 0u, report->visualCnt, place.clip, INT32_MAX,
                               entry.index);
            }

            auto objectBegin = report->objectCnt;
            for (auto i = 0u; i < scene->count(); i++) {
                auto object = scene->objectAt(i);
                LayoutObject item;
                item.object = object;
                item.scene = entry.index;
                item.layer = scene->pImpl->drawLayer(object, entry.time);
                item.clipBounds = place.clip;
                // The record of the parent, which comes before its children.
                auto above = scene->pImpl->entry(object->parent());
                if (above && above < scene->pImpl->objects + i) {
                    item.parent = objectBegin + static_cast<uint32_t>(above - scene->pImpl->objects);
                }
                if (!report->add(item)) return Result::OutOfMemory;
            }

            // The scene's own visuals, which its views may cover.
            auto directBegin = report->visualCnt;
            if (entry.direct) {
                DisplayList built;
                auto result = entry.list ? Result::Success
                                         : scene->pImpl->build(entry.time, built, false);
                if (result == Result::Success) {
                    result = visuals(entry.list ? *entry.list : built, place.physical, place.clip,
                                     entry.opacity, entry.visualKind);
                }
                if (result != Result::Success) return result;
            }
            auto directEnd = report->visualCnt;

            // A view is a scene of the report that shows the same objects again, through its
            // camera, in its rectangle: the report gives where it covers the frame, not a second
            // set of objects. Opaque, it hides the scene's own visuals on its layer and below.
            auto& config = scene->config();
            auto& space = entry.space ? *entry.space : config;
            auto& physical = place.physical;
            auto spaceX = static_cast<float>(space.width) / static_cast<float>(config.width);
            auto spaceY = static_cast<float>(space.height) / static_cast<float>(config.height);
            for (auto i = 0u; i < scene->pImpl->viewCnt; i++) {
                ViewFrame frame;
                auto result = scene->pImpl->frame(i, entry.time, frame);
                if (result != Result::Success) return result;
                auto opacity = _opacity(entry.opacity, _alpha(frame.opacity, entry.fade));
                if (!frame.shown || !opacity) continue;
                LayoutScene view;
                view.scene = scene;
                view.path = layoutScenePath(report->scenes[entry.index].path,
                                            LayoutSceneKind::View, i);
                if (!view.path) return Result::OutOfMemory;
                view.bounds = {
                    physical.x + physical.scaleX * spaceX * frame.rect[0],
                    physical.y + physical.scaleY * spaceY * frame.rect[1],
                    physical.scaleX * spaceX * frame.rect[2],
                    physical.scaleY * spaceY * frame.rect[3],
                };
                _layoutClip(view.bounds, place.clip, view.clipBounds);
                view.scaleX = place.authored.scaleX * frame.scale;
                view.scaleY = place.authored.scaleY * frame.scale;
                view.parent = entry.index;
                view.depth = entry.depth + 1u;
                view.kind = LayoutSceneKind::View;
                view.clipped = _layoutDifferent(view.bounds, view.clipBounds);
                view.time = entry.time;
                auto viewIndex = report->sceneCnt;
                if (!report->add(view)) {
                    delete[] view.path;
                    return Result::OutOfMemory;
                }
                if (_opacity(opacity, frame.background.a) == 255u) {
                    _layoutOcclude(report->visuals, directBegin, directEnd, view.clipBounds,
                                   frame.layer, viewIndex);
                }
            }

            for (auto i = 0u; i < scene->viewportCount(); i++) {
                Viewport viewport;
                if (!scene->viewportAt(i, viewport)) return Result::Unknown;
                LayoutEntry child(scene->sceneAt(i), entry.time, LayoutSceneKind::Viewport,
                                  entry.descendantsOpacity, entry.descendantsKind);
                auto result = mount(entry, child, viewport, i);
                if (result == Result::Success) result = stage(child);
                if (result != Result::Success) return result;
            }
            return Result::Success;
        }

        // Places child in the rectangle viewport covers of the scene of parent, and only inside
        // frame when there is one, and enters it as the index-th scene of its kind there.
        Result mount(const LayoutEntry& parent, LayoutEntry& child, const Viewport& viewport,
                     uint32_t index, const Viewport* frame = nullptr)
        {
            auto& config = parent.scene->config();
            auto& space = parent.space ? *parent.space : config;
            auto& childConfig = child.scene->config();
            auto& from = parent.place;
            auto& place = child.place;
            place.authored = _layoutPlace(from.authored, viewport, config, childConfig, place.bounds);
            BBox bounds;
            place.physical = _layoutPlace(from.physical, viewport, space,
                                          child.space ? *child.space : childConfig, bounds);
            _layoutClip(bounds, from.clip, place.clip);
            if (frame) {
                // A scene moving through a frame shows only where it overlaps the frame.
                BBox frameBounds = {
                    from.physical.x + from.physical.scaleX
                        * frame->x * static_cast<float>(space.width),
                    from.physical.y + from.physical.scaleY
                        * frame->y * static_cast<float>(space.height),
                    from.physical.scaleX * frame->width
                        * static_cast<float>(space.width),
                    from.physical.scaleY * frame->height
                        * static_cast<float>(space.height),
                };
                auto visible = place.clip;
                _layoutClip(visible, frameBounds, place.clip);
            }
            auto relativeX = viewport.width * static_cast<float>(config.width)
                           / static_cast<float>(childConfig.width);
            auto relativeY = viewport.height * static_cast<float>(config.height)
                           / static_cast<float>(childConfig.height);
            auto tolerance = std::fmax(std::fabs(relativeX), std::fabs(relativeY)) * 1.0e-3f;
            place.stretched = std::fabs(relativeX - relativeY) > tolerance;
            child.depth = parent.depth + 1u;
            auto path = layoutScenePath(report->scenes[parent.index].path, child.kind, index);
            return enter(child, path, parent.index);
        }

        // Mounts the stage the sequence of the scene of entry shows at its time, or the two of
        // the transition it is in.
        Result stage(const LayoutEntry& entry)
        {
            auto sequence = entry.scene->pImpl->sequence;
            if (!sequence || entry.time < sequence->begin) return Result::Success;
            auto progress = 0.0f;
            auto i = sequence->stage(entry.time, progress);
            if (i >= sequence->count) return Result::Unknown;
            if (progress > 0.0f) return cross(entry, *sequence, i, progress);
            LayoutEntry child(sequence->stages[i].scene, sequence->time(i, entry.time),
                              LayoutSceneKind::Transition, entry.descendantsOpacity, entry.descendantsKind);
            auto result = mount(entry, child, sequence->viewport, i);
            return result == Result::Success ? stage(child) : result;
        }

        // Mounts stages i and i + 1 of sequence, that of the scene of entry, through the
        // transition between them, timelineProgress into it.
        Result cross(const LayoutEntry& entry, const SceneSequence& sequence, uint32_t i,
                     float timelineProgress)
        {
            auto& transition = sequence.transitions[i];
            auto from = sequence.stages[i].scene;
            auto to = sequence.stages[i + 1u].scene;
            auto fromTime = sequence.time(i, entry.time);
            auto toTime = sequence.time(i + 1u, entry.time);
            auto& fromConfig = from->config();
            auto& toConfig = to->config();
            auto progress = ease(transition.curve, timelineProgress);
            if (transition.kind != TransitionKind::Match) {
                // Each scene shows whole where the frame of the style places it, leaving or
                // arriving with everything it holds, or with matching everything but the
                // objects that pair, which travel above both in the viewport of the sequence.
                TransitionFrame frame;
                transition.frame(sequence.viewport, progress, fromConfig.background.a,
                                 toConfig.background.a, frame);
                DisplayList parts[3];
                Config spaces[3];
                if (transition.match) {
                    auto result = Scene::Impl::matched(from, to, transition, progress, fromTime,
                                                       toTime, parts, spaces);
                    if (result != Result::Success) return result;
                }
                auto place = [&](uint32_t part, const Viewport& viewport, float share,
                                 LayoutVisualKind kind) {
                    LayoutEntry child(sequence.stages[i + part].scene,
                                      sequence.time(i + part, entry.time),
                                      LayoutSceneKind::Transition,
                                      _opacity(entry.opacity, _alpha(share, entry.fade)),
                                      kind);
                    if (transition.match) {
                        child.list = &parts[part];
                        child.space = &spaces[part];
                    }
                    auto result = mount(entry, child, viewport, i + part, &sequence.viewport);
                    return result == Result::Success ? stage(child) : result;
                };
                auto outgoing = [&]() {
                    return place(0u, frame.outgoing, frame.outgoingOpacity, LayoutVisualKind::FadeOut);
                };
                auto incoming = [&]() {
                    return place(1u, frame.incoming, frame.incomingOpacity, LayoutVisualKind::FadeIn);
                };
                auto result = frame.outgoingAbove ? incoming() : outgoing();
                if (result == Result::Success) result = frame.outgoingAbove ? outgoing() : incoming();
                if (result != Result::Success || !transition.match) return result;
                // The pairs lie in the space of the outgoing scene, stretched over the viewport.
                BBox bounds;
                auto pairs = _layoutPlace(entry.place.physical, sequence.viewport,
                                          entry.space ? *entry.space : entry.scene->config(),
                                          fromConfig, bounds);
                BBox clip;
                _layoutClip(bounds, entry.place.clip, clip);
                return visuals(parts[2], pairs, clip, entry.descendantsOpacity,
                               LayoutVisualKind::Stable);
            }

            // A match blends both scenes into one list in the space of the outgoing one, whose
            // record measures it; the scenes each of them mounts fade out or in.
            DisplayList first;
            DisplayList second;
            DisplayList blended;
            auto result = from->pImpl->build(fromTime, first, false);
            if (result == Result::Success) result = to->pImpl->build(toTime, second, false);
            if (result != Result::Success) return result;
            result = blended.blend(first, second, transition.matches,
                                   from->count(), to->count(),
                                   {static_cast<float>(fromConfig.width),
                                    static_cast<float>(fromConfig.height)},
                                   {static_cast<float>(toConfig.width),
                                    static_cast<float>(toConfig.height)}, progress);
            if (result != Result::Success) return result;
            auto bounded = std::fmax(0.0f, std::fmin(1.0f, progress));
            LayoutEntry outgoing(from, fromTime, LayoutSceneKind::Transition,
                                 entry.descendantsOpacity, entry.descendantsKind);
            outgoing.list = &blended;
            outgoing.space = &fromConfig;
            outgoing.background = _mix(fromConfig.background.a, toConfig.background.a, bounded);
            outgoing.fade = _alpha(1.0f - bounded);
            outgoing.descendantsOpacity = _opacity(entry.descendantsOpacity, outgoing.fade);
            outgoing.descendantsKind = LayoutVisualKind::FadeOut;
            LayoutEntry incoming(to, toTime, LayoutSceneKind::Transition,
                                 entry.descendantsOpacity, entry.descendantsKind);
            incoming.space = &fromConfig;
            incoming.background = 0u;
            incoming.fade = _alpha(bounded);
            incoming.descendantsOpacity = _opacity(entry.descendantsOpacity, incoming.fade);
            incoming.descendantsKind = LayoutVisualKind::FadeIn;
            incoming.direct = false;
            // Both scenes enter before the stages of their own sequences.
            result = mount(entry, outgoing, sequence.viewport, i);
            if (result == Result::Success) result = mount(entry, incoming, sequence.viewport, i + 1u);
            if (result == Result::Success) result = stage(outgoing);
            return result == Result::Success ? stage(incoming) : result;
        }

        // Points each visual at the records of its objects, which take its bounds into theirs
        // and into the families of their ancestors; then adds the containments and collisions.
        Result relate(float padding)
        {
            auto find = [this](const Object* object) {
                for (auto i = 0u; i < report->objectCnt; i++) {
                    if (report->objects[i].object == object) return i;
                }
                return UINT32_MAX;
            };
            auto apply = [this](const LayoutVisual& visual, uint32_t index) {
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
                auto& source = report->sources[i];
                visual.object = find(source.object);
                if (source.counterpart) visual.counterpart = find(source.counterpart);
                if (visual.object == UINT32_MAX
                    || (source.counterpart && visual.counterpart == UINT32_MAX)) {
                    return Result::Unknown;
                }
                apply(visual, visual.object);
                if (visual.kind == LayoutVisualKind::Morph
                    && visual.counterpart != UINT32_MAX
                    && visual.counterpart != visual.object) {
                    apply(visual, visual.counterpart);
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
                    if (!report->add(containment)) return Result::OutOfMemory;
                }
            }

            auto related = [this](const LayoutVisual& first, const LayoutVisual& second) {
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
                    collision.clearance = _clearance(first.visibleBounds, second.visibleBounds,
                                                     padding, collision.separation);
                    collision.overlapping = collision.clearance.x < 0.0f
                                         && collision.clearance.y < 0.0f;
                    if (collision.overlapping) {
                        _layoutClip(first.visibleBounds, second.visibleBounds, collision.overlap);
                    }
                    if (!report->add(collision)) return Result::OutOfMemory;
                }
            }
            return Result::Success;
        }
    };

    auto report = output.pImpl;
    report->reset();
    report->sampleTime = time;
    report->samplePadding = padding;
    RasterPresence presence(scene->config());
    Walk walk = {report, presence};
    LayoutEntry root(scene, time, LayoutSceneKind::Root, 255u, LayoutVisualKind::Stable);
    root.place.bounds = {0.0f, 0.0f, static_cast<float>(scene->config().width),
                         static_cast<float>(scene->config().height)};
    root.place.clip = root.place.bounds;
    auto result = walk.enter(root, _duplicate("root"), UINT32_MAX);
    if (result == Result::Success) result = walk.stage(root);
    if (result == Result::Success) result = walk.relate(padding);
    if (result != Result::Success) report->reset();
    return result;
}

Result Renderer::render(const Scene* scene, float time, Surface& surface, uint32_t pixelRatio) noexcept
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

    return renderer::tvgRetainedDraw(pImpl->tree, pImpl->canvas, scene, time, static_cast<float>(pixelRatio));
}

}  // namespace vis
