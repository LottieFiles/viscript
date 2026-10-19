#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <limits>
#include <new>

#include "vis.h"
#include "visScene.h"

#include "../common/visConstants.h"

namespace vis
{

static constexpr uint32_t CONTOUR_SAMPLE_LIMIT = 65536u;

struct ContourEdge
{
    Vec3 point;
    uint32_t next[2] = {};
    uint8_t degree = 0;
    bool visited = false;
};

static Result _samples(const Range& range, uint32_t& count)
{
    if (!std::isfinite(range.min) || !std::isfinite(range.max) || !std::isfinite(range.step)
        || range.min > range.max || range.step <= 0.0f) return Result::InvalidArguments;
    auto steps = std::floor((static_cast<double>(range.max) - range.min) / range.step + 1.0e-4);
    if (steps < 1.0) return Result::InvalidArguments;
    if (steps >= CONTOUR_SAMPLE_LIMIT) return Result::NonSupport;
    count = static_cast<uint32_t>(steps) + 1u;
    // Adjacent grid points must still be distinct in the retained float coordinates.
    auto previous = range.min;
    for (auto i = 1u; i < count; i++) {
        auto point = static_cast<float>(static_cast<double>(range.min) + i * static_cast<double>(range.step));
        if (!std::isfinite(point) || point <= previous) return Result::InvalidArguments;
        previous = point;
    }
    return Result::Success;
}

static float _at(const Range& range, uint32_t i)
{
    return static_cast<float>(static_cast<double>(range.min) + static_cast<double>(range.step) * i);
}

Result Plot::contour(const Range& xRange, const Range& yRange, const double* values,
                     uint32_t count, double level, Plot*& output,
                     ContourSampler sampler, void* data, uint32_t pointLimit) noexcept
{
    uint32_t columns, rows;
    auto result = _samples(xRange, columns);
    if (result != Result::Success) return result;
    result = _samples(yRange, rows);
    if (result != Result::Success) return result;
    if (static_cast<uint64_t>(columns) * rows > CONTOUR_SAMPLE_LIMIT) return Result::NonSupport;
    if (!values || count != columns * rows || !(std::fabs(level) <= FLT_MAX)) return Result::InvalidArguments;
    for (auto i = 0u; i < count; i++) {
        if (!(std::fabs(values[i]) <= FLT_MAX)) return Result::InvalidArguments;
    }
    auto horizontal = (columns - 1u) * rows;
    auto edgeCount = horizontal + columns * (rows - 1u);
    auto edges = std::unique_ptr<ContourEdge[]>(new (std::nothrow) ContourEdge[edgeCount]);
    if (!edges) return Result::OutOfMemory;
    // An edge has one crossing shared by its two cells. Equal samples take the high side;
    // an edge entirely on the level has no crossing, and isolated touching points draw nothing.
    for (auto i = 0u; i < edgeCount; i++) {
        auto alongX = i < horizontal;
        auto j = alongX ? i : i - horizontal;
        auto x = j % (alongX ? columns - 1u : columns);
        auto y = j / (alongX ? columns - 1u : columns);
        auto a = y * columns + x;
        auto b = a + (alongX ? 1u : columns);
        auto va = values[a], vb = values[b];
        if ((va >= level) == (vb >= level)) continue;
        auto ax = static_cast<double>(_at(xRange, x)), ay = static_cast<double>(_at(yRange, y));
        auto bx = static_cast<double>(_at(xRange, x + (alongX ? 1u : 0u)));
        auto by = static_cast<double>(_at(yRange, y + (alongX ? 0u : 1u)));
        auto low = 0.0, high = 1.0;
        if (va == level) high = 0.0;
        else if (vb == level) low = 1.0;
        else for (auto n = 0u; n < 40u; n++) {
            auto t = (low + high) * 0.5;
            auto value = va + (vb - va) * t;
            if (sampler && (!sampler(ax + (bx - ax) * t, ay + (by - ay) * t, value, data)
                            || !(std::fabs(value) <= FLT_MAX))) return Result::InvalidArguments;
            if (value == level) {
                low = high = t;
                break;
            }
            if ((value >= level) == (va >= level)) low = t;
            else high = t;
        }
        auto t = (low + high) * 0.5;
        edges[i].point = {static_cast<float>(ax + (bx - ax) * t),
                          static_cast<float>(ay + (by - ay) * t), 0.0f};
    }
    auto join = [&](uint32_t a, uint32_t b) {
        edges[a].next[edges[a].degree++] = b;
        edges[b].next[edges[b].degree++] = a;
    };
    for (auto y = 0u; y + 1u < rows; y++) {
        for (auto x = 0u; x + 1u < columns; x++) {
            auto a = y * columns + x;
            double v[] = {values[a] - level, values[a + 1u] - level,
                          values[a + columns + 1u] - level, values[a + columns] - level};
            uint32_t ids[] = {y * (columns - 1u) + x, horizontal + y * columns + x + 1u,
                              (y + 1u) * (columns - 1u) + x, horizontal + y * columns + x};
            uint32_t crossed[4];
            auto crossings = 0u;
            for (auto i = 0u; i < 4u; i++) {
                if ((v[i] >= 0.0) != (v[(i + 1u) % 4u] >= 0.0)) crossed[crossings++] = ids[i];
            }
            if (crossings == 2u) join(crossed[0], crossed[1]);
            else if (crossings == 4u) {
                auto scale = std::max({std::fabs(v[0]), std::fabs(v[1]), std::fabs(v[2]), std::fabs(v[3])});
                auto determinant = (v[0] / scale) * (v[2] / scale) - (v[1] / scale) * (v[3] / scale);
                if (determinant >= 0.0) {
                    join(ids[0], ids[1]);
                    join(ids[2], ids[3]);
                } else {
                    join(ids[0], ids[3]);
                    join(ids[1], ids[2]);
                }
            }
        }
    }
    auto capacity = std::min(pointLimit, edgeCount * 2u);
    auto points = std::unique_ptr<Vec3[]>(new (std::nothrow) Vec3[capacity]);
    auto ends = std::unique_ptr<uint32_t[]>(new (std::nothrow) uint32_t[edgeCount / 2u + 1u]);
    if (!points || !ends) return Result::OutOfMemory;
    auto used = 0u, pieces = 0u;
    // Boundary runs first, then loops. Each edge belongs to only one maximal run.
    for (auto pass = 0u; pass < 2u; pass++) {
        for (auto start = 0u; start < edgeCount; start++) {
            if (edges[start].visited || edges[start].degree != (pass ? 2u : 1u)) continue;
            auto begin = used, current = start, previous = edgeCount;
            do {
                auto& edge = edges[current];
                edge.visited = true;
                auto& p = edge.point;
                if (used == begin || points[used - 1u].x != p.x || points[used - 1u].y != p.y) {
                    if (used == capacity) return Result::NonSupport;
                    points[used++] = p;
                }
                auto next = edge.next[0];
                if (next == previous) next = edge.degree == 2u ? edge.next[1] : edgeCount;
                previous = current;
                current = next;
            } while (current != edgeCount && !edges[current].visited);
            if (current == start && used > begin + 1u
                && (points[used - 1u].x != points[begin].x || points[used - 1u].y != points[begin].y)) {
                if (used == capacity) return Result::NonSupport;
                points[used++] = points[begin];
            }
            if (used > begin + 1u) ends[pieces++] = used;
            else used = begin;
        }
    }
    auto plot = pieces ? gen(points.get(), used, ends.get(), pieces) : new (std::nothrow) Plot(nullptr, 0u);
    if (!plot) return Result::OutOfMemory;
    plot->flat = true;
    plot->objGrid = {xRange, yRange, level};
    output = plot;
    return Result::Success;
}

const ContourLabelOptions* Plot::contourLabels() const noexcept
{
    return objLabelled ? &objLabels : nullptr;
}

ContourGrid Plot::contourGrid() const noexcept
{
    return objGrid;
}

uint32_t Plot::labelSlots(const Vec3* points, uint32_t count, const uint32_t* ends, uint32_t pieces) const noexcept
{
    auto slots = uint64_t{0};
    auto begin = 0u;
    for (auto i = 0u; i < pieces; i++) {
        auto end = ends ? ends[i] : count;
        auto length = 0.0;
        for (auto j = begin + 1u; j < end; j++) {
            length += std::hypot(static_cast<double>(points[j].x) - points[j - 1u].x,
                                 static_cast<double>(points[j].y) - points[j - 1u].y);
        }
        auto repeats = objLabels.spacing > 0.0f ? std::max(1.0, std::floor(length / objLabels.spacing)) : 1.0;
        if (repeats > 4096u - slots) return 4097u;
        slots += static_cast<uint32_t>(repeats);
        begin = end;
    }
    return static_cast<uint32_t>(slots);
}

Result Plot::reserveLabels(const Vec3* points, uint32_t count, const uint32_t* ends, uint32_t pieces) noexcept
{
    if (!objLabelled) return Result::Success;
    auto slots = labelSlots(points, count, ends, pieces);
    if (slots > 4096u) return Result::NonSupport;
    NumberFormat format;
    format.decimals = static_cast<uint8_t>(objLabelFormat[2] - '0');
    while (objLabelCount < slots) {
        auto text = Text::gen("");
        if (!text) return Result::OutOfMemory;
        text->size = objLabels.size;
        text->orientation = TextOrientation::Plane;
        text->layer = layer;
        text->fill(style.stroke);
        auto result = text->count(objGrid.level, format);
        if (result == Result::Success && tag()) {
            auto size = std::strlen(tag()) + 32u;
            auto name = std::unique_ptr<char[]>(new (std::nothrow) char[size]);
            if (!name) { delete text; return Result::OutOfMemory; }
            std::snprintf(name.get(), size, "%s/label/%u", tag(), objLabelCount + 1u);
            text->tag(name.get());
        }
        if (result == Result::Success) result = add(text);
        if (result != Result::Success) { delete text; return result; }
        objLabelCount++;
    }
    return Result::Success;
}

Result Plot::labelContours(const ContourLabelOptions& options) noexcept
{
    if (owned() || objLabelled || childCount()) return Result::InsufficientCondition;
    if (!options.format || std::strlen(options.format) != 4u || options.format[0] != '%'
        || options.format[1] != '.' || options.format[2] < '0' || options.format[2] > '6'
        || options.format[3] != 'f' || !std::isfinite(options.size) || options.size <= 0.0f
        || !std::isfinite(options.spacing) || options.spacing < 0.0f
        || !std::isfinite(options.padding) || options.padding < 0.0f
        || objGrid.x.step <= 0.0f || objGrid.y.step <= 0.0f) return Result::InvalidArguments;
    objLabels = options;
    std::memcpy(objLabelFormat, options.format, sizeof(objLabelFormat));
    objLabels.format = objLabelFormat;
    if (labelSlots(objPoints, objCount, objEnds, objContours) > 4096u) return Result::NonSupport;
    objLabelled = true;
    return reserveLabels(objPoints, objCount, objEnds, objContours);
}

Result Scene::Impl::contourPlacements(const Plot* plot, float time, ContourPlacement* output,
                                    uint32_t capacity, uint32_t& count) const noexcept
{
    count = 0;
    auto boxes = std::unique_ptr<Bounds[]>(new (std::nothrow) Bounds[objectCnt]);
    if (!boxes) return Result::OutOfMemory;
    auto occupied = 0u;
    // Level plots visit in retained order; their boxes share the space of their parent.
    for (auto object = 0u; object < objectCnt; object++) {
        auto candidate = objects[object].object;
        if (candidate->type() != Type::Plot || candidate->parent() != plot->parent()) continue;
        auto curve = static_cast<const Plot*>(candidate);
        auto options = curve->contourLabels();
        VisualState state;
        if (!options || !sample(objects[object], time, state)) continue;
        auto shape = state.geometry ? plotAt(state) : GeometryShape{};
        auto vertices = state.geometry ? shape.count : curve->count();
        auto pieces = state.geometry ? shape.pieces : curve->contours();
        auto grid = state.geometry ? shape.contour : curve->contourGrid();
        auto points = std::unique_ptr<Vec3[]>(new (std::nothrow) Vec3[vertices]);
        auto ends = std::unique_ptr<uint32_t[]>(new (std::nothrow) uint32_t[pieces]);
        auto lengths = std::unique_ptr<float[]>(new (std::nothrow) float[vertices]);
        if (!points || !ends || !lengths) return Result::OutOfMemory;
        if (state.geometry) plotAt(state, points.get(), ends.get());
        else {
            if (vertices) std::memcpy(points.get(), curve->points(), sizeof(Vec3) * vertices);
            for (auto i = 0u; i < pieces; i++) ends[i] = curve->contourEnd(i);
        }
        auto begin = 0u, slot = 0u;
        for (auto piece = 0u; piece < pieces; piece++) {
            auto end = ends[piece];
            lengths[begin] = 0.0f;
            for (auto j = begin + 1u; j < end; j++) {
                lengths[j] = lengths[j - 1u] + std::hypot(points[j].x - points[j - 1u].x,
                                                        points[j].y - points[j - 1u].y);
            }
            auto length = lengths[end - 1u];
            auto closed = points[begin].x == points[end - 1u].x && points[begin].y == points[end - 1u].y;
            auto wanted = options->spacing > 0.0f ? std::max(1.0f, std::floor(length / options->spacing)) : 1.0f;
            auto at = [&](float distance) {
                distance = closed && length > 0.0f ? std::fmod(distance + length, length)
                                                   : std::clamp(distance, 0.0f, length);
                auto next = static_cast<uint32_t>(std::upper_bound(lengths.get() + begin, lengths.get() + end, distance)
                                                  - lengths.get());
                next = std::clamp(next, begin + 1u, end - 1u);
                auto span = lengths[next] - lengths[next - 1u];
                auto t = span > 0.0f ? (distance - lengths[next - 1u]) / span : 0.0f;
                return points[next - 1u] + (points[next] - points[next - 1u]) * t;
            };
            for (auto repeat = 0u; repeat < wanted && slot < curve->childCount(); repeat++, slot++) {
                auto child = curve->childAt(slot);
                if (child->type() != Type::Text) continue;
                auto text = static_cast<const Text*>(child);
                VisualState textState;
                auto textEntry = entry(text);
                if (!textEntry || !sample(*textEntry, time, textState)) continue;
                textState.value = static_cast<float>(grid.level);
                const char* value;
                char* written;
                auto result = shown(text, textState, value, written);
                if (result != Result::Success) return result;
                TextExtent extent;
                result = text->measure(value, extent);
                delete[] written;
                if (result != Result::Success) return result;
                Vec2 half{(extent.right - extent.left) * 0.5f + options->padding,
                          (extent.top - extent.bottom) * 0.5f + options->padding};
                if (length <= 2.0f * half.x) continue;
                auto best = std::numeric_limits<float>::infinity();
                ContourPlacement placement;
                Bounds box;
                for (auto trial = 0u; trial < 64u; trial++) {
                    auto fraction = std::fmod((repeat + 0.37f) / wanted + trial / 64.0f, 1.0f);
                    auto distance = length * fraction;
                    if (!closed && (distance < half.x || length - distance < half.x)) continue;
                    auto point = at(distance);
                    auto left = at(distance - half.x), right = at(distance + half.x);
                    auto tangent = right - left;
                    auto angle = std::atan2(tangent.y, tangent.x);
                    if (options->upright && (angle > HALF_PI || angle < -HALF_PI)) angle += angle > 0 ? -PI : PI;
                    auto model = Mat4::translate(point) * Mat4::rotateZ(angle);
                    Bounds bounds;
                    auto fits = true;
                    for (auto corner = 0u; corner < 4u; corner++) {
                        auto local = model * Vec3{corner & 1u ? half.x : -half.x, corner & 2u ? half.y : -half.y, 0.0f};
                        if (local.x < grid.x.min || local.x > grid.x.max || local.y < grid.y.min || local.y > grid.y.max) fits = false;
                        auto parentPoint = state.model * local;
                        if (!corner) bounds.min = bounds.max = parentPoint;
                        bounds.min.x = std::min(bounds.min.x, parentPoint.x);
                        bounds.min.y = std::min(bounds.min.y, parentPoint.y);
                        bounds.max.x = std::max(bounds.max.x, parentPoint.x);
                        bounds.max.y = std::max(bounds.max.y, parentPoint.y);
                    }
                    for (auto i = 0u; fits && i < occupied; i++) {
                        if (bounds.min.x < boxes[i].max.x && bounds.max.x > boxes[i].min.x
                            && bounds.min.y < boxes[i].max.y && bounds.max.y > boxes[i].min.y) fits = false;
                    }
                    if (!fits) continue;
                    auto a = point - left, b = right - point;
                    auto denominator = std::hypot(a.x, a.y) * std::hypot(b.x, b.y);
                    auto bend = denominator > 0.0f ? std::fabs(a.x * b.y - a.y * b.x) / denominator : 1.0f;
                    auto score = bend + trial * 0.0001f;
                    if (score >= best) continue;
                    best = score;
                    box = bounds;
                    placement = {text, point, angle, half, piece, distance};
                }
                if (!std::isfinite(best)) continue;
                boxes[occupied++] = box;
                if (curve == plot && count < capacity) output[count++] = placement;
            }
            begin = end;
        }
        if (curve == plot) break;
    }
    return Result::Success;
}

}
