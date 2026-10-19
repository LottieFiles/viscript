// The camera checks and moves the Input and UI modules share: whether a camera is one a scene
// draws through in its view, and how a pan, orbit, zoom or change of view moves it.
#ifndef _VIS_CAMERA_H_
#define _VIS_CAMERA_H_

#include <cmath>
#include <cstdint>

#include "vis.h"
#include "visConstants.h"
#include "visFinite.h"

namespace vis
{

// The steepest pitch an orbit reaches, 89 degrees, so the camera never looks along its up vector.
constexpr float MAX_PITCH = 1.55334306f;

static inline float _clamp(float value, float minimum, float maximum) noexcept
{
    return std::fmax(minimum, std::fmin(maximum, value));
}

static inline bool _validCamera(const Camera& camera, CameraView view) noexcept
{
    if (!_finite(camera.eye) || !_finite(camera.target) || !_finite(camera.up)) return false;
    if (view != CameraView::TwoD && view != CameraView::ThreeD) return false;
    if (camera.projection != Projection::Perspective
        && camera.projection != Projection::Orthographic) {
        return false;
    }
    if (!std::isfinite(camera.near) || !std::isfinite(camera.far) || camera.near <= 0.0f
        || camera.far <= camera.near || !std::isfinite(camera.fov) || camera.fov <= 0.0f
        || camera.fov >= PI || !std::isfinite(camera.orthoHeight)
        || camera.orthoHeight <= 0.0f) {
        return false;
    }
    if (view == CameraView::TwoD
        && (camera.projection != Projection::Orthographic
            || camera.eye.x != camera.target.x || camera.eye.y != camera.target.y
            || camera.eye.z <= camera.target.z || camera.up.x != 0.0f
            || camera.up.y <= 0.0f || camera.up.z != 0.0f)) {
        return false;
    }
    auto fx = static_cast<double>(camera.target.x) - camera.eye.x;
    auto fy = static_cast<double>(camera.target.y) - camera.eye.y;
    auto fz = static_cast<double>(camera.target.z) - camera.eye.z;
    auto forwardScale = std::fmax(std::fmax(std::fabs(fx), std::fabs(fy)), std::fabs(fz));
    auto upScale = std::fmax(std::fmax(std::fabs(camera.up.x), std::fabs(camera.up.y)),
                             std::fabs(camera.up.z));
    if (!std::isfinite(forwardScale) || !std::isfinite(upScale) || forwardScale == 0.0
        || upScale == 0.0) {
        return false;
    }
    fx /= forwardScale;
    fy /= forwardScale;
    fz /= forwardScale;
    auto forwardLength = std::hypot(fx, fy, fz);
    auto ux = static_cast<double>(camera.up.x) / upScale;
    auto uy = static_cast<double>(camera.up.y) / upScale;
    auto uz = static_cast<double>(camera.up.z) / upScale;
    auto upLength = std::hypot(ux, uy, uz);
    fx /= forwardLength;
    fy /= forwardLength;
    fz /= forwardLength;
    ux /= upLength;
    uy /= upLength;
    uz /= upLength;
    return std::hypot(fy * uz - fz * uy, fz * ux - fx * uz, fx * uy - fy * ux)
           > 1.0e-6;
}

static inline Result _cameraAction(Camera& camera, CameraView& view, CameraAction action,
                            const Vec2& delta) noexcept
{
    if (static_cast<uint8_t>(action) > static_cast<uint8_t>(CameraAction::View3D)
        || !_finite(delta)) {
        return Result::InvalidArguments;
    }
    auto previous = camera;
    auto previousView = view;
    if ((action == CameraAction::Pan || action == CameraAction::Orbit
         || action == CameraAction::Zoom)
        && delta.x == 0.0f && delta.y == 0.0f) {
        return Result::Success;
    }
    auto amountX = delta.x;
    auto amountY = delta.y;
    switch (action) {
        case CameraAction::Pan: {
            auto forward = (camera.target - camera.eye).normalized();
            auto right = forward.cross(camera.up).normalized();
            auto up = right.cross(forward).normalized();
            auto scale = camera.projection == Projection::Orthographic
                             ? camera.orthoHeight
                             : (camera.eye - camera.target).length();
            auto movement = right * (-amountX * scale) + up * (amountY * scale);
            camera.eye = camera.eye + movement;
            camera.target = camera.target + movement;
            break;
        }
        case CameraAction::Orbit: {
            // An orbit routine may remain bound while a camera timeline is in its 2D phase.
            if (view != CameraView::ThreeD) return Result::Success;
            auto offset = camera.eye - camera.target;
            auto radius = offset.length();
            auto yaw = std::atan2(offset.x, offset.z) - std::fmod(amountX, 2.0f) * PI;
            auto ratio = radius > 0.0f ? offset.y / radius : 0.0f;
            ratio = _clamp(ratio, -1.0f, 1.0f);
            auto pitch = _clamp(std::asin(ratio) + amountY * PI, -MAX_PITCH, MAX_PITCH);
            auto horizontal = radius * std::cos(pitch);
            camera.eye = camera.target
                         + Vec3{horizontal * std::sin(yaw), radius * std::sin(pitch),
                                horizontal * std::cos(yaw)};
            camera.up = {0.0f, 1.0f, 0.0f};
            break;
        }
        case CameraAction::Zoom: {
            auto factor = std::exp(_clamp(amountY, -16.0f, 16.0f));
            if (camera.projection == Projection::Orthographic) {
                camera.orthoHeight *= factor;
            } else {
                camera.eye = camera.target + (camera.eye - camera.target) * factor;
            }
            break;
        }
        case CameraAction::Reset: return Result::Success;
        case CameraAction::View2D: {
            auto distance = (camera.eye - camera.target).length();
            if (!std::isfinite(distance) || distance <= camera.near) distance = 10.0f;
            camera.eye = camera.target + Vec3{0.0f, 0.0f, distance};
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.projection = Projection::Orthographic;
            view = CameraView::TwoD;
            break;
        }
        case CameraAction::View3D: {
            auto distance = (camera.eye - camera.target).length();
            if (!std::isfinite(distance) || distance <= camera.near) distance = 10.0f;
            auto component = distance * 0.57735026919f;
            camera.eye = camera.target + Vec3{component, component, component};
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.projection = Projection::Perspective;
            view = CameraView::ThreeD;
            break;
        }
    }
    if (_validCamera(camera, view)) return Result::Success;
    camera = previous;
    view = previousView;
    return Result::InvalidArguments;
}

}  // namespace vis

#endif
