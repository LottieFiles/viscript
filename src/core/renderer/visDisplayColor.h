// Color arithmetic that the display list and its blends share: opacity and mixing, by shares
// that _clamp() bounds.
#ifndef _VIS_DISPLAY_COLOR_H_
#define _VIS_DISPLAY_COLOR_H_

#include "vis.h"
#include "../common/visMix.h"

namespace vis
{

static inline Color _opacity(Color color, float opacity)
{
    auto alpha = static_cast<float>(color.a) * _clamp(opacity);
    color.a = static_cast<uint8_t>(alpha + 0.5f);
    return color;
}

static inline uint8_t _mix(uint8_t from, uint8_t to, float progress)
{
    return static_cast<uint8_t>(_mix(static_cast<float>(from), static_cast<float>(to),
                                     _clamp(progress)) + 0.5f);
}

static inline Color _mix(const Color& from, const Color& to, float progress)
{
    return {
        _mix(from.r, to.r, progress),
        _mix(from.g, to.g, progress),
        _mix(from.b, to.b, progress),
        _mix(from.a, to.a, progress)};
}

}  // namespace vis

#endif
