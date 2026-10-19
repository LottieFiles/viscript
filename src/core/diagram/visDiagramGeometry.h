// Vector and interval arithmetic that the diagram module's layout, routing and audit share.
#ifndef _VIS_DIAGRAM_GEOMETRY_H_
#define _VIS_DIAGRAM_GEOMETRY_H_

#include <cmath>

#include "vis_diagram.h"

namespace vis::diagram::detail
{

constexpr float Epsilon = 1.0e-5f;

static inline bool _near(float lhs, float rhs) noexcept
{
    return std::fabs(lhs - rhs) <= Epsilon;
}

static inline bool _same(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return _near(lhs.x, rhs.x) && _near(lhs.y, rhs.y);
}

static inline bool _axisOverlap(float firstCenter, float firstSize, float secondCenter,
                                float secondSize) noexcept
{
    auto firstMin = firstCenter - firstSize * 0.5f;
    auto firstMax = firstCenter + firstSize * 0.5f;
    auto secondMin = secondCenter - secondSize * 0.5f;
    auto secondMax = secondCenter + secondSize * 0.5f;
    return std::fmin(firstMax, secondMax) - std::fmax(firstMin, secondMin)
           > Epsilon;
}

static inline bool _overlap(const NodePlacement& first, const NodePlacement& second) noexcept
{
    return _axisOverlap(first.center.x, first.size.x, second.center.x, second.size.x)
           && _axisOverlap(first.center.y, first.size.y, second.center.y, second.size.y);
}

// Narrows [first, last] to the part of origin + t * delta inside [minimum, maximum]. A delta
// within tolerance of zero keeps the range only when origin lies inside.
static inline bool _axisInterval(float origin, float delta, float minimum, float maximum,
                                 float& first, float& last, float tolerance = Epsilon) noexcept
{
    if (std::fabs(delta) <= tolerance) return origin >= minimum && origin <= maximum;
    auto begin = (minimum - origin) / delta;
    auto end = (maximum - origin) / delta;
    if (begin > end) {
        auto swap = begin;
        begin = end;
        end = swap;
    }
    first = std::fmax(first, begin);
    last = std::fmin(last, end);
    return first <= last;
}

static inline bool _crosses(const Vec2& from, const Vec2& to, const Vec2& center,
                            const Vec2& size, float padding) noexcept
{
    auto half = size * 0.5f + Vec2{padding, padding};
    auto minimum = center - half + Vec2{Epsilon, Epsilon};
    auto maximum = center + half - Vec2{Epsilon, Epsilon};
    if (minimum.x > maximum.x || minimum.y > maximum.y) return false;
    auto delta = to - from;
    auto first = 0.0f;
    auto last = 1.0f;
    return _axisInterval(from.x, delta.x, minimum.x, maximum.x, first, last)
           && _axisInterval(from.y, delta.y, minimum.y, maximum.y, first, last);
}

// Whether a route crosses a node's box widened by margin. When the node is the edge's source
// or target, the segment that leaves or enters it only has to stay out of the bare box.
static inline bool _routeCrosses(const Vec2* points, uint32_t count, const Vec2& center,
                                 const Vec2& size, float margin, bool source,
                                 bool target) noexcept
{
    for (auto i = 1u; i < count; i++) {
        auto end = (source && i == 1u) || (target && i + 1u == count);
        if (_crosses(points[i - 1u], points[i], center, size, end ? 0.0f : margin)) return true;
    }
    return false;
}

static inline Vec2 _outward(Port port) noexcept
{
    switch (port) {
        case Port::Left: return {-1.0f, 0.0f};
        case Port::Right: return {1.0f, 0.0f};
        case Port::Top: return {0.0f, 1.0f};
        case Port::Bottom: return {0.0f, -1.0f};
        case Port::Auto: break;
    }
    return {};
}

static inline bool _outwardSegment(const Vec2& endpoint, const Vec2& adjacent,
                                   Port port) noexcept
{
    return (adjacent - endpoint).dot(_outward(port)) > Epsilon;
}

static inline float _routeMargin(const Config& config) noexcept
{
    return std::fmax(config.routeWidth, config.arrowWidth) * 0.5f
           + config.routeWidth;
}

}  // namespace vis::diagram::detail

#endif
