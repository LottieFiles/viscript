// Double-precision vectors for geometry that float would drift in, narrowed back afterwards.
#ifndef _VIS_GEOMETRY64_H_
#define _VIS_GEOMETRY64_H_

#include <cmath>
#include <limits>

#include "vis.h"

namespace vis
{

struct Vector64
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

static inline bool _normalize(double x, double y, double z, Vector64& output)
{
    auto scale = std::fmax(std::fmax(std::fabs(x), std::fabs(y)), std::fabs(z));
    if (!std::isfinite(scale) || scale == 0.0) return false;
    x /= scale;
    y /= scale;
    z /= scale;
    auto length = std::hypot(x, y, z);
    if (!std::isfinite(length) || length == 0.0) return false;
    output = {x / length, y / length, z / length};
    return true;
}

static inline bool _narrow(const Vector64& value, Vec3& output)
{
    auto limit = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)
        || std::fabs(value.x) > limit || std::fabs(value.y) > limit || std::fabs(value.z) > limit) {
        return false;
    }
    output = {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
    return true;
}

static inline Vector64 _point(const Mat4& model, const Vec3& point)
{
    auto x = static_cast<double>(point.x);
    auto y = static_cast<double>(point.y);
    auto z = static_cast<double>(point.z);
    return {model.e[0] * x + model.e[1] * y + model.e[2] * z + model.e[3],
            model.e[4] * x + model.e[5] * y + model.e[6] * z + model.e[7],
            model.e[8] * x + model.e[9] * y + model.e[10] * z + model.e[11]};
}

static inline double _distance(const Vector64& first, const Vector64& second)
{
    return std::hypot(first.x - second.x, first.y - second.y, first.z - second.z);
}

// Callers provide at least two points and reserve an extra entry for loop closure.
static inline uint32_t _parameterize(Vector64* points, uint32_t count, bool closed,
                                    double tolerance, double* lengths)
{
    auto unique = 1u;
    for (auto i = 1u; i < count; i++) {
        if (_distance(points[i], points[unique - 1u]) > tolerance) points[unique++] = points[i];
    }
    if (closed && unique > 1u && _distance(points[unique - 1u], points[0]) <= tolerance) unique--;
    if (closed && unique > 1u) points[unique++] = points[0];
    lengths[0] = 0.0;
    for (auto i = 1u; i < unique; i++)
        lengths[i] = lengths[i - 1u] + _distance(points[i - 1u], points[i]);
    return unique;
}

}  // namespace vis

#endif
