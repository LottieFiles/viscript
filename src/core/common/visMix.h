// The mix of two numbers or colors that the scene and the renderer share, so a value they both
// interpolate rounds the same way in each, the clamp of the share they mix by, a share of an
// alpha, and an alpha or a color scaled by an opacity.
#ifndef _VIS_MIX_H_
#define _VIS_MIX_H_

#include <cmath>

#include "vis.h"

namespace vis
{

// value within [0, 1], and 0 for one that is not a finite number.
static inline float _clamp(float value)
{
    if (!std::isfinite(value)) return 0.0f;
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

// from + (to - from) * progress, in double precision and rounded once to a float; progress may
// lie outside [0, 1].
static inline float _mix(float from, float to, float progress)
{
    return static_cast<float>(static_cast<double>(from)
           + (static_cast<double>(to) - from) * progress);
}

// color with its alpha scaled by opacity, which _clamp() bounds.
static inline Color _opacity(Color color, float opacity)
{
    auto alpha = static_cast<float>(color.a) * _clamp(opacity);
    color.a = static_cast<uint8_t>(alpha + 0.5f);
    return color;
}

// share, which the caller keeps in [0, 1], as an alpha in [0, 255], rounded.
static inline uint8_t _alpha(float share)
{
    return static_cast<uint8_t>(share * 255.0f + 0.5f);
}

// alpha scaled by share, which the caller keeps in [0, 1], rounded; _alpha(share) is the same
// for an alpha of 255.
static inline uint8_t _alpha(float share, uint8_t alpha)
{
    return static_cast<uint8_t>(share * static_cast<float>(alpha) + 0.5f);
}

// alpha scaled by opacity, both in [0, 255], rounded.
static inline uint8_t _opacity(uint8_t alpha, uint8_t opacity)
{
    return static_cast<uint8_t>((static_cast<uint32_t>(alpha) * opacity + 127u) / 255u);
}

// The mix of two channels by a progress that _clamp() bounds, rounded.
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
