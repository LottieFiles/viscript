// Vector and interval arithmetic that the diagram module's layout and routing share.
#ifndef _VIS_DIAGRAM_GEOMETRY_H_
#define _VIS_DIAGRAM_GEOMETRY_H_

#include <cmath>

#include "vis_diagram.h"

namespace vis::diagram::detail
{

constexpr float Epsilon = 1.0e-5f;

static inline Vec2 _sub(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

static inline Vec2 _add(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

static inline Vec2 _mul(const Vec2& value, float scalar) noexcept
{
    return {value.x * scalar, value.y * scalar};
}

static inline float _length(const Vec2& value) noexcept
{
    return std::hypot(value.x, value.y);
}

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

static inline bool _boundsOverlap(const Vec2& firstCenter, const Vec2& firstSize,
                                  const Vec2& secondCenter,
                                  const Vec2& secondSize) noexcept
{
    return _axisOverlap(firstCenter.x, firstSize.x, secondCenter.x, secondSize.x)
           && _axisOverlap(firstCenter.y, firstSize.y, secondCenter.y,
                           secondSize.y);
}

static inline bool _axisInterval(float origin, float delta, float minimum, float maximum,
                                 float& first, float& last) noexcept
{
    if (_near(delta, 0.0f)) return origin >= minimum && origin <= maximum;
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
                            const Vec2& size, float padding = 0.0f) noexcept
{
    auto half = _add(_mul(size, 0.5f), {padding, padding});
    auto minimum = Vec2{center.x - half.x + Epsilon,
                        center.y - half.y + Epsilon};
    auto maximum = Vec2{center.x + half.x - Epsilon,
                        center.y + half.y - Epsilon};
    if (minimum.x > maximum.x || minimum.y > maximum.y) return false;
    auto delta = _sub(to, from);
    auto first = 0.0f;
    auto last = 1.0f;
    return _axisInterval(from.x, delta.x, minimum.x, maximum.x, first, last)
           && _axisInterval(from.y, delta.y, minimum.y, maximum.y, first, last);
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
    auto direction = _sub(adjacent, endpoint);
    auto outward = _outward(port);
    return direction.x * outward.x + direction.y * outward.y > Epsilon;
}

static inline float _routeMargin(const Config& config) noexcept
{
    return std::fmax(config.routeWidth, config.arrowWidth) * 0.5f
           + config.routeWidth;
}

}  // namespace vis::diagram::detail

#endif
