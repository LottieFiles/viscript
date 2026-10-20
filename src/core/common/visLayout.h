// What the renderer's layout report and the audits that read it share: the clearance a box keeps
// inside another and from a second, and whether a visual draws an object.
#ifndef _VIS_LAYOUT_H_
#define _VIS_LAYOUT_H_

#include <cmath>
#include <cstdint>

#include "vis.h"

namespace vis
{

// The least margin inner keeps inside outer, negative where it reaches out.
static inline float _clearance(const BBox& inner, const BBox& outer) noexcept
{
    auto left = inner.x - outer.x;
    auto top = inner.y - outer.y;
    auto right = outer.x + outer.width - inner.x - inner.width;
    auto bottom = outer.y + outer.height - inner.y - inner.height;
    return std::fmin(std::fmin(left, top), std::fmin(right, bottom));
}

// The gap between two spans of an axis, each a start and a size, or minus their overlap.
static inline float _axisClearance(float first, float firstSize, float second,
                                   float secondSize) noexcept
{
    auto firstEnd = first + firstSize;
    auto secondEnd = second + secondSize;
    if (firstEnd <= second) return second - firstEnd;
    if (secondEnd <= first) return first - secondEnd;
    return -std::fmin(firstEnd, secondEnd) + std::fmax(first, second);
}

// The clearance between first and second along each axis, and in separation the move of second,
// along the axis that needs less, that leaves gap between them.
static inline Vec2 _clearance(const BBox& first, const BBox& second, float gap,
                              Vec2& separation) noexcept
{
    Vec2 clearance = {_axisClearance(first.x, first.width, second.x, second.width),
                      _axisClearance(first.y, first.height, second.y, second.height)};
    auto horizontal = gap - clearance.x;
    auto vertical = gap - clearance.y;
    if (horizontal <= vertical) {
        separation.x = second.center().x < first.center().x ? -horizontal : horizontal;
    } else {
        separation.y = second.center().y < first.center().y ? -vertical : vertical;
    }
    return clearance;
}

// Whether visual draws object, itself or as the counterpart of a morph.
static inline bool _applies(const LayoutVisual& visual, uint32_t object) noexcept
{
    return visual.object == object
           || (visual.kind == LayoutVisualKind::Morph && visual.counterpart == object);
}

}  // namespace vis

#endif
