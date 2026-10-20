// Builds a frame's display list: the draw commands of every object a scene shows at a time, as
// strokes, plots, contours, spaces, warps, text and pictures that the renderers draw.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#include "vis.h"
#include "../common/visFinite.h"
#include "../common/visConstants.h"
#include "../common/visGrow.h"
#include "../common/visSort.h"
#include "../common/visMix.h"
#include "visDisplayList.h"
#include "visScene.h"

namespace vis
{

static Color _shade(Color color, float factor, float opacity)
{
    factor = _clamp(factor);
    color.r = static_cast<uint8_t>(static_cast<float>(color.r) * factor + 0.5f);
    color.g = static_cast<uint8_t>(static_cast<float>(color.g) * factor + 0.5f);
    color.b = static_cast<uint8_t>(static_cast<float>(color.b) * factor + 0.5f);
    return _opacity(color, opacity);
}

static Style _style(const Object* object, const VisualState& state)
{
    auto style = object->style;
    style.stroke = _opacity(state.stroke, state.opacity);
    style.fill = _opacity(state.fill, state.opacity * state.fillProgress);
    style.gradientEnd = state.gradientEnd;
    style.gradient = state.gradient;
    style.width = state.width * state.pixelScale;
    style.radius = state.radius * state.pixelScale;
    for (auto i = 0u; i < style.dashCount; i++) style.dash[i] *= state.pixelScale;
    style.dashOffset = state.dashOffset * state.pixelScale;
    return style;
}

static Vec2 _pixel(const Vec3& ndc, const Config& config)
{
    return {
        (ndc.x + 1.0f) * 0.5f * static_cast<float>(config.width),
        (1.0f - ndc.y) * 0.5f * static_cast<float>(config.height)};
}

static bool _point(const Vec3& point, const Mat4& model, const Config& config, Vec2& pixel, float& depth)
{
    Vec3 ndc;
    auto aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
    if (!config.camera.project(model * point, aspect, ndc)) return false;
    pixel = _pixel(ndc, config);
    depth = ndc.z;
    return true;
}

// The depth of a point as DrawCommand::depths keeps it, from the depth its projection sorts at,
// which runs from 0 at the near plane to 1 at the far one.
static float _screenDepth(float depth, const Config& config)
{
    auto& camera = config.camera;
    auto distance = static_cast<double>(camera.near)
                  + static_cast<double>(depth) * (static_cast<double>(camera.far) - camera.near);
    if (camera.projection != Projection::Perspective) return static_cast<float>(distance);
    return distance > 0.0 ? static_cast<float>(1.0 / distance) : 0.0f;
}

// Whether a scene keeps the depths of its points for split(): only in 3D.
static bool _spatial(const Config& config)
{
    return config.cameraView == CameraView::ThreeD;
}

// Projects count points, which at(i) gives in the space of model, into pixels. depths, unless it
// is null, takes the depth of each point as DrawCommand::depths keeps it, and depth their mean
// depth; false when a point does not project.
template<typename At>
static bool _project(uint32_t count, At at, const Mat4& model, const Config& config, Vec2* pixels, float* depths,
                     float& depth)
{
    auto sum = 0.0;
    for (auto i = 0u; i < count; i++) {
        auto pointDepth = 0.0f;
        if (!_point(at(i), model, config, pixels[i], pointDepth)) return false;
        if (depths) depths[i] = _screenDepth(pointDepth, config);
        sum += pointDepth;
    }
    depth = count ? static_cast<float>(sum / count) : 0.0f;
    return true;
}

// The same for count points in an array.
static bool _project(const Vec3* points, uint32_t count, const Mat4& model, const Config& config, Vec2* pixels,
                     float* depths, float& depth)
{
    return _project(count, [points](uint32_t i) { return points[i]; }, model, config, pixels, depths, depth);
}

// ends: where the depths of the two ends go, as DrawCommand::depths keeps them, or nullptr.
static bool _lineWorld(Vec3 from, Vec3 to, const Mat4& model, const Config& config, Vec2& a, Vec2& b, float& depth,
                       float* ends = nullptr)
{
    from = model * from;
    to = model * to;
    if (!config.camera.segment(from, to)) return false;
    Vec3 fromNdc;
    Vec3 toNdc;
    auto aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
    if (!config.camera.project(from, aspect, fromNdc)) return false;
    if (!config.camera.project(to, aspect, toNdc)) return false;
    a = _pixel(fromNdc, config);
    b = _pixel(toNdc, config);
    depth = (fromNdc.z + toNdc.z) * 0.5f;
    if (ends) {
        ends[0] = _screenDepth(fromNdc.z, config);
        ends[1] = _screenDepth(toNdc.z, config);
    }
    return true;
}

static float _pixelLength(const Vec3& point, float length, const Mat4& model,
                          const Config& config, const Vec2& pixel)
{
    if (length <= 0.0f) return 0.0f;
    Vec2 x;
    Vec2 y;
    float depth;
    if (!_point(point + Vec3{length, 0.0f, 0.0f}, model, config, x, depth)
        || !_point(point + Vec3{0.0f, length, 0.0f}, model, config, y, depth)) {
        return -1.0f;
    }
    return ((x - pixel).length() + (y - pixel).length()) * 0.5f;
}

// Moves explicit gradient geometry from local coordinates into output pixels with the
// same model/camera projection as the Shape points. Geometry that cannot be projected
// falls back to the renderer's automatic shape-derived placement.
static void _project(Gradient& gradient, const Mat4& model, const Config& config)
{
    if (!gradient.enabled()) return;
    float depth;
    if (gradient.line) {
        Vec2 a;
        Vec2 b;
        if (_point(gradient.from, model, config, a, depth)
            && _point(gradient.to, model, config, b, depth)) {
            gradient.from = {a.x, a.y, 0.0f};
            gradient.to = {b.x, b.y, 0.0f};
        } else {
            gradient.line = false;
        }
    }
    Vec2 center;
    auto anchor = gradient.centered ? gradient.center : Vec3{};
    auto anchored = _point(anchor, model, config, center, depth);
    if (gradient.type == GradientType::Conic && anchored) {
        constexpr auto RADIANS = PI / 180.0f;
        auto angle = gradient.angle * RADIANS;
        Vec2 direction;
        // Clockwise as seen on screen: negate the local y component of the start direction.
        if (_point(anchor + Vec3{std::cos(angle), -std::sin(angle), 0.0f}, model, config,
                   direction, depth)) {
            auto delta = direction - center;
            gradient.angle = std::atan2(delta.y, delta.x) / RADIANS;
        }
    }
    if (gradient.type == GradientType::Radial) {
        if (gradient.sized) {
            auto radius = anchored ? _pixelLength(anchor, gradient.radius, model, config, center)
                                   : -1.0f;
            if (radius > 0.0f) gradient.radius = radius;
            else gradient.sized = false;
        }
        if (gradient.focused) {
            Vec2 focal;
            if (_point(gradient.focal, model, config, focal, depth)) {
                gradient.focal = {focal.x, focal.y, 0.0f};
            } else {
                gradient.focused = false;
            }
        }
        if (gradient.focalRadius > 0.0f) {
            auto radius = anchored
                              ? _pixelLength(anchor, gradient.focalRadius, model, config, center)
                              : -1.0f;
            gradient.focalRadius = radius > 0.0f ? radius : 0.0f;
        }
    }
    if (gradient.centered) {
        if (anchored) gradient.center = {center.x, center.y, 0.0f};
        else gradient.centered = false;
    }
}

static bool _reverse(DrawDirection direction, const Vec2* points = nullptr, uint32_t count = 0,
                     bool closed = false)
{
    if (direction == DrawDirection::Reverse) return true;
    if (direction == DrawDirection::Forward) return false;
    if (!closed || !points || count < 3u) return direction == DrawDirection::Clockwise;
    auto area = 0.0;
    for (auto i = 0u; i < count; i++) {
        auto& from = points[i];
        auto& to = points[(i + 1u) % count];
        area += static_cast<double>(from.x) * to.y - static_cast<double>(from.y) * to.x;
    }
    if (direction == DrawDirection::Clockwise) return area < 0.0;
    return area > 0.0;
}

// How the strokes of object end: flat for a plot that asks for it, so an end cut at a boundary
// stays on it, and round otherwise.
static PathCap _cap(const Object* object)
{
    auto flat = object->type() == Type::Plot && static_cast<const Plot*>(object)->flat;
    return flat ? PathCap::Butt : PathCap::Round;
}

// What a stroke draws with: the share of it that shows, the end it starts from, its style, and
// the layer and mean depth of its commands. depths: the depth of each point it passes, as
// DrawCommand::depths keeps them, or nullptr. traceFill: a closed outline fills as it draws on,
// in _path and _contours. cap: how the open strokes of _path and _pieces end; the other strokes
// choose their own.
struct Pen
{
    float progress;
    DrawDirection direction;
    Style style;
    int32_t layer;
    float depth = 0.0f;
    const float* depths = nullptr;
    bool traceFill = false;
    PathCap cap = PathCap::Round;
};

// The styles of an outline on its way: fill, its fill without a stroke, which a pen that traces
// its fill draws first; stroke, its stroke without the fill, in the color of the fill when such
// a pen has no stroke color.
static void _traceStyles(const Pen& pen, Style& fill, Style& stroke)
{
    fill = pen.style;
    fill.stroke.a = 0;
    fill.width = 0.0f;
    stroke = pen.style;
    stroke.fill.a = 0;
    if (pen.traceFill && !stroke.stroke.a && pen.style.fill.a) {
        stroke.stroke = pen.style.fill;
        if (stroke.width <= 0.0f) stroke.width = 2.0f;
    }
}

// The style of a mark on a stroke, a route's head or a plot's endpoint: filled in the color of
// the stroke, with no stroke of its own.
static Style _markerStyle(const Style& style)
{
    auto marker = style;
    marker.fill = style.stroke;
    marker.stroke.a = 0u;
    marker.width = 0.0f;
    return marker;
}

// pen.depths: the depths of from and to, or nullptr.
static Result _line(DisplayList& list, const Vec2& from, const Vec2& to, const Pen& pen)
{
    auto value = _clamp(pen.progress);
    if (value <= 0.0f) return Result::Success;
    auto reverse = _reverse(pen.direction);
    auto start = reverse ? to : from;
    auto end = reverse ? from : to;
    Vec2 points[2] = {start, start + (end - start) * value};
    float depths[2] = {};
    if (pen.depths) {
        auto first = reverse ? pen.depths[1] : pen.depths[0];
        auto last = reverse ? pen.depths[0] : pen.depths[1];
        depths[0] = first;
        depths[1] = first + (last - first) * value;
    }
    return list.path(points, 2, false, pen.style, pen.layer, pen.depth, PathCap::Round, nullptr, 0u,
                     pen.depths ? depths : nullptr);
}

static float _arrowHalfWidth(float length, const Style& style)
{
    return std::fmin(length * 0.55f,
                     std::fmax(length * 0.46f, style.width * 0.75f));
}

// ends: the depths of the point and of the middle of the base, as DrawCommand::depths keeps
// them, or nullptr.
static Result _routeMarker(DisplayList& list, const Vec2& point, const Vec2& direction,
                           float length, bool start, const Style& style, int32_t layer,
                           float depth, const float* ends = nullptr)
{
    if (length <= 0.0f) return Result::Success;
    auto normal = Vec2{-direction.y, direction.x};
    auto base = start ? point + direction * length : point - direction * length;
    auto half = _arrowHalfWidth(length, style);
    Vec2 points[] = {point, base + normal * half, base - normal * half};
    // A head keeps the gradient of its stroke but not its dashes.
    auto marker = _markerStyle(style);
    marker.dashCount = 0u;
    marker.dashOffset = 0.0f;
    float depths[3] = {};
    if (ends) {
        depths[0] = ends[0];
        depths[1] = ends[1];
        depths[2] = ends[1];
    }
    return list.path(points, 3u, true, marker, layer, depth, PathCap::Round, nullptr, 0u,
                     ends ? depths : nullptr);
}

// depths: the depths of the points, which move with them, or nullptr.
static uint32_t _trimRoute(Vec2* points, uint32_t count, float start, float end,
                           float* depths = nullptr)
{
    auto first = 0u;
    while (first + 1u < count && start > 1e-6f) {
        auto segment = points[first + 1u] - points[first];
        auto length = segment.length();
        if (length <= 1e-6f) {
            first++;
            continue;
        }
        if (start < length) {
            points[first] = points[first] + segment * (start / length);
            if (depths) depths[first] += (depths[first + 1u] - depths[first]) * (start / length);
            break;
        }
        start -= length;
        first++;
    }
    if (first) {
        count -= first;
        for (auto i = 0u; i < count; i++) points[i] = points[i + first];
        if (depths) {
            for (auto i = 0u; i < count; i++) depths[i] = depths[i + first];
        }
    }

    while (count > 1u && end > 1e-6f) {
        auto segment = points[count - 1u] - points[count - 2u];
        auto length = segment.length();
        if (length <= 1e-6f) {
            count--;
            continue;
        }
        if (end < length) {
            points[count - 1u] = points[count - 1u] - segment * (end / length);
            if (depths) {
                depths[count - 1u] -= (depths[count - 1u] - depths[count - 2u]) * (end / length);
            }
            break;
        }
        end -= length;
        count--;
    }
    return count;
}

// pen.depths: the depth of each point, or nullptr. tail and tip: the lengths in pixels of the
// heads at its start and end.
static Result _route(DisplayList& list, const Vec2* points, uint32_t count, float tail, float tip, const Pen& pen)
{
    if (!points || count < 2u) return Result::InvalidArguments;
    auto value = _clamp(pen.progress);
    if (value <= 0.0f) return Result::Success;
    auto reverse = value < 1.0f - 1e-6f && _reverse(pen.direction, points, count, false);
    auto depths = pen.depths;
    auto point = [&](uint32_t index) {
        return reverse ? points[count - 1u - index] : points[index];
    };
    auto measure = [&](uint32_t index) {
        return reverse ? depths[count - 1u - index] : depths[index];
    };
    auto total = 0.0f;
    for (auto i = 1u; i < count; i++) total += (point(i) - point(i - 1u)).length();
    if (!std::isfinite(total) || total <= 1e-6f) return Result::Success;

    auto remaining = total * value;
    auto output = std::unique_ptr<Vec2[]>(new (std::nothrow) Vec2[count]);
    auto outputDepths = depths ? std::unique_ptr<float[]>(new (std::nothrow) float[count]) : nullptr;
    if (!output || (depths && !outputDepths)) return Result::OutOfMemory;
    auto outputCount = 1u;
    output[0] = point(0u);
    if (depths) outputDepths[0] = measure(0u);
    for (auto i = 1u; i < count && remaining > 1e-6f; i++) {
        auto from = point(i - 1u);
        auto to = point(i);
        auto segment = to - from;
        auto length = segment.length();
        if (!std::isfinite(length)) return Result::InvalidArguments;
        if (length <= 1e-6f) continue;
        if (remaining >= length) {
            if (depths) outputDepths[outputCount] = measure(i);
            output[outputCount++] = to;
            remaining -= length;
        } else {
            if (depths) {
                outputDepths[outputCount] = measure(i - 1u) + (measure(i) - measure(i - 1u)) * (remaining / length);
            }
            output[outputCount++] = from + segment * (remaining / length);
            remaining = 0.0f;
        }
    }
    if (outputCount < 2u) return Result::Success;

    auto firstDelta = output[1u] - output[0u];
    auto firstLength = firstDelta.length();
    auto lastDelta = output[outputCount - 1u] - output[outputCount - 2u];
    auto lastLength = lastDelta.length();
    if (firstLength <= 1e-6f || lastLength <= 1e-6f) return Result::Success;
    auto firstDirection = firstDelta * (1.0f / firstLength);
    auto lastDirection = lastDelta * (1.0f / lastLength);
    auto visibleLength = total * value;
    auto startMarker = std::fmin(reverse ? tip : tail, visibleLength * 0.45f);
    auto endMarker = std::fmin(reverse ? tail : tip, visibleLength * 0.45f);

    auto startPoint = output[0u];
    auto endPoint = output[outputCount - 1u];
    // The depths of the two tips and of the bases of their heads, where the shaft now ends.
    float startEnds[2] = {};
    float endEnds[2] = {};
    if (depths) {
        startEnds[0] = outputDepths[0u];
        endEnds[0] = outputDepths[outputCount - 1u];
    }
    auto shaftCount = _trimRoute(output.get(), outputCount, startMarker, endMarker, outputDepths.get());
    if (depths) {
        auto middle = (startEnds[0] + endEnds[0]) * 0.5f;
        startEnds[1] = shaftCount >= 2u ? outputDepths[0u] : middle;
        endEnds[1] = shaftCount >= 2u ? outputDepths[shaftCount - 1u] : middle;
    }

    auto shaft = pen.style;
    shaft.fill.a = 0u;
    auto result = shaftCount >= 2u
                ? list.path(output.get(), shaftCount, false, shaft, pen.layer, pen.depth, PathCap::Butt, nullptr,
                            0u, outputDepths.get())
                : Result::Success;
    if (result == Result::Success) {
        result = _routeMarker(list, startPoint, firstDirection, startMarker, true, pen.style,
                              pen.layer, pen.depth, depths ? startEnds : nullptr);
    }
    if (result == Result::Success) {
        result = _routeMarker(list, endPoint, lastDirection, endMarker,
                              false, pen.style, pen.layer, pen.depth, depths ? endEnds : nullptr);
    }
    return result;
}

// pen.depths: the depth of each point, or nullptr; the stroke keeps them when it draws no fill.
static Result _path(DisplayList& list, const Vec2* points, uint32_t count, bool closed, const Pen& pen)
{
    if (!points || count < 2) return Result::InvalidArguments;
    auto value = _clamp(pen.progress);
    if (value <= 0.0f) return Result::Success;
    auto segments = closed ? count : count - 1;
    auto scaled = value * static_cast<float>(segments);
    if (scaled <= 1e-6f) return Result::Success;
    auto complete = static_cast<uint32_t>(scaled);
    if (complete > segments) complete = segments;
    auto outputCnt = complete + 1;
    auto partial = scaled - static_cast<float>(complete);
    if (complete < segments && partial > 1e-6f) outputCnt++;

    auto depths = pen.depths;
    auto output = new (std::nothrow) Vec2[outputCnt];
    auto outputDepths = depths ? new (std::nothrow) float[outputCnt] : nullptr;
    if (!output || (depths && !outputDepths)) {
        delete[] output;
        delete[] outputDepths;
        return Result::OutOfMemory;
    }
    auto reverse = _reverse(pen.direction, points, count, closed);
    auto at = [&](uint32_t index) {
        if (!reverse) return index % count;
        if (closed) return (count - index % count) % count;
        return count - 1u - index;
    };
    for (auto i = 0u; i <= complete; i++) {
        output[i] = points[at(i)];
        if (depths) outputDepths[i] = depths[at(i)];
    }
    if (complete < segments && partial > 1e-6f) {
        auto from = points[at(complete)];
        auto to = points[at(complete + 1u)];
        output[outputCnt - 1] = from + (to - from) * partial;
        if (depths) {
            auto first = depths[at(complete)];
            outputDepths[outputCnt - 1] = first + (depths[at(complete + 1u)] - first) * partial;
        }
    }
    auto finished = value >= 1.0f - 1e-6f;
    Style fill;
    Style stroke;
    _traceStyles(pen, fill, stroke);
    auto result = Result::Success;
    if (pen.traceFill && closed && !finished && outputCnt >= 3u && pen.style.fill.a) {
        result = list.path(output, outputCnt, true, fill, pen.layer, pen.depth);
    }
    if (result == Result::Success) {
        auto& style = finished ? pen.style : stroke;
        result = list.path(output, outputCnt, closed && finished, style, pen.layer, pen.depth, pen.cap,
                           nullptr, 0u, style.fill.a ? nullptr : outputDepths);
    }
    delete[] output;
    delete[] outputDepths;
    return result;
}

// The pieces of an open polyline, ends and contours as in DrawCommand, as strokes: at once,
// with their ends, or, given the depth of each point, each piece on its own, since a stroke keeps
// its depths only as one contour (see DisplayList::path), so that split() cuts each where a
// surface hides it.
static Result _strokes(DisplayList& list, const Vec2* points, const float* depths, const uint32_t* ends,
                       uint32_t contours, const Style& style, int32_t layer, float depth, PathCap cap)
{
    if (!depths) {
        return list.path(points, ends[contours - 1u], false, style, layer, depth, cap,
                         contours > 1u ? ends : nullptr, contours);
    }
    auto start = 0u;
    for (auto piece = 0u; piece < contours; piece++) {
        if (ends[piece] >= start + 2u) {
            auto result = list.path(points + start, ends[piece] - start, false, style, layer, depth, cap,
                                    nullptr, 0u, depths + start);
            if (result != Result::Success) return result;
        }
        start = ends[piece];
    }
    return Result::Success;
}

// Draw progress traverses piece segments, skipping gaps; reversed progress starts at
// the last piece. Unfilled strokes retain optional per-point depths.
static Result _pieces(DisplayList& list, const Vec2* points, const uint32_t* ends, uint32_t contours, const Pen& pen)
{
    auto value = _clamp(pen.progress);
    if (value <= 0.0f) return Result::Success;
    auto count = ends[contours - 1u];
    auto depths = pen.depths;
    if (value >= 1.0f - 1e-6f) {
        return _strokes(list, points, pen.style.fill.a ? nullptr : depths, ends, contours, pen.style, pen.layer,
                        pen.depth, pen.cap);
    }
    auto segments = 0u;
    auto start = 0u;
    for (auto piece = 0u; piece < contours; piece++) {
        if (ends[piece] >= start + 2u) segments += ends[piece] - start - 1u;
        start = ends[piece];
    }
    auto remaining = value * static_cast<float>(segments);
    if (remaining <= 1e-6f) return Result::Success;
    auto output = new (std::nothrow) Vec2[count + 1u];
    auto outputDepths = depths ? new (std::nothrow) float[count + 1u] : nullptr;
    auto shown = new (std::nothrow) uint32_t[contours];
    if (!output || !shown || (depths && !outputDepths)) {
        delete[] output;
        delete[] outputDepths;
        delete[] shown;
        return Result::OutOfMemory;
    }
    auto reverse = _reverse(pen.direction);
    auto total = 0u;
    auto kept = 0u;
    for (auto step = 0u; step < contours && remaining > 1e-6f; step++) {
        auto piece = reverse ? contours - 1u - step : step;
        auto first = piece ? ends[piece - 1u] : 0u;
        auto last = ends[piece];
        if (last < first + 2u) continue;
        auto pieceSegments = last - first - 1u;
        auto at = [&](uint32_t index) { return reverse ? last - 1u - index : first + index; };
        auto complete = remaining >= static_cast<float>(pieceSegments) ? pieceSegments : static_cast<uint32_t>(remaining);
        auto partial = remaining - static_cast<float>(complete);
        auto begin = total;
        for (auto i = 0u; i <= complete; i++) {
            if (outputDepths) outputDepths[total] = depths[at(i)];
            output[total++] = points[at(i)];
        }
        if (complete < pieceSegments && partial > 1e-6f) {
            auto from = points[at(complete)];
            if (outputDepths) {
                auto lead = depths[at(complete)];
                outputDepths[total] = lead + (depths[at(complete + 1u)] - lead) * partial;
            }
            output[total++] = from + (points[at(complete + 1u)] - from) * partial;
        }
        remaining -= static_cast<float>(pieceSegments);
        // A piece shows once it has a segment of its own.
        if (total - begin >= 2u) shown[kept++] = total;
        else total = begin;
    }
    auto result = Result::Success;
    if (kept) {
        auto strokeStyle = pen.style;
        strokeStyle.fill.a = 0;
        result = _strokes(list, output, outputDepths, shown, kept, strokeStyle, pen.layer, pen.depth, pen.cap);
    }
    delete[] output;
    delete[] outputDepths;
    delete[] shown;
    return result;
}

// The active vertex uses the same segment progress as a plot's stroke, skipping piece gaps.
static Result _plotEndpoint(DisplayList& list, const Vec2* points, uint32_t count, const uint32_t* ends,
                            uint32_t pieces, float radius, const Pen& pen)
{
    if (!count || !pieces || radius <= 0.0f) return Result::Success;
    auto reverse = _reverse(pen.direction);
    auto remaining = _clamp(pen.progress) * static_cast<float>(count - pieces);
    auto index = reverse ? count - 1u : 0u;
    auto next = index;
    auto partial = 0.0f;
    for (auto step = 0u; step < pieces; step++) {
        auto piece = reverse ? pieces - 1u - step : step;
        auto first = piece && ends ? ends[piece - 1u] : 0u;
        auto last = ends ? ends[piece] : count;
        auto segments = last - first - 1u;
        auto at = [&](uint32_t i) { return reverse ? last - 1u - i : first + i; };
        if (remaining <= static_cast<float>(segments) || step + 1u == pieces) {
            auto complete = std::min(static_cast<uint32_t>(remaining), segments);
            index = at(complete);
            next = at(std::min(complete + 1u, segments));
            partial = std::min(remaining - static_cast<float>(complete), 1.0f);
            break;
        }
        remaining -= static_cast<float>(segments);
    }
    auto center = points[index] + (points[next] - points[index]) * partial;
    // The endpoint is solid, whatever gradient the plot has.
    auto marker = _markerStyle(pen.style);
    marker.gradient = {};
    auto depths = pen.depths;
    auto anchor = depths ? depths[index] + (depths[next] - depths[index]) * partial : 0.0f;
    return list.circle(center, radius, marker, pen.layer, pen.depth, depths ? &anchor : nullptr);
}

// The stroke of an open polyline from the share start of its length to the share end, as a
// passing flash draws it.
static Result _window(DisplayList& list, const Vec2* points, uint32_t count, float start, float end,
                      const Style& style, int32_t layer, float depth)
{
    if (!points || count < 2u) return Result::InvalidArguments;
    auto total = 0.0f;
    for (auto i = 1u; i < count; i++) total += (points[i] - points[i - 1u]).length();
    auto from = total * _clamp(start);
    auto to = total * _clamp(end);
    if (to - from <= 1e-6f) return Result::Success;
    auto output = new (std::nothrow) Vec2[count];
    if (!output) return Result::OutOfMemory;
    auto outputCnt = 0u;
    auto walked = 0.0f;
    for (auto i = 1u; i < count && walked < to; i++) {
        auto segment = points[i] - points[i - 1u];
        auto length = segment.length();
        if (length <= 0.0f) continue;
        auto next = walked + length;
        if (next > from) {
            if (!outputCnt) output[outputCnt++] = points[i - 1u] + segment * (std::fmax(from - walked, 0.0f) / length);
            output[outputCnt++] = next <= to ? points[i] : points[i - 1u] + segment * ((to - walked) / length);
        }
        walked = next;
    }
    auto stroke = style;
    stroke.fill.a = 0;
    auto result = outputCnt >= 2u ? list.path(output, outputCnt, false, stroke, layer, depth) : Result::Success;
    delete[] output;
    return result;
}

// A path of several contours, ends and contours as in DrawCommand. Finished, it draws at once
// with its holes; while it draws, every contour shows the same share of its own outline. Its
// strokes end round and keep no depths.
static Result _contours(DisplayList& list, const Vec2* points, const uint32_t* ends, uint32_t contours, bool closed,
                        const Pen& pen)
{
    auto value = _clamp(pen.progress);
    if (value <= 0.0f) return Result::Success;
    auto count = ends[contours - 1u];
    if (value >= 1.0f - 1e-6f) {
        return list.path(points, count, closed, pen.style, pen.layer, pen.depth, PathCap::Round, ends, contours);
    }
    auto output = new (std::nothrow) Vec2[count + contours];
    auto shown = new (std::nothrow) uint32_t[contours];
    if (!output || !shown) {
        delete[] output;
        delete[] shown;
        return Result::OutOfMemory;
    }
    auto total = 0u;
    auto kept = 0u;
    auto start = 0u;
    for (auto contour = 0u; contour < contours; contour++) {
        auto part = points + start;
        auto size = ends[contour] - start;
        start = ends[contour];
        if (size < 2u) continue;
        auto segments = closed ? size : size - 1u;
        auto scaled = value * static_cast<float>(segments);
        if (scaled <= 1e-6f) continue;
        auto complete = static_cast<uint32_t>(scaled);
        if (complete > segments) complete = segments;
        auto partial = scaled - static_cast<float>(complete);
        auto reverse = _reverse(pen.direction, part, size, closed);
        auto point = [&](uint32_t index) {
            if (!reverse) return part[index % size];
            if (closed) return part[(size - index % size) % size];
            return part[size - 1u - index];
        };
        for (auto i = 0u; i <= complete; i++) output[total++] = point(i);
        if (complete < segments && partial > 1e-6f) {
            auto from = point(complete);
            output[total++] = from + (point(complete + 1u) - from) * partial;
        }
        shown[kept++] = total;
    }
    auto result = Result::Success;
    auto many = kept > 1u ? shown : nullptr;
    Style fill;
    Style stroke;
    _traceStyles(pen, fill, stroke);
    if (kept && pen.traceFill && closed && pen.style.fill.a) {
        result = list.path(output, total, true, fill, pen.layer, pen.depth, PathCap::Round, many, kept);
    }
    if (kept && result == Result::Success) {
        result = list.path(output, total, false, stroke, pen.layer, pen.depth, PathCap::Round, many, kept);
    }
    delete[] output;
    delete[] shown;
    return result;
}

static Result _gridLine(DisplayList& list, const Vec3& from, const Vec3& to, const VisualState& state, const Config& config, const Style& style, int32_t layer)
{
    Vec2 a;
    Vec2 b;
    float depth;
    float ends[2];
    auto spatial = _spatial(config);
    if (!_lineWorld(from, to, state.model, config, a, b, depth, spatial ? ends : nullptr)) return Result::Success;
    return _line(list, a, b, Pen{state.progress, state.direction, style, layer, depth, spatial ? ends : nullptr});
}

static float _basisLength(const Mat4& model, uint8_t axis)
{
    auto x = static_cast<double>(model.e[axis]);
    auto y = static_cast<double>(model.e[axis + 4u]);
    auto z = static_cast<double>(model.e[axis + 8u]);
    auto length = std::hypot(x, y, z);
    if (!std::isfinite(length) || length > std::numeric_limits<float>::max()) return 0.0f;
    return static_cast<float>(length);
}

// A tick of a space at pixel, length pixels long, centered on its axis and square to it on
// screen, where the axis runs from pixel toward ahead. ends: the depths of its ends, as _line
// takes them, or nullptr.
static Result _tick(DisplayList& list, const Vec2& pixel, const Vec2& ahead, float length, const VisualState& state,
                    const Style& style, int32_t layer, float depth, const float* ends = nullptr)
{
    auto run = ahead - pixel;
    auto norm = run.length();
    if (!(length > 0.0f) || !(norm > 1.0e-6f) || !std::isfinite(norm)) return Result::Success;
    auto half = Vec2{-run.y, run.x} * (0.5f * length / norm);
    return _line(list, pixel - half, pixel + half, Pen{state.progress, state.direction, style, layer, depth, ends});
}

// How a space draws over its grid, which draws in the space's style and layer: its axes and
// ticks one layer up, in the axis width, and its numbers another layer up.
struct SpacePens
{
    Style axis;
    int32_t axisLayer = 0;
    Style number;
    float numberSize = 0.0f;
    int32_t numberLayer = 0;
};

static SpacePens _spacePens(const Space* space, const VisualState& state, const Style& style)
{
    SpacePens pens;
    pens.axis = style;
    pens.axis.width = space->axisWidth < 0.0f ? std::fmax(style.width, 1.5f) : space->axisWidth * state.pixelScale;
    pens.axisLayer = state.layer == std::numeric_limits<int32_t>::max() ? state.layer : state.layer + 1;
    pens.number.stroke.a = 0;
    pens.number.fill = _opacity(space->numberColor, state.opacity * state.progress);
    pens.numberSize = space->numberSize * state.pixelScale;
    pens.numberLayer = pens.axisLayer == std::numeric_limits<int32_t>::max() ? pens.axisLayer : pens.axisLayer + 1;
    return pens;
}

// The color an axis of a space and its ticks draw in.
static Color _axisColor(const Space* space, uint8_t axis, float opacity)
{
    return _opacity(axis == 0u ? space->axisX : axis == 1u ? space->axisY : space->axisZ, opacity);
}

// Where a number of a space stands from the point of its value on axis, at a pixel scale of one.
static Vec2 _numberOffset(const Space* space, uint8_t axis)
{
    return axis == 0u ? Vec2{0.0f, space->numberSize * 0.9f}
         : axis == 1u ? Vec2{-space->numberSize * 0.9f, 0.0f}
                      : Vec2{space->numberSize * 0.75f, -space->numberSize * 0.55f};
}

// A space: its grid, its axes and the ticks across them in the colors of the axes, and its
// numbers (see spaceParts()). The axes and numbers draw over the grid; split() lowers the parts
// that a surface in the grid's layer hides. In 3D both ends of a tick, which lies flat on the
// screen, keep the depth of its point, as a number keeps its anchor, so that split() treats it
// as it treats the axis it crosses.
static Result _space(DisplayList& list, const Space* space, const VisualState& state, const Config& config,
                     const Style& style)
{
    auto pens = _spacePens(space, state, style);
    auto spatial = _spatial(config);
    auto tickLength = space->tickLength * state.pixelScale;
    auto numbers = 0u;
    auto result = Result::Success;
    auto line = [&](WarpPart part, const Vec3& from, const Vec3& to) {
        if (part == WarpPart::Grid || part == WarpPart::DepthGrid) {
            result = _gridLine(list, from, to, state, config, style, state.layer);
            return result == Result::Success;
        }
        list.lift = static_cast<uint8_t>(pens.axisLayer - state.layer);
        pens.axis.stroke = _axisColor(space, spaceAxis(part), state.opacity);
        if (part == WarpPart::AxisX || part == WarpPart::AxisY || part == WarpPart::AxisZ) {
            result = _gridLine(list, from, to, state, config, pens.axis, pens.axisLayer);
            return result == Result::Success;
        }
        Vec2 pixel;
        Vec2 ahead;
        float depth;
        float aheadDepth;
        if (!(tickLength > 0.0f) || !_point(from, state.model, config, pixel, depth)
            || !_point(to, state.model, config, ahead, aheadDepth)) {
            return true;
        }
        auto anchor = _screenDepth(depth, config);
        float ends[2] = {anchor, anchor};
        result = _tick(list, pixel, ahead, tickLength, state, pens.axis, pens.axisLayer, depth,
                       spatial ? ends : nullptr);
        return result == Result::Success;
    };
    auto number = [&](WarpPart part, const Vec3& point, float value) {
        if (pens.numberSize <= 0.0f) return false;
        auto axis = spaceAxis(part);
        auto scale = space->numberMode == NumberMode::Relative ? _basisLength(state.model, axis) : 1.0f;
        Vec2 pixel;
        float depth;
        if (scale <= 0.0f || !_point(point, state.model, config, pixel, depth)) return true;
        auto anchor = _screenDepth(depth, config);
        list.lift = static_cast<uint8_t>(pens.numberLayer - state.layer);
        result = list.number(pixel + _numberOffset(space, axis) * state.pixelScale, value * scale, pens.numberSize,
                             {0.5f, 0.5f}, pens.number, pens.numberLayer, depth, spatial ? &anchor : nullptr);
        return result == Result::Success && ++numbers < MAX_SPACE_NUMBERS;
    };
    spaceParts(space, config.cameraView == CameraView::ThreeD, line, number);
    list.lift = 0u;
    return result;
}

// Points begin to end of a warp at the progress state has reached, projected into output; false
// when one does not project. Depth is their mean depth.
static bool _warpPoints(const WarpShape& warp, uint32_t begin, uint32_t end, const VisualState& state,
                        const Config& config, Vec2* output, float& depth)
{
    auto at = [&](uint32_t i) { return warp.at(begin + i, state.warpProgress); };
    return _project(end - begin, at, state.model, config, output, nullptr, depth);
}

// The shape of an object a warp bends, from the points its warp has reached: its outline, the
// contours of a path, or the shaft of an arrow, a vector or a route, whose tips keep their size
// in pixels and point along the bent ends. A bent line draws no fill.
static Result _warped(DisplayList& list, const Object* object, const VisualState& state, const Config& config,
                      const Style& style, float progress, bool windowed, bool endpoint)
{
    auto& warp = *state.warp;
    if (warp.count < 2u) return Result::Success;
    auto runs = warp.runCount;
    auto projected = new (std::nothrow) Vec2[warp.count];
    auto ends = runs > 1u ? new (std::nothrow) uint32_t[runs] : nullptr;
    if (!projected || (runs > 1u && !ends)) {
        delete[] projected;
        delete[] ends;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; ends && i < runs; i++)
        ends[i] = warp.runs[i].end;
    auto pen = Pen{progress, state.direction, style, state.layer, 0.0f, nullptr, state.traceFill, _cap(object)};
    auto result = Result::Success;
    auto visible = _warpPoints(warp, 0u, warp.count, state, config, projected, pen.depth);
    if (visible) {
        auto type = object->type();
        if (type == Type::Arrow || type == Type::Vector || type == Type::Route) {
            result = _route(list, projected, warp.count, state.markerTail * state.pixelScale,
                            state.markerTip * state.pixelScale, pen);
        } else if (ends) {
            result = type == Type::Plot ? _pieces(list, projected, ends, runs, pen)
                                        : _contours(list, projected, ends, runs, warp.closed, pen);
        } else if (windowed) {
            result = _window(list, projected, warp.count, state.start, progress, style, object->layer, pen.depth);
        } else {
            if (type == Type::Line) pen.style.fill.a = 0;
            result = _path(list, projected, warp.count, warp.closed, pen);
        }
    }
    if (result == Result::Success && visible && endpoint && !windowed) {
        result = _plotEndpoint(list, projected, warp.count, ends, runs,
                               static_cast<const Plot*>(object)->endpointRadius * state.pixelScale, pen);
    }
    delete[] projected;
    delete[] ends;
    return result;
}

// A space a warp bends: its grid lines and axes as the lines the warp has reached, in the styles
// and layers of the space, and its numbers where the warp has carried the points they stand at.
static Result _warpedSpace(DisplayList& list, const Space* space, const VisualState& state,
                           const Config& config, const Style& style)
{
    auto& warp = *state.warp;
    auto projected = new (std::nothrow) Vec2[warp.count ? warp.count : 1u];
    if (!projected) return Result::OutOfMemory;
    auto depthView = config.cameraView == CameraView::ThreeD;
    auto lineStyle = style;
    lineStyle.fill.a = 0;
    auto pens = _spacePens(space, state, lineStyle);
    auto numbers = 0u;
    auto result = Result::Success;
    auto begin = 0u;
    for (auto run = 0u; run < warp.runCount && result == Result::Success; run++) {
        auto end = warp.runs[run].end;
        auto part = warp.runs[run].part;
        auto first = begin;
        begin = end;
        auto shown = depthView
                     || (part != WarpPart::DepthGrid && part != WarpPart::AxisZ && part != WarpPart::NumberZ
                         && part != WarpPart::TickZ);
        auto depth = 0.0f;
        if (!shown || !_warpPoints(warp, first, end, state, config, projected, depth)) continue;
        auto count = end - first;
        switch (part) {
            case WarpPart::Grid:
            case WarpPart::DepthGrid:
                if (count >= 2u) {
                    result = _path(list, projected, count, false,
                                   Pen{state.progress, state.direction, lineStyle, state.layer, depth});
                }
                break;
            case WarpPart::AxisX:
            case WarpPart::AxisY:
            case WarpPart::AxisZ:
                pens.axis.stroke = _axisColor(space, spaceAxis(part), state.opacity);
                if (count >= 2u) {
                    result = _path(list, projected, count, false,
                                   Pen{state.progress, state.direction, pens.axis, pens.axisLayer, depth});
                }
                break;
            case WarpPart::TickX:
            case WarpPart::TickY:
            case WarpPart::TickZ:
                pens.axis.stroke = _axisColor(space, spaceAxis(part), state.opacity);
                if (count >= 2u) {
                    result = _tick(list, projected[0], projected[1], space->tickLength * state.pixelScale, state,
                                   pens.axis, pens.axisLayer, depth);
                }
                break;
            case WarpPart::NumberX:
            case WarpPart::NumberY:
            case WarpPart::NumberZ: {
                if (pens.numberSize <= 0.0f || numbers >= MAX_SPACE_NUMBERS) break;
                auto axis = spaceAxis(part);
                auto scale = space->numberMode == NumberMode::Relative ? _basisLength(state.model, axis) : 1.0f;
                if (scale <= 0.0f) break;
                result = list.number(projected[0] + _numberOffset(space, axis) * state.pixelScale,
                                     warp.runs[run].value * scale, pens.numberSize, {0.5f, 0.5f}, pens.number,
                                     pens.numberLayer, depth);
                numbers++;
                break;
            }
            default: break;
        }
    }
    delete[] projected;
    return result;
}

// Whether a warp bends the object, rather than moving it by its anchor.
static bool _bends(Type type)
{
    switch (type) {
        case Type::Line:
        case Type::Arrow:
        case Type::Vector:
        case Type::Route:
        case Type::Arc:
        case Type::Angle:
        case Type::Circle:
        case Type::Rectangle:
        case Type::Polygon:
        case Type::Plot:
        case Type::Path:
        case Type::Curve:
        case Type::Space: return true;
        default: return false;
    }
}

// Where a warp has carried an object it moves by its anchor, or fallback without one.
static Vec3 _anchored(const VisualState& state, const Vec3& fallback)
{
    return state.warp && state.warp->count ? state.warp->at(0u, state.warpProgress) : fallback;
}

DisplayList::~DisplayList()
{
    for (auto i = 0u; i < count; i++) {
        delete[] commands[i].points;
        delete[] commands[i].ends;
        delete[] commands[i].buffer;
        delete[] commands[i].depths;
    }
    delete[] commands;
    delete[] layers;
}

uint32_t DisplayList::find(const Object* object) const noexcept
{
    for (auto i = 0u; i < layerCount; i++) {
        if (layers[i].object == object) return i + 1u;
    }
    return 0u;
}

bool DisplayList::masking(const DrawCommand& command) const noexcept
{
    for (auto index = command.composite; index; index = layers[index - 1u].parent) {
        if (layers[index - 1u].masking) return true;
    }
    return false;
}

uint32_t DisplayList::add(const DisplayLayer& layer) noexcept
{
    if (!grow(layers, layerCount, layerCapacity, 1u, 8u, DISPLAY_LAYER_LIMIT)) return 0u;
    layers[layerCount] = layer;
    return ++layerCount;
}

DrawCommand* DisplayList::append() noexcept
{
    if (!grow(commands, count, capacity, 1u, 32u, DISPLAY_COMMAND_LIMIT)) return nullptr;
    commands[count] = {};
    commands[count].object = object;
    commands[count].counterpart = counterpart;
    commands[count].composite = composite;
    commands[count].lift = lift;
    commands[count].order = count;
    commands[count].visualKind = visualKind;
    commands[count].visualOpacity = visualOpacity;
    return commands + count++;
}

// A copy of count depths of points for a command, or nullptr when there are none to copy, when
// one is not finite or when memory runs out, which leaves the command whole in split().
static float* _depths(const float* depths, uint32_t count)
{
    if (!depths || !count) return nullptr;
    for (auto i = 0u; i < count; i++) {
        if (!std::isfinite(depths[i])) return nullptr;
    }
    auto copy = new (std::nothrow) float[count];
    if (copy) std::memcpy(copy, depths, sizeof(float) * count);
    return copy;
}

Result DisplayList::path(const Vec2* points, uint32_t count, bool closed, const Style& style,
                         int32_t layer, float depth, PathCap cap, const uint32_t* ends,
                         uint32_t contours, const float* depths) noexcept
{
    if (!points || count < 2 || !std::isfinite(depth)) return Result::InvalidArguments;
    for (auto i = 0u; i < count; i++) {
        if (!_finite(points[i])) return Result::InvalidArguments;
    }
    if (contours < 2u) ends = nullptr;
    if (ends && ends[contours - 1u] != count) return Result::InvalidArguments;
    auto command = append();
    if (!command) return Result::OutOfMemory;
    command->points = new (std::nothrow) Vec2[count];
    command->ends = ends ? new (std::nothrow) uint32_t[contours] : nullptr;
    if (!command->points || (ends && !command->ends)) {
        delete[] command->points;
        delete[] command->ends;
        command->points = nullptr;
        command->ends = nullptr;
        this->count--;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < count; i++)
        command->points[i] = points[i];
    if (!ends) command->depths = _depths(depths, count);
    if (ends) {
        for (auto i = 0u; i < contours; i++)
            command->ends[i] = ends[i];
        command->contours = contours;
    }
    command->type = CommandType::Path;
    command->count = count;
    command->style = style;
    command->layer = layer;
    command->depth = depth;
    command->closed = closed;
    command->cap = cap;
    return Result::Success;
}

Result DisplayList::circle(const Vec2& center, float radius, const Style& style, int32_t layer, float depth,
                           const float* depths) noexcept
{
    if (!_finite(center) || !std::isfinite(radius) || radius <= 0.0f || !std::isfinite(depth)) return Result::InvalidArguments;
    auto command = append();
    if (!command) return Result::OutOfMemory;
    command->type = CommandType::Circle;
    command->center = center;
    command->radius = radius;
    command->style = style;
    command->layer = layer;
    command->depth = depth;
    command->depths = _depths(depths, 1u);
    return Result::Success;
}

static bool _finite(const Mat3& value) noexcept
{
    for (auto item : value.e) {
        if (!std::isfinite(item)) return false;
    }
    return true;
}

Result DisplayList::text(const Vec2& point, const char* value, const char* font, float size,
                         const Vec2& align, const Style& style, int32_t layer, float depth,
                         const Mat3* transform, bool copy, float slant, bool tabular) noexcept
{
    if (!_finite(point) || !value || !std::isfinite(size) || size <= 0.0f || !_finite(align)
        || !std::isfinite(depth) || (transform && !_finite(*transform)) || !std::isfinite(slant)) {
        return Result::InvalidArguments;
    }
    auto command = append();
    if (!command) return Result::OutOfMemory;
    if (copy) {
        auto length = std::strlen(value) + 1u;
        command->buffer = new (std::nothrow) char[length];
        if (!command->buffer) {
            this->count--;
            return Result::OutOfMemory;
        }
        std::memcpy(command->buffer, value, length);
        value = command->buffer;
    }
    command->type = CommandType::Text;
    command->center = point;
    command->text = value;
    command->font = font;
    command->size = size;
    command->slant = slant;
    command->tabular = tabular;
    command->align = align;
    command->style = style;
    command->layer = layer;
    command->depth = depth;
    if (transform) {
        command->transform = *transform;
        command->transformed = true;
    }
    return Result::Success;
}

Result DisplayList::number(const Vec2& point, float value, float size, const Vec2& align,
                           const Style& style, int32_t layer, float depth, const float* depths) noexcept
{
    if (!_finite(point) || !std::isfinite(value) || !std::isfinite(size) || size <= 0.0f
        || !_finite(align) || !std::isfinite(depth)) {
        return Result::InvalidArguments;
    }
    auto command = append();
    if (!command) return Result::OutOfMemory;
    command->type = CommandType::Number;
    command->center = point;
    command->number = value;
    command->size = size;
    command->align = align;
    command->style = style;
    command->layer = layer;
    command->depth = depth;
    command->depths = _depths(depths, 1u);
    return Result::Success;
}

Result DisplayList::picture(const Vec2* corners, const PictureSource& source, ImageFilter filter,
                            uint8_t opacity, int32_t layer, float depth) noexcept
{
    auto encoded = source.encoded && source.encodedSize && source.mime;
    if (!corners || (!encoded && !source.pixels) || !source.width || !source.height || !std::isfinite(depth) || (filter != ImageFilter::Bilinear && filter != ImageFilter::Nearest)) {
        return Result::InvalidArguments;
    }
    for (auto i = 0u; i < 3u; i++) {
        if (!_finite(corners[i])) return Result::InvalidArguments;
    }
    auto command = append();
    if (!command) return Result::OutOfMemory;
    command->points = new (std::nothrow) Vec2[3];
    if (!command->points) {
        count--;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < 3u; i++)
        command->points[i] = corners[i];
    command->type = CommandType::Picture;
    command->count = 3;
    command->picture = source;
    command->filter = filter;
    command->opacity = opacity;
    command->layer = layer;
    command->depth = depth;
    return Result::Success;
}

static Result _cellFace(DisplayList& list, const Vec3* local, const Mat4& model,
                        const Config& config, Color color, float opacity, int32_t layer)
{
    Vec3 world[4];
    auto center = Vec3{};
    for (auto i = 0u; i < 4u; i++) {
        world[i] = model * local[i];
        center = center + world[i] * 0.25f;
    }
    auto normal = (world[1] - world[0]).cross(world[2] - world[0]).normalized();
    auto view = config.camera.projection == Projection::Perspective ? config.camera.eye - center
                                                                    : config.camera.eye - config.camera.target;
    auto facing = normal.dot(view.normalized());
    if (!std::isfinite(facing) || facing <= 0.0f) return Result::Success;
    Vec2 points[4];
    auto depth = 0.0;
    auto aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
    for (auto i = 0u; i < 4u; i++) {
        Vec3 ndc;
        if (!config.camera.project(world[i], aspect, ndc)) return Result::Success;
        points[i] = _pixel(ndc, config);
        depth += ndc.z;
    }
    Style style;
    style.stroke = _shade(color, 0.32f, opacity * 0.65f);
    style.fill = _shade(color, 0.52f + 0.48f * facing, opacity);
    style.width = 0.75f;
    return list.path(points, 4, true, style, layer, static_cast<float>(depth / 4.0));
}

// The color a face of a surface takes instead of the fill, from the corners at indices of points
// in its local space: its colormap at the mean share of the corners, which is the share of the
// middle of the face, or the mean of its corners' own colors. False leaves the face to the fill.
static bool _faceColor(const SurfaceMesh* surface, const Vec3* points, const uint32_t* corners,
                       Color& color)
{
    if (surface->colormap.map != Colormap::None) {
        auto share = 0.0f;
        for (auto i = 0u; i < 4u; i++) share += colormapShare(surface->colormap, points[corners[i]]);
        color = colormapColor(surface->colormap.map, share * 0.25f);
        return true;
    }
    auto colors = surface->colors();
    if (!colors) return false;
    uint32_t sums[4] = {};
    for (auto i = 0u; i < 4u; i++) {
        auto& corner = colors[corners[i]];
        sums[0] += corner.r;
        sums[1] += corner.g;
        sums[2] += corner.b;
        sums[3] += corner.a;
    }
    color = {static_cast<uint8_t>((sums[0] + 2u) / 4u), static_cast<uint8_t>((sums[1] + 2u) / 4u),
             static_cast<uint8_t>((sums[2] + 2u) / 4u), static_cast<uint8_t>((sums[3] + 2u) / 4u)};
    return true;
}

// face: the color the face takes instead of the authored fill and gradient, or nullptr. light:
// the unit direction toward a light fixed in the world that shades the face in place of the
// headlight, with ambient the share of the color it keeps where the light does not reach, or
// nullptr.
static Result _surfaceFace(DisplayList& list, const Vec3* local, const Mat4& model,
                           const Config& config, const Style& authored,
                           SurfaceMode mode, bool shading, int32_t layer,
                           const Color* face = nullptr, const Vec3* light = nullptr,
                           float ambient = 0.0f)
{
    Vec3 world[4];
    auto center = Vec3{};
    for (auto i = 0u; i < 4u; i++) {
        world[i] = model * local[i];
        center = center + world[i] * 0.25f;
    }
    auto normal = (world[1] - world[0]).cross(world[2] - world[0]).normalized();
    auto view = config.camera.projection == Projection::Perspective ? config.camera.eye - center
                                                                    : config.camera.eye - config.camera.target;
    auto toward = normal.dot(view.normalized());
    auto facing = std::fabs(toward);
    if (!std::isfinite(facing)) return Result::Success;
    auto shade = 0.58f + 0.42f * facing;
    if (light) {
        // The side the camera sees takes the light.
        auto lit = (toward < 0.0f ? normal * -1.0f : normal).dot(*light);
        shade = ambient + (1.0f - ambient) * std::fmax(lit, 0.0f);
    }

    Vec2 points[4];
    float depths[4];
    auto depth = 0.0;
    auto aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
    for (auto i = 0u; i < 4u; i++) {
        Vec3 ndc;
        if (!config.camera.project(world[i], aspect, ndc)) return Result::Success;
        points[i] = _pixel(ndc, config);
        depths[i] = _screenDepth(ndc.z, config);
        depth += ndc.z;
    }

    auto style = authored;
    if (face) style.gradient.type = GradientType::None;
    if (mode == SurfaceMode::Mesh) {
        style.fill.a = 0;
    } else {
        auto fill = face ? *face : authored.fill;
        style.fill = shading ? _shade(fill, shade, 1.0f) : fill;
        if (shading && style.gradient.enabled()) {
            style.gradientEnd = _shade(authored.gradientEnd, shade, 1.0f);
            for (auto i = 0u; i < style.gradient.stopCount; i++) {
                style.gradient.stops[i].color =
                    _shade(authored.gradient.stops[i].color, shade, 1.0f);
            }
        }
        if (mode == SurfaceMode::Solid) {
            style.stroke = style.fill;
            style.width = 0.9f;
            style.dashCount = 0;
        }
    }
    return list.path(points, 4, true, style, layer, static_cast<float>(depth / 4.0), PathCap::Round,
                     nullptr, 0u, _spatial(config) ? depths : nullptr);
}

// The unit direction toward the light of a surface, or false when it has none: a zero direction
// or one too long to measure.
static bool _lightDirection(const SurfaceLight& light, Vec3& direction)
{
    auto x = static_cast<double>(light.direction.x);
    auto y = static_cast<double>(light.direction.y);
    auto z = static_cast<double>(light.direction.z);
    auto length = std::sqrt(x * x + y * y + z * z);
    if (!(length > 0.0) || !std::isfinite(length)) return false;
    direction = {static_cast<float>(x / length), static_cast<float>(y / length),
                 static_cast<float>(z / length)};
    return true;
}

// The depth a face of a surface sorts at, the mean of its corners', as _surfaceFace gives it;
// false when a corner cannot be projected.
static bool _faceDepth(const Vec3* local, const Mat4& model, const Config& config, float& depth)
{
    auto aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
    auto sum = 0.0;
    for (auto i = 0u; i < 4u; i++) {
        Vec3 ndc;
        if (!config.camera.project(model * local[i], aspect, ndc)) return false;
        sum += ndc.z;
    }
    depth = static_cast<float>(sum / 4.0);
    return std::isfinite(depth);
}

// Interpolates independent mesh lines across the point grid, splitting at face boundaries
// for depth ordering. NaN depths hide faces; dashes remain continuous. Translucent strokes
// use butt caps at splits to avoid overlapping ends.
static Result _surfaceLines(DisplayList& list, const SurfaceMesh* surface, const Vec3* points,
                            const VisualState& state, const Config& config, Style style,
                            const float* depths)
{
    style.fill.a = 0;
    style.gradient.type = GradientType::None;
    auto cap = style.stroke.a == 255u ? PathCap::Round : PathCap::Butt;
    auto offset = style.dashOffset;
    auto columns = surface->columns();
    auto rows = surface->rows();
    auto cellColumns = columns - 1u;
    for (auto direction = 0u; direction < 2u; direction++) {
        // Direction 0 holds the lines across the columns, which run up the rows, and direction 1
        // the lines across the rows, which run along the columns.
        auto lines = direction ? surface->meshRows : surface->meshColumns;
        auto across = direction ? rows : columns;
        auto along = direction ? columns : rows;
        for (auto line = 0u; line < lines; line++) {
            auto position = static_cast<double>(line) * (across - 1u) / (lines - 1u);
            auto cell = std::min(static_cast<uint32_t>(position), across - 2u);
            auto share = static_cast<float>(position - cell);
            // A line on a sample between two faces lies on both.
            auto shared = share == 0.0f && cell > 0u;
            auto sample = [&](uint32_t step) {
                auto first = direction ? points[cell * columns + step] : points[step * columns + cell];
                auto second = direction ? points[(cell + 1u) * columns + step]
                                        : points[step * columns + cell + 1u];
                return first + (second - first) * share;
            };
            auto travelled = 0.0f;
            for (auto step = 0u; step + 1u < along; step++) {
                auto face = direction ? cell * cellColumns + step : step * cellColumns + cell;
                auto neighbor = direction ? face - cellColumns : face - 1u;
                auto depth = depths[face];
                if (shared && std::isfinite(depths[neighbor])) {
                    depth = std::isfinite(depth) ? std::fmin(depth, depths[neighbor]) : depths[neighbor];
                }
                Vec2 pixels[2];
                float own;
                if (!std::isfinite(depth)
                    || !_lineWorld(sample(step), sample(step + 1u), state.model, config, pixels[0],
                                   pixels[1], own)) {
                    continue;
                }
                style.dashOffset = offset + travelled;
                auto result = list.path(pixels, 2u, false, style, state.layer, depth, cap);
                if (result != Result::Success) return result;
                travelled += (pixels[1] - pixels[0]).length();
            }
        }
    }
    return Result::Success;
}

// A surface that morphs into another on its grid: the target, the paired points and the colors
// sampled between them, and the eased progress.
struct SurfaceMorph
{
    const SurfaceMesh* target;
    const MorphShape* shape;
    float progress;
};

// The color a side of a morph gives a face at the points between the two surfaces, before the
// two sides mix: its sampled colors, each corner blending the two steps around the progress;
// its colormap or its own colors there, as _faceColor gives them; or the fill, which then comes
// from the channels of the morph.
static void _morphSide(const SurfaceMesh* side, const Color* steps, const SurfaceMorph& morph,
                       const Vec3* points, const uint32_t* corners, const Color& fill, float* color)
{
    Color face = fill;
    if (steps) {
        auto count = morph.shape->count;
        auto position = morph.progress * static_cast<float>(morph.shape->steps);
        auto step = std::min(static_cast<uint32_t>(std::fmax(position, 0.0f)), morph.shape->steps - 1u);
        auto share = position - static_cast<float>(step);
        auto before = steps + static_cast<size_t>(step) * count;
        auto after = before + count;
        for (auto c = 0u; c < 4u; c++) color[c] = 0.0f;
        for (auto i = 0u; i < 4u; i++) {
            auto& a = before[corners[i]];
            auto& b = after[corners[i]];
            color[0] += (a.r * (1.0f - share) + b.r * share) * 0.25f;
            color[1] += (a.g * (1.0f - share) + b.g * share) * 0.25f;
            color[2] += (a.b * (1.0f - share) + b.b * share) * 0.25f;
            color[3] += (a.a * (1.0f - share) + b.a * share) * 0.25f;
        }
        return;
    }
    if (side->colormap.map != Colormap::None || side->colors()) _faceColor(side, points, corners, face);
    color[0] = face.r;
    color[1] = face.g;
    color[2] = face.b;
    color[3] = face.a;
}

// The color a face takes while a surface morphs: the source's colors at the points between the
// two mixed into the target's by the progress, or false when neither side has colors, which
// leaves the face to the fill of the morph.
static bool _morphFaceColor(const SurfaceMesh* source, const SurfaceMorph& morph, const Vec3* points,
                            const uint32_t* corners, const Color& fill, Color& color)
{
    auto target = morph.target;
    auto shape = morph.shape;
    auto colored = [](const SurfaceMesh* side, const Color* steps) {
        return steps || side->colormap.map != Colormap::None || side->colors();
    };
    if (!colored(source, shape->colors[0]) && !colored(target, shape->colors[1])) return false;
    float from[4];
    float to[4];
    _morphSide(source, shape->colors[0], morph, points, corners, fill, from);
    _morphSide(target, shape->colors[1], morph, points, corners, fill, to);
    auto t = morph.progress;
    uint8_t mixed[4];
    for (auto c = 0u; c < 4u; c++) {
        auto value = from[c] * (1.0f - t) + to[c] * t;
        mixed[c] = static_cast<uint8_t>(std::fmin(std::fmax(value, 0.0f), 255.0f) + 0.5f);
    }
    color = {mixed[0], mixed[1], mixed[2], mixed[3]};
    return true;
}

// The faces of a surface at points, its own or those of a morph, and its mesh lines. morph holds
// what the faces of a morph take their colors from, or nullptr.
static Result _surfaceFaces(DisplayList& list, const SurfaceMesh* surface, const Vec3* points,
                            const SurfaceMorph* morph, const VisualState& state, const Config& config,
                            const Style& style, float progress)
{
    auto columns = surface->columns();
    auto rows = surface->rows();
    auto cellColumns = columns - 1u;
    auto total = cellColumns * (rows - 1u);
    auto visible = static_cast<uint32_t>(std::ceil(static_cast<float>(total) * progress));
    if (visible > total) visible = total;
    auto reverse = _reverse(state.direction);
    // Mesh lines of their own leave the faces to seal their seams, or to a Mesh surface nothing
    // but the depths the pieces of the lines sort at.
    auto lined = surface->mode != SurfaceMode::Solid && (surface->meshColumns || surface->meshRows);
    Vec3 light;
    auto lit = surface->shading && _lightDirection(surface->light, light);
    float* depths = nullptr;
    if (lined) {
        depths = new (std::nothrow) float[total];
        if (!depths) return Result::OutOfMemory;
        for (auto i = 0u; i < total; i++) depths[i] = std::numeric_limits<float>::quiet_NaN();
    }
    auto result = Result::Success;
    for (auto shown = 0u; shown < visible; shown++) {
        auto index = reverse ? total - 1u - shown : shown;
        auto x = index % cellColumns;
        auto y = index / cellColumns;
        auto row = y * columns;
        auto next = row + columns;
        uint32_t corners[4] = {row + x, row + x + 1u, next + x + 1u, next + x};
        Vec3 face[4] = {points[corners[0]], points[corners[1]], points[corners[2]], points[corners[3]]};
        if (lined) {
            float depth;
            if (_faceDepth(face, state.model, config, depth)) depths[index] = depth;
            if (surface->mode == SurfaceMode::Mesh) continue;
        }
        Color color;
        auto colored = morph ? _morphFaceColor(surface, *morph, points, corners, state.fill, color)
                             : _faceColor(surface, points, corners, color);
        if (colored) color = _opacity(color, state.opacity * state.fillProgress);
        result = _surfaceFace(list, face, state.model, config, style,
                              lined ? SurfaceMode::Solid : surface->mode, surface->shading, state.layer,
                              colored ? &color : nullptr, lit ? &light : nullptr, surface->light.ambient);
        if (result != Result::Success) break;
    }
    if (lined && result == Result::Success) {
        result = _surfaceLines(list, surface, points, state, config, style, depths);
    }
    delete[] depths;
    return result;
}

// A surface that morphs into another on its grid: the faces at the points between the two, in
// the mode, shading, light and mesh lines of the source, colored by both.
static Result _morphSurface(DisplayList& list, const SurfaceMesh* surface, const VisualState& state,
                            const Config& config, const Style& style, float progress)
{
    auto shape = state.morphShape;
    if (!shape || !state.morph || state.morph->type() != Type::SurfaceMesh
        || shape->count != surface->columns() * surface->rows()) {
        return Result::NonSupport;
    }
    auto points = new (std::nothrow) Vec3[shape->count];
    if (!points) return Result::OutOfMemory;
    for (auto i = 0u; i < shape->count; i++) {
        points[i] = shape->from[i] + (shape->to[i] - shape->from[i]) * state.morphProgress;
    }
    SurfaceMorph morph = {static_cast<const SurfaceMesh*>(state.morph), shape, state.morphProgress};
    auto result = _surfaceFaces(list, surface, points, &morph, state, config, style, progress);
    delete[] points;
    return result;
}

static Result _voxel(DisplayList& list, const Vec3& origin, float padding, float depth,
                     const Mat4& model, const Config& config, Color color, float opacity,
                     int32_t layer)
{
    auto x0 = origin.x + padding;
    auto x1 = origin.x + 1.0f - padding;
    auto y0 = origin.y + padding;
    auto y1 = origin.y + 1.0f - padding;
    auto z0 = origin.z + padding * depth;
    auto z1 = origin.z + depth * (1.0f - padding);
    Vec3 corners[8] = {
        {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    static constexpr uint8_t faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}};
    for (auto& face : faces) {
        Vec3 points[4] = {corners[face[0]], corners[face[1]], corners[face[2]], corners[face[3]]};
        auto result = _cellFace(list, points, model, config, color, opacity, layer);
        if (result != Result::Success) return result;
    }
    return Result::Success;
}

static bool _before(int32_t firstLayer, float firstDepth, uint32_t firstOrder,
                    int32_t secondLayer, float secondDepth, uint32_t secondOrder)
{
    if (firstLayer != secondLayer) return firstLayer < secondLayer;
    if (firstDepth != secondDepth) return firstDepth > secondDepth;
    return firstOrder < secondOrder;
}

// Compares two commands at the deepest layer that holds both: each is represented there by
// itself, when it draws straight into that layer, or by the nested layer that contains it.
static bool _before(const DisplayLayer* layers, const DrawCommand& a, const DrawCommand& b)
{
    auto first = a.composite;
    auto second = b.composite;
    if (first == second) return _before(a.layer, a.depth, a.order, b.layer, b.depth, b.order);
    auto firstChild = 0u;
    auto secondChild = 0u;
    auto firstLevel = first ? layers[first - 1u].level : 0u;
    auto secondLevel = second ? layers[second - 1u].level : 0u;
    while (firstLevel > secondLevel) {
        firstChild = first;
        first = layers[first - 1u].parent;
        firstLevel--;
    }
    while (secondLevel > firstLevel) {
        secondChild = second;
        second = layers[second - 1u].parent;
        secondLevel--;
    }
    while (first != second) {
        firstChild = first;
        first = layers[first - 1u].parent;
        secondChild = second;
        second = layers[second - 1u].parent;
    }
    auto firstLayer = a.layer;
    auto firstDepth = a.depth;
    auto firstOrder = a.order;
    if (firstChild) {
        auto& layer = layers[firstChild - 1u];
        firstLayer = layer.layer;
        firstDepth = layer.depth;
        firstOrder = layer.order;
    }
    auto secondLayer = b.layer;
    auto secondDepth = b.depth;
    auto secondOrder = b.order;
    if (secondChild) {
        auto& layer = layers[secondChild - 1u];
        secondLayer = layer.layer;
        secondDepth = layer.depth;
        secondOrder = layer.order;
    }
    return _before(firstLayer, firstDepth, firstOrder, secondLayer, secondDepth, secondOrder);
}

// _before() for two indices into the commands of the list data.
static bool _commandBefore(const void* first, const void* second, const void* data)
{
    auto list = static_cast<const DisplayList*>(data);
    return _before(list->layers, list->commands[*static_cast<const uint32_t*>(first)],
                   list->commands[*static_cast<const uint32_t*>(second)]);
}

Result DisplayList::sort() noexcept
{
    // Each layer's place: its object's layer, then the mean depth and the first order of
    // every drawing inside it, nested layers included.
    if (layerCount) {
        auto sums = new (std::nothrow) double[layerCount];
        if (!sums) return Result::OutOfMemory;
        for (auto i = 0u; i < layerCount; i++) {
            layers[i].order = UINT32_MAX;
            layers[i].count = 0u;
            layers[i].depth = 0.0f;
            sums[i] = 0.0;
        }
        for (auto i = 0u; i < count; i++) {
            auto& command = commands[i];
            // The pieces split() made of one drawing count as that drawing, once.
            if (command.piece > 1u) continue;
            auto depth = command.piece ? command.whole : command.depth;
            for (auto index = command.composite; index; index = layers[index - 1u].parent) {
                auto& layer = layers[index - 1u];
                if (command.order < layer.order) layer.order = command.order;
                layer.count++;
                sums[index - 1u] += static_cast<double>(depth);
            }
        }
        for (auto i = 0u; i < layerCount; i++) {
            if (layers[i].count) layers[i].depth = static_cast<float>(sums[i] / layers[i].count);
        }
        delete[] sums;
    }
    if (count < 2) return Result::Success;
    // Sorts the indices of the commands, which are large, and then moves each command once,
    // along the cycles of the order.
    auto order = new (std::nothrow) uint32_t[2u * count];
    if (!order) return Result::OutOfMemory;
    for (auto i = 0u; i < count; i++) order[i] = i;
    stableSort(order, order + count, count, sizeof(uint32_t), _commandBefore, this);
    for (auto i = 0u; i < count; i++) {
        if (order[i] == i) continue;
        auto held = commands[i];
        auto at = i;
        for (auto next = order[at]; next != i; next = order[at]) {
            commands[at] = commands[next];
            order[at] = at;
            at = next;
        }
        commands[at] = held;
        order[at] = at;
    }
    delete[] order;
    return Result::Success;
}

static Result _labelledPieces(DisplayList& list, const Vec3* local, const Vec2* projected,
                              const uint32_t* ends, uint32_t pieces, const ContourPlacement* labels,
                              uint32_t labelCount, const Pen& pen)
{
    auto depths = pen.depths;
    auto vertices = ends[pieces - 1u];
    auto room = vertices + 4u * labelCount + 2u * pieces;
    auto output = std::unique_ptr<Vec2[]>(new (std::nothrow) Vec2[room]);
    auto outputDepths = depths ? std::unique_ptr<float[]>(new (std::nothrow) float[room]) : nullptr;
    auto outputEnds = std::unique_ptr<uint32_t[]>(new (std::nothrow) uint32_t[pieces + 2u * labelCount]);
    if (!output || !outputEnds || (depths && !outputDepths)) return Result::OutOfMemory;
    auto used = 0u, runs = 0u, start = 0u;
    auto finish = [&]() {
        if (used - start > 1u) outputEnds[runs++] = used;
        else used = start;
        start = used;
    };
    auto remaining = _clamp(pen.progress) * (vertices - pieces);
    auto reverse = _reverse(pen.direction);
    for (auto step = 0u; step < pieces && remaining > 1.0e-6f; step++) {
        auto piece = reverse ? pieces - 1u - step : step;
        auto first = piece ? ends[piece - 1u] : 0u;
        auto last = ends[piece];
        auto length = 0.0f;
        for (auto j = first + 1u; j < last; j++) length += std::hypot(local[j].x - local[j - 1u].x, local[j].y - local[j - 1u].y);
        auto closed = local[first].x == local[last - 1u].x && local[first].y == local[last - 1u].y;
        auto distance = 0.0f;
        for (auto segment = 0u; segment + 1u < last - first && remaining > 1.0e-6f; segment++, remaining -= 1.0f) {
            auto a = reverse ? last - 1u - segment : first + segment;
            auto b = reverse ? a - 1u : a + 1u;
            auto span = std::hypot(local[b].x - local[a].x, local[b].y - local[a].y);
            if (span <= 0.0f) continue;
            auto limit = distance + span * std::min(remaining, 1.0f);
            auto cursor = distance;
            while (cursor < limit) {
                auto stop = limit, resume = limit;
                for (auto i = 0u; i < labelCount; i++) {
                    if (labels[i].piece != piece) continue;
                    auto center = reverse ? length - labels[i].distance : labels[i].distance;
                    auto half = std::hypot(labels[i].half.x, labels[i].half.y);
                    for (auto wrap = closed ? -1 : 0; wrap <= (closed ? 1 : 0); wrap++) {
                        auto lo = center + wrap * length - half, hi = center + wrap * length + half;
                        if (hi <= cursor || lo >= stop) continue;
                        stop = std::max(cursor, lo);
                        resume = hi;
                    }
                }
                if (stop > cursor) {
                    auto put = [&](float position) {
                        auto t = (position - distance) / span;
                        auto point = position == distance ? projected[a] : position == distance + span ? projected[b]
                            : projected[a] + (projected[b] - projected[a]) * t;
                        if (used > start && output[used - 1u].x == point.x && output[used - 1u].y == point.y) return;
                        output[used] = point;
                        if (depths) outputDepths[used] = depths[a] + (depths[b] - depths[a]) * t;
                        used++;
                    };
                    put(cursor);
                    put(stop);
                }
                if (stop < limit) finish();
                cursor = std::max(stop, resume);
            }
            distance += span;
        }
        finish();
    }
    return runs ? _strokes(list, output.get(), outputDepths.get(), outputEnds.get(), runs,
                           pen.style, pen.layer, pen.depth, PathCap::Butt) : Result::Success;
}

// Whether object, whose entry is entry, is valid and as the scene authored it. With known, where a
// build keeps whether it found so, it checks each object once however many objects below ask.
static bool _authored(const Object* object, const ObjectEntry& entry, bool* known)
{
    if (known && *known) return true;
    if (!valid(object) || entry.fingerprint != fingerprint(object)) return false;
    if (known) *known = true;
    return true;
}

// Whether object, whose entry is entry, can be placed in state, as it samples there: the object is
// valid and as the scene authored it, the model affine, and the styles, opacity, progress and
// scale finite, with no width, radius or scale below zero.
static bool _placeable(const Object* object, const ObjectEntry& entry, const VisualState& state, bool* known)
{
    return _authored(object, entry, known) && _affine(state.model)
           && std::isfinite(state.width) && state.width >= 0.0f
           && std::isfinite(state.radius) && state.radius >= 0.0f
           && std::isfinite(state.dashOffset)
           && std::isfinite(state.opacity) && std::isfinite(state.progress)
           && std::isfinite(state.fillProgress)
           && std::isfinite(state.pixelScale) && state.pixelScale >= 0.0f;
}

// Runs the count runtime modifiers that move objects over the model, opacity and progress object
// has in state at time; the model must stay affine, the opacity and progress within [0, 1].
static Result _modify(const RuntimeModifier* modifiers, uint32_t count, const Object* object, float time,
                      VisualState& state)
{
    for (auto i = 0u; i < count; i++) {
        auto& modifier = modifiers[i];
        if (!modifier.object) continue;
        auto result = modifier.object(object, time, state.model, state.opacity, state.progress, modifier.data);
        if (result != Result::Success) return result;
        if (!_affine(state.model) || !std::isfinite(state.opacity)
            || state.opacity < 0.0f || state.opacity > 1.0f
            || !std::isfinite(state.progress) || state.progress < 0.0f
            || state.progress > 1.0f) {
            return Result::InvalidArguments;
        }
    }
    return Result::Success;
}

Result Scene::Impl::world(const Object* object, float time, VisualState& state, bool& visible,
                          WorldOpacity opacity, const Object* shared, bool* known) const noexcept
{
    visible = false;
    auto item = entry(object);
    if (!item) return Result::InvalidArguments;
    if (!sample(*item, time, state)) return Result::Success;
    state.inheritedProgressEmpty = false;
    auto knows = [this, known](const ObjectEntry* found) { return known ? known + (found - objects) : nullptr; };
    if (!_placeable(object, *item, state, knows(item))) return Result::InvalidArguments;
    if (state.morph) {
        auto morphEntry = entry(state.morph);
        if (!morphEntry || !_authored(state.morph, *morphEntry, knows(morphEntry))) return Result::InvalidArguments;
    }
    auto modified = _modify(runtimeModifiers, runtimeModifierCnt, object, time, state);
    if (modified != Result::Success) return modified;
    for (auto i = 0u; i < runtimeModifierCnt; i++) {
        auto& modifier = runtimeModifiers[i];
        if (!modifier.fill) continue;
        auto result = modifier.fill(object, time, state.fill, modifier.data);
        if (result != Result::Success) return result;
    }
    // Group opacities stop at the nearest layer, which applies them itself (WorldOpacity).
    auto multiply = opacity == WorldOpacity::All || !composited(object);
    // An object moves by as many layers as each object above it moved its own since it began.
    auto layer = static_cast<int64_t>(state.layer);
    if (opacity == WorldOpacity::Layer || opacity == WorldOpacity::Mask) {
        multiply = true;
        if (object->type() != Type::Group) state.opacity = 1.0f;
    }
    // Whether a label that space placed lies on the way from the object up to it: the space
    // passes such a label what a group passes its children (see Space::place()).
    auto labelled = [object](const Object* space) {
        for (auto item = object; item && item != space; item = item->parent()) {
            if (item->objLabelOf == space) return true;
        }
        return false;
    };
    for (auto parentObject = object->parent(); parentObject; parentObject = parentObject->parent()) {
        auto parent = entry(parentObject);
        VisualState parentState;
        if (!parent || !sample(*parent, time, parentState)) return Result::Success;
        if (!_placeable(parentObject, *parent, parentState, knows(parent))) return Result::InvalidArguments;
        modified = _modify(runtimeModifiers, runtimeModifierCnt, parentObject, time, parentState);
        if (modified != Result::Success) return modified;
        if (parentObject->type() == Type::Angle) {
            auto point = angleLabelPoint(static_cast<const Angle*>(parentObject), &parentState);
            state.model = Mat4::translate(point) * state.model;
        }
        if (parentObject->type() == Type::Plot && object->type() == Type::Text) {
            auto plot = static_cast<const Plot*>(parentObject);
            if (plot->contourLabels()) {
                auto placements = std::unique_ptr<ContourPlacement[]>(new (std::nothrow) ContourPlacement[plot->childCount()]);
                if (!placements) return Result::OutOfMemory;
                uint32_t count;
                auto result = contourPlacements(plot, time, placements.get(), plot->childCount(), count);
                if (result != Result::Success) return result;
                auto found = false;
                for (auto i = 0u; i < count; i++) {
                    auto& placement = placements[i];
                    if (placement.text != object) continue;
                    auto grid = parentState.geometry ? plotAt(parentState).contour : plot->contourGrid();
                    state.value = static_cast<float>(grid.level);
                    const char* value;
                    char* written;
                    result = shown(placement.text, state, value, written);
                    if (result != Result::Success) return result;
                    TextExtent extent;
                    result = placement.text->measure(value, extent);
                    delete[] written;
                    if (result != Result::Success) return result;
                    Vec3 offset{extent.width * 0.5f - (extent.left + extent.right) * 0.5f,
                                extent.ascent - extent.height * 0.5f - (extent.top + extent.bottom) * 0.5f, 0.0f};
                    state.model = Mat4::translate(placement.point) * Mat4::rotateZ(placement.angle)
                                  * Mat4::translate(offset) * state.model;
                    state.fill = parentState.stroke;
                    found = true;
                    break;
                }
                if (!found) return Result::Success;
            }
        }
        state.model = parentState.model * state.model;
        layer += static_cast<int64_t>(parentState.layer) - parent->base.layer;
        auto reached = multiply;
        if (opacity == WorldOpacity::Mask ? parentObject == shared
                                          : opacity != WorldOpacity::All && composited(parentObject)) {
            multiply = false;
        }
        auto group = parentObject->type() == Type::Group;
        auto labels = parentObject->type() == Type::Angle
                      || (parentObject->type() == Type::Plot && static_cast<const Plot*>(parentObject)->contourLabels())
                      || (parentObject->type() == Type::Space && labelled(parentObject));
        if (group || labels) {
            // The layer of a space leaves the space's opacity to what it draws, so its labels
            // take it themselves wherever the layer of a group would apply the group's.
            if ((group || opacity == WorldOpacity::Mask) ? multiply : reached) state.opacity *= parentState.opacity;
            state.inheritedProgressEmpty = state.inheritedProgressEmpty || parentState.progress <= 0.0f;
            state.progress *= parentState.progress;
            state.fillProgress *= parentState.fillProgress;
            state.pixelScale *= parentState.pixelScale;
            state.traceFill = state.traceFill || parentState.traceFill;
            if (parentState.progress < 1.0f) state.direction = parentState.direction;
        }
    }
    if (!_affine(state.model) || !std::isfinite(state.opacity) || !std::isfinite(state.progress)
        || !std::isfinite(state.fillProgress)
        || !std::isfinite(state.pixelScale) || state.pixelScale < 0.0f) {
        return Result::InvalidArguments;
    }
    state.layer = static_cast<int32_t>(std::clamp<int64_t>(layer, INT32_MIN, INT32_MAX));
    visible = true;
    return Result::Success;
}

bool Scene::Impl::plotEndpoint(const Object* object, float time, const VisualState& state) const noexcept
{
    if (object->type() != Type::Plot || static_cast<const Plot*>(object)->endpointRadius <= 0.0f
        || state.opacity <= 0.0f || state.morph || state.inheritedProgressEmpty) return false;
    if (state.progress > 0.0f) return true;
    // Explicit progress clips keep the vertex; only actual scheduled reveals hide it at zero.
    auto stored = entry(object);
    if (!stored) return false;
    const Clip* last = nullptr;
    for (auto i = stored->firstClip; i != NO_CLIP; i = clips[i].next) {
        auto& clip = clips[i];
        if (!(clip.channels & CHANNEL_PROGRESS)) continue;
        if (time >= clip.begin) last = &clip;
        else if (!last && clip.progressReveal && clip.kind != AnimationKind::Uncreate) return false;
    }
    return !last || !last->progressReveal;
}

int32_t Scene::Impl::drawLayer(const Object* object, float time) const noexcept
{
    VisualState state;
    auto item = entry(object);
    if (!item || !sample(*item, time, state)) return object->layer;
    auto layer = static_cast<int64_t>(state.layer);
    for (auto above = object->parent(); above; above = above->parent()) {
        auto parent = entry(above);
        if (parent && sample(*parent, time, state)) layer += static_cast<int64_t>(state.layer) - parent->base.layer;
    }
    return static_cast<int32_t>(std::clamp<int64_t>(layer, INT32_MIN, INT32_MAX));
}

static Result _morph(DisplayList& list, const Object* object, const VisualState& state, const Config& config,
                     const Style& style)
{
    auto shape = state.morphShape;
    if (!shape || !shape->count) return Result::NonSupport;
    auto from = shape->from;
    auto to = shape->to;
    auto count = shape->count;
    auto closed = shape->closed;
    auto projected = new (std::nothrow) Vec2[count];
    auto depths = _spatial(config) ? new (std::nothrow) float[count] : nullptr;
    if (!projected) {
        delete[] depths;
        return Result::OutOfMemory;
    }
    auto at = [&](uint32_t i) { return from[i] + (to[i] - from[i]) * state.morphProgress; };
    auto pen = Pen{state.progress, state.direction, style, state.layer, 0.0f, depths, state.traceFill, _cap(object)};
    auto result = Result::Success;
    if (_project(count, at, state.model, config, projected, depths, pen.depth)) {
        if (shape->ends) {
            auto closedCount = shape->closedPieces;
            if (style.fill.a) {
                auto fill = pen;
                fill.style.stroke.a = 0;
                result = _contours(list, projected, shape->ends, shape->pieces, closedCount == shape->pieces, fill);
            }
            auto begin = 0u;
            for (auto i = 0u; i < shape->pieces && result == Result::Success; i++) {
                auto end = shape->ends[i];
                auto stroke = pen;
                stroke.style.fill.a = 0;
                auto growth = shape->growth[i];
                auto scale = growth > 0 ? state.morphProgress : growth < 0 ? 1.0f - state.morphProgress : 1.0f;
                stroke.style.width *= scale;
                if (scale <= 0.0f) stroke.style.stroke.a = 0;
                stroke.depths = depths ? depths + begin : nullptr;
                if (stroke.style.stroke.a && stroke.style.width > 0.0f) {
                    result = _path(list, projected + begin, end - begin, i < closedCount, stroke);
                }
                begin = end;
            }
        } else {
            result = _path(list, projected, count, closed, pen);
        }
    }
    delete[] projected;
    delete[] depths;
    return result;
}

static Vec3 _edge(const Bounds& bounds, const Vec3& direction)
{
    auto half = bounds.size() * 0.5f;
    auto distance = std::numeric_limits<float>::infinity();
    if (std::fabs(direction.x) > 1.0e-7f) distance = std::fmin(distance, half.x / std::fabs(direction.x));
    if (std::fabs(direction.y) > 1.0e-7f) distance = std::fmin(distance, half.y / std::fabs(direction.y));
    if (std::fabs(direction.z) > 1.0e-7f) distance = std::fmin(distance, half.z / std::fabs(direction.z));
    if (!std::isfinite(distance)) distance = 0.0f;
    return bounds.center() + direction * distance;
}

// Local bounds at the sampled size of a circle or rectangle, whose size may animate, at the
// sampled points of a shape that a geometry clip changes, and of anything else as far as the
// sampled warp has carried it, or as it shows before any warp. False when there are none, or
// when memory runs out, which sets failed.
static bool _localBounds(const Object* object, const VisualState& state, Bounds& bounds, bool& failed)
{
    Vec3 center;
    Vec3 half;
    if (state.warp) {
        return localBounds(object, state.warp, state.warpProgress, bounds);
    } else if (object->type() == Type::Arc || object->type() == Type::Angle) {
        return arcBounds(arcGeometry(object, &state), bounds);
    } else if (auto count = drawnCount(object, state)) {
        auto points = new (std::nothrow) Vec3[count];
        if (!points) {
            failed = true;
            return false;
        }
        drawn(object, state, points);
        auto defined = false;
        for (auto i = 0u; i < count; i++) include(bounds, defined, points[i]);
        delete[] points;
        return defined;
    } else if ((state.geometry && object->type() == Type::Plot)
               || object->type() == Type::Line || object->type() == Type::Arrow || object->type() == Type::Vector) {
        // An empty plot or a consumed segment has no bounds.
        return false;
    } else if (object->type() == Type::Circle) {
        center = static_cast<const Circle*>(object)->center;
        half = {state.size.x, state.size.x, 0.0f};
    } else if (object->type() == Type::Rectangle) {
        center = static_cast<const Rectangle*>(object)->center;
        half = {state.size.x * 0.5f, state.size.y * 0.5f, 0.0f};
    } else {
        return localBounds(object, nullptr, 0.0f, bounds);
    }
    auto defined = false;
    include(bounds, defined, center - half);
    include(bounds, defined, center + half);
    return true;
}

Result Scene::Impl::bounds(const Object* object, float time, Bounds& bounds, bool& defined, bool hidden) const noexcept
{
    VisualState state;
    auto visible = false;
    auto result = world(object, time, state, visible);
    if (result != Result::Success) return result;
    if (visible && (hidden || (state.opacity > 0.0f && (state.progress > 0.0f || plotEndpoint(object, time, state)))) && object->type() != Type::Connector) {
        if (state.morph) {
            auto shape = state.morphShape;
            if (!shape || !shape->count) return Result::NonSupport;
            for (auto i = 0u; i < shape->count; i++) {
                auto point = shape->from[i] + (shape->to[i] - shape->from[i]) * state.morphProgress;
                include(bounds, defined, state.model * point);
            }
        } else if (state.warp && _bends(object->type())) {
            // Each point the warp has reached, but where a space writes its numbers and ticks.
            auto& warp = *state.warp;
            auto begin = 0u;
            for (auto run = 0u; run < warp.runCount; run++) {
                auto marks = spaceMark(warp.runs[run].part);
                for (auto i = begin; i < warp.runs[run].end && !marks; i++)
                    include(bounds, defined, state.model * warp.at(i, state.warpProgress));
                begin = warp.runs[run].end;
            }
        } else {
            Bounds local;
            auto failed = false;
            if (_localBounds(object, state, local, failed)) include(bounds, defined, local, state.model);
            if (failed) return Result::OutOfMemory;
        }
    }
    for (auto i = 0u; i < object->childCount(); i++) {
        result = this->bounds(object->childAt(i), time, bounds, defined, hidden);
        if (result != Result::Success) return result;
    }
    return Result::Success;
}

// A ray against a segment in world space. The furthest forward crossing wins, including
// concave contours and separate closed shapes below a group.
static void _crossing(const Vec3& origin, const Vec3& direction, const Vec3& a, const Vec3& b, float& reach)
{
    auto edge = b - a;
    auto normal = direction.cross(edge);
    auto denominator = normal.dot(normal);
    if (denominator <= 1.0e-14f) return;
    auto offset = a - origin;
    auto t = offset.cross(edge).dot(normal) / denominator;
    auto u = offset.cross(direction).dot(normal) / denominator;
    if (t >= 0.0f && u >= -1.0e-6f && u <= 1.000001f
        && (origin + direction * t - (a + edge * u)).length() <= 1.0e-4f) reach = std::fmax(reach, t);
}

// Read a world ray in the shape's local xy plane, including nonuniform scale and shear.
static bool _planeRay(const Mat4& model, const Vec3& center, const Vec3& origin, const Vec3& direction,
                      Vec2& point, Vec2& ray)
{
    auto base = model * center;
    auto x = model * (center + Vec3{1, 0, 0}) - base;
    auto y = model * (center + Vec3{0, 1, 0}) - base;
    auto normal = x.cross(y);
    auto determinant = normal.dot(normal);
    if (determinant <= 1.0e-14f) return false;
    auto offset = origin - base;
    auto tolerance = 1.0e-4f * std::sqrt(determinant);
    if (std::fabs(offset.dot(normal)) > tolerance || std::fabs(direction.dot(normal)) > tolerance) return false;
    auto xx = x.dot(x);
    auto yy = y.dot(y);
    auto xy = x.dot(y);
    point = {(offset.dot(x) * yy - offset.dot(y) * xy) / determinant,
             (offset.dot(y) * xx - offset.dot(x) * xy) / determinant};
    ray = {(direction.dot(x) * yy - direction.dot(y) * xy) / determinant,
           (direction.dot(y) * xx - direction.dot(x) * xy) / determinant};
    return true;
}

// Exact circle crossings; a rounded corner only accepts its own quadrant.
static void _roundCrossing(const Vec2& point, const Vec2& ray, const Vec2& center, float radius,
                           float xsign, float ysign, float& reach)
{
    auto x = static_cast<double>(point.x - center.x);
    auto y = static_cast<double>(point.y - center.y);
    auto a = static_cast<double>(ray.x) * ray.x + static_cast<double>(ray.y) * ray.y;
    auto b = x * ray.x + y * ray.y;
    auto c = x * x + y * y - static_cast<double>(radius) * radius;
    auto discriminant = b * b - a * c;
    if (a <= 0.0 || discriminant < 0.0) return;
    for (auto sign : {-1.0, 1.0}) {
        auto t = (-b + sign * std::sqrt(discriminant)) / a;
        if (t < 0.0 || (xsign && (x + t * ray.x) * xsign < -1.0e-6)
            || (ysign && (y + t * ray.y) * ysign < -1.0e-6)) continue;
        reach = std::fmax(reach, static_cast<float>(t));
    }
}

template<typename Impl>
static Result _outlineEdge(const Impl& impl, const Object* object, float time, const Vec3& origin,
                           const Vec3& direction, float& reach)
{
    VisualState state;
    auto alive = false;
    auto result = impl.world(object, time, state, alive);
    if (result != Result::Success) return result;
    if (!alive) return Result::Success;
    auto type = object->type();
    if (!state.warp && (type == Type::Circle || type == Type::Rectangle)) {
        auto center = type == Type::Circle ? static_cast<const Circle*>(object)->center
                                          : static_cast<const Rectangle*>(object)->center;
        Vec2 point;
        Vec2 ray;
        if (_planeRay(state.model, center, origin, direction, point, ray)) {
            if (type == Type::Circle) _roundCrossing(point, ray, {}, state.size.x, 0, 0, reach);
            else {
                auto half = state.size * 0.5f;
                auto radius = std::fmin(state.corner, std::fmin(half.x, half.y));
                auto segment = [&](Vec2 a, Vec2 b) {
                    _crossing(origin, direction, state.model * (center + Vec3{a.x, a.y, 0}),
                              state.model * (center + Vec3{b.x, b.y, 0}), reach);
                };
                segment({-half.x + radius, half.y}, {half.x - radius, half.y});
                segment({-half.x + radius, -half.y}, {half.x - radius, -half.y});
                segment({half.x, -half.y + radius}, {half.x, half.y - radius});
                segment({-half.x, -half.y + radius}, {-half.x, half.y - radius});
                if (radius > 0.0f) {
                    for (auto x : {-1.0f, 1.0f}) {
                        for (auto y : {-1.0f, 1.0f}) {
                            _roundCrossing(point, ray, {x * (half.x - radius), y * (half.y - radius)},
                                           radius, x, y, reach);
                        }
                    }
                }
            }
        }
    } else if (state.warp) {
        auto& warp = *state.warp;
        auto begin = 0u;
        for (auto run = 0u; run < warp.runCount; run++) {
            auto end = warp.runs[run].end;
            if (warp.closed && warp.runs[run].part == WarpPart::Outline) {
                for (auto i = begin; i < end; i++) {
                    auto next = i + 1u == end ? begin : i + 1u;
                    _crossing(origin, direction, state.model * warp.at(i, state.warpProgress),
                              state.model * warp.at(next, state.warpProgress), reach);
                }
            }
            begin = end;
        }
    } else if (type == Type::Polygon || (type == Type::Path && static_cast<const Path*>(object)->closed())) {
        auto path = type == Type::Path ? static_cast<const Path*>(object) : nullptr;
        auto count = drawnCount(object, state);
        if (!count) count = path ? path->count() : static_cast<const Polygon*>(object)->count();
        auto points = new (std::nothrow) Vec3[count];
        if (!points) return Result::OutOfMemory;
        if (state.geometry) drawn(object, state, points);
        else {
            auto source = path ? path->points() : static_cast<const Polygon*>(object)->points();
            std::memcpy(points, source, count * sizeof(Vec3));
        }
        auto begin = 0u;
        auto contours = path ? path->contours() : 1u;
        for (auto run = 0u; run < contours; run++) {
            auto end = path ? path->contourEnd(run) : count;
            for (auto i = begin; i < end; i++) {
                auto next = i + 1u == end ? begin : i + 1u;
                _crossing(origin, direction, state.model * points[i], state.model * points[next], reach);
            }
            begin = end;
        }
        delete[] points;
    }
    for (auto i = 0u; i < object->childCount(); i++) {
        result = _outlineEdge(impl, object->childAt(i), time, origin, direction, reach);
        if (result != Result::Success) return result;
    }
    return Result::Success;
}

Result Scene::Impl::ends(const Connector* connector, float time, Vec3& from, Vec3& to, bool& defined) const noexcept
{
    defined = false;
    Bounds source;
    Bounds destination;
    auto sourceDefined = false;
    auto destinationDefined = false;
    auto result = bounds(connector->from(), time, source, sourceDefined, true);
    if (result != Result::Success) return result;
    result = bounds(connector->to(), time, destination, destinationDefined, true);
    if (result != Result::Success) return result;
    if (!sourceDefined || !destinationDefined) return Result::Success;
    auto delta = destination.center() - source.center();
    auto length = delta.length();
    if (!std::isfinite(length) || length <= 1.0e-7f) return Result::Success;
    auto direction = delta / length;
    auto sourceReach = -1.0f;
    auto destinationReach = -1.0f;
    result = _outlineEdge(*this, connector->from(), time, source.center(), direction, sourceReach);
    if (result != Result::Success) return result;
    result = _outlineEdge(*this, connector->to(), time, destination.center(), direction * -1.0f, destinationReach);
    if (result != Result::Success) return result;
    from = sourceReach >= 0.0f ? source.center() + direction * sourceReach : _edge(source, direction);
    to = destinationReach >= 0.0f ? destination.center() - direction * destinationReach : _edge(destination, direction * -1.0f);
    from = from + direction * connector->padding;
    to = to - direction * connector->padding;
    defined = (to - from).dot(direction) > 1.0e-7f;
    return Result::Success;
}

Result Scene::Impl::layer(const Object* object, float time, DisplayList& list,
                          uint32_t& index) const noexcept
{
    index = 0u;
    auto owner = object;
    while (owner && !composited(owner)) owner = owner->parent();
    if (!owner) return Result::Success;
    index = list.find(owner);
    if (index) return Result::Success;
    DisplayLayer entry;
    entry.object = owner;
    entry.blend = owner->objBlend;
    VisualState state;
    auto visible = false;
    Result result;
    if (owner->objMasked) {
        // A mask draws on its own, faded by the groups it does not share with the object it
        // masks; the layer of that object carries the groups above both.
        const Object* shared = nullptr;
        for (auto above = owner->parent(); above && !shared; above = above->parent()) {
            for (auto masked = owner->objMasked; masked && !shared; masked = masked->parent()) {
                if (masked == above) shared = above;
            }
        }
        result = world(owner, time, state, visible, WorldOpacity::Mask, shared);
        entry.masking = true;
    } else {
        uint32_t parent;
        result = layer(owner->parent(), time, list, parent);
        if (result != Result::Success) return result;
        result = world(owner, time, state, visible, WorldOpacity::Layer);
        entry.parent = parent;
        entry.level = parent ? list.layers[parent - 1u].level + 1u : 1u;
    }
    if (result != Result::Success) return result;
    entry.layer = visible ? state.layer : owner->layer;
    entry.opacity = visible ? _clamp(state.opacity) : 0.0f;
    entry.blur = state.blur;
    entry.shadow = state.shadow;
    index = list.add(entry);
    if (!index) return Result::OutOfMemory;
    if (!owner->objMask) return Result::Success;
    // The mask exists even while its target draws nothing, so that it hides the object then.
    uint32_t mask;
    result = layer(owner->objMask, time, list, mask);
    if (result != Result::Success) return result;
    list.layers[index - 1u].mask = mask;
    list.layers[index - 1u].maskMode = owner->objMaskMode;
    return Result::Success;
}

Result Scene::Impl::matched(const Scene* from, const Scene* to, const SceneTransition& transition,
                            float progress, float fromTime, float toTime,
                            DisplayList* parts, Config* spaces) noexcept
{
    DisplayList first;
    DisplayList second;
    auto result = from->pImpl->build(fromTime, first, false);
    if (result == Result::Success) result = to->pImpl->build(toTime, second, false);
    auto& fromConfig = from->pImpl->cfg;
    auto& toConfig = to->pImpl->cfg;
    Vec2 fromSize = {static_cast<float>(fromConfig.width), static_cast<float>(fromConfig.height)};
    Vec2 toSize = {static_cast<float>(toConfig.width), static_cast<float>(toConfig.height)};
    float progresses[] = {0.0f, 1.0f, progress};
    uint8_t selections[] = {BLEND_FROM, BLEND_TO, BLEND_MATCHED};
    for (auto i = 0u; i < 3u && result == Result::Success; i++) {
        result = parts[i].blend(first, second, transition.matches, from->pImpl->objectCnt,
                                to->pImpl->objectCnt, fromSize, toSize, progresses[i],
                                selections[i]);
        spaces[i] = fromConfig;
    }
    spaces[1].background = toConfig.background;
    spaces[2].background.a = 0u;
    return result;
}

// Whether a view leaves object out: it or a group above it is among those the view hides.
static bool _hidden(const Object* object, const ViewFrame& frame)
{
    for (auto item = object; item; item = item->parent()) {
        for (auto i = 0u; i < frame.hiddenCount; i++) {
            if (frame.hidden[i] == item) return true;
        }
    }
    return false;
}

// The layer by which the drawings of an object take their place among the scene's own: its
// layer, or that of the outermost layer it draws into. A mask goes with the layer it masks
// and has no place of its own.
static bool _placed(const DisplayList& list, int32_t layer, int32_t& placed)
{
    placed = layer;
    auto index = list.composite;
    if (!index) return true;
    while (list.layers[index - 1u].parent) index = list.layers[index - 1u].parent;
    if (list.layers[index - 1u].masking) return false;
    placed = list.layers[index - 1u].layer;
    return true;
}

Result Scene::Impl::build(float time, DisplayList& list, bool sort, const ViewFrame* frame) const noexcept
{
    if (!std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    auto config = cfg;
    if (frame) {
        config.camera = frame->camera;
        config.cameraView = frame->cameraView;
    } else {
        auto cameraResult = sample(time, config.camera, config.cameraView);
        if (cameraResult != Result::Success) return cameraResult;
    }
    if (!valid(config)) return Result::InvalidArguments;
    auto layered = false;
    for (auto i = 0u; i < objectCnt && !layered; i++)
        layered = composited(objects[i].object);
    auto opacity = layered ? WorldOpacity::Drawing : WorldOpacity::All;
    // Whether each object is valid and as authored, found once for this build (see world()); without
    // the room for it, world() checks each time.
    auto known = std::unique_ptr<bool[]>(new (std::nothrow) bool[objectCnt]{});
    for (auto i = 0u; i < objectCnt; i++) {
        VisualState state;
        auto object = objects[i].object;
        list.object = object;
        auto visible = false;
        auto sampled = world(object, time, state, visible, opacity, nullptr, known.get());
        if (sampled != Result::Success) return sampled;
        if (!visible) continue;
        if (layered) {
            sampled = layer(object, time, list, list.composite);
            if (sampled != Result::Success) return sampled;
            // A layer faded out hides everything inside it.
            auto shown = true;
            for (auto index = list.composite; index && shown; index = list.layers[index - 1u].parent)
                shown = list.layers[index - 1u].opacity > 0.0f;
            if (!shown) continue;
        }
        if (frame) {
            int32_t placed;
            if ((_placed(list, state.layer, placed) && placed > frame->layer) || _hidden(object, *frame)) continue;
        }
        auto style = _style(object, state);
        _project(style.gradient, state.model, config);
        auto progress = _clamp(state.progress);
        auto endpoint = plotEndpoint(object, time, state) && !objects[i].windowed;
        if ((progress <= 0.0f && !endpoint) || state.pixelScale <= 0.0f) continue;
        Result result = Result::Success;

        if (state.morph) {
            result = object->type() == Type::SurfaceMesh
                         ? _morphSurface(list, static_cast<SurfaceMesh*>(object), state, config, style, progress)
                         : _morph(list, object, state, config, style);
            if (result != Result::Success) return result;
            continue;
        }
        if (state.warp && _bends(object->type())) {
            result = object->type() == Type::Space
                   ? _warpedSpace(list, static_cast<const Space*>(object), state, config, style)
                   : _warped(list, object, state, config, style, progress, objects[i].windowed, endpoint);
            if (result != Result::Success) return result;
            continue;
        }

        // In 3D, the depths of the points of what the object draws, for split().
        auto spatial = _spatial(config);
        float ends[2];
        // What the strokes of the object draw with; each drawing below sets the depths.
        auto pen = Pen{progress, state.direction, style, state.layer, 0.0f, nullptr, state.traceFill, _cap(object)};
        switch (object->type()) {
            // The trimmed ends of a line, an arrow or a vector, in the object's space, or the ends
            // a connector joins, in the world. Arrows and vectors draw as routes with their heads,
            // and so does a connector that has a head.
            case Type::Line:
            case Type::Arrow:
            case Type::Vector:
            case Type::Connector: {
                auto type = object->type();
                Vec3 endpoints[2];
                if (type == Type::Connector) {
                    auto defined = false;
                    result = this->ends(static_cast<Connector*>(object), time, endpoints[0], endpoints[1], defined);
                    if (result != Result::Success) return result;
                    if (!defined) break;
                } else if (!segment(object, &state, endpoints)) {
                    break;
                }
                Vec2 pixels[2];
                if (!_lineWorld(endpoints[0], endpoints[1], type == Type::Connector ? Mat4::identity() : state.model,
                                config, pixels[0], pixels[1], pen.depth, spatial ? ends : nullptr)) {
                    break;
                }
                pen.depths = spatial ? ends : nullptr;
                auto heads = type == Type::Arrow || type == Type::Vector
                             || (type == Type::Connector && (state.markerTip > 0.0f || state.markerTail > 0.0f));
                result = heads ? _route(list, pixels, 2u, state.markerTail * state.pixelScale,
                                        state.markerTip * state.pixelScale, pen)
                               : _line(list, pixels[0], pixels[1], pen);
                break;
            }
            case Type::Point: {
                auto point = static_cast<Point*>(object);
                Vec2 pixel;
                float depth;
                if (_point(_anchored(state, point->point), state.model, config, pixel, depth)) {
                    auto anchor = _screenDepth(depth, config);
                    result = list.circle(pixel, style.radius * progress, style, state.layer, depth,
                                         spatial ? &anchor : nullptr);
                }
                break;
            }
            case Type::Text: {
                auto text = static_cast<Text*>(object);
                Vec2 pixel;
                float depth;
                // The string this time shows: a counting text's number, or a text change on its
                // way, which have to be written; the text's own strings outlast the list.
                const char* value;
                char* written;
                result = shown(text, state, value, written);
                if (result != Result::Success) return result;
                auto anchor = _anchored(state, text->point);
                if (_point(anchor, state.model, config, pixel, depth)) {
                    style.fill = _opacity(style.fill, progress);
                    if (text->orientation == TextOrientation::Plane) {
                        // The model carries every scaling of a plane text, an effect's too.
                        constexpr auto ReferenceSize = 100.0f;
                        auto worldSize = state.size.x;
                        Vec2 x;
                        Vec2 y;
                        float xDepth;
                        float yDepth;
                        if (_point(anchor + Vec3{worldSize, 0.0f, 0.0f}, state.model,
                                   config, x, xDepth)
                            && _point(anchor + Vec3{0.0f, -worldSize, 0.0f}, state.model,
                                      config, y, yDepth)) {
                            Mat3 transform = {{
                                (x.x - pixel.x) / ReferenceSize,
                                (y.x - pixel.x) / ReferenceSize, pixel.x,
                                (x.y - pixel.y) / ReferenceSize,
                                (y.y - pixel.y) / ReferenceSize, pixel.y,
                                0.0f, 0.0f, 1.0f}};
                            result = list.text(pixel, value, text->font(), ReferenceSize,
                                               text->align, style, state.layer,
                                               (depth + xDepth + yDepth) / 3.0f, &transform,
                                               written != nullptr, text->slant, text->tabular);
                        }
                    } else {
                        // A billboard keeps the size in pixels it was authored with, enlarged as
                        // far as the models above it have scaled since then: by a scale, a
                        // transform, a scaled group or an effect.
                        auto size = state.size.x
                                  * billboardScale(billboardReference(object), state.model, state.pixelScale);
                        if (size > 0.0f && std::isfinite(size)) {
                            if (text->objScreenGap >= 0.0f && object->parent() == object->objLabelOf) {
                                auto space = object->objLabelOf;
                                VisualState spaceState;
                                bool visible;
                                result = world(space, time, spaceState, visible);
                                if (result != Result::Success) { delete[] written; return result; }
                                Vec2 origin;
                                Vec3 originPoint;
                                spaceOrigin(spaceState.warp, spaceState.warpProgress, originPoint);
                                float originDepth;
                                Vec2 direction{0.0f, -1.0f};
                                if (visible && _point(originPoint, spaceState.model, config, origin, originDepth)) {
                                    direction = pixel - origin;
                                    auto length = direction.length();
                                    direction = length > 1.0e-6f ? direction / length : Vec2{0.0f, -1.0f};
                                }
                                TextExtent extent;
                                result = text->measure(value, extent);
                                if (result != Result::Success) { delete[] written; return result; }
                                auto scale = size / text->size;
                                Vec2 half{(extent.right - extent.left) * 0.5f * scale,
                                          (extent.top - extent.bottom) * 0.5f * scale};
                                Vec2 center{((extent.left + extent.right) * 0.5f - text->align.x * extent.width) * scale,
                                            (extent.ascent - text->align.y * extent.height
                                             - (extent.top + extent.bottom) * 0.5f) * scale};
                                auto support = std::fabs(direction.x) * half.x + std::fabs(direction.y) * half.y;
                                pixel = pixel + direction * ((space->tickLength * 0.5f + text->objScreenGap)
                                                             * state.pixelScale + support) - center;
                            }
                            result = list.text(pixel, value, text->font(), size, text->align, style,
                                               state.layer, depth, nullptr, written != nullptr,
                                               text->slant, text->tabular);
                        }
                    }
                }
                delete[] written;
                break;
            }
            case Type::Space: result = _space(list, static_cast<Space*>(object), state, config, style); break;
            case Type::Arc:
            case Type::Angle: {
                auto arc = arcGeometry(object, &state);
                Vec3 local[514]; Vec2 points[514]; float depths[514];
                auto count = arcOutline(arc, local, 514u);
                if (count > 1u
                    && _project(local, count, state.model, config, points, spatial ? depths : nullptr, pen.depth)) {
                    pen.depths = spatial ? depths : nullptr;
                    result = _path(list, points, count, arc.sector, pen);
                }
                break;
            }
            case Type::Circle: {
                auto circle = static_cast<Circle*>(object);
                auto at = [&](uint32_t j) { return circlePoint(circle->center, state.size.x, j); };
                Vec2 points[CIRCLE_POINTS];
                float depths[CIRCLE_POINTS];
                if (_project(CIRCLE_POINTS, at, state.model, config, points, spatial ? depths : nullptr, pen.depth)) {
                    pen.depths = spatial ? depths : nullptr;
                    result = _path(list, points, CIRCLE_POINTS, true, pen);
                }
                break;
            }
            case Type::Polygon:
            case Type::Plot:
            case Type::Path:
            case Type::Curve: {
                auto points = static_cast<const Vec3*>(nullptr);
                auto count = 0u;
                auto closed = false;
                if (object->type() == Type::Polygon) {
                    auto polygon = static_cast<Polygon*>(object);
                    points = polygon->points();
                    count = polygon->count();
                    closed = true;
                } else if (object->type() == Type::Plot) {
                    // A geometry clip gives the points of the shape the plot shows, which may
                    // have other pieces than the plot has now; a plot left empty draws nothing.
                    auto plot = static_cast<Plot*>(object);
                    points = plot->points();
                    count = state.geometry ? drawnCount(object, state) : plot->count();
                    if (!count) break;
                } else if (object->type() == Type::Path) {
                    auto path = static_cast<Path*>(object);
                    points = path->points();
                    count = path->count();
                    closed = path->closed();
                } else {
                    auto curve = static_cast<Curve*>(object);
                    points = curve->points();
                    count = curve->count();
                }
                // A geometry clip gives the points on their way; there are as many.
                Vec3* shaped = nullptr;
                if (drawnCount(object, state) == count) {
                    shaped = new (std::nothrow) Vec3[count];
                    if (!shaped) return Result::OutOfMemory;
                    drawn(object, state, shaped);
                    points = shaped;
                }
                auto projected = new (std::nothrow) Vec2[count];
                if (!projected) {
                    delete[] shaped;
                    return Result::OutOfMemory;
                }
                // Without memory for the depths the drawing stays whole in split().
                auto depths = spatial ? new (std::nothrow) float[count] : nullptr;
                auto visible = _project(points, count, state.model, config, projected, depths, pen.depth);
                pen.depths = depths;
                auto path = object->type() == Type::Path ? static_cast<Path*>(object) : nullptr;
                auto plot = object->type() == Type::Plot ? static_cast<Plot*>(object) : nullptr;
                auto shown = plot ? state.geometry : nullptr;
                auto geometry = shown ? plotAt(state) : GeometryShape{};
                auto contours = path ? path->contours() : shown ? geometry.pieces : plot ? plot->contours() : 1u;
                if (visible && plot && plot->contourLabels() && contours) {
                    auto ends = std::unique_ptr<uint32_t[]>(new (std::nothrow) uint32_t[contours]);
                    auto labels = std::unique_ptr<ContourPlacement[]>(new (std::nothrow) ContourPlacement[plot->childCount()]);
                    if (!ends || !labels) result = Result::OutOfMemory;
                    else {
                        if (shown) plotAt(state, nullptr, ends.get());
                        else for (auto j = 0u; j < contours; j++) ends[j] = plot->contourEnd(j);
                        uint32_t labelCount;
                        result = contourPlacements(plot, time, labels.get(), plot->childCount(), labelCount);
                        if (result == Result::Success) result = _labelledPieces(list, points, projected, ends.get(), contours,
                            labels.get(), labelCount, pen);
                        if (result == Result::Success && endpoint) {
                            result = _plotEndpoint(list, projected, count, ends.get(), contours,
                                                   plot->endpointRadius * state.pixelScale, pen);
                        }
                        endpoint = false;
                    }
                } else if (visible && (path || plot) && contours > 1u) {
                    auto ends = new (std::nothrow) uint32_t[contours];
                    if (!ends) {
                        delete[] projected;
                        delete[] depths;
                        delete[] shaped;
                        return Result::OutOfMemory;
                    }
                    if (shown) plotAt(state, nullptr, ends);
                    else for (auto j = 0u; j < contours; j++)
                        ends[j] = path ? path->contourEnd(j) : plot->contourEnd(j);
                    result = path ? _contours(list, projected, ends, contours, closed, pen)
                                  : _pieces(list, projected, ends, contours, pen);
                    if (result == Result::Success && endpoint) {
                        result = _plotEndpoint(list, projected, count, ends, contours,
                                               plot->endpointRadius * state.pixelScale, pen);
                    }
                    delete[] ends;
                    endpoint = false;
                } else if (visible && objects[i].windowed) {
                    result = _window(list, projected, count, state.start, progress, style, object->layer, pen.depth);
                } else if (visible) {
                    result = _path(list, projected, count, closed, pen);
                }
                if (result == Result::Success && visible && endpoint) {
                    result = _plotEndpoint(list, projected, count, nullptr, 1u,
                                           plot->endpointRadius * state.pixelScale, pen);
                }
                delete[] projected;
                delete[] depths;
                delete[] shaped;
                break;
            }
            case Type::Route: {
                auto route = static_cast<DirectedRoute*>(object);
                auto count = route->count();
                auto points = route->points();
                Vec3* shaped = nullptr;
                if (drawnCount(object, state) == count) {
                    shaped = new (std::nothrow) Vec3[count];
                    if (!shaped) return Result::OutOfMemory;
                    drawn(object, state, shaped);
                    points = shaped;
                }
                auto projected = new (std::nothrow) Vec2[count];
                if (!projected) {
                    delete[] shaped;
                    return Result::OutOfMemory;
                }
                auto depths = spatial ? new (std::nothrow) float[count] : nullptr;
                if (_project(points, count, state.model, config, projected, depths, pen.depth)) {
                    pen.depths = depths;
                    result = _route(list, projected, count, state.markerTail * state.pixelScale,
                                    state.markerTip * state.pixelScale, pen);
                }
                delete[] projected;
                delete[] depths;
                delete[] shaped;
                break;
            }
            case Type::Picture: {
                auto image = static_cast<Picture*>(object);
                auto height = image->width * static_cast<float>(image->pixelHeight()) / static_cast<float>(image->pixelWidth());
                auto center = _anchored(state, image->center);
                Vec3 local[3] = {
                    center + Vec3{-image->width * 0.5f, height * 0.5f, 0.0f},
                    center + Vec3{image->width * 0.5f, height * 0.5f, 0.0f},
                    center + Vec3{-image->width * 0.5f, -height * 0.5f, 0.0f}};
                Vec2 corners[3];
                auto depth = 0.0f;
                auto visible = _project(local, 3u, state.model, config, corners, nullptr, depth);
                auto opacity = _alpha(_clamp(state.opacity * progress));
                if (visible && opacity) {
                    PictureSource source;
                    source.encoded = image->encoded();
                    source.encodedSize = image->encodedSize();
                    source.mime = image->mime();
                    source.pixels = image->pixels();
                    source.width = image->pixelWidth();
                    source.height = image->pixelHeight();
                    result = list.picture(corners, source, image->filter, opacity, state.layer, depth);
                }
                break;
            }
            case Type::Cell: {
                auto cell = static_cast<Cell*>(object);
                auto total = cell->columns() * cell->rows();
                auto visible = static_cast<uint32_t>(std::ceil(static_cast<float>(total) * progress));
                if (visible > total) visible = total;
                auto colors = cell->colors();
                auto nextColors = colors;
                auto colorProgress = 0.0f;
                auto frames = sampleFrames(cell);
                if (frames > 1u) {
                    auto duration = sampleDuration(cell);
                    auto position = _clamp((time - objects[i].born) / duration)
                                  * static_cast<float>(frames - 1u);
                    auto frame = static_cast<uint32_t>(std::floor(position));
                    if (frame >= frames - 1u) {
                        frame = frames - 1u;
                    } else {
                        colorProgress = position - static_cast<float>(frame);
                        nextColors = sampleColors(cell, frame + 1u);
                    }
                    colors = sampleColors(cell, frame);
                }
                auto padding = cell->mode == CellMode::Padd ? cell->padding : 0.0f;
                auto reverse = _reverse(state.direction);
                for (auto shown = 0u; shown < visible; shown++) {
                    auto index = reverse ? total - 1u - shown : shown;
                    auto color = colors[index];
                    if (colorProgress > 0.0f) color = _mix(color, nextColors[index], colorProgress);
                    if (!color.a) continue;
                    auto x = index % cell->columns();
                    auto y = index / cell->columns();
                    auto origin = cell->origin + Vec3{static_cast<float>(x), static_cast<float>(y), 0.0f};
                    if (config.cameraView == CameraView::ThreeD) {
                        result = _voxel(list, origin, padding, cell->depth, state.model, config,
                                        color, state.opacity, state.layer);
                    } else {
                        Vec3 local[4] = {
                            origin + Vec3{padding, padding, 0.0f},
                            origin + Vec3{1.0f - padding, padding, 0.0f},
                            origin + Vec3{1.0f - padding, 1.0f - padding, 0.0f},
                            origin + Vec3{padding, 1.0f - padding, 0.0f}};
                        Vec2 points[4];
                        auto depth = 0.0f;
                        if (_project(local, 4u, state.model, config, points, nullptr, depth)) {
                            Style cellStyle;
                            cellStyle.stroke.a = 0;
                            cellStyle.fill = _opacity(color, state.opacity);
                            cellStyle.width = 0.0f;
                            result = list.path(points, 4, true, cellStyle, state.layer, depth);
                        }
                    }
                    if (result != Result::Success) return result;
                }
                break;
            }
            case Type::SurfaceMesh: {
                auto surface = static_cast<SurfaceMesh*>(object);
                result = _surfaceFaces(list, surface, surface->points(), nullptr, state, config, style, progress);
                if (result != Result::Success) return result;
                break;
            }
            case Type::Group: break;
            case Type::Rectangle: {
                auto rectangle = static_cast<Rectangle*>(object);
                auto half = Vec2{state.size.x * 0.5f, state.size.y * 0.5f};
                // Size and corner animate on their own channels, so the corner may outgrow a
                // shrinking side for a moment.
                auto radius = std::fmin(state.corner, std::fmin(half.x, half.y));
                Vec3 local[CORNER_POINTS * 4u];
                auto count = rectangleOutline(rectangle->center, half, radius, local);
                Vec2 points[CORNER_POINTS * 4u];
                float depths[CORNER_POINTS * 4u];
                if (_project(local, count, state.model, config, points, spatial ? depths : nullptr, pen.depth)) {
                    pen.depths = spatial ? depths : nullptr;
                    result = _path(list, points, count, true, pen);
                }
                break;
            }
        }
        if (result != Result::Success) return result;
    }
    list.object = nullptr;
    list.composite = 0u;
    if (!sort) return Result::Success;
    auto result = list.split(config);
    return result == Result::Success ? list.sort() : result;
}

}  // namespace vis
