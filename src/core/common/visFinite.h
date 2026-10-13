#ifndef _VIS_FINITE_H_
#define _VIS_FINITE_H_

#include <cmath>

#include "vis.h"

namespace vis
{

static inline bool _finite(float value) noexcept
{
    return std::isfinite(value);
}

static inline bool _finite(const Vec2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

static inline bool _finite(const Vec3& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

static inline bool _finite(const Mat3& value) noexcept
{
    for (auto item : value.e) {
        if (!std::isfinite(item)) return false;
    }
    return true;
}

static inline bool _finite(const Mat4& value) noexcept
{
    for (auto item : value.e) {
        if (!std::isfinite(item)) return false;
    }
    return true;
}

static inline bool _affine(const Mat4& value) noexcept
{
    return _finite(value) && value.e[12] == 0.0f && value.e[13] == 0.0f
        && value.e[14] == 0.0f && value.e[15] == 1.0f;
}

}  // namespace vis

#endif
