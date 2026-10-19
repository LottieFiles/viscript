// The mix of two numbers that the scene and the renderer share, so a value they both interpolate
// rounds the same way in each, and the clamp of the share they mix by.
#ifndef _VIS_MIX_H_
#define _VIS_MIX_H_

#include <cmath>

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

}  // namespace vis

#endif
