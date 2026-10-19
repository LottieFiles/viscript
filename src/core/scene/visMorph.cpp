// Pairs the outlines of a morph: resampled to even arc length and aligned so that points travel
// the shortest way.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

#include "visMorph.h"
#include "visGeometry64.h"
#include "visMatch.h"
#include "../common/visSort.h"

namespace vis
{

// Even samples along both outlines that line a morph up.
static constexpr uint32_t MORPH_SAMPLES = 128;
// Fractions of the outline length closer than this are one point.
static constexpr double MORPH_EPSILON = 1.0e-9;

// An outline in local space as an arc-length polyline: points without repeats and, when it
// is closed, the first point again at the end. lengths[i] is the distance to point i.
struct MorphOutline
{
    Vector64* points = nullptr;
    double* lengths = nullptr;
    uint32_t count = 0;
    bool closed = false;

    ~MorphOutline()
    {
        delete[] points;
        delete[] lengths;
    }
};

static Result _morphOutline(const Vec3* raw, uint32_t count, bool closed, MorphOutline& output)
{
    if (count < 2u) return Result::InvalidArguments;
    if (count >= MAX_MORPH_POINTS) return Result::InsufficientCondition;
    output.points = new (std::nothrow) Vector64[count + 1u];
    output.lengths = new (std::nothrow) double[count + 1u];
    if (!output.points || !output.lengths) {
        return Result::OutOfMemory;
    }
    auto points = output.points;
    auto extent = 0.0;
    for (auto i = 0u; i < count; i++) {
        points[i] = {raw[i].x, raw[i].y, raw[i].z};
        extent = std::fmax(extent, std::fmax(std::fabs(points[i].x),
                                             std::fmax(std::fabs(points[i].y), std::fabs(points[i].z))));
    }
    auto tolerance = extent * 1.0e-9;
    auto unique = _parameterize(points, count, closed, tolerance, output.lengths);
    output.count = unique;
    output.closed = closed;
    auto total = output.lengths[unique - 1u];
    if (!std::isfinite(extent) || unique < 2u || !std::isfinite(total) || total <= 0.0) {
        return Result::InvalidArguments;
    }
    return Result::Success;
}

static Result _morphOutline(const Object* object, MorphOutline& output)
{
    auto closed = false;
    auto count = outline(object, nullptr, 0u, closed);
    if (count < 2u) return Result::InvalidArguments;
    if (count >= MAX_MORPH_POINTS) return Result::InsufficientCondition;
    auto raw = new (std::nothrow) Vec3[count];
    if (!raw) return Result::OutOfMemory;
    outline(object, raw, count, closed);
    auto result = _morphOutline(raw, count, closed, output);
    delete[] raw;
    return result;
}

// The point at the fraction u of the outline's length.
static Vector64 _morphPoint(const MorphOutline& outline, double u)
{
    auto lengths = outline.lengths;
    auto distance = std::fmin(std::fmax(u, 0.0), 1.0) * lengths[outline.count - 1u];
    auto low = 0u;
    auto high = outline.count - 1u;
    while (high - low > 1u) {
        auto middle = low + (high - low) / 2u;
        if (lengths[middle] <= distance) low = middle;
        else high = middle;
    }
    auto& a = outline.points[low];
    auto& b = outline.points[low + 1u];
    auto span = lengths[low + 1u] - lengths[low];
    auto t = span > 0.0 ? std::fmin(std::fmax((distance - lengths[low]) / span, 0.0), 1.0) : 0.0;
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

// Lines the target up with the source in the parent space they share, by the even samples
// that lie closest: the fraction of the target's length where the source's start lands, and
// whether the target runs the other way. A closed outline may start anywhere, an open one at
// either end.
static void _alignMorph(const MorphOutline& source, const Mat4& sourceModel, const MorphOutline& target,
                        const Mat4& targetModel, double& offset, bool& reversed, int directionOnly = -1)
{
    Vector64 from[MORPH_SAMPLES];
    Vector64 to[MORPH_SAMPLES];
    auto closed = source.closed;
    auto steps = static_cast<double>(closed ? MORPH_SAMPLES : MORPH_SAMPLES - 1u);
    for (auto i = 0u; i < MORPH_SAMPLES; i++) {
        auto u = static_cast<double>(i) / steps;
        auto a = _morphPoint(source, u);
        auto b = _morphPoint(target, u);
        from[i] = _point(sourceModel, Vec3{static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z)});
        to[i] = _point(targetModel, Vec3{static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z)});
    }
    auto best = std::numeric_limits<double>::infinity();
    auto shifts = closed ? MORPH_SAMPLES : 1u;
    offset = 0.0;
    reversed = false;
    for (auto direction = 0u; direction < 2u; direction++) {
        if (directionOnly >= 0 && direction != static_cast<uint32_t>(directionOnly)) continue;
        for (auto shift = 0u; shift < shifts; shift++) {
            auto cost = 0.0;
            for (auto i = 0u; i < MORPH_SAMPLES && cost < best; i++) {
                uint32_t j;
                if (closed) j = direction ? (shift + MORPH_SAMPLES - i) % MORPH_SAMPLES : (shift + i) % MORPH_SAMPLES;
                else j = direction ? MORPH_SAMPLES - 1u - i : i;
                auto x = from[i].x - to[j].x;
                auto y = from[i].y - to[j].y;
                auto z = from[i].z - to[j].z;
                cost += x * x + y * y + z * z;
            }
            if (cost < best) {
                best = cost;
                offset = closed ? static_cast<double>(shift) / MORPH_SAMPLES : 0.0;
                reversed = direction != 0u;
            }
        }
    }
}

// Where the target's vertices land along the source, in increasing order: the fraction of the
// source's length that meets each of them once the target is lined up.
static void _morphStops(const MorphOutline& target, double offset, bool reversed, double* stops)
{
    auto lengths = target.lengths;
    auto total = lengths[target.count - 1u];
    if (!target.closed) {
        for (auto i = 0u; i < target.count; i++) {
            auto vertex = reversed ? target.count - 1u - i : i;
            auto t = lengths[vertex] / total;
            stops[i] = reversed ? 1.0 - t : t;
        }
        return;
    }
    // A closed outline repeats its first point at the end, which is not a vertex of its own.
    auto vertices = target.count - 1u;
    auto count = 0u;
    auto stop = [&](uint32_t vertex, double shift) {
        auto t = lengths[vertex] / total;
        auto u = (reversed ? offset - t : t - offset) + shift;
        stops[count++] = std::fmin(std::fmax(u, 0.0), 1.0);
    };
    if (reversed) {
        auto first = 0u;
        while (first + 1u < vertices && lengths[first + 1u] / total <= offset) first++;
        for (auto i = first + 1u; i-- > 0u;)
            stop(i, 0.0);
        for (auto i = vertices; i-- > first + 1u;)
            stop(i, 1.0);
    } else {
        auto first = 0u;
        while (first < vertices && lengths[first] / total < offset) first++;
        for (auto i = first; i < vertices; i++)
            stop(i, 0.0);
        for (auto i = 0u; i < first; i++)
            stop(i, 1.0);
    }
}

static Result _resampleMorph(const MorphOutline& from, const Mat4& sourceModel,
                             const MorphOutline& to, const Mat4& targetModel,
                             MorphShape& shape, int directionOnly = -1)
{
    auto offset = 0.0;
    auto reversed = false;
    _alignMorph(from, sourceModel, to, targetModel, offset, reversed, directionOnly);

    // Every vertex of either outline is a point of both.
    auto closed = from.closed;
    auto sourceVertices = closed ? from.count - 1u : from.count;
    auto targetVertices = closed ? to.count - 1u : to.count;
    auto stops = new (std::nothrow) double[targetVertices];
    auto fractions = new (std::nothrow) double[sourceVertices + targetVertices];
    if (!stops || !fractions) {
        delete[] stops;
        delete[] fractions;
        return Result::OutOfMemory;
    }
    _morphStops(to, offset, reversed, stops);
    auto count = 0u;
    auto add = [&](double u) {
        if (count && u - fractions[count - 1u] <= MORPH_EPSILON) return;
        if (closed && u >= 1.0 - MORPH_EPSILON) return;
        fractions[count++] = u;
    };
    auto sourceTotal = from.lengths[from.count - 1u];
    auto i = 0u;
    auto j = 0u;
    while (i < sourceVertices || j < targetVertices) {
        auto own = i < sourceVertices ? from.lengths[i] / sourceTotal : 2.0;
        if (j >= targetVertices || own <= stops[j]) {
            add(own);
            i++;
        } else {
            add(stops[j++]);
        }
    }
    delete[] stops;
    if (count > MAX_MORPH_POINTS) {
        delete[] fractions;
        return Result::InsufficientCondition;
    }
    shape.from = new (std::nothrow) Vec3[count];
    shape.to = new (std::nothrow) Vec3[count];
    if (!shape.from || !shape.to) {
        delete[] fractions;
        return Result::OutOfMemory;
    }
    auto valid = true;
    for (auto k = 0u; k < count && valid; k++) {
        auto u = fractions[k];
        auto t = closed ? (reversed ? offset - u : offset + u) : (reversed ? 1.0 - u : u);
        if (closed) t -= std::floor(t);
        valid = _narrow(_morphPoint(from, u), shape.from[k]) && _narrow(_morphPoint(to, t), shape.to[k]);
    }
    delete[] fractions;
    if (!valid) return Result::InvalidArguments;
    shape.count = count;
    shape.closed = closed;
    return Result::Success;
}

// Pairs the outlines of source and target point by point. Two outlines of one kind with the
// same number of points keep the pairing their points were given in. Any others are lined up
// and then resampled at the vertices of both, so that each keeps its corners and neither
// twists.
Result pairMorph(const Object* source, const Object* target, MorphShape& shape)
{
    auto type = source->type();
    if (type == Type::SurfaceMesh) {
        // Two surfaces on one grid pair point for point.
        auto from = static_cast<const SurfaceMesh*>(source);
        auto to = static_cast<const SurfaceMesh*>(target);
        auto count = from->columns() * from->rows();
        if (count > MAX_MORPH_POINTS) return Result::InsufficientCondition;
        shape.from = new (std::nothrow) Vec3[count];
        shape.to = new (std::nothrow) Vec3[count];
        if (!shape.from || !shape.to) return Result::OutOfMemory;
        std::memcpy(shape.from, from->points(), sizeof(Vec3) * count);
        std::memcpy(shape.to, to->points(), sizeof(Vec3) * count);
        shape.count = count;
        return Result::Success;
    }
    auto sourceClosed = false;
    auto targetClosed = false;
    auto sourceCount = outline(source, nullptr, 0u, sourceClosed);
    auto targetCount = outline(target, nullptr, 0u, targetClosed);
    if (type == target->type() && sourceCount == targetCount && sourceCount >= 2u
        && (type == Type::Polygon || type == Type::Plot || type == Type::Path || type == Type::Curve)) {
        if (sourceCount > MAX_MORPH_POINTS) return Result::InsufficientCondition;
        shape.from = new (std::nothrow) Vec3[sourceCount];
        shape.to = new (std::nothrow) Vec3[sourceCount];
        if (!shape.from || !shape.to) return Result::OutOfMemory;
        outline(source, shape.from, sourceCount, sourceClosed);
        outline(target, shape.to, targetCount, targetClosed);
        shape.count = sourceCount;
        shape.closed = sourceClosed;
        return Result::Success;
    }
    MorphOutline from;
    MorphOutline to;
    auto result = _morphOutline(source, from);
    if (result == Result::Success) result = _morphOutline(target, to);
    if (result != Result::Success) return result;
    return _resampleMorph(from, source->model, to, target->model, shape);
}

struct MorphComponent
{
    MorphOutline outline;
    Vector64 center;
    Vector64 position;
    double area = 0.0;
    double measure = 0.0;
    bool used = false;
};

// Closed pieces use signed area and its centroid; open pieces use length and its midpoint
// weighted by segment length. Positions are normalized against the bounds of the whole side.
static Result _morphComponents(const Object* object, MorphComponent* pieces, uint32_t count)
{
    auto warp = warped(object);
    auto path = object->type() == Type::Path ? static_cast<const Path*>(object) : nullptr;
    auto plot = object->type() == Type::Plot ? static_cast<const Plot*>(object) : nullptr;
    Vector64 low = {INFINITY, INFINITY, INFINITY};
    Vector64 high = {-INFINITY, -INFINITY, -INFINITY};
    auto start = 0u;
    for (auto i = 0u; i < count; i++) {
        auto& part = pieces[i];
        auto& line = part.outline;
        Result result;
        if (warp || path || plot) {
            auto end = warp ? warp->runs[i].end : path ? path->contourEnd(i) : plot->contourEnd(i);
            auto points = warp ? warp->to : path ? path->points() : plot->points();
            auto closed = warp ? warp->closed : path ? path->closed() : false;
            // A contour plot keeps its closing point even though Plot has no closed flag.
            if (plot && end > start + 2u) {
                auto delta = points[end - 1u] - points[start];
                closed = delta.length() == 0.0f;
            }
            result = _morphOutline(points + start, end - start, closed, line);
            start = end;
        } else result = _morphOutline(object, line);
        if (result != Result::Success) return result;
        auto total = line.lengths[line.count - 1u];
        for (auto j = 1u; j < line.count; j++) {
            auto a = line.points[j - 1u];
            auto b = line.points[j];
            auto length = line.lengths[j] - line.lengths[j - 1u];
            part.center.x += (a.x + b.x) * 0.5 * length / total;
            part.center.y += (a.y + b.y) * 0.5 * length / total;
            part.center.z += (a.z + b.z) * 0.5 * length / total;
            if (line.closed) part.area += a.x * b.y - b.x * a.y;
        }
        if (line.closed && std::fabs(part.area) > 1.0e-12) {
            part.center.x = part.center.y = 0.0;
            for (auto j = 1u; j < line.count; j++) {
                auto a = line.points[j - 1u];
                auto b = line.points[j];
                auto cross = a.x * b.y - b.x * a.y;
                part.center.x += (a.x + b.x) * cross / (3.0 * part.area);
                part.center.y += (a.y + b.y) * cross / (3.0 * part.area);
            }
        }
        part.measure = line.closed ? std::fabs(part.area) : total;
        part.position = _point(object->model, {static_cast<float>(part.center.x),
                              static_cast<float>(part.center.y), static_cast<float>(part.center.z)});
        for (auto j = 0u; j < line.count; j++) {
            auto point = line.points[j];
            auto p = _point(object->model, {static_cast<float>(point.x), static_cast<float>(point.y),
                                          static_cast<float>(point.z)});
            low = {std::fmin(low.x, p.x), std::fmin(low.y, p.y), std::fmin(low.z, p.z)};
            high = {std::fmax(high.x, p.x), std::fmax(high.y, p.y), std::fmax(high.z, p.z)};
        }
    }
    for (auto i = 0u; i < count; i++) {
        auto& p = pieces[i].position;
        p = {high.x > low.x ? (p.x - low.x) / (high.x - low.x) : 0.5,
             high.y > low.y ? (p.y - low.y) / (high.y - low.y) : 0.5,
             high.z > low.z ? (p.z - low.z) / (high.z - low.z) : 0.5};
    }
    return Result::Success;
}

// Whether the piece at index first of the components data pairs before the one at second:
// closed pieces before open ones, and larger ones first.
static bool _pairsBefore(const void* first, const void* second, const void* data)
{
    auto& a = static_cast<const MorphComponent*>(data)[*static_cast<const uint32_t*>(first)];
    auto& b = static_cast<const MorphComponent*>(data)[*static_cast<const uint32_t*>(second)];
    return a.outline.closed != b.outline.closed ? a.outline.closed : a.measure > b.measure;
}

Result pairMorphPieces(const Animation& animation, MorphShape& shape)
{
    auto source = animation.target;
    auto target = animation.related;
    auto sourceCount = _morphPieceCount(source);
    auto targetCount = _morphPieceCount(target);
    auto capacity = std::max(sourceCount, targetCount);
    if (!capacity) return Result::InvalidArguments;
    if (capacity > MAX_ANIMATION_GROUP) return Result::InsufficientCondition;
    auto from = new (std::nothrow) MorphComponent[sourceCount];
    auto to = new (std::nothrow) MorphComponent[targetCount];
    auto parts = new (std::nothrow) MorphShape[capacity];
    auto order = new (std::nothrow) uint32_t[2u * sourceCount];
    auto growth = new (std::nothrow) int8_t[capacity];
    auto result = Result::OutOfMemory;
    auto kept = 0u;
    if (from && to && parts && order && growth) {
        result = _morphComponents(source, from, sourceCount);
        if (result == Result::Success) result = _morphComponents(target, to, targetCount);
    }
    auto pair = [&](MorphComponent* a, MorphComponent* b) {
        if (result != Result::Success) return;
        growth[kept] = !a ? 1 : !b ? -1 : 0;
        auto& part = parts[kept++];
        if (a && b) {
            auto ac = a->outline.closed;
            auto bc = b->outline.closed;
            // Closed to open pairs keep the closing segment as an ordinary segment.
            a->outline.closed = b->outline.closed = ac && bc;
            auto direction = ac && bc && a->area && b->area ? (a->area * b->area < 0.0 ? 1 : 0) : -1;
            result = _resampleMorph(a->outline, source->model, b->outline, target->model, part, direction);
            a->outline.closed = ac;
            b->outline.closed = bc;
        } else {
            auto own = a ? a : b;
            auto& line = own->outline;
            part.count = line.count - (line.closed ? 1u : 0u);
            part.closed = line.closed;
            part.from = new (std::nothrow) Vec3[part.count];
            part.to = new (std::nothrow) Vec3[part.count];
            if (!part.from || !part.to) result = Result::OutOfMemory;
            for (auto i = 0u; i < part.count && result == Result::Success; i++) {
                if (!_narrow(a ? line.points[i] : own->center, part.from[i])
                    || !_narrow(b ? line.points[i] : own->center, part.to[i])) result = Result::InvalidArguments;
            }
        }
        if (a) a->used = true;
        if (b) b->used = true;
        if (result == Result::Success) {
            if (part.count > MAX_MORPH_POINTS - shape.count) result = Result::InsufficientCondition;
            else shape.count += part.count;
        }
    };
    for (auto i = 0u; i < animation.pairCount && result == Result::Success; i++)
        pair(from + animation.pairs[2u * i], to + animation.pairs[2u * i + 1u]);
    if (result == Result::Success) {
        for (auto i = 0u; i < sourceCount; i++) order[i] = i;
        stableSort(order, order + sourceCount, sourceCount, sizeof(uint32_t), _pairsBefore, from);
        for (auto pass = 0u; pass < 2u; pass++) {
            for (auto i = 0u; i < sourceCount; i++) {
                auto& own = from[order[i]];
                if (own.used) continue;
                auto best = targetCount;
                auto distance = std::numeric_limits<double>::infinity();
                auto size = std::numeric_limits<double>::infinity();
                for (auto j = 0u; j < targetCount; j++) {
                    if (to[j].used) continue;
                    auto matching = own.outline.closed == to[j].outline.closed;
                    if (!pass && !matching) continue;
                    auto preferred = best < targetCount && own.outline.closed == to[best].outline.closed;
                    auto d = _distance(own.position, to[j].position);
                    auto m = std::fabs(own.measure - to[j].measure);
                    if (best == targetCount || (matching && !preferred)
                        || (matching == preferred && (d < distance || (d == distance && m < size)))) {
                        best = j;
                        distance = d;
                        size = m;
                    }
                }
                if (best < targetCount || pass) pair(&own, best < targetCount ? to + best : nullptr);
            }
        }
        for (auto i = 0u; i < targetCount; i++) if (!to[i].used) pair(nullptr, to + i);
    }
    if (result == Result::Success) {
        shape.from = new (std::nothrow) Vec3[shape.count];
        shape.to = new (std::nothrow) Vec3[shape.count];
        shape.ends = new (std::nothrow) uint32_t[kept];
        shape.growth = new (std::nothrow) int8_t[kept];
        if (!shape.from || !shape.to || !shape.ends || !shape.growth) result = Result::OutOfMemory;
        else {
            auto offset = 0u;
            for (auto closed = 1; closed >= 0; closed--) {
                for (auto i = 0u; i < kept; i++) {
                    auto& part = parts[i];
                    if (part.closed != static_cast<bool>(closed)) continue;
                    std::memcpy(shape.from + offset, part.from, part.count * sizeof(Vec3));
                    std::memcpy(shape.to + offset, part.to, part.count * sizeof(Vec3));
                    offset += part.count;
                    shape.growth[shape.pieces] = growth[i];
                    shape.ends[shape.pieces++] = offset;
                    if (closed) shape.closedPieces++;
                }
            }
        }
    }
    for (auto i = 0u; i < kept; i++) _freeMorph(parts[i]);
    delete[] from;
    delete[] to;
    delete[] parts;
    delete[] order;
    delete[] growth;
    return result;
}

bool validMorphPairs(const Animation& animation)
{
    if (!animation.pairs && !animation.pairCount) return true;
    if (!animation.pairs || !animation.pairCount || animation.target->type() == Type::SurfaceMesh) return false;
    auto from = _morphPieceCount(animation.target);
    auto to = _morphPieceCount(animation.related);
    if (animation.pairCount > std::min(from, to)) return false;
    for (auto i = 0u; i < animation.pairCount; i++) {
        auto a = animation.pairs[2u * i];
        auto b = animation.pairs[2u * i + 1u];
        if (a >= from || b >= to) return false;
        for (auto j = 0u; j < i; j++)
            if (a == animation.pairs[2u * j] || b == animation.pairs[2u * j + 1u]) return false;
    }
    return true;
}

}  // namespace vis
