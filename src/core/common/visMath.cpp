// The arithmetic of the value types in vis.h: vectors, matrices, colors, camera projection,
// animation curves and easing, the spring response, and the names of results.
#include <cmath>
#include <cstring>
#include <limits>

#include "vis.h"
#include "visFinite.h"
#include "visConstants.h"

namespace vis
{

static constexpr auto _epsilon = 1.0e-6f;

static float _narrow(double value) noexcept
{
    auto limit = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || std::fabs(value) > limit) return std::numeric_limits<float>::quiet_NaN();
    auto output = static_cast<float>(value);
    return value != 0.0 && output == 0.0f ? std::numeric_limits<float>::quiet_NaN() : output;
}

static bool _narrow(double x, double y, double z, Vec3& output) noexcept
{
    auto limit = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::fabs(x) > limit || std::fabs(y) > limit || std::fabs(z) > limit) {
        return false;
    }
    auto narrowedX = static_cast<float>(x);
    auto narrowedY = static_cast<float>(y);
    auto narrowedZ = static_cast<float>(z);
    if ((x != 0.0 && narrowedX == 0.0f) || (y != 0.0 && narrowedY == 0.0f) || (z != 0.0 && narrowedZ == 0.0f)) return false;
    output = {narrowedX, narrowedY, narrowedZ};
    return true;
}

static bool _narrowPoint(double x, double y, double z, Vec3& output) noexcept
{
    auto limit = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::fabs(x) > limit || std::fabs(y) > limit || std::fabs(z) > limit) {
        return false;
    }
    output = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    return true;
}

static bool _normalized(double x, double y, double z, Vec3& output) noexcept
{
    auto scale = std::fmax(std::fmax(std::fabs(x), std::fabs(y)), std::fabs(z));
    if (!std::isfinite(scale) || scale == 0.0) return false;
    x /= scale;
    y /= scale;
    z /= scale;
    auto length = std::sqrt(x * x + y * y + z * z);
    return std::isfinite(length) && length > 0.0 && _narrow(x / length, y / length, z / length, output);
}

static float _clamp(float value) noexcept
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static bool _basis(const Camera& camera, Vec3& forward, Vec3& right, Vec3& correctedUp) noexcept
{
    if (!_finite(camera.eye) || !_finite(camera.target) || !_finite(camera.up)) return false;
    if (!_normalized(static_cast<double>(camera.target.x) - camera.eye.x,
                     static_cast<double>(camera.target.y) - camera.eye.y,
                     static_cast<double>(camera.target.z) - camera.eye.z, forward)) {
        return false;
    }
    auto up = Vec3{};
    if (!_normalized(camera.up.x, camera.up.y, camera.up.z, up)) return false;

    auto sideX = static_cast<double>(forward.y) * up.z - static_cast<double>(forward.z) * up.y;
    auto sideY = static_cast<double>(forward.z) * up.x - static_cast<double>(forward.x) * up.z;
    auto sideZ = static_cast<double>(forward.x) * up.y - static_cast<double>(forward.y) * up.x;
    auto sideLength = std::hypot(sideX, sideY, sideZ);
    if (!std::isfinite(sideLength) || sideLength <= _epsilon || !_normalized(sideX, sideY, sideZ, right)) {
        return false;
    }
    return _normalized(static_cast<double>(right.y) * forward.z - static_cast<double>(right.z) * forward.y,
                       static_cast<double>(right.z) * forward.x - static_cast<double>(right.x) * forward.z,
                       static_cast<double>(right.x) * forward.y - static_cast<double>(right.y) * forward.x,
                       correctedUp);
}

static int32_t _hex(char value) noexcept
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Vec2 Vec2::operator+(const Vec2& v) const noexcept
{
    return {x + v.x, y + v.y};
}

Vec2 Vec2::operator-(const Vec2& v) const noexcept
{
    return {x - v.x, y - v.y};
}

Vec2 Vec2::operator*(float s) const noexcept
{
    return {x * s, y * s};
}

Vec2 Vec2::operator/(float s) const noexcept
{
    return {x / s, y / s};
}

float Vec2::dot(const Vec2& v) const noexcept
{
    return x * v.x + y * v.y;
}

float Vec2::length() const noexcept
{
    return std::hypot(x, y);
}

Vec3 Vec3::operator+(const Vec3& v) const noexcept
{
    return {x + v.x, y + v.y, z + v.z};
}

Vec3 Vec3::operator-(const Vec3& v) const noexcept
{
    return {x - v.x, y - v.y, z - v.z};
}

Vec3 Vec3::operator*(float s) const noexcept
{
    return {x * s, y * s, z * s};
}

Vec3 Vec3::operator/(float s) const noexcept
{
    return {x / s, y / s, z / s};
}

float Vec3::dot(const Vec3& v) const noexcept
{
    return x * v.x + y * v.y + z * v.z;
}

Vec3 Vec3::cross(const Vec3& v) const noexcept
{
    return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
}

float Vec3::length() const noexcept
{
    return std::hypot(x, y, z);
}

Vec3 Vec3::normalized() const noexcept
{
    if (!_finite(*this)) return {};
    auto scale = std::fmax(std::fmax(std::fabs(x), std::fabs(y)), std::fabs(z));
    if (scale == 0.0f) return {};
    auto sx = x / scale;
    auto sy = y / scale;
    auto sz = z / scale;
    auto len = std::sqrt(sx * sx + sy * sy + sz * sz);
    return {sx / len, sy / len, sz / len};
}

Mat4 Mat4::identity() noexcept
{
    return {{1.0f, 0.0f, 0.0f, 0.0f,
             0.0f, 1.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 1.0f, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f}};
}

Mat4 Mat4::translate(const Vec3& v) noexcept
{
    return {{1.0f, 0.0f, 0.0f, v.x,
             0.0f, 1.0f, 0.0f, v.y,
             0.0f, 0.0f, 1.0f, v.z,
             0.0f, 0.0f, 0.0f, 1.0f}};
}

Mat4 Mat4::scale(const Vec3& v) noexcept
{
    return {{v.x, 0.0f, 0.0f, 0.0f,
             0.0f, v.y, 0.0f, 0.0f,
             0.0f, 0.0f, v.z, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f}};
}

Mat4 Mat4::rotateX(float radians) noexcept
{
    auto c = std::cos(radians);
    auto s = std::sin(radians);
    return {{1.0f, 0.0f, 0.0f, 0.0f,
             0.0f, c, -s, 0.0f,
             0.0f, s, c, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f}};
}

Mat4 Mat4::rotateY(float radians) noexcept
{
    auto c = std::cos(radians);
    auto s = std::sin(radians);
    return {{c, 0.0f, s, 0.0f,
             0.0f, 1.0f, 0.0f, 0.0f,
             -s, 0.0f, c, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f}};
}

Mat4 Mat4::rotateZ(float radians) noexcept
{
    auto c = std::cos(radians);
    auto s = std::sin(radians);
    return {{c, -s, 0.0f, 0.0f,
             s, c, 0.0f, 0.0f,
             0.0f, 0.0f, 1.0f, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f}};
}

Mat4 Mat4::operator*(const Mat4& m) const noexcept
{
    auto out = Mat4{};
    for (auto row = 0; row < 4; row++) {
        for (auto col = 0; col < 4; col++) {
            auto value = 0.0;
            for (auto k = 0; k < 4; k++)
                value += static_cast<double>(e[row * 4 + k]) * m.e[k * 4 + col];
            out.e[row * 4 + col] = _narrow(value);
        }
    }
    return out;
}

Vec3 Mat4::operator*(const Vec3& v) const noexcept
{
    auto x = static_cast<double>(e[0]) * v.x + static_cast<double>(e[1]) * v.y + static_cast<double>(e[2]) * v.z + e[3];
    auto y = static_cast<double>(e[4]) * v.x + static_cast<double>(e[5]) * v.y + static_cast<double>(e[6]) * v.z + e[7];
    auto z = static_cast<double>(e[8]) * v.x + static_cast<double>(e[9]) * v.y + static_cast<double>(e[10]) * v.z + e[11];
    auto w = static_cast<double>(e[12]) * v.x + static_cast<double>(e[13]) * v.y + static_cast<double>(e[14]) * v.z + e[15];
    if (!std::isfinite(w) || w == 0.0) {
        auto invalid = std::numeric_limits<float>::quiet_NaN();
        return {invalid, invalid, invalid};
    }
    x /= w;
    y /= w;
    z /= w;
    auto output = Vec3{};
    if (!_narrow(x, y, z, output)) {
        auto invalid = std::numeric_limits<float>::quiet_NaN();
        return {invalid, invalid, invalid};
    }
    return output;
}

Color Color::hex(const char* value, Color fallback) noexcept
{
    if (!value) return fallback;
    if (*value == '#') value++;

    auto len = std::strlen(value);
    if (len != 3 && len != 4 && len != 6 && len != 8) return fallback;

    uint8_t values[4] = {0, 0, 0, 255};
    auto count = static_cast<uint32_t>(len == 3 || len == 6 ? 3 : 4);
    auto compact = len <= 4;
    for (auto i = 0u; i < count; i++) {
        auto high = _hex(value[compact ? i : i * 2]);
        auto low = compact ? high : _hex(value[i * 2 + 1]);
        if (high < 0 || low < 0) return fallback;
        values[i] = static_cast<uint8_t>(high * 16 + low);
    }
    return {values[0], values[1], values[2], values[3]};
}

bool Camera::project(const Vec3& world, float aspect, Vec3& ndc) const noexcept
{
    if (!_finite(world) || !std::isfinite(aspect) || aspect <= 0.0f) return false;
    if (!std::isfinite(near) || !std::isfinite(far) || far <= near) return false;
    if (projection != Projection::Perspective && projection != Projection::Orthographic) return false;

    Vec3 forward;
    Vec3 right;
    Vec3 correctedUp;
    if (!_basis(*this, forward, right, correctedUp)) return false;

    auto x = static_cast<double>(world.x) - eye.x;
    auto y = static_cast<double>(world.y) - eye.y;
    auto z = static_cast<double>(world.z) - eye.z;
    auto depth = x * forward.x + y * forward.y + z * forward.z;
    if (!std::isfinite(depth) || depth < near || depth > far) return false;

    auto halfHeight = static_cast<double>(orthoHeight) * 0.5;
    if (projection == Projection::Perspective) {
        if (near <= 0.0f || !std::isfinite(fov) || fov <= 0.0f || fov >= PI) return false;
        halfHeight = std::tan(static_cast<double>(fov) * 0.5) * depth;
    } else if (!std::isfinite(orthoHeight) || orthoHeight <= 0.0f) {
        return false;
    }

    auto horizontal = halfHeight * static_cast<double>(aspect);
    if (!std::isfinite(halfHeight) || !std::isfinite(horizontal) || halfHeight <= 0.0f || horizontal <= 0.0f) {
        return false;
    }
    auto projectedX = (x * right.x + y * right.y + z * right.z) / horizontal;
    auto projectedY = (x * correctedUp.x + y * correctedUp.y + z * correctedUp.z) / halfHeight;
    auto projectedZ = (depth - static_cast<double>(near)) / (static_cast<double>(far) - near);
    return _narrow(projectedX, projectedY, projectedZ, ndc);
}

bool Camera::segment(Vec3& from, Vec3& to) const noexcept
{
    if (!_finite(from) || !_finite(to)) return false;
    if (!std::isfinite(near) || !std::isfinite(far) || far <= near) return false;

    Vec3 forward;
    Vec3 right;
    Vec3 correctedUp;
    if (!_basis(*this, forward, right, correctedUp)) return false;

    auto ax = static_cast<double>(from.x);
    auto ay = static_cast<double>(from.y);
    auto az = static_cast<double>(from.z);
    auto bx = static_cast<double>(to.x);
    auto by = static_cast<double>(to.y);
    auto bz = static_cast<double>(to.z);
    auto dx = bx - ax;
    auto dy = by - ay;
    auto dz = bz - az;
    auto depthA = (ax - eye.x) * forward.x + (ay - eye.y) * forward.y + (az - eye.z) * forward.z;
    auto depthB = (bx - eye.x) * forward.x + (by - eye.y) * forward.y + (bz - eye.z) * forward.z;
    if (!std::isfinite(depthA) || !std::isfinite(depthB)) return false;
    if ((depthA < near && depthB < near) || (depthA > far && depthB > far)) return false;

    auto t0 = 0.0;
    auto t1 = 1.0;
    auto depthDelta = depthB - depthA;
    if (!std::isfinite(depthDelta)) return false;
    if (depthA < near) t0 = (near - depthA) / depthDelta;
    else if (depthA > far) t0 = (far - depthA) / depthDelta;
    if (depthB < near) t1 = (near - depthA) / depthDelta;
    else if (depthB > far) t1 = (far - depthA) / depthDelta;
    if (t0 > t1) return false;

    return _narrowPoint(ax + dx * t0, ay + dy * t0, az + dz * t0, from) && _narrowPoint(ax + dx * t1, ay + dy * t1, az + dz * t1, to);
}

AnimCurve AnimCurve::preset(AnimCurvePreset preset, float strength) noexcept
{
    AnimCurve curve;
    curve.kind = preset;
    curve.strength = strength;
    return curve;
}

AnimCurve AnimCurve::cubicBezier(const Vec2& control1, const Vec2& control2,
                                 float strength) noexcept
{
    AnimCurve curve;
    curve.kind = AnimCurvePreset::Custom;
    curve.control1 = control1;
    curve.control2 = control2;
    curve.strength = strength;
    return curve;
}

bool AnimCurve::valid() const noexcept
{
    if (static_cast<uint8_t>(kind) > static_cast<uint8_t>(AnimCurvePreset::Custom)
        || !std::isfinite(strength) || strength < 0.0f || strength > 2.0f) {
        return false;
    }
    if (kind != AnimCurvePreset::Custom) return true;
    return _finite(control1) && _finite(control2)
        && control1.x >= 0.0f && control1.x <= 1.0f
        && control2.x >= 0.0f && control2.x <= 1.0f
        && control1.y >= -2.0f && control1.y <= 2.0f
        && control2.y >= -2.0f && control2.y <= 2.0f;
}

static double _cubic(double t, double first, double second) noexcept
{
    auto inverse = 1.0 - t;
    return 3.0 * inverse * inverse * t * first
         + 3.0 * inverse * t * t * second + t * t * t;
}

static double _bezier(const AnimCurve& curve, double progress) noexcept
{
    auto low = 0.0;
    auto high = 1.0;
    for (auto i = 0u; i < 18u; i++) {
        auto parameter = (low + high) * 0.5;
        if (_cubic(parameter, curve.control1.x, curve.control2.x) < progress) low = parameter;
        else high = parameter;
    }
    return _cubic((low + high) * 0.5, curve.control1.y, curve.control2.y);
}

static double _bounce(double t) noexcept
{
    constexpr auto n = 7.5625;
    constexpr auto d = 2.75;
    if (t < 1.0 / d) return n * t * t;
    if (t < 2.0 / d) {
        t -= 1.5 / d;
        return n * t * t + 0.75;
    }
    if (t < 2.5 / d) {
        t -= 2.25 / d;
        return n * t * t + 0.9375;
    }
    t -= 2.625 / d;
    return n * t * t + 0.984375;
}

float ease(const AnimCurve& curve, float progress) noexcept
{
    auto t = static_cast<double>(_clamp(progress));
    if (!curve.valid() || t == 0.0 || t == 1.0) return static_cast<float>(t);

    auto sample = curve.reverse ? 1.0 - t : t;
    auto value = sample;
    switch (curve.kind) {
        case AnimCurvePreset::Smooth: value = sample * sample * (3.0 - 2.0 * sample); break;
        case AnimCurvePreset::EaseIn: value = sample * sample; break;
        case AnimCurvePreset::EaseOut: {
            auto inverse = 1.0 - sample;
            value = 1.0 - inverse * inverse;
            break;
        }
        case AnimCurvePreset::EaseInOut: {
            if (sample < 0.5) value = 2.0 * sample * sample;
            else {
                auto inverse = 1.0 - sample;
                value = 1.0 - 2.0 * inverse * inverse;
            }
            break;
        }
        case AnimCurvePreset::Gentle:
            value = sample * sample * sample * (sample * (sample * 6.0 - 15.0) + 10.0);
            break;
        case AnimCurvePreset::Snappy: {
            auto inverse = 1.0 - sample;
            value = 1.0 - inverse * inverse * inverse * inverse;
            break;
        }
        case AnimCurvePreset::Back: {
            constexpr auto overshoot = 1.70158;
            auto inverse = sample - 1.0;
            value = 1.0 + (overshoot + 1.0) * inverse * inverse * inverse
                  + overshoot * inverse * inverse;
            break;
        }
        case AnimCurvePreset::Bounce: value = _bounce(sample); break;
        case AnimCurvePreset::Elastic:
            value = std::pow(2.0, -10.0 * sample)
                  * std::sin((10.0 * sample - 0.75) * (2.0 * PI / 3.0)) + 1.0;
            break;
        case AnimCurvePreset::Custom: value = _bezier(curve, sample); break;
        default: break;
    }
    value = sample + (value - sample) * curve.strength;
    return static_cast<float>(curve.reverse ? 1.0 - value : value);
}

double springResponse(double seconds, double mass, double stiffness, double damping,
                      double displacement, double velocity) noexcept
{
    auto invalid = std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(seconds) || seconds < 0.0 || !std::isfinite(mass) || mass <= 0.0
        || !std::isfinite(stiffness) || stiffness <= 0.0 || !std::isfinite(damping) || damping < 0.0
        || !std::isfinite(displacement) || !std::isfinite(velocity)) return invalid;
    auto squared = stiffness / mass;
    auto drag = damping / mass;
    if (!std::isfinite(squared) || squared <= 0.0 || !std::isfinite(drag)) return invalid;
    if (seconds == 0.0) return displacement;

    auto omega = std::sqrt(squared);
    auto alpha = 0.5 * drag;
    double cosine;
    double sine;
    double decay;
    if (alpha < omega) {
        auto rate = std::sqrt(omega - alpha) * std::sqrt(omega + alpha);
        auto phase = rate * seconds;
        if (!std::isfinite(phase)) return invalid;
        decay = alpha * seconds;
        auto sinc = std::fabs(phase) < 1.0e-4
                  ? 1.0 - phase * phase / 6.0 + phase * phase * phase * phase / 120.0
                  : std::sin(phase) / phase;
        cosine = std::cos(phase);
        sine = seconds * sinc;
    } else if (alpha == omega) {
        decay = alpha * seconds;
        cosine = 1.0;
        sine = seconds;
    } else {
        auto rate = std::sqrt(alpha - omega) * std::sqrt(alpha + omega);
        // The slow root avoids subtracting two nearly equal large numbers.
        auto slow = -(omega / (alpha + rate)) * omega;
        decay = -slow * seconds;
        cosine = 0.5 * (1.0 + std::exp(-2.0 * rate * seconds));
        // expm1 keeps the two exponentials' divided difference near critical damping.
        sine = -std::expm1(-2.0 * rate * seconds) / (2.0 * rate);
    }
    if (!std::isfinite(decay)) return 0.0;
    auto stateScale = std::fmax(std::fabs(displacement), std::fabs(velocity));
    if (stateScale == 0.0) return 0.0;
    auto response = cosine + alpha * sine;
    auto responseScale = std::fmax(std::fabs(response), std::fabs(sine));
    // Combine bounded terms before restoring the shared amplitude and decay.
    auto normalized = (displacement / stateScale) * (response / responseScale)
                    + (velocity / stateScale) * (sine / responseScale);
    if (normalized == 0.0) return 0.0;
    auto magnitude = std::exp(std::log(stateScale) + std::log(responseScale)
                            + std::log(std::fabs(normalized)) - decay);
    auto value = std::copysign(magnitude, normalized);
    return std::isfinite(value) ? value : invalid;
}

const char* result(Result result) noexcept
{
    switch (result) {
        case Result::Success: return "Success";
        case Result::InvalidArguments: return "InvalidArguments";
        case Result::InsufficientCondition: return "InsufficientCondition";
        case Result::NonSupport: return "NonSupport";
        case Result::OutOfMemory: return "OutOfMemory";
        case Result::IoError: return "IoError";
        case Result::ScriptError: return "ScriptError";
        default: return "Unknown";
    }
}

}  // namespace vis
