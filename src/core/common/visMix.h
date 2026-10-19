// The mix of two numbers that the scene and the renderer share, so a value they both interpolate
// rounds the same way in each.
#ifndef _VIS_MIX_H_
#define _VIS_MIX_H_

namespace vis
{

// from + (to - from) * progress, in double precision and rounded once to a float; progress may
// lie outside [0, 1].
static inline float _mix(float from, float to, float progress)
{
    return static_cast<float>(static_cast<double>(from)
           + (static_cast<double>(to) - from) * progress);
}

}  // namespace vis

#endif
