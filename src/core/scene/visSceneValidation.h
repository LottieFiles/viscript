#ifndef _VIS_SCENE_VALIDATION_H_
#define _VIS_SCENE_VALIDATION_H_

#include "../common/visFinite.h"

namespace vis
{

static inline bool _finite(const Range& value)
{
    return std::isfinite(value.min) && std::isfinite(value.max) && std::isfinite(value.step) && value.min <= value.max && value.step > 0.0f;
}

static inline bool _identity(const Mat4& value)
{
    auto identity = Mat4::identity();
    for (auto i = 0u; i < 16u; i++) {
        if (value.e[i] != identity.e[i]) return false;
    }
    return true;
}

}  // namespace vis

#endif
