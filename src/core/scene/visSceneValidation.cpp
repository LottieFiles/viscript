// valid(): the checks a Config and an Object pass before a scene takes them.
#include "visScene.h"
#include "visSceneValidation.h"
#include "../common/visConstants.h"
#include "../common/visControl.h"

namespace vis
{

bool valid(const Config& config) noexcept
{
    if (!config.width || !config.height || !config.fps) return false;
    if (config.cameraMode != CameraMode::Fixed && config.cameraMode != CameraMode::Interactive) return false;
    return _validCamera(config.camera, config.cameraView);
}

bool valid(const Shadow& shadow) noexcept
{
    return _finite(shadow.offset) && std::isfinite(shadow.blur) && shadow.blur >= 0.0f
           && shadow.blur <= EFFECT_SIGMA_LIMIT;
}

bool valid(const Object* object) noexcept
{
    if (!object || !std::isfinite(object->blur) || object->blur < 0.0f || object->blur > EFFECT_SIGMA_LIMIT
        || !valid(object->shadow)) {
        return false;
    }
    if (static_cast<uint8_t>(object->type()) > static_cast<uint8_t>(Type::Angle) || !std::isfinite(object->style.width) || object->style.width < 0.0f || !std::isfinite(object->style.radius) || object->style.radius <= 0.0f || object->style.dashCount > Style::DashLimit || !std::isfinite(object->style.dashOffset) || !std::isfinite(object->opacity) || object->opacity < 0.0f || object->opacity > 1.0f || !std::isfinite(object->progress) || object->progress < 0.0f || object->progress > 1.0f) {
        return false;
    }
    auto dashVisible = !object->style.dashCount;
    for (auto i = 0u; i < object->style.dashCount; i++) {
        if (!std::isfinite(object->style.dash[i]) || object->style.dash[i] < 0.0f) return false;
        if (object->style.dash[i] > 0.0f) dashVisible = true;
    }
    if (!dashVisible) return false;
    if (!_affine(object->model)) return false;
    auto pointsValid = [](const Vec3* points, uint32_t count) {
        for (auto i = 0u; i < count; i++) {
            if (!_finite(points[i])) return false;
        }
        return true;
    };
    switch (object->type()) {
        case Type::Line: {
            auto value = static_cast<const Line*>(object);
            return _finite(value->trimDistance) && value->trimDistance.x >= 0.0f && value->trimDistance.y >= 0.0f && _finite(value->from) && _finite(value->to);
        }
        case Type::Arrow: {
            auto value = static_cast<const Arrow*>(object);
            return _finite(value->trimDistance) && value->trimDistance.x >= 0.0f && value->trimDistance.y >= 0.0f && _finite(value->from) && _finite(value->to) && std::isfinite(value->tail) && value->tail >= 0.0f && std::isfinite(value->tip) && value->tip >= 0.0f;
        }
        case Type::Point: return _finite(static_cast<const Point*>(object)->point);
        case Type::Text: {
            auto value = static_cast<const Text*>(object);
            return value->text() && _finite(value->point) && std::isfinite(value->size)
                   && value->size >= 0.0f && _finite(value->align) && _finite(value->box)
                   && value->box.x >= 0.0f && value->box.y >= 0.0f && _finite(value->boxOffset)
                   && std::isfinite(value->slant) && std::fabs(value->slant) <= 1.0f
                   && (value->orientation == TextOrientation::Billboard
                       || value->orientation == TextOrientation::Plane)
                   && static_cast<uint8_t>(value->role)
                          <= static_cast<uint8_t>(TextRole::Code)
                   && (!value->counting() || std::isfinite(value->value()));
        }
        case Type::Vector: {
            auto value = static_cast<const Vector*>(object);
            return _finite(value->trimDistance) && value->trimDistance.x >= 0.0f && value->trimDistance.y >= 0.0f && _finite(value->value) && _finite(value->origin) && std::isfinite(value->tail) && value->tail >= 0.0f && std::isfinite(value->tip) && value->tip >= 0.0f;
        }
        case Type::Space: {
            auto value = static_cast<const Space*>(object);
            return std::isfinite(value->axisWidth) && (value->axisWidth == -1.0f || value->axisWidth >= 0.0f)
                   && _finite(value->x) && _finite(value->y) && _finite(value->z)
                   && (value->numberMode == NumberMode::Fixed || value->numberMode == NumberMode::Relative)
                   && std::isfinite(value->numberSize) && value->numberSize > 0.0f && _finite(value->ticks)
                   && value->ticks.x >= 0.0f && value->ticks.y >= 0.0f && value->ticks.z >= 0.0f
                   && std::isfinite(value->tickLength) && value->tickLength >= 0.0f;
        }
        case Type::Arc: {
            auto value = static_cast<const Arc*>(object);
            return _finite(value->center) && std::isfinite(value->radius) && value->radius > 0.0f
                   && std::isfinite(value->start) && std::isfinite(value->angle) && std::fabs(value->angle) <= TWO_PI;
        }
        case Type::Angle: {
            auto value = static_cast<const Angle*>(object);
            return _finite(value->vertex) && _finite(value->from) && _finite(value->to)
                   && std::isfinite(value->radius) && value->radius > 0.0f
                   && std::isfinite(value->labelDistance) && value->labelDistance >= 0.0f
                   && static_cast<uint8_t>(value->right) <= static_cast<uint8_t>(RightAngle::Always);
        }
        case Type::Circle: {
            auto value = static_cast<const Circle*>(object);
            return _finite(value->center) && std::isfinite(value->radius) && value->radius > 0.0f;
        }
        case Type::Polygon: {
            auto value = static_cast<const Polygon*>(object);
            if (!value->points() || value->count() < 3) return false;
            return pointsValid(value->points(), value->count());
        }
        case Type::Plot: {
            auto value = static_cast<const Plot*>(object);
            if (!std::isfinite(value->endpointRadius) || value->endpointRadius < 0.0f) return false;
            // A geometry clip may leave a plot with no points and no pieces.
            if (!value->count() && !value->contours()) return true;
            if (!value->points() || value->count() < 2) return false;
            if (!pointsValid(value->points(), value->count())) return false;
            // Every piece holds two points or more.
            auto start = 0u;
            for (auto i = 0u; i < value->contours(); i++) {
                auto end = value->contourEnd(i);
                if (end < start || end - start < 2u || end > value->count()) return false;
                start = end;
            }
            return true;
        }
        case Type::Route: {
            auto value = static_cast<const DirectedRoute*>(object);
            if (!value->points() || value->count() < 2 || !std::isfinite(value->tail)
                || value->tail < 0.0f || !std::isfinite(value->tip) || value->tip < 0.0f) {
                return false;
            }
            return pointsValid(value->points(), value->count());
        }
        case Type::Path: {
            auto value = static_cast<const Path*>(object);
            if (!value->points() || value->count() < (value->closed() ? 3u : 2u)) return false;
            return pointsValid(value->points(), value->count());
        }
        case Type::Curve: {
            auto value = static_cast<const Curve*>(object);
            if (!value->points() || value->count() < 2u) return false;
            return pointsValid(value->points(), value->count());
        }
        case Type::Picture: {
            auto value = static_cast<const Picture*>(object);
            auto source = (value->encoded() && value->encodedSize() && value->mime()) || value->pixels();
            return source && value->pixelWidth() && value->pixelHeight() && _finite(value->center) && std::isfinite(value->width) && value->width > 0.0f && (value->filter == ImageFilter::Bilinear || value->filter == ImageFilter::Nearest);
        }
        case Type::Cell: {
            auto value = static_cast<const Cell*>(object);
            auto frames = sampleFrames(value);
            auto duration = sampleDuration(value);
            return value->colors() && value->columns() && value->rows()
                   && value->rows() <= 16384u / value->columns()
                   && frames && std::isfinite(duration) && duration >= 0.0f
                   && (frames == 1u) == (duration == 0.0f)
                   && _finite(value->origin) && std::isfinite(value->depth)
                   && value->depth > 0.0f
                   && (value->mode == CellMode::Full || value->mode == CellMode::Padd)
                   && std::isfinite(value->padding) && value->padding >= 0.0f
                   && value->padding < 0.5f;
        }
        case Type::Group: return true;
        case Type::Rectangle: {
            auto value = static_cast<const Rectangle*>(object);
            return _finite(value->center) && _finite(value->size) && value->size.x > 0.0f && value->size.y > 0.0f
                   && std::isfinite(value->corner) && value->corner >= 0.0f
                   && value->corner <= std::fmin(value->size.x, value->size.y) * 0.5f;
        }
        case Type::Connector: {
            auto value = static_cast<const Connector*>(object);
            return _identity(value->model) && value->from() && value->to() && value->from() != value->to()
                   && std::isfinite(value->padding) && value->padding >= 0.0f
                   && std::isfinite(value->tail) && value->tail >= 0.0f
                   && std::isfinite(value->tip) && value->tip >= 0.0f;
        }
        case Type::SurfaceMesh: {
            auto value = static_cast<const SurfaceMesh*>(object);
            if (!value->points() || value->columns() < 2u || value->rows() < 2u
                || value->rows() > 65536u / value->columns()
                || (value->mode != SurfaceMode::Solid && value->mode != SurfaceMode::Mesh
                    && value->mode != SurfaceMode::SolidMesh)) {
                return false;
            }
            auto count = value->columns() * value->rows();
            if (!pointsValid(value->points(), count)) return false;
            if ((value->meshColumns == 1u || value->meshColumns > value->columns())
                || (value->meshRows == 1u || value->meshRows > value->rows())) {
                return false;
            }
            auto& light = value->light;
            if (!_finite(light.direction) || !std::isfinite(light.ambient) || light.ambient < 0.0f
                || light.ambient > 1.0f) {
                return false;
            }
            auto& colormap = value->colormap;
            if (colormap.map == Colormap::None) return true;
            auto span = static_cast<double>(colormap.max) - colormap.min;
            return static_cast<uint8_t>(colormap.map) <= static_cast<uint8_t>(Colormap::Cividis)
                   && _finite(colormap.axis) && colormap.axis.dot(colormap.axis) > 0.0f
                   && std::isfinite(colormap.min) && std::isfinite(colormap.max) && span != 0.0
                   && std::isfinite(static_cast<float>(span));
        }
    }
    return false;
}

}  // namespace vis
