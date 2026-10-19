// The scene engine behind vis.h: owns the objects, records what each play, transition and
// camera move does as clips on a timeline, and samples them into the frame a renderer draws.
// Sections, each marked "// ---------- name ----------":
//  Limits and billboards / Fingerprints and validity / Interpolation and matrices
//  / Time spans and visual states / Storing shapes / Applying clips and overlap rules
//  / Motion paths / Morph pairs and geometry ownership / Scene transition frames
//  / Storage growth and bookkeeping / Sampling clips / Animation factories / Animation targets
//  / Creating scenes and attaching objects / Cloning / Placement, measuring and viewports
//  / Object transitions / Layers, effects and fade transforms / Match transforms and handoffs
//  / Highlights and emphasis / Arranging / Preparing plays / Playing animations / Updating objects
//  / Saving and restoring / Warps / Runtime modifiers and camera input / Cameras and views
//  / Frames and queries

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

#include "vis.h"
#include "visRendererBackend.h"
#include "visScene.h"
#include "visMatch.h"
#include "visMorph.h"
#include "visGeometry64.h"
#include "visTimeline.h"
#include "../common/visHash.h"
#include "../common/visConstants.h"
#include "../common/visGrow.h"
#include "../common/visString.h"
#include "visSceneValidation.h"
#include "visWarp.h"

namespace vis
{

// ---------- Limits and billboards ----------

static constexpr uint32_t MAX_OBJECTS = 1048576;
static constexpr uint32_t MAX_OBJECT_DEPTH = 64;
static constexpr uint32_t MAX_VIEWPORTS = 64;
static constexpr uint32_t MAX_VIEWPORT_DEPTH = 16;
static constexpr uint32_t MAX_VIEWS = 64;
static constexpr uint32_t MAX_VIEW_HIDDEN = 256;
static constexpr auto SOURCE_SERIF_FONT = "Source Serif 4";
static constexpr auto KOREAN_FONT = "IBM Plex Sans KR";

float billboardScale(const Mat4& reference, const Mat4& model, float fallback) noexcept
{
    auto stretch = [](const Mat4& value) {
        auto x = Vec3{value.e[0], value.e[4], value.e[8]};
        auto y = Vec3{value.e[1], value.e[5], value.e[9]};
        return std::fmin(x.length(), y.length());
    };
    auto from = stretch(reference);
    return from > 1.0e-6f ? stretch(model) / from : fallback;
}

Mat4 Scene::Impl::billboardReference(const Object* object) const noexcept
{
    auto reference = Mat4::identity();
    for (auto owner = object; owner; owner = owner->parent()) {
        if (auto item = entry(owner)) reference = item->base.model * reference;
    }
    return reference;
}

// ---------- Fingerprints and validity ----------

static bool _equal(const Color& first, const Color& second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b
           && first.a == second.a;
}

static const TextTheme& _textTheme(const Theme& theme, TextRole role)
{
    switch (role) {
        case TextRole::H1: return theme.h1;
        case TextRole::H2: return theme.h2;
        case TextRole::H3: return theme.h3;
        case TextRole::Text: return theme.text;
        case TextRole::Code: return theme.code;
    }
    return theme.text;
}

static bool _hangul(const char* text)
{
    auto cursor = reinterpret_cast<const uint8_t*>(text);
    while (cursor && *cursor) {
        uint32_t codepoint = 0u;
        uint32_t length = 0u;
        if (*cursor < 0x80u) {
            codepoint = *cursor;
            length = 1u;
        } else if ((*cursor & 0xe0u) == 0xc0u) {
            codepoint = *cursor & 0x1fu;
            length = 2u;
        } else if ((*cursor & 0xf0u) == 0xe0u) {
            codepoint = *cursor & 0x0fu;
            length = 3u;
        } else if ((*cursor & 0xf8u) == 0xf0u) {
            codepoint = *cursor & 0x07u;
            length = 4u;
        } else {
            cursor++;
            continue;
        }
        auto valid = true;
        for (auto i = 1u; i < length; i++) {
            if ((cursor[i] & 0xc0u) != 0x80u) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6u) | (cursor[i] & 0x3fu);
        }
        if (!valid) {
            cursor++;
            continue;
        }
        if ((codepoint >= 0x1100u && codepoint <= 0x11ffu)
            || (codepoint >= 0x3130u && codepoint <= 0x318fu)
            || (codepoint >= 0xa960u && codepoint <= 0xa97fu)
            || (codepoint >= 0xac00u && codepoint <= 0xd7ffu)
            || (codepoint >= 0xffa0u && codepoint <= 0xffdcu)) {
            return true;
        }
        cursor += length;
    }
    return false;
}

static const char* _textFont(const TextTheme& theme, const Text* text)
{
    if (std::strcmp(theme.font, SOURCE_SERIF_FONT) == 0 && _hangul(text->text())) return KOREAN_FONT;
    return theme.font;
}

static bool _valid(const TextTheme& text)
{
    return text.font && text.font[0] && std::isfinite(text.size) && text.size > 0.0f;
}

static bool _valid(const Theme& theme)
{
    return theme.objectCount && theme.objectCount <= Theme::ColorLimit
           && std::isfinite(theme.objectWidth) && theme.objectWidth > 0.0f
           && std::isfinite(theme.roundness) && theme.roundness >= 0.0f
           && theme.roundness <= 0.5f
           && _valid(theme.h1) && _valid(theme.h2) && _valid(theme.h3)
           && _valid(theme.text) && _valid(theme.code);
}

static bool _palette(Type type)
{
    return type != Type::Space && type != Type::Text && type != Type::Picture
           && type != Type::Cell && type != Type::Group;
}

// Shapes that enclose an area, so a fill inherited from a group has something to paint.
static bool _fillable(const Object* object)
{
    auto type = object->type();
    if (type == Type::Arc) return static_cast<const Arc*>(object)->sector;
    return type == Type::Point || type == Type::Circle || type == Type::Rectangle
           || type == Type::Polygon || type == Type::Path || type == Type::SurfaceMesh;
}

// Style properties a group passes down to the shapes below it.
enum struct Inherited : uint8_t
{
    Stroke,
    Fill,
    Width
};

static Style _defaultStyle(Type type)
{
    Style style;
    if (type == Type::Point) {
        style.fill = style.stroke;
    } else if (type == Type::Text) {
        style.fill = style.stroke;
        style.stroke.a = 0;
    } else if (type == Type::Space) {
        style.stroke = {55, 65, 81, 180};
        style.width = 1.0f;
    } else if (type == Type::Group) {
        style.stroke.a = 0;
        style.fill.a = 0;
        style.width = 0.0f;
    } else if (type == Type::SurfaceMesh) {
        style.fill = {91, 141, 239, 255};
        style.stroke = {35, 55, 84, 255};
        style.width = 1.0f;
    }
    return style;
}

static bool _colorChanged(const Object* object)
{
    auto style = _defaultStyle(object->type());
    return !_equal(object->style.stroke, style.stroke)
           || !_equal(object->style.fill, style.fill);
}

static void _hash(uint64_t& hash, const Vec2& value)
{
    _hash(hash, value.x);
    _hash(hash, value.y);
}

static void _hash(uint64_t& hash, const Vec3& value)
{
    _hash(hash, value.x);
    _hash(hash, value.y);
    _hash(hash, value.z);
}

static void _hash(uint64_t& hash, const Color& value)
{
    _hash(hash, value.r);
    _hash(hash, value.g);
    _hash(hash, value.b);
    _hash(hash, value.a);
}

static void _hash(uint64_t& hash, const Range& value)
{
    _hash(hash, value.min);
    _hash(hash, value.max);
    _hash(hash, value.step);
}

static void _hash(uint64_t& hash, const char* value)
{
    if (!value) {
        auto empty = uint8_t{0};
        _hash(hash, empty);
        return;
    }
    _hash(hash, value, std::strlen(value) + 1u);
}

uint64_t fingerprint(const Object* object) noexcept
{
    auto hash = uint64_t{14695981039346656037ull};
    auto type = object->type();
    _hash(hash, type);
    _hash(hash, object->style.stroke);
    _hash(hash, object->style.fill);
    _hash(hash, object->style.gradientEnd);
    _hash(hash, object->style.width);
    _hash(hash, object->style.radius);
    _hash(hash, object->style.dashCount);
    _hash(hash, object->style.dashOffset);
    auto& gradient = object->style.gradient;
    _hash(hash, gradient.type);
    _hash(hash, gradient.stopCount);
    for (auto i = 0u; i < gradient.stopCount && i < Gradient::StopLimit; i++) {
        _hash(hash, gradient.stops[i].offset);
        _hash(hash, gradient.stops[i].color);
    }
    _hash(hash, gradient.from);
    _hash(hash, gradient.to);
    _hash(hash, gradient.center);
    _hash(hash, gradient.focal);
    _hash(hash, gradient.radius);
    _hash(hash, gradient.focalRadius);
    _hash(hash, gradient.angle);
    _hash(hash, gradient.line);
    _hash(hash, gradient.centered);
    _hash(hash, gradient.sized);
    _hash(hash, gradient.focused);
    for (auto i = 0u; i < object->style.dashCount && i < Style::DashLimit; i++) {
        _hash(hash, object->style.dash[i]);
    }
    _hash(hash, object->layer);
    _hash(hash, object->opacity);
    _hash(hash, object->progress);
    _hash(hash, object->blur);
    _hash(hash, object->shadow.color);
    _hash(hash, object->shadow.offset);
    _hash(hash, object->shadow.blur);
    for (auto value : object->model.e)
        _hash(hash, value);
    switch (type) {
        case Type::Line: {
            auto value = static_cast<const Line*>(object);
            _hash(hash, value->from);
            _hash(hash, value->to);
            _hash(hash, value->trimDistance);
            break;
        }
        case Type::Arrow: {
            auto value = static_cast<const Arrow*>(object);
            _hash(hash, value->from);
            _hash(hash, value->to);
            _hash(hash, value->tail);
            _hash(hash, value->tip);
            _hash(hash, value->trimDistance);
            break;
        }
        case Type::Point: _hash(hash, static_cast<const Point*>(object)->point); break;
        case Type::Text: {
            auto value = static_cast<const Text*>(object);
            _hash(hash, value->point);
            _hash(hash, value->size);
            _hash(hash, value->align);
            _hash(hash, value->box);
            _hash(hash, value->boxOffset);
            _hash(hash, value->slant);
            _hash(hash, value->tabular);
            _hash(hash, value->orientation);
            _hash(hash, value->role);
            _hash(hash, value->text());
            _hash(hash, value->font());
            if (value->counting()) {
                auto format = value->format();
                _hash(hash, value->value());
                _hash(hash, format.decimals);
                _hash(hash, format.sign);
                _hash(hash, format.separator);
            }
            break;
        }
        case Type::Vector: {
            auto value = static_cast<const Vector*>(object);
            _hash(hash, value->value);
            _hash(hash, value->origin);
            _hash(hash, value->tail);
            _hash(hash, value->tip);
            _hash(hash, value->trimDistance);
            break;
        }
        case Type::Space: {
            auto value = static_cast<const Space*>(object);
            _hash(hash, value->x);
            _hash(hash, value->y);
            _hash(hash, value->z);
            _hash(hash, value->axisWidth);
            _hash(hash, value->axisX);
            _hash(hash, value->axisY);
            _hash(hash, value->axisZ);
            _hash(hash, value->numberColor);
            _hash(hash, value->numberMode);
            _hash(hash, value->numberSize);
            _hash(hash, value->numbers);
            _hash(hash, value->ticks);
            _hash(hash, value->tickLength);
            break;
        }
        case Type::Arc: {
            auto value = static_cast<const Arc*>(object);
            _hash(hash, value->center); _hash(hash, value->radius);
            _hash(hash, value->start); _hash(hash, value->angle); _hash(hash, value->sector);
            break;
        }
        case Type::Angle: {
            auto value = static_cast<const Angle*>(object);
            _hash(hash, value->vertex); _hash(hash, value->from); _hash(hash, value->to);
            _hash(hash, value->radius); _hash(hash, value->other); _hash(hash, value->right);
            _hash(hash, value->labelDistance);
            break;
        }
        case Type::Circle: {
            auto value = static_cast<const Circle*>(object);
            _hash(hash, value->center);
            _hash(hash, value->radius);
            break;
        }
        case Type::Polygon: {
            auto value = static_cast<const Polygon*>(object);
            _hash(hash, value->count());
            for (auto i = 0u; i < value->count(); i++)
                _hash(hash, value->points()[i]);
            break;
        }
        case Type::Plot: {
            auto value = static_cast<const Plot*>(object);
            _hash(hash, value->count());
            for (auto i = 0u; i < value->count(); i++)
                _hash(hash, value->points()[i]);
            _hash(hash, value->contours());
            for (auto i = 0u; i + 1u < value->contours(); i++)
                _hash(hash, value->contourEnd(i));
            _hash(hash, value->flat);
            _hash(hash, value->endpointRadius);
            auto grid = value->contourGrid();
            _hash(hash, grid.level);
            if (auto labels = value->contourLabels()) {
                _hash(hash, labels->format);
                _hash(hash, labels->size);
                _hash(hash, labels->spacing);
                _hash(hash, labels->padding);
                _hash(hash, labels->upright);
            }
            break;
        }
        case Type::Route: {
            auto value = static_cast<const DirectedRoute*>(object);
            _hash(hash, value->count());
            _hash(hash, value->tail);
            _hash(hash, value->tip);
            for (auto i = 0u; i < value->count(); i++)
                _hash(hash, value->points()[i]);
            break;
        }
        case Type::Path: {
            auto value = static_cast<const Path*>(object);
            _hash(hash, value->count());
            _hash(hash, value->closed());
            _hash(hash, value->contours());
            for (auto i = 0u; i + 1u < value->contours(); i++)
                _hash(hash, value->contourEnd(i));
            for (auto i = 0u; i < value->count(); i++)
                _hash(hash, value->points()[i]);
            break;
        }
        case Type::Curve: {
            auto value = static_cast<const Curve*>(object);
            _hash(hash, value->count());
            for (auto i = 0u; i < value->count(); i++)
                _hash(hash, value->points()[i]);
            break;
        }
        case Type::Picture: {
            auto value = static_cast<const Picture*>(object);
            _hash(hash, value->center);
            _hash(hash, value->width);
            _hash(hash, value->filter);
            _hash(hash, value->pixelWidth());
            _hash(hash, value->pixelHeight());
            _hash(hash, value->encodedSize());
            if (value->mime()) _hash(hash, value->mime());
            break;
        }
        case Type::Cell: {
            auto value = static_cast<const Cell*>(object);
            _hash(hash, value->origin);
            _hash(hash, value->depth);
            _hash(hash, value->mode);
            _hash(hash, value->padding);
            _hash(hash, value->columns());
            _hash(hash, value->rows());
            _hash(hash, sampleFrames(value));
            _hash(hash, sampleDuration(value));
            break;
        }
        case Type::Group: break;
        case Type::Rectangle: {
            auto value = static_cast<const Rectangle*>(object);
            _hash(hash, value->center);
            _hash(hash, value->size);
            _hash(hash, value->corner);
            break;
        }
        case Type::Connector: {
            auto value = static_cast<const Connector*>(object);
            _hash(hash, value->from());
            _hash(hash, value->to());
            _hash(hash, value->padding);
            _hash(hash, value->tail);
            _hash(hash, value->tip);
            break;
        }
        case Type::SurfaceMesh: {
            auto value = static_cast<const SurfaceMesh*>(object);
            _hash(hash, value->mode);
            _hash(hash, value->shading);
            _hash(hash, value->columns());
            _hash(hash, value->rows());
            _hash(hash, value->colormap.map);
            _hash(hash, value->colormap.axis);
            _hash(hash, value->colormap.min);
            _hash(hash, value->colormap.max);
            _hash(hash, value->meshColumns);
            _hash(hash, value->meshRows);
            _hash(hash, value->light.direction);
            _hash(hash, value->light.ambient);
            auto count = value->columns() * value->rows();
            for (auto i = 0u; i < count; i++)
                _hash(hash, value->points()[i]);
            _hash(hash, value->colors() != nullptr);
            for (auto i = 0u; value->colors() && i < count; i++)
                _hash(hash, value->colors()[i]);
            break;
        }
    }
    return hash;
}

// ---------- Interpolation and matrices ----------

static bool _equal(const Mat4& first, const Mat4& second)
{
    for (auto i = 0u; i < 16u; i++) {
        if (first.e[i] != second.e[i]) return false;
    }
    return true;
}

static float _clamp(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static uint8_t _lerp(uint8_t from, uint8_t to, float progress)
{
    auto value = static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * progress;
    return static_cast<uint8_t>(value + 0.5f);
}

static Color _lerp(const Color& from, const Color& to, float progress)
{
    return {
        _lerp(from.r, to.r, progress),
        _lerp(from.g, to.g, progress),
        _lerp(from.b, to.b, progress),
        _lerp(from.a, to.a, progress)};
}

static Mat4 _lerp(const Mat4& from, const Mat4& to, float progress)
{
    Mat4 result;
    for (auto i = 0; i < 16; i++) {
        result.e[i] = static_cast<float>(static_cast<double>(from.e[i]) + (static_cast<double>(to.e[i]) - from.e[i]) * progress);
    }
    return result;
}

struct Rotation
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;
};

static bool _normalize(Rotation& value)
{
    auto length = std::sqrt(value.x * value.x + value.y * value.y
                            + value.z * value.z + value.w * value.w);
    if (!std::isfinite(length) || length == 0.0) return false;
    value = {value.x / length, value.y / length, value.z / length, value.w / length};
    return true;
}

static bool _frame(const Camera& camera, Rotation& rotation, double& distance)
{
    auto dx = static_cast<double>(camera.target.x) - camera.eye.x;
    auto dy = static_cast<double>(camera.target.y) - camera.eye.y;
    auto dz = static_cast<double>(camera.target.z) - camera.eye.z;
    Vector64 forward;
    Vector64 sourceUp;
    if (!_normalize(dx, dy, dz, forward)
        || !_normalize(camera.up.x, camera.up.y, camera.up.z, sourceUp)) {
        return false;
    }
    Vector64 right;
    if (!_normalize(forward.y * sourceUp.z - forward.z * sourceUp.y,
                    forward.z * sourceUp.x - forward.x * sourceUp.z,
                    forward.x * sourceUp.y - forward.y * sourceUp.x, right)) {
        return false;
    }
    Vector64 up;
    if (!_normalize(right.y * forward.z - right.z * forward.y,
                    right.z * forward.x - right.x * forward.z,
                    right.x * forward.y - right.y * forward.x, up)) {
        return false;
    }
    auto m00 = right.x;
    auto m01 = up.x;
    auto m02 = -forward.x;
    auto m10 = right.y;
    auto m11 = up.y;
    auto m12 = -forward.y;
    auto m20 = right.z;
    auto m21 = up.z;
    auto m22 = -forward.z;
    auto trace = m00 + m11 + m22;
    if (trace > 0.0) {
        auto scale = std::sqrt(trace + 1.0) * 2.0;
        rotation = {(m21 - m12) / scale, (m02 - m20) / scale,
                    (m10 - m01) / scale, scale * 0.25};
    } else if (m00 > m11 && m00 > m22) {
        auto scale = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
        rotation = {scale * 0.25, (m01 + m10) / scale,
                    (m02 + m20) / scale, (m21 - m12) / scale};
    } else if (m11 > m22) {
        auto scale = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
        rotation = {(m01 + m10) / scale, scale * 0.25,
                    (m12 + m21) / scale, (m02 - m20) / scale};
    } else {
        auto scale = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
        rotation = {(m02 + m20) / scale, (m12 + m21) / scale,
                    scale * 0.25, (m10 - m01) / scale};
    }
    distance = std::hypot(dx, dy, dz);
    return std::isfinite(distance) && distance > 0.0 && _normalize(rotation);
}

static bool _lerp(const Rotation& from, const Rotation& to, float progress, Rotation& output)
{
    auto destination = to;
    auto cosine = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
    if (cosine < 0.0) {
        destination = {-to.x, -to.y, -to.z, -to.w};
        cosine = -cosine;
    }
    cosine = std::fmax(-1.0, std::fmin(1.0, cosine));
    if (cosine > 0.9995) {
        output = {from.x + (destination.x - from.x) * progress,
                  from.y + (destination.y - from.y) * progress,
                  from.z + (destination.z - from.z) * progress,
                  from.w + (destination.w - from.w) * progress};
        return _normalize(output);
    }
    auto angle = std::acos(cosine);
    auto sine = std::sin(angle);
    if (!std::isfinite(sine) || sine == 0.0) return false;
    auto fromWeight = std::sin((1.0 - progress) * angle) / sine;
    auto toWeight = std::sin(progress * angle) / sine;
    output = {from.x * fromWeight + destination.x * toWeight,
              from.y * fromWeight + destination.y * toWeight,
              from.z * fromWeight + destination.z * toWeight,
              from.w * fromWeight + destination.w * toWeight};
    return _normalize(output);
}

static bool _rotate(const Rotation& rotation, const Vector64& value, Vector64& output)
{
    auto tx = 2.0 * (rotation.y * value.z - rotation.z * value.y);
    auto ty = 2.0 * (rotation.z * value.x - rotation.x * value.z);
    auto tz = 2.0 * (rotation.x * value.y - rotation.y * value.x);
    return _normalize(value.x + rotation.w * tx + rotation.y * tz - rotation.z * ty,
                      value.y + rotation.w * ty + rotation.z * tx - rotation.x * tz,
                      value.z + rotation.w * tz + rotation.x * ty - rotation.y * tx,
                      output);
}

// Row-major 3x3 linear part of an affine Mat4, in double precision.
struct Linear
{
    double m[9] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
};

static Linear _linear(const Mat4& model)
{
    Linear linear;
    for (auto row = 0; row < 3; row++) {
        for (auto column = 0; column < 3; column++)
            linear.m[row * 3 + column] = model.e[row * 4 + column];
    }
    return linear;
}

static Linear _multiply(const Linear& first, const Linear& second)
{
    Linear result;
    for (auto row = 0; row < 3; row++) {
        for (auto column = 0; column < 3; column++) {
            result.m[row * 3 + column] = first.m[row * 3] * second.m[column]
                                       + first.m[row * 3 + 1] * second.m[3 + column]
                                       + first.m[row * 3 + 2] * second.m[6 + column];
        }
    }
    return result;
}

static Linear _transpose(const Linear& value)
{
    Linear result;
    for (auto row = 0; row < 3; row++) {
        for (auto column = 0; column < 3; column++)
            result.m[row * 3 + column] = value.m[column * 3 + row];
    }
    return result;
}

static Vector64 _apply(const Linear& linear, const Vector64& value)
{
    auto& m = linear.m;
    return {m[0] * value.x + m[1] * value.y + m[2] * value.z,
            m[3] * value.x + m[4] * value.y + m[5] * value.z,
            m[6] * value.x + m[7] * value.y + m[8] * value.z};
}

static double _determinant(const Linear& value)
{
    auto& m = value.m;
    return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6])
         + m[2] * (m[3] * m[7] - m[4] * m[6]);
}

// Inverse transpose (the cofactor matrix over the determinant); false when singular.
static bool _inverseTranspose(const Linear& value, Linear& output)
{
    auto& m = value.m;
    auto scale = 0.0;
    for (auto element : m)
        scale = std::fmax(scale, std::fabs(element));
    auto determinant = _determinant(value);
    if (!std::isfinite(determinant) || scale == 0.0
        || std::fabs(determinant) <= 1.0e-12 * scale * scale * scale) {
        return false;
    }
    auto inverse = 1.0 / determinant;
    output.m[0] = (m[4] * m[8] - m[5] * m[7]) * inverse;
    output.m[1] = (m[5] * m[6] - m[3] * m[8]) * inverse;
    output.m[2] = (m[3] * m[7] - m[4] * m[6]) * inverse;
    output.m[3] = (m[2] * m[7] - m[1] * m[8]) * inverse;
    output.m[4] = (m[0] * m[8] - m[2] * m[6]) * inverse;
    output.m[5] = (m[1] * m[6] - m[0] * m[7]) * inverse;
    output.m[6] = (m[1] * m[5] - m[2] * m[4]) * inverse;
    output.m[7] = (m[2] * m[3] - m[0] * m[5]) * inverse;
    output.m[8] = (m[0] * m[4] - m[1] * m[3]) * inverse;
    return true;
}

static Linear _matrix(const Rotation& value)
{
    auto x = value.x;
    auto y = value.y;
    auto z = value.z;
    auto w = value.w;
    Linear result;
    result.m[0] = 1.0 - 2.0 * (y * y + z * z);
    result.m[1] = 2.0 * (x * y - z * w);
    result.m[2] = 2.0 * (x * z + y * w);
    result.m[3] = 2.0 * (x * y + z * w);
    result.m[4] = 1.0 - 2.0 * (x * x + z * z);
    result.m[5] = 2.0 * (y * z - x * w);
    result.m[6] = 2.0 * (x * z - y * w);
    result.m[7] = 2.0 * (y * z + x * w);
    result.m[8] = 1.0 - 2.0 * (x * x + y * y);
    return result;
}

static Rotation _multiply(const Rotation& first, const Rotation& second)
{
    return {first.w * second.x + first.x * second.w + first.y * second.z - first.z * second.y,
            first.w * second.y - first.x * second.z + first.y * second.w + first.z * second.x,
            first.w * second.z + first.x * second.y - first.y * second.x + first.z * second.w,
            first.w * second.w - first.x * second.x - first.y * second.y - first.z * second.z};
}

static Rotation _conjugate(const Rotation& value)
{
    return {-value.x, -value.y, -value.z, value.w};
}

// Turn of angle radians about a unit axis.
static Rotation _rotation(const Vector64& axis, double angle)
{
    auto sine = std::sin(angle * 0.5);
    return {axis.x * sine, axis.y * sine, axis.z * sine, std::cos(angle * 0.5)};
}

// Shortest turn from one unit direction to another. A reversal turns about the axis
// closest to +z so that planar paths stay in their plane.
static Rotation _turn(const Vector64& from, const Vector64& to)
{
    auto dot = from.x * to.x + from.y * to.y + from.z * to.z;
    Rotation value = {from.y * to.z - from.z * to.y, from.z * to.x - from.x * to.z,
                      from.x * to.y - from.y * to.x, 1.0 + dot};
    if (value.w > 1.0e-9 && _normalize(value)) return value;
    Vector64 axis;
    if (!_normalize(-from.z * from.x, -from.z * from.y, 1.0 - from.z * from.z, axis)) axis = {1.0, 0.0, 0.0};
    return _rotation(axis, 3.14159265358979323846);
}

static Mat4 _model(const Linear& linear, const Vector64& translation)
{
    auto model = Mat4::identity();
    for (auto row = 0; row < 3; row++) {
        for (auto column = 0; column < 3; column++)
            model.e[row * 4 + column] = static_cast<float>(linear.m[row * 3 + column]);
    }
    model.e[3] = static_cast<float>(translation.x);
    model.e[7] = static_cast<float>(translation.y);
    model.e[11] = static_cast<float>(translation.z);
    return model;
}

static bool _inverse(const Mat4& value, Mat4& output)
{
    Linear inverseTranspose;
    if (!_affine(value) || !_inverseTranspose(_linear(value), inverseTranspose)) return false;
    auto inverse = _transpose(inverseTranspose);
    auto moved = _apply(inverse, {value.e[3], value.e[7], value.e[11]});
    output = _model(inverse, {-moved.x, -moved.y, -moved.z});
    return _affine(output);
}

// Model of the object's parent chain, mapping its parent space to the world.
static Mat4 _parentWorld(const Object* object)
{
    auto matrix = Mat4::identity();
    for (auto parent = object->parent(); parent; parent = parent->parent())
        matrix = parent->model * matrix;
    return matrix;
}

static constexpr auto ARC_LIMIT = 6.28318530717958647692;

static bool _arc(float arc)
{
    return std::isfinite(arc) && std::fabs(static_cast<double>(arc)) < ARC_LIMIT;
}

// Circular-arc displacement about unit axis; axial travel remains linear for exact endpoints.
static Vector64 _travel(const Vector64& vector, const Vector64& axis, double arc, double progress)
{
    auto along = vector.x * axis.x + vector.y * axis.y + vector.z * axis.z;
    auto planar = Vector64{vector.x - axis.x * along, vector.y - axis.y * along, vector.z - axis.z * along};
    auto planarLength = std::hypot(planar.x, planar.y, planar.z);
    if (std::fabs(arc) < 1.0e-4 || planarLength == 0.0) {
        return {vector.x * progress, vector.y * progress, vector.z * progress};
    }
    auto half = Vector64{planar.x * 0.5, planar.y * 0.5, planar.z * 0.5};
    auto bend = 1.0 / std::tan(arc * 0.5);
    auto center = Vector64{half.x + (axis.y * half.z - axis.z * half.y) * bend,
                           half.y + (axis.z * half.x - axis.x * half.z) * bend,
                           half.z + (axis.x * half.y - axis.y * half.x) * bend};
    // The start relates to the center by -center, which is perpendicular to the axis.
    auto angle = arc * progress;
    auto cosine = std::cos(angle);
    auto sine = std::sin(angle);
    auto start = Vector64{-center.x, -center.y, -center.z};
    auto turned = Vector64{
        start.x * cosine + (axis.y * start.z - axis.z * start.y) * sine,
        start.y * cosine + (axis.z * start.x - axis.x * start.z) * sine,
        start.z * cosine + (axis.x * start.y - axis.y * start.x) * sine};
    return {center.x + turned.x + axis.x * along * progress,
            center.y + turned.y + axis.y * along * progress,
            center.z + turned.z + axis.z * along * progress};
}

static Vector64 _unit(const Vec3& axis)
{
    Vector64 unit;
    if (!_normalize(axis.x, axis.y, axis.z, unit)) unit = {0.0, 0.0, 1.0};
    return unit;
}

static bool _axis(const Vec3& axis)
{
    Vector64 unit;
    return _finite(axis) && _normalize(axis.x, axis.y, axis.z, unit);
}

// Arc travel is affine because each point's arc center depends affinely on that point.
// Use G = a * (I - axis axis^T) + b * [axis]x + progress * axis axis^T;
// G is zero at start and identity at end. Axial travel remains linear.
static Mat4 _sweep(const Mat4& from, const Mat4& to, const Vector64& axis, double arc, float progress)
{
    if (std::fabs(arc) < 1.0e-4) return _lerp(from, to, progress);
    auto angle = arc * progress;
    auto bend = 1.0 / std::tan(arc * 0.5);
    auto a = 0.5 * ((1.0 - std::cos(angle)) + bend * std::sin(angle));
    auto b = 0.5 * ((1.0 - std::cos(angle)) * bend - std::sin(angle));
    double n[3] = {axis.x, axis.y, axis.z};
    double cross[9] = {0.0, -n[2], n[1], n[2], 0.0, -n[0], -n[1], n[0], 0.0};
    double sweep[9];
    for (auto row = 0; row < 3; row++) {
        for (auto column = 0; column < 3; column++) {
            auto along = n[row] * n[column];
            sweep[row * 3 + column] = a * ((row == column ? 1.0 : 0.0) - along) + b * cross[row * 3 + column]
                                    + progress * along;
        }
    }
    auto model = from;
    for (auto column = 0; column < 4; column++) {
        double change[3];
        for (auto row = 0; row < 3; row++)
            change[row] = static_cast<double>(to.e[row * 4 + column]) - from.e[row * 4 + column];
        for (auto row = 0; row < 3; row++) {
            model.e[row * 4 + column] = static_cast<float>(
                from.e[row * 4 + column] + sweep[row * 3] * change[0] + sweep[row * 3 + 1] * change[1]
                + sweep[row * 3 + 2] * change[2]);
        }
    }
    return model;
}

static bool _animationSafe(const Camera& camera)
{
    Rotation rotation;
    double distance;
    if (!_frame(camera, rotation, distance)) return false;
    auto limit = static_cast<double>(std::numeric_limits<float>::max());
    return std::fabs(static_cast<double>(camera.target.x)) + distance <= limit
           && std::fabs(static_cast<double>(camera.target.y)) + distance <= limit
           && std::fabs(static_cast<double>(camera.target.z)) + distance <= limit;
}

static double _spacing(float value)
{
    value = std::fabs(value);
    auto adjacent = std::nextafter(value, std::numeric_limits<float>::infinity());
    if (std::isfinite(adjacent)) return static_cast<double>(adjacent) - value;
    return static_cast<double>(value) - std::nextafter(value, 0.0f);
}

static bool _samePose(const Camera& from, const Camera& to)
{
    return from.eye.x == to.eye.x && from.eye.y == to.eye.y && from.eye.z == to.eye.z
           && from.target.x == to.target.x && from.target.y == to.target.y
           && from.target.z == to.target.z && from.up.x == to.up.x && from.up.y == to.up.y
           && from.up.z == to.up.z;
}

static bool _poseSafe(const Camera& from, const Camera& to)
{
    if (_samePose(from, to)) return true;
    Rotation fromRotation;
    Rotation toRotation;
    double fromDistance;
    double toDistance;
    if (!_frame(from, fromRotation, fromDistance) || !_frame(to, toRotation, toDistance)) return false;
    auto envelope = [](const Camera& camera, double distance) {
        return std::fmax(std::fmax(std::fabs(static_cast<double>(camera.target.x)) + distance,
                                   std::fabs(static_cast<double>(camera.target.y)) + distance),
                         std::fabs(static_cast<double>(camera.target.z)) + distance);
    };
    auto maximum = std::fmax(envelope(from, fromDistance), envelope(to, toDistance));
    auto upper = static_cast<float>(maximum);
    if (static_cast<double>(upper) < maximum) {
        upper = std::nextafter(upper, std::numeric_limits<float>::infinity());
    }
    constexpr auto precisionMargin = 6.928203230275509;  // 4 * sqrt(3)
    return std::fmin(fromDistance, toDistance) > precisionMargin * _spacing(upper);
}

static bool _planesSafe(const Camera& from, const Camera& to)
{
    if (from.near == to.near && from.far == to.far) return true;
    auto maximum = std::fmax(std::fmax(from.near, from.far), std::fmax(to.near, to.far));
    auto fromGap = static_cast<double>(from.far) - from.near;
    auto toGap = static_cast<double>(to.far) - to.near;
    return std::fmin(fromGap, toGap) > _spacing(maximum);
}

static bool _lerp(const Camera& from, const Camera& to, float progress, Camera& camera)
{
    if (progress <= 0.0f) {
        camera = from;
        return true;
    }
    if (progress >= 1.0f) {
        camera = to;
        return true;
    }
    camera = from;
    if (!_samePose(from, to)) {
        Rotation fromRotation;
        Rotation toRotation;
        double fromDistance;
        double toDistance;
        if (!_frame(from, fromRotation, fromDistance) || !_frame(to, toRotation, toDistance)) return false;
        Rotation rotation;
        if (!_lerp(fromRotation, toRotation, progress, rotation)) return false;
        Vector64 backward;
        Vector64 up;
        if (!_rotate(rotation, {0.0, 0.0, 1.0}, backward)
            || !_rotate(rotation, {0.0, 1.0, 0.0}, up)) {
            return false;
        }
        auto target = Vector64{
            static_cast<double>(from.target.x) + (static_cast<double>(to.target.x) - from.target.x) * progress,
            static_cast<double>(from.target.y) + (static_cast<double>(to.target.y) - from.target.y) * progress,
            static_cast<double>(from.target.z) + (static_cast<double>(to.target.z) - from.target.z) * progress};
        auto distance = fromDistance + (toDistance - fromDistance) * progress;
        if (!_narrow(target, camera.target)
            || !_narrow({target.x + backward.x * distance,
                         target.y + backward.y * distance,
                         target.z + backward.z * distance}, camera.eye)
            || !_narrow(up, camera.up)) {
            return false;
        }
    }
    camera.fov = static_cast<float>(static_cast<double>(from.fov) + (static_cast<double>(to.fov) - from.fov) * progress);
    camera.orthoHeight = static_cast<float>(static_cast<double>(from.orthoHeight) + (static_cast<double>(to.orthoHeight) - from.orthoHeight) * progress);
    camera.near = static_cast<float>(static_cast<double>(from.near) + (static_cast<double>(to.near) - from.near) * progress);
    camera.far = static_cast<float>(static_cast<double>(from.far) + (static_cast<double>(to.far) - from.far) * progress);
    return true;
}

// Turns the eye and up of from by angle radians about axis through the target.
static bool _orbit(const Camera& from, const Vec3& axis, double angle, Camera& camera)
{
    Vector64 unit;
    if (!_normalize(axis.x, axis.y, axis.z, unit)) return false;
    auto cosine = std::cos(angle);
    auto sine = std::sin(angle);
    // Rodrigues' rotation formula.
    auto turn = [&unit, cosine, sine](double x, double y, double z) {
        auto along = (unit.x * x + unit.y * y + unit.z * z) * (1.0 - cosine);
        return Vector64{x * cosine + (unit.y * z - unit.z * y) * sine + unit.x * along,
                        y * cosine + (unit.z * x - unit.x * z) * sine + unit.y * along,
                        z * cosine + (unit.x * y - unit.y * x) * sine + unit.z * along};
    };
    auto offset = turn(static_cast<double>(from.eye.x) - from.target.x, static_cast<double>(from.eye.y) - from.target.y,
                       static_cast<double>(from.eye.z) - from.target.z);
    camera = from;
    return _narrow({from.target.x + offset.x, from.target.y + offset.y, from.target.z + offset.z}, camera.eye)
           && _narrow(turn(from.up.x, from.up.y, from.up.z), camera.up);
}

// Adds the share progress of a layered move's change to camera (see CameraClip::layered).
static bool _layer(const CameraClip& clip, float progress, Camera& camera)
{
    auto current = camera;
    // An orbit keeps the curve's overshoot, as it does on its own.
    if (clip.orbit != 0.0f) return _orbit(current, clip.axis, static_cast<double>(clip.orbit) * progress, camera);
    progress = _clamp(progress);
    Rotation from;
    Rotation to;
    double fromDistance;
    double toDistance;
    if (!_frame(clip.from, from, fromDistance) || !_frame(clip.to, to, toDistance)) return false;
    auto share = static_cast<double>(progress);
    auto scale = std::pow(toDistance / fromDistance, share);
    auto reach = Vector64{(static_cast<double>(current.eye.x) - current.target.x) * scale,
                          (static_cast<double>(current.eye.y) - current.target.y) * scale,
                          (static_cast<double>(current.eye.z) - current.target.z) * scale};
    // A look that keeps the view direction leaves the eye's direction and up as they are.
    auto turn = _multiply(to, _conjugate(from));
    if (std::fabs(turn.w) < 1.0 - 1.0e-12) {
        Rotation part;
        Rotation rotation;
        double distance;
        Vector64 backward;
        Vector64 up;
        if (!_lerp(Rotation{}, turn, progress, part) || !_frame(current, rotation, distance)) return false;
        rotation = _multiply(part, rotation);
        if (!_rotate(rotation, {0.0, 0.0, 1.0}, backward) || !_rotate(rotation, {0.0, 1.0, 0.0}, up)
            || !_narrow(up, camera.up)) {
            return false;
        }
        reach = {backward.x * distance * scale, backward.y * distance * scale, backward.z * distance * scale};
    }
    auto target = Vector64{current.target.x + (static_cast<double>(clip.to.target.x) - clip.from.target.x) * share,
                           current.target.y + (static_cast<double>(clip.to.target.y) - clip.from.target.y) * share,
                           current.target.z + (static_cast<double>(clip.to.target.z) - clip.from.target.z) * share};
    camera.orthoHeight = static_cast<float>(
        current.orthoHeight * std::pow(static_cast<double>(clip.to.orthoHeight) / clip.from.orthoHeight, share));
    // The field of view and the clip planes ease from the current camera's to the look's, as a
    // plain look eases them from its start: between two valid lenses they stay valid however the
    // moves combine. The planes move together so that near stays in front of far.
    auto toward = [share](float value, float goal) {
        return static_cast<float>(static_cast<double>(value) + (static_cast<double>(goal) - value) * share);
    };
    if (clip.from.fov != clip.to.fov) camera.fov = toward(current.fov, clip.to.fov);
    if (clip.from.near != clip.to.near || clip.from.far != clip.to.far) {
        camera.near = toward(current.near, clip.to.near);
        camera.far = toward(current.far, clip.to.far);
    }
    return _narrow(target, camera.target)
           && _narrow({target.x + reach.x, target.y + reach.y, target.z + reach.z}, camera.eye);
}

// Whether to faces another way than from or holds its up direction another way.
static bool _turns(const Camera& from, const Camera& to)
{
    if (_samePose(from, to)) return false;
    Rotation start;
    Rotation end;
    double distance;
    if (!_frame(from, start, distance) || !_frame(to, end, distance)) return true;
    return std::fabs(_multiply(end, _conjugate(start)).w) < 1.0 - 1.0e-12;
}

// The reason a camera move that turns the camera or changes its projection cannot run at time.
static const char* _planarReason(char (&reason)[256], float time)
{
    std::snprintf(reason, sizeof(reason),
                  "the camera is in the 2D view at %.2f s, while this move would turn it or change its "
                  "projection, which only the 3D view allows",
                  static_cast<double>(time));
    return reason;
}

// ---------- Time spans and visual states ----------

// A span of time whose ends count when closed.
struct TimeSpan
{
    float begin;
    bool beginClosed;
    float end;
    bool endClosed;
};

static TimeSpan _shared(const TimeSpan& first, const TimeSpan& second)
{
    auto span = first;
    if (second.begin > span.begin || (second.begin == span.begin && !second.beginClosed)) {
        span.begin = second.begin;
        span.beginClosed = second.beginClosed;
    }
    if (second.end < span.end || (second.end == span.end && !second.endClosed)) {
        span.end = second.end;
        span.endClosed = second.endClosed;
    }
    return span;
}

static bool _occupied(const TimeSpan& span)
{
    return span.begin < span.end || (span.begin == span.end && span.beginClosed && span.endClosed);
}

static VisualState _state(const Object* object)
{
    VisualState state;
    state.model = object->model;
    state.stroke = object->style.stroke;
    state.fill = object->style.fill;
    state.gradientEnd = object->style.gradientEnd;
    state.width = object->style.width;
    state.radius = object->style.radius;
    state.dashOffset = object->style.dashOffset;
    switch (object->type()) {
        case Type::Line: state.trimDistance = static_cast<const Line*>(object)->trimDistance; break;
        case Type::Arrow: {
            auto directed = static_cast<const Arrow*>(object);
            state.trimDistance = directed->trimDistance;
            state.markerTail = directed->tail;
            state.markerTip = directed->tip;
            break;
        }
        case Type::Vector: {
            auto directed = static_cast<const Vector*>(object);
            state.trimDistance = directed->trimDistance;
            state.markerTail = directed->tail;
            state.markerTip = directed->tip;
            break;
        }
        case Type::Connector: {
            auto directed = static_cast<const Connector*>(object);
            state.markerTail = directed->tail;
            state.markerTip = directed->tip;
            break;
        }
        case Type::Route: {
            auto directed = static_cast<const DirectedRoute*>(object);
            state.markerTail = directed->tail;
            state.markerTip = directed->tip;
            break;
        }
        case Type::Arc: {
            auto arc = static_cast<const Arc*>(object);
            state.size = {arc->radius, arc->radius}; state.arcStart = arc->start; state.arcAngle = arc->angle;
            break;
        }
        case Type::Angle: {
            auto radius = static_cast<const Angle*>(object)->radius;
            state.size = {radius, radius};
            break;
        }
        case Type::Circle: {
            auto radius = static_cast<const Circle*>(object)->radius;
            state.size = {radius, radius};
            break;
        }
        case Type::Rectangle: {
            auto rectangle = static_cast<const Rectangle*>(object);
            state.size = rectangle->size;
            state.corner = rectangle->corner;
            break;
        }
        case Type::Text: {
            auto text = static_cast<const Text*>(object);
            state.size = {text->size, text->size};
            state.value = text->value();
            if (!text->counting()) {
                state.text = text->text();
                state.textFrom = state.text;
            }
            break;
        }
        default: break;
    }
    state.opacity = object->opacity;
    state.progress = object->progress;
    state.blur = object->blur;
    state.shadow = object->shadow;
    state.layer = object->layer;
    state.gradient = object->style.gradient;
    // A copy starts from the warp its source showed (see Scene::clone()); any other object
    // starts from its own geometry, whatever warps follow.
    if (auto born = bornWarp(object)) {
        state.warp = born;
        state.warpProgress = 1.0f;
    }
    return state;
}

// Copies into state the parts of from that no field of an object holds: how much of its fill
// shows, the scale of its pixel sizes, and how its outline draws. _state() gives every object
// the same ones, but a copy starts from those its source showed (see Scene::clone()).
static void _unheld(VisualState& state, const VisualState& from)
{
    state.fillProgress = from.fillProgress;
    state.pixelScale = from.pixelScale;
    state.traceFill = from.traceFill;
    state.direction = from.direction;
}

// Gives entry state as its new base, read from the object's fields once a layout or a match has
// changed them, with the parts of the old base that no field holds, so a copy keeps its own.
static void _rebase(ObjectEntry& entry, VisualState state)
{
    _unheld(state, entry.base);
    entry.base = state;
}

// ---------- Storing shapes ----------

void Scene::Impl::store(Object* object, const VisualState& state) noexcept
{
    object->style.stroke = state.stroke;
    object->style.fill = state.fill;
    object->style.gradientEnd = state.gradientEnd;
    object->style.width = state.width;
    object->style.radius = state.radius;
    object->style.dashOffset = state.dashOffset;
    switch (object->type()) {
        case Type::Line: static_cast<Line*>(object)->trimDistance = state.trimDistance; break;
        case Type::Arrow: {
            auto directed = static_cast<Arrow*>(object);
            directed->trimDistance = state.trimDistance;
            directed->tail = state.markerTail;
            directed->tip = state.markerTip;
            break;
        }
        case Type::Vector: {
            auto directed = static_cast<Vector*>(object);
            directed->trimDistance = state.trimDistance;
            directed->tail = state.markerTail;
            directed->tip = state.markerTip;
            break;
        }
        case Type::Connector: {
            auto directed = static_cast<Connector*>(object);
            directed->tail = state.markerTail;
            directed->tip = state.markerTip;
            break;
        }
        case Type::Route: {
            auto directed = static_cast<DirectedRoute*>(object);
            directed->tail = state.markerTail;
            directed->tip = state.markerTip;
            break;
        }
        case Type::Arc: {
            auto arc = static_cast<Arc*>(object);
            arc->radius = state.size.x; arc->start = state.arcStart; arc->angle = state.arcAngle;
            break;
        }
        case Type::Angle: static_cast<Angle*>(object)->radius = state.size.x; break;
        case Type::Circle: static_cast<Circle*>(object)->radius = state.size.x; break;
        case Type::Rectangle: {
            auto rectangle = static_cast<Rectangle*>(object);
            rectangle->size = state.size;
            rectangle->corner = state.corner;
            break;
        }
        case Type::Text: {
            auto text = static_cast<Text*>(object);
            text->size = state.size.x;
            if (text->objCounting) {
                text->objValue = state.value;
                text->write(state.value, text->objText, text->objCapacity);
            } else if (state.text) {
                // A state points only at the text's own strings.
                text->objText = const_cast<char*>(state.text);
            }
            break;
        }
        default: break;
    }
    object->style.gradient = state.gradient;
    object->opacity = state.opacity;
    object->progress = state.progress;
    object->model = state.model;
    object->blur = state.blur;
    object->shadow = state.shadow;
    object->layer = state.layer;
    // The geometry a geometry clip ends at; its copies keep the ones before.
    if (state.geometry) writeShape(object, *state.geometry);
}

void Scene::Impl::writeShape(Object* object, const GeometryShape& shape) noexcept
{
    auto points = shape.points;
    switch (object->type()) {
        case Type::Arc: static_cast<Arc*>(object)->center = points[0]; break;
        case Type::Angle: {
            auto angle = static_cast<Angle*>(object);
            angle->vertex = points[0]; angle->from = points[1]; angle->to = points[2];
            break;
        }
        case Type::Line:
            static_cast<Line*>(object)->from = points[0];
            static_cast<Line*>(object)->to = points[1];
            break;
        case Type::Arrow:
            static_cast<Arrow*>(object)->from = points[0];
            static_cast<Arrow*>(object)->to = points[1];
            break;
        case Type::Vector:
            static_cast<Vector*>(object)->value = points[0];
            static_cast<Vector*>(object)->origin = points[1];
            break;
        case Type::Polygon: {
            auto polygon = static_cast<Polygon*>(object);
            std::memcpy(polygon->objPoints, points, sizeof(Vec3) * polygon->objCount);
            break;
        }
        case Type::Plot: {
            // room() made room for the shape, which may hold other pieces than the plot has.
            auto plot = static_cast<Plot*>(object);
            if (shape.count > plot->objRoom || (shape.pieces > 1u && shape.pieces > plot->objEndRoom)) break;
            if (shape.count) std::memcpy(plot->objPoints, points, sizeof(Vec3) * shape.count);
            if (shape.pieces > 1u) std::memcpy(plot->objEnds, shape.ends, sizeof(uint32_t) * shape.pieces);
            plot->objCount = shape.count;
            plot->objContours = shape.pieces;
            plot->objGrid = shape.contour;
            if (shape.samples.count) std::memcpy(plot->objSamplePoints, shape.samples.points, sizeof(Vec3) * shape.samples.count);
            plot->objSamples = shape.samples;
            plot->objSamples.points = plot->objSamplePoints;
            break;
        }
        case Type::Route: {
            auto route = static_cast<DirectedRoute*>(object);
            std::memcpy(route->objPoints, points, sizeof(Vec3) * route->objCount);
            break;
        }
        case Type::Path: {
            auto path = static_cast<Path*>(object);
            if (path->objContours == 1u) std::memcpy(path->objPoints, points, sizeof(Vec3) * path->objCount);
            break;
        }
        case Type::Curve: {
            auto curve = static_cast<Curve*>(object);
            std::memcpy(curve->objControls, points, sizeof(Vec3) * 4u);
            for (auto i = 0u; i < curve->objCount; i++)
                curve->objPoints[i] = curvePoint(curve->objControls, i, curve->objCount);
            break;
        }
        default: break;
    }
}

bool Scene::Impl::room(Object* object, const GeometryShape& shape) noexcept
{
    if (object->type() != Type::Plot) return true;
    auto plot = static_cast<Plot*>(object);
    if (shape.samples.count > plot->objSampleRoom) {
        auto points = new (std::nothrow) Vec3[shape.samples.count];
        if (!points) return false;
        if (plot->objSamples.count) std::memcpy(points, plot->objSamples.points, sizeof(Vec3) * plot->objSamples.count);
        delete[] plot->objSamplePoints;
        plot->objSamplePoints = points;
        plot->objSamples.points = points;
        plot->objSampleRoom = shape.samples.count;
    }
    if (shape.count > plot->objRoom) {
        auto points = new (std::nothrow) Vec3[shape.count];
        if (!points) return false;
        if (plot->objCount) std::memcpy(points, plot->objPoints, sizeof(Vec3) * plot->objCount);
        delete[] plot->objPoints;
        plot->objPoints = points;
        plot->objRoom = shape.count;
    }
    if (shape.pieces > 1u && shape.pieces > plot->objEndRoom) {
        auto ends = new (std::nothrow) uint32_t[shape.pieces];
        if (!ends) return false;
        if (plot->objContours > 1u) std::memcpy(ends, plot->objEnds, sizeof(uint32_t) * plot->objContours);
        delete[] plot->objEnds;
        plot->objEnds = ends;
        plot->objEndRoom = shape.pieces;
    }
    return true;
}

// ---------- Applying clips and overlap rules ----------

// Gives the object of entry the colors of source where it shows the theme's, in its base and in
// its clips, which clips holds in the list of entry.
static void _inherit(ObjectEntry& entry, Clip* clips, const Style& source, bool inheritColor,
                     bool inheritGradient)
{
    auto authoredStroke = entry.object->style.stroke;
    auto authoredFill = entry.object->style.fill;
    auto authoredGradientEnd = entry.object->style.gradientEnd;
    auto authoredGradient = entry.object->style.gradient;
    if (inheritColor) {
        entry.object->style.stroke = source.stroke;
        entry.object->style.fill = source.fill;
        if (_equal(entry.base.stroke, authoredStroke)) entry.base.stroke = source.stroke;
        if (_equal(entry.base.fill, authoredFill)) entry.base.fill = source.fill;
    }
    if (inheritGradient) {
        entry.object->style.gradient = source.gradient;
        entry.object->style.gradientEnd = source.gradientEnd;
        if (entry.base.gradient == authoredGradient) entry.base.gradient = source.gradient;
        if (_equal(entry.base.gradientEnd, authoredGradientEnd)) {
            entry.base.gradientEnd = source.gradientEnd;
        }
    }
    for (auto i = entry.firstClip; i != NO_CLIP; i = clips[i].next) {
        auto& clip = clips[i];
        if (inheritColor) {
            if (_equal(clip.from.stroke, authoredStroke)) clip.from.stroke = source.stroke;
            if (_equal(clip.to.stroke, authoredStroke)) clip.to.stroke = source.stroke;
            if (_equal(clip.from.fill, authoredFill)) clip.from.fill = source.fill;
            if (_equal(clip.to.fill, authoredFill)) clip.to.fill = source.fill;
        }
        if (inheritGradient) {
            if (clip.from.gradient == authoredGradient) clip.from.gradient = source.gradient;
            if (clip.to.gradient == authoredGradient) clip.to.gradient = source.gradient;
            if (_equal(clip.from.gradientEnd, authoredGradientEnd)) {
                clip.from.gradientEnd = source.gradientEnd;
            }
            if (_equal(clip.to.gradientEnd, authoredGradientEnd)) {
                clip.to.gradientEnd = source.gradientEnd;
            }
        }
    }
    entry.fingerprint = fingerprint(entry.object);
}

// Whether the points of two shapes can travel from one to the other: as many of them, in the same
// pieces.
static bool _alike(const GeometryShape& from, const GeometryShape& to)
{
    if (from.count != to.count || from.pieces != to.pieces) return false;
    return from.pieces < 2u || std::memcmp(from.ends, to.ends, sizeof(uint32_t) * from.pieces) == 0;
}

static VisualState _lerp(const VisualState& from, const VisualState& to, float progress)
{
    auto bounded = _clamp(progress);
    VisualState state;
    state.model = _lerp(from.model, to.model, progress);
    state.stroke = _lerp(from.stroke, to.stroke, bounded);
    state.fill = _lerp(from.fill, to.fill, bounded);
    state.gradientEnd = _lerp(from.gradientEnd, to.gradientEnd, bounded);
    state.width = static_cast<float>(static_cast<double>(from.width) + (static_cast<double>(to.width) - from.width) * bounded);
    state.radius = static_cast<float>(static_cast<double>(from.radius) + (static_cast<double>(to.radius) - from.radius) * bounded);
    state.dashOffset = static_cast<float>(static_cast<double>(from.dashOffset) + (static_cast<double>(to.dashOffset) - from.dashOffset) * bounded);
    state.markerTail = static_cast<float>(static_cast<double>(from.markerTail) + (static_cast<double>(to.markerTail) - from.markerTail) * bounded);
    state.markerTip = static_cast<float>(static_cast<double>(from.markerTip) + (static_cast<double>(to.markerTip) - from.markerTip) * bounded);
    state.opacity = static_cast<float>(static_cast<double>(from.opacity) + (static_cast<double>(to.opacity) - from.opacity) * bounded);
    state.progress = static_cast<float>(static_cast<double>(from.progress) + (static_cast<double>(to.progress) - from.progress) * bounded);
    state.start = static_cast<float>(static_cast<double>(from.start) + (static_cast<double>(to.start) - from.start) * bounded);
    state.fillProgress = static_cast<float>(static_cast<double>(from.fillProgress) + (static_cast<double>(to.fillProgress) - from.fillProgress) * bounded);
    state.pixelScale = static_cast<float>(static_cast<double>(from.pixelScale) + (static_cast<double>(to.pixelScale) - from.pixelScale) * progress);
    if (state.pixelScale < 0.0f) state.pixelScale = 0.0f;
    state.gradient = mix(from.gradient, to.gradient, bounded);
    // Sizes follow the curve past its ends, so a back or elastic curve overshoots them.
    auto extent = [progress](float a, float b) {
        auto value = static_cast<float>(static_cast<double>(a) + (static_cast<double>(b) - a) * progress);
        return value < 0.0f ? 0.0f : value;
    };
    state.size = {extent(from.size.x, to.size.x), extent(from.size.y, to.size.y)};
    state.arcStart = static_cast<float>(static_cast<double>(from.arcStart) + (static_cast<double>(to.arcStart) - from.arcStart) * progress);
    state.arcAngle = static_cast<float>(static_cast<double>(from.arcAngle) + (static_cast<double>(to.arcAngle) - from.arcAngle) * progress);
    state.corner = extent(from.corner, to.corner);
    state.trimDistance = {extent(from.trimDistance.x, to.trimDistance.x), extent(from.trimDistance.y, to.trimDistance.y)};
    auto number = [bounded](float a, float b) {
        return static_cast<float>(static_cast<double>(a) + (static_cast<double>(b) - a) * bounded);
    };
    state.blur = number(from.blur, to.blur);
    state.shadow.color = _lerp(from.shadow.color, to.shadow.color, bounded);
    state.shadow.offset = {number(from.shadow.offset.x, to.shadow.offset.x),
                           number(from.shadow.offset.y, to.shadow.offset.y)};
    state.shadow.blur = number(from.shadow.blur, to.shadow.blur);
    // A count follows the curve past its ends, as sizes do, and may turn negative.
    state.value = static_cast<float>(static_cast<double>(from.value) + (static_cast<double>(to.value) - from.value) * progress);
    // A text change never passes either string.
    state.text = to.text;
    state.textFrom = from.text;
    state.textProgress = from.text == to.text ? 1.0f : bounded;
    state.textChange = to.textChange;
    // A layer changes at a step: the higher of the two holds until the clip ends.
    state.layer = from.layer > to.layer ? from.layer : to.layer;
    // Function columns blend on a matching grid even when the clipped pieces differ.
    // Incompatible function states and different point pieces switch halfway; ordinary
    // points follow the curve past its ends, as the model does.
    auto blended = from.geometry && to.geometry && sameGrid(*from.geometry, *to.geometry);
    if (from.geometry && to.geometry && !blended
        && (from.geometry->samples.count || to.geometry->samples.count || !_alike(*from.geometry, *to.geometry))) {
        state.geometry = bounded < 0.5f ? from.geometry : to.geometry;
        state.geometryFrom = state.geometry;
        state.geometryProgress = 1.0f;
    } else {
        state.geometry = to.geometry;
        state.geometryFrom = from.geometry;
        state.geometryProgress = from.geometry == to.geometry ? 1.0f : blended ? bounded : progress;
    }
    state.traceFill = bounded < 1.0f ? from.traceFill : to.traceFill;
    state.direction = bounded < 1.0f ? from.direction : to.direction;
    // A warp's points travel straight, past their images too when the curve overshoots.
    state.warp = to.warp;
    state.warpProgress = from.warp == to.warp ? to.warpProgress : progress;
    return state;
}

static void _apply(VisualState& state, const VisualState& source, uint32_t channels)
{
    if (channels & CHANNEL_MODEL) state.model = source.model;
    if (channels & CHANNEL_OPACITY) state.opacity = source.opacity;
    if (channels & CHANNEL_STROKE) state.stroke = source.stroke;
    if (channels & CHANNEL_FILL) state.fill = source.fill;
    if (channels & CHANNEL_DASH_OFFSET) state.dashOffset = source.dashOffset;
    if (channels & CHANNEL_TAIL) state.markerTail = source.markerTail;
    if (channels & CHANNEL_TIP) state.markerTip = source.markerTip;
    if (channels & CHANNEL_PROGRESS) {
        state.progress = source.progress;
        state.start = source.start;
        state.direction = source.direction;
        state.traceFill = source.traceFill;
    }
    if (channels & CHANNEL_FILL_PROGRESS) state.fillProgress = source.fillProgress;
    if (channels & CHANNEL_PIXEL_SCALE) state.pixelScale = source.pixelScale;
    if (channels & CHANNEL_WIDTH) state.width = source.width;
    if (channels & CHANNEL_RADIUS) state.radius = source.radius;
    if (channels & CHANNEL_GRADIENT) {
        state.gradient = source.gradient;
        state.gradientEnd = source.gradientEnd;
    }
    if (channels & CHANNEL_MORPH) {
        state.morph = source.morph;
        state.morphShape = source.morphShape;
        state.morphProgress = source.morphProgress;
        state.warp = source.warp;
        state.warpProgress = source.warpProgress;
    }
    if (channels & CHANNEL_ARC_START) state.arcStart = source.arcStart;
    if (channels & CHANNEL_ARC_ANGLE) state.arcAngle = source.arcAngle;
    if (channels & CHANNEL_SIZE) state.size = source.size;
    if (channels & CHANNEL_CORNER) state.corner = source.corner;
    if (channels & CHANNEL_TRIM_DISTANCE) state.trimDistance = source.trimDistance;
    if (channels & CHANNEL_BLUR) state.blur = source.blur;
    if (channels & CHANNEL_SHADOW) state.shadow = source.shadow;
    if (channels & CHANNEL_VALUE) state.value = source.value;
    if (channels & CHANNEL_TEXT) {
        state.text = source.text;
        state.textFrom = source.textFrom;
        state.textProgress = source.textProgress;
        state.textChange = source.textChange;
    }
    if (channels & CHANNEL_LAYER) state.layer = source.layer;
    if (channels & CHANNEL_GEOMETRY) {
        state.geometry = source.geometry;
        state.geometryFrom = source.geometryFrom;
        state.geometryProgress = source.geometryProgress;
    }
}

// Latest end among the clips already authored on these channels of one object.
static float _channelEnd(const ObjectEntry& entry, uint32_t channels)
{
    auto end = 0.0f;
    for (auto i = 0u; i < CHANNEL_COUNT; i++) {
        if ((channels & (1u << i)) && entry.channelEnd[i] > end) end = entry.channelEnd[i];
    }
    return end;
}

static void _channelEnd(ObjectEntry& entry, uint32_t channels, float end)
{
    for (auto i = 0u; i < CHANNEL_COUNT; i++) {
        if ((channels & (1u << i)) && entry.channelEnd[i] < end) entry.channelEnd[i] = end;
    }
}

static constexpr auto TIMELINE_OVERLAP =
    "the object already animates this property until a later time; animate one property "
    "of an object in time order";
static constexpr auto TIMELINE_UNBORN = "the object is added after the animation starts";
static constexpr auto TIMELINE_HEIR =
    "a shape that takes its color from the group already animates that color until a later "
    "time; animate the group after it or set the shape's own color";
static constexpr auto TIMELINE_FLAT =
    "the motion the object plays flattens it, so only a shift can add to it; begin when that "
    "motion ends";

// The property each channel animates, in words.
static constexpr const char* CHANNEL_NAMES[] = {
    "transform", "opacity", "stroke color", "fill color", "dash offset", "tail", "tip",
    "drawing progress", "fill progress", "effect scale", "stroke width", "radius", "gradient",
    "shape", "size", "corner", "blur", "shadow", "counted value", "text", "layer", "geometry", "trim distance", "arc start", "arc angle"};
static_assert(sizeof(CHANNEL_NAMES) / sizeof(CHANNEL_NAMES[0]) == CHANNEL_COUNT, "every channel needs a name");

// Fails a clip of these channels that would begin at time inside an earlier clip of the object,
// naming the first property it shares, the time that property is busy until and the remedy.
// Impl is the scene's implementation, which only scene members may name.
template<typename Impl>
static Result _overlap(Impl& impl, const ObjectEntry& entry, uint32_t channels, float time,
                       const char* remedy = "begin at that time or later")
{
    auto channel = 0u;
    while (channel < CHANNEL_COUNT && (!(channels & (1u << channel)) || entry.channelEnd[channel] <= time)) {
        channel++;
    }
    if (channel == CHANNEL_COUNT) return impl.fail(Result::InsufficientCondition, TIMELINE_OVERLAP);
    auto tag = entry.object->tag();
    auto named = tag && tag[0];
    std::snprintf(impl.reason, sizeof(impl.reason), "%s%.48s%s already animates its %s until %.2f s; %s",
                  named ? "'" : "the object", named ? tag : "", named ? "'" : "", CHANNEL_NAMES[channel],
                  static_cast<double>(entry.channelEnd[channel]), remedy);
    return impl.fail(Result::InsufficientCondition, impl.reason);
}

static constexpr auto MOTION_OUTLINE =
    "the path must be a line, arrow, vector, circle, rectangle, polygon, plot of one piece, route, "
    "path of one contour, curve or connector with a visible length";
static constexpr auto MOTION_LIMIT =
    "the scene follows too many path points; reuse one path object or simplify it";
static constexpr auto MOTION_BOUNDS = "the object has no bounds to turn, scale or travel about";
static constexpr auto MOTION_SPACE = "the object's parent transform cannot be inverted";
static constexpr auto MOTION_RANGE = "the motion leaves the representable coordinate range";
static constexpr auto MOTION_ARC = "the arc must be finite and between -2*pi and 2*pi radians";
static constexpr auto MOTION_AXIS = "the axis must be a finite, non-zero vector";
static constexpr auto MOTION_VALUES = "the pivot, shift, angle and scale factors must be finite";
static constexpr auto MOTION_SELF = "an object cannot move along itself";
static constexpr auto MOTION_PATH = "moving along a path needs the path object";
static constexpr auto MOTION_FRACTION = "from and to must be fractions of the path length between 0 and 1";
static constexpr auto SCENE_MEMORY = "out of memory";
static constexpr auto PLAY_EMPTY = "there is nothing to animate";
static constexpr auto PLAY_DURATION = "the duration must be a positive number of seconds";
static constexpr auto PLAY_LAG = "the lag must be zero or positive";
static constexpr auto PLAY_GROUP = "one call animates at most 4096 objects";
static constexpr auto PLAY_CURVE = "the animation curve is invalid";
static constexpr auto PLAY_TWICE = "an object can take part only once in one call";
static constexpr auto PLAY_KIND = "the animation kind is unknown";
static constexpr auto PLAY_RELATED = "this animation does not take a second object";
static constexpr auto PLAY_COPY = "only a morph can play from a copy of its object";
static constexpr auto TARGET_FOREIGN = "the object is not part of this scene";
static constexpr auto TARGET_REMOVED = "the object was removed earlier in the timeline";
static constexpr auto TARGET_INVALID = "the object has invalid geometry or style values";
static constexpr auto TARGET_CHANGED =
    "the object was edited directly after it was added; animate the change instead";
static constexpr auto TARGET_BELOW_CHANGED =
    "an object below the target was edited directly after it was added; animate the change instead";
static constexpr auto TARGET_HEIR_CHANGED =
    "a shape that takes its color from the group was edited directly after it was added; "
    "animate the change instead";
static constexpr auto TARGET_FIXED =
    "a connector follows its endpoints, and a moving group must contain an endpoint of each "
    "connector inside it";
static constexpr auto TARGET_MATRIX = "the matrix must be finite and affine, with a last row of 0, 0, 0, 1";
static constexpr auto VALUE_NONE = "the animation changes no property";
static constexpr auto VALUE_OPACITY = "the opacity must be between 0 and 1";
static constexpr auto VALUE_PROGRESS = "the draw progress must be between 0 and 1";
static constexpr auto VALUE_DASH = "the dash offset needs a finite value and a stroke with a dash pattern";
static constexpr auto VALUE_MARKER = "tail and tip need an arrow, vector, connector or route";
static constexpr auto VALUE_MARKER_SIZE = "tail and tip lengths must be zero or positive";
static constexpr auto VALUE_WIDTH = "the stroke width must be a finite number of pixels, zero or more";
static constexpr auto VALUE_WIDTH_OBJECT = "text, pictures and cells have no stroke width to animate";
static constexpr auto VALUE_RADIUS = "radius needs a circle, arc or angle (world units) or a point (pixels) and a positive value";
static constexpr auto VALUE_SIZE =
    "size needs a rectangle with a positive width and height, or a text with one positive size";
static constexpr auto VALUE_CORNER =
    "corner needs a rectangle and a value between 0 and half the rectangle's shorter side";
static constexpr auto VALUE_BLUR = "the blur must be between 0 and 256 pixels";
static constexpr auto VALUE_SHADOW = "the shadow needs a finite offset and a blur between 0 and 256 pixels";
static constexpr auto VALUE_COUNT = "value needs a counting text and a finite number";
static constexpr auto VALUE_TEXT =
    "text needs a text object and a string; a counting text changes through its value instead";
static constexpr auto VALUE_TEXT_CHANGE = "the text change is unknown";
static constexpr auto VALUE_LAYER =
    "the objects below the target move by as many layers as it does, and a layer that moves "
    "must stay within the range of a 32-bit integer";
static constexpr auto VALUE_GEOMETRY =
    "geometry needs a line, arrow, vector, polygon, plot, route, path of one contour, curve, arc or angle";
static constexpr auto VALUE_GEOMETRY_COUNT =
    "the geometry must keep the object's point count: 2 for a line, arrow or vector, 4 for a "
    "curve, 1 for an arc, 3 for an angle, and as many as a polygon, plot, route or path has";
static constexpr auto VALUE_GEOMETRY_POINTS = "the geometry points must be finite";
static constexpr auto VALUE_GEOMETRY_PIECES =
    "only a plot takes pieces with its points; other shapes keep the point count they have";
static constexpr auto VALUE_GEOMETRY_ENDS =
    "the pieces must follow one another through the points, each holding two points or more and "
    "the last ending at the point count, and no points take no pieces";
static constexpr auto GEOMETRY_LIMIT =
    "the scene reshapes too many points; reshape fewer or simpler shapes, or less often";
static constexpr auto VALUE_GRADIENT =
    "the gradient must be valid and keep the current gradient's type, stop count and explicit "
    "geometry, so give the shape a gradient of that form when it is created";
static constexpr auto VALUE_GRADIENT_END =
    "an end color animates a two-stop gradient; a gradient with stops animates its stop colors "
    "through a gradient of the same form";
static constexpr auto VALUE_GRADIENT_COLOR =
    "a gradient with stops sets this shape's colors, so stroke and fill can change only their "
    "alpha; animate gradient instead";
static constexpr auto DRAW_DIRECTION = "the draw direction is unknown";
static constexpr auto EFFECT_SCALE = "the effect scale must be a positive number";
static constexpr auto EFFECT_INDICATE = "the indicate scale must be greater than 1";
static constexpr auto EFFECT_WIGGLE =
    "the wiggle needs a positive scale, a finite angle and a positive number of swings";
static constexpr auto EFFECT_BOUNDS = "the object has no bounds to grow, shrink, scale or move about";
static constexpr auto MORPH_TARGET = "morph needs a second, different object as its target";
static constexpr auto MORPH_KIND =
    "morph needs a line, circle, rectangle, polygon, plot, path or curve on both sides, or two "
    "surfaces";
static constexpr auto MORPH_GRID = "morph needs two surfaces with the same columns and rows of points";
static constexpr auto MORPH_COLORS =
    "morph colors need two surfaces, 1 to 64 steps and one color per point at each step";
static constexpr auto MORPH_CLOSED =
    "morph outlines must both be closed (circle, rectangle, polygon) or both be open (line, plot, "
    "curve)";
static constexpr auto MORPH_OUTLINE = "morph needs outlines with a visible length on both sides";
static constexpr auto MORPH_PAIRS = "morph pairs need distinct piece indices within each outline";
static constexpr auto MORPH_LIMIT = "the scene morphs too many outline points; simplify the shapes";
static constexpr auto MORPH_CHILDREN = "objects with children cannot morph";
static constexpr auto MORPH_PARENT = "the morph source and target must have the same parent";
static constexpr auto MORPH_FRESH =
    "add the morph target right before the morph, without animations of its own";
static constexpr auto MORPH_CONNECTED = "objects attached to a connector cannot morph";
static constexpr auto MORPH_EARLY = "a morph that begins before then must end at that time or later";
static constexpr auto WARP_MAP = "the warp needs a function that maps points";
static constexpr auto WARP_STEP = "the warp step must be a positive length";
static constexpr auto WARP_TWICE = "a warp takes each object once, and no target below another";
static constexpr auto WARP_KIND = "a warp cannot bend cells or surfaces; warp the objects beside them instead";
static constexpr auto WARP_UNBORN =
    "an object below the target is added after the warp begins; warp it once it is shown";
static constexpr auto WARP_SPACE =
    "a warp maps images back through the transforms of the objects it moves, and one of them "
    "cannot be inverted";
static constexpr auto WARP_LIMIT = "the scene warps too many points; raise the step or warp fewer objects";
static constexpr auto WARP_CHANGED = "a warp function cannot change the scene";
static constexpr auto WARP_SIZE =
    "a warped circle or rectangle keeps the outline its warp gave, so its radius, size and corner "
    "no longer change";
static constexpr auto WARP_GEOMETRY =
    "a warped shape keeps the outline its warp gave, so the points that define it no longer "
    "change; add a new shape for new points";
static constexpr auto FADE_PAIR = "the cross-fade needs two separate objects, neither inside the other";
static constexpr auto FADE_FRESH =
    "add the replacing object right before the cross-fade, without animations of its own";
static constexpr auto MATCH_PAIR = "the match needs two separate objects, neither inside the other";
static constexpr auto MATCH_SOURCE = "the matched object and its pieces must be at rest at the cursor";
static constexpr auto MATCH_FRESH =
    "add the object to match into right before the match, without animations of its own";
static constexpr auto MATCH_PIECES = "one match takes at most 4096 pieces of each object";
static constexpr auto MATCH_PAIRS =
    "each pair needs a piece of each object, and a piece joins at most one pair";
static constexpr auto MATCH_SPACE = "a piece's parent transform cannot be inverted";
static constexpr auto MATCH_LINES = "a diff takes at most 4096 lines of each object";
static constexpr auto MATCH_FADE = "the fade share must be greater than 0 and at most 1";
static constexpr auto MATCH_PATH_ARC = "path and arc cannot be used together";
static constexpr auto MATCH_PATH_LIVE = "a match path must be a snapshot outline, not a live connector";
static constexpr auto HIGHLIGHT_SELECTION = "a highlight selects the target or objects below it";
static constexpr auto HIGHLIGHT_DIM = "the dim opacity must be between 0 and 1";
static constexpr auto ARRANGE_ITEMS = "arrange takes distinct objects that share one parent";
static constexpr auto ARRANGE_BOUNDS = "arrange needs objects with bounds; give each geometry or children first";
static constexpr auto ARRANGE_DIRECTION = "the arrange direction must be a finite, non-zero vector";
static constexpr auto ARRANGE_GAP = "the arrange gap must be zero or positive";
static constexpr auto ARRANGE_CENTER = "the arrange center must be a finite point";
static constexpr auto SAVE_EMPTY = "there is nothing to save";
static constexpr auto SAVE_GROUP = "one save records at most 4096 objects, the targets and the objects below them";
static constexpr auto RESTORE_EMPTY = "there is nothing to restore";
static constexpr auto RESTORE_DURATION = "the duration must be zero or a positive number of seconds";
static constexpr auto RESTORE_GROUP =
    "one restore brings back at most 4096 objects, the targets and the objects below them";
static constexpr auto EMPHASIS_VALUES =
    "the emphasis needs a known kind and shape, a positive width and size, a padding of zero or "
    "more, a window greater than 0 and at most 1, from 1 to 64 lines and a finite point";
static constexpr auto EMPHASIS_EMPTY = "the target has no geometry at the cursor to emphasize";
static constexpr auto EMPHASIS_OUTLINE =
    "the target has no outline of one stroke at the cursor for a passing flash to run along";
static constexpr auto REMOVE_GONE = "the object is not in the scene or was already removed";
static constexpr auto LAYOUT_SEALED =
    "layout and direct edits must come before the first play, wait, look or remove; animate the "
    "change instead";
static constexpr auto LAYOUT_ANIMATED = "the object already animates; animate the change instead";
static constexpr auto LAYOUT_ARC = "the values apply at once, so there is no travel for an arc to carry";
static constexpr auto LAYOUT_HEIR_ANIMATED =
    "a shape that takes its color from the group already animates; animate the change instead";
static constexpr auto LAYOUT_ENDPOINTS = "the connector's endpoints are no longer in the scene";
static constexpr auto ADD_OWNED = "the object already belongs to a scene or a parent";
static constexpr auto ADD_PARENT = "the parent is not part of this scene";
static constexpr auto ADD_DEPTH = "objects nest at most 64 levels deep";
static constexpr auto ADD_COUNT = "a scene holds at most 1048576 objects";
static constexpr auto ADD_TAG = "another object in the scene already uses this id";
static constexpr auto ADD_ENDPOINT = "a connector's endpoints must be in the scene or added with it";
static constexpr auto ADD_CONNECTOR =
    "a connector inside a moved group needs at least one endpoint inside that group";
static constexpr auto CAMERA_MODE = "only a fixed camera can move; set the camera mode to fixed";
static constexpr auto CAMERA_POSE =
    "the camera move leaves the supported range: keep the eye away from the target, the up "
    "vector off the view direction, and near and far apart";
static constexpr auto ORBIT_VIEW = "only a 3D camera orbits; use look to pan or zoom a 2D camera";
static constexpr auto ORBIT_ANGLE = "the orbit angle must be a finite, non-zero number of radians";
static constexpr auto ORBIT_AXIS = "the orbit axis must be a finite, non-zero vector";
static constexpr auto VIEWPORT_SCENE = "a scene can be shown once, by another scene, outside its own viewports";
static constexpr auto VIEWPORT_RECT = "the viewport must lie inside the unit square with a positive size";
static constexpr auto VIEWPORT_LIMIT = "viewports nest at most 16 scenes deep and 64 per scene";
static constexpr auto VIEW_INDEX = "the scene has no view with this index";
static constexpr auto VIEW_RECT = "the view must lie inside the unit square with a positive size";
static constexpr auto VIEW_OPACITY = "the opacity of a view must be between 0 and 1";
static constexpr auto VIEW_OBJECT = "the objects a view follows or hides must be part of this scene";
static constexpr auto VIEW_LIMIT = "a scene holds at most 64 views, each hiding at most 256 objects";
static constexpr auto VIEW_UNBORN = "the view is made after this time";
static constexpr auto TRANSITION_SCENES = "a transition needs 2 to 64 distinct scenes that are not shown elsewhere";
static constexpr auto TRANSITION_TIMING = "the transition needs a positive duration, a non-negative hold and a valid curve";
static constexpr auto TRANSITION_TWICE = "the scene already has a transition";
static constexpr auto TRANSITION_STYLES =
    "give one transition style for every transition, or one for each transition in order";
static constexpr auto TRANSITION_STYLE =
    "a transition style must be Match, Fade, Slide, ZoomIn or ZoomOut";
static constexpr auto TRANSITION_SIDE = "a slide comes from the Left, Right, Bottom or Top";
static constexpr auto TRANSITION_AREA =
    "a zoom needs an area of positive size that overlaps the viewport and fits in it once "
    "widened to its shape";
static constexpr auto TRANSITION_OBJECT =
    "a zoom object must be in one of the two scenes of its transition and cover an area at "
    "the end of its scene";
static constexpr auto TRANSITION_MATCH = "only a slide or a zoom also matches";
static constexpr auto TRANSITION_MARGIN = "area margin must be finite and non-negative";
static constexpr auto TRANSITION_MARGIN_OBJECT = "area margin applies only to an object zoom area";

// Why an object cannot take part in a timeline call now, or nullptr when it can. Objects that
// are only read, like pivots and motion paths, may already be removed.
static const char* _unplayable(const ObjectEntry* entry, const Object* object, bool read = false)
{
    if (!entry) return TARGET_FOREIGN;
    if (!read && entry->dead >= 0.0f) return TARGET_REMOVED;
    if (!valid(object)) return TARGET_INVALID;
    if (entry->fingerprint != fingerprint(object)) return TARGET_CHANGED;
    return nullptr;
}

// ---------- Motion paths ----------

static constexpr uint32_t MAX_PATH_VERTICES = 2048;
static constexpr uint32_t MAX_MOTION_VERTICES = 262144;

struct MotionVertex
{
    Vec3 point;
    // Arc length from the first vertex.
    float length = 0.0f;
    // Orientation of the segment that starts here, parallel-transported from the first one.
    Rotation turn;
};

// An ordinary outline copied into a follower's parent space when authored, or a live
// connector reference. Only connector paths read parent at each sample.
struct MotionPath
{
    MotionVertex* vertices = nullptr;
    const Object* source = nullptr;
    const Object* parent = nullptr;
    uint64_t fingerprint = 0;
    Mat4 space = Mat4::identity();
    // Closed paths: the last segment one loop earlier and the first segment a loop later.
    Rotation before;
    Rotation after;
    uint32_t count = 0;
    bool closed = false;
    bool relative = false;
};

static bool _direction(const Vector64& from, const Vector64& to, Vector64& direction)
{
    return _normalize(to.x - from.x, to.y - from.y, to.z - from.z, direction);
}

// Builds an arc-length parameterized polyline from the outline of source mapped by space.
// Dense outlines are resampled to MAX_PATH_VERTICES at even arc lengths.
static Result _motionPath(const Object* source, const Mat4& space, MotionPath& path, bool relative)
{
    auto closed = false;
    auto count = outline(source, nullptr, 0u, closed);
    if (count < 2u || count == UINT32_MAX) return Result::InvalidArguments;
    auto raw = new (std::nothrow) Vec3[count];
    auto points = new (std::nothrow) Vector64[count + 1u];
    auto lengths = new (std::nothrow) double[count + 1u];
    if (!raw || !points || !lengths) {
        delete[] raw;
        delete[] points;
        delete[] lengths;
        return Result::OutOfMemory;
    }
    outline(source, raw, count, closed);
    // Match guides contribute only their detour, which is invariant under translation.
    // Cancel it before narrowing so a distant guide keeps its small local bends.
    auto mapped = space;
    if (relative) mapped.e[3] = mapped.e[7] = mapped.e[11] = 0.0f;
    auto origin = _point(mapped, raw[0]);
    auto extent = 0.0;
    for (auto i = 0u; i < count; i++) {
        points[i] = _point(mapped, raw[i]);
        if (relative) points[i] = {points[i].x - origin.x, points[i].y - origin.y, points[i].z - origin.z};
        extent = std::fmax(extent, std::fmax(std::fabs(points[i].x),
                                             std::fmax(std::fabs(points[i].y), std::fabs(points[i].z))));
    }
    delete[] raw;
    auto tolerance = extent * 1.0e-9;
    auto unique = _parameterize(points, count, closed, tolerance, lengths);
    auto total = lengths[unique - 1u];
    if (!std::isfinite(extent) || unique < 2u || !std::isfinite(total) || total <= 0.0
        || total > std::numeric_limits<float>::max()) {
        delete[] points;
        delete[] lengths;
        return Result::InvalidArguments;
    }
    auto vertexCount = unique < MAX_PATH_VERTICES ? unique : MAX_PATH_VERTICES;
    auto vertices = new (std::nothrow) MotionVertex[vertexCount];
    auto resampled = new (std::nothrow) Vector64[vertexCount];
    if (!vertices || !resampled) {
        delete[] points;
        delete[] lengths;
        delete[] vertices;
        delete[] resampled;
        return Result::OutOfMemory;
    }
    if (vertexCount == unique) {
        for (auto i = 0u; i < unique; i++)
            resampled[i] = points[i];
    } else {
        auto segment = 0u;
        for (auto i = 0u; i < vertexCount; i++) {
            auto distance = total * static_cast<double>(i) / static_cast<double>(vertexCount - 1u);
            while (segment + 2u < unique && lengths[segment + 1u] < distance) segment++;
            auto span = lengths[segment + 1u] - lengths[segment];
            auto t = span > 0.0 ? std::fmin(std::fmax((distance - lengths[segment]) / span, 0.0), 1.0) : 0.0;
            auto& a = points[segment];
            auto& b = points[segment + 1u];
            resampled[i] = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
        }
        resampled[vertexCount - 1u] = points[unique - 1u];
    }
    delete[] points;
    delete[] lengths;

    // Parallel transport: each segment turns from the previous one by the shortest turn.
    auto length = 0.0;
    Vector64 previous;
    Vector64 first;
    auto valid = true;
    for (auto i = 0u; i < vertexCount && valid; i++) {
        auto& vertex = vertices[i];
        valid = _narrow(resampled[i], vertex.point);
        if (i) length += _distance(resampled[i - 1u], resampled[i]);
        vertex.length = static_cast<float>(length);
        if (i + 1u == vertexCount) {
            vertex.turn = vertices[i - 1u].turn;
            break;
        }
        Vector64 direction;
        if (!_direction(resampled[i], resampled[i + 1u], direction)) direction = i ? previous : Vector64{1.0, 0.0, 0.0};
        if (i == 0u) {
            first = direction;
        } else {
            vertex.turn = _multiply(_turn(previous, direction), vertices[i - 1u].turn);
            _normalize(vertex.turn);
        }
        previous = direction;
    }
    delete[] resampled;
    if (!valid) {
        delete[] vertices;
        return Result::InvalidArguments;
    }
    path.closed = closed;
    if (closed) {
        auto wrap = _turn(previous, first);
        path.before = _conjugate(wrap);
        path.after = _multiply(wrap, vertices[vertexCount - 1u].turn);
        _normalize(path.after);
    }
    path.vertices = vertices;
    path.count = vertexCount;
    return Result::Success;
}

// The stretch of a path an animation travels, as arc lengths. A loop travels a whole closed
// path from end to end.
struct MotionSpan
{
    double low = 0.0;
    double high = 0.0;
    bool forward = true;
    bool loop = false;
};

static double _segment(const MotionVertex* vertices, uint32_t index)
{
    return static_cast<double>(vertices[index + 1u].length) - vertices[index].length;
}

// Orientation while distance is within the turning stretch around a vertex. Corners turn
// over a short stretch on both sides instead of snapping, but never before the travel
// starts or after it ends, so an object departs and arrives along a segment. A loop
// finishes its last corner on arrival and so ends in its starting heading.
static bool _corner(const MotionPath& path, uint32_t vertex, const MotionSpan& span,
                    double distance, Rotation& orientation)
{
    auto vertices = path.vertices;
    auto last = path.count - 1u;
    if (!path.closed && (vertex == 0u || vertex == last)) return false;
    auto before = vertex > 0u ? _segment(vertices, vertex - 1u) : _segment(vertices, last - 1u);
    auto after = vertex < last ? _segment(vertices, vertex) : _segment(vertices, 0u);
    auto& incoming = vertex > 0u ? vertices[vertex - 1u].turn : path.before;
    auto& outgoing = vertex < last ? vertices[vertex].turn : path.after;
    auto at = static_cast<double>(vertices[vertex].length);
    auto radius = std::fmin(std::fmin(before, after) * 0.5, static_cast<double>(vertices[last].length) * 0.05);
    auto weight = 0.0;
    if (span.loop && vertex == (span.forward ? last : 0u)) {
        if (!(radius > 0.0) || std::fabs(distance - at) >= radius) return false;
        weight = span.forward ? 1.0 - (at - distance) / radius : (distance - at) / radius;
    } else {
        radius = std::fmin(radius, std::fmin(at - span.low, span.high - at));
        if (!(radius > 0.0) || std::fabs(distance - at) >= radius) return false;
        weight = 0.5 + 0.5 * (distance - at) / radius;
    }
    if (!_lerp(incoming, outgoing, static_cast<float>(weight), orientation)) orientation = outgoing;
    return true;
}

// Position at arc length distance along the path and, when asked, the orientation there.
// At a vertex the segment inside the travelled span counts, so the heading at either end
// of the travel is the heading of the segment travelled there.
static void _follow(const MotionPath& path, double distance, const MotionSpan& span,
                    Vector64& position, Rotation* orientation)
{
    auto vertices = path.vertices;
    auto last = path.count - 1u;
    auto total = static_cast<double>(vertices[last].length);
    distance = std::fmin(std::fmax(distance, 0.0), total);
    auto low = 0u;
    auto high = last;
    while (high - low > 1u) {
        auto middle = low + (high - low) / 2u;
        if (vertices[middle].length <= distance) low = middle;
        else high = middle;
    }
    if (low > 0u && distance == static_cast<double>(vertices[low].length) && distance >= span.high
        && span.high > span.low) {
        low--;
    }
    auto& a = vertices[low];
    auto& b = vertices[low + 1u];
    auto length = static_cast<double>(b.length) - a.length;
    auto t = length > 0.0 ? std::fmin(std::fmax((distance - a.length) / length, 0.0), 1.0) : 0.0;
    position = {a.point.x + (static_cast<double>(b.point.x) - a.point.x) * t,
                a.point.y + (static_cast<double>(b.point.y) - a.point.y) * t,
                a.point.z + (static_cast<double>(b.point.z) - a.point.z) * t};
    if (!orientation) return;
    if (_corner(path, low, span, distance, *orientation)) return;
    if (_corner(path, low + 1u, span, distance, *orientation)) return;
    *orientation = a.turn;
}

Result Scene::Impl::follow(const Object* source, const Object* follower, uint32_t& index, bool relative) noexcept
{
    auto& impl = *this;
    auto parent = Mat4::identity();
    if (follower && !_inverse(_parentWorld(follower), parent)) return impl.fail(Result::InvalidArguments, MOTION_SPACE);
    if (source->type() == Type::Connector) {
        if (!grow(impl.paths, impl.pathCnt, impl.pathCap, 1u, 4u)) return Result::OutOfMemory;
        MotionPath path;
        path.source = source;
        path.parent = follower->parent();
        index = impl.pathCnt;
        impl.paths[impl.pathCnt++] = path;
        return Result::Success;
    }
    auto space = parent * _parentWorld(source) * source->model;
    if (!_affine(space)) return impl.fail(Result::InvalidArguments, MOTION_RANGE);
    auto print = fingerprint(source);
    for (auto i = 0u; i < impl.pathCnt; i++) {
        auto& path = impl.paths[i];
        if (path.source == source && path.fingerprint == print && path.relative == relative && _equal(path.space, space)) {
            index = i;
            return Result::Success;
        }
    }
    if (!grow(impl.paths, impl.pathCnt, impl.pathCap, 1u, 4u)) return Result::OutOfMemory;
    MotionPath path;
    auto result = _motionPath(source, space, path, relative);
    if (result == Result::InvalidArguments) return impl.fail(result, MOTION_OUTLINE);
    if (result != Result::Success) return result;
    if (path.count > MAX_MOTION_VERTICES - impl.pathVertices) {
        delete[] path.vertices;
        return impl.fail(Result::InsufficientCondition, MOTION_LIMIT);
    }
    path.source = source;
    path.fingerprint = print;
    path.space = space;
    path.relative = relative;
    impl.pathVertices += path.count;
    index = impl.pathCnt;
    impl.paths[impl.pathCnt++] = path;
    return Result::Success;
}

// Discards paths copied by a play call that failed before it added its clips.
void Scene::Impl::dropPaths(uint32_t count, uint32_t vertices) noexcept
{
    for (auto i = count; i < pathCnt; i++) {
        delete[] paths[i].vertices;
        paths[i] = {};
    }
    pathCnt = count;
    pathVertices = vertices;
}

// ---------- Morph pairs and geometry ownership ----------

Result Scene::Impl::pair(const Animation& animation, uint32_t& index) noexcept
{
    auto& impl = *this;
    if (!grow(impl.morphs, impl.morphCnt, impl.morphCap, 1u, 4u)) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    MorphShape shape;
    auto multi = animation.target->type() != Type::SurfaceMesh
                 && (_morphPieceCount(animation.target) != 1u || _morphPieceCount(animation.related) != 1u);
    auto result = multi ? pairMorphPieces(animation, shape) : pairMorph(animation.target, animation.related, shape);
    // The sampled colors of a surface count against the points a scene may morph, three colors
    // to the room of a point.
    auto weight = shape.count;
    if (result == Result::Success && animation.colorSteps) {
        const Color* sides[2] = {animation.sourceColors, animation.targetColors};
        for (auto side = 0u; side < 2u && result == Result::Success; side++) {
            if (!sides[side]) continue;
            shape.colors[side] = new (std::nothrow) Color[animation.colorCount];
            if (!shape.colors[side]) {
                result = Result::OutOfMemory;
                break;
            }
            std::memcpy(shape.colors[side], sides[side], sizeof(Color) * animation.colorCount);
            weight += (animation.colorCount + 2u) / 3u;
        }
        shape.steps = animation.colorSteps;
    }
    if (result == Result::Success && weight > MAX_MORPH_POINTS - impl.morphPoints) {
        result = Result::InsufficientCondition;
    }
    if (result != Result::Success) {
        _freeMorph(shape);
        if (result == Result::InvalidArguments) return impl.fail(result, MORPH_OUTLINE);
        if (result == Result::InsufficientCondition) return impl.fail(result, MORPH_LIMIT);
        return impl.fail(result, SCENE_MEMORY);
    }
    impl.morphPoints += weight;
    index = impl.morphCnt;
    impl.morphs[impl.morphCnt++] = shape;
    return Result::Success;
}

// Discards the outlines paired by a play call that failed before it added its clips.
void Scene::Impl::dropMorphs(uint32_t count, uint32_t points) noexcept
{
    for (auto i = count; i < morphCnt; i++) _freeMorph(morphs[i]);
    morphCnt = count;
    morphPoints = points;
}

// Points the geometry clips of one scene keep in all: 96 MiB.
static constexpr uint32_t MAX_GEOMETRY_POINTS = 8388608;

static void _freeGeometry(GeometryShape* shape)
{
    if (!shape) return;
    delete[] shape->points;
    delete[] shape->ends;
    delete[] shape->samples.points;
    delete shape;
}

// Whether two shapes hold the same points in the same pieces.
static bool _same(const GeometryShape& first, const GeometryShape& second)
{
    if (!_alike(first, second)) return false;
    if (first.samples.count != second.samples.count) return false;
    if (first.samples.count && (!sameGrid(first, second)
        || std::memcmp(first.samples.points, second.samples.points, sizeof(Vec3) * first.samples.count) != 0)) return false;
    return !first.count || std::memcmp(first.points, second.points, sizeof(Vec3) * first.count) == 0;
}

const GeometryShape* Scene::Impl::reshape(const GeometryShape& shape, const GeometryShape* reuse) noexcept
{
    if (reuse && _same(*reuse, shape)) return reuse;
    auto total = shape.count + shape.samples.count;
    if (total > MAX_GEOMETRY_POINTS - geometryPoints) return nullptr;
    if (!grow(geometries, geometryCnt, geometryCap, 1u, 8u)) return nullptr;
    auto copy = new (std::nothrow) GeometryShape;
    auto points = copy && shape.count ? new (std::nothrow) Vec3[shape.count] : nullptr;
    auto ends = copy && shape.pieces > 1u ? new (std::nothrow) uint32_t[shape.pieces] : nullptr;
    auto samples = copy && shape.samples.count ? new (std::nothrow) Vec3[shape.samples.count] : nullptr;
    if (!copy || (shape.count && !points) || (shape.pieces > 1u && !ends) || (shape.samples.count && !samples)) {
        delete[] points;
        delete[] ends;
        delete[] samples;
        delete copy;
        return nullptr;
    }
    if (points) std::memcpy(points, shape.points, sizeof(Vec3) * shape.count);
    if (ends) std::memcpy(ends, shape.ends, sizeof(uint32_t) * shape.pieces);
    copy->points = points;
    copy->ends = ends;
    copy->count = shape.count;
    copy->pieces = shape.pieces;
    if (samples) std::memcpy(samples, shape.samples.points, sizeof(Vec3) * shape.samples.count);
    copy->contour = shape.contour;
    copy->samples = shape.samples;
    copy->samples.points = samples;
    geometries[geometryCnt++] = copy;
    geometryPoints += total;
    return copy;
}

// Discards the geometries kept by a call that failed before it added its clips.
void Scene::Impl::dropGeometries(uint32_t count, uint32_t points) noexcept
{
    for (auto i = count; i < geometryCnt; i++) {
        _freeGeometry(geometries[i]);
        geometries[i] = nullptr;
    }
    geometryCnt = count;
    geometryPoints = points;
}

GeometryShape Scene::Impl::own(const Object* object, Vec3* points) noexcept
{
    GeometryShape shape;
    shape.count = shapeCount(object);
    shape.points = points;
    shapeOf(object, points);
    if (object->type() == Type::Plot) {
        auto plot = static_cast<const Plot*>(object);
        shape.pieces = plot->objContours;
        shape.ends = shape.pieces > 1u ? plot->objEnds : nullptr;
        shape.samples = plot->objSamples;
        shape.contour = plot->objGrid;
    }
    return shape;
}

bool Scene::Impl::reshapable(ObjectEntry& entry) noexcept
{
    if (entry.geometry) return true;
    auto count = shapeCount(entry.object);
    auto points = count ? new (std::nothrow) Vec3[count] : nullptr;
    if (count && !points) return false;
    auto copy = reshape(own(entry.object, points));
    delete[] points;
    if (!copy) return false;
    // The object draws its own geometry until now; from now on it draws the copies, since its
    // own becomes the one its latest clip ends at. Its saves keep the starting one.
    entry.geometry = copy;
    entry.base.geometry = copy;
    entry.base.geometryFrom = copy;
    entry.base.geometryProgress = 1.0f;
    for (auto i = 0u; i < saveCnt; i++) {
        for (auto r = 0u; r < saves[i].count; r++) {
            auto& record = saves[i].objects[r];
            if (record.object != entry.object || record.state.geometry) continue;
            record.state.geometry = copy;
            record.state.geometryFrom = copy;
            record.state.geometryProgress = 1.0f;
        }
    }
    return true;
}

SceneSequence::~SceneSequence()
{
    if (owned) {
        for (auto i = 0u; i < count; i++)
            delete stages[i].scene;
    }
    if (transitions) {
        for (auto i = 0u; i + 1u < count; i++)
            delete[] transitions[i].matches;
    }
    delete[] stages;
    delete[] transitions;
}

// ---------- Scene transition frames ----------

void SceneTransition::frame(const Viewport& viewport, float progress, uint8_t outgoingAlpha,
                            uint8_t incomingAlpha, TransitionFrame& output) const noexcept
{
    auto bounded = _clamp(progress);
    output = {};
    output.outgoing = viewport;
    output.incoming = viewport;
    // The opacity of the scene above, and how far the scene below would fade out if nothing
    // covered it: it fades only as far as the background of the scene above lets it show.
    auto above = 1.0f;
    auto below = 0.0f;
    switch (kind) {
        case TransitionKind::Match: return;
        case TransitionKind::Fade:
            above = bounded;
            below = bounded;
            break;
        case TransitionKind::Slide: {
            // The incoming scene starts one viewport away on its side and the outgoing one ends
            // one viewport away on the other, so the two stay edge to edge.
            auto x = 0.0f;
            auto y = 0.0f;
            switch (from) {
                case TransitionSide::Left: x = -1.0f; break;
                case TransitionSide::Right: x = 1.0f; break;
                case TransitionSide::Bottom: y = 1.0f; break;
                case TransitionSide::Top: y = -1.0f; break;
            }
            output.incoming.x += x * (1.0f - progress) * viewport.width;
            output.incoming.y += y * (1.0f - progress) * viewport.height;
            output.outgoing.x -= x * progress * viewport.width;
            output.outgoing.y -= y * progress * viewport.height;
            break;
        }
        case TransitionKind::ZoomIn:
        case TransitionKind::ZoomOut: {
            // The outer scene grows by a factor that rises evenly toward 1 / size, about the
            // point that keeps the area on its way to the whole viewport, and the inner scene
            // fills the area as it grows. Zooming in, the outgoing scene is outer; zooming out
            // runs the same backwards with the scenes swapped.
            auto in = kind == TransitionKind::ZoomIn;
            auto share = static_cast<double>(in ? progress : 1.0f - progress);
            auto size = static_cast<double>(area.width);
            auto growth = 1.0 / size;
            auto scale = 1.0;
            auto offset = -share;
            if (growth - 1.0 >= 1.0e-6) {
                scale = std::pow(growth, share);
                offset = growth * (1.0 - scale) / (growth - 1.0);
            }
            auto place = [&](double x, double y, double extent) {
                return Viewport{viewport.x + static_cast<float>(x) * viewport.width,
                                viewport.y + static_cast<float>(y) * viewport.height,
                                static_cast<float>(extent) * viewport.width,
                                static_cast<float>(extent) * viewport.height};
            };
            auto outer = place(offset * area.x, offset * area.y, scale);
            auto inner = place((scale + offset) * area.x, (scale + offset) * area.y, scale * size);
            // Zooming in, the inner scene fades in from a tenth to half of the way and the outer
            // one fades out from half to nine tenths, as far as the background of the inner
            // scene lets it show; zooming out runs both backwards.
            auto ramp = [&](float begin, float end) {
                return _clamp((bounded - begin) / (end - begin));
            };
            output.outgoing = in ? outer : inner;
            output.incoming = in ? inner : outer;
            output.outgoingAbove = !in;
            above = in ? ramp(0.1f, 0.5f) : 1.0f - ramp(0.5f, 0.9f);
            below = in ? ramp(0.5f, 0.9f) : 1.0f - ramp(0.1f, 0.5f);
            break;
        }
    }
    auto alpha = output.outgoingAbove ? outgoingAlpha : incomingAlpha;
    auto shown = 1.0f - below * (1.0f - static_cast<float>(alpha) / 255.0f);
    output.outgoingOpacity = output.outgoingAbove ? above : shown;
    output.incomingOpacity = output.outgoingAbove ? shown : above;
}

// ---------- Storage growth and bookkeeping ----------

// Lets go of the records of save and the copies of strings they hold.
static void _release(SavedState& save)
{
    for (auto i = 0u; i < save.count; i++)
        delete[] save.objects[i].text;
    delete[] save.objects;
    save = {};
}

Scene::Impl::~Impl()
{
    for (auto i = 0u; i < objectCnt; i++) {
        if (objects[i].object->parent()) objects[i].object = nullptr;
    }
    for (auto i = 0u; i < objectCnt; i++)
        delete objects[i].object;
    delete[] objects;
    delete[] clips;
    delete[] cameras;
    for (auto i = 0u; i < viewportCnt; i++)
        delete viewports[i].scene;
    delete[] viewports;
    for (auto i = 0u; i < viewCnt; i++)
        delete views[i];
    delete[] views;
    delete sequence;
    for (auto i = 0u; i < markerCnt; i++)
        delete[] markers[i].name;
    delete[] markers;
    delete[] handoffs;
    for (auto i = 0u; i < saveCnt; i++)
        _release(saves[i]);
    delete[] saves;
    for (auto i = 0u; i < pathCnt; i++)
        delete[] paths[i].vertices;
    delete[] paths;
    for (auto i = 0u; i < morphCnt; i++) _freeMorph(morphs[i]);
    delete[] morphs;
    for (auto i = 0u; i < warpCnt; i++)
        delete warps[i];
    delete[] warps;
    for (auto i = 0u; i < geometryCnt; i++)
        _freeGeometry(geometries[i]);
    delete[] geometries;
    for (auto font : themeFonts)
        delete[] font;
}

bool Scene::Impl::assign(const Theme& theme) noexcept
{
    const char* names[] = {
        theme.h1.font,
        theme.h2.font,
        theme.h3.font,
        theme.text.font,
        theme.code.font,
    };
    char* copies[5] = {};
    for (auto i = 0u; i < 5u; i++) {
        copies[i] = _duplicate(names[i]);
        if (copies[i]) continue;
        for (auto copy : copies)
            delete[] copy;
        return false;
    }
    for (auto font : themeFonts)
        delete[] font;
    for (auto i = 0u; i < 5u; i++)
        themeFonts[i] = copies[i];
    sceneTheme = theme;
    sceneTheme.h1.font = themeFonts[0];
    sceneTheme.h2.font = themeFonts[1];
    sceneTheme.h3.font = themeFonts[2];
    sceneTheme.text.font = themeFonts[3];
    sceneTheme.code.font = themeFonts[4];
    cfg.background = theme.background;
    colorCursor = 0u;
    return true;
}

bool Scene::Impl::growClips(uint32_t count) noexcept
{
    return grow(clips, clipCnt, clipCap, count, 16u);
}

void Scene::Impl::link(uint32_t index) noexcept
{
    auto& clip = clips[index];
    clip.next = NO_CLIP;
    auto item = entry(clip.object);
    if (!item) return;
    if (item->lastClip == NO_CLIP) item->firstClip = index;
    else clips[item->lastClip].next = index;
    item->lastClip = index;
    item->clipCount++;
    work += item->family;
}

// The latest time a camera move runs until, or 0 without one.
static float _cameraEnd(const CameraClip* cameras, uint32_t cameraCnt) noexcept
{
    auto end = 0.0f;
    for (auto i = 0u; i < cameraCnt; i++) end = std::fmax(end, cameras[i].end);
    return end;
}

// The earliest time after begin and before end at which the stored camera moves show the 2D
// view, or a negative number when they show the 3D view throughout.
static float _planarSince(const CameraClip* cameras, uint32_t cameraCnt, CameraView initialView,
                          float begin, float end) noexcept
{
    constexpr auto INF = std::numeric_limits<float>::infinity();
    const TimeSpan asked = {begin, false, end, false};
    auto earliest = -1.0f;
    auto check = [&asked, &earliest](const TimeSpan& owned, const TimeSpan& planar) {
        auto span = _shared(_shared(owned, planar), asked);
        if (_occupied(span) && (earliest < 0.0f || span.begin < earliest)) earliest = span.begin;
    };
    // Sampling lets the last stored move that sets the view win, and a move keeps setting it once
    // it starts to: a plain move from its begin, a layered move into 3D once under way, and a
    // layered move back to 2D from its end. Walking back from the last move, each one therefore
    // shows its view from where it starts setting it until a move stored after it starts.
    auto until = INF;
    auto untilClosed = false;
    for (auto i = cameraCnt; i-- > 0u;) {
        auto& clip = cameras[i];
        TimeSpan owned = {clip.begin, true, until, !untilClosed};
        if (clip.layered) {
            if (clip.fromView == clip.toView) continue;
            if (clip.toView == CameraView::ThreeD) owned.beginClosed = false;
            else owned.begin = clip.end;
        }
        if (clip.toView == CameraView::TwoD && clip.fromView == CameraView::ThreeD) {
            check(owned, {clip.end, true, INF, false});
        } else if (!clip.layered && clip.fromView == CameraView::TwoD) {
            if (clip.toView == CameraView::TwoD) check(owned, {clip.begin, true, INF, false});
            // A plain move into 3D still shows 2D at its first moment.
            else check(owned, {clip.begin, true, clip.begin, true});
        }
        if (owned.begin < until) {
            until = owned.begin;
            untilClosed = owned.beginClosed;
        } else if (owned.begin == until) {
            untilClosed = untilClosed || owned.beginClosed;
        }
    }
    if (cameraCnt && initialView == CameraView::TwoD) check({-INF, false, until, !untilClosed}, {-INF, false, INF, false});
    return earliest;
}

ViewEntry::~ViewEntry()
{
    delete[] hidden;
    delete[] cameras;
    delete[] fades;
}

CameraTrack Scene::Impl::track(uint32_t index) noexcept
{
    if (!index) return {cameras, cameraCnt, cameraCap, initialCamera, initialView, cfg.camera, cfg.cameraView};
    auto& view = *views[index - 1u];
    return {view.cameras, view.cameraCnt, view.cameraCap, view.initialCamera, view.initialView,
            view.camera, view.cameraView};
}

bool Scene::Impl::growHandoffs(uint32_t count) noexcept
{
    return grow(handoffs, handoffCnt, handoffCap, count, 8u);
}

void Scene::Impl::advance(float time) noexcept
{
    cursor = time;
    if (time > extent) extent = time;
}

Result Scene::Impl::fail(Result result, const char* reason) noexcept
{
    diagnostic = reason;
    return result;
}

ObjectEntry* Scene::Impl::entry(const Object* object) const noexcept
{
    // The id names the entry; an object of another scene may carry an id that names one here.
    if (!object || object->objId == 0u || object->objId > objectCnt) return nullptr;
    auto item = objects + (object->objId - 1u);
    return item->object == object ? item : nullptr;
}

void Scene::Impl::discard(Object* copy, uint32_t first) noexcept
{
    // The objects above the copy lose it from their families, and the work their clips read for
    // its objects, as attach() added them; the copy holds no clips of its own yet.
    auto count = objectCnt - first;
    for (auto above = copy->objParent; above; above = above->objParent) {
        auto item = entry(above);
        if (!item) continue;
        item->family -= count;
        work -= static_cast<uint64_t>(item->clipCount) * count;
    }
    for (auto i = first; i < objectCnt; i++)
        objects[i] = ObjectEntry{};
    objectCnt = first;
    if (auto parent = copy->objParent) {
        // The copy is the last child of its parent unless another was added after it.
        auto index = parent->childCnt;
        while (index > 0u && parent->children[index - 1u] != copy) index--;
        if (index > 0u) {
            for (auto i = index; i < parent->childCnt; i++)
                parent->children[i - 1u] = parent->children[i];
            parent->childCnt--;
        }
    }
    copy->objParent = nullptr;
    delete copy;
}

// ---------- Sampling clips ----------

// The model a layered clip leaves when its change is change and the clips before it give model:
// the change applies on top, its pivot first moved to where the object has carried it.
static Mat4 _layer(const Clip& clip, const Mat4& change, const Mat4& model)
{
    if (!clip.follows) return change * model;
    auto moved = model * clip.anchor - clip.pivot;
    return Mat4::translate(moved) * change * Mat4::translate(moved * -1.0f) * model;
}

// Ease scale out and back while oscillating rotation about the pivot; pixel sizes follow scale.
static void _wiggle(const Clip& clip, float t, VisualState& state)
{
    auto smooth = [](float x) { return x * x * x * (10.0f + x * (6.0f * x - 15.0f)); };
    t = _clamp(t);
    auto there = smooth(t < 0.5f ? 2.0f * t : 2.0f - 2.0f * t);
    auto grow = 1.0f + (clip.stretch.x - 1.0f) * there;
    auto turn = clip.angle * there * std::sin(clip.swings * PI * t);
    state.model = Mat4::translate(clip.pivot) * Mat4::rotateZ(turn) * Mat4::scale({grow, grow, grow})
                * Mat4::translate(clip.pivot * -1.0f) * state.model;
    state.pixelScale *= grow;
}

bool Scene::Impl::sample(const ObjectEntry& entry, float time, VisualState& state) const noexcept
{
    if (time < entry.born) return false;
    if (entry.dead >= 0.0f && time >= entry.dead) return false;

    // Each clip writes only its channels. Clips that share a channel never overlap in
    // time and are stored in time order, so later clips win and parallel clips on
    // disjoint channels compose. The one exception is a layered clip, stored after the
    // model clips it overlaps, which multiplies its change onto their model. The object's
    // own list holds its clips in that order, so the clips of other objects cost nothing.
    state = entry.base;
    for (auto i = entry.firstClip; i != NO_CLIP; i = clips[i].next) {
        auto& clip = clips[i];
        if (time < clip.begin) continue;
        if (time >= clip.end) {
            auto last = clip.kind == AnimationKind::Indicate ? clip.from : clip.to;
            if (clip.motion == ClipMotion::Path && paths[clip.path - 1u].source->type() == Type::Connector
                && !motion(clip, 1.0f, clip.end, last.model)) return false;
            auto model = state.model;
            _apply(state, last, clip.channels);
            if (clip.layered) state.model = _layer(clip, last.model, model);
            continue;
        }
        auto progress = (time - clip.begin) / (clip.end - clip.begin);
        auto eased = ease(clip.curve, progress);
        if (clip.kind == AnimationKind::Indicate) eased = std::sin(PI * _clamp(eased));
        auto sampled = _lerp(clip.from, clip.to, eased);
        if (clip.motion != ClipMotion::Linear && !motion(clip, eased, time, sampled.model)) return false;
        if (clip.kind == AnimationKind::Wiggle) _wiggle(clip, eased, sampled);
        if (clip.window > 0.0f) {
            // The bright segment head travels from the outline start to window beyond its end.
            auto head = eased * (1.0f + clip.window);
            sampled.progress = _clamp(head);
            sampled.start = _clamp(head - clip.window);
        }
        if (clip.kind == AnimationKind::DrawBorderThenFill) {
            constexpr auto BorderPhase = 2.0f / 3.0f;
            sampled.progress = _clamp(eased / BorderPhase);
            sampled.fillProgress = _clamp((eased - BorderPhase) / (1.0f - BorderPhase));
        }
        if (clip.kind == AnimationKind::Morph) {
            sampled.morph = clip.related;
            sampled.morphShape = clip.morph && clip.morph <= morphCnt ? morphs + clip.morph - 1u : nullptr;
            sampled.morphProgress = _clamp(eased);
        }
        if (clip.layered) sampled.model = _layer(clip, sampled.model, state.model);
        _apply(state, sampled, clip.channels);
    }
    auto parent = entry.object->parent();
    if (entry.object->type() == Type::Text && parent && parent->type() == Type::Plot) {
        auto plot = static_cast<const Plot*>(parent);
        if (plot->contourLabels()) {
            auto parentEntry = this->entry(parent);
            VisualState parentState;
            if (!parentEntry || !sample(*parentEntry, time, parentState)) return false;
            auto grid = parentState.geometry ? plotAt(parentState).contour : plot->contourGrid();
            state.value = static_cast<float>(grid.level);
        }
    }
    return true;
}

bool Scene::Impl::motion(const Clip& clip, float progress, float time, Mat4& model) const noexcept
{
    auto& from = clip.from.model;
    switch (clip.motion) {
        case ClipMotion::Linear: model = _lerp(from, clip.to.model, progress); return true;
        case ClipMotion::Arc:
            model = _sweep(from, clip.to.model, _unit(clip.axis), clip.arc, progress);
            return _affine(model);
        case ClipMotion::Pivot: {
            auto axis = _unit(clip.axis);
            auto linear = _matrix(_rotation(axis, static_cast<double>(clip.angle) * progress));
            Linear stretch;
            stretch.m[0] = 1.0 + (static_cast<double>(clip.stretch.x) - 1.0) * progress;
            stretch.m[4] = 1.0 + (static_cast<double>(clip.stretch.y) - 1.0) * progress;
            stretch.m[8] = 1.0 + (static_cast<double>(clip.stretch.z) - 1.0) * progress;
            linear = _multiply(linear, stretch);
            auto moved = _travel({clip.offset.x, clip.offset.y, clip.offset.z}, axis, clip.arc, progress);
            auto anchor = _apply(linear, {clip.pivot.x, clip.pivot.y, clip.pivot.z});
            model = _model(linear, {clip.pivot.x + moved.x - anchor.x, clip.pivot.y + moved.y - anchor.y,
                                    clip.pivot.z + moved.z - anchor.z}) * from;
            return _affine(model);
        }
        case ClipMotion::MatchPath: {
            if (!clip.path || clip.path > pathCnt) return false;
            auto& path = paths[clip.path - 1u];
            auto& first = path.vertices[0].point;
            auto& last = path.vertices[path.count - 1u];
            MotionSpan span;
            span.high = last.length;
            Vector64 position;
            _follow(path, static_cast<double>(last.length) * _clamp(progress), span, position, nullptr);
            auto x = position.x - (first.x + (static_cast<double>(last.point.x) - first.x) * progress);
            auto y = position.y - (first.y + (static_cast<double>(last.point.y) - first.y) * progress);
            auto z = position.z - (first.z + (static_cast<double>(last.point.z) - first.z) * progress);
            model = _lerp(from, clip.to.model, progress);
            model.e[3] += static_cast<float>(clip.axis.x * x + clip.stretch.x * y + clip.offset.x * z);
            model.e[7] += static_cast<float>(clip.axis.y * x + clip.stretch.y * y + clip.offset.y * z);
            model.e[11] += static_cast<float>(clip.axis.z * x + clip.stretch.z * y + clip.offset.z * z);
            return _affine(model);
        }
        case ClipMotion::Path: {
            if (!clip.path || clip.path > pathCnt) return false;
            auto& path = paths[clip.path - 1u];
            if (path.source->type() == Type::Connector) {
                Vec3 start;
                Vec3 finish;
                auto defined = false;
                if (ends(static_cast<const Connector*>(path.source), time, start, finish, defined) != Result::Success
                    || !defined) return false;
                auto parent = Mat4::identity();
                if (path.parent) {
                    VisualState state;
                    auto alive = false;
                    if (world(path.parent, time, state, alive) != Result::Success || !alive
                        || !_inverse(state.model, parent)) return false;
                }
                start = parent * start;
                finish = parent * finish;
                auto fraction = clip.pathFrom + (clip.pathTo - clip.pathFrom) * progress;
                auto position = start + (finish - start) * fraction;
                Linear linear;
                if (clip.orient) {
                    Vec3 a;
                    Vec3 b;
                    auto initial = false;
                    if (ends(static_cast<const Connector*>(path.source), clip.begin, a, b, initial) != Result::Success
                        || !initial) return false;
                    // A straight connector has one heading; turn by its change since the start.
                    auto initialParent = Mat4::identity();
                    if (path.parent) {
                        VisualState state;
                        auto alive = false;
                        if (world(path.parent, clip.begin, state, alive) != Result::Success || !alive
                            || !_inverse(state.model, initialParent)) return false;
                    }
                    auto original = (initialParent * b - initialParent * a).normalized();
                    auto current = (finish - start).normalized();
                    auto turn = _turn({original.x, original.y, original.z}, {current.x, current.y, current.z});
                    linear = _matrix(turn);
                }
                auto anchor = _apply(linear, {clip.pivot.x, clip.pivot.y, clip.pivot.z});
                model = _model(linear, {position.x - anchor.x, position.y - anchor.y, position.z - anchor.z}) * from;
                return _affine(model);
            }
            auto total = static_cast<double>(path.vertices[path.count - 1u].length);
            auto start = static_cast<double>(clip.pathFrom);
            auto finish = static_cast<double>(clip.pathTo);
            auto distance = total * (start + (finish - start) * progress);
            MotionSpan span;
            span.low = total * std::fmin(start, finish);
            span.high = total * std::fmax(start, finish);
            span.forward = finish >= start;
            span.loop = path.closed && std::fmin(start, finish) == 0.0 && std::fmax(start, finish) == 1.0;
            Vector64 position;
            Linear linear;
            if (clip.orient) {
                Rotation origin;
                Rotation current;
                Vector64 ignored;
                _follow(path, total * start, span, ignored, &origin);
                _follow(path, distance, span, position, &current);
                auto turn = _multiply(current, _conjugate(origin));
                if (_normalize(turn)) linear = _matrix(turn);
            } else {
                _follow(path, distance, span, position, nullptr);
            }
            auto anchor = _apply(linear, {clip.pivot.x, clip.pivot.y, clip.pivot.z});
            model = _model(linear, {position.x - anchor.x, position.y - anchor.y, position.z - anchor.z}) * from;
            return _affine(model);
        }
    }
    return false;
}

// Applies the moves of a camera timeline at time to camera and view, which hold the pose before
// the first move.
static bool _sampleCamera(const CameraClip* cameras, uint32_t cameraCnt, float time, Camera& camera,
                          CameraView& view) noexcept
{
    for (auto i = 0u; i < cameraCnt; i++) {
        auto& clip = cameras[i];
        // A layered move may begin before a move stored ahead of it, so later moves still count.
        if (time < clip.begin) continue;
        if (time >= clip.end) {
            // A layered look back to the 2D view ends with the moves it adds to (see
            // Scene::look), so it leaves the camera as a plain look does: facing the plane
            // straight on rather than at the sum of the turns.
            if (clip.layered && !(clip.fromView == CameraView::ThreeD && clip.toView == CameraView::TwoD)) {
                if (!_layer(clip, 1.0f, camera)) return false;
                if (clip.fromView != clip.toView || clip.from.projection != clip.to.projection) {
                    camera.projection = clip.to.projection;
                }
                if (clip.fromView != clip.toView) view = clip.toView;
                continue;
            }
            camera = clip.to;
            view = clip.toView;
            continue;
        }
        auto progress = (time - clip.begin) / (clip.end - clip.begin);
        auto eased = ease(clip.curve, progress);
        if (clip.layered) {
            if (!_layer(clip, eased, camera)) return false;
            // A layered look switches the view and projection when a plain look would.
            if (clip.fromView == CameraView::TwoD && clip.toView == CameraView::ThreeD && progress > 0.0f) {
                camera.projection = clip.to.projection;
                view = clip.toView;
            } else if (clip.fromView == clip.toView && clip.from.projection != clip.to.projection
                       && progress >= 0.5f) {
                camera.projection = clip.to.projection;
            }
            continue;
        }
        // An orbit keeps the curve's overshoot: turning past the end and back is still an orbit.
        if (clip.orbit != 0.0f ? !_orbit(clip.from, clip.axis, static_cast<double>(clip.orbit) * eased, camera)
                               : !_lerp(clip.from, clip.to, _clamp(eased), camera)) {
            return false;
        }
        view = clip.fromView;
        if (clip.fromView == CameraView::TwoD && clip.toView == CameraView::ThreeD
            && progress > 0.0f) {
            camera.projection = clip.to.projection;
            view = clip.toView;
        } else if (clip.fromView == clip.toView && progress >= 0.5f) {
            camera.projection = clip.to.projection;
        }
    }
    return true;
}

Result Scene::Impl::sample(float time, Camera& camera, CameraView& view) const noexcept
{
    if (!cameraCnt) {
        camera = cfg.camera;
        view = cfg.cameraView;
    } else {
        camera = initialCamera;
        view = initialView;
        if (!_sampleCamera(cameras, cameraCnt, time, camera, view)) return Result::InvalidArguments;
    }
    auto sampled = cfg;
    sampled.camera = camera;
    sampled.cameraView = view;
    if (!valid(sampled)) return Result::InvalidArguments;
    for (auto i = 0u; i < runtimeModifierCnt; i++) {
        auto& modifier = runtimeModifiers[i];
        if (!modifier.camera) continue;
        auto result = modifier.camera(time, camera, view, modifier.data);
        if (result != Result::Success) return result;
    }
    sampled.camera = camera;
    sampled.cameraView = view;
    return valid(sampled) ? Result::Success : Result::InvalidArguments;
}

// ---------- Animation factories ----------

Animation Animation::create(Object* target, DrawDirection direction) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Create;
    animation.direction = direction;
    return animation;
}

Animation Animation::uncreate(Object* target, DrawDirection direction) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Uncreate;
    animation.direction = direction;
    return animation;
}

Animation Animation::fillReveal(Object* target) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::FillReveal;
    return animation;
}

Animation Animation::drawBorderThenFill(Object* target, DrawDirection direction) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::DrawBorderThenFill;
    animation.direction = direction;
    return animation;
}

Animation Animation::write(Object* target, DrawDirection direction) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Write;
    animation.direction = direction;
    return animation;
}

Animation Animation::fade(Object* target, float opacity) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Fade;
    animation.value = _clamp(opacity);
    return animation;
}

Animation Animation::fadeIn(Object* target, const Vec3& shift, float scale) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::FadeIn;
    animation.offset = shift;
    animation.scale = scale;
    return animation;
}

Animation Animation::fadeOut(Object* target, const Vec3& shift, float scale) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::FadeOut;
    animation.offset = shift;
    animation.scale = scale;
    return animation;
}

Animation Animation::growFromCenter(Object* target) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Grow;
    return animation;
}

Animation Animation::growFromEdge(Object* target, GrowthEdge edge) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::GrowFromEdge;
    animation.growthEdge = edge;
    return animation;
}

Animation Animation::shrinkToCenter(Object* target) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Shrink;
    return animation;
}

Animation Animation::indicate(Object* target, Color color, float scale) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Indicate;
    animation.color = color;
    animation.scale = scale;
    return animation;
}

Animation Animation::morph(Object* source, Object* target) noexcept
{
    Animation animation;
    animation.target = source;
    animation.related = target;
    animation.kind = AnimationKind::Morph;
    return animation;
}

Animation Animation::shift(Object* target, const Vec3& by) noexcept
{
    if (!target) return {};
    auto animation = transform(target, Mat4::translate(by) * target->model);
    return animation;
}

Animation Animation::transform(Object* target, const Mat4& model) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Transform;
    animation.model = model;
    return animation;
}

Animation Animation::stroke(Object* target, Color color) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Stroke;
    animation.color = color;
    return animation;
}

Animation Animation::fill(Object* target, Color color) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Fill;
    animation.color = color;
    return animation;
}

Animation Animation::rotate(Object* target, float radians, const Vec3& axis) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Rotate;
    animation.angle = radians;
    animation.axis = axis;
    animation.centered = true;
    return animation;
}

Animation Animation::rotate(Object* target, float radians, const Vec3& pivot, const Vec3& axis) noexcept
{
    auto animation = rotate(target, radians, axis);
    animation.pivot = pivot;
    animation.centered = false;
    return animation;
}

Animation Animation::rescale(Object* target, const Vec3& factors) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Scale;
    animation.stretch = factors;
    animation.centered = true;
    return animation;
}

Animation Animation::rescale(Object* target, const Vec3& factors, const Vec3& pivot) noexcept
{
    auto animation = rescale(target, factors);
    animation.pivot = pivot;
    animation.centered = false;
    return animation;
}

Animation Animation::moveAlong(Object* target, Object* path, bool orient) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::MoveAlong;
    animation.related = path;
    animation.orient = orient;
    return animation;
}

Animation Animation::wiggle(Object* target, float scale, float angle, float swings) noexcept
{
    Animation animation;
    animation.target = target;
    animation.kind = AnimationKind::Wiggle;
    animation.scale = scale;
    animation.angle = angle;
    animation.value = swings;
    return animation;
}

// ---------- Animation targets ----------

static constexpr uint32_t TARGET_MODEL = 1u << 0u;
static constexpr uint32_t TARGET_OPACITY = 1u << 1u;
static constexpr uint32_t TARGET_STROKE = 1u << 2u;
static constexpr uint32_t TARGET_FILL = 1u << 3u;
static constexpr uint32_t TARGET_DASH_OFFSET = 1u << 4u;
static constexpr uint32_t TARGET_TAIL = 1u << 5u;
static constexpr uint32_t TARGET_TIP = 1u << 6u;
static constexpr uint32_t TARGET_PROGRESS = 1u << 7u;
static constexpr uint32_t TARGET_WIDTH = 1u << 8u;
static constexpr uint32_t TARGET_RADIUS = 1u << 9u;
static constexpr uint32_t TARGET_SIZE = 1u << 10u;
static constexpr uint32_t TARGET_CORNER = 1u << 11u;
static constexpr uint32_t TARGET_GRADIENT = 1u << 12u;
static constexpr uint32_t TARGET_BLUR = 1u << 13u;
static constexpr uint32_t TARGET_SHADOW = 1u << 14u;
static constexpr uint32_t TARGET_VALUE = 1u << 15u;
static constexpr uint32_t TARGET_TEXT = 1u << 16u;
static constexpr uint32_t TARGET_LAYER = 1u << 17u;
static constexpr uint32_t TARGET_GEOMETRY = 1u << 18u;
static constexpr uint32_t TARGET_TRIM_DISTANCE = 1u << 19u;
static constexpr uint32_t TARGET_ARC_START = 1u << 20u;
static constexpr uint32_t TARGET_ARC_ANGLE = 1u << 21u;
static constexpr uint32_t TARGET_ALL = (1u << 22u) - 1u;
// The properties that make an object a layer that carries effects.
static constexpr uint32_t TARGET_EFFECTS = TARGET_BLUR | TARGET_SHADOW;
// The properties a group passes to the shapes that take them from it.
static constexpr uint32_t TARGET_INHERITED = TARGET_STROKE | TARGET_FILL | TARGET_WIDTH;

// A circle keeps its radius in the size channel; a point's radius is its style radius.
static uint32_t _channels(uint32_t properties, Type type)
{
    auto channels = 0u;
    if (properties & TARGET_MODEL) channels |= CHANNEL_MODEL;
    if (properties & TARGET_OPACITY) channels |= CHANNEL_OPACITY;
    if (properties & TARGET_STROKE) channels |= CHANNEL_STROKE;
    if (properties & TARGET_FILL) channels |= CHANNEL_FILL;
    if (properties & TARGET_DASH_OFFSET) channels |= CHANNEL_DASH_OFFSET;
    if (properties & TARGET_TAIL) channels |= CHANNEL_TAIL;
    if (properties & TARGET_TIP) channels |= CHANNEL_TIP;
    if (properties & TARGET_PROGRESS) channels |= CHANNEL_PROGRESS;
    if (properties & TARGET_WIDTH) channels |= CHANNEL_WIDTH;
    if (properties & TARGET_RADIUS) channels |= (type == Type::Circle || type == Type::Arc || type == Type::Angle) ? CHANNEL_SIZE : CHANNEL_RADIUS;
    if (properties & TARGET_ARC_START) channels |= CHANNEL_ARC_START;
    if (properties & TARGET_ARC_ANGLE) channels |= CHANNEL_ARC_ANGLE;
    if (properties & TARGET_SIZE) channels |= CHANNEL_SIZE;
    if (properties & TARGET_CORNER) channels |= CHANNEL_CORNER;
    if (properties & TARGET_TRIM_DISTANCE) channels |= CHANNEL_TRIM_DISTANCE;
    if (properties & TARGET_GRADIENT) channels |= CHANNEL_GRADIENT;
    if (properties & TARGET_BLUR) channels |= CHANNEL_BLUR;
    if (properties & TARGET_SHADOW) channels |= CHANNEL_SHADOW;
    if (properties & TARGET_VALUE) channels |= CHANNEL_VALUE;
    if (properties & TARGET_TEXT) channels |= CHANNEL_TEXT;
    if (properties & TARGET_LAYER) channels |= CHANNEL_LAYER;
    if (properties & TARGET_GEOMETRY) channels |= CHANNEL_GEOMETRY;
    return channels;
}

// Whether target would change the radius, size or corner of a warped circle or rectangle, which
// draws the outline its warp gave instead.
static bool _warpSized(const Object* object, uint32_t properties)
{
    auto type = object->type();
    return warped(object) && (type == Type::Circle || type == Type::Rectangle || type == Type::Arc || type == Type::Angle)
           && (_channels(properties, type) & (CHANNEL_SIZE | CHANNEL_CORNER | CHANNEL_ARC_START | CHANNEL_ARC_ANGLE));
}

uint32_t Scene::Impl::heirs(const Object* group, uint32_t properties, Heir* out, uint32_t capacity,
                            uint32_t count) const noexcept
{
    // A group that is typeset, or lies below one, colors its texts too: they are one block.
    auto texts = false;
    for (auto above = group; above && !texts; above = above->parent()) {
        texts = above->type() == Type::Group && static_cast<const Group*>(above)->typeset;
    }
    return heirs(group, properties, texts, out, capacity, count);
}

uint32_t Scene::Impl::heirs(const Object* group, uint32_t properties, bool texts, Heir* out,
                            uint32_t capacity, uint32_t count) const noexcept
{
    for (auto i = 0u; i < group->childCnt; i++) {
        auto child = group->children[i];
        if (child->type() == Type::Group) {
            // A nearer group passes on only what it does not set itself.
            auto passed = properties;
            if (child->objStrokeAuthored) passed &= ~TARGET_STROKE;
            if (child->objFillAuthored) passed &= ~TARGET_FILL;
            if (child->objWidthAuthored) passed &= ~TARGET_WIDTH;
            if (passed) count = heirs(child, passed, texts, out, capacity, count);
            continue;
        }
        auto taken = _palette(child->type()) ? properties : 0u;
        if (child->objStrokeAuthored) taken &= ~TARGET_STROKE;
        if (child->objFillAuthored || !_fillable(child)) taken &= ~TARGET_FILL;
        if (child->objWidthAuthored) taken &= ~TARGET_WIDTH;
        if (texts && child->type() == Type::Text && !child->objColorAuthored && !child->objFillAuthored) {
            taken = properties & TARGET_FILL;
        }
        if (taken) {
            if (count < capacity) out[count] = {child, entry(child), 0u, taken};
            count++;
        }
        // Shapes nested in a shape take the colors from the group above both.
        count = heirs(child, properties, texts, out, capacity, count);
    }
    return count;
}

AnimationTarget AnimationTarget::from(Object* target) noexcept
{
    AnimationTarget state;
    state.target = target;
    if (!target) return state;
    state.targetModel = target->model;
    state.targetStroke = target->style.stroke;
    state.targetFill = target->style.fill;
    state.targetOpacity = target->opacity;
    state.targetDashOffset = target->style.dashOffset;
    state.targetProgress = target->progress;
    auto sampled = _state(target);
    state.targetTail = sampled.markerTail;
    state.targetTip = sampled.markerTip;
    state.targetWidth = sampled.width;
    state.targetRadius = (target->type() == Type::Circle || target->type() == Type::Arc || target->type() == Type::Angle) ? sampled.size.x : sampled.radius;
    state.targetStart = sampled.arcStart; state.targetAngle = sampled.arcAngle;
    state.targetSize = sampled.size;
    state.targetCorner = sampled.corner;
    state.targetTrimDistance = sampled.trimDistance;
    state.targetGradient = sampled.gradient;
    state.targetGradientEnd = sampled.gradientEnd;
    state.targetValue = sampled.value;
    state.targetLayer = target->layer;
    return state;
}

AnimationTarget& AnimationTarget::shift(const Vec3& by) noexcept
{
    targetModel = Mat4::translate(by) * targetModel;
    properties |= TARGET_MODEL;
    return *this;
}

AnimationTarget& AnimationTarget::transform(const Mat4& model) noexcept
{
    targetModel = model;
    properties |= TARGET_MODEL;
    return *this;
}

AnimationTarget& AnimationTarget::opacity(float value) noexcept
{
    targetOpacity = value;
    properties |= TARGET_OPACITY;
    return *this;
}

AnimationTarget& AnimationTarget::stroke(Color color) noexcept
{
    targetStroke = color;
    properties |= TARGET_STROKE;
    return *this;
}

AnimationTarget& AnimationTarget::fill(Color color) noexcept
{
    targetFill = color;
    properties |= TARGET_FILL;
    return *this;
}

AnimationTarget& AnimationTarget::dashOffset(float value) noexcept
{
    targetDashOffset = value;
    properties |= TARGET_DASH_OFFSET;
    return *this;
}

AnimationTarget& AnimationTarget::tail(float value) noexcept
{
    targetTail = value;
    properties |= TARGET_TAIL;
    return *this;
}

AnimationTarget& AnimationTarget::tip(float value) noexcept
{
    targetTip = value;
    properties |= TARGET_TIP;
    return *this;
}

AnimationTarget& AnimationTarget::progress(float value) noexcept
{
    targetProgress = value;
    properties |= TARGET_PROGRESS;
    return *this;
}

AnimationTarget& AnimationTarget::strokeWidth(float pixels) noexcept
{
    targetWidth = pixels;
    properties |= TARGET_WIDTH;
    return *this;
}

AnimationTarget& AnimationTarget::start(float radians) noexcept
{
    targetStart = radians; properties |= TARGET_ARC_START; return *this;
}

AnimationTarget& AnimationTarget::angle(float radians) noexcept
{
    targetAngle = radians; properties |= TARGET_ARC_ANGLE; return *this;
}

AnimationTarget& AnimationTarget::radius(float value) noexcept
{
    targetRadius = value;
    properties |= TARGET_RADIUS;
    return *this;
}

AnimationTarget& AnimationTarget::size(float value) noexcept
{
    targetSize = {value, value};
    properties |= TARGET_SIZE;
    return *this;
}

AnimationTarget& AnimationTarget::size(const Vec2& value) noexcept
{
    targetSize = value;
    properties |= TARGET_SIZE;
    return *this;
}

AnimationTarget& AnimationTarget::trimDistance(float value) noexcept
{
    return trimDistance(Vec2{value, value});
}

AnimationTarget& AnimationTarget::trimDistance(const Vec2& value) noexcept
{
    targetTrimDistance = value;
    properties |= TARGET_TRIM_DISTANCE;
    return *this;
}

AnimationTarget& AnimationTarget::corner(float value) noexcept
{
    targetCorner = value;
    properties |= TARGET_CORNER;
    return *this;
}

AnimationTarget& AnimationTarget::gradient(const Gradient& value) noexcept
{
    targetGradient = value;
    properties |= TARGET_GRADIENT;
    return *this;
}

AnimationTarget& AnimationTarget::gradient(Color end) noexcept
{
    targetGradientEnd = end;
    properties |= TARGET_GRADIENT;
    return *this;
}

AnimationTarget& AnimationTarget::blur(float pixels) noexcept
{
    targetBlur = pixels;
    properties |= TARGET_BLUR;
    return *this;
}

AnimationTarget& AnimationTarget::shadow(const Shadow& value) noexcept
{
    targetShadow = value;
    properties |= TARGET_SHADOW;
    return *this;
}

AnimationTarget& AnimationTarget::value(float number) noexcept
{
    targetValue = number;
    properties |= TARGET_VALUE;
    return *this;
}

AnimationTarget& AnimationTarget::text(const char* value, TextChange change) noexcept
{
    targetText = value;
    targetTextChange = change;
    properties |= TARGET_TEXT;
    return *this;
}

AnimationTarget& AnimationTarget::layer(int32_t value) noexcept
{
    targetLayer = value;
    properties |= TARGET_LAYER;
    return *this;
}

AnimationTarget& AnimationTarget::geometry(const Vec3* points, uint32_t count) noexcept
{
    targetGeometry = points;
    targetGeometryCount = count;
    targetGeometryEnds = nullptr;
    targetGeometryPieces = 0;
    targetGeometryPieced = false;
    targetSamples = {};
    targetContoured = false;
    properties |= TARGET_GEOMETRY;
    return *this;
}

AnimationTarget& AnimationTarget::geometry(const Vec3* points, uint32_t count, const uint32_t* ends,
                                           uint32_t pieces, const PlotSamples* samples, const ContourGrid* contour) noexcept
{
    geometry(points, count);
    targetGeometryEnds = ends;
    targetGeometryPieces = pieces;
    targetGeometryPieced = true;
    if (samples) targetSamples = *samples;
    if (contour) { targetContour = *contour; targetContoured = true; }
    return *this;
}

AnimationTarget& AnimationTarget::arc(float radians) noexcept
{
    targetArc = radians;
    return *this;
}

static std::atomic<uint64_t> _sceneSerial{0};

Scene::Scene(const Config& config) noexcept
{
    pImpl = new (std::nothrow) Impl;
    if (pImpl) {
        pImpl->serial = ++_sceneSerial;
        pImpl->cfg = config;
        pImpl->sceneTheme.background = config.background;
        pImpl->initialCamera = config.camera;
        pImpl->initialView = config.cameraView;
    }
}

Scene::~Scene()
{
    delete pImpl;
}

// ---------- Creating scenes and attaching objects ----------

Scene* Scene::gen(const Config& config) noexcept
{
    if (!valid(config)) return nullptr;
    auto scene = new (std::nothrow) Scene(config);
    // Instance methods rely on the fully initialized state published here.
    if (!scene || scene->pImpl) return scene;
    delete scene;
    return nullptr;
}

Result Scene::theme(const Theme& theme) noexcept
{
    if (!_valid(theme)) return Result::InvalidArguments;
    if (pImpl->owner || pImpl->objectCnt || pImpl->viewportCnt || pImpl->sequence
        || pImpl->definitionsSealed) {
        return Result::InsufficientCondition;
    }
    if (!pImpl->assign(theme)) return Result::OutOfMemory;
    return Result::Success;
}

const Theme& Scene::theme() const noexcept
{
    if (pImpl) return pImpl->sceneTheme;
    static const Theme fallback;
    return fallback;
}

static bool _treeCount(Object* object, uint32_t& count, uint32_t depth)
{
    if (!object || depth > MAX_OBJECT_DEPTH || count >= MAX_OBJECTS) return false;
    count++;
    for (auto i = 0u; i < object->childCount(); i++) {
        if (!_treeCount(object->childAt(i), count, depth + 1u)) return false;
    }
    return true;
}

static void _tree(Object* object, Object** objects, uint32_t& count)
{
    objects[count++] = object;
    for (auto i = 0u; i < object->childCount(); i++)
        _tree(object->childAt(i), objects, count);
}

static bool _contains(Object* const* objects, uint32_t count, const Object* object)
{
    for (auto i = 0u; i < count; i++) {
        if (objects[i] == object) return true;
    }
    return false;
}

static const Object* _parent(const Object* object, const Object* root, const Object* proposedParent)
{
    if (object == root) return proposedParent;
    return object ? object->parent() : nullptr;
}

static bool _inside(const Object* object, const Object* ancestor, const Object* root = nullptr,
                    const Object* proposedParent = nullptr)
{
    for (auto current = object; current; current = _parent(current, root, proposedParent)) {
        if (current == ancestor) return true;
    }
    return false;
}

static bool _connectorAncestors(const Connector* connector, const Object* root,
                                const Object* proposedParent)
{
    for (auto ancestor = _parent(connector, root, proposedParent); ancestor;
         ancestor = _parent(ancestor, root, proposedParent)) {
        if (_identity(ancestor->model)) continue;
        if (!_inside(connector->from(), ancestor, root, proposedParent)
            && !_inside(connector->to(), ancestor, root, proposedParent)) {
            return false;
        }
    }
    return true;
}

static bool _connectorTransform(const Object* object, const Object* target)
{
    if (object->type() == Type::Connector) {
        auto connector = static_cast<const Connector*>(object);
        if (!_inside(connector->from(), target) && !_inside(connector->to(), target)) return false;
    }
    for (auto i = 0u; i < object->childCount(); i++) {
        if (!_connectorTransform(object->childAt(i), target)) return false;
    }
    return true;
}

static bool _transformable(const Object* object)
{
    return object && object->type() != Type::Connector && _connectorTransform(object, object);
}

Result Scene::attach(Object* object, Object* parent) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!object || object->objOwner || object->objParent) return pImpl->fail(Result::InvalidArguments, ADD_OWNED);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto depth = 0u;
    if (parent) {
        auto parentEntry = pImpl->entry(parent);
        if (parent->objOwner != this || !parentEntry || parentEntry->dead >= 0.0f) {
            return pImpl->fail(Result::InvalidArguments, ADD_PARENT);
        }
        for (auto ancestor = parent; ancestor; ancestor = ancestor->parent()) {
            if (depth++ >= MAX_OBJECT_DEPTH) return pImpl->fail(Result::InsufficientCondition, ADD_DEPTH);
            auto ancestorEntry = pImpl->entry(ancestor);
            if (ancestor->objOwner != this) return pImpl->fail(Result::InvalidArguments, ADD_PARENT);
            if (auto reason = _unplayable(ancestorEntry, ancestor)) return pImpl->fail(Result::InvalidArguments, reason);
        }
    }

    auto count = 0u;
    if (!_treeCount(object, count, depth)) return pImpl->fail(Result::InsufficientCondition, ADD_DEPTH);
    if (count > MAX_OBJECTS - pImpl->objectCnt) return pImpl->fail(Result::InsufficientCondition, ADD_COUNT);
    auto tree = new (std::nothrow) Object*[count];
    if (!tree) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    auto index = 0u;
    _tree(object, tree, index);
    for (auto i = 0u; i < count; i++) {
        auto item = tree[i];
        if (!valid(item) || item->objOwner || pImpl->entry(item)) {
            delete[] tree;
            return pImpl->fail(Result::InvalidArguments, valid(item) ? ADD_OWNED : TARGET_INVALID);
        }
        auto tag = item->tag();
        if (!tag) continue;
        if (this->object(tag)) {
            delete[] tree;
            return pImpl->fail(Result::InvalidArguments, ADD_TAG);
        }
        for (auto j = 0u; j < i; j++) {
            auto previous = tree[j]->tag();
            if (previous && std::strcmp(previous, tag) == 0) {
                delete[] tree;
                return pImpl->fail(Result::InvalidArguments, ADD_TAG);
            }
        }
    }
    auto longestSpan = 0.0f;
    for (auto i = 0u; i < count; i++) {
        auto span = sampleDuration(tree[i]);
        if (span > longestSpan) longestSpan = span;
    }
    auto nextCursor = pImpl->cursor;
    if (longestSpan > 0.0f) {
        auto next = _decimal(pImpl->cursor) + _decimal(longestSpan);
        if (!std::isfinite(next) || next > std::numeric_limits<float>::max()) {
            delete[] tree;
            return pImpl->fail(Result::InvalidArguments, TIMELINE_RANGE);
        }
        nextCursor = static_cast<float>(next);
        if (nextCursor <= pImpl->cursor) {
            delete[] tree;
            return pImpl->fail(Result::InvalidArguments, TIMELINE_RANGE);
        }
    }
    for (auto i = 0u; i < count; i++) {
        if (tree[i]->type() != Type::Connector) continue;
        auto connector = static_cast<Connector*>(tree[i]);
        Object* endpoints[2] = {connector->from(), connector->to()};
        for (auto endpoint : endpoints) {
            auto entry = pImpl->entry(endpoint);
            auto existing = endpoint && endpoint->objOwner == this && entry && entry->dead < 0.0f;
            if (!existing && !_contains(tree, count, endpoint)) {
                delete[] tree;
                return pImpl->fail(Result::InvalidArguments, ADD_ENDPOINT);
            }
        }
        if (!_connectorAncestors(connector, object, parent)) {
            delete[] tree;
            return pImpl->fail(Result::InvalidArguments, ADD_CONNECTOR);
        }
    }

    auto fonts = new (std::nothrow) char*[count]{};
    if (!fonts) {
        delete[] tree;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < count; i++) {
        if (tree[i]->type() != Type::Text) continue;
        auto text = static_cast<Text*>(tree[i]);
        if (text->font()) continue;
        auto& typography = _textTheme(pImpl->sceneTheme, text->role);
        fonts[i] = _duplicate(_textFont(typography, text));
        if (fonts[i]) continue;
        for (auto j = 0u; j < count; j++)
            delete[] fonts[j];
        delete[] fonts;
        delete[] tree;
        return Result::OutOfMemory;
    }
    if (!grow(pImpl->objects, pImpl->objectCnt, pImpl->objectCap, count, 8u, MAX_OBJECTS)) {
        for (auto i = 0u; i < count; i++)
            delete[] fonts[i];
        delete[] fonts;
        delete[] tree;
        return Result::OutOfMemory;
    }

    // A color or width that differs from the default was set directly; remember that before
    // the theme and the groups above fill in the rest.
    for (auto i = 0u; i < count; i++) {
        auto item = tree[i];
        if (item->objCopied) continue;
        auto defaults = _defaultStyle(item->type());
        if (!_equal(item->style.stroke, defaults.stroke)) item->objStrokeAuthored = true;
        if (!_equal(item->style.fill, defaults.fill)) item->objFillAuthored = true;
        if (item->style.width != defaults.width) item->objWidthAuthored = true;
    }
    // The nearest group above item that sets property; the attached root hangs below parent.
    auto source = [object, parent](const Object* item, Inherited property) -> const Object* {
        for (auto above = item == object ? parent : item->objParent; above;
             above = above == object ? parent : above->objParent) {
            if (above->type() != Type::Group) continue;
            auto set = property == Inherited::Stroke ? above->objStrokeAuthored
                     : property == Inherited::Fill   ? above->objFillAuthored
                                                     : above->objWidthAuthored;
            if (set) return above;
        }
        return nullptr;
    };
    // Whether group is typeset or lies below a typeset group, whose texts are one block.
    auto block = [object, parent](const Object* group) {
        for (auto above = group; above; above = above == object ? parent : above->objParent) {
            if (above->type() == Type::Group && static_cast<const Group*>(above)->typeset) return true;
        }
        return false;
    };

    auto colorCursor = pImpl->colorCursor;
    for (auto i = 0u; i < count; i++) {
        auto item = tree[i];
        if (item->objCopied) {
            // A copy keeps the size and colors of what it copies.
            if (fonts[i]) static_cast<Text*>(item)->objFont = fonts[i];
            continue;
        }
        if (item->type() == Type::Text) {
            auto text = static_cast<Text*>(item);
            auto& typography = _textTheme(pImpl->sceneTheme, text->role);
            if (fonts[i]) text->objFont = fonts[i];
            if (text->size == 0.0f) text->size = typography.size;
            if (!item->objColorAuthored && !_colorChanged(item)) {
                // A block of text takes the fill a group in it sets, as shapes take a group's.
                auto group = source(item, Inherited::Fill);
                text->style.fill = group && block(group) ? group->style.fill : typography.color;
                text->style.stroke.a = 0;
            }
            continue;
        }
        if (item->type() == Type::Space) {
            auto space = static_cast<Space*>(item);
            auto defaults = _defaultStyle(Type::Space);
            if (!item->objColorAuthored && _equal(space->style.stroke, defaults.stroke)) {
                space->style.stroke = pImpl->sceneTheme.axis.grid;
            }
            auto axes = AxisTheme{};
            if (_equal(space->axisX, axes.x)) space->axisX = pImpl->sceneTheme.axis.x;
            if (_equal(space->axisY, axes.y)) space->axisY = pImpl->sceneTheme.axis.y;
            if (_equal(space->axisZ, axes.z)) space->axisZ = pImpl->sceneTheme.axis.z;
            if (_equal(space->numberColor, axes.label)) {
                space->numberColor = pImpl->sceneTheme.axis.label;
            }
            continue;
        }
        if (_palette(item->type())) {
            auto defaults = _defaultStyle(item->type());
            if (!item->objWidthAuthored && item->style.width == defaults.width) {
                auto group = source(item, Inherited::Width);
                item->style.width = group ? group->style.width : pImpl->sceneTheme.objectWidth;
            }
            if (!item->objGradientAuthored
                && (item->style.gradient != defaults.gradient
                    || !_equal(item->style.gradientEnd, defaults.gradientEnd))) {
                item->objGradientAuthored = true;
                item->objGradientEndAuthored =
                    !_equal(item->style.gradientEnd, defaults.gradientEnd);
            }
            if (!item->objGradientAuthored) {
                item->style.gradient = {};
                if (pImpl->sceneTheme.gradient) item->style.gradient.type = GradientType::Linear;
                item->style.gradientEnd = pImpl->sceneTheme.endGradientStop;
                item->objThemeGradient = true;
            } else if (item->style.gradient.enabled() && !item->objGradientEndAuthored) {
                item->style.gradientEnd = pImpl->sceneTheme.endGradientStop;
            }
        }
        if (!_palette(item->type())) continue;
        auto inherited = false;
        if (!item->objStrokeAuthored) {
            if (auto group = source(item, Inherited::Stroke)) {
                item->style.stroke = group->style.stroke;
                inherited = true;
            }
        }
        if (!item->objFillAuthored && _fillable(item)) {
            if (auto group = source(item, Inherited::Fill)) {
                item->style.fill = group->style.fill;
                inherited = true;
            }
        }
        if (inherited || item->objColorAuthored || item->objStrokeAuthored || item->objFillAuthored) continue;
        auto color = pImpl->sceneTheme.objects[colorCursor];
        auto defaults = _defaultStyle(item->type());
        if (defaults.stroke.a) item->style.stroke = color;
        if (defaults.fill.a) item->style.fill = color;
        item->objThemeColor = true;
        colorCursor = (colorCursor + 1u) % pImpl->sceneTheme.objectCount;
    }
    pImpl->colorCursor = colorCursor;

    object->objParent = parent;
    for (auto i = 0u; i < count; i++) {
        auto item = tree[i];
        item->objId = pImpl->objectCnt + 1u;
        item->objOwner = this;
        item->objCopied = false;
        auto& entry = pImpl->objects[pImpl->objectCnt++];
        entry.object = item;
        entry.base = _state(item);
        entry.fingerprint = fingerprint(item);
        entry.born = pImpl->cursor;
    }
    // The tree lists each object before the objects below it, so going back through it adds
    // every subtree to its parent's family before that family is added to its own parent.
    auto first = pImpl->objectCnt - count;
    for (auto i = count; i-- > 1u;) {
        pImpl->entry(tree[i]->objParent)->family += pImpl->objects[first + i].family;
    }
    // The objects above the new ones read their clips for each of them too; they have none yet.
    uint64_t above = 0;
    for (auto ancestor = parent; ancestor; ancestor = ancestor->objParent) {
        auto item = pImpl->entry(ancestor);
        item->family += count;
        above += item->clipCount;
    }
    pImpl->work += above * count;
    if (longestSpan > 0.0f) {
        pImpl->advance(nextCursor);
        pImpl->definitionsSealed = true;
    }
    delete[] fonts;
    delete[] tree;
    return Result::Success;
}

Result Scene::add(Object* object) noexcept
{
    return attach(object, nullptr);
}

static constexpr auto CLONE_SHOWN = "the object does not show at the cursor, so there is nothing to clone";
static constexpr auto CLONE_MORPH = "the object is in the middle of a morph; clone it once the morph ends";
static constexpr auto CLONE_WARP = "the object is in the middle of a warp; clone it once the warp ends";
static constexpr auto CLONE_MASK =
    "a mask and the object it masks are cloned together; clone an object above both";
static constexpr auto CLONE_GROUP = "a group made by another module, such as a panel, cannot be cloned yet";
static constexpr auto CLONE_CONNECTOR = "a connector that ends at another connector cannot be cloned yet";
static constexpr auto CLONE_IDS = "every id the clone could derive is taken; give the copy an id";
static constexpr auto CLONE_EDIT = "the copy's settings could not be changed";
static constexpr auto CLONE_TREE = "the copy's settings may change, but not the objects below it";
// How many numbered ids a clone tries, from <id>:copy to <id>:copy4096.
static constexpr uint32_t CLONE_ID_LIMIT = 4096;

// ---------- Cloning ----------

// One object of a clone: the source, the state it shows at the cursor and its copy.
struct CloneItem
{
    Object* source = nullptr;
    Object* copy = nullptr;
    // The item of the source's parent, or UINT32_MAX for the root.
    uint32_t parent = UINT32_MAX;
    VisualState state;
    // The copy's style before the edit, which tells what the edit set, and what of it a group
    // passes down (TARGET_STROKE, TARGET_FILL and TARGET_WIDTH).
    Style style;
    uint32_t passed = 0;
    char* tag = nullptr;
    bool shown = false;
};

struct CloneItems
{
    CloneItem* data = nullptr;
    uint32_t count = 0;
    // Once attached, the copies belong to the scene; before, those below no other copy go here.
    bool attached = false;

    ~CloneItems()
    {
        // A copy below another goes with it, so only those below none are deleted.
        for (auto i = 0u; data && !attached && i < count; i++) {
            if (data[i].copy && data[i].copy->parent()) data[i].copy = nullptr;
        }
        for (auto i = 0u; data && i < count; i++) {
            if (!attached) delete data[i].copy;
            delete[] data[i].tag;
        }
        delete[] data;
    }
};

// Lists the objects of the tree of object in pre-order, each after its parent.
static uint32_t _cloneTree(Object* object, CloneItem* items, uint32_t count, uint32_t parent)
{
    auto index = count++;
    items[index].source = object;
    items[index].parent = parent;
    for (auto i = 0u; i < object->childCount(); i++)
        count = _cloneTree(object->childAt(i), items, count, index);
    return count;
}

// The copy of an object of the clone, or the object itself when it is outside.
static Object* _cloneOf(const CloneItem* items, uint32_t count, Object* object)
{
    for (auto i = 0u; i < count; i++) {
        if (items[i].source == object) return items[i].copy;
    }
    return object;
}

// Whether the tree of object is still the copies, in the order the clone lists them.
static bool _cloneSame(const Object* object, const CloneItem* items, uint32_t count, uint32_t& index)
{
    if (index >= count || items[index].copy != object) return false;
    index++;
    for (auto i = 0u; i < object->childCount(); i++) {
        if (!_cloneSame(object->childAt(i), items, count, index)) return false;
    }
    return true;
}

// The id the copy of an object with tag takes below a copy with id root of a source with id
// origin; under a copy without an id, the tag with :copy and number after it.
static char* _cloneTag(const char* tag, const char* origin, const char* root, uint32_t number)
{
    char suffix[16] = "";
    if (number > 1u) std::snprintf(suffix, sizeof(suffix), "%u", number);
    const char* parts[3] = {tag, ":copy", suffix};
    if (root) {
        auto length = origin ? std::strlen(origin) : 0u;
        auto nested = origin && std::strncmp(tag, origin, length) == 0 && tag[length] == '/';
        parts[0] = root;
        parts[1] = nested ? tag + length : "/";
        parts[2] = nested ? "" : tag;
    }
    auto size = std::strlen(parts[0]) + std::strlen(parts[1]) + std::strlen(parts[2]) + 1u;
    auto output = new (std::nothrow) char[size];
    if (output) std::snprintf(output, size, "%s%s%s", parts[0], parts[1], parts[2]);
    return output;
}

// Gives each item the id its copy takes with number (see Scene::clone()); false when memory
// runs out.
static bool _cloneTags(CloneItem* items, uint32_t count, const char* given, uint32_t number)
{
    for (auto i = 0u; i < count; i++) {
        delete[] items[i].tag;
        items[i].tag = nullptr;
    }
    auto origin = items[0].source->tag();
    auto root = given ? _duplicate(given) : origin ? _cloneTag(origin, nullptr, nullptr, number) : nullptr;
    if ((given || origin) && !root) return false;
    items[0].tag = root;
    for (auto i = 1u; i < count; i++) {
        auto tag = items[i].source->tag();
        if (!tag) continue;
        items[i].tag = _cloneTag(tag, origin, root, number);
        if (!items[i].tag) return false;
    }
    return true;
}

// Brings a sampled state within the ranges the fields of an object of kind hold: a curve that
// overshoots, such as back or elastic, carries values past their ends on the way.
static void _settle(VisualState& state, Type kind)
{
    constexpr auto Least = 1.0e-6f;
    state.opacity = _clamp(state.opacity);
    state.progress = _clamp(state.progress);
    state.fillProgress = _clamp(state.fillProgress);
    state.width = std::fmax(state.width, 0.0f);
    state.radius = std::fmax(state.radius, Least);
    state.markerTail = std::fmax(state.markerTail, 0.0f);
    state.markerTip = std::fmax(state.markerTip, 0.0f);
    state.pixelScale = std::fmax(state.pixelScale, 0.0f);
    state.blur = std::fmin(std::fmax(state.blur, 0.0f), EFFECT_SIGMA_LIMIT);
    state.shadow.blur = std::fmin(std::fmax(state.shadow.blur, 0.0f), EFFECT_SIGMA_LIMIT);
    auto least = kind == Type::Text ? 0.0f : Least;
    state.size = {std::fmax(state.size.x, least), std::fmax(state.size.y, least)};
    state.trimDistance = {std::fmax(state.trimDistance.x, 0.0f), std::fmax(state.trimDistance.y, 0.0f)};
    state.corner = std::fmin(std::fmax(state.corner, 0.0f), std::fmin(state.size.x, state.size.y) * 0.5f);
}

Object* Scene::Impl::replica(const Object* source, const VisualState& state, Object* from, Object* to) noexcept
{
    Object* copy = nullptr;
    switch (source->type()) {
        case Type::Point: copy = Point::gen(static_cast<const Point*>(source)->point); break;
        case Type::Line: {
            auto line = static_cast<const Line*>(source);
            copy = Line::gen(line->from, line->to);
            break;
        }
        case Type::Arrow: {
            auto arrow = static_cast<const Arrow*>(source);
            copy = Arrow::gen(arrow->from, arrow->to);
            break;
        }
        case Type::Vector: {
            auto vector = static_cast<const Vector*>(source);
            copy = Vector::gen(vector->value, vector->origin);
            break;
        }
        case Type::Arc: {
            auto arc = static_cast<const Arc*>(source);
            auto replica = Arc::gen(arc->center, arc->radius, arc->start, arc->angle);
            if (replica) replica->sector = arc->sector;
            copy = replica; break;
        }
        case Type::Angle: {
            auto angle = static_cast<const Angle*>(source);
            auto replica = Angle::gen(angle->vertex, angle->from, angle->to, angle->radius);
            if (replica) { replica->other = angle->other; replica->right = angle->right; replica->labelDistance = angle->labelDistance; }
            copy = replica; break;
        }
        case Type::Circle: {
            auto circle = static_cast<const Circle*>(source);
            copy = Circle::gen(circle->center, circle->radius);
            break;
        }
        case Type::Polygon: {
            auto polygon = static_cast<const Polygon*>(source);
            copy = Polygon::gen(polygon->points(), polygon->count());
            break;
        }
        case Type::Plot: {
            // A plot that took a geometry starts empty, as one left empty does, and takes the
            // shape of its state below, so the copy has room for the points it shows and no
            // more: the plot's own points are those its latest clip ends at, which may be many
            // more.
            auto plot = static_cast<const Plot*>(source);
            auto replica = plot->objCount && !state.geometry
                               ? Plot::gen(plot->points(), plot->count(),
                                           plot->objContours > 1u ? plot->objEnds : nullptr, plot->objContours,
                                           &plot->objSamples)
                               : new (std::nothrow) Plot(nullptr, 0u);
            if (replica) {
                replica->flat = plot->flat;
                replica->endpointRadius = plot->endpointRadius;
                replica->objGrid = plot->objGrid;
                replica->objLabels = plot->objLabels;
                std::memcpy(replica->objLabelFormat, plot->objLabelFormat, sizeof(plot->objLabelFormat));
                replica->objLabels.format = replica->objLabelFormat;
                replica->objLabelled = plot->objLabelled;
                replica->objLabelCount = plot->objLabelCount;
            }
            copy = replica;
            break;
        }
        case Type::Route: {
            auto route = static_cast<const DirectedRoute*>(source);
            copy = DirectedRoute::gen(route->points(), route->count());
            break;
        }
        case Type::Path: {
            auto path = static_cast<const Path*>(source);
            auto points = new (std::nothrow) Vec3[path->objCount];
            auto ends = path->objEnds ? new (std::nothrow) uint32_t[path->objContours] : nullptr;
            if (points && (ends || !path->objEnds)) {
                std::memcpy(points, path->objPoints, sizeof(Vec3) * path->objCount);
                if (ends) std::memcpy(ends, path->objEnds, sizeof(uint32_t) * path->objContours);
                copy = new (std::nothrow) Path(points, path->objCount, path->objClosed, ends, path->objContours);
            }
            if (!copy) {
                delete[] points;
                delete[] ends;
            }
            break;
        }
        case Type::Curve: {
            auto curve = static_cast<const Curve*>(source);
            auto points = new (std::nothrow) Vec3[curve->objCount];
            if (points) {
                std::memcpy(points, curve->objPoints, sizeof(Vec3) * curve->objCount);
                copy = new (std::nothrow) Curve(points, curve->objCount);
            }
            if (!copy) delete[] points;
            else std::memcpy(static_cast<Curve*>(copy)->objControls, curve->objControls, sizeof(curve->objControls));
            break;
        }
        case Type::SurfaceMesh: {
            auto surface = static_cast<const SurfaceMesh*>(source);
            auto mesh = SurfaceMesh::gen(surface->points(), surface->columns(), surface->rows());
            if (mesh) {
                mesh->mode = surface->mode;
                mesh->shading = surface->shading;
            }
            copy = mesh;
            break;
        }
        case Type::Text: {
            auto text = static_cast<const Text*>(source);
            Text* replica = nullptr;
            if (text->objCounting) {
                // A counting copy counts on from the value it shows.
                replica = Text::gen("", text->point);
                if (replica && replica->count(state.value, text->objFormat, text->objPrefix, text->objSuffix)
                                   != Result::Success) {
                    delete replica;
                    return nullptr;
                }
            } else {
                const char* value = nullptr;
                char* written = nullptr;
                if (shown(text, state, value, written) != Result::Success) return nullptr;
                replica = Text::gen(value, text->point);
                delete[] written;
            }
            if (!replica) return nullptr;
            replica->align = text->align;
            replica->orientation = text->orientation;
            replica->role = text->role;
            replica->box = text->box;
            replica->boxOffset = text->boxOffset;
            replica->slant = text->slant;
            replica->tabular = text->tabular;
            replica->objScreenGap = text->objScreenGap;
            if (text->objFont && replica->font(text->objFont) != Result::Success) {
                delete replica;
                return nullptr;
            }
            copy = replica;
            break;
        }
        case Type::Picture: {
            auto picture = static_cast<const Picture*>(source);
            auto image = new (std::nothrow) Picture(picture->center, picture->width);
            if (!image) return nullptr;
            image->filter = picture->filter;
            image->objWidth = picture->objWidth;
            image->objHeight = picture->objHeight;
            auto copied = true;
            if (picture->objEncoded) {
                image->objEncodedSize = picture->objEncodedSize;
                image->objEncoded = new (std::nothrow) uint8_t[picture->objEncodedSize];
                image->objMime = _duplicate(picture->objMime);
                copied = image->objEncoded && image->objMime;
                if (copied) std::memcpy(image->objEncoded, picture->objEncoded, picture->objEncodedSize);
            } else if (picture->objPixels) {
                auto pixels = static_cast<size_t>(picture->objWidth) * picture->objHeight;
                image->objPixels = new (std::nothrow) uint32_t[pixels];
                copied = image->objPixels;
                if (copied) std::memcpy(image->objPixels, picture->objPixels, pixels * sizeof(uint32_t));
            }
            if (!copied) {
                delete image;
                return nullptr;
            }
            copy = image;
            break;
        }
        case Type::Group: {
            auto group = Group::gen();
            if (group) group->typeset = static_cast<const Group*>(source)->typeset;
            copy = group;
            break;
        }
        case Type::Rectangle: {
            auto rectangle = static_cast<const Rectangle*>(source);
            copy = Rectangle::gen(rectangle->center, rectangle->size, rectangle->corner);
            break;
        }
        case Type::Connector: {
            auto connector = Connector::gen(from, to);
            if (connector) connector->padding = static_cast<const Connector*>(source)->padding;
            copy = connector;
            break;
        }
        case Type::Space:
        case Type::Cell: break;
    }
    if (!copy) return nullptr;
    copy->style = source->style;
    copy->layer = source->layer;
    copy->objColorAuthored = source->objColorAuthored;
    copy->objStrokeAuthored = source->objStrokeAuthored;
    copy->objFillAuthored = source->objFillAuthored;
    copy->objWidthAuthored = source->objWidthAuthored;
    copy->objGradientAuthored = source->objGradientAuthored;
    copy->objGradientEndAuthored = source->objGradientEndAuthored;
    copy->objThemeGradient = source->objThemeGradient;
    copy->objThemeColor = source->objThemeColor;
    copy->objIsolated = source->objIsolated;
    copy->objMaskMode = source->objMaskMode;
    copy->objBlend = source->objBlend;
    copy->objEffects = source->objEffects;
    copy->objCopied = true;
    // A copy of a label shows as the space that placed it shows, while it lies below it.
    copy->objLabelOf = source->objLabelOf;
    // The warp the source shows, which has ended, is the copy's from the start: the scene keeps
    // its points in the local space both share, and the copy's own warps go on from them.
    copy->objWarp = copy->objBornWarp = state.warp;
    // The copy's string is its own; the state points at the source's. So are its points: those
    // the source shows, on their way too, which the copy keeps as its own geometry.
    auto values = state;
    values.text = nullptr;
    values.textFrom = nullptr;
    values.geometry = nullptr;
    values.geometryFrom = nullptr;
    values.geometryProgress = 1.0f;
    store(copy, values);
    if (state.geometry) {
        auto shape = copy->type() == Type::Plot ? plotAt(state) : *state.geometry;
        auto points = shape.count ? new (std::nothrow) Vec3[shape.count] : nullptr;
        auto ends = shape.pieces > 1u ? new (std::nothrow) uint32_t[shape.pieces] : nullptr;
        auto samples = state.geometry->samples;
        auto samplePoints = samples.count ? new (std::nothrow) Vec3[samples.count] : nullptr;
        shape.samples = samples;
        if ((shape.count && !points) || (shape.pieces > 1u && !ends)
            || (samples.count && !samplePoints) || !room(copy, shape)) {
            delete[] points;
            delete[] ends;
            delete[] samplePoints;
            delete copy;
            return nullptr;
        }
        if (copy->type() == Type::Plot) plotAt(state, points, ends);
        else shapeAt(state, shape.count, points);
        shape.points = points;
        shape.ends = ends;
        if (samples.count) {
            if (state.geometryFrom && sameGrid(*state.geometryFrom, *state.geometry)) plotSamplesAt(state, samplePoints);
            else std::memcpy(samplePoints, samples.points, sizeof(Vec3) * samples.count);
            shape.samples.points = samplePoints;
        }
        writeShape(copy, shape);
        delete[] points;
        delete[] ends;
        delete[] samplePoints;
    }
    return copy;
}

Result Scene::clone(Object* source, Object*& output, const CloneOptions& options) noexcept
{
    output = nullptr;
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto entry = source ? impl.entry(source) : nullptr;
    if (!entry || source->objOwner != this) return impl.fail(Result::InvalidArguments, TARGET_FOREIGN);
    if (options.parent) {
        auto parentEntry = impl.entry(options.parent);
        if (options.parent->objOwner != this || !parentEntry || parentEntry->dead >= 0.0f) {
            return impl.fail(Result::InvalidArguments, ADD_PARENT);
        }
    }
    // Below an object that does not show at the cursor, nothing does.
    for (auto above = source->objParent; above; above = above->objParent) {
        auto aboveEntry = impl.entry(above);
        VisualState state;
        if (!aboveEntry || !impl.sample(*aboveEntry, impl.cursor, state)) {
            return impl.fail(Result::InsufficientCondition, CLONE_SHOWN);
        }
    }
    auto parent = options.parent ? options.parent : source->objParent;
    auto count = 0u;
    if (!_treeCount(source, count, 0u)) return impl.fail(Result::InsufficientCondition, ADD_DEPTH);
    CloneItems items;
    items.data = new (std::nothrow) CloneItem[count];
    if (!items.data) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    items.count = count;
    _cloneTree(source, items.data, 0u, UINT32_MAX);

    // What each object shows at the cursor; below an object that does not show, none does.
    for (auto i = 0u; i < count; i++) {
        auto& item = items.data[i];
        auto kind = item.source->type();
        if (kind == Type::Space || kind == Type::Cell) {
            std::snprintf(impl.reason, sizeof(impl.reason), "a %s cannot be cloned yet", type(kind));
            return impl.fail(Result::NonSupport, impl.reason);
        }
        if (kind == Type::Group && !item.source->objPlain) return impl.fail(Result::NonSupport, CLONE_GROUP);
        auto itemEntry = impl.entry(item.source);
        if (auto reason = _unplayable(itemEntry, item.source, true)) return impl.fail(Result::InvalidArguments, reason);
        item.shown = (i == 0u || items.data[item.parent].shown) && impl.sample(*itemEntry, impl.cursor, item.state);
        if (!item.shown) {
            if (i == 0u) return impl.fail(Result::InsufficientCondition, CLONE_SHOWN);
            item.state = _state(item.source);
        } else if (item.state.morph) {
            return impl.fail(Result::InsufficientCondition, CLONE_MORPH);
        } else if (item.state.warp && item.state.warpProgress != 1.0f) {
            // A copy keeps the images of a warp, not points on their way to them.
            return impl.fail(Result::InsufficientCondition, CLONE_WARP);
        }
        if ((item.source->objMask && !_inside(item.source->objMask, source))
            || (item.source->objMasked && !_inside(item.source->objMasked, source))) {
            return impl.fail(Result::InvalidArguments, CLONE_MASK);
        }
        if (kind == Type::Connector) {
            auto connector = static_cast<const Connector*>(item.source);
            const Object* ends[] = {connector->from(), connector->to()};
            for (auto end : ends) {
                if (end->type() == Type::Connector && _inside(end, source)) {
                    return impl.fail(Result::NonSupport, CLONE_CONNECTOR);
                }
            }
        }
        _settle(item.state, kind);
    }

    // A clone's sampled models become its new reference models. Keep each billboard's
    // painted font size when that reference changes, before the caller edits the snapshot.
    auto parentReference = parent ? impl.billboardReference(parent) : Mat4::identity();
    VisualState parentState;
    parentState.model = Mat4::identity();
    auto parentSampled = false;
    for (auto i = 0u; i < count; i++) {
        auto& item = items.data[i];
        if (!item.shown || item.source->type() != Type::Text
            || static_cast<const Text*>(item.source)->orientation != TextOrientation::Billboard) continue;
        if (parent && !parentSampled) {
            bool visible;
            auto result = impl.world(parent, impl.cursor, parentState, visible);
            if (result != Result::Success) return impl.fail(result, TARGET_INVALID);
            if (!visible) parentState.model = _parentWorld(parent) * parent->model;
            parentSampled = true;
        }
        VisualState original;
        bool visible;
        auto result = impl.world(item.source, impl.cursor, original, visible);
        if (result != Result::Success) return impl.fail(result, TARGET_INVALID);
        auto local = item.state.model;
        auto pixels = item.state.pixelScale;
        for (auto above = item.parent; above != UINT32_MAX; above = items.data[above].parent) {
            local = items.data[above].state.model * local;
            if (items.data[above].source->type() == Type::Group) pixels *= items.data[above].state.pixelScale;
        }
        // Copies retain their label owner; a placed label also inherits its Space's pixels.
        for (auto above = parent; above; above = above->parent()) {
            auto labels = above->type() == Type::Angle
                          || (above->type() == Type::Plot && static_cast<const Plot*>(above)->contourLabels());
            if (above->type() == Type::Space) {
                for (auto below = item.source; below; below = below->parent()) {
                    if (below->objLabelOf == above) { labels = true; break; }
                }
            }
            if (above->type() != Type::Group && !labels) continue;
            VisualState sampled;
            if (impl.sample(*impl.entry(above), impl.cursor, sampled)) pixels *= sampled.pixelScale;
        }
        auto from = billboardScale(impl.billboardReference(item.source), original.model, original.pixelScale);
        auto to = billboardScale(parentReference * local, parentState.model * local, pixels);
        if (from == to) continue;
        // A collapsed snapshot has no ink; a singular new reference uses pixelScale instead.
        if (from == 0.0f) { item.state.pixelScale = 0.0f; continue; }
        auto size = item.state.size.x * static_cast<double>(from) / to;
        if (!std::isfinite(size) || !(static_cast<float>(size) > 0.0f)
            || size > std::numeric_limits<float>::max()) {
            return impl.fail(Result::InvalidArguments, TARGET_INVALID);
        }
        item.state.size = {static_cast<float>(size), static_cast<float>(size)};
    }

    // The ids: the given one, or the first number whose ids are all free.
    for (auto number = 1u;; number++) {
        if (number > CLONE_ID_LIMIT) return impl.fail(Result::InvalidArguments, CLONE_IDS);
        if (!_cloneTags(items.data, count, options.tag, number)) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
        if (options.tag) break;
        auto free = true;
        for (auto i = 0u; free && i < count; i++) free = !items.data[i].tag || !object(items.data[i].tag);
        if (free) break;
    }

    // The copies, connectors last so that the endpoints they join below source exist.
    for (auto pass = 0u; pass < 2u; pass++) {
        for (auto i = 0u; i < count; i++) {
            auto& item = items.data[i];
            auto connector = item.source->type() == Type::Connector;
            if (connector != (pass == 1u)) continue;
            Object* ends[2] = {};
            if (connector) {
                auto original = static_cast<Connector*>(item.source);
                ends[0] = _cloneOf(items.data, count, original->from());
                ends[1] = _cloneOf(items.data, count, original->to());
            }
            item.copy = Impl::replica(item.source, item.state, ends[0], ends[1]);
            if (!item.copy) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
            if (item.tag) {
                item.copy->tag(item.tag);
                if (!item.copy->tag()) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
            }
        }
    }
    for (auto i = 1u; i < count; i++) {
        auto& item = items.data[i];
        auto result = items.data[item.parent].copy->add(item.copy);
        if (result != Result::Success) {
            return impl.fail(result, result == Result::OutOfMemory ? SCENE_MEMORY : TARGET_INVALID);
        }
    }

    auto root = items.data[0].copy;
    if (options.edit) {
        for (auto i = 0u; i < count; i++)
            items.data[i].style = items.data[i].copy->style;
        auto result = options.edit(root, options.data);
        if (root->objOwner || root->objParent) {
            // The edit gave the copy away; it is no longer the clone's to delete.
            items.attached = true;
            return impl.fail(Result::InvalidArguments, CLONE_TREE);
        }
        if (result != Result::Success) return impl.fail(result, CLONE_EDIT);
        auto index = 0u;
        if (!_cloneSame(root, items.data, count, index) || index != count) {
            return impl.fail(Result::InvalidArguments, CLONE_TREE);
        }
        // A color, width or gradient the edit gave a copy is the copy's own now, and a group
        // passes it to the copies below that take it from the group, as when it is added.
        for (auto i = 0u; i < count; i++) {
            auto& item = items.data[i];
            auto copy = item.copy;
            auto stroke = !_equal(item.style.stroke, copy->style.stroke);
            auto fill = !_equal(item.style.fill, copy->style.fill);
            auto width = item.style.width != copy->style.width;
            auto gradient = item.style.gradient != copy->style.gradient;
            auto end = !_equal(item.style.gradientEnd, copy->style.gradientEnd);
            if (stroke) copy->objColorAuthored = copy->objStrokeAuthored = true;
            if (fill) copy->objColorAuthored = copy->objFillAuthored = true;
            if (width) copy->objWidthAuthored = true;
            if (gradient || end) copy->objGradientAuthored = true;
            if (end) copy->objGradientEndAuthored = true;
            if (stroke || fill) copy->objThemeColor = false;
            if (gradient || end) copy->objThemeGradient = false;
            if (copy->type() != Type::Group) continue;
            item.passed = (stroke ? TARGET_STROKE : 0u) | (fill ? TARGET_FILL : 0u) | (width ? TARGET_WIDTH : 0u);
        }
        for (auto i = 0u; i < count; i++) {
            auto group = items.data[i].copy;
            auto passed = items.data[i].passed;
            if (!passed) continue;
            auto heirCount = impl.heirs(group, passed, nullptr, 0u);
            auto heirs = heirCount ? new (std::nothrow) Heir[heirCount] : nullptr;
            if (heirCount && !heirs) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
            impl.heirs(group, passed, heirs, heirCount);
            for (auto j = 0u; j < heirCount; j++) {
                auto& heir = heirs[j];
                if (heir.properties & TARGET_STROKE) heir.object->style.stroke = group->style.stroke;
                if (heir.properties & TARGET_FILL) heir.object->style.fill = group->style.fill;
                if (heir.properties & TARGET_WIDTH) heir.object->style.width = group->style.width;
            }
            delete[] heirs;
        }
    }

    auto first = impl.objectCnt;
    Result result;
    if (!parent) result = add(root);
    else if (parent->type() == Type::Space) result = static_cast<Space*>(parent)->add(root);
    else result = parent->add(root);
    if (result != Result::Success) return result;
    items.attached = true;
    // The entries hold the copies in the order the clone lists them; they take the parts of
    // the state the fields do not hold, and those the source does not show never show.
    for (auto i = 0u; i < count; i++) {
        auto& item = items.data[i];
        auto& copyEntry = impl.objects[first + i];
        _unheld(copyEntry.base, item.state);
        if (!item.shown) copyEntry.dead = impl.cursor;
    }
    for (auto i = 0u; i < count; i++) {
        auto& item = items.data[i];
        if (!item.source->objMask) continue;
        auto masked = item.copy->mask(_cloneOf(items.data, count, item.source->objMask), item.source->objMaskMode);
        if (masked != Result::Success) {
            impl.discard(root, first);
            return masked;
        }
    }
    output = root;
    return Result::Success;
}

static constexpr auto MEASURE_TEXT = "only a valid text can be measured";
static constexpr auto MEASURE_FONT = "the text's font cannot set it at its size";

// ---------- Placement, measuring and viewports ----------

// World units per pixel at the camera; a perspective camera measures at its target.
static float _pixelWorld(const Config& config)
{
    auto& camera = config.camera;
    auto height = static_cast<float>(config.height);
    if (camera.projection == Projection::Perspective) {
        return 2.0f * std::tan(camera.fov * 0.5f) * (camera.target - camera.eye).length() / height;
    }
    return camera.orthoHeight / height;
}

static constexpr auto PLACE_LABEL = "the label must be another object of the space's scene, not the space or one above it";
static constexpr auto PLACE_LOOSE = "a label that is not attached yet must have no parent, since it is placed to go below the space";
static constexpr auto PLACE_AXIS = "the axis must be 0, 1 or 2, and the value finite";
static constexpr auto PLACE_DIRECTION = "the direction must be finite, and the space must not flatten it";
static constexpr auto PLACE_GAP = "the gap must be zero or positive";
static constexpr auto PLACE_PARENT = "the label's parent transform cannot be inverted";
static constexpr auto PLACE_BOUNDS = "the label has no bounds yet; give it geometry or children first";

Result Space::place(Object* label, uint8_t axis, float value, const Vec3& direction, float gap) noexcept
{
    // The space's scene gives the size of a pixel.
    if (!objOwner) return Result::InsufficientCondition;
    if (!label || label == this || (label->objOwner && label->objOwner != objOwner)) {
        return fail(Result::InvalidArguments, PLACE_LABEL);
    }
    if (!label->objOwner && label->parent()) return fail(Result::InvalidArguments, PLACE_LOOSE);
    for (auto above = parent(); above; above = above->parent()) {
        if (above == label) return fail(Result::InvalidArguments, PLACE_LABEL);
    }
    if (axis > 2u || !std::isfinite(value)) return fail(Result::InvalidArguments, PLACE_AXIS);
    if (!std::isfinite(gap) || gap < 0.0f) return fail(Result::InvalidArguments, PLACE_GAP);
    // The way from the tick to the label, in this space and then in the world: below the x axis
    // and left of the others unless direction says otherwise.
    auto toward = direction;
    if (toward.x == 0.0f && toward.y == 0.0f && toward.z == 0.0f) {
        if (axis == 0u) toward.y = -1.0f;
        else toward.x = -1.0f;
    }
    if (!_finite(toward)) return fail(Result::InvalidArguments, PLACE_DIRECTION);
    Vec3 at;
    if (axis == 0u) at.x = value;
    else if (axis == 1u) at.y = value;
    else at.z = value;
    auto world = _parentWorld(this) * model;
    auto tick = world * at;
    auto way = world * (at + toward) - tick;
    auto length = way.length();
    if (!_finite(tick) || !std::isfinite(length) || length <= 1.0e-12f) {
        return fail(Result::InvalidArguments, PLACE_DIRECTION);
    }
    auto unit = way / length;
    // The tick and the way in the label's parent space, where its bounds and layout live: this
    // space for a label that is not attached yet.
    Mat4 inverse;
    if (!_inverse(label->objOwner ? _parentWorld(label) : world, inverse)) {
        return fail(Result::InvalidArguments, PLACE_PARENT);
    }
    auto from = inverse * tick;
    auto run = inverse * (tick + unit) - from;
    auto runLength = run.length();
    if (!std::isfinite(runLength) || runLength <= 1.0e-12f) return fail(Result::InvalidArguments, PLACE_PARENT);
    auto to = from + run * ((tickLength * 0.5f + gap) * _pixelWorld(objOwner->pImpl->cfg));
    // A text without a box bounds only its point, so it pins the side of its glyphs that faces
    // the tick there instead, the way unit runs on screen.
    auto text = label->type() == Type::Text ? static_cast<Text*>(label) : nullptr;
    auto pinned = text && text->box.x <= 0.0f && text->box.y <= 0.0f;
    auto align = text ? text->align : Vec2{};
    auto most = std::fmax(std::fabs(unit.x), std::fabs(unit.y));
    if (pinned && most > 1.0e-6f) text->align = {0.5f - 0.5f * unit.x / most, 0.5f + 0.5f * unit.y / most};
    Bounds own;
    if (!label->bounds(own)) {
        if (text) text->align = align;
        return fail(Result::InsufficientCondition, PLACE_BOUNDS);
    }
    // Then the side of its bounds that faces the tick lies the distance past it, centered on the
    // line from the tick.
    auto normal = run / runLength;
    auto half = own.size() * 0.5f;
    auto support = std::fabs(normal.x) * half.x + std::fabs(normal.y) * half.y + std::fabs(normal.z) * half.z;
    auto screenGap = text ? text->objScreenGap : -1.0f;
    if (text) text->objScreenGap = -1.0f;
    auto result = label->layout(Mat4::translate(to + normal * support - own.center()) * label->model);
    if (result != Result::Success) {
        if (text) {
            text->align = align;
            text->objScreenGap = screenGap;
        }
        return result;
    }
    // A label below the space, or one to be added below it, shows as the space shows.
    auto below = !label->objOwner;
    for (auto above = label->parent(); above && !below; above = above->parent()) below = above == this;
    if (below) label->objLabelOf = this;
    return result;
}

Result Space::placeScreen(Text* label, uint8_t axis, float value, float gap) noexcept
{
    if (!objOwner) return Result::InsufficientCondition;
    if (!label || (label->objOwner && (label->objOwner != objOwner || label->parent() != this))
        || (label->parent() && label->parent() != this)
        || (!label->objOwner && label->parent())) {
        return fail(Result::InvalidArguments, "a screen label must be a direct text child of the space, or unattached with no parent");
    }
    if (label->orientation != TextOrientation::Billboard) {
        return fail(Result::InvalidArguments, "a screen label must use billboard orientation");
    }
    if (objWarp) {
        return fail(Result::InsufficientCondition, "screen labels must be placed before the space's first warp");
    }
    if (axis > 2u || !std::isfinite(value)) return fail(Result::InvalidArguments, PLACE_AXIS);
    if (!std::isfinite(gap) || gap < 0.0f) return fail(Result::InvalidArguments, PLACE_GAP);
    Vec3 at;
    if (axis == 0u) at.x = value;
    else if (axis == 1u) at.y = value;
    else at.z = value;
    auto previous = label->objScreenGap;
    label->objScreenGap = gap;
    auto result = label->layout(Mat4::translate(at - label->model * label->point) * label->model);
    if (result != Result::Success) label->objScreenGap = previous;
    else label->objLabelOf = this;
    return result;
}

Result Scene::measure(const Text* text, TextExtent& output) noexcept
{
    return measure(text, text ? text->text() : nullptr, output);
}

Result Scene::reserve(Text* text, const char* sample) noexcept
{
    if (!text || text->objOwner) return Result::InvalidArguments;
    TextExtent initial;
    TextExtent reserved;
    auto result = measure(text, initial);
    if (result != Result::Success) return result;
    result = measure(text, sample, reserved);
    if (result != Result::Success) return result;
    text->box = {std::fmax(initial.width, reserved.width), std::fmax(initial.height, reserved.height)};
    return Result::Success;
}

Result Scene::measure(const Text* text, const char* sample, TextExtent& output) noexcept
{
    output = {};
    pImpl->diagnostic = nullptr;
    if (!sample || !valid(text)) return pImpl->fail(Result::InvalidArguments, MEASURE_TEXT);
    // The font and size attach() gives a text that sets none of its own.
    auto& typography = _textTheme(pImpl->sceneTheme, text->role);
    auto font = text->font() ? text->font() : _textFont(typography, text);
    auto size = text->size > 0.0f ? text->size : typography.size;
    auto result = renderer::measureText(font, size, sample, text->slant, text->tabular, output);
    if (result == Result::InsufficientCondition) return pImpl->fail(result, "no font is loaded, not even the default one");
    if (result != Result::Success) return pImpl->fail(result, MEASURE_FONT);
    if (text->orientation == TextOrientation::Billboard) {
        auto scale = _pixelWorld(pImpl->cfg);
        output.width *= scale;
        output.height *= scale;
        output.ascent *= scale;
        output.descent *= scale;
        output.left *= scale;
        output.right *= scale;
        output.top *= scale;
        output.bottom *= scale;
    }
    return Result::Success;
}

Result Scene::update(Object* object) noexcept
{
    pImpl->diagnostic = nullptr;
    auto entry = object ? pImpl->entry(object) : nullptr;
    if (!entry || object->objOwner != this) return pImpl->fail(Result::InvalidArguments, TARGET_FOREIGN);
    if (entry->dead >= 0.0f) return pImpl->fail(Result::InvalidArguments, TARGET_REMOVED);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (pImpl->definitionsSealed) return pImpl->fail(Result::InsufficientCondition, LAYOUT_SEALED);
    // The base state applies from time zero; once clips captured it, rewriting it
    // would desynchronize earlier samples.
    for (auto i = 0u; i < pImpl->clipCnt; i++) {
        auto& clip = pImpl->clips[i];
        if (clip.object == object || clip.related == object) {
            return pImpl->fail(Result::InsufficientCondition, LAYOUT_ANIMATED);
        }
    }
    if (!valid(object)) return pImpl->fail(Result::InvalidArguments, TARGET_INVALID);
    if (!_equal(entry->base.model, object->model) && !_transformable(object)) {
        return pImpl->fail(Result::InvalidArguments, TARGET_FIXED);
    }
    if (object->type() == Type::Connector) {
        auto connector = static_cast<Connector*>(object);
        auto from = pImpl->entry(connector->from());
        auto to = pImpl->entry(connector->to());
        if (!from || !to || from->dead >= 0.0f || to->dead >= 0.0f) {
            return pImpl->fail(Result::InvalidArguments, LAYOUT_ENDPOINTS);
        }
    }
    // A group's new stroke, fill or width reaches the shapes that take it from the group.
    auto passed = 0u;
    if (object->type() == Type::Group) {
        if (!_equal(entry->base.stroke, object->style.stroke)) passed |= TARGET_STROKE;
        if (!_equal(entry->base.fill, object->style.fill)) passed |= TARGET_FILL;
        if (entry->base.width != object->style.width) passed |= TARGET_WIDTH;
    }
    if (passed) {
        auto heirCount = pImpl->heirs(object, passed, nullptr, 0u);
        auto heirs = heirCount ? new (std::nothrow) Heir[heirCount] : nullptr;
        if (heirCount && !heirs) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
        pImpl->heirs(object, passed, heirs, heirCount);
        for (auto i = 0u; i < heirCount; i++) {
            auto& heir = heirs[i];
            if (!heir.entry || heir.entry->dead >= 0.0f) continue;
            auto reason = heir.entry->fingerprint != fingerprint(heir.object) ? TARGET_HEIR_CHANGED : nullptr;
            for (auto j = 0u; !reason && j < pImpl->clipCnt; j++) {
                auto& clip = pImpl->clips[j];
                if (clip.object == heir.object || clip.related == heir.object) reason = LAYOUT_HEIR_ANIMATED;
            }
            if (reason) {
                delete[] heirs;
                return pImpl->fail(reason == TARGET_HEIR_CHANGED ? Result::InvalidArguments : Result::InsufficientCondition, reason);
            }
        }
        for (auto i = 0u; i < heirCount; i++) {
            auto& heir = heirs[i];
            if (!heir.entry || heir.entry->dead >= 0.0f) continue;
            if (heir.properties & TARGET_STROKE) heir.object->style.stroke = object->style.stroke;
            if (heir.properties & TARGET_FILL) heir.object->style.fill = object->style.fill;
            if (heir.properties & TARGET_WIDTH) heir.object->style.width = object->style.width;
            _rebase(*heir.entry, _state(heir.object));
            heir.entry->fingerprint = fingerprint(heir.object);
        }
        delete[] heirs;
        if (passed & TARGET_STROKE) object->objStrokeAuthored = true;
        if (passed & TARGET_FILL) object->objFillAuthored = true;
        if (passed & TARGET_WIDTH) object->objWidthAuthored = true;
    }
    _rebase(*entry, _state(object));
    entry->fingerprint = fingerprint(object);
    return Result::Success;
}

static bool _valid(const Viewport& viewport)
{
    if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y)
        || !std::isfinite(viewport.width) || !std::isfinite(viewport.height)) {
        return false;
    }
    if (viewport.x < 0.0f || viewport.y < 0.0f || viewport.width <= 0.0f || viewport.height <= 0.0f) {
        return false;
    }
    return viewport.x + viewport.width <= 1.0f && viewport.y + viewport.height <= 1.0f;
}

uint32_t Scene::depth(const Scene* scene) noexcept
{
    auto currentDepth = 1u;
    if (!scene) return currentDepth;
    for (auto i = 0u; i < scene->viewportCount(); i++) {
        auto child = depth(scene->sceneAt(i));
        if (child >= MAX_VIEWPORT_DEPTH) return child;
        if (child + 1u > currentDepth) currentDepth = child + 1u;
    }
    if (scene->pImpl->sequence) {
        auto sequence = scene->pImpl->sequence;
        for (auto i = 0u; i < sequence->count; i++) {
            auto child = depth(sequence->stages[i].scene);
            if (child >= MAX_VIEWPORT_DEPTH) return child;
            if (child + 1u > currentDepth) currentDepth = child + 1u;
        }
    }
    return currentDepth;
}

Result Scene::viewport(Scene* scene, const Viewport& viewport) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!scene || scene == this || scene->pImpl->owner) {
        return pImpl->fail(Result::InvalidArguments, VIEWPORT_SCENE);
    }
    if (!_valid(viewport)) return pImpl->fail(Result::InvalidArguments, VIEWPORT_RECT);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto ancestors = 1u;
    for (auto owner = pImpl->owner; owner; owner = owner->pImpl->owner) {
        if (owner == scene) return pImpl->fail(Result::InvalidArguments, VIEWPORT_SCENE);
        if (++ancestors >= MAX_VIEWPORT_DEPTH) return pImpl->fail(Result::InsufficientCondition, VIEWPORT_LIMIT);
    }
    auto childDepth = depth(scene);
    if (childDepth > MAX_VIEWPORT_DEPTH - ancestors) return pImpl->fail(Result::InsufficientCondition, VIEWPORT_LIMIT);
    if (pImpl->viewportCnt >= MAX_VIEWPORTS) return pImpl->fail(Result::InsufficientCondition, VIEWPORT_LIMIT);
    if (!grow(pImpl->viewports, pImpl->viewportCnt, pImpl->viewportCap, 1u, 4u, MAX_VIEWPORTS)) {
        return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    auto& entry = pImpl->viewports[pImpl->viewportCnt++];
    entry.scene = scene;
    entry.viewport = viewport;
    scene->pImpl->owner = this;
    scene->pImpl->definitionsSealed = true;
    return Result::Success;
}

Result Scene::remove(Object* object) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!object) return pImpl->fail(Result::InvalidArguments, TARGET_FOREIGN);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto entry = pImpl->entry(object);
    if (!entry || entry->dead >= 0.0f) return pImpl->fail(Result::InsufficientCondition, REMOVE_GONE);
    if (auto reason = _unplayable(entry, object)) return pImpl->fail(Result::InvalidArguments, reason);
    if (pImpl->cursor < entry->born) return pImpl->fail(Result::InsufficientCondition, TIMELINE_UNBORN);
    if (pImpl->cursor < _channelEnd(*entry, CHANNEL_ALL)) {
        return pImpl->fail(Result::InsufficientCondition,
                           "the object still animates after the removal time");
    }
    entry->dead = pImpl->cursor;
    pImpl->definitionsSealed = true;
    return Result::Success;
}

// ---------- Object transitions ----------

static AnimCurve _legacyCurve(Easing easing) noexcept
{
    if (static_cast<uint8_t>(easing) > static_cast<uint8_t>(Easing::EaseInOut)) {
        auto curve = AnimCurve::preset(AnimCurvePreset::Linear);
        curve.kind = static_cast<AnimCurvePreset>(UINT8_MAX);
        return curve;
    }
    return AnimCurve::preset(static_cast<AnimCurvePreset>(easing));
}

Result Scene::transition(Scene* const* scenes, uint32_t count, const Viewport& viewport,
                         float duration, float hold, Easing easing) noexcept
{
    return transition(scenes, count, viewport, duration, hold, _legacyCurve(easing));
}

Result Scene::transition(Scene* const* scenes, uint32_t count, const Viewport& viewport,
                         float duration, float hold, const AnimCurve& curve) noexcept
{
    return transition(scenes, count, viewport, duration, hold, curve, nullptr, 0u);
}

Result Scene::transition(Scene* const* scenes, uint32_t count, const Viewport& viewport,
                         float duration, float hold, const AnimCurve& curve,
                         const TransitionStyle* styles, uint32_t styleCount, bool live) noexcept
{
    return transition(scenes, count, viewport, duration, &hold, 1u, curve, styles, styleCount, live);
}

Result Scene::transition(Scene* const* scenes, uint32_t count, const Viewport& viewport,
                         float duration, const float* holds, uint32_t holdCount, const AnimCurve& curve,
                         const TransitionStyle* styles, uint32_t styleCount, bool live) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!scenes || count < 2u || count > MAX_VIEWPORTS) return pImpl->fail(Result::InvalidArguments, TRANSITION_SCENES);
    if (!_valid(viewport)) return pImpl->fail(Result::InvalidArguments, VIEWPORT_RECT);
    if (!holds || (holdCount != 1u && holdCount != count)) {
        return pImpl->fail(Result::InvalidArguments, "hold must give one duration or one for each stage");
    }
    for (auto i = 0u; i < holdCount; i++) {
        if (!std::isfinite(holds[i]) || holds[i] < 0.0f) {
            return pImpl->fail(Result::InvalidArguments, "hold must be finite and non-negative");
        }
    }
    if (!std::isfinite(duration) || duration <= 0.0f || !curve.valid()) {
        return pImpl->fail(Result::InvalidArguments, TRANSITION_TIMING);
    }
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (pImpl->sequence) return pImpl->fail(Result::InsufficientCondition, TRANSITION_TWICE);
    for (auto i = 0u; i < count; i++) {
        auto scene = scenes[i];
        if (!scene || scene == this || scene->pImpl->owner
            || !valid(scene->pImpl->cfg) || depth(scene) >= MAX_VIEWPORT_DEPTH) {
            return Result::InvalidArguments;
        }
        for (auto j = i + 1u; j < count; j++) {
            if (scene == scenes[j]) return Result::InvalidArguments;
        }
    }
    if (styleCount && (!styles || (styleCount != 1u && styleCount != count - 1u))) {
        return pImpl->fail(Result::InvalidArguments, TRANSITION_STYLES);
    }
    for (auto i = 0u; i < styleCount; i++) {
        if (static_cast<uint8_t>(styles[i].kind) > static_cast<uint8_t>(TransitionKind::ZoomOut)) {
            return pImpl->fail(Result::InvalidArguments, TRANSITION_STYLE);
        }
        if (styles[i].kind == TransitionKind::Slide
            && static_cast<uint8_t>(styles[i].from) > static_cast<uint8_t>(TransitionSide::Top)) {
            return pImpl->fail(Result::InvalidArguments, TRANSITION_SIDE);
        }
        if (styles[i].match
            && (styles[i].kind == TransitionKind::Match || styles[i].kind == TransitionKind::Fade)) {
            return pImpl->fail(Result::InvalidArguments, TRANSITION_MATCH);
        }
        if (!std::isfinite(styles[i].areaMargin) || styles[i].areaMargin < 0.0f) {
            return pImpl->fail(Result::InvalidArguments, TRANSITION_MARGIN);
        }
        if (styles[i].areaMargin != 0.05f
            && (!styles[i].object || (styles[i].kind != TransitionKind::ZoomIn
                                     && styles[i].kind != TransitionKind::ZoomOut))) {
            return pImpl->fail(Result::InvalidArguments, TRANSITION_MARGIN_OBJECT);
        }
    }

    // The area of the zoom of transition index inside the viewport, already of the shape of the
    // viewport and inside it, or why there is none: an object stands for the part of the frame
    // it covers at the end of its scene.
    auto zoomArea = [&](const TransitionStyle& style, uint32_t index, Viewport& output) -> const char* {
        auto area = style.area;
        if (style.object) {
            auto owner = scenes[index]->pImpl->entry(style.object) ? scenes[index] : nullptr;
            if (!owner && scenes[index + 1u]->pImpl->entry(style.object)) owner = scenes[index + 1u];
            if (!owner) return TRANSITION_OBJECT;
            auto time = owner->duration();
            Bounds bounds;
            auto defined = false;
            Camera camera;
            CameraView view;
            if (owner->pImpl->bounds(style.object, time, bounds, defined) != Result::Success
                || !defined || owner->pImpl->sample(time, camera, view) != Result::Success) {
                return TRANSITION_OBJECT;
            }
            auto& config = owner->pImpl->cfg;
            auto aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
            auto left = std::numeric_limits<float>::max();
            auto top = left;
            auto right = -left;
            auto bottom = -left;
            for (auto corner = 0u; corner < 8u; corner++) {
                Vec3 point = {corner & 1u ? bounds.max.x : bounds.min.x,
                              corner & 2u ? bounds.max.y : bounds.min.y,
                              corner & 4u ? bounds.max.z : bounds.min.z};
                Vec3 ndc;
                if (!camera.project(point, aspect, ndc)) return TRANSITION_OBJECT;
                left = std::fmin(left, (ndc.x + 1.0f) * 0.5f);
                right = std::fmax(right, (ndc.x + 1.0f) * 0.5f);
                top = std::fmin(top, (1.0f - ndc.y) * 0.5f);
                bottom = std::fmax(bottom, (1.0f - ndc.y) * 0.5f);
            }
            if (right - left <= 0.0f && bottom - top <= 0.0f) return TRANSITION_OBJECT;
            // The margin, as far as the viewport allows, keeps the edges of the object under
            // the scene drawn in its area; zero preserves its exact geometry bounds.
            auto extent = std::fmax(right - left, bottom - top);
            auto margin = std::fmax(std::fmin(extent * style.areaMargin, (1.0f - extent) * 0.5f), 0.0f);
            area = {left - margin, top - margin, right - left + 2.0f * margin,
                    bottom - top + 2.0f * margin};
        }
        auto size = std::fmax(area.width, area.height);
        if (!std::isfinite(area.x) || !std::isfinite(area.y) || !std::isfinite(size)
            || !(area.width >= 0.0f) || !(area.height >= 0.0f) || size <= 0.0f || size > 1.0f
            || area.x >= 1.0f || area.y >= 1.0f || area.x + area.width <= 0.0f
            || area.y + area.height <= 0.0f) {
            return TRANSITION_AREA;
        }
        auto inside = [&](float start, float extent) {
            return std::fmin(std::fmax(start + (extent - size) * 0.5f, 0.0f), 1.0f - size);
        };
        output = {inside(area.x, area.width), inside(area.y, area.height), size, size};
        return nullptr;
    };

    auto sequence = new (std::nothrow) SceneSequence;
    if (!sequence) return Result::OutOfMemory;
    sequence->stages = new (std::nothrow) SceneStage[count];
    sequence->transitions = new (std::nothrow) SceneTransition[count - 1u];
    sequence->count = count;
    if (!sequence->stages || !sequence->transitions) {
        delete sequence;
        return Result::OutOfMemory;
    }

    auto cursor = pImpl->cursor;
    sequence->begin = cursor;
    sequence->viewport = viewport;
    sequence->live = live;
    for (auto i = 0u; i < count; i++) {
        auto& stage = sequence->stages[i];
        stage.scene = scenes[i];
        stage.start = i ? sequence->transitions[i - 1u].begin : cursor;
        stage.end = cursor;
        auto hold = holds[holdCount == 1u ? 0u : i];
        if (hold > 0.0f && !_timeline(cursor, hold, 0.0f, 1u, stage.end)) {
            delete sequence;
            return Result::InvalidArguments;
        }
        cursor = stage.end;
        if (i + 1u >= count) continue;
        auto& transition = sequence->transitions[i];
        transition.begin = cursor;
        if (!_timeline(cursor, duration, 0.0f, 1u, transition.end)) {
            delete sequence;
            return Result::InvalidArguments;
        }
        transition.curve = curve;
        auto style = styleCount ? styles[styleCount == 1u ? 0u : i] : TransitionStyle{};
        transition.kind = style.kind;
        transition.from = style.from;
        if (transition.kind == TransitionKind::ZoomIn || transition.kind == TransitionKind::ZoomOut) {
            auto failure = zoomArea(style, i, transition.area);
            if (failure) {
                delete sequence;
                return pImpl->fail(Result::InvalidArguments, failure);
            }
        }
        transition.match = style.match;
        // Only a style that matches pairs the objects of the two scenes.
        auto matching = transition.kind == TransitionKind::Match || transition.match;
        auto objects = matching ? scenes[i]->pImpl->objectCnt : 0u;
        if (objects) {
            transition.matches = new (std::nothrow) uint32_t[objects];
            if (!transition.matches) {
                delete sequence;
                return Result::OutOfMemory;
            }
            for (auto j = 0u; j < objects; j++) {
                transition.matches[j] = UINT32_MAX;
                auto tag = scenes[i]->pImpl->objects[j].object->tag();
                auto target = tag && tag[0] ? scenes[i + 1u]->object(tag) : nullptr;
                auto entry = target ? scenes[i + 1u]->pImpl->entry(target) : nullptr;
                if (entry) {
                    transition.matches[j] = static_cast<uint32_t>(
                        entry - scenes[i + 1u]->pImpl->objects);
                }
            }
        }
        cursor = transition.end;
    }

    for (auto i = 0u; i + 1u < count; i++) {
        auto& transition = sequence->transitions[i];
        auto from = scenes[i]->pImpl;
        auto to = scenes[i + 1u]->pImpl;
        for (auto j = 0u; j < from->objectCnt; j++) {
            auto target = transition.matches ? transition.matches[j] : UINT32_MAX;
            if (target >= to->objectCnt) continue;
            auto& sourceEntry = from->objects[j];
            auto& targetEntry = to->objects[target];
            if (!targetEntry.object->objThemeColor
                && !targetEntry.object->objThemeGradient) continue;
            _inherit(targetEntry, to->clips, sourceEntry.object->style,
                     targetEntry.object->objThemeColor,
                     targetEntry.object->objThemeGradient);
        }
    }

    sequence->owned = true;
    for (auto i = 0u; i < count; i++) {
        scenes[i]->pImpl->owner = this;
        scenes[i]->pImpl->definitionsSealed = true;
    }
    pImpl->sequence = sequence;
    pImpl->advance(cursor);
    pImpl->definitionsSealed = true;
    return Result::Success;
}

Result Scene::play(const Animation& animation, float duration, Easing easing) noexcept
{
    return play(animation, duration, _legacyCurve(easing));
}

Result Scene::play(const Animation& animation, float duration, const AnimCurve& curve) noexcept
{
    return play(&animation, 1, duration, curve, 0.0f);
}

// ---------- Layers, effects and fade transforms ----------

static bool _effectModel(const Object* object, const Mat4& model, float scale,
                         const Vec3& offset, Mat4& output)
{
    Bounds bounds;
    if (!object || !object->bounds(bounds)) return false;
    auto center = bounds.center();
    auto scaling = Mat4::translate(center) * Mat4::scale({scale, scale, scale})
                 * Mat4::translate(center * -1.0f);
    output = Mat4::translate(offset) * scaling * model;
    return _affine(output);
}

static bool _growthModel(const Object* object, const Mat4& model, GrowthEdge edge,
                         Mat4& output)
{
    Bounds bounds;
    if (!object || !object->bounds(bounds)) return false;
    auto anchor = bounds.center();
    auto scaling = Vec3{1.0f, 1.0f, 1.0f};
    switch (edge) {
        case GrowthEdge::Left:
            anchor.x = bounds.min.x;
            scaling = {0.0f, 1.0f, 1.0f};
            break;
        case GrowthEdge::Right:
            anchor.x = bounds.max.x;
            scaling = {0.0f, 1.0f, 1.0f};
            break;
        case GrowthEdge::Bottom:
            anchor.y = bounds.min.y;
            scaling = {1.0f, 0.0f, 1.0f};
            break;
        case GrowthEdge::Top:
            anchor.y = bounds.max.y;
            scaling = {1.0f, 0.0f, 1.0f};
            break;
        default: return false;
    }
    auto effect = Mat4::translate(anchor) * Mat4::scale(scaling)
                * Mat4::translate(anchor * -1.0f);
    output = effect * model;
    return _affine(output);
}

// Whether model differs from authored only by where it places the object, so that a clip toward
// it may begin while another clip moves the object and add that shift, which change receives.
static bool _shifts(const Mat4& authored, const Mat4& model, Mat4& change)
{
    auto reach = 1.0f;
    for (auto row = 0u; row < 3u; row++) {
        for (auto column = 0u; column < 3u; column++) reach = std::fmax(reach, std::fabs(authored.e[row * 4u + column]));
    }
    for (auto row = 0u; row < 3u; row++) {
        for (auto column = 0u; column < 3u; column++) {
            if (std::fabs(model.e[row * 4u + column] - authored.e[row * 4u + column]) > 1.0e-5f * reach) return false;
        }
    }
    change = Mat4::translate({model.e[3] - authored.e[3], model.e[7] - authored.e[7], model.e[11] - authored.e[11]});
    return true;
}

// Whether a clip of the animation may begin while another clip moves the target, its change of
// the transform then adding to theirs: a transform, a turn, a scale, a travel along a path, a
// morph, or an effect that scales or shifts.
static bool _layerable(const Animation& animation)
{
    switch (animation.kind) {
        case AnimationKind::Transform:
        case AnimationKind::Morph:
        case AnimationKind::Rotate:
        case AnimationKind::Scale:
        case AnimationKind::MoveAlong:
        case AnimationKind::FadeIn:
        case AnimationKind::FadeOut:
        case AnimationKind::Grow:
        case AnimationKind::GrowFromEdge:
        case AnimationKind::Shrink:
        case AnimationKind::Indicate:
        case AnimationKind::Wiggle: return true;
        default: return false;
    }
}

// Whether a point lies inside bounds or on their faces.
static bool _within(const Bounds& bounds, const Vec3& point)
{
    auto size = bounds.size();
    auto slack = 1.0e-4f * std::fmax(1.0f, std::fmax(size.x, std::fmax(size.y, size.z)));
    return point.x >= bounds.min.x - slack && point.x <= bounds.max.x + slack && point.y >= bounds.min.y - slack
           && point.y <= bounds.max.y + slack && point.z >= bounds.min.z - slack && point.z <= bounds.max.z + slack;
}

// The point an effect scales its target about: the bounds center, or the edge it grows from.
static bool _effectPivot(const Object* object, AnimationKind kind, GrowthEdge edge, Vec3& pivot)
{
    Bounds bounds;
    if (!object || !object->bounds(bounds)) return false;
    pivot = bounds.center();
    if (kind != AnimationKind::GrowFromEdge) return true;
    switch (edge) {
        case GrowthEdge::Left: pivot.x = bounds.min.x; return true;
        case GrowthEdge::Right: pivot.x = bounds.max.x; return true;
        case GrowthEdge::Bottom: pivot.y = bounds.min.y; return true;
        case GrowthEdge::Top: pivot.y = bounds.max.y; return true;
        default: return false;
    }
}

// Makes a clip that carries the object from its authored model to model while another clip moves
// it a layer over that motion: its models become the identity and the change from the authored
// model to model, which has an inverse unless the change only shifts. The change turns, scales
// and skews about the object's center, which travels with the object, so over a still object it
// moves every point as the clip would alone, and once every motion ends the object has model.
static void _layered(Clip& clip, const Object* object, const Mat4& authored, const Mat4& model)
{
    clip.layered = true;
    clip.from.model = Mat4::identity();
    if (_shifts(authored, model, clip.to.model)) return;
    Mat4 inverse;
    _inverse(authored, inverse);
    clip.to.model = model * inverse;
    Bounds bounds;
    clip.follows = object->bounds(bounds);
    if (!clip.follows) return;
    clip.pivot = bounds.center();
    clip.anchor = inverse * clip.pivot;
}

// Makes a clip that begins while another clip moves its target a layer over that motion: its
// models become its change from the authored model, which has an inverse unless the clip only
// shifts. A pivot on or inside the object travels with the object, while a pivot outside it or
// at another object stays where it is.
static void _layered(Clip& clip, const Animation& animation, const Mat4& authored)
{
    if (animation.kind == AnimationKind::Transform) {
        _layered(clip, animation.target, authored, animation.model);
        return;
    }
    // A morph carries the source to the model of the shape it becomes.
    if (animation.kind == AnimationKind::Morph) {
        _layered(clip, animation.target, authored, animation.related->model);
        return;
    }
    clip.layered = true;
    Mat4 inverse;
    _inverse(authored, inverse);
    clip.from.model = clip.from.model * inverse;
    clip.to.model = clip.to.model * inverse;
    Bounds bounds;
    switch (animation.kind) {
        case AnimationKind::Rotate:
        case AnimationKind::Scale:
            clip.follows = !animation.related
                           && (animation.centered || (animation.target->bounds(bounds) && _within(bounds, clip.pivot)));
            break;
        case AnimationKind::MoveAlong: clip.follows = true; break;
        default: clip.follows = _effectPivot(animation.target, animation.kind, animation.growthEdge, clip.pivot); break;
    }
    if (clip.follows) clip.anchor = inverse * clip.pivot;
}

// Shapes whose outline a morph can turn into another; arrows and routes would lose their tips.
static bool _morphable(const Object* object)
{
    switch (object->type()) {
        case Type::Line:
        case Type::Circle:
        case Type::Rectangle:
        case Type::Polygon:
        case Type::Plot:
        case Type::Path:
        case Type::Curve: return true;
        default: return false;
    }
}

// Why two objects cannot morph into each other, or nullptr when they can.
static const char* _morphGeometry(const Object* source, const Object* target)
{
    if (!source || !target) return MORPH_KIND;
    auto surfaces = (source->type() == Type::SurfaceMesh) + (target->type() == Type::SurfaceMesh);
    if (surfaces == 1) return MORPH_KIND;
    if (surfaces == 2) {
        auto from = static_cast<const SurfaceMesh*>(source);
        auto to = static_cast<const SurfaceMesh*>(target);
        return from->columns() == to->columns() && from->rows() == to->rows() ? nullptr : MORPH_GRID;
    }
    if (!_morphable(source) || !_morphable(target)) return MORPH_KIND;
    if (_morphPieceCount(source) != 1u || _morphPieceCount(target) != 1u) return nullptr;
    auto sourceClosed = false;
    auto targetClosed = false;
    outline(source, nullptr, 0u, sourceClosed);
    outline(target, nullptr, 0u, targetClosed);
    return sourceClosed == targetClosed ? nullptr : MORPH_CLOSED;
}

static bool _hasClip(const ObjectEntry& entry)
{
    return entry.firstClip != NO_CLIP;
}

Result Scene::fadeTransform(Object* source, Object* target, float duration,
                            Easing easing) noexcept
{
    return fadeTransform(source, target, duration, _legacyCurve(easing));
}

Result Scene::fadeTransform(Object* source, Object* target, float duration,
                            const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!source || !target || source == target || _inside(source, target) || _inside(target, source)) {
        return pImpl->fail(Result::InvalidArguments, FADE_PAIR);
    }
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto sourceEntry = pImpl->entry(source);
    auto targetEntry = pImpl->entry(target);
    if (auto reason = _unplayable(sourceEntry, source)) return pImpl->fail(Result::InvalidArguments, reason);
    if (auto reason = _unplayable(targetEntry, target)) return pImpl->fail(Result::InvalidArguments, reason);
    if (targetEntry->born != pImpl->cursor || _hasClip(*targetEntry)) {
        return pImpl->fail(Result::InsufficientCondition, FADE_FRESH);
    }
    if (!pImpl->growHandoffs()) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);

    auto begin = pImpl->cursor;
    Animation animations[] = {Animation::fadeOut(source), Animation::fadeIn(target)};
    auto result = play(animations, 2u, duration, curve, 0.0f);
    if (result != Result::Success) return result;
    pImpl->handoffs[pImpl->handoffCnt++] = {source, target, begin, pImpl->cursor};
    return remove(source);
}

// ---------- Match transforms and handoffs ----------

struct MatchPieces
{
    MatchPiece* data = nullptr;
    ~MatchPieces() { delete[] data; }
};

static uint32_t _piece(const MatchPiece* pieces, uint32_t begin, uint32_t end, const Object* object)
{
    for (auto i = begin; i < end; i++) {
        if (pieces[i].object == object) return i;
    }
    return UINT32_MAX;
}

// Two circles or two rectangles, so that one can become the other through its size, corner,
// and colors. A warped one draws the outline its warp gave instead, which no size reaches.
static bool _sameShape(const Object* first, const Object* second)
{
    return first->type() == second->type() && (first->type() == Type::Circle || first->type() == Type::Rectangle)
           && !warped(first) && !warped(second);
}

// Two pieces whose sizes count in the same unit, so one can grow into the other: shapes of one
// kind, or texts of one orientation.
static bool _sizable(const Object* first, const Object* second)
{
    if (_sameShape(first, second)) return true;
    return first->type() == Type::Text && second->type() == Type::Text
           && static_cast<const Text*>(first)->orientation == static_cast<const Text*>(second)->orientation;
}

// Where a piece stands in the world: its point when the pair are texts pinned alike, which
// keeps their glyphs in step through a change of size, or else the center of its bounds.
static Vector64 _anchor(const Object* object, bool point)
{
    auto parent = _parentWorld(object);
    if (point) return _point(parent * object->model, static_cast<const Text*>(object)->point);
    Bounds bounds;
    if (object->bounds(bounds)) return _point(parent, bounds.center());
    return _point(parent * object->model, {});
}

static bool _pinnedAlike(const Object* first, const Object* second)
{
    if (first->type() != Type::Text || second->type() != Type::Text) return false;
    auto a = static_cast<const Text*>(first)->align;
    auto b = static_cast<const Text*>(second)->align;
    return a.x == b.x && a.y == b.y;
}

// The model of object moved by the world vector by, as a translation in its parent space.
static bool _moved(const Object* object, const Vector64& by, Mat4& output)
{
    Mat4 inverse;
    if (!_inverse(_parentWorld(object), inverse)) return false;
    auto local = _apply(_linear(inverse), by);
    output = Mat4::translate({static_cast<float>(local.x), static_cast<float>(local.y),
                              static_cast<float>(local.z)}) * object->model;
    return _affine(output);
}

static Object* _counterpart(Object* source, Object* copy, const Object* object)
{
    if (object == source) return copy;
    for (auto i = 0u; i < source->childCount() && i < copy->childCount(); i++) {
        if (auto found = _counterpart(source->childAt(i), copy->childAt(i), object)) return found;
    }
    return nullptr;
}

// Matches a clone of source made beside it at the cursor into target, as matchTransform() does
// with options.copy, and removes the clone again when the match fails. Impl is the scene's
// implementation, which only scene members may name.
template<typename Impl>
static Result _matchCopy(Scene& scene, Impl& impl, Object* source, Object* target, float duration,
                         const AnimCurve& curve, const MatchOptions& options) noexcept
{
    if (options.pairCount > MAX_ANIMATION_GROUP || (options.pairCount && !options.pairs)) {
        return impl.fail(Result::InvalidArguments, MATCH_PAIRS);
    }
    auto sourceCount = _pieces(source, nullptr);
    auto targetCount = _pieces(target, nullptr);
    if (sourceCount > MAX_ANIMATION_GROUP || targetCount > MAX_ANIMATION_GROUP) {
        return impl.fail(Result::InvalidArguments, MATCH_PIECES);
    }
    auto copyCount = 0u;
    if (!_treeCount(source, copyCount, 0u)) return impl.fail(Result::InsufficientCondition, ADD_DEPTH);
    // Room for the match contacts, their original counterparts, and the clone contacts.
    if (!impl.growHandoffs(2u * (sourceCount + targetCount + 1u) + copyCount)) {
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    auto first = impl.objectCnt;
    auto time = impl.cursor;
    Object* copy = nullptr;
    auto result = scene.clone(source, copy);
    if (result != Result::Success) return result;
    auto local = options;
    local.copy = false;
    Object** pairs = nullptr;
    if (options.pairCount) {
        pairs = new (std::nothrow) Object*[2u * options.pairCount];
        if (!pairs) {
            impl.discard(copy, first);
            return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
        }
        for (auto i = 0u; i < options.pairCount; i++) {
            auto piece = _counterpart(source, copy, options.pairs[2u * i]);
            pairs[2u * i] = piece ? piece : options.pairs[2u * i];
            pairs[2u * i + 1u] = options.pairs[2u * i + 1u];
        }
        local.pairs = pairs;
    }
    auto handoffs = impl.handoffCnt;
    result = scene.matchTransform(copy, target, duration, curve, local);
    delete[] pairs;
    if (result != Result::Success) {
        impl.discard(copy, first);
        return result;
    }
    auto end = impl.handoffCnt;
    for (auto i = handoffs; i < end; i++) {
        auto contact = impl.handoffs[i];
        if (!contact.counterpart) continue;
        auto original = _counterpart(copy, source, contact.source);
        if (original) {
            impl.handoffs[impl.handoffCnt++] = {original, contact.target, time, impl.cursor, contact.family};
        } else if (auto original = _counterpart(copy, source, contact.target)) {
            impl.handoffs[impl.handoffCnt++] = {original, contact.source, time, impl.cursor, contact.family};
        }
    }
    for (auto i = first; i < impl.objectCnt; i++) {
        auto piece = impl.objects[i].object;
        auto original = _counterpart(copy, source, piece);
        if (original) impl.handoffs[impl.handoffCnt++] = {original, piece, time, impl.cursor, false};
    }
    return Result::Success;
}

Result Scene::matchTransform(Object* source, Object* target, float duration,
                             const AnimCurve& curve, const MatchOptions& options) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (!source || !target || source == target || _inside(source, target) || _inside(target, source)) {
        return impl.fail(Result::InvalidArguments, MATCH_PAIR);
    }
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (!std::isfinite(duration) || duration <= 0.0f) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (!std::isfinite(options.lag) || options.lag < 0.0f) return impl.fail(Result::InvalidArguments, PLAY_LAG);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);
    if (!_arc(options.arc)) return impl.fail(Result::InvalidArguments, MOTION_ARC);
    if (options.path) {
        if (options.arc != 0.0f) return impl.fail(Result::InvalidArguments, MATCH_PATH_ARC);
        if (options.path->type() == Type::Connector) {
            return impl.fail(Result::InvalidArguments, MATCH_PATH_LIVE);
        }
        if (auto reason = _unplayable(impl.entry(options.path), options.path, true)) {
            return impl.fail(Result::InvalidArguments, reason);
        }
    }
    if (!(options.fade > 0.0f && options.fade <= 1.0f)) return impl.fail(Result::InvalidArguments, MATCH_FADE);
    if (options.copy) return _matchCopy(*this, impl, source, target, duration, curve, options);
    auto sourceEntry = impl.entry(source);
    auto targetEntry = impl.entry(target);
    if (auto reason = _unplayable(sourceEntry, source)) return impl.fail(Result::InvalidArguments, reason);
    if (auto reason = _unplayable(targetEntry, target)) return impl.fail(Result::InvalidArguments, reason);
    auto time = impl.cursor;
    if (time < sourceEntry->born || time < _channelEnd(*sourceEntry, CHANNEL_ALL)) {
        return impl.fail(Result::InsufficientCondition, MATCH_SOURCE);
    }
    if (targetEntry->born != time || _hasClip(*targetEntry)) {
        return impl.fail(Result::InsufficientCondition, MATCH_FRESH);
    }

    auto sourceCount = _pieces(source, nullptr);
    auto targetCount = _pieces(target, nullptr);
    if (sourceCount > MAX_ANIMATION_GROUP || targetCount > MAX_ANIMATION_GROUP) {
        return impl.fail(Result::InvalidArguments, MATCH_PIECES);
    }
    if (!sourceCount && !targetCount) return impl.fail(Result::InvalidArguments, PLAY_EMPTY);
    if (options.pairCount > (sourceCount < targetCount ? sourceCount : targetCount)
        || (options.pairCount && !options.pairs)) {
        return impl.fail(Result::InvalidArguments, MATCH_PAIRS);
    }
    float nextCursor;
    if (!_timeline(time, duration, options.lag, sourceCount > targetCount ? sourceCount : targetCount,
                   nextCursor)) {
        return impl.fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    auto total = sourceCount + targetCount;
    MatchPieces holder;
    holder.data = new (std::nothrow) MatchPiece[total];
    if (!holder.data) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    auto pieces = holder.data;
    _pieces(source, pieces);
    _pieces(target, pieces + sourceCount);
    for (auto i = 0u; i < total; i++) {
        auto& piece = pieces[i];
        piece.entry = impl.entry(piece.object);
        if (auto reason = _unplayable(piece.entry, piece.object)) return impl.fail(Result::InvalidArguments, reason);
        if (i < sourceCount) {
            if (time < piece.entry->born || time < _channelEnd(*piece.entry, CHANNEL_ALL)) {
                return impl.fail(Result::InsufficientCondition, MATCH_SOURCE);
            }
        } else if (piece.entry->born != time || _hasClip(*piece.entry)) {
            return impl.fail(Result::InsufficientCondition, MATCH_FRESH);
        }
    }

    // Given pairs come first; each text of target then takes the first free text of source
    // that reads the same, or the text a diff finds, unless the caller pairs everything itself.
    for (auto i = 0u; i < options.pairCount; i++) {
        auto from = _piece(pieces, 0u, sourceCount, options.pairs[2u * i]);
        auto to = _piece(pieces, sourceCount, total, options.pairs[2u * i + 1u]);
        if (from == UINT32_MAX || to == UINT32_MAX || pieces[from].partner != UINT32_MAX
            || pieces[to].partner != UINT32_MAX) {
            return impl.fail(Result::InvalidArguments, MATCH_PAIRS);
        }
        pieces[from].partner = to;
        pieces[to].partner = from;
        pieces[from].same = pieces[to].same = _sameText(pieces[from].object, pieces[to].object)
                                              || _sameShape(pieces[from].object, pieces[to].object);
    }
    if (options.diff) {
        auto result = diffMatch(source, target, pieces, sourceCount, total);
        if (result != Result::Success) {
            return impl.fail(result, result == Result::OutOfMemory ? SCENE_MEMORY : MATCH_LINES);
        }
    }
    for (auto to = sourceCount; options.strings && !options.diff && to < total; to++) {
        if (pieces[to].partner != UINT32_MAX || pieces[to].object->type() != Type::Text) continue;
        for (auto from = 0u; from < sourceCount; from++) {
            if (pieces[from].partner != UINT32_MAX || !_sameText(pieces[from].object, pieces[to].object)) continue;
            pieces[from].partner = to;
            pieces[to].partner = from;
            pieces[from].same = pieces[to].same = true;
            break;
        }
    }
    if (options.shapes && !pairShapes(pieces, sourceCount, total)) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);

    // A fading piece yields its authored slot to an incoming piece, which may itself arrive
    // from another slot. Exempt that replacement while it fades, without exempting siblings.
    for (auto from = 0u; from < sourceCount; from++) {
        if (pieces[from].partner != UINT32_MAX) continue;
        for (auto to = sourceCount; to < total; to++) {
            if (pieces[to].contact != UINT32_MAX) continue;
            auto point = _pinnedAlike(pieces[from].object, pieces[to].object);
            auto a = _anchor(pieces[from].object, point);
            auto b = _anchor(pieces[to].object, point);
            if (_distance(a, b) > 1.0e-6) continue;
            pieces[from].contact = to;
            pieces[to].contact = from;
            break;
        }
    }

    // A text that reads the same, or a shape of the kind of its pair, leaves at once, since the
    // piece it pairs with starts in its place and look; every other piece plays one clip, an
    // extra piece unseen in the place and look of its pair.
    auto clipCount = 0u;
    for (auto i = 0u; i < total; i++) {
        auto& piece = pieces[i];
        piece.to = _state(piece.object);
        piece.from = piece.to;
        if (piece.partner == UINT32_MAX) {
            (i < sourceCount ? piece.to : piece.from).opacity = 0.0f;
            piece.channels = CHANNEL_OPACITY;
            clipCount++;
            continue;
        }
        if (i < sourceCount && piece.same) continue;
        auto other = pieces[piece.partner].object;
        auto point = _pinnedAlike(piece.object, other);
        auto there = _anchor(other, point);
        auto here = _anchor(piece.object, point);
        auto& start = i < sourceCount ? piece.to : piece.from;
        if (!_moved(piece.object, {there.x - here.x, there.y - here.y, there.z - here.z}, start.model)) {
            return impl.fail(Result::InvalidArguments, MATCH_SPACE);
        }
        piece.channels = CHANNEL_MODEL | CHANNEL_OPACITY;
        if (options.path && !_inverse(_parentWorld(piece.object), piece.pathSpace)) {
            return impl.fail(Result::InvalidArguments, MATCH_SPACE);
        }
        if (_sizable(piece.object, other)) {
            start.size = _state(other).size;
            if (piece.object->type() == Type::Text
                && static_cast<const Text*>(piece.object)->orientation == TextOrientation::Billboard) {
                VisualState hereState; VisualState thereState;
                bool visible;
                auto result = impl.world(piece.object, time, hereState, visible);
                if (result != Result::Success) return impl.fail(result, TARGET_INVALID);
                result = impl.world(other, time, thereState, visible);
                if (result != Result::Success) return impl.fail(result, TARGET_INVALID);
                auto from = billboardScale(impl.billboardReference(other), thereState.model, thereState.pixelScale);
                auto to = billboardScale(impl.billboardReference(piece.object), hereState.model, hereState.pixelScale);
                if (from != to) {
                    auto size = thereState.size.x * static_cast<double>(from) / to;
                    if (!std::isfinite(size) || size < 0.0 || size > std::numeric_limits<float>::max()) {
                        return impl.fail(Result::InvalidArguments, MATCH_SPACE);
                    }
                    start.size = {static_cast<float>(size), static_cast<float>(size)};
                }
            }
            piece.channels |= CHANNEL_SIZE;
        }
        if (piece.same || piece.extra) {
            start.fill = other->style.fill;
            start.opacity = piece.extra ? 0.0f : other->opacity;
            piece.channels |= CHANNEL_FILL;
            if (_sameShape(piece.object, other)) {
                start.stroke = other->style.stroke;
                start.width = other->style.width;
                piece.channels |= CHANNEL_STROKE | CHANNEL_WIDTH;
            }
            if (other->type() == Type::Rectangle && piece.object->type() == Type::Rectangle) {
                start.corner = static_cast<const Rectangle*>(other)->corner;
                piece.channels |= CHANNEL_CORNER;
            }
        } else {
            start.opacity = 0.0f;
        }
        clipCount++;
    }
    if (!impl.growClips(clipCount) || !impl.growHandoffs(total + 1u)) {
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }

    uint32_t path = 0u;
    if (options.path) {
        auto previousCount = impl.pathCnt;
        auto previousVertices = impl.pathVertices;
        auto result = impl.follow(options.path, nullptr, path, true);
        if (result != Result::Success) {
            impl.dropPaths(previousCount, previousVertices);
            return result;
        }
        path++;
    }
    for (auto i = 0u; i < total; i++) {
        auto& piece = pieces[i];
        auto outgoing = i < sourceCount;
        if (outgoing && piece.same) {
            piece.entry->dead = time;
            continue;
        }
        auto index = outgoing ? i : i - sourceCount;
        auto& clip = impl.clips[impl.clipCnt++];
        clip = Clip{};
        clip.object = piece.object;
        impl.link(impl.clipCnt - 1u);
        clip.from = piece.from;
        clip.to = piece.to;
        clip.begin = static_cast<float>(static_cast<double>(time)
                     + static_cast<double>(duration) * options.lag * index);
        clip.end = static_cast<float>(static_cast<double>(time)
                   + static_cast<double>(duration) * (1.0 + static_cast<double>(options.lag) * index));
        // An unpaired piece fades in its share of the time: out at the start, in at the end.
        if (piece.partner == UINT32_MAX && options.fade < 1.0f) {
            auto share = static_cast<float>(static_cast<double>(duration) * options.fade);
            if (outgoing) clip.end = clip.begin + share;
            else clip.begin = clip.end - share;
        }
        piece.contactBegin = clip.begin;
        piece.contactEnd = clip.end;
        clip.curve = curve;
        clip.channels = piece.channels;
        if ((piece.channels & CHANNEL_MODEL) && path) {
            clip.motion = ClipMotion::MatchPath;
            clip.path = path;
            auto& space = piece.pathSpace.e;
            clip.axis = {space[0], space[4], space[8]};
            clip.stretch = {space[1], space[5], space[9]};
            clip.offset = {space[2], space[6], space[10]};
        } else if ((piece.channels & CHANNEL_MODEL) && options.arc != 0.0f) {
            clip.motion = ClipMotion::Arc;
            clip.arc = options.arc;
        }
        _channelEnd(*piece.entry, clip.channels, clip.end);
        if (outgoing) {
            Impl::store(piece.object, piece.to);
            piece.entry->fingerprint = fingerprint(piece.object);
        } else {
            // The piece starts in its pair's place and look, or unseen.
            _rebase(*piece.entry, piece.from);
        }
    }
    if (sourceEntry->dead < 0.0f) sourceEntry->dead = nextCursor;
    for (auto i = 0u; i < total; i++) {
        auto& piece = pieces[i];
        auto partner = piece.partner != UINT32_MAX ? piece.partner : piece.contact;
        if (partner != UINT32_MAX && (i < sourceCount || piece.extra)) {
            auto begin = time;
            auto end = nextCursor;
            auto counterpart = piece.partner != UINT32_MAX;
            if (!counterpart) {
                begin = piece.contactBegin;
                end = piece.contactEnd;
            }
            impl.handoffs[impl.handoffCnt++] = {piece.object, pieces[partner].object, begin, end, true, counterpart};
        }
    }
    impl.handoffs[impl.handoffCnt++] = {source, target, time, nextCursor, false};
    impl.advance(nextCursor);
    impl.definitionsSealed = true;
    return Result::Success;
}

bool Scene::handoff(const Object* first, const Object* second, float time) const noexcept
{
    if (!first || !second) return false;
    for (auto i = 0u; i < pImpl->handoffCnt; i++) {
        auto& handoff = pImpl->handoffs[i];
        if (time < handoff.begin || time > handoff.end) continue;
        auto within = [&](const Object* object, const Object* contact) {
            return handoff.family ? _inside(object, contact) : object == contact;
        };
        if ((within(first, handoff.source) && within(second, handoff.target))
            || (within(first, handoff.target) && within(second, handoff.source))) {
            return true;
        }
    }
    return false;
}

bool Scene::shows(const Object* object) const noexcept
{
    if (!object || object->objOwner != this) return false;
    for (auto above = object; above; above = above->objParent) {
        auto entry = pImpl->entry(above);
        if (!entry || (entry->dead >= 0.0f && entry->dead <= entry->born)) return false;
    }
    return true;
}

// ---------- Highlights and emphasis ----------

// The objects below object, at any depth, that are not groups, or object itself if it is not.
static uint32_t _leaves(Object* object, Object** output, uint32_t count = 0u)
{
    if (object->type() != Type::Group) {
        if (output) output[count] = object;
        return count + 1u;
    }
    for (auto i = 0u; i < object->childCount(); i++) count = _leaves(object->childAt(i), output, count);
    return count;
}

Result Scene::highlight(Object* target, Object* const* selection, uint32_t count, float dim,
                        float duration, const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (!target) return impl.fail(Result::InvalidArguments, TARGET_FOREIGN);
    if (auto reason = _unplayable(impl.entry(target), target)) return impl.fail(Result::InvalidArguments, reason);
    if (count && !selection) return impl.fail(Result::InvalidArguments, HIGHLIGHT_SELECTION);
    for (auto i = 0u; i < count; i++) {
        if (!selection[i] || !_inside(selection[i], target)) {
            return impl.fail(Result::InvalidArguments, HIGHLIGHT_SELECTION);
        }
    }
    if (!std::isfinite(dim) || dim < 0.0f || dim > 1.0f) return impl.fail(Result::InvalidArguments, HIGHLIGHT_DIM);
    if (!std::isfinite(duration) || duration <= 0.0f) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);

    auto total = _leaves(target, nullptr);
    auto leaves = new (std::nothrow) Object*[total ? total : 1u];
    auto targets = new (std::nothrow) AnimationTarget[total ? total : 1u];
    if (!leaves || !targets) {
        delete[] leaves;
        delete[] targets;
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    _leaves(target, leaves);
    auto moving = 0u;
    for (auto i = 0u; i < total; i++) {
        auto leaf = leaves[i];
        // Objects removed, or not added yet, at the cursor keep out of it.
        auto entry = impl.entry(leaf);
        if (!entry || entry->dead >= 0.0f || entry->born > impl.cursor) continue;
        auto marked = !count;
        for (auto j = 0u; j < count && !marked; j++) marked = _inside(leaf, selection[j]);
        auto opacity = marked ? 1.0f : dim;
        if (leaf->opacity != opacity) targets[moving++] = AnimationTarget::from(leaf).opacity(opacity);
    }
    delete[] leaves;
    auto result = moving ? play(targets, moving, duration, curve) : wait(duration);
    delete[] targets;
    return result;
}

Emphasis Emphasis::circumscribe(Object* target, EmphasisShape shape) noexcept
{
    Emphasis emphasis;
    emphasis.kind = EmphasisKind::Circumscribe;
    emphasis.target = target;
    emphasis.shape = shape;
    return emphasis;
}

Emphasis Emphasis::passingFlash(Object* target) noexcept
{
    Emphasis emphasis;
    emphasis.kind = EmphasisKind::PassingFlash;
    emphasis.target = target;
    emphasis.width = 6.0f;
    return emphasis;
}

Emphasis Emphasis::flash(Object* target) noexcept
{
    Emphasis emphasis;
    emphasis.kind = EmphasisKind::Flash;
    emphasis.target = target;
    return emphasis;
}

Emphasis Emphasis::flash(const Vec3& point) noexcept
{
    Emphasis emphasis;
    emphasis.kind = EmphasisKind::Flash;
    emphasis.point = point;
    return emphasis;
}

Emphasis Emphasis::ripple(Object* target) noexcept
{
    Emphasis emphasis;
    emphasis.kind = EmphasisKind::Ripple;
    emphasis.target = target;
    emphasis.padding = 0.0f;
    emphasis.size = 0.4f;
    return emphasis;
}

// The local bounds of object at its sampled state as a frame drawn around it sees them: a circle
// or rectangle at its sampled size, a text without a box at the size of its glyphs as scene sets
// them, and what a warp bends as far as it has carried its points. False when the object has no
// bounds of its own.
static bool _emphasisLocal(Scene& scene, const Object* object, const VisualState& state, Bounds& local)
{
    Vec3 point;
    if (state.warp && !anchor(object, point)) return localBounds(object, state.warp, state.warpProgress, local);
    switch (object->type()) {
        case Type::Line:
        case Type::Arrow:
        case Type::Vector: {
            Vec3 ends[2];
            if (!segment(object, &state, ends)) return false;
            local = {ends[0], ends[0]};
            local.min = {std::fmin(ends[0].x, ends[1].x), std::fmin(ends[0].y, ends[1].y), std::fmin(ends[0].z, ends[1].z)};
            local.max = {std::fmax(ends[0].x, ends[1].x), std::fmax(ends[0].y, ends[1].y), std::fmax(ends[0].z, ends[1].z)};
            return true;
        }
        case Type::Arc:
        case Type::Angle: return arcBounds(arcGeometry(object, &state), local);
        case Type::Circle: {
            auto circle = static_cast<const Circle*>(object);
            auto half = Vec3{state.size.x, state.size.x, 0.0f};
            local = {circle->center - half, circle->center + half};
            return true;
        }
        case Type::Rectangle: {
            auto rectangle = static_cast<const Rectangle*>(object);
            auto half = Vec3{state.size.x * 0.5f, state.size.y * 0.5f, 0.0f};
            local = {rectangle->center - half, rectangle->center + half};
            return true;
        }
        case Type::Text: {
            auto text = static_cast<const Text*>(object);
            TextExtent extent;
            if (text->box.x > 0.0f || text->box.y > 0.0f || scene.measure(text, extent) != Result::Success) break;
            // A warp moves a text's glyphs with its anchor.
            auto at = state.warp && state.warp->count ? state.warp->at(0u, state.warpProgress) : text->point;
            auto left = at.x - text->align.x * extent.width;
            auto top = at.y + text->align.y * extent.height;
            local = {{left, top - extent.height, at.z}, {left + extent.width, top, at.z}};
            return true;
        }
        default: break;
    }
    return localBounds(object, state.warp, state.warpProgress, local);
}

// The world bounds of object and the objects below it at time, shown or hidden, as a frame drawn
// around them sees them. Objects not added yet or already removed then take no part, and neither
// do connectors. Impl is the scene's implementation, which only scene members may name.
template<typename Impl>
static Result _emphasisBounds(Scene& scene, const Impl& impl, const Object* object, float time, Bounds& bounds,
                              bool& defined)
{
    VisualState state;
    auto alive = false;
    auto result = impl.world(object, time, state, alive);
    if (result != Result::Success) return result;
    Bounds local;
    auto known = alive && _emphasisLocal(scene, object, state, local);
    for (auto corner = 0u; known && corner < 8u; corner++) {
        auto point = state.model * Vec3{corner & 1u ? local.max.x : local.min.x, corner & 2u ? local.max.y : local.min.y,
                                        corner & 4u ? local.max.z : local.min.z};
        if (!defined) bounds = {point, point};
        bounds.min = {std::fmin(bounds.min.x, point.x), std::fmin(bounds.min.y, point.y), std::fmin(bounds.min.z, point.z)};
        bounds.max = {std::fmax(bounds.max.x, point.x), std::fmax(bounds.max.y, point.y), std::fmax(bounds.max.z, point.z)};
        defined = true;
    }
    for (auto i = 0u; i < object->childCount(); i++) {
        result = _emphasisBounds(scene, impl, object->childAt(i), time, bounds, defined);
        if (result != Result::Success) return result;
    }
    return Result::Success;
}

// The highest layer of object and the objects below it.
static int32_t _highestLayer(const Object* object)
{
    auto layer = object->layer;
    for (auto i = 0u; i < object->childCount(); i++) {
        auto below = _highestLayer(object->childAt(i));
        if (below > layer) layer = below;
    }
    return layer;
}

// The outline of a frame around bounds grown by padding in the world xy plane, closed by its
// first point: a rectangle from its top right corner, or an ellipse through the corners from its
// right end, both counterclockwise. Writes at most 65 points and returns their count.
static uint32_t _frame(const Bounds& bounds, float padding, EmphasisShape shape, Vec3* points)
{
    auto center = bounds.center();
    auto half = bounds.size() * 0.5f;
    auto x = half.x + padding;
    auto y = half.y + padding;
    if (shape == EmphasisShape::Rectangle) {
        points[0] = {center.x + x, center.y + y, center.z};
        points[1] = {center.x - x, center.y + y, center.z};
        points[2] = {center.x - x, center.y - y, center.z};
        points[3] = {center.x + x, center.y - y, center.z};
        points[4] = points[0];
        return 5u;
    }
    constexpr auto segments = 64u;
    for (auto i = 0u; i <= segments; i++) {
        auto angle = TWO_PI * static_cast<float>(i % segments) / static_cast<float>(segments);
        points[i] = {center.x + 1.41421356f * x * std::cos(angle), center.y + 1.41421356f * y * std::sin(angle),
                     center.z};
    }
    return segments + 1u;
}

// The most lines a flash bursts into.
static constexpr auto EMPHASIS_LINES = 64u;

// The line of a flash that leaves bounds at angle in the world xy plane: it starts padding past
// where that direction from their center leaves them and runs size along it.
static void _burst(const Bounds& bounds, float padding, float size, float angle, Vec3* line)
{
    auto center = bounds.center();
    auto half = bounds.size() * 0.5f;
    auto direction = Vec3{std::cos(angle), std::sin(angle), 0.0f};
    auto reach = std::numeric_limits<float>::infinity();
    if (std::fabs(direction.x) > 1e-6f) reach = std::fmin(reach, half.x / std::fabs(direction.x));
    if (std::fabs(direction.y) > 1e-6f) reach = std::fmin(reach, half.y / std::fabs(direction.y));
    line[0] = center + direction * (reach + padding);
    line[1] = line[0] + direction * size;
}

// The outline a passing flash runs along at time in world space, closed by its first point when
// it closes: the outline of object as authored, placed by its model then, or a connector where it
// is drawn. Leaves count 0 when object has no outline of one stroke with some length then. Impl
// is the scene's implementation, which only scene members may name.
template<typename Impl>
static Result _passingOutline(const Impl& impl, const Object* object, float time, Vec3*& points, uint32_t& count)
{
    points = nullptr;
    count = 0u;
    VisualState state;
    auto alive = false;
    auto result = impl.world(object, time, state, alive);
    if (result != Result::Success || !alive) return result;
    auto connector = object->type() == Type::Connector ? static_cast<const Connector*>(object) : nullptr;
    auto closed = false;
    auto straight = !state.warp && (object->type() == Type::Line || object->type() == Type::Arrow || object->type() == Type::Vector);
    Vec3 ends[2];
    auto authored = connector ? 2u : straight ? (segment(object, &state, ends) ? 2u : 0u) : outline(object, nullptr, 0u, closed);
    if (authored < 2u) return Result::Success;
    points = new (std::nothrow) Vec3[authored + 1u];
    if (!points) return Result::OutOfMemory;
    auto length = 0.0f;
    if (connector) {
        auto defined = false;
        result = impl.ends(connector, time, points[0], points[1], defined);
        if (defined) count = 2u;
    } else {
        if (straight) {
            points[0] = ends[0];
            points[1] = ends[1];
        } else outline(object, points, authored, closed);
        for (auto i = 0u; i < authored; i++) points[i] = state.model * points[i];
        if (closed) points[authored] = points[0];
        count = authored + (closed ? 1u : 0u);
    }
    for (auto i = 1u; i < count; i++) length += (points[i] - points[i - 1u]).length();
    if (result != Result::Success || !std::isfinite(length) || length <= 1e-6f) {
        delete[] points;
        points = nullptr;
        count = 0u;
    }
    return result;
}

// Tags the shapes of an emphasis after its target as <target tag>.<effect>.<n>.<part>, or
// <effect>.<n>.<part> for an untagged target, with the smallest n from 1 that leaves every tag
// unused; a part of nullptr is the shape's number from 1. False when memory runs out.
static bool _emphasisTags(const Scene& scene, const Object* target, const char* effect, Object* const* shapes,
                          const char* const* parts, uint32_t count)
{
    auto base = target ? target->tag() : nullptr;
    // Room for the dots, n and a numbered part of up to ten digits each, and the end.
    auto longest = size_t{10};
    for (auto i = 0u; i < count; i++) {
        if (parts[i] && std::strlen(parts[i]) > longest) longest = std::strlen(parts[i]);
    }
    auto size = (base ? std::strlen(base) : 0u) + std::strlen(effect) + longest + 16u;
    auto tag = new (std::nothrow) char[size];
    if (!tag) return false;
    auto write = [&](uint32_t n, uint32_t i) {
        if (parts[i]) std::snprintf(tag, size, "%s%s%s.%u.%s", base ? base : "", base ? "." : "", effect, n, parts[i]);
        else std::snprintf(tag, size, "%s%s%s.%u.%u", base ? base : "", base ? "." : "", effect, n, i + 1u);
    };
    auto n = 1u;
    for (auto i = 0u; i < count;) {
        write(n, i);
        if (scene.object(tag)) {
            n++;
            i = 0u;
        } else {
            i++;
        }
    }
    auto tagged = true;
    for (auto i = 0u; i < count && tagged; i++) {
        write(n, i);
        tagged = shapes[i]->tag(tag).tag() != nullptr;
    }
    delete[] tag;
    return tagged;
}

// Adds the shapes of an emphasis at the cursor, plays a clip of each that draws a stretch of its
// outline windows[i] long over duration, and removes them at the end. When that fails, the
// shapes added so far are removed at once, so none ever shows, and the others are deleted. Impl
// is the scene's implementation, which only scene members may name.
template<typename Impl>
static Result _emphasize(Scene& scene, Impl& impl, Object* const* shapes, const float* windows, uint32_t count,
                         float duration, const AnimCurve& curve)
{
    auto animations = new (std::nothrow) Animation[count];
    auto result = animations ? Result::Success : impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    auto added = 0u;
    while (result == Result::Success && added < count) {
        result = scene.add(shapes[added]);
        if (result != Result::Success) break;
        impl.entry(shapes[added])->windowed = true;
        animations[added] = Animation::create(shapes[added]);
        added++;
    }
    if (result == Result::Success) result = scene.play(animations, count, duration, curve);
    delete[] animations;
    if (result != Result::Success) {
        auto reason = impl.diagnostic;
        for (auto i = 0u; i < added; i++) scene.remove(shapes[i]);
        for (auto i = added; i < count; i++) delete shapes[i];
        impl.diagnostic = reason;
        return result;
    }
    for (auto i = 0u; i < count; i++) {
        auto& clip = impl.clips[impl.clipCnt - count + i];
        clip.window = windows[i];
        clip.to.start = 1.0f;
    }
    for (auto i = 0u; i < count && result == Result::Success; i++) result = scene.remove(shapes[i]);
    return result;
}

// Adds the ring of a ripple at the cursor, grows it to size and corner along curve while it fades
// out evenly over duration, and removes it at the end. When the ring cannot be added or grown,
// it is deleted or removed at once, so it never shows; the fade, checked with the growth, then
// cannot fail. Impl is the scene's implementation, which only scene members may name.
template<typename Impl>
static Result _ripple(Scene& scene, Impl& impl, Rectangle* ring, const Vec2& size, float corner, float duration,
                      const AnimCurve& curve)
{
    auto begin = impl.cursor;
    // Room for both clips, so that neither runs out of memory.
    if (!impl.growClips(2u)) {
        delete ring;
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    auto result = scene.add(ring);
    if (result != Result::Success) {
        delete ring;
        return result;
    }
    result = scene.play(AnimationTarget::from(ring).size(size).corner(corner), duration, curve);
    if (result != Result::Success) {
        auto reason = impl.diagnostic;
        scene.remove(ring);
        impl.diagnostic = reason;
        return result;
    }
    // The fade runs beside the growth on a channel of its own.
    impl.advance(begin);
    result = scene.play(AnimationTarget::from(ring).opacity(0.0f), duration,
                        AnimCurve::preset(AnimCurvePreset::Linear));
    if (result != Result::Success) return result;
    return scene.remove(ring);
}

Result Scene::emphasize(const Emphasis& emphasis, float duration, const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto target = emphasis.target;
    // A flash may burst out of a point instead of a target.
    if (!target && emphasis.kind != EmphasisKind::Flash) return impl.fail(Result::InvalidArguments, TARGET_FOREIGN);
    if (auto reason = target ? _unplayable(impl.entry(target), target) : nullptr) {
        return impl.fail(Result::InvalidArguments, reason);
    }
    if (static_cast<uint8_t>(emphasis.kind) > static_cast<uint8_t>(EmphasisKind::Ripple)
        || static_cast<uint8_t>(emphasis.shape) > static_cast<uint8_t>(EmphasisShape::Ellipse)
        || !std::isfinite(emphasis.width) || emphasis.width <= 0.0f || !std::isfinite(emphasis.padding)
        || emphasis.padding < 0.0f || !std::isfinite(emphasis.window) || emphasis.window <= 0.0f
        || emphasis.window > 1.0f || !std::isfinite(emphasis.size) || emphasis.size <= 0.0f
        || emphasis.count < 1u || emphasis.count > EMPHASIS_LINES || !_finite(emphasis.point)) {
        return impl.fail(Result::InvalidArguments, EMPHASIS_VALUES);
    }
    if (!std::isfinite(duration) || duration <= 0.0f) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);
    float end;
    if (!_timeline(impl.cursor, duration, 0.0f, 1u, end)) return impl.fail(Result::InvalidArguments, TIMELINE_RANGE);

    // Emphasis parts hold tag suffix, stroke width and draw window. Frames and rays draw/erase;
    // passing flash traverses the outline; ripple grows a complete rounded ring.
    Object* shapes[EMPHASIS_LINES] = {};
    const char* parts[EMPHASIS_LINES] = {};
    float windows[EMPHASIS_LINES];
    float widths[EMPHASIS_LINES];
    auto count = 0u;
    const char* effect;
    Vec2 grown;
    auto rounded = 0.0f;
    if (emphasis.kind == EmphasisKind::PassingFlash) {
        Vec3* points;
        uint32_t vertices;
        auto result = _passingOutline(impl, target, impl.cursor, points, vertices);
        if (result == Result::OutOfMemory) return impl.fail(result, SCENE_MEMORY);
        if (result != Result::Success) return impl.fail(result, TARGET_CHANGED);
        if (!vertices) return impl.fail(Result::InvalidArguments, EMPHASIS_OUTLINE);
        shapes[0] = Plot::gen(points, vertices);
        delete[] points;
        parts[0] = "flash";
        windows[0] = emphasis.window;
        widths[0] = emphasis.width;
        count = 1u;
        effect = "passing_flash";
    } else {
        Bounds bounds = {emphasis.point, emphasis.point};
        auto defined = !target;
        auto result = target ? _emphasisBounds(*this, impl, target, impl.cursor, bounds, defined) : Result::Success;
        // Measuring a text while no font is loaded leaves its reason behind.
        impl.diagnostic = nullptr;
        if (result != Result::Success) return impl.fail(result, TARGET_CHANGED);
        if (!defined || !_finite(bounds.min) || !_finite(bounds.max)) {
            return impl.fail(Result::InvalidArguments, EMPHASIS_EMPTY);
        }
        if (emphasis.kind == EmphasisKind::Circumscribe) {
            Vec3 frame[65];
            auto corners = _frame(bounds, emphasis.padding, emphasis.shape, frame);
            const char* names[] = {"frame", "flash"};
            for (; count < 2u; count++) {
                shapes[count] = Plot::gen(frame, corners);
                parts[count] = names[count];
                windows[count] = count ? emphasis.window : 2.0f;
                widths[count] = count ? emphasis.width * 2.0f : emphasis.width;
            }
            effect = "circumscribe";
        } else if (emphasis.kind == EmphasisKind::Flash) {
            for (; count < emphasis.count; count++) {
                Vec3 line[2];
                auto angle = TWO_PI * static_cast<float>(count) / static_cast<float>(emphasis.count);
                _burst(bounds, emphasis.padding, emphasis.size, angle, line);
                shapes[count] = Plot::gen(line, 2u);
                windows[count] = 2.0f;
                widths[count] = emphasis.width;
            }
            effect = "flash";
        } else {
            // The corners of the target's own outline, in world units: half the shorter side
            // around a circle, and a rectangle's corner as its model scales it. A warped one
            // has the bounds of its outline as anything else does.
            auto extent = bounds.size();
            auto corner = 0.0f;
            auto own = !warped(target);
            if (own && target->type() == Type::Circle) corner = std::fmin(extent.x, extent.y) * 0.5f;
            if (own && target->type() == Type::Rectangle) {
                VisualState state;
                auto alive = false;
                result = impl.world(target, impl.cursor, state, alive);
                if (result != Result::Success) return impl.fail(result, TARGET_CHANGED);
                if (alive) {
                    auto center = static_cast<const Rectangle*>(target)->center;
                    auto origin = state.model * center;
                    auto across = (state.model * (center + Vec3{state.corner, 0.0f, 0.0f}) - origin).length();
                    auto up = (state.model * (center + Vec3{0.0f, state.corner, 0.0f}) - origin).length();
                    corner = std::fmin(across, up);
                }
            }
            // A flat target leaves the ring a sliver to grow from.
            auto size = Vec2{std::fmax(extent.x + 2.0f * emphasis.padding, 1e-4f),
                             std::fmax(extent.y + 2.0f * emphasis.padding, 1e-4f)};
            corner = std::fmin(corner + emphasis.padding, std::fmin(size.x, size.y) * 0.5f);
            grown = {size.x + 2.0f * emphasis.size, size.y + 2.0f * emphasis.size};
            rounded = std::fmin(corner + emphasis.size, std::fmin(grown.x, grown.y) * 0.5f);
            shapes[0] = Rectangle::gen(bounds.center(), size, corner);
            parts[0] = "ring";
            widths[0] = emphasis.width;
            count = 1u;
            effect = "ripple";
        }
    }
    auto layer = target ? _highestLayer(target) : 0;
    auto made = true;
    for (auto i = 0u; i < count; i++) {
        if (!shapes[i]) {
            made = false;
            continue;
        }
        shapes[i]->stroke(emphasis.color, widths[i]).gradient(false);
        shapes[i]->layer = layer;
    }
    if (!made || !_emphasisTags(*this, target, effect, shapes, parts, count)) {
        for (auto i = 0u; i < count; i++) delete shapes[i];
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    if (emphasis.kind == EmphasisKind::Ripple) {
        return _ripple(*this, impl, static_cast<Rectangle*>(shapes[0]), grown, rounded, duration, curve);
    }
    return _emphasize(*this, impl, shapes, windows, count, duration, curve);
}

// ---------- Arranging ----------

// How far bounds reach from their center along a unit axis.
static float _reach(const Bounds& bounds, const Vec3& axis)
{
    auto half = bounds.size() * 0.5f;
    return std::fabs(axis.x) * half.x + std::fabs(axis.y) * half.y + std::fabs(axis.z) * half.z;
}

// An item that grows into its place, and the model it had before.
struct ArrangedItem
{
    Object* object = nullptr;
    Mat4 model;
};

Result Scene::arrange(Object* const* items, uint32_t count, const Vec3& direction, float gap,
                      const Vec3& center, float duration, const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (!items || !count) return impl.fail(Result::InvalidArguments, PLAY_EMPTY);
    if (count > MAX_ANIMATION_GROUP) return impl.fail(Result::InvalidArguments, PLAY_GROUP);
    auto length = direction.length();
    if (!std::isfinite(length) || length <= 1.0e-7f) return impl.fail(Result::InvalidArguments, ARRANGE_DIRECTION);
    if (!std::isfinite(gap) || gap < 0.0f) return impl.fail(Result::InvalidArguments, ARRANGE_GAP);
    if (!_finite(center)) return impl.fail(Result::InvalidArguments, ARRANGE_CENTER);
    if (!std::isfinite(duration) || duration <= 0.0f) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);
    for (auto i = 0u; i < count; i++) {
        if (!items[i]) return impl.fail(Result::InvalidArguments, TARGET_FOREIGN);
        if (auto reason = _unplayable(impl.entry(items[i]), items[i])) return impl.fail(Result::InvalidArguments, reason);
        if (items[i]->parent() != items[0]->parent()) return impl.fail(Result::InvalidArguments, ARRANGE_ITEMS);
        for (auto j = 0u; j < i; j++) {
            if (items[j] == items[i]) return impl.fail(Result::InvalidArguments, ARRANGE_ITEMS);
        }
    }
    auto boxes = new (std::nothrow) Bounds[count];
    auto animations = new (std::nothrow) Animation[count];
    auto grown = new (std::nothrow) ArrangedItem[count];
    auto release = [&]() {
        delete[] boxes;
        delete[] animations;
        delete[] grown;
    };
    if (!boxes || !animations || !grown) {
        release();
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    for (auto i = 0u; i < count; i++) {
        if (!items[i]->bounds(boxes[i])) {
            release();
            return impl.fail(Result::InvalidArguments, ARRANGE_BOUNDS);
        }
    }
    // The items follow one another along the axis, each as long as its bounds reach.
    auto axis = direction / length;
    auto total = 0.0f;
    for (auto i = 0u; i < count; i++) total += _reach(boxes[i], axis) * 2.0f + (i ? gap : 0.0f);
    auto cursor = -total * 0.5f;
    auto moving = 0u;
    auto growing = 0u;
    for (auto i = 0u; i < count; i++) {
        auto item = items[i];
        auto reach = _reach(boxes[i], axis);
        auto offset = center + axis * (cursor + reach) - boxes[i].center();
        cursor += reach * 2.0f + gap;
        auto entry = impl.entry(item);
        // An item attached at the cursor has not been seen yet: it starts in its place.
        if (entry->born == impl.cursor && !_hasClip(*entry)) {
            grown[growing++] = {item, item->model};
            item->model = Mat4::translate(offset) * item->model;
            _rebase(*entry, _state(item));
            entry->fingerprint = fingerprint(item);
            animations[moving++] = Animation::growFromCenter(item);
        } else if (offset.length() > 1.0e-6f) {
            animations[moving++] = Animation::transform(item, Mat4::translate(offset) * item->model);
        }
    }
    auto result = moving ? play(animations, moving, duration, curve) : wait(duration);
    if (result != Result::Success) {
        for (auto i = 0u; i < growing; i++) {
            auto entry = impl.entry(grown[i].object);
            grown[i].object->model = grown[i].model;
            _rebase(*entry, _state(grown[i].object));
            entry->fingerprint = fingerprint(grown[i].object);
        }
    }
    release();
    return result;
}

// ---------- Preparing plays ----------

static bool _connectorReferences(const ObjectEntry* objects, uint32_t count,
                                 const Object* object)
{
    for (auto i = 0u; i < count; i++) {
        auto candidate = objects[i].object;
        if (candidate->type() != Type::Connector) continue;
        auto connector = static_cast<const Connector*>(candidate);
        if (connector->from() == object || connector->to() == object) return true;
    }
    return false;
}

// Pivot objects and motion paths are only read when the group is authored, so several
// animations may share one and it may animate in the same group.
static const Object* _written(const Animation& animation)
{
    switch (animation.kind) {
        case AnimationKind::Rotate:
        case AnimationKind::Scale:
        case AnimationKind::MoveAlong: return nullptr;
        default: return animation.related;
    }
}

static bool _sameAnimationObject(const Animation& lhs, const Animation& rhs)
{
    auto lhsRelated = _written(lhs);
    auto rhsRelated = _written(rhs);
    if (lhs.target == rhs.target || lhs.target == rhsRelated || lhsRelated == rhs.target) return true;
    return lhsRelated && lhsRelated == rhsRelated;
}

static uint32_t _channels(const Animation& animation)
{
    switch (animation.kind) {
        case AnimationKind::Transform: return CHANNEL_MODEL;
        case AnimationKind::Create:
        case AnimationKind::Uncreate:
        case AnimationKind::Write: return CHANNEL_PROGRESS;
        case AnimationKind::DrawBorderThenFill: return CHANNEL_PROGRESS | CHANNEL_FILL_PROGRESS;
        case AnimationKind::FillReveal: return CHANNEL_FILL_PROGRESS;
        case AnimationKind::Fade: return CHANNEL_OPACITY;
        case AnimationKind::FadeIn:
        case AnimationKind::FadeOut:
            if (animation.scale != 1.0f || animation.offset.length() > 0.0f) {
                return CHANNEL_OPACITY | CHANNEL_MODEL | CHANNEL_PIXEL_SCALE;
            }
            return CHANNEL_OPACITY;
        case AnimationKind::Grow:
        case AnimationKind::GrowFromEdge:
        case AnimationKind::Shrink: return CHANNEL_MODEL | CHANNEL_OPACITY | CHANNEL_PIXEL_SCALE;
        case AnimationKind::Indicate:
            return CHANNEL_MODEL | CHANNEL_PIXEL_SCALE | CHANNEL_STROKE | CHANNEL_FILL;
        // The shape draws in the source's layer until the target takes its place.
        case AnimationKind::Morph: return CHANNEL_ALL & ~CHANNEL_LAYER;
        case AnimationKind::Stroke: return CHANNEL_STROKE;
        case AnimationKind::Fill: return CHANNEL_FILL;
        case AnimationKind::Rotate:
        case AnimationKind::Scale:
        case AnimationKind::MoveAlong: return CHANNEL_MODEL;
        case AnimationKind::Wiggle: return CHANNEL_MODEL | CHANNEL_PIXEL_SCALE;
    }
    return CHANNEL_ALL;
}

// Fills the motion of a clip slot from the state before any clip of its group applies, so
// pivots and paths read objects that other animations of the same group change.
Result Scene::Impl::prepare(const Animation& animation, Clip& clip, float begin, float end) noexcept
{
    auto& impl = *this;
    auto target = animation.target;
    clip = Clip{};
    clip.from.model = target->model;
    clip.begin = begin;
    clip.end = end;
    switch (animation.kind) {
        case AnimationKind::Transform:
            if (animation.arc != 0.0f) {
                clip.motion = ClipMotion::Arc;
                clip.axis = animation.axis;
                clip.arc = animation.arc;
            }
            return Result::Success;
        case AnimationKind::Rotate:
        case AnimationKind::Scale: {
            Bounds bounds;
            clip.motion = ClipMotion::Pivot;
            clip.pivot = animation.pivot;
            if (animation.related) {
                Mat4 parent;
                if (!animation.related->bounds(bounds)) return impl.fail(Result::InvalidArguments, MOTION_BOUNDS);
                if (!_inverse(_parentWorld(target), parent)) return impl.fail(Result::InvalidArguments, MOTION_SPACE);
                clip.pivot = parent * (_parentWorld(animation.related) * bounds.center());
            } else if (animation.centered) {
                if (!target->bounds(bounds)) return impl.fail(Result::InvalidArguments, MOTION_BOUNDS);
                clip.pivot = bounds.center();
            }
            clip.axis = animation.axis;
            if (animation.kind == AnimationKind::Rotate) clip.angle = animation.angle;
            else clip.stretch = animation.stretch;
            clip.offset = animation.offset;
            clip.arc = animation.arc;
            break;
        }
        case AnimationKind::MoveAlong: {
            Bounds bounds;
            if (!target->bounds(bounds)) return impl.fail(Result::InvalidArguments, MOTION_BOUNDS);
            clip.motion = ClipMotion::Path;
            clip.pivot = bounds.center();
            clip.orient = animation.orient;
            clip.pathFrom = animation.from;
            clip.pathTo = animation.to;
            uint32_t index;
            auto result = impl.follow(animation.related, target, index);
            if (result != Result::Success) return result;
            clip.path = index + 1u;
            break;
        }
        case AnimationKind::Morph: {
            uint32_t index;
            auto result = impl.pair(animation, index);
            if (result != Result::Success) return result;
            clip.morph = index + 1u;
            return Result::Success;
        }
        case AnimationKind::Wiggle: {
            Bounds bounds;
            if (!target->bounds(bounds)) return impl.fail(Result::InvalidArguments, EFFECT_BOUNDS);
            clip.pivot = bounds.center();
            clip.stretch = {animation.scale, animation.scale, animation.scale};
            clip.angle = animation.angle;
            clip.swings = animation.value;
            return Result::Success;
        }
        default: return Result::Success;
    }
    auto time = animation.related && animation.related->type() == Type::Connector ? end : impl.cursor;
    if (!_finite(clip.pivot) || !impl.motion(clip, 1.0f, time, clip.to.model)) {
        return impl.fail(Result::InvalidArguments, MOTION_RANGE);
    }
    return Result::Success;
}

// Hides the object of entry until its reveal: in its base and in its clips stored before count.
static void _hideBefore(ObjectEntry& entry, Clip* clips, uint32_t count, bool progress)
{
    if (entry.revealScheduled) return;
    if (progress) entry.base.progress = 0.0f;
    else entry.base.opacity = 0.0f;
    for (auto i = entry.firstClip; i != NO_CLIP && i < count; i = clips[i].next) {
        auto& previous = clips[i];
        if (progress) {
            previous.from.progress = 0.0f;
            previous.to.progress = 0.0f;
            previous.progressReveal = true;
        } else {
            previous.from.opacity = 0.0f;
            previous.to.opacity = 0.0f;
        }
    }
    entry.revealScheduled = true;
}

static void _hideFillBefore(ObjectEntry& entry, Clip* clips, uint32_t count)
{
    if (entry.fillRevealScheduled) return;
    entry.base.fillProgress = 0.0f;
    for (auto i = entry.firstClip; i != NO_CLIP && i < count; i = clips[i].next) {
        auto& previous = clips[i];
        previous.from.fillProgress = 0.0f;
        previous.to.fillProgress = 0.0f;
    }
    entry.fillRevealScheduled = true;
}

// What an animation gives an object, copied by scene members from the private fields of an
// AnimationTarget or Animation.
struct Targeted
{
    uint32_t properties = 0;
    float width = 0.0f;
    float radius = 0.0f;
    Vec2 size;
    float corner = 0.0f;
    Gradient gradient;
    Color gradientEnd;
    Color stroke;
    Color fill;
};

// Why the width, sizes, gradient or colors an animation names cannot reach object, or nullptr.
static const char* _unanimatable(const Object* object, const Targeted& values)
{
    auto type = object->type();
    auto properties = values.properties;
    if (properties & TARGET_WIDTH) {
        if (!std::isfinite(values.width) || values.width < 0.0f) return VALUE_WIDTH;
        if (type == Type::Text || type == Type::Picture || type == Type::Cell) return VALUE_WIDTH_OBJECT;
    }
    if ((properties & TARGET_RADIUS)
        && ((type != Type::Circle && type != Type::Point && type != Type::Arc && type != Type::Angle) || !std::isfinite(values.radius) || values.radius <= 0.0f)) {
        return VALUE_RADIUS;
    }
    if (properties & TARGET_SIZE) {
        auto& size = values.size;
        if ((type != Type::Rectangle && type != Type::Text) || !std::isfinite(size.x) || !std::isfinite(size.y)
            || size.x <= 0.0f || size.y <= 0.0f || (type == Type::Text && size.x != size.y)) {
            return VALUE_SIZE;
        }
    }
    if ((properties & TARGET_CORNER)
        && (type != Type::Rectangle || !std::isfinite(values.corner) || values.corner < 0.0f)) {
        return VALUE_CORNER;
    }
    if (type == Type::Rectangle && (properties & (TARGET_SIZE | TARGET_CORNER))) {
        auto rectangle = static_cast<const Rectangle*>(object);
        auto size = (properties & TARGET_SIZE) ? values.size : rectangle->size;
        auto corner = (properties & TARGET_CORNER) ? values.corner : rectangle->corner;
        if (corner > std::fmin(size.x, size.y) * 0.5f) return VALUE_CORNER;
    }
    auto& current = object->style.gradient;
    if (properties & TARGET_GRADIENT) {
        auto& gradient = values.gradient;
        if (!current.enabled() || !gradient.valid() || gradient.type != current.type
            || gradient.stopCount != current.stopCount || gradient.line != current.line
            || gradient.centered != current.centered || gradient.sized != current.sized
            || gradient.focused != current.focused) {
            return VALUE_GRADIENT;
        }
        if (current.stopCount && !_equal(values.gradientEnd, object->style.gradientEnd)) return VALUE_GRADIENT_END;
    }
    // With stops, the gradient paints both stroke and fill and keeps only their alpha.
    if (current.enabled() && current.stopCount) {
        auto& style = object->style;
        auto recolored = [](Color a, Color b) { return a.r != b.r || a.g != b.g || a.b != b.b; };
        if (((properties & TARGET_STROKE) && recolored(values.stroke, style.stroke))
            || ((properties & TARGET_FILL) && recolored(values.fill, style.fill))) {
            return VALUE_GRADIENT_COLOR;
        }
    }
    return nullptr;
}

// Whether ends break count points into pieces as Plot::gen() takes them, ends being optional for
// one piece, or no points into none.
static bool _pieceEnds(const uint32_t* ends, uint32_t pieces, uint32_t count)
{
    if (!count) return !pieces;
    if (!pieces || (pieces > 1u && !ends)) return false;
    if (!ends) return count >= 2u;
    auto start = 0u;
    for (auto i = 0u; i < pieces; i++) {
        if (ends[i] < start || ends[i] - start < 2u || ends[i] > count) return false;
        start = ends[i];
    }
    return start == count;
}

const char* Scene::Impl::unanimatable(const AnimationTarget& target) noexcept
{
    auto object = target.target;
    auto properties = target.properties;
    if (!properties || (properties & ~TARGET_ALL)) return VALUE_NONE;
    Targeted values;
    values.properties = properties;
    values.width = target.targetWidth;
    values.radius = target.targetRadius;
    values.size = target.targetSize;
    values.corner = target.targetCorner;
    values.gradient = target.targetGradient;
    values.gradientEnd = target.targetGradientEnd;
    values.stroke = target.targetStroke;
    values.fill = target.targetFill;
    if (auto reason = _unanimatable(object, values)) return reason;
    if ((properties & TARGET_ARC_START) && (object->type() != Type::Arc || !std::isfinite(target.targetStart)))
        return "start needs an arc and finite radians";
    if ((properties & TARGET_ARC_ANGLE) && (object->type() != Type::Arc || !std::isfinite(target.targetAngle) || std::fabs(target.targetAngle) > 6.28318530718f))
        return "angle needs an arc and a sweep between -2 pi and 2 pi";
    if (properties & TARGET_MODEL) {
        if (!_transformable(object)) return TARGET_FIXED;
        if (!_affine(target.targetModel)) return TARGET_MATRIX;
        if (!_arc(target.targetArc)) return MOTION_ARC;
    }
    if ((properties & TARGET_OPACITY)
        && (!std::isfinite(target.targetOpacity) || target.targetOpacity < 0.0f || target.targetOpacity > 1.0f)) {
        return VALUE_OPACITY;
    }
    if ((properties & TARGET_PROGRESS)
        && (!std::isfinite(target.targetProgress) || target.targetProgress < 0.0f || target.targetProgress > 1.0f)) {
        return VALUE_PROGRESS;
    }
    if ((properties & TARGET_BLUR)
        && (!std::isfinite(target.targetBlur) || target.targetBlur < 0.0f || target.targetBlur > EFFECT_SIGMA_LIMIT)) {
        return VALUE_BLUR;
    }
    if ((properties & TARGET_SHADOW) && !valid(target.targetShadow)) return VALUE_SHADOW;
    if ((properties & TARGET_VALUE)
        && (object->type() != Type::Text || !static_cast<const Text*>(object)->counting()
            || !std::isfinite(target.targetValue))) {
        return VALUE_COUNT;
    }
    if (properties & TARGET_TEXT) {
        if (object->type() != Type::Text || static_cast<const Text*>(object)->counting() || !target.targetText) {
            return VALUE_TEXT;
        }
        if (target.targetTextChange != TextChange::Sweep && target.targetTextChange != TextChange::Retype) {
            return VALUE_TEXT_CHANGE;
        }
    }
    if ((properties & TARGET_DASH_OFFSET) && (!object->style.dashCount || !std::isfinite(target.targetDashOffset))) {
        return VALUE_DASH;
    }
    auto type = object->type();
    if ((properties & (TARGET_TAIL | TARGET_TIP))
        && type != Type::Arrow && type != Type::Vector && type != Type::Connector && type != Type::Route) {
        return VALUE_MARKER;
    }
    if (((properties & TARGET_TAIL) && (!std::isfinite(target.targetTail) || target.targetTail < 0.0f))
        || ((properties & TARGET_TIP) && (!std::isfinite(target.targetTip) || target.targetTip < 0.0f))) {
        return VALUE_MARKER_SIZE;
    }
    if ((properties & TARGET_TRIM_DISTANCE)
        && ((type != Type::Line && type != Type::Arrow && type != Type::Vector)
            || !_finite(target.targetTrimDistance) || target.targetTrimDistance.x < 0.0f || target.targetTrimDistance.y < 0.0f)) {
        return "trim distance needs a line, arrow or vector and non-negative distances";
    }
    if ((properties & TARGET_TRIM_DISTANCE) && warped(object)) {
        return "a warped shape keeps the outline its warp gave, so trim distance no longer changes";
    }
    if (properties & TARGET_GEOMETRY) {
        if (!geometric(object)) return VALUE_GEOMETRY;
        if (target.targetContoured && (type != Type::Plot || !(std::fabs(target.targetContour.level) <= std::numeric_limits<float>::max())
            || !_finite(Vec3{target.targetContour.x.min, target.targetContour.x.max, target.targetContour.x.step})
            || !_finite(Vec3{target.targetContour.y.min, target.targetContour.y.max, target.targetContour.y.step})
            || target.targetContour.x.min >= target.targetContour.x.max || target.targetContour.x.step <= 0.0f
            || target.targetContour.y.min >= target.targetContour.y.max || target.targetContour.y.step <= 0.0f)) {
            return "contour geometry needs finite ranges and a level";
        }
        if (!target.targetSamples.valid()) return "plot samples need an increasing x grid and a finite y_range";
        auto points = target.targetGeometry;
        auto count = target.targetGeometryCount;
        if (target.targetGeometryPieced) {
            if (type != Type::Plot) return VALUE_GEOMETRY_PIECES;
            if ((count && !points) || !_pieceEnds(target.targetGeometryEnds, target.targetGeometryPieces, count)) {
                return VALUE_GEOMETRY_ENDS;
            }
        } else if (!points || count != shapeCount(object)) {
            return VALUE_GEOMETRY_COUNT;
        }
        for (auto i = 0u; i < count; i++) {
            if (!_finite(points[i])) return VALUE_GEOMETRY_POINTS;
        }
        // Controls far apart could still carry a point of the curve out of range.
        if (type == Type::Curve) {
            auto samples = static_cast<const Curve*>(object)->count();
            for (auto i = 0u; i < samples; i++) {
                if (!_finite(curvePoint(points, i, samples))) return VALUE_GEOMETRY_POINTS;
            }
        }
    }
    return nullptr;
}

VisualState Scene::Impl::targeted(const AnimationTarget& target, const VisualState& state,
                                  const char* text, const GeometryShape* geometry) noexcept
{
    auto properties = target.properties;
    auto to = state;
    if (properties & TARGET_MODEL) to.model = target.targetModel;
    if (properties & TARGET_OPACITY) to.opacity = target.targetOpacity;
    if (properties & TARGET_STROKE) to.stroke = target.targetStroke;
    if (properties & TARGET_FILL) to.fill = target.targetFill;
    if (properties & TARGET_DASH_OFFSET) to.dashOffset = target.targetDashOffset;
    if (properties & TARGET_TAIL) to.markerTail = target.targetTail;
    if (properties & TARGET_TIP) to.markerTip = target.targetTip;
    if (properties & TARGET_PROGRESS) to.progress = target.targetProgress;
    if (properties & TARGET_WIDTH) to.width = target.targetWidth;
    if (properties & TARGET_RADIUS) {
        if (target.target->type() == Type::Circle || target.target->type() == Type::Arc || target.target->type() == Type::Angle) to.size = {target.targetRadius, target.targetRadius};
        else to.radius = target.targetRadius;
    }
    if (properties & TARGET_ARC_START) to.arcStart = target.targetStart;
    if (properties & TARGET_ARC_ANGLE) to.arcAngle = target.targetAngle;
    if (properties & TARGET_SIZE) to.size = target.targetSize;
    if (properties & TARGET_CORNER) to.corner = target.targetCorner;
    if (properties & TARGET_TRIM_DISTANCE) to.trimDistance = target.targetTrimDistance;
    if (properties & TARGET_GRADIENT) {
        to.gradient = target.targetGradient;
        to.gradientEnd = target.targetGradientEnd;
    }
    if (properties & TARGET_BLUR) to.blur = target.targetBlur;
    if (properties & TARGET_SHADOW) to.shadow = target.targetShadow;
    if (properties & TARGET_VALUE) to.value = target.targetValue;
    if ((properties & TARGET_TEXT) && text) {
        to.text = text;
        to.textFrom = text;
        to.textProgress = 1.0f;
        to.textChange = target.targetTextChange;
    }
    if (properties & TARGET_LAYER) to.layer = target.targetLayer;
    if ((properties & TARGET_GEOMETRY) && geometry) {
        to.geometry = geometry;
        to.geometryFrom = geometry;
        to.geometryProgress = 1.0f;
    }
    return to;
}

// How many of the latest copies keep() looks through for one that reads the same, enough
// for a text that goes back and forth between a few strings.
static constexpr uint32_t TEXT_REUSE = 16;

const char* Scene::Impl::keep(Text* text, const char* value, bool& added) noexcept
{
    added = false;
    if (std::strcmp(text->objText, value) == 0) return text->objText;
    auto count = text->objTextCnt;
    for (auto i = count; i > 0u && i + TEXT_REUSE > count; i--) {
        if (std::strcmp(text->objTexts[i - 1u], value) == 0) return text->objTexts[i - 1u];
    }
    auto copy = _duplicate(value);
    // The array grows in steps of powers of two from two strings: the first and the copy.
    auto full = count < 2u || (count & (count - 1u)) == 0u;
    auto texts = full ? new (std::nothrow) char*[count < 2u ? 2u : count * 2u] : text->objTexts;
    if (!copy || !texts) {
        delete[] copy;
        if (texts != text->objTexts) delete[] texts;
        return nullptr;
    }
    if (full) {
        if (count) std::memcpy(texts, text->objTexts, sizeof(char*) * count);
        else texts[count++] = text->objText;
        delete[] text->objTexts;
        text->objTexts = texts;
    }
    texts[count++] = copy;
    text->objTextCnt = count;
    added = true;
    return copy;
}

void Scene::Impl::drop(Text* text) noexcept
{
    if (!text->objTextCnt) return;
    delete[] text->objTexts[--text->objTextCnt];
    // Only the first string left: the text owns it on its own again.
    if (text->objTextCnt == 1u) {
        delete[] text->objTexts;
        text->objTexts = nullptr;
        text->objTextCnt = 0u;
    }
}

// The stroke and fill a play call target animates on a group, which pass to its heirs.
struct Passing
{
    const Object* group = nullptr;
    uint32_t properties = 0;
};

// The heirs a play call animates along with their groups.
struct Inheritance
{
    Heir* heirs = nullptr;
    uint32_t count = 0;
    ~Inheritance() { delete[] heirs; }
};

// Collects the heirs of the groups whose colors a play call of count targets animates.
// passing(i) gives what target i passes, and named(i, object) the colors target i animates on
// object itself; those, and the colors a nearer group in the call passes, win. Impl is the
// scene's implementation, which only scene members may name.
template<typename Impl, typename Passes, typename Names>
static Result _inherit(Impl& impl, uint32_t count, float duration, float lag, Passes passing,
                       Names named, Inheritance& inheritance)
{
    auto total = 0u;
    for (auto i = 0u; i < count; i++) {
        auto passed = passing(i);
        if (passed.properties) total = impl.heirs(passed.group, passed.properties, nullptr, 0u, total);
    }
    if (!total) return Result::Success;
    inheritance.heirs = new (std::nothrow) Heir[total];
    if (!inheritance.heirs) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    auto filled = 0u;
    for (auto i = 0u; i < count; i++) {
        auto passed = passing(i);
        if (!passed.properties) continue;
        auto first = filled;
        filled = impl.heirs(passed.group, passed.properties, inheritance.heirs, total, filled);
        for (auto j = first; j < filled; j++) inheritance.heirs[j].target = i;
    }
    for (auto i = 0u; i < total; i++) {
        auto heir = inheritance.heirs[i];
        auto begin = _begin(impl.cursor, duration, lag, heir.target);
        // Removed shapes, and shapes added after the animation starts, keep their colors.
        if (!heir.entry || heir.entry->dead >= 0.0f || begin < heir.entry->born) continue;
        auto group = passing(heir.target).group;
        for (const Object* above = heir.object; above && above != group; above = above->parent()) {
            for (auto j = 0u; j < count; j++) {
                auto nearer = passing(j);
                heir.properties &= ~(above == heir.object ? named(j, above) : nearer.group == above ? nearer.properties : 0u);
            }
        }
        if (!heir.properties) continue;
        if (auto reason = _unplayable(heir.entry, heir.object)) {
            return impl.fail(Result::InvalidArguments, reason == TARGET_CHANGED ? TARGET_HEIR_CHANGED : reason);
        }
        if (begin < _channelEnd(*heir.entry, _channels(heir.properties, heir.object->type()))) {
            return impl.fail(Result::InsufficientCondition, TIMELINE_HEIR);
        }
        inheritance.heirs[inheritance.count++] = heir;
    }
    return Result::Success;
}

// Writes a clip for each heir that carries it to the values its group animates to, which
// values(i, properties, state) writes for play call target i. The clips were already allocated.
template<typename Impl, typename Values>
static void _inheritClips(Impl& impl, const Inheritance& inheritance, float duration, float lag,
                          const AnimCurve& curve, Values values)
{
    for (auto i = 0u; i < inheritance.count; i++) {
        auto& heir = inheritance.heirs[i];
        auto from = _state(heir.object);
        auto to = from;
        values(heir.target, heir.properties, to);
        auto& clip = impl.clips[impl.clipCnt++];
        clip = Clip{};
        clip.object = heir.object;
        impl.link(impl.clipCnt - 1u);
        clip.from = from;
        clip.to = to;
        clip.begin = _begin(impl.cursor, duration, lag, heir.target);
        clip.end = _end(impl.cursor, duration, lag, heir.target);
        clip.curve = curve;
        clip.channels = _channels(heir.properties, heir.object->type());
        _channelEnd(*heir.entry, clip.channels, clip.end);
        impl.store(heir.object, to);
        heir.entry->fingerprint = fingerprint(heir.object);
    }
}

// Plays animations with the object of each morph from a copy replaced by a clone of it made
// beside it at the cursor; when a clone or the play fails, the clones go again. Impl is the
// scene's implementation, which only scene members may name.
template<typename Impl>
static Result _playCopies(Scene& scene, Impl& impl, const Animation* animations, uint32_t count,
                          uint32_t copies, float duration, const AnimCurve& curve, float lag) noexcept
{
    struct Copied
    {
        Object* copy = nullptr;
        uint32_t first = 0;
    };
    auto local = new (std::nothrow) Animation[count];
    auto copied = new (std::nothrow) Copied[copies];
    auto result = local && copied ? Result::Success : impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    auto made = 0u;
    for (auto i = 0u; result == Result::Success && i < count; i++) {
        local[i] = animations[i];
        if (!local[i].copy) continue;
        auto& item = copied[made];
        item.first = impl.objectCnt;
        result = scene.clone(local[i].target, item.copy);
        if (result != Result::Success) break;
        made++;
        local[i].target = item.copy;
        local[i].copy = false;
    }
    if (result == Result::Success) result = scene.play(local, count, duration, curve, lag);
    if (result != Result::Success) {
        for (auto i = made; i > 0u; i--)
            impl.discard(copied[i - 1u].copy, copied[i - 1u].first);
    }
    delete[] local;
    delete[] copied;
    return result;
}

// Following a connector reads the models of both endpoint families and the follower's
// parent. Refuse cycles, including dependencies added in the same play call.
template<typename Impl>
static Result _connectorCycle(const Impl& impl, const Animation* animations, uint32_t count,
                              const Animation& animation)
{
    auto seen = new (std::nothrow) bool[impl.objectCnt]{};
    auto queue = new (std::nothrow) const Object*[impl.objectCnt];
    if (!seen || !queue) {
        delete[] seen;
        delete[] queue;
        return Result::OutOfMemory;
    }
    auto size = 0u;
    auto add = [&](const Object* object) {
        for (auto item = object; item; item = item->parent()) {
            auto entry = impl.entry(item);
            if (!entry) continue;
            auto index = static_cast<uint32_t>(entry - impl.objects);
            if (seen[index]) continue;
            seen[index] = true;
            queue[size++] = item;
        }
    };
    auto family = [&](const Object* object) {
        for (auto i = 0u; i < impl.objectCnt; i++) {
            if (_inside(impl.objects[i].object, object)) add(impl.objects[i].object);
        }
    };
    auto read = [&](const Object* object) {
        auto connector = static_cast<const Connector*>(object);
        family(connector->from());
        family(connector->to());
    };
    read(animation.related);
    add(animation.target->parent());
    auto cyclic = false;
    for (auto head = 0u; head < size && !cyclic; head++) {
        auto object = queue[head];
        if (object == animation.target) {
            cyclic = true;
            break;
        }
        auto entry = impl.entry(object);
        for (auto i = entry->firstClip; i != NO_CLIP; i = impl.clips[i].next) {
            auto& clip = impl.clips[i];
            if (clip.motion != ClipMotion::Path) continue;
            auto& path = impl.paths[clip.path - 1u];
            if (path.source->type() != Type::Connector) continue;
            read(path.source);
            add(path.parent);
        }
        for (auto i = 0u; i < count; i++) {
            auto& proposed = animations[i];
            if (proposed.target == object && proposed.kind == AnimationKind::MoveAlong && proposed.related
                && proposed.related->type() == Type::Connector) {
                read(proposed.related);
                add(proposed.target->parent());
            }
        }
    }
    delete[] seen;
    delete[] queue;
    return cyclic ? Result::InvalidArguments : Result::Success;
}

// ---------- Playing animations ----------

Result Scene::play(const Animation* animations, uint32_t count, float duration, Easing easing, float lag) noexcept
{
    return play(animations, count, duration, _legacyCurve(easing), lag);
}

Result Scene::play(const Animation* animations, uint32_t count, float duration,
                   const AnimCurve& curve, float lag) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (!animations || !count) return impl.fail(Result::InvalidArguments, PLAY_EMPTY);
    if (!std::isfinite(duration)) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (!std::isfinite(lag)) return impl.fail(Result::InvalidArguments, PLAY_LAG);
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (duration <= 0.0f) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (lag < 0.0f) return impl.fail(Result::InvalidArguments, PLAY_LAG);
    if (count > MAX_ANIMATION_GROUP) return impl.fail(Result::InvalidArguments, PLAY_GROUP);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);
    float nextCursor;
    if (!_timeline(impl.cursor, duration, lag, count, nextCursor)) {
        return impl.fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    auto copies = 0u;
    for (auto i = 0u; i < count; i++) {
        if (!animations[i].copy) continue;
        if (animations[i].kind != AnimationKind::Morph) return impl.fail(Result::InvalidArguments, PLAY_COPY);
        copies++;
    }
    if (copies) return _playCopies(*this, impl, animations, count, copies, duration, curve, lag);
    for (auto i = 0u; i < count; i++) {
        if (animations[i].kind != AnimationKind::Morph && (animations[i].pairs || animations[i].pairCount)) {
            return impl.fail(Result::InvalidArguments, "only a morph takes piece pairs");
        }
        auto target = animations[i].target;
        auto entry = target ? impl.entry(target) : nullptr;
        if (auto reason = _unplayable(entry, target)) return impl.fail(Result::InvalidArguments, reason);
        auto& animation = animations[i];
        auto related = animation.related;
        auto relatedEntry = related ? impl.entry(related) : nullptr;
        switch (animation.kind) {
            case AnimationKind::Transform:
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                if (!_transformable(target)) return impl.fail(Result::InvalidArguments, TARGET_FIXED);
                if (!_affine(animation.model)) return impl.fail(Result::InvalidArguments, TARGET_MATRIX);
                if (!_arc(animation.arc)) return impl.fail(Result::InvalidArguments, MOTION_ARC);
                if (animation.arc != 0.0f && !_axis(animation.axis)) {
                    return impl.fail(Result::InvalidArguments, MOTION_AXIS);
                }
                break;
            case AnimationKind::Rotate:
            case AnimationKind::Scale: {
                if (!_transformable(target)) return impl.fail(Result::InvalidArguments, TARGET_FIXED);
                if (!_finite(animation.pivot) || !_finite(animation.offset) || !std::isfinite(animation.angle)
                    || !_finite(animation.stretch)) {
                    return impl.fail(Result::InvalidArguments, MOTION_VALUES);
                }
                if (!_arc(animation.arc)) return impl.fail(Result::InvalidArguments, MOTION_ARC);
                if (!_axis(animation.axis)) return impl.fail(Result::InvalidArguments, MOTION_AXIS);
                if (related) {
                    if (auto reason = _unplayable(relatedEntry, related, true)) {
                        return impl.fail(Result::InvalidArguments, reason);
                    }
                }
                break;
            }
            case AnimationKind::MoveAlong: {
                auto closed = false;
                if (!related) return impl.fail(Result::InvalidArguments, MOTION_PATH);
                if (related == target) return impl.fail(Result::InvalidArguments, MOTION_SELF);
                if (auto reason = _unplayable(relatedEntry, related, true)) {
                    return impl.fail(Result::InvalidArguments, reason);
                }
                if (!_transformable(target)) return impl.fail(Result::InvalidArguments, TARGET_FIXED);
                if (!std::isfinite(animation.from) || !std::isfinite(animation.to)
                    || animation.from < 0.0f || animation.from > 1.0f
                    || animation.to < 0.0f || animation.to > 1.0f) {
                    return impl.fail(Result::InvalidArguments, MOTION_FRACTION);
                }
                if (related->type() == Type::Connector) {
                    auto connector = static_cast<const Connector*>(related);
                    if (_inside(connector->from(), target) || _inside(connector->to(), target)) {
                        return impl.fail(Result::InvalidArguments, "an endpoint cannot move along its connector");
                    }
                    auto result = _connectorCycle(impl, animations, count, animation);
                    if (result != Result::Success) {
                        return impl.fail(result, result == Result::OutOfMemory ? SCENE_MEMORY
                                         : "moving along a connector cannot depend on the target");
                    }
                } else if (outline(related, nullptr, 0u, closed) < 2u) {
                    return impl.fail(Result::InvalidArguments, MOTION_OUTLINE);
                }
                break;
            }
            case AnimationKind::Create:
            case AnimationKind::Uncreate:
            case AnimationKind::DrawBorderThenFill:
            case AnimationKind::Write:
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                if (static_cast<uint8_t>(animation.direction) > static_cast<uint8_t>(DrawDirection::CounterClockwise)) {
                    return impl.fail(Result::InvalidArguments, DRAW_DIRECTION);
                }
                break;
            case AnimationKind::FillReveal:
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                break;
            case AnimationKind::Stroke:
            case AnimationKind::Fill: {
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                Targeted values;
                values.properties = animation.kind == AnimationKind::Stroke ? TARGET_STROKE : TARGET_FILL;
                values.stroke = animation.color;
                values.fill = animation.color;
                if (auto reason = _unanimatable(target, values)) return impl.fail(Result::InvalidArguments, reason);
                break;
            }
            case AnimationKind::Fade:
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                if (!std::isfinite(animation.value) || animation.value < 0.0f || animation.value > 1.0f) {
                    return impl.fail(Result::InvalidArguments, VALUE_OPACITY);
                }
                break;
            case AnimationKind::FadeIn:
            case AnimationKind::FadeOut: {
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                if (!_finite(animation.offset)) return impl.fail(Result::InvalidArguments, MOTION_VALUES);
                if (!std::isfinite(animation.scale) || animation.scale <= 0.0f) {
                    return impl.fail(Result::InvalidArguments, EFFECT_SCALE);
                }
                auto moves = animation.offset.length() > 0.0f || animation.scale != 1.0f;
                Mat4 effect;
                if (moves && !_transformable(target)) return impl.fail(Result::InvalidArguments, TARGET_FIXED);
                if (moves && !_effectModel(target, target->model, animation.scale, animation.offset, effect)) {
                    return impl.fail(Result::InvalidArguments, EFFECT_BOUNDS);
                }
                break;
            }
            case AnimationKind::Grow:
            case AnimationKind::GrowFromEdge:
            case AnimationKind::Shrink: {
                Mat4 effect;
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                if (!_transformable(target)) return impl.fail(Result::InvalidArguments, TARGET_FIXED);
                if (animation.kind == AnimationKind::GrowFromEdge
                        ? !_growthModel(target, target->model, animation.growthEdge, effect)
                        : !_effectModel(target, target->model, 0.0f, {}, effect)) {
                    return impl.fail(Result::InvalidArguments, EFFECT_BOUNDS);
                }
                break;
            }
            case AnimationKind::Indicate: {
                Mat4 effect;
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                if (!_transformable(target)) return impl.fail(Result::InvalidArguments, TARGET_FIXED);
                if (!std::isfinite(animation.scale) || animation.scale <= 1.0f) {
                    return impl.fail(Result::InvalidArguments, EFFECT_INDICATE);
                }
                if (!_effectModel(target, target->model, animation.scale, {}, effect)) {
                    return impl.fail(Result::InvalidArguments, EFFECT_BOUNDS);
                }
                break;
            }
            case AnimationKind::Wiggle: {
                Mat4 effect;
                if (related) return impl.fail(Result::InvalidArguments, PLAY_RELATED);
                if (!_transformable(target)) return impl.fail(Result::InvalidArguments, TARGET_FIXED);
                if (!std::isfinite(animation.scale) || animation.scale <= 0.0f || !std::isfinite(animation.angle)
                    || !std::isfinite(animation.value) || animation.value <= 0.0f) {
                    return impl.fail(Result::InvalidArguments, EFFECT_WIGGLE);
                }
                if (!_effectModel(target, target->model, animation.scale, {}, effect)) {
                    return impl.fail(Result::InvalidArguments, EFFECT_BOUNDS);
                }
                break;
            }
            case AnimationKind::Morph: {
                if (!related || related == target) return impl.fail(Result::InvalidArguments, MORPH_TARGET);
                if (auto reason = _unplayable(relatedEntry, related)) return impl.fail(Result::InvalidArguments, reason);
                if (auto reason = _morphGeometry(target, related)) return impl.fail(Result::NonSupport, reason);
                if (!validMorphPairs(animation)) return impl.fail(Result::InvalidArguments, MORPH_PAIRS);
                if (animation.sourceColors || animation.targetColors || animation.colorSteps
                    || animation.colorCount) {
                    auto surface = target->type() == Type::SurfaceMesh ? static_cast<const SurfaceMesh*>(target)
                                                                       : nullptr;
                    auto points = surface ? static_cast<uint64_t>(surface->columns()) * surface->rows() : 0u;
                    if (!surface || (!animation.sourceColors && !animation.targetColors)
                        || animation.colorSteps < 1u || animation.colorSteps > MAX_MORPH_COLOR_STEPS
                        || animation.colorCount != (animation.colorSteps + 1ull) * points) {
                        return impl.fail(Result::InvalidArguments, MORPH_COLORS);
                    }
                }
                if (target->childCount() || related->childCount()) {
                    return impl.fail(Result::InsufficientCondition, MORPH_CHILDREN);
                }
                if (target->parent() != related->parent()) return impl.fail(Result::InsufficientCondition, MORPH_PARENT);
                if (relatedEntry->born != impl.cursor || _hasClip(*relatedEntry)) {
                    return impl.fail(Result::InsufficientCondition, MORPH_FRESH);
                }
                if (_connectorReferences(impl.objects, impl.objectCnt, target)
                    || _connectorReferences(impl.objects, impl.objectCnt, related)) {
                    return impl.fail(Result::InsufficientCondition, MORPH_CONNECTED);
                }
                break;
            }
            default: return impl.fail(Result::InvalidArguments, PLAY_KIND);
        }
        for (auto j = i + 1; j < count; j++) {
            if (_sameAnimationObject(animation, animations[j])) return impl.fail(Result::InvalidArguments, PLAY_TWICE);
        }
        auto begin = _begin(pImpl->cursor, duration, lag, i);
        if (begin < entry->born) return pImpl->fail(Result::InsufficientCondition, TIMELINE_UNBORN);
        auto channels = _channels(animation);
        // A shift, turn, scale, travel, transform, morph or effect may begin while another clip
        // moves the target; only a shift adds to it without an inverse of the model it starts
        // from. A morph must not end first, since the shape it becomes takes no part in that clip.
        if ((channels & CHANNEL_MODEL) && begin < _channelEnd(*entry, CHANNEL_MODEL) && _layerable(animation)) {
            auto morph = animation.kind == AnimationKind::Morph;
            auto end = _end(pImpl->cursor, duration, lag, i);
            if (morph && end < _channelEnd(*entry, CHANNEL_MODEL)) {
                return _overlap(impl, *entry, CHANNEL_MODEL, begin, MORPH_EARLY);
            }
            Mat4 inverse;
            Mat4 change;
            auto whole = morph ? &related->model : animation.kind == AnimationKind::Transform ? &animation.model : nullptr;
            auto shifts = whole && _shifts(target->model, *whole, change);
            if (!shifts && !_inverse(target->model, inverse)) {
                return impl.fail(Result::InsufficientCondition, TIMELINE_FLAT);
            }
            channels &= ~CHANNEL_MODEL;
        }
        if (begin < _channelEnd(*entry, channels)) return _overlap(impl, *entry, channels, begin);
    }
    // A group's stroke and fill animate the shapes that take those colors from it.
    auto passing = [animations](uint32_t i) -> Passing {
        auto& animation = animations[i];
        if (animation.target->type() != Type::Group) return {};
        if (animation.kind == AnimationKind::Stroke) return {animation.target, TARGET_STROKE};
        if (animation.kind == AnimationKind::Fill) return {animation.target, TARGET_FILL};
        return {};
    };
    auto named = [animations](uint32_t i, const Object* object) -> uint32_t {
        auto& animation = animations[i];
        // A morph hands the target's look to the related object when it ends.
        if (animation.target != object && (animation.kind != AnimationKind::Morph || animation.related != object)) {
            return 0u;
        }
        auto channels = _channels(animation);
        return ((channels & CHANNEL_STROKE) ? TARGET_STROKE : 0u) | ((channels & CHANNEL_FILL) ? TARGET_FILL : 0u)
               | ((channels & CHANNEL_WIDTH) ? TARGET_WIDTH : 0u);
    };
    Inheritance inheritance;
    auto inherited = _inherit(impl, count, duration, lag, passing, named, inheritance);
    if (inherited != Result::Success) return inherited;
    if (!pImpl->growClips(count + inheritance.count)) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    auto previousClipCount = pImpl->clipCnt;
    auto previousPathCount = pImpl->pathCnt;
    auto previousPathVertices = pImpl->pathVertices;
    auto previousMorphCount = pImpl->morphCnt;
    auto previousMorphPoints = pImpl->morphPoints;
    for (auto i = 0u; i < count; i++) {
        auto result = pImpl->prepare(animations[i], pImpl->clips[previousClipCount + i],
                                     _begin(pImpl->cursor, duration, lag, i), _end(pImpl->cursor, duration, lag, i));
        if (result != Result::Success) {
            pImpl->dropPaths(previousPathCount, previousPathVertices);
            pImpl->dropMorphs(previousMorphCount, previousMorphPoints);
            return result;
        }
    }

    for (auto i = 0u; i < count; i++) {
        auto& animation = animations[i];
        auto entry = pImpl->entry(animation.target);
        auto from = _state(animation.target);
        auto to = from;
        auto begin = _begin(pImpl->cursor, duration, lag, i);
        auto end = _end(pImpl->cursor, duration, lag, i);
        auto authored = from.model;
        auto layered = (_channels(animation) & CHANNEL_MODEL) && begin < _channelEnd(*entry, CHANNEL_MODEL);
        switch (animation.kind) {
            // A layered shift leaves the authored model shifted, its turn and scale those it had;
            // any other transform leaves its matrix, which the object has once every motion ends.
            case AnimationKind::Transform: {
                Mat4 change;
                to.model = layered && _shifts(authored, animation.model, change) ? change * authored : animation.model;
                break;
            }
            case AnimationKind::Create: {
                from.progress = 0.0f;
                to.progress = 1.0f;
                from.direction = animation.direction;
                to.direction = animation.direction;
                _hideBefore(*entry, pImpl->clips, previousClipCount, true);
                break;
            }
            case AnimationKind::Uncreate:
                to.progress = 0.0f;
                from.direction = animation.direction;
                to.direction = animation.direction;
                entry->revealScheduled = true;
                break;
            case AnimationKind::FillReveal:
                from.fillProgress = 0.0f;
                to.fillProgress = 1.0f;
                _hideFillBefore(*entry, pImpl->clips, previousClipCount);
                break;
            case AnimationKind::DrawBorderThenFill:
                from.progress = 0.0f;
                from.fillProgress = 0.0f;
                to.progress = 1.0f;
                to.fillProgress = 1.0f;
                from.direction = animation.direction;
                to.direction = animation.direction;
                _hideBefore(*entry, pImpl->clips, previousClipCount, true);
                _hideFillBefore(*entry, pImpl->clips, previousClipCount);
                break;
            case AnimationKind::Write:
                from.progress = 0.0f;
                to.progress = 1.0f;
                from.traceFill = true;
                to.traceFill = true;
                from.direction = animation.direction;
                to.direction = animation.direction;
                _hideBefore(*entry, pImpl->clips, previousClipCount, true);
                break;
            case AnimationKind::Fade:
                to.opacity = animation.value;
                entry->revealScheduled = true;
                break;
            case AnimationKind::FadeIn:
                from.opacity = 0.0f;
                to.opacity = 1.0f;
                if (animation.scale != 1.0f || animation.offset.length() > 0.0f) {
                    _effectModel(animation.target, to.model, animation.scale, animation.offset, from.model);
                    from.pixelScale = animation.scale;
                }
                _hideBefore(*entry, pImpl->clips, previousClipCount, false);
                break;
            case AnimationKind::FadeOut:
                to.opacity = 0.0f;
                if (animation.scale != 1.0f || animation.offset.length() > 0.0f) {
                    _effectModel(animation.target, from.model, animation.scale, animation.offset, to.model);
                    to.pixelScale = animation.scale;
                }
                entry->revealScheduled = true;
                break;
            case AnimationKind::Grow:
            case AnimationKind::GrowFromEdge:
                if (animation.kind == AnimationKind::GrowFromEdge) {
                    _growthModel(animation.target, to.model, animation.growthEdge, from.model);
                } else {
                    _effectModel(animation.target, to.model, 0.0f, {}, from.model);
                }
                from.opacity = 0.0f;
                from.pixelScale = 0.0f;
                to.opacity = 1.0f;
                _hideBefore(*entry, pImpl->clips, previousClipCount, false);
                break;
            case AnimationKind::Shrink:
                _effectModel(animation.target, from.model, 0.0f, {}, to.model);
                to.opacity = 0.0f;
                to.pixelScale = 0.0f;
                entry->revealScheduled = true;
                break;
            // A wiggle ends where it began; its sampler adds the growth and the turns.
            case AnimationKind::Wiggle: entry->revealScheduled = true; break;
            case AnimationKind::Indicate:
                _effectModel(animation.target, from.model, animation.scale, {}, to.model);
                to.pixelScale = animation.scale;
                if (from.stroke.a) {
                    to.stroke = animation.color;
                    to.stroke.a = from.stroke.a;
                }
                if (from.fill.a) {
                    to.fill = animation.color;
                    to.fill.a = from.fill.a;
                }
                entry->revealScheduled = true;
                break;
            case AnimationKind::Morph: {
                auto targetEntry = pImpl->entry(animation.related);
                if (animation.related->objThemeColor
                    || animation.related->objThemeGradient) {
                    _inherit(*targetEntry, pImpl->clips, animation.target->style,
                             animation.related->objThemeColor,
                             animation.related->objThemeGradient);
                }
                to = _state(animation.related);
                // A target cloned from a warped object shows its warp once it takes over; until
                // then the source draws the shape between them, which already ends in that warp.
                to.warp = nullptr;
                to.warpProgress = 0.0f;
                targetEntry->born = end;
                targetEntry->revealScheduled = true;
                entry->dead = end;
                break;
            }
            case AnimationKind::Stroke: to.stroke = animation.color; break;
            case AnimationKind::Fill: to.fill = animation.color; break;
            case AnimationKind::Rotate:
            case AnimationKind::Scale:
            case AnimationKind::MoveAlong: to.model = pImpl->clips[pImpl->clipCnt].to.model; break;
        }

        auto& clip = pImpl->clips[pImpl->clipCnt++];
        clip.object = animation.target;
        pImpl->link(pImpl->clipCnt - 1u);
        clip.from = from;
        clip.to = to;
        clip.begin = begin;
        clip.end = end;
        clip.curve = curve;
        clip.kind = animation.kind;
        clip.progressReveal = animation.kind == AnimationKind::Create
                              || animation.kind == AnimationKind::Uncreate
                              || animation.kind == AnimationKind::Write
                              || animation.kind == AnimationKind::DrawBorderThenFill;
        clip.related = animation.related;
        clip.channels = _channels(animation);
        if (layered) _layered(clip, animation, authored);
        _channelEnd(*entry, clip.channels, end);
        if (animation.kind == AnimationKind::Indicate || animation.kind == AnimationKind::Morph) continue;
        if (animation.kind == AnimationKind::FadeOut || animation.kind == AnimationKind::Shrink) {
            auto authored = from;
            authored.opacity = 0.0f;
            Impl::store(animation.target, authored);
        } else {
            Impl::store(animation.target, to);
        }
        entry->fingerprint = fingerprint(animation.target);
        // The object now sets the color itself; below a group, shapes added later inherit it.
        if (animation.kind == AnimationKind::Stroke) animation.target->objStrokeAuthored = true;
        if (animation.kind == AnimationKind::Fill) animation.target->objFillAuthored = true;
    }
    _inheritClips(impl, inheritance, duration, lag, curve, [animations](uint32_t i, uint32_t properties, VisualState& to) {
        if (properties & TARGET_STROKE) to.stroke = animations[i].color;
        if (properties & TARGET_FILL) to.fill = animations[i].color;
    });
    // All endpoint clips now exist, including those scheduled alongside a follower. Keep
    // the follower's authored model where sampling leaves it for subsequent motions.
    for (auto i = 0u; i < count; i++) {
        auto& animation = animations[i];
        if (animation.kind != AnimationKind::MoveAlong || animation.related->type() != Type::Connector) continue;
        auto entry = pImpl->entry(animation.target);
        VisualState state;
        if (pImpl->sample(*entry, _channelEnd(*entry, CHANNEL_MODEL), state)) {
            animation.target->model = state.model;
            entry->fingerprint = fingerprint(animation.target);
        }
    }
    pImpl->advance(nextCursor);
    pImpl->definitionsSealed = true;
    return Result::Success;
}

Result Scene::play(const AnimationTarget& target, float duration, Easing easing) noexcept
{
    return play(target, duration, _legacyCurve(easing));
}

Result Scene::play(const AnimationTarget& target, float duration, const AnimCurve& curve) noexcept
{
    return play(&target, 1u, duration, curve, 0.0f);
}

Result Scene::play(const AnimationTarget* targets, uint32_t count, float duration,
                   Easing easing, float lag) noexcept
{
    return play(targets, count, duration, _legacyCurve(easing), lag);
}

// Points state at the geometry its object has now: the scene's copy of it.
static void _reshaped(VisualState& state, const GeometryShape* geometry)
{
    state.geometry = geometry;
    state.geometryFrom = geometry;
    state.geometryProgress = 1.0f;
}

GeometryShape Scene::Impl::shaping(const AnimationTarget& target, const GeometryShape& current) noexcept
{
    GeometryShape shape;
    shape.points = target.targetGeometry;
    shape.count = target.targetGeometryCount;
    shape.samples = target.targetSamples;
    shape.contour = target.targetContoured ? target.targetContour : current.contour;
    if (target.targetGeometryPieced) {
        shape.pieces = target.targetGeometryPieces;
        shape.ends = shape.pieces > 1u ? target.targetGeometryEnds : nullptr;
    } else {
        shape.pieces = current.pieces;
        shape.ends = current.ends;
    }
    return shape;
}

Result Scene::Impl::reshape(const AnimationTarget* targets, uint32_t count, const GeometryShape** shaped,
                            uint32_t& previousCount, uint32_t& previousPoints) noexcept
{
    auto& impl = *this;
    for (auto i = 0u; i < count; i++) {
        if (!(targets[i].properties & TARGET_GEOMETRY)) continue;
        if (!impl.reshapable(*impl.entry(targets[i].target))) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    previousCount = impl.geometryCnt;
    previousPoints = impl.geometryPoints;
    for (auto i = 0u; i < count; i++) {
        auto& target = targets[i];
        if (!(target.properties & TARGET_GEOMETRY)) continue;
        auto entry = impl.entry(target.target);
        auto shape = shaping(target, *entry->geometry);
        shaped[i] = impl.reshape(shape, entry->geometry);
        // A plot that changes its pieces makes room for them before any clip ends at them.
        if (shaped[i] && room(target.target, *shaped[i])) continue;
        auto full = !shaped[i] && shape.count + shape.samples.count > MAX_GEOMETRY_POINTS - impl.geometryPoints;
        impl.dropGeometries(previousCount, previousPoints);
        return full ? impl.fail(Result::InsufficientCondition, GEOMETRY_LIMIT)
                    : impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    return Result::Success;
}

Result Scene::play(const AnimationTarget* targets, uint32_t count, float duration,
                   const AnimCurve& curve, float lag) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (!targets || !count) return impl.fail(Result::InvalidArguments, PLAY_EMPTY);
    if (!std::isfinite(duration)) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (!std::isfinite(lag)) return impl.fail(Result::InvalidArguments, PLAY_LAG);
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (duration <= 0.0f) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (lag < 0.0f) return impl.fail(Result::InvalidArguments, PLAY_LAG);
    if (count > MAX_ANIMATION_GROUP) return impl.fail(Result::InvalidArguments, PLAY_GROUP);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);
    float nextCursor;
    if (!_timeline(impl.cursor, duration, lag, count, nextCursor)) {
        return impl.fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    for (auto i = 0u; i < count; i++) {
        auto& target = targets[i];
        auto entry = target.target ? impl.entry(target.target) : nullptr;
        if (auto reason = _unplayable(entry, target.target)) return impl.fail(Result::InvalidArguments, reason);
        if (auto reason = Impl::unanimatable(target)) return impl.fail(Result::InvalidArguments, reason);
        if (_warpSized(target.target, target.properties)) {
            return impl.fail(Result::InvalidArguments, WARP_SIZE);
        }
        if ((target.properties & TARGET_GEOMETRY) && warped(target.target)) {
            return impl.fail(Result::InvalidArguments, WARP_GEOMETRY);
        }
        for (auto j = i + 1u; j < count; j++) {
            if (target.target == targets[j].target) return impl.fail(Result::InvalidArguments, PLAY_TWICE);
        }
        auto begin = _begin(pImpl->cursor, duration, lag, i);
        if (begin < entry->born) return pImpl->fail(Result::InsufficientCondition, TIMELINE_UNBORN);
        auto channels = _channels(target.properties, target.target->type());
        // A change of the transform may begin while another clip moves the target; only a shift
        // adds to it without an inverse of the model it starts from.
        if ((channels & CHANNEL_MODEL) && begin < _channelEnd(*entry, CHANNEL_MODEL)) {
            Mat4 change;
            Mat4 inverse;
            if (!_shifts(target.target->model, target.targetModel, change) && !_inverse(target.target->model, inverse)) {
                return impl.fail(Result::InsufficientCondition, TIMELINE_FLAT);
            }
            channels &= ~CHANNEL_MODEL;
        }
        if (begin < _channelEnd(*entry, channels)) return _overlap(impl, *entry, channels, begin);
    }
    for (auto i = 0u; i < count; i++) {
        auto& target = targets[i];
        if (!(target.properties & TARGET_GEOMETRY) || target.target->type() != Type::Plot) continue;
        auto plot = static_cast<Plot*>(target.target);
        auto shape = Impl::shaping(target, GeometryShape{plot->points(), plot->objEnds, plot->count(), plot->contours(), plot->objSamples, plot->objGrid});
        auto result = plot->reserveLabels(shape.points, shape.count, shape.ends, shape.pieces);
        if (result != Result::Success) return impl.fail(result, "could not reserve contour labels");
    }
    // A group's stroke, fill and width animate the shapes that take them from it.
    auto passing = [targets](uint32_t i) -> Passing {
        if (targets[i].target->type() != Type::Group) return {};
        return {targets[i].target, targets[i].properties & TARGET_INHERITED};
    };
    auto named = [targets](uint32_t i, const Object* object) -> uint32_t {
        return targets[i].target == object ? targets[i].properties : 0u;
    };
    Inheritance inheritance;
    auto inherited = _inherit(impl, count, duration, lag, passing, named, inheritance);
    if (inherited != Result::Success) return inherited;
    if (!pImpl->growClips(count + inheritance.count)) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    // Geometry changes keep copies of their points before any clip points at them.
    const GeometryShape** shaped = nullptr;
    auto reshapes = 0u;
    for (auto i = 0u; i < count; i++) {
        if (targets[i].properties & TARGET_GEOMETRY) reshapes++;
    }
    auto previousGeometryCount = pImpl->geometryCnt;
    auto previousGeometryPoints = pImpl->geometryPoints;
    if (reshapes) {
        shaped = new (std::nothrow) const GeometryShape*[count]();
        if (!shaped) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
        auto result = impl.reshape(targets, count, shaped, previousGeometryCount, previousGeometryPoints);
        if (result != Result::Success) {
            delete[] shaped;
            return result;
        }
    }
    // Texts keep the strings of their changes before any clip points at them.
    struct Kept
    {
        const char* text = nullptr;
        bool added = false;
    };
    auto changes = 0u;
    for (auto i = 0u; i < count; i++) {
        if (targets[i].properties & TARGET_TEXT) changes++;
    }
    auto kept = changes ? new (std::nothrow) Kept[count] : nullptr;
    auto stored = !changes || kept;
    for (auto i = 0u; kept && stored && i < count; i++) {
        auto& target = targets[i];
        if (!(target.properties & TARGET_TEXT)) continue;
        kept[i].text = Impl::keep(static_cast<Text*>(target.target), target.targetText, kept[i].added);
        if (kept[i].text) continue;
        for (auto j = 0u; j < i; j++) {
            if (kept[j].added) Impl::drop(static_cast<Text*>(targets[j].target));
        }
        stored = false;
    }
    if (!stored) {
        delete[] kept;
        delete[] shaped;
        pImpl->dropGeometries(previousGeometryCount, previousGeometryPoints);
        return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    }

    for (auto i = 0u; i < count; i++) {
        auto& target = targets[i];
        auto entry = pImpl->entry(target.target);
        auto from = _state(target.target);
        if (target.properties & TARGET_GEOMETRY) _reshaped(from, entry->geometry);
        auto to = Impl::targeted(target, from, kept ? kept[i].text : nullptr, shaped ? shaped[i] : nullptr);
        auto begin = _begin(pImpl->cursor, duration, lag, i);
        // A layered change holds only itself; a layered shift leaves the authored model shifted.
        Mat4 change;
        auto layered = (target.properties & TARGET_MODEL) && begin < _channelEnd(*entry, CHANNEL_MODEL);
        if (layered && _shifts(from.model, to.model, change)) to.model = change * from.model;

        auto& clip = pImpl->clips[pImpl->clipCnt++];
        clip = Clip{};
        if ((target.properties & TARGET_MODEL) && target.targetArc != 0.0f) {
            clip.motion = ClipMotion::Arc;
            clip.arc = target.targetArc;
        }
        clip.object = target.target;
        pImpl->link(pImpl->clipCnt - 1u);
        clip.from = from;
        clip.to = to;
        if (layered) _layered(clip, target.target, from.model, target.targetModel);
        clip.begin = begin;
        clip.end = _end(pImpl->cursor, duration, lag, i);
        clip.curve = curve;
        clip.channels = _channels(target.properties, target.target->type());
        _channelEnd(*entry, clip.channels, clip.end);
        Impl::store(target.target, to);
        if (target.properties & TARGET_GEOMETRY) entry->geometry = to.geometry;
        entry->fingerprint = fingerprint(target.target);
        // The object now sets these itself; below a group, shapes added later inherit them.
        if (target.properties & TARGET_STROKE) target.target->objStrokeAuthored = true;
        if (target.properties & TARGET_FILL) target.target->objFillAuthored = true;
        if (target.properties & TARGET_WIDTH) target.target->objWidthAuthored = true;
        if (target.properties & TARGET_EFFECTS) target.target->objEffects = true;
    }
    delete[] kept;
    delete[] shaped;
    _inheritClips(impl, inheritance, duration, lag, curve, [targets](uint32_t i, uint32_t properties, VisualState& to) {
        if (properties & TARGET_STROKE) to.stroke = targets[i].targetStroke;
        if (properties & TARGET_FILL) to.fill = targets[i].targetFill;
        if (properties & TARGET_WIDTH) to.width = targets[i].targetWidth;
    });
    pImpl->advance(nextCursor);
    pImpl->definitionsSealed = true;
    return Result::Success;
}

// Why the objects below object cannot move their starting layers by moved, or nullptr: each must
// still be in the state it was added with, and its layer must stay a 32-bit integer.
template<typename Impl>
static const char* _carried(Impl& impl, const Object* object, int64_t moved)
{
    for (auto i = 0u; i < object->childCount(); i++) {
        auto child = object->childAt(i);
        auto entry = impl.entry(child);
        if (entry && entry->fingerprint != fingerprint(child)) return TARGET_BELOW_CHANGED;
        auto layer = static_cast<int64_t>(child->layer) + moved;
        if (layer < INT32_MIN || layer > INT32_MAX) return VALUE_LAYER;
        if (auto reason = _carried(impl, child, moved)) return reason;
    }
    return nullptr;
}

// Moves the starting layer of every object below object by moved, which _carried() allowed.
template<typename Impl>
static void _carry(Impl& impl, Object* object, int64_t moved)
{
    for (auto i = 0u; i < object->childCount(); i++) {
        auto child = object->childAt(i);
        child->layer = static_cast<int32_t>(child->layer + moved);
        if (auto entry = impl.entry(child)) {
            entry->base.layer = child->layer;
            entry->fingerprint = fingerprint(child);
        }
        _carry(impl, child, moved);
    }
}

// ---------- Updating objects ----------

Result Scene::update(const AnimationTarget& target) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    auto object = target.target;
    auto entry = object ? impl.entry(object) : nullptr;
    if (!entry || object->objOwner != this) return impl.fail(Result::InvalidArguments, TARGET_FOREIGN);
    if (entry->dead >= 0.0f) return impl.fail(Result::InvalidArguments, TARGET_REMOVED);
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (auto reason = Impl::unanimatable(target)) return impl.fail(Result::InvalidArguments, reason);
    if (_warpSized(object, target.properties)) return impl.fail(Result::InvalidArguments, WARP_SIZE);
    if ((target.properties & TARGET_GEOMETRY) && warped(object)) {
        return impl.fail(Result::InvalidArguments, WARP_GEOMETRY);
    }
    if (target.targetArc != 0.0f) return impl.fail(Result::InvalidArguments, LAYOUT_ARC);
    if ((target.properties & TARGET_GEOMETRY) && object->type() == Type::Plot) {
        auto plot = static_cast<Plot*>(object);
        auto shape = Impl::shaping(target, GeometryShape{plot->points(), plot->objEnds, plot->count(), plot->contours(), plot->objSamples, plot->objGrid});
        auto result = plot->reserveLabels(shape.points, shape.count, shape.ends, shape.pieces);
        if (result != Result::Success) return impl.fail(result, "could not reserve contour labels");
        entry = impl.entry(object);
    }
    if (!impl.definitionsSealed) {
        // Before the timeline starts, the values become the object's starting state; a new
        // string replaces the text's own, as Text::text() does, since no text change has
        // kept it yet. The objects below take a new layer along in their starting states.
        auto moved = static_cast<int64_t>(target.targetLayer) - object->layer;
        if ((target.properties & TARGET_LAYER) && moved) {
            if (auto reason = _carried(impl, object, moved)) return impl.fail(Result::InvalidArguments, reason);
        }
        char* copy = nullptr;
        if ((target.properties & TARGET_TEXT) && !(copy = _duplicate(target.targetText))) {
            return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
        }
        // The new points become the object's own; its old ones, and the old pieces of a plot, come
        // back if the update fails.
        GeometryShape old;
        GeometryShape shape;
        Vec3* points = nullptr;
        uint32_t* ends = nullptr;
        Vec3* samplePoints = nullptr;
        if (target.properties & TARGET_GEOMETRY) {
            auto count = shapeCount(object);
            points = count ? new (std::nothrow) Vec3[count] : nullptr;
            if (!count || points) old = Impl::own(object, points);
            ends = old.pieces > 1u ? new (std::nothrow) uint32_t[old.pieces] : nullptr;
            if (ends) {
                std::memcpy(ends, old.ends, sizeof(uint32_t) * old.pieces);
                old.ends = ends;
            }
            if (old.samples.count) {
                samplePoints = new (std::nothrow) Vec3[old.samples.count];
                if (samplePoints) {
                    std::memcpy(samplePoints, old.samples.points, sizeof(Vec3) * old.samples.count);
                    old.samples.points = samplePoints;
                }
            }
            shape = Impl::shaping(target, old);
            if ((count && !points) || (old.pieces > 1u && !ends)
                || (old.samples.count && !samplePoints) || !Impl::room(object, shape)) {
                delete[] points;
                delete[] ends;
                delete[] samplePoints;
                delete[] copy;
                return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
            }
        }
        auto before = _state(object);
        auto reshaping = (target.properties & TARGET_GEOMETRY) != 0u;
        if (reshaping) _reshaped(before, &old);
        Impl::store(object, Impl::targeted(target, before, nullptr, reshaping ? &shape : nullptr));
        auto text = static_cast<Text*>(object);
        auto previous = copy ? text->objText : nullptr;
        if (copy) text->objText = copy;
        auto result = update(object);
        if (result != Result::Success) {
            if (copy) text->objText = previous;
            delete[] copy;
            Impl::store(object, before);
            delete[] points;
            delete[] ends;
            delete[] samplePoints;
            return result;
        }
        delete[] previous;
        delete[] points;
        delete[] ends;
        delete[] samplePoints;
        if ((target.properties & TARGET_LAYER) && moved) _carry(impl, object, moved);
    } else {
        // Afterwards a clip of zero length at the cursor switches the values, for the object and
        // for the shapes that take a group's stroke, fill or width from it.
        if (auto reason = _unplayable(entry, object)) return impl.fail(Result::InvalidArguments, reason);
        auto time = impl.cursor;
        if (time < entry->born) return impl.fail(Result::InsufficientCondition, TIMELINE_UNBORN);
        auto channels = _channels(target.properties, object->type());
        // A change of the transform may land while another clip moves the object, and then adds
        // to that motion; only a shift does so without an inverse of the model it starts from.
        Mat4 change;
        auto layered = (channels & CHANNEL_MODEL) && time < _channelEnd(*entry, CHANNEL_MODEL);
        auto shifts = layered && _shifts(object->model, target.targetModel, change);
        Mat4 inverse;
        if (layered && !shifts && !_inverse(object->model, inverse)) {
            return impl.fail(Result::InsufficientCondition, TIMELINE_FLAT);
        }
        auto waits = layered ? channels & ~CHANNEL_MODEL : channels;
        if (time < _channelEnd(*entry, waits)) return _overlap(impl, *entry, waits, time);
        auto passing = [&target](uint32_t) -> Passing {
            if (target.target->type() != Type::Group) return {};
            return {target.target, target.properties & TARGET_INHERITED};
        };
        auto named = [&target](uint32_t, const Object* candidate) -> uint32_t {
            return target.target == candidate ? target.properties : 0u;
        };
        Inheritance inheritance;
        auto inherited = _inherit(impl, 1u, 0.0f, 0.0f, passing, named, inheritance);
        if (inherited != Result::Success) return inherited;
        if (!impl.growClips(1u + inheritance.count)) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
        const GeometryShape* shaped = nullptr;
        auto previousGeometryCount = impl.geometryCnt;
        auto previousGeometryPoints = impl.geometryPoints;
        if (target.properties & TARGET_GEOMETRY) {
            auto reshaped = impl.reshape(&target, 1u, &shaped, previousGeometryCount, previousGeometryPoints);
            if (reshaped != Result::Success) return reshaped;
        }
        const char* kept = nullptr;
        auto added = false;
        if ((target.properties & TARGET_TEXT)
            && !(kept = Impl::keep(static_cast<Text*>(object), target.targetText, added))) {
            impl.dropGeometries(previousGeometryCount, previousGeometryPoints);
            return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
        }
        auto& clip = impl.clips[impl.clipCnt++];
        clip = Clip{};
        clip.object = object;
        impl.link(impl.clipCnt - 1u);
        clip.from = _state(object);
        if (shaped) _reshaped(clip.from, entry->geometry);
        clip.to = Impl::targeted(target, clip.from, kept, shaped);
        if (shifts) clip.to.model = change * clip.from.model;
        clip.begin = time;
        clip.end = time;
        clip.channels = channels;
        _channelEnd(*entry, channels, time);
        auto state = clip.to;
        auto authored = clip.from.model;
        if (layered) _layered(clip, object, authored, target.targetModel);
        Impl::store(object, state);
        if (shaped) entry->geometry = shaped;
        entry->fingerprint = fingerprint(object);
        _inheritClips(impl, inheritance, 0.0f, 0.0f, clip.curve, [&target](uint32_t, uint32_t properties, VisualState& to) {
            if (properties & TARGET_STROKE) to.stroke = target.targetStroke;
            if (properties & TARGET_FILL) to.fill = target.targetFill;
            if (properties & TARGET_WIDTH) to.width = target.targetWidth;
        });
    }
    // The object now sets these itself; below a group, shapes added later inherit them.
    if (target.properties & TARGET_STROKE) object->objStrokeAuthored = true;
    if (target.properties & TARGET_FILL) object->objFillAuthored = true;
    if (target.properties & TARGET_WIDTH) object->objWidthAuthored = true;
    if (target.properties & TARGET_EFFECTS) object->objEffects = true;
    return Result::Success;
}

// ---------- Saving and restoring ----------

// How many objects object holds at any depth, itself included.
static uint32_t _family(const Object* object)
{
    auto count = 1u;
    for (auto i = 0u; i < object->childCount(); i++)
        count += _family(object->childAt(i));
    return count;
}

// Records the state of save's target at the cursor, then those of the objects below it that show
// then, each after its parent, into records with room for all of them. Returns why it cannot,
// or nullptr.
template<typename Impl>
static const char* _record(Impl& impl, SavedState& save)
{
    auto add = [&impl, &save](Object* object, uint32_t parent) -> const char* {
        auto entry = impl.entry(object);
        VisualState state;
        // An object below the target that does not show at the cursor has no state to keep.
        if (!entry || !impl.sample(*entry, impl.cursor, state)) return nullptr;
        if (entry->fingerprint != fingerprint(object)) return TARGET_BELOW_CHANGED;
        auto& record = save.objects[save.count++];
        record.object = object;
        record.parent = parent;
        record.baseLayer = entry->base.layer;
        if (object->type() == Type::Text) {
            record.counting = static_cast<const Text*>(object)->counting();
            // Before the timeline a text may still replace its string, so the save keeps a copy.
            if (state.text && !impl.definitionsSealed) {
                record.text = _duplicate(state.text);
                if (!record.text) return SCENE_MEMORY;
                state.text = record.text;
            }
        }
        // A text or geometry change in progress counts as done, and a morph has no state to come
        // back to.
        state.textFrom = state.text;
        state.textProgress = 1.0f;
        state.geometryFrom = state.geometry;
        state.geometryProgress = 1.0f;
        state.morph = nullptr;
        state.morphShape = nullptr;
        state.morphProgress = 0.0f;
        record.state = state;
        return nullptr;
    };
    if (auto reason = add(save.target, UINT32_MAX)) return reason;
    for (auto i = 0u; i < save.count; i++) {
        auto object = save.objects[i].object;
        for (auto j = 0u; j < object->childCount(); j++) {
            if (auto reason = add(object->childAt(j), i)) return reason;
        }
    }
    return nullptr;
}

Result Scene::save(Object* target) noexcept
{
    return save(&target, 1u);
}

Result Scene::save(Object* const* targets, uint32_t count) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (!targets || !count) return impl.fail(Result::InvalidArguments, SAVE_EMPTY);
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (count > MAX_ANIMATION_GROUP) return impl.fail(Result::InvalidArguments, SAVE_GROUP);
    auto total = 0u;
    for (auto i = 0u; i < count; i++) {
        auto target = targets[i];
        auto entry = target ? impl.entry(target) : nullptr;
        if (auto reason = _unplayable(entry, target)) return impl.fail(Result::InvalidArguments, reason);
        for (auto j = i + 1u; j < count; j++) {
            if (target == targets[j]) return impl.fail(Result::InvalidArguments, PLAY_TWICE);
        }
        if (impl.cursor < entry->born) return impl.fail(Result::InsufficientCondition, TIMELINE_UNBORN);
        auto family = _family(target);
        if (family > MAX_ANIMATION_GROUP - total) return impl.fail(Result::InvalidArguments, SAVE_GROUP);
        total += family;
    }
    if (!grow(impl.saves, impl.saveCnt, impl.saveCap, count, 8u)) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    // Every target is recorded before any joins the stacks, so a failure leaves them as they were.
    auto first = impl.saveCnt;
    for (auto i = 0u; i < count; i++) {
        auto& save = impl.saves[first + i];
        save.target = targets[i];
        save.objects = new (std::nothrow) SavedObject[_family(targets[i])];
        auto reason = save.objects ? _record(impl, save) : SCENE_MEMORY;
        if (!reason) {
            for (auto r = 0u; r < save.count; r++) {
                auto& record = save.objects[r];
                record.strokeAuthored = record.object->objStrokeAuthored;
                record.fillAuthored = record.object->objFillAuthored;
                record.widthAuthored = record.object->objWidthAuthored;
            }
            continue;
        }
        for (auto j = 0u; j <= i; j++)
            _release(impl.saves[first + j]);
        return impl.fail(reason == SCENE_MEMORY ? Result::OutOfMemory : Result::InvalidArguments, reason);
    }
    impl.saveCnt += count;
    return Result::Success;
}

// Whether two models place an object alike, up to rounding.
static bool _near(const Mat4& first, const Mat4& second)
{
    auto reach = 1.0f;
    for (auto i = 0u; i < 12u; i++) reach = std::fmax(reach, std::fmax(std::fabs(first.e[i]), std::fabs(second.e[i])));
    for (auto i = 0u; i < 16u; i++) {
        if (std::fabs(first.e[i] - second.e[i]) > 1.0e-5f * reach) return false;
    }
    return true;
}

// The channels in which a saved state differs from rest, the state its object comes to once the
// clips authored so far end. Strings and layers are left to the caller.
static uint32_t _changed(const VisualState& saved, const VisualState& rest)
{
    auto channels = 0u;
    if (!_near(saved.model, rest.model)) channels |= CHANNEL_MODEL;
    if (saved.opacity != rest.opacity) channels |= CHANNEL_OPACITY;
    if (!_equal(saved.stroke, rest.stroke)) channels |= CHANNEL_STROKE;
    if (!_equal(saved.fill, rest.fill)) channels |= CHANNEL_FILL;
    if (saved.dashOffset != rest.dashOffset) channels |= CHANNEL_DASH_OFFSET;
    if (saved.markerTail != rest.markerTail) channels |= CHANNEL_TAIL;
    if (saved.markerTip != rest.markerTip) channels |= CHANNEL_TIP;
    // The way a drawing goes shows only while it is partly drawn.
    if (saved.progress != rest.progress
        || (saved.progress < 1.0f && (saved.direction != rest.direction || saved.traceFill != rest.traceFill))) {
        channels |= CHANNEL_PROGRESS;
    }
    if (saved.fillProgress != rest.fillProgress) channels |= CHANNEL_FILL_PROGRESS;
    if (saved.pixelScale != rest.pixelScale) channels |= CHANNEL_PIXEL_SCALE;
    if (saved.width != rest.width) channels |= CHANNEL_WIDTH;
    if (saved.arcStart != rest.arcStart) channels |= CHANNEL_ARC_START;
    if (saved.arcAngle != rest.arcAngle) channels |= CHANNEL_ARC_ANGLE;
    if (saved.radius != rest.radius) channels |= CHANNEL_RADIUS;
    if (!(saved.gradient == rest.gradient) || !_equal(saved.gradientEnd, rest.gradientEnd)) channels |= CHANNEL_GRADIENT;
    if (saved.size.x != rest.size.x || saved.size.y != rest.size.y) channels |= CHANNEL_SIZE;
    if (saved.corner != rest.corner) channels |= CHANNEL_CORNER;
    if (saved.trimDistance.x != rest.trimDistance.x || saved.trimDistance.y != rest.trimDistance.y) channels |= CHANNEL_TRIM_DISTANCE;
    if (saved.blur != rest.blur) channels |= CHANNEL_BLUR;
    if (!_equal(saved.shadow.color, rest.shadow.color) || saved.shadow.offset.x != rest.shadow.offset.x
        || saved.shadow.offset.y != rest.shadow.offset.y || saved.shadow.blur != rest.shadow.blur) {
        channels |= CHANNEL_SHADOW;
    }
    if (saved.value != rest.value) channels |= CHANNEL_VALUE;
    return channels;
}

// A change restore() makes to one object, planned before any is made.
struct Restoring
{
    Object* object = nullptr;
    ObjectEntry* entry = nullptr;
    // The saved string: one the text keeps already, or the copy a save before the timeline made,
    // which the text then keeps a copy of.
    const char* text = nullptr;
    bool copied = false;
    VisualState from;
    VisualState to;
    // A layered change of place.
    Mat4 change;
    float begin = 0.0f;
    float end = 0.0f;
    uint32_t channels = 0u;
    bool layered = false;
    bool added = false;
};

Result Scene::restore(Object* target, float duration, const AnimCurve& curve) noexcept
{
    return restore(&target, 1u, duration, curve, 0.0f);
}

Result Scene::restore(Object* const* targets, uint32_t count, float duration, const AnimCurve& curve,
                      float lag) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (!targets || !count) return impl.fail(Result::InvalidArguments, RESTORE_EMPTY);
    if (!std::isfinite(duration) || duration < 0.0f) return impl.fail(Result::InvalidArguments, RESTORE_DURATION);
    if (!std::isfinite(lag) || lag < 0.0f) return impl.fail(Result::InvalidArguments, PLAY_LAG);
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (count > MAX_ANIMATION_GROUP) return impl.fail(Result::InvalidArguments, RESTORE_GROUP);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);
    auto nextCursor = impl.cursor;
    if (duration > 0.0f && !_timeline(impl.cursor, duration, lag, count, nextCursor)) {
        return impl.fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    auto begin = [&impl, duration, lag](uint32_t i) { return _begin(impl.cursor, duration, lag, i); };
    auto end = [&impl, duration, lag](uint32_t i) { return _end(impl.cursor, duration, lag, i); };
    // The newest save of each target, and how many objects they hold in all.
    auto taken = new (std::nothrow) uint32_t[count];
    if (!taken) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    auto total = 0u;
    for (auto i = 0u; i < count; i++) {
        auto target = targets[i];
        auto entry = target ? impl.entry(target) : nullptr;
        auto reason = _unplayable(entry, target);
        for (auto j = i + 1u; !reason && j < count; j++) {
            if (target == targets[j]) reason = PLAY_TWICE;
        }
        if (!reason && begin(i) < entry->born) reason = TIMELINE_UNBORN;
        auto index = impl.saveCnt;
        while (!reason && index && impl.saves[index - 1u].target != target) index--;
        if (!reason && !index) {
            auto tag = target->tag();
            auto named = tag && tag[0];
            std::snprintf(impl.reason, sizeof(impl.reason), "%s%.48s%s has no saved state; save it before restoring",
                          named ? "'" : "the object", named ? tag : "", named ? "'" : "");
            reason = impl.reason;
        }
        if (!reason && impl.saves[index - 1u].count > MAX_ANIMATION_GROUP - total) reason = RESTORE_GROUP;
        if (reason) {
            delete[] taken;
            return impl.fail(reason == TIMELINE_UNBORN || reason == impl.reason ? Result::InsufficientCondition
                                                                                 : Result::InvalidArguments,
                             reason);
        }
        taken[i] = index - 1u;
        total += impl.saves[taken[i]].count;
    }

    auto plan = new (std::nothrow) Restoring[total];
    if (!plan) {
        delete[] taken;
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    auto failed = [&impl, taken, plan](Result result, const char* reason) {
        delete[] taken;
        delete[] plan;
        return impl.fail(result, reason);
    };
    auto planned = 0u;
    for (auto i = 0u; i < count; i++) {
        auto& save = impl.saves[taken[i]];
        auto time = begin(i);
        for (auto r = 0u; r < save.count; r++) {
            auto& record = save.objects[r];
            auto object = record.object;
            auto entry = impl.entry(object);
            // Objects removed since the save, or added only after this time, stay as they are.
            if (!entry || entry->dead >= 0.0f || time < entry->born) continue;
            if (!valid(object)) return failed(Result::InvalidArguments, TARGET_INVALID);
            if (entry->fingerprint != fingerprint(object)) {
                return failed(Result::InvalidArguments, r ? TARGET_BELOW_CHANGED : TARGET_CHANGED);
            }
            VisualState rest;
            if (!impl.sample(*entry, std::numeric_limits<float>::max(), rest)) continue;
            auto& saved = record.state;
            auto channels = _changed(saved, rest);
            // A text that turned between counting and a plain string since the save shows what
            // it shows now.
            auto text = object->type() == Type::Text ? static_cast<const Text*>(object) : nullptr;
            if (!text || !text->counting() || !record.counting) channels &= ~CHANNEL_VALUE;
            if (text && !text->counting() && !record.counting && saved.text && rest.text
                && std::strcmp(saved.text, rest.text) != 0) {
                channels |= CHANNEL_TEXT;
            }
            // The geometry travels back once the object changed it since the save; its first
            // change gave the save the geometry it started with (see reshapable()). A warp
            // leaves its images, as for the other shapes. A plot takes back its pieces too, and
            // makes room for them first.
            if (!warped(object) && saved.geometry && rest.geometry && saved.geometry != rest.geometry
                && !_same(*saved.geometry, *rest.geometry)) {
                if (!Impl::room(object, *saved.geometry)) return failed(Result::OutOfMemory, SCENE_MEMORY);
                channels |= CHANNEL_GEOMETRY;
            }
            // An object below the target keeps its place among the objects beside it: its own
            // layer moves by as much as its parent's starting layer moved since the save.
            auto layer = static_cast<int64_t>(saved.layer);
            if (record.parent != UINT32_MAX) {
                auto& parent = save.objects[record.parent];
                if (auto above = impl.entry(parent.object)) layer += static_cast<int64_t>(above->base.layer) - parent.baseLayer;
            }
            if (layer < INT32_MIN || layer > INT32_MAX) return failed(Result::InvalidArguments, VALUE_LAYER);
            if (layer != rest.layer) channels |= CHANNEL_LAYER;
            if (!channels) continue;
            if ((channels & CHANNEL_MODEL) && !_transformable(object)) return failed(Result::InvalidArguments, TARGET_FIXED);
            for (auto k = 0u; k < planned; k++) {
                if (plan[k].object == object) return failed(Result::InvalidArguments, PLAY_TWICE);
            }
            // A change of place alone may add to a motion still running; every other value waits.
            Mat4 change;
            auto layered = (channels & CHANNEL_MODEL) && time < _channelEnd(*entry, CHANNEL_MODEL)
                           && _shifts(rest.model, saved.model, change);
            auto waits = layered ? channels & ~CHANNEL_MODEL : channels;
            if (time < _channelEnd(*entry, waits)) {
                delete[] taken;
                delete[] plan;
                return _overlap(impl, *entry, waits, time);
            }
            auto& item = plan[planned++];
            item.object = object;
            item.entry = entry;
            item.text = saved.text;
            item.copied = record.text != nullptr;
            item.from = rest;
            item.to = rest;
            _apply(item.to, saved, channels & ~(CHANNEL_TEXT | CHANNEL_LAYER));
            item.to.layer = static_cast<int32_t>(layer);
            item.change = change;
            item.begin = time;
            item.end = end(i);
            item.channels = channels;
            item.layered = layered;
        }
    }
    if (planned && !impl.growClips(planned)) return failed(Result::OutOfMemory, SCENE_MEMORY);
    // Texts keep the strings saved before the timeline before any clip points at them; a string
    // saved later is one the text keeps until it is gone.
    for (auto k = 0u; k < planned; k++) {
        auto& item = plan[k];
        if (!(item.channels & CHANNEL_TEXT) || !item.copied) continue;
        item.text = Impl::keep(static_cast<Text*>(item.object), item.text, item.added);
        if (item.text) continue;
        while (k--) {
            if (plan[k].added) Impl::drop(static_cast<Text*>(plan[k].object));
        }
        return failed(Result::OutOfMemory, SCENE_MEMORY);
    }

    for (auto k = 0u; k < planned; k++) {
        auto& item = plan[k];
        auto& clip = impl.clips[impl.clipCnt++];
        clip = Clip{};
        clip.object = item.object;
        impl.link(impl.clipCnt - 1u);
        clip.from = item.from;
        clip.to = item.to;
        if (item.channels & CHANNEL_TEXT) {
            clip.to.text = item.text;
            clip.to.textFrom = item.text;
            clip.to.textProgress = 1.0f;
            clip.to.textChange = TextChange::Sweep;
        }
        clip.begin = item.begin;
        clip.end = item.end;
        clip.curve = curve;
        clip.channels = item.channels;
        // The saved values become the object's latest authored state.
        auto authored = _state(item.object);
        _apply(authored, clip.to, item.channels);
        Impl::store(item.object, authored);
        if (item.channels & CHANNEL_GEOMETRY) item.entry->geometry = clip.to.geometry;
        // A layered change of place holds only the shift, which adds to the running motion.
        if (item.layered) {
            clip.layered = true;
            clip.from.model = Mat4::identity();
            clip.to.model = item.change;
        }
        _channelEnd(*item.entry, item.channels, item.end);
        if (item.channels & (CHANNEL_BLUR | CHANNEL_SHADOW)) item.object->objEffects = true;
    }
    // The objects set again the styles they set themselves when saved, which decide what the
    // shapes below a group take from it; then the saves leave their stacks.
    for (auto i = 0u; i < count; i++) {
        auto& save = impl.saves[taken[i]];
        auto time = begin(i);
        for (auto r = 0u; r < save.count; r++) {
            auto& record = save.objects[r];
            auto entry = impl.entry(record.object);
            if (!entry || entry->dead >= 0.0f || time < entry->born) continue;
            record.object->objStrokeAuthored = record.strokeAuthored;
            record.object->objFillAuthored = record.fillAuthored;
            record.object->objWidthAuthored = record.widthAuthored;
            entry->fingerprint = fingerprint(record.object);
        }
        save.target = nullptr;
    }
    auto kept = 0u;
    for (auto i = 0u; i < impl.saveCnt; i++) {
        if (impl.saves[i].target) impl.saves[kept++] = impl.saves[i];
        else _release(impl.saves[i]);
    }
    for (auto i = kept; i < impl.saveCnt; i++)
        impl.saves[i] = {};
    impl.saveCnt = kept;
    delete[] taken;
    delete[] plan;
    impl.advance(nextCursor);
    impl.definitionsSealed = true;
    return Result::Success;
}

// ---------- Warps ----------

// An object a warp moves, with the matrix from its local space to the parent space of its target.
struct WarpItem
{
    Object* object = nullptr;
    Mat4 space;
    Mat4 inverse;
    uint32_t channels = 0;
    WarpShape* shape = nullptr;
};

Result Scene::warp(Object* const* targets, uint32_t count, PointMap map, void* data, float duration,
                   const AnimCurve& curve, float step) noexcept
{
    pImpl->diagnostic = nullptr;
    auto& impl = *pImpl;
    if (impl.owner) return impl.fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (!targets || !count) return impl.fail(Result::InvalidArguments, PLAY_EMPTY);
    if (count > MAX_ANIMATION_GROUP) return impl.fail(Result::InvalidArguments, PLAY_GROUP);
    if (!map) return impl.fail(Result::InvalidArguments, WARP_MAP);
    if (!std::isfinite(duration) || duration <= 0.0f) return impl.fail(Result::InvalidArguments, PLAY_DURATION);
    if (!curve.valid()) return impl.fail(Result::InvalidArguments, PLAY_CURVE);
    if (!std::isfinite(step) || step <= 0.0f) return impl.fail(Result::InvalidArguments, WARP_STEP);
    float end;
    if (!_timeline(impl.cursor, duration, 0.0f, 1u, end)) return impl.fail(Result::InvalidArguments, TIMELINE_RANGE);
    auto begin = impl.cursor;
    auto total = 0u;
    for (auto i = 0u; i < count; i++) {
        auto target = targets[i];
        if (!target) return impl.fail(Result::InvalidArguments, TARGET_FOREIGN);
        auto entry = impl.entry(target);
        if (auto reason = _unplayable(entry, target)) return impl.fail(Result::InvalidArguments, reason);
        if (begin < entry->born) return impl.fail(Result::InsufficientCondition, TIMELINE_UNBORN);
        for (auto j = 0u; j < i; j++) {
            if (_inside(target, targets[j]) || _inside(targets[j], target)) {
                return impl.fail(Result::InvalidArguments, WARP_TWICE);
            }
        }
        total += _family(target);
    }

    // Every object at any depth below the targets that has geometry of its own: groups hold none,
    // a connector follows its ends, and removed objects draw no more.
    auto items = new (std::nothrow) WarpItem[total];
    if (!items) return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    auto release = [&items, &total]() {
        for (auto i = 0u; i < total; i++)
            delete items[i].shape;
        delete[] items;
    };
    auto used = 0u;
    auto gather = [&](auto& self, Object* object, const Mat4& above) -> Result {
        auto entry = impl.entry(object);
        if (!entry) return impl.fail(Result::InvalidArguments, TARGET_FOREIGN);
        if (entry->dead >= 0.0f) return Result::Success;
        if (entry->fingerprint != fingerprint(object)) return impl.fail(Result::InvalidArguments, TARGET_BELOW_CHANGED);
        if (!valid(object)) return impl.fail(Result::InvalidArguments, TARGET_INVALID);
        if (begin < entry->born) return impl.fail(Result::InsufficientCondition, WARP_UNBORN);
        auto type = object->type();
        if (type == Type::Cell || type == Type::SurfaceMesh) return impl.fail(Result::InvalidArguments, WARP_KIND);
        auto space = above * object->model;
        if (type != Type::Group && type != Type::Connector) {
            auto& item = items[used++];
            item.object = object;
            item.space = space;
            // A circle's or rectangle's own size no longer shows once it is warped, and the warp
            // maps the points the geometry clips of a shape end at.
            item.channels = CHANNEL_MORPH;
            if (type == Type::Circle || type == Type::Rectangle) item.channels |= CHANNEL_SIZE | CHANNEL_CORNER;
            if (type == Type::Arc || type == Type::Angle) item.channels |= CHANNEL_SIZE | CHANNEL_ARC_START | CHANNEL_ARC_ANGLE;
            if (type == Type::Line || type == Type::Arrow || type == Type::Vector) item.channels |= CHANNEL_TRIM_DISTANCE;
            if (geometric(object)) item.channels |= CHANNEL_GEOMETRY;
            if (begin < _channelEnd(*entry, item.channels)) return _overlap(impl, *entry, item.channels, begin);
            if (!_inverse(space, item.inverse)) return impl.fail(Result::InsufficientCondition, WARP_SPACE);
        }
        for (auto i = 0u; i < object->childCount(); i++) {
            auto result = self(self, object->childAt(i), space);
            if (result != Result::Success) return result;
        }
        return Result::Success;
    };
    for (auto i = 0u; i < count; i++) {
        auto result = gather(gather, targets[i], Mat4::identity());
        if (result != Result::Success) {
            release();
            return result;
        }
    }

    // The map may run script code, which must leave the scene as it is.
    auto objects = impl.objectCnt;
    auto clips = impl.clipCnt;
    auto points = 0u;
    for (auto i = 0u; i < used; i++) {
        auto& item = items[i];
        WarpDraft draft;
        if (auto previous = item.object->objWarp) {
            // A warped object goes on from the images of its latest warp.
            draft.copy(*previous);
        } else {
            // Screen labels need the mapped origin as well as their separately warped anchors.
            if (item.object->type() == Type::Space) {
                for (auto j = 0u; j < item.object->childCount(); j++) {
                    auto child = item.object->childAt(j);
                    if (child->type() != Type::Text || static_cast<Text*>(child)->objScreenGap < 0.0f
                        || child->objLabelOf != item.object) continue;
                    draft.add({});
                    draft.end(WarpPart::Origin);
                    break;
                }
            }
            draft.gather(item.object, item.space, step);
        }
        if (draft.failed) {
            release();
            return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
        }
        if (draft.full) {
            release();
            return impl.fail(Result::InsufficientCondition, WARP_LIMIT);
        }
        Vec3 missing;
        auto budget = WarpDraft::PointLimit - impl.warpPoints - points;
        auto result = draft.map(item.space, item.inverse, map, data, step, budget, item.shape, missing);
        if (impl.objectCnt != objects || impl.clipCnt != clips || impl.cursor != begin) {
            release();
            return impl.fail(Result::InsufficientCondition, WARP_CHANGED);
        }
        if (result == Result::InvalidArguments) {
            release();
            std::snprintf(impl.reason, sizeof(impl.reason),
                          "the warp function gives no finite image of the point (%g, %g, %g)",
                          static_cast<double>(missing.x), static_cast<double>(missing.y),
                          static_cast<double>(missing.z));
            return impl.fail(result, impl.reason);
        }
        if (result != Result::Success) {
            release();
            return impl.fail(result, result == Result::InsufficientCondition ? WARP_LIMIT : SCENE_MEMORY);
        }
        points += item.shape->count;
    }
    if (!grow(impl.warps, impl.warpCnt, impl.warpCap, used, 4u) || !impl.growClips(used)) {
        release();
        return impl.fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    for (auto i = 0u; i < used; i++) {
        auto& item = items[i];
        auto entry = impl.entry(item.object);
        auto& clip = impl.clips[impl.clipCnt++];
        clip = Clip{};
        clip.object = item.object;
        clip.from = _state(item.object);
        clip.from.warp = item.object->objWarp;
        clip.from.warpProgress = 1.0f;
        _reshaped(clip.from, entry->geometry);
        clip.to = clip.from;
        clip.to.warp = item.shape;
        clip.begin = begin;
        clip.end = end;
        clip.curve = curve;
        clip.kind = AnimationKind::Transform;
        clip.channels = item.channels;
        impl.link(impl.clipCnt - 1u);
        _channelEnd(*entry, item.channels, end);
        item.object->objWarp = item.shape;
        impl.warps[impl.warpCnt++] = item.shape;
        impl.warpPoints += item.shape->count;
        item.shape = nullptr;
    }
    delete[] items;
    impl.advance(end);
    impl.definitionsSealed = true;
    return Result::Success;
}

// ---------- Runtime modifiers and camera input ----------

Result Scene::runtime(const RuntimeModifier* modifier) noexcept
{
    if (!modifier) {
        for (auto i = 0u; i < pImpl->runtimeModifierCnt; i++) {
            pImpl->runtimeModifiers[i] = {};
        }
        pImpl->runtimeModifierCnt = 0u;
        return Result::Success;
    }
    if (!modifier->object && !modifier->fill && !modifier->camera && !modifier->input) {
        return Result::InvalidArguments;
    }
    if (pImpl->runtimeModifierCnt > 1u
        || (pImpl->runtimeModifierCnt == 1u
            && pImpl->runtimeModifiers[0].data != modifier->data)) {
        return Result::InsufficientCondition;
    }
    pImpl->runtimeModifiers[0] = *modifier;
    pImpl->runtimeModifierCnt = 1u;
    return Result::Success;
}

Result Scene::runtimeAdd(const RuntimeModifier* modifier) noexcept
{
    if (!modifier || !modifier->data
        || (!modifier->object && !modifier->fill && !modifier->camera && !modifier->input)) {
        return Result::InvalidArguments;
    }
    for (auto i = 0u; i < pImpl->runtimeModifierCnt; i++) {
        if (pImpl->runtimeModifiers[i].data != modifier->data) continue;
        if (modifier->key) {
            for (auto j = 0u; j < pImpl->runtimeModifierCnt; j++) {
                if (j != i && pImpl->runtimeModifiers[j].key == modifier->key) {
                    return Result::InsufficientCondition;
                }
            }
        }
        pImpl->runtimeModifiers[i] = *modifier;
        return Result::Success;
    }
    if (modifier->key) {
        for (auto i = 0u; i < pImpl->runtimeModifierCnt; i++) {
            if (pImpl->runtimeModifiers[i].key == modifier->key) {
                return Result::InsufficientCondition;
            }
        }
    }
    if (pImpl->runtimeModifierCnt >= RuntimeLimit) return Result::InsufficientCondition;
    pImpl->runtimeModifiers[pImpl->runtimeModifierCnt++] = *modifier;
    return Result::Success;
}

Result Scene::runtimeRemove(void* data) noexcept
{
    if (!data) return Result::InvalidArguments;
    for (auto i = 0u; i < pImpl->runtimeModifierCnt; i++) {
        if (pImpl->runtimeModifiers[i].data != data) continue;
        for (auto j = i + 1u; j < pImpl->runtimeModifierCnt; j++) {
            pImpl->runtimeModifiers[j - 1u] = pImpl->runtimeModifiers[j];
        }
        pImpl->runtimeModifiers[--pImpl->runtimeModifierCnt] = {};
        return Result::Success;
    }
    return Result::InsufficientCondition;
}

Result Scene::cameraInput(const CameraInput& input) noexcept
{
    if (pImpl->cfg.cameraMode != CameraMode::Interactive) {
        return Result::InsufficientCondition;
    }
    if (static_cast<uint8_t>(input.action) > static_cast<uint8_t>(CameraAction::View3D)
        || !std::isfinite(input.delta.x) || !std::isfinite(input.delta.y)) {
        return Result::InvalidArguments;
    }
    for (auto i = 0u; i < pImpl->runtimeModifierCnt; i++) {
        auto& modifier = pImpl->runtimeModifiers[i];
        if (!modifier.input) continue;
        auto result = modifier.input(input, modifier.data);
        if (result == Result::Success) return result;
        if (result != Result::NonSupport && result != Result::InsufficientCondition) {
            return result;
        }
    }
    return Result::NonSupport;
}

// ---------- Cameras and views ----------

Result Scene::look(const Camera& camera, CameraView view, float duration, Easing easing) noexcept
{
    return look(camera, view, duration, _legacyCurve(easing));
}

Result Scene::look(const Camera& camera, CameraView view, float duration,
                   const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    if (pImpl->cfg.cameraMode != CameraMode::Fixed) return pImpl->fail(Result::InsufficientCondition, CAMERA_MODE);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    return pImpl->look(pImpl->track(0u), camera, view, duration, curve);
}

Result Scene::Impl::look(const CameraTrack& track, const Camera& camera, CameraView view, float duration,
                         const AnimCurve& curve) noexcept
{
    if (!std::isfinite(duration) || duration <= 0.0f) return fail(Result::InvalidArguments, PLAY_DURATION);
    if (!curve.valid()) return fail(Result::InvalidArguments, PLAY_CURVE);
    // A move that begins while another camera move runs adds its change to it (see
    // CameraClip::layered).
    auto busy = _cameraEnd(track.clips, track.count);
    auto layered = cursor < busy;
    auto current = cfg;
    current.camera = track.camera;
    current.cameraView = track.view;
    auto config = cfg;
    config.camera = camera;
    config.cameraView = view;
    auto poseChanged = !_samePose(track.camera, camera);
    float nextCursor;
    if (!valid(current) || !valid(config)
        || (poseChanged && (!_animationSafe(track.camera) || !_animationSafe(camera)
                            || !_poseSafe(track.camera, camera)))
        || !_planesSafe(track.camera, camera)) {
        return fail(Result::InvalidArguments, CAMERA_POSE);
    }
    if (!_timeline(cursor, duration, 0.0f, 1u, nextCursor)) {
        return fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    if (layered && track.view == CameraView::ThreeD) {
        auto& from = track.camera;
        // A 2D camera faces the plane straight on, which a layered look back to 2D reaches only
        // once the moves it adds to have ended.
        if (view == CameraView::TwoD && nextCursor < busy) {
            std::snprintf(reason, sizeof(reason),
                          "the camera already moves until %.2f s; a look that returns to the 2D view ends at "
                          "that time or later",
                          static_cast<double>(busy));
            return fail(Result::InsufficientCondition, reason);
        }
        // Moves stored earlier may still show the 2D view after a layered turn or projection change
        // begins; a look back to 2D shows that view itself from its end.
        if (_turns(from, camera) || (view == CameraView::ThreeD && camera.projection != from.projection)) {
            auto planar = _planarSince(track.clips, track.count, track.initialView, cursor,
                                       view == CameraView::ThreeD ? std::numeric_limits<float>::infinity()
                                                                  : nextCursor);
            if (planar >= 0.0f) return fail(Result::InsufficientCondition, _planarReason(reason, planar));
        }
    }
    if (!grow(track.clips, track.count, track.capacity, 1u, 4u)) return fail(Result::OutOfMemory, SCENE_MEMORY);
    if (!track.count) {
        track.initial = track.camera;
        track.initialView = track.view;
    }
    auto& clip = track.clips[track.count++];
    clip.from = track.camera;
    clip.to = camera;
    clip.fromView = track.view;
    clip.toView = view;
    clip.begin = cursor;
    clip.end = nextCursor;
    clip.curve = curve;
    clip.layered = layered;
    track.camera = camera;
    track.view = view;
    advance(nextCursor);
    definitionsSealed = true;
    return Result::Success;
}

Result Scene::orbit(float angle, const Vec3& axis, float duration, Easing easing) noexcept
{
    return orbit(angle, axis, duration, _legacyCurve(easing));
}

Result Scene::orbit(float angle, const Vec3& axis, float duration, const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    if (pImpl->cfg.cameraMode != CameraMode::Fixed) return pImpl->fail(Result::InsufficientCondition, CAMERA_MODE);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    return pImpl->orbit(pImpl->track(0u), angle, axis, duration, curve);
}

Result Scene::Impl::orbit(const CameraTrack& track, float angle, const Vec3& axis, float duration,
                          const AnimCurve& curve) noexcept
{
    if (track.view != CameraView::ThreeD) return fail(Result::InsufficientCondition, ORBIT_VIEW);
    if (!std::isfinite(angle) || angle == 0.0f) return fail(Result::InvalidArguments, ORBIT_ANGLE);
    Vector64 unit;
    if (!std::isfinite(axis.x) || !std::isfinite(axis.y) || !std::isfinite(axis.z)
        || !_normalize(axis.x, axis.y, axis.z, unit)) {
        return fail(Result::InvalidArguments, ORBIT_AXIS);
    }
    if (!std::isfinite(duration) || duration <= 0.0f) return fail(Result::InvalidArguments, PLAY_DURATION);
    if (!curve.valid()) return fail(Result::InvalidArguments, PLAY_CURVE);
    // An orbit that begins while another camera move runs turns about the target that move sets.
    auto layered = cursor < _cameraEnd(track.clips, track.count);
    // The turn keeps the eye's distance and the up vector's angle to the view direction, so a
    // pose that is safe to animate stays safe at every moment of the orbit.
    auto current = cfg;
    current.camera = track.camera;
    current.cameraView = track.view;
    auto config = current;
    if (!valid(current) || !_animationSafe(track.camera)
        || !_orbit(track.camera, axis, static_cast<double>(angle), config.camera) || !valid(config)
        || !_animationSafe(config.camera)) {
        return fail(Result::InvalidArguments, CAMERA_POSE);
    }
    float nextCursor;
    if (!_timeline(cursor, duration, 0.0f, 1u, nextCursor)) {
        return fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    // Moves stored earlier may still show the 2D view after a layered orbit begins.
    if (layered) {
        auto planar = _planarSince(track.clips, track.count, track.initialView, cursor,
                                   std::numeric_limits<float>::infinity());
        if (planar >= 0.0f) return fail(Result::InsufficientCondition, _planarReason(reason, planar));
    }
    if (!grow(track.clips, track.count, track.capacity, 1u, 4u)) return fail(Result::OutOfMemory, SCENE_MEMORY);
    if (!track.count) {
        track.initial = track.camera;
        track.initialView = track.view;
    }
    auto& clip = track.clips[track.count++];
    clip.from = track.camera;
    clip.to = config.camera;
    clip.fromView = track.view;
    clip.toView = track.view;
    clip.begin = cursor;
    clip.end = nextCursor;
    clip.curve = curve;
    clip.axis = axis;
    clip.orbit = angle;
    clip.layered = layered;
    track.camera = config.camera;
    advance(nextCursor);
    definitionsSealed = true;
    return Result::Success;
}

Result Scene::view(const SceneView& view, uint32_t* index) noexcept
{
    pImpl->diagnostic = nullptr;
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (!_valid(view.viewport)) return pImpl->fail(Result::InvalidArguments, VIEW_RECT);
    auto config = pImpl->cfg;
    config.camera = view.camera;
    config.cameraView = view.cameraView;
    if (!valid(config)) return pImpl->fail(Result::InvalidArguments, CAMERA_POSE);
    if (!std::isfinite(view.opacity) || view.opacity < 0.0f || view.opacity > 1.0f) {
        return pImpl->fail(Result::InvalidArguments, VIEW_OPACITY);
    }
    if (view.hiddenCount > MAX_VIEW_HIDDEN) return pImpl->fail(Result::InvalidArguments, VIEW_LIMIT);
    if ((view.hiddenCount && !view.hidden) || (view.follow && !pImpl->entry(view.follow))) {
        return pImpl->fail(Result::InvalidArguments, VIEW_OBJECT);
    }
    for (auto i = 0u; i < view.hiddenCount; i++) {
        if (!view.hidden[i] || !pImpl->entry(view.hidden[i])) return pImpl->fail(Result::InvalidArguments, VIEW_OBJECT);
    }
    if (pImpl->viewCnt >= MAX_VIEWS) return pImpl->fail(Result::InsufficientCondition, VIEW_LIMIT);
    if (!grow(pImpl->views, pImpl->viewCnt, pImpl->viewCap, 1u, 4u, MAX_VIEWS)) {
        return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    }
    auto entry = new (std::nothrow) ViewEntry;
    if (!entry) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    if (view.hiddenCount) {
        entry->hidden = new (std::nothrow) const Object*[view.hiddenCount];
        if (!entry->hidden) {
            delete entry;
            return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
        }
        for (auto i = 0u; i < view.hiddenCount; i++)
            entry->hidden[i] = view.hidden[i];
    }
    entry->settings = view;
    entry->settings.hidden = entry->hidden;
    entry->initialCamera = view.camera;
    entry->initialView = view.cameraView;
    entry->camera = view.camera;
    entry->cameraView = view.cameraView;
    entry->opacity = view.opacity;
    entry->born = pImpl->cursor;
    if (index) *index = pImpl->viewCnt;
    pImpl->views[pImpl->viewCnt++] = entry;
    return Result::Success;
}

Result Scene::look(uint32_t view, const Camera& camera, CameraView cameraView, float duration,
                   const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    if (view >= pImpl->viewCnt) return pImpl->fail(Result::InvalidArguments, VIEW_INDEX);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (pImpl->cursor < pImpl->views[view]->born) return pImpl->fail(Result::InsufficientCondition, VIEW_UNBORN);
    return pImpl->look(pImpl->track(view + 1u), camera, cameraView, duration, curve);
}

Result Scene::orbit(uint32_t view, float angle, const Vec3& axis, float duration,
                    const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    if (view >= pImpl->viewCnt) return pImpl->fail(Result::InvalidArguments, VIEW_INDEX);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (pImpl->cursor < pImpl->views[view]->born) return pImpl->fail(Result::InsufficientCondition, VIEW_UNBORN);
    return pImpl->orbit(pImpl->track(view + 1u), angle, axis, duration, curve);
}

Result Scene::fade(uint32_t view, float opacity, float duration, const AnimCurve& curve) noexcept
{
    pImpl->diagnostic = nullptr;
    if (view >= pImpl->viewCnt) return pImpl->fail(Result::InvalidArguments, VIEW_INDEX);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (!std::isfinite(opacity) || opacity < 0.0f || opacity > 1.0f) {
        return pImpl->fail(Result::InvalidArguments, VIEW_OPACITY);
    }
    if (!std::isfinite(duration) || duration <= 0.0f) return pImpl->fail(Result::InvalidArguments, PLAY_DURATION);
    if (!curve.valid()) return pImpl->fail(Result::InvalidArguments, PLAY_CURVE);
    auto& entry = *pImpl->views[view];
    auto cursor = pImpl->cursor;
    if (cursor < entry.born) return pImpl->fail(Result::InsufficientCondition, VIEW_UNBORN);
    // The fades of a view follow one another, as the animations of one property of an object do.
    if (entry.fadeCnt && cursor < entry.fades[entry.fadeCnt - 1u].end) {
        std::snprintf(pImpl->reason, sizeof(pImpl->reason),
                      "the view already fades until %.2f s; another fade of it starts at that time or later",
                      static_cast<double>(entry.fades[entry.fadeCnt - 1u].end));
        return pImpl->fail(Result::InsufficientCondition, pImpl->reason);
    }
    float nextCursor;
    if (!_timeline(cursor, duration, 0.0f, 1u, nextCursor)) {
        return pImpl->fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    if (!grow(entry.fades, entry.fadeCnt, entry.fadeCap, 1u, 4u)) return pImpl->fail(Result::OutOfMemory, SCENE_MEMORY);
    auto& fade = entry.fades[entry.fadeCnt++];
    fade.begin = cursor;
    fade.end = nextCursor;
    fade.from = entry.opacity;
    fade.to = opacity;
    fade.curve = curve;
    entry.opacity = opacity;
    pImpl->advance(nextCursor);
    pImpl->definitionsSealed = true;
    return Result::Success;
}

uint32_t Scene::viewCount() const noexcept
{
    return pImpl->viewCnt;
}

bool Scene::viewAt(uint32_t index, SceneView& view) const noexcept
{
    if (index >= pImpl->viewCnt) return false;
    auto& entry = *pImpl->views[index];
    view = entry.settings;
    view.camera = entry.camera;
    view.cameraView = entry.cameraView;
    view.opacity = entry.opacity;
    return true;
}

// ---------- Frames and queries ----------

// The height of the world a camera shows across the frame, at its target for a perspective one.
static double _span(const Camera& camera) noexcept
{
    if (camera.projection == Projection::Orthographic) return camera.orthoHeight;
    auto distance = std::hypot(static_cast<double>(camera.target.x) - camera.eye.x,
                               static_cast<double>(camera.target.y) - camera.eye.y,
                               static_cast<double>(camera.target.z) - camera.eye.z);
    return 2.0 * distance * std::tan(0.5 * static_cast<double>(camera.fov));
}

Result Scene::Impl::frame(uint32_t index, float time, ViewFrame& frame) const noexcept
{
    frame = {};
    if (index >= viewCnt || !std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    auto& entry = *views[index];
    auto& settings = entry.settings;
    if (time < entry.born) return Result::Success;
    auto opacity = settings.opacity;
    for (auto i = 0u; i < entry.fadeCnt; i++) {
        auto& fade = entry.fades[i];
        if (time < fade.begin) break;
        if (time >= fade.end) {
            opacity = fade.to;
            continue;
        }
        auto eased = ease(fade.curve, (time - fade.begin) / (fade.end - fade.begin));
        opacity = fade.from + (fade.to - fade.from) * eased;
    }
    opacity = _clamp(opacity);
    if (!(opacity > 0.0f)) return Result::Success;

    auto camera = entry.initialCamera;
    auto view = entry.initialView;
    if (!_sampleCamera(entry.cameras, entry.cameraCnt, time, camera, view)) return Result::InvalidArguments;
    if (settings.follow) {
        Bounds bounds;
        auto defined = false;
        auto result = this->bounds(settings.follow, time, bounds, defined);
        if (result != Result::Success) return result;
        if (defined) {
            auto center = bounds.center();
            // A 2D eye stands straight above its target, so it takes the target's x and y as they
            // are, which adding the shift does not always round back to.
            if (view == CameraView::TwoD) camera.eye = {center.x, center.y, camera.eye.z + (center.z - camera.target.z)};
            else camera.eye = camera.eye + (center - camera.target);
            camera.target = center;
        }
    }
    auto config = cfg;
    config.camera = camera;
    config.cameraView = view;
    if (!valid(config)) return Result::InvalidArguments;

    Camera main;
    CameraView mainView;
    auto result = sample(time, main, mainView);
    if (result != Result::Success) return result;
    auto width = static_cast<double>(cfg.width);
    auto height = static_cast<double>(cfg.height);
    auto& rect = settings.viewport;
    double area[] = {rect.x * width, rect.y * height, rect.width * width, rect.height * height};
    // World units a pixel spans at the target of each camera; an optical view grows what the
    // scene sizes in pixels by the ratio of the two, its magnification.
    auto mainUnit = _span(main) / height;
    auto viewUnit = _span(camera) / area[3];
    auto scale = settings.optical ? mainUnit / viewUnit : 1.0;
    // The scene lowers at the size of the frame, where a pixel spans scale view pixels.
    auto unit = viewUnit * scale;
    frame.camera = camera;
    if (camera.projection == Projection::Orthographic) {
        frame.camera.orthoHeight = static_cast<float>(unit * height);
    } else {
        auto distance = 0.5 * _span(camera) / std::tan(0.5 * static_cast<double>(camera.fov));
        frame.camera.fov = static_cast<float>(2.0 * std::atan(0.5 * unit * height / distance));
    }
    config.camera = frame.camera;
    if (!std::isfinite(scale) || scale <= 0.0 || !valid(config)) return Result::InvalidArguments;
    frame.cameraView = view;
    frame.scale = static_cast<float>(scale);
    frame.offset = {static_cast<float>(area[0] + 0.5 * (area[2] - scale * width)),
                    static_cast<float>(area[1] + 0.5 * (area[3] - scale * height))};
    for (auto i = 0u; i < 4u; i++)
        frame.rect[i] = static_cast<float>(area[i]);
    frame.opacity = opacity;
    frame.layer = settings.layer;
    frame.background = settings.background;
    frame.hidden = settings.hidden;
    frame.hiddenCount = settings.hiddenCount;
    frame.shown = true;
    return Result::Success;
}

CameraView Scene::view(float time) const noexcept
{
    if (!std::isfinite(time) || time < 0.0f) return CameraView::TwoD;
    Camera camera;
    CameraView view;
    if (pImpl->sample(time, camera, view) != Result::Success) return CameraView::TwoD;
    return view;
}

uint32_t Scene::text(const Text* text, float time, char* output, uint32_t capacity) const noexcept
{
    if (output && capacity) output[0] = '\0';
    auto entry = text && std::isfinite(time) ? pImpl->entry(text) : nullptr;
    VisualState state;
    if (!entry || !pImpl->sample(*entry, time, state)) return 0u;
    const char* value;
    char* written;
    if (shown(text, state, value, written) != Result::Success) return 0u;
    auto length = std::strlen(value);
    if (length >= UINT32_MAX) length = 0u;
    else if (output && length < capacity) std::memcpy(output, value, length + 1u);
    delete[] written;
    return static_cast<uint32_t>(length);
}

uint32_t Scene::count() const noexcept
{
    return pImpl->objectCnt;
}

Object* Scene::object(uint32_t id) const noexcept
{
    if (!id || id > pImpl->objectCnt) return nullptr;
    return pImpl->objects[id - 1u].object;
}

Object* Scene::objectAt(uint32_t index) const noexcept
{
    if (index >= pImpl->objectCnt) return nullptr;
    return pImpl->objects[index].object;
}

Object* Scene::object(const char* tag) const noexcept
{
    if (!tag) return nullptr;
    for (auto i = 0u; i < pImpl->objectCnt; i++) {
        auto value = pImpl->objects[i].object->tag();
        if (value && std::strcmp(value, tag) == 0) return pImpl->objects[i].object;
    }
    return nullptr;
}

// Adds to output the fonts that the texts of scene, and of the scenes it shows, are set in but
// that are not loaded, after the count names it holds, each once; returns the new count.
static uint32_t _missingFonts(const Scene* scene, const char** output, uint32_t capacity, uint32_t count)
{
    for (auto i = 0u; i < scene->count() && count < capacity; i++) {
        auto object = scene->objectAt(i);
        if (object->type() != Type::Text) continue;
        auto font = static_cast<const Text*>(object)->font();
        if (!font || !font[0]) continue;
        auto named = false;
        for (auto j = 0u; j < count && !named; j++) named = std::strcmp(output[j], font) == 0;
        if (named) continue;
        // A font that is not loaded is measured as the default font, and the extent says so.
        TextExtent extent;
        if (renderer::measureText(font, 1.0f, "x", 0.0f, false, extent) == Result::Success && extent.fallback) {
            output[count++] = font;
        }
    }
    for (auto i = 0u; i < scene->viewportCount(); i++) count = _missingFonts(scene->sceneAt(i), output, capacity, count);
    for (auto i = 0u; i < scene->transitionCount(); i++) count = _missingFonts(scene->transitionAt(i), output, capacity, count);
    return count;
}

uint32_t Scene::missingFonts(const char** output, uint32_t capacity) const noexcept
{
    return output ? _missingFonts(this, output, capacity, 0u) : 0u;
}

uint32_t Scene::viewportCount() const noexcept
{
    return pImpl->viewportCnt;
}

Scene* Scene::sceneAt(uint32_t index) const noexcept
{
    if (index >= pImpl->viewportCnt) return nullptr;
    return pImpl->viewports[index].scene;
}

bool Scene::viewportAt(uint32_t index, Viewport& viewport) const noexcept
{
    if (index >= pImpl->viewportCnt) return false;
    viewport = pImpl->viewports[index].viewport;
    return true;
}

uint32_t Scene::transitionCount() const noexcept
{
    return pImpl->sequence ? pImpl->sequence->count : 0u;
}

Scene* Scene::transitionAt(uint32_t index) const noexcept
{
    if (!pImpl->sequence || index >= pImpl->sequence->count) return nullptr;
    return pImpl->sequence->stages[index].scene;
}

Config& Scene::config() noexcept
{
    return pImpl->cfg;
}

const Config& Scene::config() const noexcept
{
    return pImpl->cfg;
}

}  // namespace vis
