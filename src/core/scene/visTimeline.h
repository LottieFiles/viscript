// Timeline arithmetic that the scene and the modules share: decimal-stable times, and where
// the items of a lagged group begin and end.
#ifndef _VIS_TIMELINE_H_
#define _VIS_TIMELINE_H_

#include <cmath>
#include <cstddef>
#include <limits>

#include "vis.h"

namespace vis
{

static constexpr uint32_t MAX_MARKERS = 256;
static constexpr size_t MAX_MARKER_NAME = 128;
static constexpr auto SCENE_MOUNTED = "the scene is shown inside another scene and can no longer change";
static constexpr auto TIMELINE_RANGE = "the timeline would end beyond the representable time range";
static constexpr auto WAIT_DURATION = "the wait must be zero or a positive number of seconds";
static constexpr auto SEEK_TIME = "the time must be zero or a positive number of seconds";

// Accumulate authored decimals to avoid drift across frame boundaries.
static inline double _decimal(float value)
{
    static constexpr double POWERS[] = {1.0e0, 1.0e1, 1.0e2, 1.0e3, 1.0e4, 1.0e5, 1.0e6, 1.0e7,
                                        1.0e8, 1.0e9, 1.0e10, 1.0e11, 1.0e12, 1.0e13, 1.0e14};
    auto magnitude = std::fabs(static_cast<double>(value));
    if (!(magnitude >= 1.0e-6 && magnitude < 1.0e7)) return value;
    auto exponent = static_cast<int>(std::floor(std::log10(magnitude)));
    for (auto digits = 1; digits <= 9; digits++) {
        auto places = digits - 1 - exponent;
        auto decimal = places >= 0 ? std::nearbyint(magnitude * POWERS[places]) / POWERS[places]
                                   : std::nearbyint(magnitude / POWERS[-places]) * POWERS[-places];
        if (static_cast<float>(decimal) == static_cast<float>(magnitude)) return value < 0.0f ? -decimal : decimal;
    }
    return value;
}

static inline float _begin(float cursor, float duration, float lag, uint32_t index)
{
    return static_cast<float>(_decimal(cursor) + _decimal(duration) * _decimal(lag) * index);
}

static inline float _end(float cursor, float duration, float lag, uint32_t index)
{
    return static_cast<float>(_decimal(cursor) + _decimal(duration) * (1.0 + _decimal(lag) * index));
}

static inline bool _timeline(float cursor, float duration, float lag, uint32_t count,
                      float& nextCursor) noexcept
{
    // As _end() reckons the last item, so that it ends where the cursor goes.
    auto span = _decimal(duration) * (1.0 + _decimal(lag) * (count - 1u));
    auto next = _decimal(cursor) + span;
    if (!std::isfinite(span) || next > std::numeric_limits<float>::max()) return false;
    nextCursor = static_cast<float>(next);
    if (nextCursor <= cursor) return false;
    for (auto i = 0u; i < count; i++) {
        auto begin = _begin(cursor, duration, lag, i);
        auto end = _end(cursor, duration, lag, i);
        if (!std::isfinite(begin) || !std::isfinite(end) || end <= begin) return false;
    }
    return true;
}

}  // namespace vis

#endif
