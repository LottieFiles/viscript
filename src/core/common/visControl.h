// What the UI panel and the Input controller share: the region and family checks of their
// bindings, and the drive that adds up their camera actions and moves the scene's camera by
// them each frame.
#ifndef _VIS_CONTROL_H_
#define _VIS_CONTROL_H_

#include <cmath>

#include "vis.h"
#include "visCamera.h"
#include "visFinite.h"

namespace vis
{

static inline float clamp01(float value) noexcept
{
    return _clamp(value, 0.0f, 1.0f);
}

static inline bool validRegion(const BBox& bounds) noexcept
{
    return std::isfinite(bounds.x) && std::isfinite(bounds.y)
           && std::isfinite(bounds.width) && std::isfinite(bounds.height)
           && bounds.width > 0.0f && bounds.height > 0.0f;
}

static inline bool contains(const BBox& bounds, const Vec2& point) noexcept
{
    return point.x >= bounds.x && point.x <= bounds.x + bounds.width
           && point.y >= bounds.y && point.y <= bounds.y + bounds.height;
}

// Whether either object lies in the other's family.
static inline bool family(const Object* first, const Object* second) noexcept
{
    if (!first || !second) return false;
    for (auto object = first; object; object = object->parent()) {
        if (object == second) return true;
    }
    for (auto object = second; object; object = object->parent()) {
        if (object == first) return true;
    }
    return false;
}

// The camera actions of a panel or controller: pans, orbits and zooms add up in steps of at
// most four units, and a 2D or 3D action forces that view until a reset. Each frame moves the
// scene's camera to the forced view, then by the orbit, the zoom and the pan.
struct CameraDrive
{
    Vec2 move;
    Vec2 orbit;
    float zoom = 0.0f;
    // View2D or View3D once an action forces that view; until then Reset, which moves nothing.
    CameraAction forced = CameraAction::Reset;

    Result act(CameraAction action, const Vec2& delta) noexcept
    {
        if (!_finite(delta)
            || static_cast<uint8_t>(action) > static_cast<uint8_t>(CameraAction::View3D)) {
            return Result::InvalidArguments;
        }
        // Steps this small cannot carry a finite sum past the largest float.
        auto step = Vec2{_clamp(delta.x, -4.0f, 4.0f), _clamp(delta.y, -4.0f, 4.0f)};
        switch (action) {
            case CameraAction::Pan: move = move + step; break;
            case CameraAction::Orbit: orbit = orbit + step; break;
            case CameraAction::Zoom: zoom += step.y; break;
            case CameraAction::Reset: *this = {}; break;
            case CameraAction::View2D:
            case CameraAction::View3D: forced = action; break;
        }
        return Result::Success;
    }

    Result view(CameraView value) noexcept
    {
        if (value != CameraView::TwoD && value != CameraView::ThreeD) {
            return Result::InvalidArguments;
        }
        forced = value == CameraView::TwoD ? CameraAction::View2D : CameraAction::View3D;
        return Result::Success;
    }

    Result apply(Camera& camera, CameraView& view) const noexcept
    {
        const CameraInput steps[] = {
            {forced, {}},
            {CameraAction::Orbit, orbit},
            {CameraAction::Zoom, {0.0f, zoom}},
            {CameraAction::Pan, move},
        };
        for (auto& step : steps) {
            auto result = _cameraAction(camera, view, step.action, step.delta);
            if (result != Result::Success) return result;
        }
        return _validCamera(camera, view) ? Result::Success : Result::InvalidArguments;
    }
};

}  // namespace vis

#endif
