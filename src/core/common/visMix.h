// The mix of two numbers or colors that the scene and the renderer share, so a value they both
// interpolate rounds the same way in each, the clamp of the share they mix by, a color's opacity,
// and the single precision mix of the scene's colors.
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

// The mix of two channels by a progress in [0, 1], rounded, in single precision as the scene mixes
// colors. Where the multiply and the add round apart, as in WebAssembly, it can differ from _mix()
// by one.
static inline uint8_t _lerp(uint8_t from, uint8_t to, float progress)
{
    auto value = static_cast<float>(from)
               + (static_cast<float>(to) - static_cast<float>(from)) * progress;
    return static_cast<uint8_t>(value + 0.5f);
}

static inline Color _lerp(const Color& from, const Color& to, float progress)
{
    return {
        _lerp(from.r, to.r, progress),
        _lerp(from.g, to.g, progress),
        _lerp(from.b, to.b, progress),
        _lerp(from.a, to.a, progress)};
}

}  // namespace vis

#endif
