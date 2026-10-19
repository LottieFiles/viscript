#include "testSceneChecks.h"

void timelineTracks(Renderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto box = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto late = Rectangle::gen({2.0f, 1.0f, 0.0f}, {0.5f, 0.5f});
    CHECK(scene && box && late);
    if (!scene || !box || !late) {
        delete scene;
        delete box;
        delete late;
        return;
    }
    box->style.stroke.a = 0;
    box->fill(Color::hex("#ffffff"));
    CHECK(scene->add(box) == Result::Success);
    CHECK(near(scene->cursor(), 0.0f));

    // A shift and a fade of one object overlap because they write different channels.
    CHECK(scene->play(Animation::shift(box, {2.0f, 0.0f, 0.0f}), 2.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(near(scene->cursor(), 2.0f));
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::fade(box, 0.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(near(scene->cursor(), 1.0f));
    CHECK(near(scene->duration(), 2.0f));
    Surface surface;
    BBox bounds;
    CHECK(renderer->bounds(scene, box, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 50.0f, 1.0f));
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(red(surface, 50u, 40u) > 180u && red(surface, 50u, 40u) < 200u);
    CHECK(renderer->bounds(scene, box, 1.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 70.0f, 1.0f));
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(red(surface, 70u, 40u) > 120u && red(surface, 70u, 40u) < 136u);

    // One channel stays in time order, and the failure explains itself: it names the property
    // and the time it is busy until.
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::fill(box, Color::hex("#ff0000")), 0.5f) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    CHECK(scene->remove(box) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(scene->seek(1.25f) == Result::Success);
    CHECK(scene->play(Animation::fill(box, Color::hex("#00ff00")), 0.5f) == Result::InsufficientCondition);
    CHECK(scene->diagnostic()
          && std::strcmp(scene->diagnostic(),
                         "the object already animates its fill color until 1.50 s; begin at that time or later") == 0);
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(box, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(near(scene->duration(), 3.0f));

    // Objects cannot animate before they exist.
    CHECK(scene->seek(4.0f) == Result::Success);
    CHECK(scene->add(late) == Result::Success);
    CHECK(scene->seek(3.5f) == Result::Success);
    CHECK(scene->play(Animation::fadeIn(late), 0.5f) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(near(scene->duration(), 4.0f));

    // Camera moves overlap object clips and add up with each other, lens changes included.
    auto camera = scene->config().camera;
    camera.orthoHeight = 2.0f;
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    camera.orthoHeight = 3.0f;
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    camera.fov = 0.5f;
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    CHECK(near(scene->duration(), 4.0f));

    CHECK(scene->mark("beat") == Result::Success);
    CHECK(scene->mark("beat") == Result::InvalidArguments);
    CHECK(scene->mark("") == Result::InvalidArguments);
    CHECK(scene->mark(nullptr) == Result::InvalidArguments);
    auto time = 0.0f;
    CHECK(scene->marker("beat", time) && near(time, 2.0f));
    CHECK(!scene->marker("missing", time));
    CHECK(scene->markerCount() == 1u);
    auto name = scene->markerAt(0u, time);
    CHECK(name && std::strcmp(name, "beat") == 0 && near(time, 2.0f));
    CHECK(scene->markerAt(1u, time) == nullptr);
    CHECK(scene->seek(-1.0f) == Result::InvalidArguments);
    CHECK(scene->seek(std::numeric_limits<float>::infinity()) == Result::InvalidArguments);
    delete scene;
}

// Mean column of the red pixels, or -1 when none are red.
static float _redColumn(const Surface& surface)
{
    auto sum = 0.0;
    auto count = 0u;
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            auto value = surface.data()[static_cast<size_t>(y) * surface.stride() + x];
            if ((value & 0xffu) > 160u && ((value >> 8) & 0xffu) < 90u) {
                sum += x;
                count++;
            }
        }
    }
    return count ? static_cast<float>(sum / count) : -1.0f;
}

// Each object samples the clips of its own list, in the order they were stored, whatever the
// clips of other objects stored between them: forty points switch on one by one, each group of
// ten stored latest first, while a counting text counts up in clips stored among theirs.
void clipLists(Renderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto counter = Text::gen("0");
    CHECK(scene && counter);
    if (!scene || !counter) {
        delete scene;
        delete counter;
        return;
    }
    constexpr auto Count = 40u;
    Point* points[Count] = {};
    CHECK(counter->count(0.0f, NumberFormat{}) == Result::Success);
    CHECK(scene->add(counter) == Result::Success);
    for (auto i = 0u; i < Count; i++) {
        points[i] = Point::gen({-3.0f + 0.15f * static_cast<float>(i), -1.0f, 0.0f});
        CHECK(points[i] != nullptr);
        if (!points[i]) {
            delete scene;
            return;
        }
        points[i]->opacity = 0.0f;
        CHECK(scene->add(points[i]) == Result::Success);
    }
    CHECK(scene->wait(4.0f) == Result::Success);
    for (auto step = 0u; step < Count / 10u; step++) {
        for (auto j = 10u; j-- > 0u;) {
            auto i = step * 10u + j;
            CHECK(scene->seek(0.1f * static_cast<float>(i)) == Result::Success);
            CHECK(scene->update(AnimationTarget::from(points[i]).opacity(1.0f)) == Result::Success);
        }
        CHECK(scene->seek(0.1f * static_cast<float>(step * 10u + 9u)) == Result::Success);
        auto value = static_cast<float>((step + 1u) * 10u);
        CHECK(scene->play(AnimationTarget::from(counter).value(value), 0.05f,
                          AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
    }
    CHECK(scene->clipCount() == Count + Count / 10u);
    char shown[16];
    const struct { float time; const char* text; } counts[] = {
        {0.0f, "0"}, {0.85f, "0"}, {0.96f, "10"}, {1.5f, "10"}, {2.0f, "20"}, {3.0f, "30"}, {4.0f, "40"}};
    for (auto& expected : counts) {
        CHECK(scene->text(counter, expected.time, shown, sizeof(shown)) > 0u);
        CHECK(std::strcmp(shown, expected.text) == 0);
    }
    for (auto i = 0u; i < Count; i++) {
        auto on = 0.1f * static_cast<float>(i);
        BBox bounds;
        CHECK(renderer->bounds(scene, points[i], on + 0.01f, bounds) == Result::Success);
        if (i > 0u) CHECK(renderer->bounds(scene, points[i], on - 0.06f, bounds) == Result::InsufficientCondition);
    }
    delete scene;
}

// Placing every object reads each clip once for its object and once for every object below
// it, whether those objects joined before the clip or after it; the scenes of viewports and
// transitions add theirs.
void sampleWork()
{
    auto scene = Scene::gen();
    auto inset = Scene::gen();
    auto from = Scene::gen();
    auto to = Scene::gen();
    auto staged = Scene::gen();
    auto group = Group::gen();
    auto inner = Group::gen();
    auto first = Circle::gen({}, 0.5f);
    auto second = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    auto third = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto apart = Rectangle::gen({0.0f, 2.0f, 0.0f}, {1.0f, 1.0f});
    auto later = Circle::gen({3.0f, 0.0f, 0.0f}, 0.5f);
    auto shown = Circle::gen({}, 0.25f);
    auto leaving = Circle::gen({}, 0.25f);
    auto coming = Circle::gen({}, 0.25f);
    Scene* scenes[] = {scene, inset, from, to, staged};
    Object* objects[] = {group, inner, first, second, third, apart, later, shown, leaving, coming};
    auto made = true;
    for (auto item : scenes) made = made && item;
    for (auto item : objects) made = made && item;
    CHECK(made);
    if (!made) {
        for (auto item : scenes) delete item;
        for (auto item : objects) delete item;
        return;
    }
    CHECK(inner->add(second) == Result::Success);
    CHECK(inner->add(third) == Result::Success);
    CHECK(group->add(first) == Result::Success);
    CHECK(group->add(inner) == Result::Success);
    CHECK(scene->add(group) == Result::Success);
    CHECK(scene->add(apart) == Result::Success);
    CHECK(scene->sampleWork() == 0u);
    // A move of the group is read for it and the four objects below it, a turn of the inner
    // group for it and its two circles.
    CHECK(scene->play(Animation::shift(group, {1.0f, 0.0f, 0.0f}), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->sampleWork() == 5u);
    CHECK(scene->play(Animation::rotate(inner, 1.0f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->play(Animation::fade(apart, 0.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->clipCount() == 3u && scene->sampleWork() == 9u);
    // A circle that joins the inner group later is read for the clip of each group above it.
    CHECK(inner->add(later) == Result::Success);
    CHECK(scene->sampleWork() == 11u);
    CHECK(scene->play(Animation::shift(later, {0.0f, 1.0f, 0.0f}), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->play(Animation::shift(group, {-1.0f, 0.0f, 0.0f}), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->clipCount() == 5u && scene->sampleWork() == 18u);
    CHECK(inset->add(shown) == Result::Success);
    CHECK(inset->play(Animation::fade(shown, 0.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->viewport(inset, {0.5f, 0.0f, 0.5f, 1.0f}) == Result::Success);
    CHECK(scene->sampleWork() == 19u);
    CHECK(from->add(leaving) == Result::Success);
    CHECK(from->play(Animation::fade(leaving, 0.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(to->add(coming) == Result::Success);
    CHECK(to->play(Animation::shift(coming, {1.0f, 0.0f, 0.0f}), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(to->play(Animation::fade(coming, 0.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    Scene* stages[] = {from, to};
    CHECK(staged->transition(stages, 2u, {}, 1.0f, 0.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(staged->sampleWork() == 3u);
    delete scene;
    delete staged;

    // A morph from a copy that fails takes the copy back, and with it the work the clip of the
    // group above it read for it.
    auto copying = Scene::gen();
    auto holder = Group::gen();
    auto source = Circle::gen({}, 0.5f);
    auto stale = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(copying && holder && source && stale);
    if (!copying || !holder || !source || !stale) {
        delete copying;
        delete holder;
        delete source;
        delete stale;
        return;
    }
    CHECK(holder->add(source) == Result::Success && copying->add(holder) == Result::Success);
    CHECK(copying->add(stale) == Result::Success);
    CHECK(copying->play(Animation::shift(holder, {1.0f, 0.0f, 0.0f}), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(copying->sampleWork() == 2u);
    auto held = copying->count();
    auto morph = Animation::morph(source, stale);
    morph.copy = true;
    CHECK(copying->play(morph, 1.0f) != Result::Success);
    CHECK(copying->count() == held && copying->sampleWork() == 2u);
    CHECK(copying->play(Animation::shift(holder, {-1.0f, 0.0f, 0.0f}), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(copying->sampleWork() == 4u);
    delete copying;
}

// An orbit keeps every angle: a full turn passes behind the scene and returns, where a look
// to the same pose would not move at all.
void cameraOrbit(Renderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 64;
    config.height = 64;
    config.background = {0, 0, 0, 255};
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {0.0f, 0.0f, 10.0f};
    auto scene = Scene::gen(config);
    auto dot = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(scene && dot);
    if (!scene || !dot) {
        delete dot;
        delete scene;
        return;
    }
    dot->stroke({255, 0, 0, 255}).fill({255, 0, 0, 255});
    dot->style.radius = 3.0f;
    CHECK(scene->add(dot) == Result::Success);
    CHECK(scene->orbit(2.0f * PI, {0.0f, 1.0f, 0.0f}, 4.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(near(scene->duration(), 4.0f));
    Surface surface;
    auto start = -1.0f;
    if (renderer->render(scene, 0.0f, surface) == Result::Success) start = _redColumn(surface);
    CHECK(start > 36.0f);
    // A quarter turn counterclockwise about +y puts the eye on +x, in line with the dot.
    if (renderer->render(scene, 1.0f, surface) == Result::Success) CHECK(std::fabs(_redColumn(surface) - 31.5f) < 1.0f);
    // Half way the eye is behind the target, so the dot is on the left.
    if (renderer->render(scene, 2.0f, surface) == Result::Success) {
        auto column = _redColumn(surface);
        CHECK(column >= 0.0f && column < 27.0f);
    }
    if (renderer->render(scene, 4.0f, surface) == Result::Success) CHECK(std::fabs(_redColumn(surface) - start) < 0.5f);

    // A look that changes the lens adds to the orbit: the field of view narrows while the eye
    // keeps circling, and the next move starts where both end. A perspective camera puts a point
    // x right of its view at depth d at column 31.5 + 32 x / (d tan(fov / 2)).
    CHECK(scene->seek(3.0f) == Result::Success);
    auto lens = scene->config().camera;
    lens.fov = 0.5f;
    CHECK(scene->look(lens, CameraView::ThreeD, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    auto column = [](float x, float depth, float fov) { return 31.5f + 32.0f * x / (depth * std::tan(0.5f * fov)); };
    // At 3.5 s the eye is an eighth turn short of +z, which puts the dot sqrt(2) right of the
    // view and sqrt(2) beyond the target.
    auto diagonal = std::sqrt(2.0f);
    CHECK(renderer->render(scene, 3.5f, surface) == Result::Success);
    CHECK(std::fabs(_redColumn(surface) - column(diagonal, 10.0f + diagonal, 0.5f * (0.7853982f + 0.5f))) < 1.0f);
    CHECK(renderer->render(scene, 4.0f, surface) == Result::Success);
    CHECK(std::fabs(_redColumn(surface) - column(2.0f, 10.0f, 0.5f)) < 1.0f);
    CHECK(scene->seek(4.0f) == Result::Success);
    // Over the pole: the up direction turns with the eye, so the pose stays valid.
    CHECK(scene->orbit(PI, {1.0f, 0.0f, 0.0f}, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    if (renderer->render(scene, 4.5f, surface) == Result::Success) CHECK(_redColumn(surface) > 36.0f);
    if (renderer->render(scene, 5.0f, surface) == Result::Success) CHECK(_redColumn(surface) > 36.0f);
    CHECK(near(scene->config().camera.eye.z, -10.0f, 1.0e-3f) && near(scene->config().camera.up.y, -1.0f, 1.0e-4f));

    CHECK(scene->orbit(0.0f, {0.0f, 1.0f, 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(scene->orbit(std::numeric_limits<float>::quiet_NaN(), {0.0f, 1.0f, 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->orbit(PI, {0.0f, 0.0f, 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->orbit(PI, {0.0f, std::numeric_limits<float>::infinity(), 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->orbit(PI, {0.0f, 1.0f, 0.0f}, 0.0f) == Result::InvalidArguments);
    CHECK(near(scene->duration(), 5.0f));
    delete scene;

    auto planar = Scene::gen(Config{});
    CHECK(planar);
    if (planar) {
        CHECK(planar->orbit(PI, {0.0f, 0.0f, 1.0f}, 1.0f) == Result::InsufficientCondition);
        CHECK(planar->diagnostic() != nullptr);
    }
    delete planar;
}

void intentMotion(Renderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto pivoted = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto straight = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto swept = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto sheared = Rectangle::gen({0.0f, 0.0f, 0.0f}, {2.0f, 0.1f});
    auto arcing = Rectangle::gen({1.0f, 1.0f, 0.0f}, {0.1f, 0.1f});
    auto ring = Circle::gen({0.0f, 0.0f, 0.0f}, 1.0f);
    auto car = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.4f, 0.2f});
    auto back = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.1f, 0.1f});
    auto sun = Circle::gen({-3.0f, -1.0f, 0.0f}, 0.1f);
    auto planet = Circle::gen({-2.5f, -1.0f, 0.0f}, 0.05f);
    auto label = Text::gen("label", {0.0f, 1.5f, 0.0f});
    CHECK(scene && pivoted && straight && swept && sheared && arcing && ring && car && back && sun && planet
          && label);
    if (!scene || !pivoted || !straight || !swept || !sheared || !arcing || !ring || !car || !back || !sun
        || !planet || !label) {
        delete scene;
        delete pivoted;
        delete straight;
        delete swept;
        delete sheared;
        delete arcing;
        delete ring;
        delete car;
        delete back;
        delete sun;
        delete planet;
        delete label;
        return;
    }
    Object* objects[] = {pivoted, straight, swept, sheared, arcing, ring, car, back, sun, planet, label};
    for (auto object : objects) {
        // Fill only, so pixel bounds measure the geometry without stroke width.
        object->style.stroke.a = 0;
        object->fill(Color::hex("#ffffff"));
        CHECK(scene->add(object) == Result::Success);
    }

    // Rotating about a corner keeps the square's size and swings it on a circle. A transform
    // interpolates element-wise, so a quarter turn shrinks midway and a shear keeps the x-axis
    // in place, while an arc equal to the turn carries every point on a circle.
    auto shear = Mat4::identity();
    shear.e[1] = 1.0f;
    Animation group[] = {
        Animation::rotate(pivoted, PI * 0.5f, {-2.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}),
        Animation::transform(straight, _rotationAbout(PI * 0.5f, {2.0f, 0.0f, 0.0f})),
        Animation::shift(arcing, {-2.0f, 0.0f, 0.0f}),
        Animation::moveAlong(car, ring, true),
        Animation::moveAlong(back, ring),
        Animation::rotate(planet, PI * 2.0f, {0.0f, 0.0f, 1.0f}),
        Animation::transform(swept, _rotationAbout(PI * 0.5f, {2.0f, 0.0f, 0.0f})),
        Animation::transform(sheared, shear),
    };
    group[2].arc = PI;
    group[4].from = 1.0f;
    group[4].to = 0.5f;
    group[5].related = sun;
    group[6].arc = PI * 0.5f;
    CHECK(scene->play(group, 8u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);

    BBox bounds;
    CHECK(renderer->bounds(scene, pivoted, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.width, 28.28f, 0.6f) && near(bounds.height, 28.28f, 0.6f));
    CHECK(near(bounds.center().x, 30.0f, 0.6f) && near(bounds.center().y, 35.86f, 0.6f));
    CHECK(renderer->bounds(scene, pivoted, 1.0f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 20.0f, 0.6f) && near(bounds.center().y, 40.0f, 0.6f));
    CHECK(renderer->bounds(scene, straight, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.width, 20.0f, 0.6f) && near(bounds.center().x, 120.0f, 0.6f));
    CHECK(renderer->bounds(scene, swept, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.width, 28.28f, 0.6f) && near(bounds.center().x, 120.0f, 0.6f));
    CHECK(renderer->bounds(scene, swept, 0.25f, bounds) == Result::Success);
    CHECK(near(bounds.width, 20.0f * (std::cos(PI / 8.0f) + std::sin(PI / 8.0f)), 0.6f));
    CHECK(near(bounds.center().x, 120.0f, 0.6f) && near(bounds.center().y, 40.0f, 0.6f));
    CHECK(renderer->bounds(scene, sheared, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.height, 2.0f, 0.6f) && near(bounds.width, 41.0f, 0.6f));

    // A half-circle arc passes over the chord's midpoint at the top.
    CHECK(renderer->bounds(scene, arcing, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f, 0.6f) && near(bounds.center().y, 0.0f, 0.6f));

    // Following the circle: a quarter turn reaches the top and turns the car upright;
    // the second follower travels backwards over the last half of the outline.
    CHECK(renderer->bounds(scene, car, 0.25f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f, 0.6f) && near(bounds.center().y, 20.0f, 0.6f));
    CHECK(near(bounds.width, 4.0f, 0.6f) && near(bounds.height, 8.0f, 0.6f));
    CHECK(renderer->bounds(scene, car, 1.0f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 100.0f, 0.6f) && near(bounds.width, 8.0f, 0.6f));
    CHECK(renderer->bounds(scene, back, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f, 0.6f) && near(bounds.center().y, 60.0f, 0.6f));

    // Turning about another object's center is an orbit.
    CHECK(renderer->bounds(scene, planet, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 10.0f, 0.6f) && near(bounds.center().y, 60.0f, 0.6f));

    // The outline is copied when played: moving the path afterwards keeps the old route.
    CHECK(scene->play(Animation::moveAlong(back, ring), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->play(Animation::shift(ring, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(renderer->bounds(scene, back, 1.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 60.0f, 0.6f) && near(bounds.center().y, 40.0f, 0.6f));

    // Scale about a corner keeps that corner in place.
    CHECK(scene->play(Animation::rescale(pivoted, {2.0f, 2.0f, 1.0f}, {-3.5f, -0.5f, 0.0f}), 1.0f)
          == Result::Success);
    CHECK(renderer->bounds(scene, pivoted, scene->duration(), bounds) == Result::Success);
    CHECK(near(bounds.x, 10.0f, 0.6f) && near(bounds.width, 40.0f, 0.6f));

    // Several followers share one path and the path may animate in the same group.
    Animation shared[] = {Animation::moveAlong(arcing, ring), Animation::moveAlong(car, ring, true),
                          Animation::fade(ring, 0.5f)};
    CHECK(scene->play(shared, 3u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), 0.2f) == Result::Success);

    auto connector = Connector::gen(sun, planet);
    CHECK(connector && scene->add(connector) == Result::Success);
    CHECK(scene->play(Animation::moveAlong(car, label), 1.0f) == Result::InvalidArguments);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(scene->play(Animation::moveAlong(car, car), 1.0f) == Result::InvalidArguments);
    CHECK(scene->play(Animation::rotate(car, 1.0f, {0.0f, 0.0f, 0.0f}), 1.0f) == Result::InvalidArguments);
    CHECK(scene->play(Animation::rotate(connector, 1.0f), 1.0f) == Result::InvalidArguments);
    auto wide = Animation::shift(car, {1.0f, 0.0f, 0.0f});
    wide.arc = 7.0f;
    CHECK(scene->play(wide, 1.0f) == Result::InvalidArguments);
    auto partial = Animation::moveAlong(car, ring);
    partial.to = 1.5f;
    CHECK(scene->play(partial, 1.0f) == Result::InvalidArguments);
    CHECK(scene->play(AnimationTarget::from(car).shift({0.0f, 1.0f, 0.0f}).arc(PI), 1.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(car).shift({0.0f, 1.0f, 0.0f}).arc(-7.0f), 1.0f)
          == Result::InvalidArguments);
    delete scene;
}

void timelineDiagnostics()
{
    const Vec3 corners[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    auto scene = Scene::gen({});
    auto triangle = Polygon::gen(corners, 3u);
    auto square = Polygon::gen(corners, 4u);
    auto group = Group::gen();
    auto circle = Circle::gen({0.0f, 0.0f, 0.0f}, 1.0f);
    CHECK(scene && triangle && square && group && circle);
    if (!scene || !triangle || !square || !group || !circle) {
        delete scene;
        delete triangle;
        delete square;
        delete group;
        delete circle;
        return;
    }
    CHECK(scene->add(triangle) == Result::Success);
    CHECK(scene->add(square) == Result::Success);
    CHECK(scene->add(group) == Result::Success);
    CHECK(scene->add(circle) == Result::Success);

    // Layout explains missing bounds; the scene keeps the reason for the failed call.
    CHECK(group->moveTo({1.0f, 0.0f, 0.0f}) == Result::InsufficientCondition);
    CHECK(_says(scene, "no bounds yet"));
    CHECK(scene->play(Animation::morph(triangle, group), 1.0f) == Result::NonSupport);
    CHECK(_says(scene, "morph needs a line, circle, rectangle"));
    CHECK(scene->play(AnimationTarget::from(circle).opacity(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "opacity must be between 0 and 1"));
    // A fade clamps its opacity into range, but not one that is not a number, which play() refuses.
    auto nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene->play(Animation::fade(circle, nan), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "opacity must be between 0 and 1"));
    CHECK(scene->play(Animation::fade(circle, 0.5f), -1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "positive number of seconds"));
    Animation twice[] = {Animation::fade(circle, 0.5f), Animation::shift(circle, {1.0f, 0.0f, 0.0f})};
    CHECK(scene->play(twice, 2u, 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "only once in one call"));
    CHECK(scene->play(AnimationTarget::from(circle).tip(4.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "tail and tip need"));

    // A successful call keeps working; edits after the timeline starts are explained.
    CHECK(scene->play(Animation::fade(circle, 0.5f), 1.0f) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    CHECK(circle->moveTo({2.0f, 0.0f, 0.0f}) == Result::InsufficientCondition);
    CHECK(_says(scene, "must come before the first play"));
    CHECK(scene->remove(circle) == Result::Success);
    CHECK(scene->play(Animation::fade(circle, 1.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "removed earlier"));
    CHECK(scene->remove(circle) == Result::InsufficientCondition);
    CHECK(_says(scene, "already removed"));
    delete scene;
}

// A view draws the scene again through its own camera into its rectangle: magnified where its
// camera looks, with optical pixel sizes, clipped at its edges, between the layers it divides,
// and as it moves, fades and follows.
void viewRendering(Renderer* renderer)
{
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto black = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto yellow = Color::hex("#ffff00");
    auto green = Color::hex("#00ff00");
    auto blue = Color::hex("#0000ff");
    auto white = Color::hex("#ffffff");
    auto magenta = Color::hex("#ff00ff");
    auto none = Color{0, 0, 0, 0};
    // 10 pixels per unit with the origin at pixel (100, 50); the view fills the bottom right
    // quarter, (100, 50) to (200, 100), and looks at (2, 1) 1.25 units high: 40 pixels per unit,
    // four times the scene's camera, with (2, 1) at pixel (150, 75).
    auto make = [&](bool optical, Object** dot = nullptr) -> Scene* {
        Config config;
        config.width = 200;
        config.height = 100;
        config.camera.orthoHeight = 10.0f;
        config.antialiasing = false;
        config.background = black;
        auto scene = Scene::gen(config);
        if (!scene) return nullptr;
        auto point = Point::gen({2.0f, 1.0f, 0.0f});
        auto rail = Line::gen({3.0f, -5.0f, 0.0f}, {3.0f, 5.0f, 0.0f});
        auto beam = Line::gen({-10.0f, 1.5f, 0.0f}, {10.0f, 1.5f, 0.0f});
        auto under = Rectangle::gen({5.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
        auto ghost = Circle::gen({1.75f, 0.75f, 0.0f}, 0.2f);
        auto cover = Rectangle::gen({9.0f, -4.5f, 0.0f}, {2.0f, 1.0f});
        auto badge = Rectangle::gen({2.25f, 1.0f, 0.0f}, {0.1f, 0.1f});
        Object* objects[] = {point, rail, beam, under, ghost, cover, badge};
        auto created = true;
        for (auto object : objects) created = created && object;
        if (!created) {
            for (auto object : objects) delete object;
            delete scene;
            return nullptr;
        }
        point->stroke(red, 2.0f).fill(red);
        point->style.radius = 2.0f;
        rail->stroke(yellow, 2.0f);
        beam->stroke(green, 2.0f);
        under->stroke(none).fill(white);
        ghost->stroke(magenta).fill(magenta);
        cover->stroke(none).fill(white);
        cover->layer = 1;
        badge->stroke(none).fill(white);
        badge->layer = 1;
        for (auto object : objects) CHECK(scene->add(object) == Result::Success);
        SceneView view;
        view.viewport = {0.5f, 0.5f, 0.5f, 0.5f};
        view.camera.eye = {2.0f, 1.0f, 10.0f};
        view.camera.target = {2.0f, 1.0f, 0.0f};
        view.camera.orthoHeight = 1.25f;
        view.layer = 0;
        view.optical = optical;
        view.background = blue;
        const Object* hidden[] = {ghost};
        view.hidden = hidden;
        view.hiddenCount = 1u;
        CHECK(scene->view(view) == Result::Success);
        if (dot) *dot = point;
        return scene;
    };

    Object* dot = nullptr;
    auto scene = make(true, &dot);
    CHECK(scene != nullptr);
    if (!scene) return;
    Surface surface;
    float column;
    float row;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    // The scene itself, outside the view and below it.
    CHECK(_pixelAt(surface, 120u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 130u, 20u) == pixel(yellow));
    CHECK(_pixelAt(surface, 117u, 42u) == pixel(magenta));
    CHECK(_pixelAt(surface, 50u, 75u) == pixel(black));
    // The dot, 3 pixels in radius with its stroke, sits at the view's center 12 in radius.
    auto dotPixels = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
    CHECK(dotPixels > 420u && dotPixels < 540u);
    CHECK(std::fabs(column - 149.5f) < 1.0f && std::fabs(row - 74.5f) < 1.0f);
    CHECK(_pixelAt(surface, 165u, 75u) == pixel(blue));
    // The rail at x = 3 runs down the view at column 190, 8 pixels thick instead of 2.
    auto railPixels = _viewPixels(surface, yellow, 150u, 60u, 200u, 61u, column, row);
    CHECK(railPixels >= 7u && railPixels <= 9u && std::fabs(column - 189.5f) < 1.0f);
    // The beam at y = 1.5 crosses the whole view along row 55, 8 pixels thick, and stops at its
    // left edge.
    CHECK(_pixelAt(surface, 101u, 55u) == pixel(green));
    CHECK(_pixelAt(surface, 198u, 55u) == pixel(green));
    CHECK(_pixelAt(surface, 98u, 55u) == pixel(black));
    CHECK(_viewPixels(surface, green, 101u, 50u, 102u, 100u, column, row) == 8u && near(row, 54.5f));
    // The cover on layer 1 draws over the view, the badge on layer 1 stays out of it, the hidden
    // circle is left out and the view's background hides the white square on layer 0 below it.
    CHECK(_pixelAt(surface, 190u, 95u) == pixel(white));
    CHECK(_pixelAt(surface, 140u, 85u) == pixel(blue));
    CHECK(_pixelAt(surface, 150u, 88u) == pixel(blue));
    CHECK(_viewPixels(surface, white, 100u, 50u, 180u, 90u, column, row) == 0u);
    CHECK(_viewPixels(surface, magenta, 100u, 50u, 200u, 100u, column, row) == 0u);

    // The layout report lists the view as a scene of its own that covers what lies below it.
    LayoutReport report;
    CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
    CHECK(report.sceneCount() == 2u);
    auto shown = report.sceneAt(1u);
    CHECK(shown && shown->kind == LayoutSceneKind::View && shown->scene == scene);
    CHECK(shown && shown->path && std::strcmp(shown->path, "root/view:0") == 0);
    CHECK(shown && near(shown->bounds.x, 100.0f) && near(shown->bounds.width, 100.0f) && near(shown->bounds.height, 50.0f));
    CHECK(shown && near(shown->scaleX, 4.0f, 1.0e-3f) && near(shown->scaleY, 4.0f, 1.0e-3f) && shown->parent == 0u);
    CHECK(report.objectCount() == scene->count());
    for (auto i = 0u; i < report.visualCount(); i++) {
        auto visual = report.visualAt(i);
        auto object = report.objectAt(visual->object)->object;
        if (object->type() != Type::Rectangle) continue;
        auto& size = static_cast<const Rectangle*>(object)->size;
        // The square below the view is hidden by it; the cover above it is not.
        if (size.x == 1.0f) CHECK(visual->occluded && visual->occluder == 1u);
        if (size.x == 2.0f) CHECK(!visual->occluded);
    }

    // A view that is not optical keeps the pixel sizes.
    auto plain = make(false);
    CHECK(plain != nullptr);
    if (plain) {
        CHECK(renderer->render(plain, 0.0f, surface) == Result::Success);
        CHECK(_viewPixels(surface, yellow, 150u, 60u, 200u, 61u, column, row) == 2u);
        dotPixels = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
        CHECK(dotPixels > 24u && dotPixels < 48u && std::fabs(column - 149.5f) < 1.0f);
        CHECK(_viewPixels(surface, green, 101u, 50u, 102u, 100u, column, row) == 2u);
        CHECK(renderer->layout(plain, 0.0f, report) == Result::Success);
        CHECK(report.sceneCount() == 2u && near(report.sceneAt(1u)->scaleX, 1.0f));
    }
    delete plain;

    // The view's camera pans to (1.5, 1) and widens to 2.5 units over a second: the dot ends
    // half a unit right of the center, 20 pixels per unit, twice its size in the scene.
    auto wide = Camera{};
    wide.eye = {1.5f, 1.0f, 10.0f};
    wide.target = {1.5f, 1.0f, 0.0f};
    wide.orthoHeight = 2.5f;
    CHECK(scene->look(0u, wide, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(scene->fade(0u, 0.5f, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    auto halfway = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
    CHECK(halfway > 160u && halfway < 420u && column > 150.5f && column < 159.5f);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    dotPixels = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
    CHECK(std::fabs(column - 159.5f) < 1.0f && std::fabs(row - 74.5f) < 1.0f);
    CHECK(dotPixels > 100u && dotPixels < 160u);
    CHECK(_viewPixels(surface, yellow, 150u, 60u, 200u, 61u, column, row) == 4u);
    CHECK(std::fabs(column - 179.5f) < 1.0f);
    // Then it fades to half: its background lies half over the black scene.
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    auto mixed = _pixelAt(surface, 110u, 95u);
    CHECK((mixed & 0xffffu) == 0u && std::abs(static_cast<int>((mixed >> 16) & 0xffu) - 128) <= 2);
    CHECK(_pixelAt(surface, 190u, 95u) == pixel(white));

    // Seeking a retained renderer back and forth draws what a fresh one draws.
    float times[] = {2.0f, 0.0f, 0.5f, 1.5f, 1.0f, 0.0f};
    uint64_t expected[6] = {};
    for (auto i = 0u; i < 6u; i++) {
        auto fresh = Renderer::gen();
        CHECK(fresh != nullptr);
        if (fresh && fresh->render(scene, times[i], surface) == Result::Success) expected[i] = checksum(surface);
        delete fresh;
    }
    for (auto i = 0u; i < 6u; i++) {
        CHECK(renderer->render(scene, times[i], surface) == Result::Success);
        CHECK(checksum(surface) == expected[i]);
    }
    delete scene;

    // A view made at 0.5 s that follows a moving dot keeps it at its center.
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    config.background = black;
    auto followed = Scene::gen(config);
    auto mover = Point::gen({-4.0f, 0.0f, 0.0f});
    CHECK(followed && mover);
    if (followed && mover) {
        mover->stroke(red, 2.0f).fill(red);
        mover->style.radius = 2.0f;
        CHECK(followed->add(mover) == Result::Success);
        CHECK(followed->play(AnimationTarget::from(mover).shift({8.0f, 0.0f, 0.0f}), 2.0f, linear) == Result::Success);
        CHECK(followed->seek(0.5f) == Result::Success);
        SceneView inset;
        inset.viewport = {0.0f, 0.0f, 0.25f, 0.5f};
        inset.camera.orthoHeight = 2.5f;
        inset.background = blue;
        inset.follow = mover;
        CHECK(followed->view(inset) == Result::Success);
        CHECK(followed->seek(2.0f) == Result::Success);
        CHECK(followed->remove(mover) == Result::Success);
        CHECK(renderer->render(followed, 0.25f, surface) == Result::Success);
        CHECK(_viewPixels(surface, blue, 0u, 0u, 200u, 100u, column, row) == 0u);
        float moments[] = {0.5f, 1.0f, 1.75f};
        for (auto time : moments) {
            CHECK(renderer->render(followed, time, surface) == Result::Success);
            dotPixels = _viewPixels(surface, red, 0u, 0u, 50u, 50u, column, row);
            CHECK(dotPixels > 100u && dotPixels < 160u);
            CHECK(std::fabs(column - 24.5f) < 1.0f && std::fabs(row - 24.5f) < 1.0f);
        }
        // Once the dot is gone the view holds the camera it was given.
        CHECK(renderer->render(followed, 2.5f, surface) == Result::Success);
        CHECK(_pixelAt(surface, 25u, 25u) == pixel(blue));
    } else {
        delete mover;
    }
    delete followed;

    // A perspective view orbits on its own: a quarter turn about +y puts its eye on +x, in line
    // with a dot at (2, 0, 0), which then sits at the center of the view on the left half while
    // the scene's camera still shows it right of the frame's center.
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {0.0f, 0.0f, 10.0f};
    auto spatial = Scene::gen(config);
    auto ball = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(spatial && ball);
    if (spatial && ball) {
        ball->stroke(red).fill(red);
        ball->style.radius = 3.0f;
        CHECK(spatial->add(ball) == Result::Success);
        SceneView orbiting;
        orbiting.viewport = {0.0f, 0.0f, 0.5f, 1.0f};
        orbiting.camera = config.camera;
        orbiting.cameraView = CameraView::ThreeD;
        orbiting.background = blue;
        uint32_t index = 0u;
        CHECK(spatial->view(orbiting, &index) == Result::Success);
        CHECK(spatial->orbit(index, 1.5707964f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::Success);
        CHECK(renderer->render(spatial, 0.0f, surface) == Result::Success);
        _viewPixels(surface, red, 0u, 0u, 100u, 100u, column, row);
        CHECK(column > 65.0f);
        CHECK(renderer->render(spatial, 1.0f, surface) == Result::Success);
        _viewPixels(surface, red, 0u, 0u, 100u, 100u, column, row);
        CHECK(std::fabs(column - 49.5f) < 1.0f);
        _viewPixels(surface, red, 100u, 0u, 200u, 100u, column, row);
        CHECK(column > 115.0f);
    } else {
        delete ball;
    }
    delete spatial;
}

// Views of Scene::view(): each keeps its own camera timeline and fades, made and moved with the
// checks of the scene's camera, and none of it touches the scene's own camera.
void viewTimelines()
{
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen();
    auto dot = Point::gen({1.0f, 0.5f, 0.0f});
    auto overlay = Group::gen();
    auto foreign = Point::gen({});
    CHECK(scene && dot && overlay && foreign);
    if (!scene || !dot || !overlay || !foreign) {
        delete scene;
        delete dot;
        delete overlay;
        delete foreign;
        return;
    }
    CHECK(scene->add(dot) == Result::Success);
    CHECK(scene->add(overlay) == Result::Success);
    auto opening = scene->config().camera;

    SceneView view;
    view.viewport = {0.5f, 0.0f, 0.5f, 0.5f};
    view.camera = opening;
    view.camera.orthoHeight = 2.0f;
    view.follow = dot;
    view.opacity = 0.0f;
    {
        const Object* hidden[] = {overlay};
        view.hidden = hidden;
        view.hiddenCount = 1u;
        uint32_t index = 99u;
        CHECK(scene->view(view, &index) == Result::Success);
        CHECK(index == 0u);
    }
    CHECK(scene->viewCount() == 1u);
    SceneView made;
    CHECK(scene->viewAt(0u, made));
    CHECK(!scene->viewAt(1u, made));
    // The view keeps its own copy of the hidden list.
    CHECK(made.hiddenCount == 1u && made.hidden && made.hidden[0] == overlay);
    CHECK(made.viewport.x == 0.5f && made.viewport.height == 0.5f);
    CHECK(made.layer == INT32_MAX && made.optical && made.follow == dot);
    CHECK(made.opacity == 0.0f && made.camera.orthoHeight == 2.0f);
    // Making a view is no timeline call: the scene may still change its objects' starting states.
    CHECK(scene->update(dot) == Result::Success);
    CHECK(near(scene->cursor(), 0.0f));

    // Rejected views say why and leave the scene as it was.
    auto rejected = view;
    rejected.hidden = nullptr;
    rejected.hiddenCount = 0u;
    rejected.viewport = {0.75f, 0.0f, 0.5f, 0.5f};
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    CHECK(scene->diagnostic() != nullptr);
    rejected.viewport = {0.0f, 0.0f, 0.0f, 0.5f};
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.viewport = view.viewport;
    rejected.camera.projection = Projection::Perspective;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.camera = view.camera;
    rejected.opacity = 1.5f;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.opacity = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.opacity = 1.0f;
    rejected.follow = foreign;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "part of this scene"));
    rejected.follow = nullptr;
    rejected.hiddenCount = 1u;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    {
        const Object* hidden[] = {foreign};
        rejected.hidden = hidden;
        CHECK(scene->view(rejected) == Result::InvalidArguments);
    }
    rejected.hidden = nullptr;
    rejected.hiddenCount = 257u;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    CHECK(scene->viewCount() == 1u);

    // A look of the view moves its camera alone and advances the cursor like any camera move.
    auto closer = view.camera;
    closer.orthoHeight = 1.0f;
    CHECK(scene->look(0u, closer, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(near(scene->cursor(), 1.0f));
    CHECK(scene->viewAt(0u, made) && made.camera.orthoHeight == 1.0f);
    CHECK(scene->config().camera.orthoHeight == opening.orthoHeight);
    CHECK(scene->update(dot) == Result::InsufficientCondition);
    // A second move of the same view may begin while the first runs; the scene's own camera
    // starts its timeline afresh.
    CHECK(scene->seek(0.5f) == Result::Success);
    auto panned = closer;
    panned.target.x = panned.eye.x = 2.0f;
    CHECK(scene->look(0u, panned, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    auto wide = opening;
    wide.orthoHeight = 16.0f;
    CHECK(scene->look(wide, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(scene->config().camera.orthoHeight == 16.0f);
    CHECK(scene->viewAt(0u, made) && made.camera.target.x == 2.0f && made.camera.orthoHeight == 1.0f);
    CHECK(near(scene->duration(), 1.5f));

    // A 2D view does not orbit, and the camera checks of the scene apply to views.
    CHECK(scene->orbit(0u, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() != nullptr);
    auto tilted = closer;
    tilted.eye.x += 1.0f;
    CHECK(scene->look(0u, tilted, CameraView::TwoD, 1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->look(0u, closer, CameraView::TwoD, 0.0f, linear) == Result::InvalidArguments);
    CHECK(scene->look(1u, closer, CameraView::TwoD, 1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "no view"));
    CHECK(scene->orbit(3u, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::InvalidArguments);

    // Fades of a view follow one another.
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->fade(0u, 1.0f, 0.5f, linear) == Result::Success);
    CHECK(scene->viewAt(0u, made) && made.opacity == 1.0f);
    CHECK(scene->seek(1.75f) == Result::Success);
    CHECK(scene->fade(0u, 0.0f, 0.5f, linear) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "fades until 2.00"));
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->fade(0u, 1.5f, 0.5f, linear) == Result::InvalidArguments);
    CHECK(scene->fade(0u, 0.5f, -1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->fade(2u, 0.5f, 1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->fade(0u, 0.5f, 1.0f, linear) == Result::Success);
    CHECK(near(scene->duration(), 3.0f));

    // A view shows from the cursor at which it is made; it cannot move or fade before that.
    view.hidden = nullptr;
    view.hiddenCount = 0u;
    view.follow = nullptr;
    view.cameraView = CameraView::ThreeD;
    view.camera.projection = Projection::Perspective;
    view.camera.eye = {0.0f, 0.0f, 10.0f};
    uint32_t late = 0u;
    CHECK(scene->view(view, &late) == Result::Success);
    CHECK(late == 1u);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->fade(late, 1.0f, 0.5f, linear) == Result::InsufficientCondition);
    CHECK(scene->orbit(late, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "made after"));
    CHECK(scene->seek(3.0f) == Result::Success);
    CHECK(scene->orbit(late, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::Success);
    CHECK(scene->viewAt(late, made) && made.cameraView == CameraView::ThreeD && made.camera.eye.x > 0.0f);
    CHECK(near(scene->duration(), 4.0f));

    for (auto i = scene->viewCount(); i < 64u; i++) CHECK(scene->view(view) == Result::Success);
    CHECK(scene->view(view) == Result::InsufficientCondition);
    CHECK(scene->viewCount() == 64u);

    // An interactive scene camera leaves the views to their own moves.
    Config interactive;
    interactive.cameraMode = CameraMode::Interactive;
    auto steered = Scene::gen(interactive);
    CHECK(steered != nullptr);
    if (steered) {
        SceneView inset;
        CHECK(steered->view(inset) == Result::Success);
        auto zoomed = inset.camera;
        zoomed.orthoHeight = 2.0f;
        CHECK(steered->look(zoomed, CameraView::TwoD, 1.0f, linear) == Result::InsufficientCondition);
        CHECK(steered->look(0u, zoomed, CameraView::TwoD, 1.0f, linear) == Result::Success);
    }
    delete steered;

    // A scene shown in another one can no longer gain views.
    auto parent = Scene::gen();
    CHECK(parent != nullptr);
    if (parent) {
        CHECK(parent->viewport(scene, {0.0f, 0.0f, 0.5f, 0.5f}) == Result::Success);
        CHECK(scene->view(SceneView{}) == Result::InsufficientCondition);
        CHECK(scene->look(0u, closer, CameraView::TwoD, 1.0f, linear) == Result::InsufficientCondition);
        CHECK(scene->fade(0u, 1.0f, 1.0f, linear) == Result::InsufficientCondition);
        delete parent;
    } else {
        delete scene;
    }
    delete foreign;
}
