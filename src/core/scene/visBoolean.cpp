// Path::combine(): union, intersection, difference and exclusion of path regions in double
// precision, as contours the renderer fills.
#include <cmath>
#include <new>

#include "vis.h"
#include "visScene.h"
#include "../common/visGrow.h"
#include "../common/visSort.h"

namespace vis
{

// Edges two regions may hold together in one step of a combination, and the cuts their
// crossings may make in them, two for each crossing.
static constexpr uint32_t BOOLEAN_EDGE_LIMIT = 8192u;
static constexpr uint32_t BOOLEAN_CUT_LIMIT = 16384u;

struct BooleanPoint
{
    double x = 0.0;
    double y = 0.0;
};

// Contours in the plane of the result: contour i ends just before ends[i]. A shape of one
// contour fills by the nonzero rule and one of several by the even-odd rule, as drawn.
struct BooleanRegion
{
    BooleanPoint* points = nullptr;
    uint32_t* ends = nullptr;
    uint32_t count = 0;
    uint32_t contours = 0;
    bool evenOdd = false;

    BooleanRegion() = default;
    BooleanRegion(const BooleanRegion&) = delete;
    BooleanRegion& operator=(const BooleanRegion&) = delete;

    ~BooleanRegion()
    {
        delete[] points;
        delete[] ends;
    }

    void take(BooleanRegion& other)
    {
        delete[] points;
        delete[] ends;
        points = other.points;
        ends = other.ends;
        count = other.count;
        contours = other.contours;
        evenOdd = other.evenOdd;
        other.points = nullptr;
        other.ends = nullptr;
        other.count = 0;
        other.contours = 0;
    }
};

// An edge of one of the two regions in a step, region 0 or 1.
struct BooleanEdge
{
    BooleanPoint from;
    BooleanPoint to;
    uint8_t region = 0;
};

// A point where an edge is cut, at the fraction t of its length.
struct BooleanSplit
{
    BooleanPoint point;
    double t = 0.0;
    uint32_t edge = 0;
};

// A piece of an edge between two cuts, as merged vertices; key names the pair of vertices.
struct BooleanSegment
{
    uint64_t key = 0;
    uint32_t from = 0;
    uint32_t to = 0;
    uint8_t region = 0;
};

// An affine map of space, rows of 3 linear terms and a translation.
struct BooleanAffine
{
    double m[12] = {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0};
};

static BooleanAffine _affine(const Mat4& model)
{
    BooleanAffine output;
    for (auto i = 0u; i < 12u; i++)
        output.m[i] = model.e[i];
    return output;
}

static BooleanAffine _multiply(const BooleanAffine& a, const BooleanAffine& b)
{
    BooleanAffine output;
    for (auto row = 0u; row < 3u; row++) {
        for (auto column = 0u; column < 4u; column++) {
            auto value = column == 3u ? a.m[row * 4u + 3u] : 0.0;
            for (auto k = 0u; k < 3u; k++)
                value += a.m[row * 4u + k] * b.m[k * 4u + column];
            output.m[row * 4u + column] = value;
        }
    }
    return output;
}

static bool _invert(const BooleanAffine& value, BooleanAffine& output)
{
    auto& m = value.m;
    auto a = m[0], b = m[1], c = m[2];
    auto d = m[4], e = m[5], f = m[6];
    auto g = m[8], h = m[9], i = m[10];
    auto determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-300) return false;
    auto inverse = 1.0 / determinant;
    double linear[9] = {(e * i - f * h) * inverse, (c * h - b * i) * inverse, (b * f - c * e) * inverse,
                        (f * g - d * i) * inverse, (a * i - c * g) * inverse, (c * d - a * f) * inverse,
                        (d * h - e * g) * inverse, (b * g - a * h) * inverse, (a * e - b * d) * inverse};
    for (auto row = 0u; row < 3u; row++) {
        auto moved = 0.0;
        for (auto k = 0u; k < 3u; k++) {
            output.m[row * 4u + k] = linear[row * 3u + k];
            moved += linear[row * 3u + k] * m[k * 4u + 3u];
        }
        output.m[row * 4u + 3u] = -moved;
    }
    return true;
}

// The map from the local space of object to the world.
static BooleanAffine _world(const Object* object)
{
    BooleanAffine output;
    for (auto at = object; at; at = at->parent())
        output = _multiply(_affine(at->model), output);
    return output;
}

static BooleanPoint _apply(const BooleanAffine& map, const Vec3& point)
{
    auto& m = map.m;
    return {m[0] * point.x + m[1] * point.y + m[2] * point.z + m[3],
            m[4] * point.x + m[5] * point.y + m[6] * point.z + m[7]};
}

static double _cross(const BooleanPoint& a, const BooleanPoint& b)
{
    return a.x * b.y - a.y * b.x;
}

static BooleanPoint _minus(const BooleanPoint& a, const BooleanPoint& b)
{
    return {a.x - b.x, a.y - b.y};
}

static double _length(const BooleanPoint& a)
{
    return std::hypot(a.x, a.y);
}

static bool _near(const BooleanPoint& a, const BooleanPoint& b, double tolerance)
{
    return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance;
}

// The orders _combine() sorts by: cuts along each edge in turn, points (as their indices into
// positions) from left to right, and pieces by the pair of vertices they join.
static bool _splitBefore(const void* first, const void* second, const void*)
{
    auto& a = *static_cast<const BooleanSplit*>(first);
    auto& b = *static_cast<const BooleanSplit*>(second);
    return a.edge != b.edge ? a.edge < b.edge : a.t < b.t;
}

static bool _leftOf(const void* first, const void* second, const void* positions)
{
    auto points = static_cast<const BooleanPoint*>(positions);
    return points[*static_cast<const uint32_t*>(first)].x < points[*static_cast<const uint32_t*>(second)].x;
}

static bool _segmentBefore(const void* first, const void* second, const void*)
{
    return static_cast<const BooleanSegment*>(first)->key < static_cast<const BooleanSegment*>(second)->key;
}

// The contours of a closed shape mapped into the plane of the result.
static Result _region(const Object* object, const BooleanAffine& map, BooleanRegion& region)
{
    if (!valid(object)) return Result::InvalidArguments;
    auto type = object->type();
    if (type != Type::Circle && type != Type::Rectangle && type != Type::Polygon && type != Type::Path) {
        return Result::InvalidArguments;
    }
    const Vec3* source = nullptr;
    Vec3* outlined = nullptr;
    auto count = 0u;
    auto contours = 1u;
    if (type == Type::Path) {
        auto path = static_cast<const Path*>(object);
        if (!path->closed()) return Result::InvalidArguments;
        source = path->points();
        count = path->count();
        contours = path->contours();
    } else {
        auto closed = false;
        count = outline(object, nullptr, 0u, closed);
        if (count < 3u || !closed) return Result::InvalidArguments;
        outlined = new (std::nothrow) Vec3[count];
        if (!outlined) return Result::OutOfMemory;
        outline(object, outlined, count, closed);
        source = outlined;
    }
    region.points = new (std::nothrow) BooleanPoint[count];
    region.ends = new (std::nothrow) uint32_t[contours];
    if (!region.points || !region.ends) {
        delete[] outlined;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < count; i++)
        region.points[i] = _apply(map, source[i]);
    if (type == Type::Path) {
        auto path = static_cast<const Path*>(object);
        for (auto i = 0u; i < contours; i++)
            region.ends[i] = path->contourEnd(i);
    } else {
        region.ends[0] = count;
    }
    delete[] outlined;
    region.count = count;
    region.contours = contours;
    region.evenOdd = contours > 1u;
    for (auto i = 0u; i < count; i++) {
        if (!std::isfinite(region.points[i].x) || !std::isfinite(region.points[i].y)) return Result::InvalidArguments;
    }
    return Result::Success;
}

// Appends the edges of region, leaving out repeated points and contours that enclose nothing.
static void _edges(const BooleanRegion& region, uint8_t index, double tolerance, BooleanEdge* edges,
                   uint32_t& count)
{
    auto start = 0u;
    for (auto contour = 0u; contour < region.contours; contour++) {
        auto end = region.ends[contour];
        auto first = count;
        auto previous = start;
        auto kept = 0u;
        for (auto i = start; i < end; i++) {
            if (kept && _near(region.points[i], region.points[previous], tolerance)) continue;
            if (kept) edges[count++] = {region.points[previous], region.points[i], index};
            previous = i;
            kept++;
        }
        if (kept >= 3u && !_near(region.points[previous], region.points[start], tolerance)) {
            edges[count++] = {region.points[previous], region.points[start], index};
        } else if (kept >= 3u && count > first) {
            // The last point repeats the first: the last edge ends there instead.
            edges[count - 1u].to = region.points[start];
        }
        if (count - first < 3u) count = first;
        start = end;
    }
}

// Records where two edges cut each other: inside both, or where an end of one lies inside the
// other, at that end exactly. Overlapping edges cut each other at the ends of both.
static bool _cut(const BooleanEdge* edges, uint32_t first, uint32_t second, double tolerance,
                 BooleanSplit*& splits, uint32_t& count, uint32_t& capacity)
{
    auto& e = edges[first];
    auto& f = edges[second];
    auto d1 = _minus(e.to, e.from);
    auto d2 = _minus(f.to, f.from);
    auto length1 = _length(d1);
    auto length2 = _length(d2);
    if (!(length1 > 0.0) || !(length2 > 0.0)) return true;
    auto tolerance1 = tolerance / length1;
    auto tolerance2 = tolerance / length2;
    auto add = [&](uint32_t edge, double t, const BooleanPoint& point) {
        if (!grow(splits, count, capacity, 1u, 64u)) return false;
        splits[count++] = {point, t, edge};
        return true;
    };
    auto offset = _minus(f.from, e.from);
    auto denominator = _cross(d1, d2);
    if (std::fabs(denominator) > 1.0e-12 * length1 * length2) {
        auto t = _cross(offset, d2) / denominator;
        auto u = _cross(offset, d1) / denominator;
        if (t < -tolerance1 || t > 1.0 + tolerance1 || u < -tolerance2 || u > 1.0 + tolerance2) return true;
        auto firstStart = t <= tolerance1;
        auto firstEnd = t >= 1.0 - tolerance1;
        auto secondStart = u <= tolerance2;
        auto secondEnd = u >= 1.0 - tolerance2;
        BooleanPoint point = {e.from.x + d1.x * t, e.from.y + d1.y * t};
        if (firstStart) point = e.from;
        else if (firstEnd) point = e.to;
        else if (secondStart) point = f.from;
        else if (secondEnd) point = f.to;
        if (!firstStart && !firstEnd && !add(first, t, point)) return false;
        if (!secondStart && !secondEnd && !add(second, u, point)) return false;
        return true;
    }
    if (std::fabs(_cross(d1, offset)) > tolerance * length1) return true;
    const BooleanPoint secondEnds[2] = {f.from, f.to};
    for (auto& point : secondEnds) {
        auto t = ((point.x - e.from.x) * d1.x + (point.y - e.from.y) * d1.y) / (length1 * length1);
        if (t > tolerance1 && t < 1.0 - tolerance1 && !add(first, t, point)) return false;
    }
    const BooleanPoint firstEnds[2] = {e.from, e.to};
    for (auto& point : firstEnds) {
        auto u = ((point.x - f.from.x) * d2.x + (point.y - f.from.y) * d2.y) / (length2 * length2);
        if (u > tolerance2 && u < 1.0 - tolerance2 && !add(second, u, point)) return false;
    }
    return true;
}

static uint32_t _root(uint32_t* parents, uint32_t index)
{
    while (parents[index] != index) {
        parents[index] = parents[parents[index]];
        index = parents[index];
    }
    return index;
}

// The winding of the segment about point for a ray towards +x, counted as just above point.
static int _winding(const BooleanPoint& from, const BooleanPoint& to, const BooleanPoint& point)
{
    auto side = _cross(_minus(to, from), _minus(point, from));
    if (from.y <= point.y) return to.y > point.y && side > 0.0 ? 1 : 0;
    return to.y <= point.y && side < 0.0 ? -1 : 0;
}

static bool _inside(int winding, bool evenOdd)
{
    return evenOdd ? (winding & 1) != 0 : winding != 0;
}

static bool _apply(PathOperation operation, bool first, bool second)
{
    switch (operation) {
        case PathOperation::Union: return first || second;
        case PathOperation::Intersection: return first && second;
        case PathOperation::Difference: return first && !second;
        case PathOperation::Exclusion: return first != second;
    }
    return false;
}

// Combines two regions into the contours of the result: every edge is cut where it meets
// another, the pieces that part the result from the rest are kept, turned so that the result
// lies on their left, and linked into contours, outer ones counterclockwise and holes
// clockwise.
static Result _combine(const BooleanRegion& first, const BooleanRegion& second, PathOperation operation,
                       double tolerance, BooleanRegion& output)
{
    auto capacity = first.count + second.count;
    if (capacity > BOOLEAN_EDGE_LIMIT) return Result::NonSupport;
    auto edges = new (std::nothrow) BooleanEdge[capacity ? capacity : 1u];
    if (!edges) return Result::OutOfMemory;
    auto edgeCount = 0u;
    _edges(first, 0u, tolerance, edges, edgeCount);
    _edges(second, 1u, tolerance, edges, edgeCount);
    output.evenOdd = true;

    // Where the edges cut one another.
    BooleanSplit* splits = nullptr;
    auto splitCount = 0u;
    auto splitCapacity = 0u;
    auto result = Result::Success;
    for (auto i = 0u; i < edgeCount && result == Result::Success; i++) {
        auto& e = edges[i];
        auto minX = std::fmin(e.from.x, e.to.x) - tolerance;
        auto maxX = std::fmax(e.from.x, e.to.x) + tolerance;
        auto minY = std::fmin(e.from.y, e.to.y) - tolerance;
        auto maxY = std::fmax(e.from.y, e.to.y) + tolerance;
        for (auto j = i + 1u; j < edgeCount; j++) {
            auto& f = edges[j];
            if (std::fmax(f.from.x, f.to.x) < minX || std::fmin(f.from.x, f.to.x) > maxX
                || std::fmax(f.from.y, f.to.y) < minY || std::fmin(f.from.y, f.to.y) > maxY) {
                continue;
            }
            if (!_cut(edges, i, j, tolerance, splits, splitCount, splitCapacity)) {
                result = Result::OutOfMemory;
                break;
            }
            if (splitCount > BOOLEAN_CUT_LIMIT) {
                result = Result::NonSupport;
                break;
            }
        }
    }
    auto sorted = result == Result::Success ? new (std::nothrow) BooleanSplit[splitCount + 1u] : nullptr;
    if (result == Result::Success && !sorted) result = Result::OutOfMemory;
    if (sorted) stableSort(splits, sorted, splitCount, sizeof(BooleanSplit), _splitBefore);
    delete[] sorted;

    // The pieces between the cuts, with the points at both ends.
    auto pieceCapacity = edgeCount + splitCount;
    auto positions = result == Result::Success ? new (std::nothrow) BooleanPoint[pieceCapacity * 2u + 1u] : nullptr;
    auto regions = result == Result::Success ? new (std::nothrow) uint8_t[pieceCapacity + 1u] : nullptr;
    if (result == Result::Success && (!positions || !regions)) result = Result::OutOfMemory;
    auto pieceCount = 0u;
    if (result == Result::Success) {
        auto split = 0u;
        for (auto i = 0u; i < edgeCount; i++) {
            auto& e = edges[i];
            auto last = e.from;
            auto add = [&](const BooleanPoint& point) {
                positions[pieceCount * 2u] = last;
                positions[pieceCount * 2u + 1u] = point;
                regions[pieceCount++] = e.region;
                last = point;
            };
            for (; split < splitCount && splits[split].edge == i; split++) {
                auto& point = splits[split].point;
                if (_near(point, last, tolerance) || _near(point, e.to, tolerance)) continue;
                add(point);
            }
            add(e.to);
        }
    }
    delete[] splits;
    delete[] edges;

    // Points closer than the tolerance are one vertex.
    auto pointCount = pieceCount * 2u;
    auto order = result == Result::Success ? new (std::nothrow) uint32_t[pointCount * 2u + 1u] : nullptr;
    auto parents = result == Result::Success ? new (std::nothrow) uint32_t[pointCount + 1u] : nullptr;
    if (result == Result::Success && (!order || !parents)) result = Result::OutOfMemory;
    if (result == Result::Success) {
        for (auto i = 0u; i < pointCount; i++) {
            order[i] = i;
            parents[i] = i;
        }
        stableSort(order, order + pointCount, pointCount, sizeof(uint32_t), _leftOf, positions);
    }
    if (result == Result::Success) {
        for (auto i = 0u; i < pointCount; i++) {
            auto& a = positions[order[i]];
            for (auto j = i + 1u; j < pointCount && positions[order[j]].x - a.x <= tolerance; j++) {
                if (std::fabs(positions[order[j]].y - a.y) > tolerance) continue;
                auto one = _root(parents, order[i]);
                auto other = _root(parents, order[j]);
                if (one < other) parents[other] = one;
                else if (other < one) parents[one] = other;
            }
        }
    }
    delete[] order;

    // Each piece by its two vertices; pieces on the same two vertices lie on top of each other.
    auto segments = result == Result::Success ? new (std::nothrow) BooleanSegment[pieceCount * 2u + 1u] : nullptr;
    if (result == Result::Success && !segments) result = Result::OutOfMemory;
    auto segmentCount = 0u;
    if (result == Result::Success) {
        for (auto i = 0u; i < pieceCount; i++) {
            auto from = _root(parents, i * 2u);
            auto to = _root(parents, i * 2u + 1u);
            if (from == to) continue;
            auto low = from < to ? from : to;
            auto high = from < to ? to : from;
            segments[segmentCount++] = {(static_cast<uint64_t>(low) << 32u) | high, from, to, regions[i]};
        }
        stableSort(segments, segments + segmentCount, segmentCount, sizeof(BooleanSegment), _segmentBefore);
    }
    delete[] regions;
    delete[] parents;

    // Keep each group of pieces that parts the result from the rest, the result on its left.
    auto kept = result == Result::Success ? new (std::nothrow) uint32_t[segmentCount * 2u + 1u] : nullptr;
    if (result == Result::Success && !kept) result = Result::OutOfMemory;
    auto keptCount = 0u;
    for (auto group = 0u; result == Result::Success && group < segmentCount;) {
        auto next = group;
        while (next < segmentCount && segments[next].key == segments[group].key) next++;
        auto low = static_cast<uint32_t>(segments[group].key >> 32u);
        auto high = static_cast<uint32_t>(segments[group].key & 0xffffffffu);
        int change[2] = {0, 0};
        for (auto i = group; i < next; i++)
            change[segments[i].region] += segments[i].from == low ? 1 : -1;
        auto& a = positions[low];
        auto& b = positions[high];
        BooleanPoint middle = {(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
        int winding[2] = {0, 0};
        for (auto i = 0u; i < segmentCount; i++) {
            if (i >= group && i < next) continue;
            winding[segments[i].region] += _winding(positions[segments[i].from], positions[segments[i].to], middle);
        }
        // Without the group's own pieces, the count is the winding on the side the ray starts
        // from: the right of low to high when that runs up, or level towards -x, and the left
        // otherwise. Across the group the winding changes by its pieces' net direction.
        bool left[2];
        bool right[2];
        auto dy = b.y - a.y;
        auto dx = b.x - a.x;
        for (auto k = 0u; k < 2u; k++) {
            int windingLeft;
            int windingRight;
            if (dy > 0.0 || (dy == 0.0 && dx < 0.0)) {
                windingRight = winding[k];
                windingLeft = winding[k] + change[k];
            } else {
                windingLeft = winding[k];
                windingRight = winding[k] - change[k];
            }
            auto evenOdd = k ? second.evenOdd : first.evenOdd;
            left[k] = _inside(windingLeft, evenOdd);
            right[k] = _inside(windingRight, evenOdd);
        }
        auto resultLeft = _apply(operation, left[0], left[1]);
        auto resultRight = _apply(operation, right[0], right[1]);
        if (resultLeft != resultRight) {
            kept[keptCount * 2u] = resultLeft ? low : high;
            kept[keptCount * 2u + 1u] = resultLeft ? high : low;
            keptCount++;
        }
        group = next;
    }
    delete[] segments;

    // Link the kept pieces into contours. Where several leave one vertex, the contour turns
    // to the first one clockwise from the way it came, so each contour bounds one face.
    auto vertexCount = pointCount;
    auto starts = result == Result::Success ? new (std::nothrow) uint32_t[vertexCount + 2u] : nullptr;
    auto outgoing = result == Result::Success ? new (std::nothrow) uint32_t[keptCount + 1u] : nullptr;
    auto used = result == Result::Success ? new (std::nothrow) bool[keptCount + 1u] : nullptr;
    auto points = result == Result::Success ? new (std::nothrow) BooleanPoint[keptCount + 1u] : nullptr;
    auto contourEnds = result == Result::Success ? new (std::nothrow) uint32_t[keptCount + 1u] : nullptr;
    if (result == Result::Success && (!starts || !outgoing || !used || !points || !contourEnds)) {
        result = Result::OutOfMemory;
    }
    auto count = 0u;
    auto contours = 0u;
    if (result == Result::Success) {
        for (auto i = 0u; i <= vertexCount + 1u; i++)
            starts[i] = 0u;
        for (auto i = 0u; i < keptCount; i++)
            starts[kept[i * 2u] + 2u]++;
        for (auto i = 2u; i <= vertexCount + 1u; i++)
            starts[i] += starts[i - 1u];
        for (auto i = 0u; i < keptCount; i++) {
            outgoing[starts[kept[i * 2u] + 1u]++] = i;
            used[i] = false;
        }
        for (auto first = 0u; first < keptCount; first++) {
            if (used[first]) continue;
            auto origin = kept[first * 2u];
            auto contourStart = count;
            auto current = first;
            while (true) {
                used[current] = true;
                points[count++] = positions[kept[current * 2u]];
                auto vertex = kept[current * 2u + 1u];
                if (vertex == origin) break;
                auto back = _minus(positions[kept[current * 2u]], positions[vertex]);
                auto backAngle = std::atan2(back.y, back.x);
                auto best = UINT32_MAX;
                auto bestTurn = 0.0;
                for (auto k = starts[vertex]; k < starts[vertex + 1u]; k++) {
                    auto candidate = outgoing[k];
                    if (used[candidate]) continue;
                    auto way = _minus(positions[kept[candidate * 2u + 1u]], positions[vertex]);
                    auto turn = backAngle - std::atan2(way.y, way.x);
                    while (turn <= 0.0) turn += 6.283185307179586;
                    while (turn > 6.283185307179586) turn -= 6.283185307179586;
                    if (best == UINT32_MAX || turn < bestTurn) {
                        best = candidate;
                        bestTurn = turn;
                    }
                }
                if (best == UINT32_MAX) break;
                current = best;
            }
            // Points where the contour runs straight on add nothing; each is judged against its
            // neighbors as traced.
            auto size = count - contourStart;
            if (size > 3u) {
                auto opening = points[contourStart];
                auto previous = points[count - 1u];
                auto write = contourStart;
                for (auto i = 0u; i < size; i++) {
                    auto point = points[contourStart + i];
                    auto following = i + 1u < size ? points[contourStart + i + 1u] : opening;
                    auto in = _minus(point, previous);
                    auto out = _minus(following, point);
                    auto straight = std::fabs(_cross(in, out)) <= tolerance * _length(_minus(following, previous))
                                    && in.x * out.x + in.y * out.y > 0.0;
                    if (!straight) points[write++] = point;
                    previous = point;
                }
                count = write;
            }
            // A contour no wider than the tolerance is left over from edges that nearly meet.
            auto area = 0.0;
            auto perimeter = 0.0;
            for (auto i = contourStart; i < count; i++) {
                auto& point = points[i];
                auto& following = points[i + 1u < count ? i + 1u : contourStart];
                area += _cross(point, following);
                perimeter += _length(_minus(following, point));
            }
            if (count - contourStart < 3u || std::fabs(area) * 0.5 <= tolerance * perimeter) {
                count = contourStart;
                continue;
            }
            contourEnds[contours++] = count;
        }
    }
    delete[] kept;
    delete[] starts;
    delete[] outgoing;
    delete[] used;
    delete[] positions;
    if (result != Result::Success) {
        delete[] points;
        delete[] contourEnds;
        return result;
    }
    delete[] output.points;
    delete[] output.ends;
    output.points = points;
    output.ends = contourEnds;
    output.count = count;
    output.contours = contours;
    return Result::Success;
}

Result Path::combine(const Object* const* shapes, uint32_t count, PathOperation operation,
                     const Object* parent, Path*& output) noexcept
{
    output = nullptr;
    if (!shapes || count < 2u || static_cast<uint8_t>(operation) > static_cast<uint8_t>(PathOperation::Exclusion)) {
        return Result::InvalidArguments;
    }
    BooleanAffine space;
    if (parent && !_invert(_world(parent), space)) return Result::InvalidArguments;
    auto regions = new (std::nothrow) BooleanRegion[count];
    if (!regions) return Result::OutOfMemory;
    auto result = Result::Success;
    auto extent = 0.0;
    for (auto i = 0u; i < count && result == Result::Success; i++) {
        if (!shapes[i]) {
            result = Result::InvalidArguments;
            break;
        }
        result = _region(shapes[i], _multiply(space, _world(shapes[i])), regions[i]);
        for (auto j = 0u; result == Result::Success && j < regions[i].count; j++) {
            extent = std::fmax(extent, std::fmax(std::fabs(regions[i].points[j].x), std::fabs(regions[i].points[j].y)));
        }
    }
    if (result == Result::Success && !(extent > 0.0)) result = Result::InvalidArguments;
    auto tolerance = extent * 1.0e-9;
    BooleanRegion combined;
    if (result == Result::Success) combined.take(regions[0]);
    for (auto i = 1u; i < count && result == Result::Success; i++) {
        BooleanRegion next;
        result = _combine(combined, regions[i], operation, tolerance, next);
        if (result == Result::Success) combined.take(next);
    }
    delete[] regions;
    if (result != Result::Success) return result;
    if (!combined.contours) return Result::InsufficientCondition;
    auto points = new (std::nothrow) Vec3[combined.count];
    auto ends = combined.contours > 1u ? new (std::nothrow) uint32_t[combined.contours] : nullptr;
    if (!points || (combined.contours > 1u && !ends)) {
        delete[] points;
        delete[] ends;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < combined.count; i++)
        points[i] = {static_cast<float>(combined.points[i].x), static_cast<float>(combined.points[i].y), 0.0f};
    for (auto i = 0u; ends && i < combined.contours; i++)
        ends[i] = combined.ends[i];
    output = new (std::nothrow) Path(points, combined.count, true, ends, combined.contours);
    if (output) return Result::Success;
    delete[] points;
    delete[] ends;
    return Result::OutOfMemory;
}

}  // namespace vis
