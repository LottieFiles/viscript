#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

#include "vis.h"
#include "testCheck.h"

static constexpr auto _pi = 3.14159265358979323846f;

static bool near(const vis::Vec2& a, const vis::Vec2& b)
{
    return near(a.x, b.x) && near(a.y, b.y);
}

static bool near(const vis::Vec3& a, const vis::Vec3& b)
{
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

static void _vectors()
{
    auto a = vis::Vec2{3.0f, 4.0f};
    CHECK(near(a.length(), 5.0f));
    CHECK(near(a.normalized(), {0.6f, 0.8f}));
    CHECK(near(a + vis::Vec2{1.0f, -2.0f}, {4.0f, 2.0f}));
    CHECK(near(a.dot({-4.0f, 3.0f}), 0.0f));
    CHECK(near(vis::Vec2{}.normalized(), {}));
    CHECK(near(vis::Vec2{1.0e-7f, 0.0f}.normalized(), {1.0f, 0.0f}));
    auto large2 = vis::Vec2{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}.normalized();
    CHECK(std::isfinite(large2.x) && std::isfinite(large2.y));
    CHECK(near(large2.length(), 1.0f));

    auto x = vis::Vec3{1.0f, 0.0f, 0.0f};
    auto y = vis::Vec3{0.0f, 1.0f, 0.0f};
    CHECK(near(x.cross(y), {0.0f, 0.0f, 1.0f}));
    CHECK(near(x.cross(y).dot(x), 0.0f));
    auto large3 = vis::Vec3{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max()}
                      .normalized();
    CHECK(std::isfinite(large3.x) && std::isfinite(large3.y) && std::isfinite(large3.z));
    CHECK(near(large3.length(), 1.0f));
}

static void _matrices()
{
    auto model = vis::Mat3::translate({3.0f, -1.0f}) * vis::Mat3::rotate(_pi * 0.5f) * vis::Mat3::scale({2.0f, 3.0f});
    CHECK(near(model * vis::Vec2{1.0f, 0.0f}, {3.0f, 1.0f}));

    auto inverse = vis::Mat3{};
    CHECK(model.inverse(inverse));
    CHECK(near(inverse * (model * vis::Vec2{2.0f, -1.0f}), {2.0f, -1.0f}));
    auto singular = vis::Mat3::scale({0.0f, 1.0f});
    CHECK(!singular.inverse(inverse));
    auto small = vis::Mat3::scale({1.0e-4f, 1.0e-4f});
    CHECK(small.inverse(inverse));
    CHECK(near(inverse * (small * vis::Vec2{2.0f, -1.0f}), {2.0f, -1.0f}));
    auto projective2 = vis::Mat3::identity();
    projective2.e[8] = 1.0e-7f;
    auto projected2 = projective2 * vis::Vec2{1.0f, 2.0f};
    CHECK(projected2.x > 9.9e6f && projected2.y > 1.99e7f);
    projective2.e[8] = 0.0f;
    projected2 = projective2 * vis::Vec2{1.0f, 2.0f};
    CHECK(!std::isfinite(projected2.x) && !std::isfinite(projected2.y));
    auto maximum = std::numeric_limits<float>::max();
    auto minimum = std::numeric_limits<float>::denorm_min();
    auto scaled2 = vis::Mat3{};
    scaled2.e[0] = scaled2.e[4] = scaled2.e[8] = maximum;
    CHECK(near(scaled2 * vis::Vec2{2.0f, 3.0f}, {2.0f, 3.0f}));
    scaled2.e[0] = scaled2.e[4] = scaled2.e[8] = minimum;
    CHECK(near(scaled2 * vis::Vec2{0.5f, 1.5f}, {0.5f, 1.5f}));
    scaled2 = {};
    scaled2.e[0] = minimum;
    scaled2.e[8] = maximum;
    projected2 = scaled2 * vis::Vec2{1.0f, 0.0f};
    CHECK(!std::isfinite(projected2.x) && !std::isfinite(projected2.y));
    auto cancellation2A = vis::Mat3::identity();
    cancellation2A.e[0] = maximum;
    cancellation2A.e[1] = maximum;
    auto cancellation2B = vis::Mat3::identity();
    cancellation2B.e[0] = 2.0f;
    cancellation2B.e[3] = -2.0f;
    auto cancelled2 = cancellation2A * cancellation2B;
    CHECK(cancelled2.e[0] == 0.0f && cancelled2.e[1] == maximum);

    auto model3 = vis::Mat4::translate({1.0f, 2.0f, 3.0f}) * vis::Mat4::rotateZ(_pi * 0.5f) * vis::Mat4::scale({2.0f, 3.0f, 4.0f});
    CHECK(near(model3 * vis::Vec3{1.0f, 0.0f, 1.0f}, {1.0f, 4.0f, 7.0f}));
    CHECK(near(vis::Mat4::from(model) * vis::Vec3{1.0f, 0.0f, 5.0f}, {3.0f, 1.0f, 5.0f}));
    auto projective3 = vis::Mat4::identity();
    projective3.e[15] = 1.0e-7f;
    auto projected3 = projective3 * vis::Vec3{1.0f, 2.0f, 3.0f};
    CHECK(projected3.x > 9.9e6f && projected3.z > 2.99e7f);
    projective3.e[15] = 0.0f;
    projected3 = projective3 * vis::Vec3{1.0f, 2.0f, 3.0f};
    CHECK(!std::isfinite(projected3.x) && !std::isfinite(projected3.z));
    auto scaled3 = vis::Mat4{};
    scaled3.e[0] = scaled3.e[5] = scaled3.e[10] = scaled3.e[15] = maximum;
    CHECK(near(scaled3 * vis::Vec3{2.0f, 3.0f, 4.0f}, {2.0f, 3.0f, 4.0f}));
    scaled3.e[0] = scaled3.e[5] = scaled3.e[10] = scaled3.e[15] = minimum;
    CHECK(near(scaled3 * vis::Vec3{0.5f, 1.5f, 2.5f}, {0.5f, 1.5f, 2.5f}));
    scaled3 = {};
    scaled3.e[0] = minimum;
    scaled3.e[15] = maximum;
    projected3 = scaled3 * vis::Vec3{1.0f, 0.0f, 0.0f};
    CHECK(!std::isfinite(projected3.x) && !std::isfinite(projected3.y) && !std::isfinite(projected3.z));
    auto cancellation3A = vis::Mat4::identity();
    cancellation3A.e[0] = maximum;
    cancellation3A.e[1] = maximum;
    auto cancellation3B = vis::Mat4::identity();
    cancellation3B.e[0] = 2.0f;
    cancellation3B.e[4] = -2.0f;
    auto cancelled3 = cancellation3A * cancellation3B;
    CHECK(cancelled3.e[0] == 0.0f && cancelled3.e[1] == maximum);
}

static vis::Camera _camera()
{
    auto camera = vis::Camera{};
    camera.eye = {0.0f, 0.0f, 0.0f};
    camera.target = {0.0f, 0.0f, -1.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.projection = vis::Projection::Perspective;
    camera.fov = _pi * 0.5f;
    camera.near = 1.0f;
    camera.far = 11.0f;
    return camera;
}

static void _orthographic()
{
    auto camera = vis::Camera{};
    camera.eye = {2.0f, -3.0f, 10.0f};
    camera.target = {2.0f, -3.0f, 0.0f};
    camera.orthoHeight = 6.0f;
    auto ndc = vis::Vec3{};
    CHECK(camera.project({3.0f, -2.0f, 0.0f}, 1280.0f / 720.0f, ndc));
    CHECK(near(ndc, {0.1875f, 1.0f / 3.0f, 0.0099505f}));

    auto expected = vis::Vec2{-0.35f, 0.42f};
    auto ray = camera.ray(expected, 1280.0f / 720.0f);
    auto world = ray.origin + ray.direction * 10.0f;
    CHECK(camera.project(world, 1280.0f / 720.0f, ndc));
    CHECK(near(vis::Vec2{ndc.x, ndc.y}, expected));
}

static void _projection()
{
    auto camera = _camera();
    auto ndc = vis::Vec3{};
    CHECK(camera.project({2.0f, 1.0f, -2.0f}, 2.0f, ndc));
    CHECK(near(ndc, {0.5f, 0.5f, 0.1f}));
    CHECK(camera.project({8.0f, 0.0f, -2.0f}, 2.0f, ndc));
    CHECK(ndc.x > 1.0f);
    CHECK(!camera.project({0.0f, 0.0f, 1.0f}, 2.0f, ndc));
    CHECK(!camera.project({0.0f, 0.0f, -12.0f}, 2.0f, ndc));

    auto scaledCamera = _camera();
    scaledCamera.target = {0.0f, 0.0f, -1.0e-7f};
    scaledCamera.up = {0.0f, 1.0e-7f, 0.0f};
    scaledCamera.near = 1.0e-8f;
    CHECK(scaledCamera.project({0.0f, 0.0f, -1.0e-6f}, 1.0f, ndc));
    CHECK(near(ndc.x, 0.0f) && near(ndc.y, 0.0f));

    camera.projection = vis::Projection::Orthographic;
    camera.orthoHeight = 4.0f;
    CHECK(camera.project({2.0f, 1.0f, -5.0f}, 2.0f, ndc));
    CHECK(near(ndc, {0.5f, 0.5f, 0.4f}));
    camera.orthoHeight = 1.0e-7f;
    CHECK(camera.project({1.0e-8f, 0.0f, -5.0f}, 1.0f, ndc));
    CHECK(near(ndc.x, 0.2f));
    camera.orthoHeight = 1.0e-40f;
    CHECK(camera.project({1.0e-41f, 0.0f, -5.0f}, 1.0f, ndc));
    CHECK(near(ndc.x, 0.2f, 1.0e-3f));

    camera.projection = vis::Projection::Perspective;
    camera.fov = std::numeric_limits<float>::denorm_min();
    CHECK(camera.project({std::numeric_limits<float>::denorm_min(), 0.0f, -2.0f}, 1.0f, ndc));
    CHECK(near(ndc.x, 1.0f));

    auto nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(!camera.project({nan, 0.0f, -5.0f}, 2.0f, ndc));
    CHECK(!camera.project({0.0f, 0.0f, -5.0f}, nan, ndc));

    camera.eye = {-std::numeric_limits<float>::max(), 0.0f, 0.0f};
    camera.target = {std::numeric_limits<float>::max(), 0.0f, 0.0f};
    CHECK(!camera.project({0.0f, 0.0f, 0.0f}, 1.0f, ndc));
}

static void _clipping()
{
    auto camera = _camera();
    auto from = vis::Vec3{0.0f, 0.0f, -0.25f};
    auto to = vis::Vec3{2.0f, 0.0f, -5.0f};
    CHECK(camera.segment(from, to));
    CHECK(near(from.z, -1.0f));
    CHECK(near(from.x, 0.3157895f));
    CHECK(near(to, {2.0f, 0.0f, -5.0f}));

    from = {0.0f, 0.0f, -5.0f};
    to = {0.0f, 0.0f, -20.0f};
    CHECK(camera.segment(from, to));
    CHECK(near(to.z, -11.0f));

    from = {0.0f, 0.0f, 0.5f};
    to = {0.0f, 0.0f, 2.0f};
    CHECK(!camera.segment(from, to));

    camera.eye = {-std::numeric_limits<float>::max(), 0.0f, 0.0f};
    camera.target = {std::numeric_limits<float>::max(), 0.0f, 0.0f};
    from = {0.0f, 0.0f, 0.0f};
    to = {1.0f, 0.0f, 0.0f};
    CHECK(!camera.segment(from, to));

    auto maximum = std::numeric_limits<float>::max();
    auto wide = vis::Camera{};
    wide.eye = {-maximum, 0.0f, 0.0f};
    wide.target = {};
    wide.up = {0.0f, 1.0f, 0.0f};
    wide.projection = vis::Projection::Orthographic;
    wide.orthoHeight = maximum;
    wide.near = 1.0f;
    wide.far = maximum;
    from = {0.0f, 0.0f, maximum};
    to = {0.0f, 0.0f, -maximum};
    auto projected = vis::Vec3{};
    CHECK(wide.project(from, 1.0f, projected));
    CHECK(wide.project(to, 1.0f, projected));
    CHECK(wide.segment(from, to));
    CHECK(from.z == maximum && to.z == -maximum);
}

static void _rays()
{
    auto camera = _camera();
    auto expected = vis::Vec2{0.25f, -0.5f};
    auto ray = camera.ray(expected, 2.0f);
    auto point = ray.origin + ray.direction * 5.0f;
    auto ndc = vis::Vec3{};
    CHECK(camera.project(point, 2.0f, ndc));
    CHECK(near(vis::Vec2{ndc.x, ndc.y}, expected));

    camera.projection = vis::Projection::Orthographic;
    camera.orthoHeight = 4.0f;
    ray = camera.ray(expected, 2.0f);
    point = ray.origin + ray.direction * 3.0f;
    CHECK(near(ray.direction, {0.0f, 0.0f, -1.0f}));
    CHECK(camera.project(point, 2.0f, ndc));
    CHECK(near(vis::Vec2{ndc.x, ndc.y}, expected));

    auto subnormal = std::numeric_limits<float>::denorm_min();
    camera.projection = vis::Projection::Perspective;
    camera.fov = subnormal;
    ray = camera.ray({2.0f, 0.0f}, 1.0f);
    point = ray.origin + ray.direction * 2.0f;
    CHECK(camera.project(point, 1.0f, ndc));
    CHECK(near(ndc.x, 2.0f));
    CHECK(camera.ray({1.0f, 0.0f}, 1.0f).direction.length() == 0.0f);

    camera.projection = vis::Projection::Orthographic;
    camera.orthoHeight = subnormal;
    ray = camera.ray({2.0f, 0.0f}, 1.0f);
    point = ray.origin + ray.direction * 3.0f;
    CHECK(camera.project(point, 1.0f, ndc));
    CHECK(near(ndc.x, 2.0f));
}

static void _easing()
{
    for (auto easing : {vis::Easing::Linear, vis::Easing::Smooth, vis::Easing::EaseIn,
                        vis::Easing::EaseOut, vis::Easing::EaseInOut}) {
        CHECK(near(vis::ease(easing, 0.0f), 0.0f));
        CHECK(near(vis::ease(easing, 1.0f), 1.0f));
    }
    CHECK(near(vis::ease(vis::Easing::Smooth, 0.5f), 0.5f));
    CHECK(near(vis::ease(vis::Easing::EaseIn, 0.5f), 0.25f));
    CHECK(near(vis::ease(vis::Easing::EaseOut, 0.5f), 0.75f));
    CHECK(near(vis::ease(vis::Easing::EaseInOut, 0.25f), 0.125f));
    CHECK(near(vis::ease(vis::Easing::EaseInOut, 0.75f), 0.875f));
    CHECK(near(vis::ease(vis::Easing::Linear, -1.0f), 0.0f));
    CHECK(near(vis::ease(vis::Easing::Linear, 2.0f), 1.0f));

    for (auto preset : {vis::AnimCurvePreset::Linear, vis::AnimCurvePreset::Gentle,
                        vis::AnimCurvePreset::Snappy, vis::AnimCurvePreset::Back,
                        vis::AnimCurvePreset::Bounce, vis::AnimCurvePreset::Elastic}) {
        auto curve = vis::AnimCurve::preset(preset);
        CHECK(curve.valid());
        CHECK(near(vis::ease(curve, 0.0f), 0.0f));
        CHECK(near(vis::ease(curve, 1.0f), 1.0f));
    }
    auto back = vis::AnimCurve::preset(vis::AnimCurvePreset::Back);
    CHECK(vis::ease(back, 0.8f) > 1.0f);
    auto weak = vis::AnimCurve::preset(vis::AnimCurvePreset::Back, 0.0f);
    CHECK(near(vis::ease(weak, 0.37f), 0.37f));
    auto strong = vis::AnimCurve::preset(vis::AnimCurvePreset::Back, 1.6f);
    CHECK(vis::ease(strong, 0.8f) > vis::ease(back, 0.8f));
    auto reversed = back.reversed();
    CHECK(reversed.reverse);
    CHECK(near(vis::ease(reversed, 0.2f), 1.0f - vis::ease(back, 0.8f)));
    CHECK(near(vis::ease(reversed.reversed(), 0.2f), vis::ease(back, 0.2f)));

    auto custom = vis::AnimCurve::cubicBezier({0.42f, 0.0f}, {0.58f, 1.0f}, 0.9f);
    CHECK(custom.valid());
    CHECK(near(vis::ease(custom, 0.5f), 0.5f));
    custom.control1.x = -0.1f;
    CHECK(!custom.valid());
    custom = vis::AnimCurve::preset(vis::AnimCurvePreset::Smooth, 2.1f);
    CHECK(!custom.valid());
}

static void _colors()
{
    auto color = vis::Color::hex("#336699");
    CHECK(color.r == 0x33 && color.g == 0x66 && color.b == 0x99 && color.a == 0xff);
    color = vis::Color::hex("8040ff80");
    CHECK(color.r == 0x80 && color.g == 0x40 && color.b == 0xff && color.a == 0x80);
    color = vis::Color::hex("#abc");
    CHECK(color.r == 0xaa && color.g == 0xbb && color.b == 0xcc && color.a == 0xff);
    color = vis::Color::hex("#abcd");
    CHECK(color.r == 0xaa && color.g == 0xbb && color.b == 0xcc && color.a == 0xdd);
    auto fallback = vis::Color{1, 2, 3, 4};
    color = vis::Color::hex("not-a-color", fallback);
    CHECK(color.r == 1 && color.g == 2 && color.b == 3 && color.a == 4);
    CHECK(std::strcmp(vis::result(vis::Result::IoError), "IoError") == 0);
    CHECK(std::strcmp(vis::result(static_cast<vis::Result>(255)), "Unknown") == 0);
}

int main()
{
    _vectors();
    _matrices();
    _orthographic();
    _projection();
    _clipping();
    _rays();
    _easing();
    _colors();
    return report();
}
