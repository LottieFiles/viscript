// WarpDraft: gathers the points a warp will move, a space's grid, ticks and numbers among them,
// and maps them.
#include <cmath>

#include "visSceneValidation.h"
#include "visWarp.h"

namespace vis
{

// The fewest points a warp samples a circle and a rounded corner with, as many as the renderer
// draws them with.
static constexpr uint32_t WARP_CIRCLE_POINTS = 64;
static constexpr uint32_t WARP_CORNER_POINTS = 8;
// The most numbers a space writes, as the renderer counts them.
static constexpr uint32_t WARP_SPACE_NUMBERS = 256;

// Calls tick with each value along range where a space draws a grid line or writes a number,
// stepping as the renderer does, until tick returns false.
template<typename Tick>
static void _ticks(const Range& range, Tick tick)
{
    auto index = 0u;
    for (auto value = std::ceil(range.min / range.step) * range.step;
         value <= range.max + range.step * 0.001f && index++ < 4096; value += range.step) {
        if (std::fabs(value) < range.step * 0.001f) continue;
        if (!tick(value)) return;
    }
}

// The geometry of a space as runs: its grid lines, those of the 3D view, its axes, its ticks, each
// as its point and one a little along its axis, which tell where the tick stands and which way
// its axis runs there once warped, and the points where it writes numbers, in the order the
// renderer draws them.
static void _gatherSpace(const Space* space, WarpDraft& draft)
{
    auto line = [&draft](const Vec3& from, const Vec3& to, WarpPart part) {
        draft.add(from);
        draft.add(to);
        draft.end(part);
    };
    auto& x = space->x;
    auto& y = space->y;
    auto& z = space->z;
    _ticks(x, [&](float value) { line({value, y.min, 0.0f}, {value, y.max, 0.0f}, WarpPart::Grid); return true; });
    _ticks(y, [&](float value) { line({x.min, value, 0.0f}, {x.max, value, 0.0f}, WarpPart::Grid); return true; });
    _ticks(x, [&](float value) { line({value, 0.0f, z.min}, {value, 0.0f, z.max}, WarpPart::DepthGrid); return true; });
    _ticks(z, [&](float value) { line({x.min, 0.0f, value}, {x.max, 0.0f, value}, WarpPart::DepthGrid); return true; });
    _ticks(y, [&](float value) { line({0.0f, value, z.min}, {0.0f, value, z.max}, WarpPart::DepthGrid); return true; });
    _ticks(z, [&](float value) { line({0.0f, y.min, value}, {0.0f, y.max, value}, WarpPart::DepthGrid); return true; });
    line({x.min, 0.0f, 0.0f}, {x.max, 0.0f, 0.0f}, WarpPart::AxisX);
    line({0.0f, y.min, 0.0f}, {0.0f, y.max, 0.0f}, WarpPart::AxisY);
    line({0.0f, 0.0f, z.min}, {0.0f, 0.0f, z.max}, WarpPart::AxisZ);
    auto ticks = [&](const Range& range, float step, uint8_t axis, WarpPart part) {
        spaceTicks(range, step, [&](float value) {
            auto ahead = value + spaceTickLead(value, step);
            line(axis == 0u ? Vec3{value, 0.0f, 0.0f} : axis == 1u ? Vec3{0.0f, value, 0.0f} : Vec3{0.0f, 0.0f, value},
                 axis == 0u ? Vec3{ahead, 0.0f, 0.0f} : axis == 1u ? Vec3{0.0f, ahead, 0.0f} : Vec3{0.0f, 0.0f, ahead},
                 part);
            return true;
        });
    };
    ticks(x, space->ticks.x, 0u, WarpPart::TickX);
    ticks(y, space->ticks.y, 1u, WarpPart::TickY);
    ticks(z, space->ticks.z, 2u, WarpPart::TickZ);
    if (!space->numbers) return;
    auto written = 0u;
    auto numbers = [&](const Range& range, uint8_t axis, WarpPart part) {
        _ticks(range, [&](float value) {
            if (written >= WARP_SPACE_NUMBERS) return false;
            auto point = Vec3{};
            if (axis == 0u) point.x = value;
            else if (axis == 1u) point.y = value;
            else point.z = value;
            draft.add(point);
            draft.end(part, value);
            written++;
            return true;
        });
    };
    numbers(x, 0u, WarpPart::NumberX);
    numbers(y, 1u, WarpPart::NumberY);
    numbers(z, 2u, WarpPart::NumberZ);
}

// The geometry of object in its local space, as runs that a warp cuts and maps: outlines and
// contours, a circle and rounded corners sampled finely enough that no piece of them is longer
// than step once space maps it, or the one point a warp moves an object by.
void WarpDraft::gather(const Object* object, const Mat4& space, float step)
{
    auto& draft = *this;
    constexpr auto PI2 = 6.28318530717958647692;
    auto samples = [step](double length, uint32_t least) {
        auto count = std::ceil(length / static_cast<double>(step));
        if (!(count < static_cast<double>(PointLimit))) return PointLimit;
        return count > static_cast<double>(least) ? static_cast<uint32_t>(count) : least;
    };
    auto mappedLength = [&space](const Vec3* points, uint32_t count) {
        auto length = 0.0;
        for (auto i = 1u; i < count; i++) {
            auto a = space * points[i - 1u];
            auto b = space * points[i];
            length += std::hypot(static_cast<double>(b.x) - a.x, static_cast<double>(b.y) - a.y,
                                 static_cast<double>(b.z) - a.z);
        }
        return length;
    };
    auto points = [&draft](const Vec3* source, uint32_t count) {
        for (auto i = 0u; i < count; i++)
            draft.add(source[i]);
    };
    Vec3 point;
    if (anchor(object, point)) {
        draft.add(point);
        draft.end(WarpPart::Outline);
        return;
    }
    switch (object->type()) {
        case Type::Line:
        case Type::Arrow:
        case Type::Vector: {
            Vec3 ends[2];
            if (segment(object, nullptr, ends)) {
                draft.add(ends[0]);
                draft.add(ends[1]);
            }
            break;
        }
        case Type::Route: {
            auto route = static_cast<const DirectedRoute*>(object);
            points(route->points(), route->count());
            break;
        }
        case Type::Polygon: {
            auto polygon = static_cast<const Polygon*>(object);
            points(polygon->points(), polygon->count());
            draft.closed = true;
            break;
        }
        case Type::Plot: {
            // A run for each piece, as for each contour of a path.
            auto plot = static_cast<const Plot*>(object);
            auto start = 0u;
            for (auto piece = 0u; piece + 1u < plot->contours(); piece++) {
                auto finish = plot->contourEnd(piece);
                points(plot->points() + start, finish - start);
                draft.end(WarpPart::Outline);
                start = finish;
            }
            points(plot->points() + start, plot->count() - start);
            break;
        }
        case Type::Curve: {
            auto curve = static_cast<const Curve*>(object);
            points(curve->points(), curve->count());
            break;
        }
        case Type::Path: {
            auto path = static_cast<const Path*>(object);
            auto start = 0u;
            for (auto contour = 0u; contour < path->contours(); contour++) {
                auto finish = path->contourEnd(contour);
                for (auto i = start; i < finish; i++)
                    draft.add(path->points()[i]);
                draft.end(WarpPart::Outline);
                start = finish;
            }
            draft.closed = path->closed();
            break;
        }
        case Type::Arc:
        case Type::Angle: {
            Vec3 outlinePoints[514];
            auto arc = arcGeometry(object);
            auto count = arcOutline(arc, outlinePoints, 514u);
            points(outlinePoints, count); draft.closed = arc.sector;
            break;
        }
        case Type::Circle: {
            // The circle itself, not the polygon the renderer draws, so that no corner shows once
            // a map stretches it.
            auto circle = static_cast<const Circle*>(object);
            auto ring = [circle](double turn) {
                return circle->center + Vec3{static_cast<float>(std::cos(turn * PI2)) * circle->radius,
                                             static_cast<float>(std::sin(turn * PI2)) * circle->radius, 0.0f};
            };
            Vec3 coarse[WARP_CIRCLE_POINTS + 1u];
            for (auto i = 0u; i <= WARP_CIRCLE_POINTS; i++)
                coarse[i] = ring(static_cast<double>(i) / WARP_CIRCLE_POINTS);
            auto count = samples(mappedLength(coarse, WARP_CIRCLE_POINTS + 1u), WARP_CIRCLE_POINTS);
            for (auto i = 0u; i < count && !draft.full && !draft.failed; i++)
                draft.add(ring(static_cast<double>(i) / count));
            draft.closed = true;
            break;
        }
        case Type::Rectangle: {
            auto rectangle = static_cast<const Rectangle*>(object);
            auto half = Vec2{rectangle->size.x * 0.5f, rectangle->size.y * 0.5f};
            auto radius = std::fmin(rectangle->corner, std::fmin(half.x, half.y));
            auto& center = rectangle->center;
            draft.closed = true;
            if (radius <= 0.0f) {
                draft.add(center + Vec3{-half.x, -half.y, 0.0f});
                draft.add(center + Vec3{half.x, -half.y, 0.0f});
                draft.add(center + Vec3{half.x, half.y, 0.0f});
                draft.add(center + Vec3{-half.x, half.y, 0.0f});
                break;
            }
            // Each corner from where its side ends to where the next side begins, counterclockwise
            // from the right side, as the renderer goes round.
            Vec2 corners[4] = {
                {half.x - radius, half.y - radius},
                {-half.x + radius, half.y - radius},
                {-half.x + radius, -half.y + radius},
                {half.x - radius, -half.y + radius}};
            for (auto corner = 0u; corner < 4u; corner++) {
                auto arc = [&](double share) {
                    auto turn = (corner + share) * 0.25;
                    return center + Vec3{corners[corner].x + static_cast<float>(std::cos(turn * PI2)) * radius,
                                         corners[corner].y + static_cast<float>(std::sin(turn * PI2)) * radius, 0.0f};
                };
                Vec3 coarse[WARP_CORNER_POINTS + 1u];
                for (auto i = 0u; i <= WARP_CORNER_POINTS; i++)
                    coarse[i] = arc(static_cast<double>(i) / WARP_CORNER_POINTS);
                auto count = samples(mappedLength(coarse, WARP_CORNER_POINTS + 1u), WARP_CORNER_POINTS);
                for (auto i = 0u; i <= count && !draft.full && !draft.failed; i++)
                    draft.add(arc(static_cast<double>(i) / count));
            }
            break;
        }
        case Type::Space: _gatherSpace(static_cast<const Space*>(object), draft); return;
        default: return;
    }
    draft.end(WarpPart::Outline);
}

// Cuts the straight pieces of draft into pieces at most step long once space maps them into the
// parent space of the warp's target, then sends every point there through map and its image back:
// to = inverse * map(space * from). Budget is how many points the scene may still keep. Where map
// gives no finite image, writes the point it was given into missing.
Result WarpDraft::map(const Mat4& space, const Mat4& inverse, PointMap callback, void* data,
                      float step, uint32_t budget, WarpShape*& output, Vec3& missing) const
{
    auto& draft = *this;
    auto same = [](const Vec3& first, const Vec3& second) {
        return first.x == second.x && first.y == second.y && first.z == second.z;
    };
    output = nullptr;
    auto points = draft.points;
    auto pieces = [&](uint32_t from, uint32_t to) {
        auto a = space * points[from];
        auto b = space * points[to];
        auto length = std::hypot(static_cast<double>(b.x) - a.x, static_cast<double>(b.y) - a.y,
                                 static_cast<double>(b.z) - a.z);
        auto count = std::ceil(length / static_cast<double>(step));
        return count > 1.0 ? count : 1.0;
    };
    // A run is closed when its object is and it is an outline of three or more points.
    auto closed = [&](uint32_t run, uint32_t begin) {
        return draft.closed && draft.runs[run].part == WarpPart::Outline && draft.runs[run].end - begin > 2u;
    };
    auto total = 0.0;
    auto begin = 0u;
    for (auto run = 0u; run < draft.runCount; run++) {
        auto end = draft.runs[run].end;
        auto last = begin;
        total += 1.0;
        for (auto i = begin + 1u; i < end && total <= budget; i++) {
            if (same(points[i], points[last])) continue;
            total += pieces(last, i);
            last = i;
        }
        if (closed(run, begin) && !same(points[last], points[begin])) total += pieces(last, begin) - 1.0;
        if (total > budget) return Result::InsufficientCondition;
        begin = end;
    }
    auto count = static_cast<uint32_t>(total);
    auto shape = new (std::nothrow) WarpShape;
    if (!shape) return Result::OutOfMemory;
    shape->from = new (std::nothrow) Vec3[count];
    shape->to = new (std::nothrow) Vec3[count];
    shape->runs = new (std::nothrow) WarpRun[draft.runCount];
    if (!shape->from || !shape->to || !shape->runs) {
        delete shape;
        return Result::OutOfMemory;
    }
    shape->runCount = draft.runCount;
    shape->closed = draft.closed;
    auto from = shape->from;
    auto written = 0u;
    auto cut = [&](uint32_t a, uint32_t b) {
        auto n = static_cast<uint32_t>(pieces(a, b));
        for (auto k = 1u; k < n; k++)
            from[written++] = points[a] + (points[b] - points[a]) * (static_cast<float>(k) / static_cast<float>(n));
    };
    begin = 0u;
    for (auto run = 0u; run < draft.runCount; run++) {
        auto end = draft.runs[run].end;
        auto last = begin;
        from[written++] = points[begin];
        for (auto i = begin + 1u; i < end; i++) {
            if (same(points[i], points[last])) continue;
            cut(last, i);
            from[written++] = points[i];
            last = i;
        }
        if (closed(run, begin) && !same(points[last], points[begin])) cut(last, begin);
        shape->runs[run] = draft.runs[run];
        shape->runs[run].end = written;
        begin = end;
    }
    shape->count = written;
    for (auto i = 0u; i < written; i++) {
        auto point = space * from[i];
        Vec3 image;
        auto mapped = callback(point, image, data) && _finite(image);
        if (mapped) {
            shape->to[i] = inverse * image;
            mapped = _finite(shape->to[i]);
        }
        if (!mapped) {
            missing = point;
            delete shape;
            return Result::InvalidArguments;
        }
    }
    output = shape;
    return Result::Success;
}

}  // namespace vis
