// Retargeting: an eased move from one state to another that a newer target restarts from where
// it is. The UI, input and states modules move their values with it.
#ifndef _VIS_RETARGET_H_
#define _VIS_RETARGET_H_

#include "vis.h"

namespace vis::detail
{

template<typename State>
struct Retarget
{
    State from;
    State to;
    AnimCurve curve;
    double begin = 0.0;
    float duration = 0.0f;
    bool active = false;
};

inline double retargetElapsed(double time, double begin, double cycle = 0.0) noexcept
{
    auto elapsed = time - begin;
    if (elapsed < 0.0 && cycle > 0.0) elapsed += cycle;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

template<typename State, typename Interpolate>
State retargetSample(const Retarget<State>& track, double time, double cycle,
                     Interpolate interpolate) noexcept
{
    if (!track.active || track.duration <= 0.0f) return track.to;
    auto elapsed = retargetElapsed(time, track.begin, cycle);
    if (elapsed <= 0.0) return track.from;
    if (elapsed >= track.duration) return track.to;
    auto progress = static_cast<float>(elapsed / track.duration);
    return interpolate(track.from, track.to, ease(track.curve, progress));
}

template<typename State>
bool retargetActive(const Retarget<State>& track, double time,
                    double cycle = 0.0) noexcept
{
    return track.active
        && retargetElapsed(time, track.begin, cycle) < track.duration;
}

template<typename State, typename Interpolate>
void retarget(Retarget<State>& track, const State& target, double time, float duration,
              const AnimCurve& curve, double cycle, Interpolate interpolate) noexcept
{
    auto current = retargetSample(track, time, cycle, interpolate);
    track.from = duration > 0.0f ? current : target;
    track.to = target;
    track.curve = curve;
    track.begin = time;
    track.duration = duration;
    track.active = duration > 0.0f;
}

template<typename State>
void retargetSet(Retarget<State>& track, const State& state) noexcept
{
    track.from = state;
    track.to = state;
    track.begin = 0.0;
    track.duration = 0.0f;
    track.active = false;
}

}  // namespace vis::detail

#endif
