#include "testSceneChecks.h"

// Turns every point a quarter turn about the z axis.
static bool _turned(const Vec3& point, Vec3& image, void*) noexcept
{
    image = {-point.y, point.x, point.z};
    return true;
}

// A space draws a tick across an axis at every multiple of its step along it but the origin,
// centered on the axis, square to it and tickLength pixels long, in the color of the axis. A warp
// carries each tick to the image of its point and turns it square to the axis it bends, and
// leaves it out of the bounds of the space, as it leaves out the numbers.
void spaceTicks(Renderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto red = pixel(Color::hex("#ff0000"));
    auto green = pixel(Color::hex("#00ff00"));
    auto black = pixel(config.background);
    // A scene with a space 30 pixels to a unit, centered, with a red x axis, a green y axis and
    // no grid: the point (x, y) falls on column 120 + 30 x and row 60 - 30 y.
    auto make = [&](const Vec3& ticks, float length, Space*& space) -> Scene* {
        auto scene = Scene::gen(config);
        space = Space::gen({-3.0f, 3.0f, 1.0f}, {-1.5f, 1.5f, 0.5f}, {-1.0f, 1.0f, 1.0f});
        if (scene && space) {
            space->style.stroke.a = 0;
            space->axisX = Color::hex("#ff0000");
            space->axisY = Color::hex("#00ff00");
            space->ticks = ticks;
            space->tickLength = length;
            if (scene->add(space) == Result::Success) return scene;
        }
        delete space;
        delete scene;
        space = nullptr;
        return nullptr;
    };
    auto at = [](const Surface& surface, uint32_t x, uint32_t y) {
        return surface.data() ? surface.data()[static_cast<size_t>(y) * surface.stride() + x] : 0u;
    };
    auto image = [&](const Vec3& ticks, float length) {
        Space* space = nullptr;
        auto scene = make(ticks, length, space);
        Surface surface;
        auto sum = scene && renderer->render(scene, 0.0f, surface) == Result::Success ? checksum(surface) : 0u;
        delete scene;
        return sum;
    };

    // Axis width is independent of the grid, reaches ticks and warped axes, and grows
    // the drawn bounds while the space's coordinate bounds stay fixed.
    {
        Space* wide = nullptr;
        auto axes = make({1.0f, 0.5f, 0.0f}, 10.0f, wide);
        CHECK(axes && wide);
        if (axes && wide) {
            Surface frame;
            BBox thin, thick;
            CHECK(renderer->bounds(axes, wide, 0.0f, thin) == Result::Success);
            wide->axisWidth = 6.0f;
            CHECK(axes->update(wide) == Result::Success);
            CHECK(renderer->render(axes, 0.0f, frame) == Result::Success);
            CHECK(at(frame, 165u, 58u) == red && at(frame, 182u, 56u) == red);
            CHECK(renderer->bounds(axes, wide, 0.0f, thick) == Result::Success);
            CHECK(thick.width > thin.width + 3.0f && thick.height > thin.height + 3.0f);
            Bounds box;
            CHECK(wide->bounds(box) && near(box.max.x, 3.0f));
            Object* targets[] = {wide};
            auto up = Vec3{0.0f, 1.0f, 0.0f};
            CHECK(axes->warp(targets, 1u, _moved, &up, 1.0f, linear) == Result::Success);
            CHECK(renderer->render(axes, 1.0f, frame) == Result::Success);
            CHECK(at(frame, 165u, 28u) == red && at(frame, 182u, 26u) == red);
        }
        delete axes;
        axes = make({1.0f, 0.5f, 0.0f}, 10.0f, wide);
        wide->axisWidth = 0.0f;
        CHECK(axes->update(wide) == Result::Success);
        Surface hidden;
        CHECK(renderer->render(axes, 0.0f, hidden) == Result::Success);
        CHECK(changed(hidden, config.background) == 0u);
        delete axes;
        auto invalid = Space::gen();
        auto owner = Scene::gen();
        invalid->axisWidth = -2.0f;
        CHECK(owner->add(invalid) == Result::InvalidArguments);
        invalid->axisWidth = std::numeric_limits<float>::infinity();
        CHECK(owner->add(invalid) == Result::InvalidArguments);
        invalid->axisWidth = 0.0f;
        CHECK(owner->add(invalid) == Result::Success);
        delete owner;
    }

    Space* space = nullptr;
    auto scene = make({1.0f, 0.5f, 1.0f}, 10.0f, space);
    CHECK(scene != nullptr);
    if (!scene) return;
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    // The x ticks at -3 to 3, the ends of the axis among them, stand 10 pixels tall across row
    // 60, and none between them.
    const uint32_t columns[] = {30u, 60u, 90u, 150u, 180u, 210u};
    for (auto column : columns) {
        CHECK(at(surface, column, 57u) == red && at(surface, column, 63u) == red);
        CHECK(at(surface, column, 68u) == black && at(surface, column + 6u, 57u) == black);
    }
    // The y ticks at -1.5 to 1.5 lie 10 pixels wide across column 120.
    const uint32_t rows[] = {15u, 30u, 45u, 75u, 90u, 105u};
    for (auto row : rows) {
        CHECK(at(surface, 117u, row) == green && at(surface, 123u, row) == green);
        CHECK(at(surface, 112u, row) == black && at(surface, 117u, row + 7u) == black);
    }
    // None at the origin, where the green y axis shows, and none along z in the 2D view.
    CHECK(at(surface, 120u, 57u) == green && at(surface, 120u, 63u) == green);
    auto plain = image({}, 10.0f);
    CHECK(plain != 0u && plain != checksum(surface));
    CHECK(image({1.0f, 0.5f, 0.0f}, 0.0f) == plain);
    CHECK(image({0.0f, 0.0f, 1.0f}, 10.0f) == plain);

    // A warp carries each tick with its axis, here up a unit, so x = 2 crosses row 30, and the
    // bounds stay those of the same space without ticks.
    Space* bare = nullptr;
    auto unticked = make({}, 10.0f, bare);
    CHECK(unticked != nullptr);
    Object* targets[] = {space};
    Object* bareTargets[] = {bare};
    auto up = Vec3{0.0f, 1.0f, 0.0f};
    CHECK(scene->warp(targets, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(at(surface, 180u, 27u) == red && at(surface, 180u, 33u) == red && at(surface, 180u, 57u) == black);
    CHECK(at(surface, 117u, 45u) == green && at(surface, 117u, 105u) == black);
    if (unticked) {
        CHECK(unticked->warp(bareTargets, 1u, _moved, &up, 1.0f, linear) == Result::Success);
        Bounds ticked;
        Bounds plainBox;
        CHECK(space->bounds(ticked) && bare->bounds(plainBox));
        CHECK(near(ticked.min.x, plainBox.min.x) && near(ticked.max.x, plainBox.max.x) && near(ticked.max.x, 3.0f)
              && near(ticked.min.y, plainBox.min.y) && near(ticked.max.y, plainBox.max.y));
    }
    delete unticked;
    delete scene;

    // A quarter turn stands the x axis up along column 120, and its tick at x = 1 now lies
    // across it at row 30, where the y tick at y = 1 lay; that one stands across the turned y
    // axis at column 90, where the x tick at x = -1 stood.
    scene = make({1.0f, 0.5f, 0.0f}, 10.0f, space);
    CHECK(scene != nullptr);
    if (!scene) return;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(at(surface, 116u, 30u) == green && at(surface, 90u, 57u) == red);
    targets[0] = space;
    CHECK(scene->warp(targets, 1u, _turned, nullptr, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(at(surface, 116u, 30u) == red && at(surface, 124u, 30u) == red && at(surface, 120u, 26u) == red);
    CHECK(at(surface, 90u, 57u) == green && at(surface, 90u, 63u) == green && at(surface, 94u, 57u) == black);
    delete scene;

    // Ticks are lengths and steps: a negative or infinite one fails.
    auto invalid = Scene::gen(config);
    auto wrong = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    CHECK(invalid && wrong);
    if (invalid && wrong) {
        wrong->ticks.y = -0.5f;
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
        wrong->ticks = {std::numeric_limits<float>::infinity(), 0.0f, 0.0f};
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
        wrong->ticks = {1.0f, 1.0f, 0.0f};
        wrong->tickLength = -1.0f;
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
        wrong->tickLength = std::numeric_limits<float>::quiet_NaN();
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
    }
    delete wrong;
    delete invalid;
}

// A space draws a grid line and writes a number at each multiple of the step of a range, a whole
// count of the step as written. Added up as floats, the ninth step of 0.1 came to 0.9000001, which
// it wrote out in full, wider than 0.9, and a warp did the same; 9 * 0.1f would be 0.90000004. Now
// the numbers of a space from 0 to 0.9 end where those of one with the single step of 0.9 end,
// warped or not.
void spaceSteps(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 120;
    config.camera.orthoHeight = 2.0f;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    // The right edge of the space with step along x, and that of the same space once a warp that
    // moves nothing has taken it.
    auto edges = [&](float step, float& warped) {
        warped = -1.0f;
        auto scene = Scene::gen(config);
        auto space = Space::gen({0.0f, 0.9f, step}, {-0.5f, 0.5f, 1.0f}, {-1.0f, 1.0f, 1.0f});
        if (space) {
            space->numbers = true;
            space->numberSize = 12.0f;
        }
        if (!scene || !space || scene->add(space) != Result::Success) {
            delete space;
            delete scene;
            return -1.0f;
        }
        BBox box;
        auto plain = renderer->bounds(scene, space, 0.0f, box) == Result::Success ? box.x + box.width : -1.0f;
        Object* targets[] = {space};
        auto still = Vec3{};
        if (scene->warp(targets, 1u, _moved, &still, 1.0f, linear) == Result::Success
            && renderer->bounds(scene, space, 1.0f, box) == Result::Success) {
            warped = box.x + box.width;
        }
        delete scene;
        return plain;
    };
    float warped, single;
    auto stepped = edges(0.1f, warped);
    auto whole = edges(0.9f, single);
    CHECK(whole > 0.0f && near(stepped, whole) && single > 0.0f && near(warped, single));
}

// A space places a label beside the tick at a value: its bounds center on the line from the
// tick, below the x axis and left of the y axis unless told otherwise, with the side that faces
// the tick tickLength / 2 + gap pixels from the axis, through the transforms of the space and of
// the label's parent. A text without a box pins the side of its glyphs that faces the tick.
void spaceLabels()
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    // 30 pixels to a unit, so the default tick and gap put a label 8 pixels from its axis.
    constexpr auto away = 8.0f / 30.0f;
    auto centered = [](const Object* object, float x, float y) {
        Bounds box;
        if (!object->bounds(box)) return false;
        auto center = box.center();
        auto fits = near(center.x, x, 1.0e-4f) && near(center.y, y, 1.0e-4f);
        if (!fits) {
            std::printf("  centered at (%.5f, %.5f), expected (%.5f, %.5f)\n", static_cast<double>(center.x),
                        static_cast<double>(center.y), static_cast<double>(x), static_cast<double>(y));
        }
        return fits;
    };
    auto scene = Scene::gen(config);
    auto space = Space::gen({-3.0f, 3.0f, 1.0f}, {-1.5f, 1.5f, 0.5f});
    auto below = Text::gen("a");
    auto right = Text::gen("b");
    auto boxed = Text::gen("c");
    auto bar = Rectangle::gen({}, {1.0f, 0.5f});
    auto root = Rectangle::gen({5.0f, 5.0f, 0.0f}, {1.0f, 0.5f});
    CHECK(scene && space && below && right && boxed && bar && root);
    if (!scene || !space || !below || !right || !boxed || !bar || !root) {
        delete root;
        delete bar;
        delete boxed;
        delete right;
        delete below;
        delete space;
        delete scene;
        return;
    }
    boxed->box = {0.6f, 0.3f};
    CHECK(scene->add(space) == Result::Success);
    Object* children[] = {below, right, boxed, bar};
    for (auto child : children) CHECK(space->add(child) == Result::Success);
    CHECK(scene->add(root) == Result::Success);
    // A text hangs from its top below the x axis, and stands by its left side right of the y axis.
    CHECK(space->place(below, 0u, 2.0f) == Result::Success);
    CHECK(centered(below, 2.0f, -away) && near(below->align.x, 0.5f) && near(below->align.y, 0.0f));
    CHECK(space->place(right, 1u, 1.5f, {1.0f, 0.0f, 0.0f}) == Result::Success);
    CHECK(centered(right, away, 1.5f) && near(right->align.x, 0.0f) && near(right->align.y, 0.5f));
    // A text with a box and any other object place their bounds; the box keeps its align.
    CHECK(space->place(boxed, 1u, -1.0f) == Result::Success);
    CHECK(centered(boxed, -away - 0.3f, -1.0f) && near(boxed->align.x, 0.5f) && near(boxed->align.y, 0.5f));
    CHECK(space->place(bar, 0u, -1.0f, {0.0f, 2.0f, 0.0f}, 10.0f) == Result::Success);
    CHECK(centered(bar, -1.0f, 14.0f / 30.0f + 0.25f));
    // The space's transform carries the tick, here to (3, 0.5), and its scale does not stretch
    // the distance, which is in pixels: a label at the root moves in the world, and one below
    // the space in the space, where the 8 pixels are half as long.
    space->model = Mat4::translate({1.0f, 0.5f, 0.0f}) * Mat4::scale({2.0f, 2.0f, 1.0f});
    CHECK(scene->update(space) == Result::Success);
    CHECK(space->place(root, 0u, 1.0f) == Result::Success);
    CHECK(centered(root, 3.0f, 0.5f - away - 0.25f));
    CHECK(space->place(bar, 0u, 1.0f) == Result::Success);
    CHECK(centered(bar, 1.0f, -away * 0.5f - 0.25f));
    // A text not attached yet is placed in the space, to be added below it; one inside a parent
    // that is not attached yet is refused.
    auto fresh = Text::gen("d");
    CHECK(fresh && space->place(fresh, 0u, -2.0f) == Result::Success);
    if (fresh) {
        CHECK(centered(fresh, -2.0f, -away * 0.5f) && near(fresh->align.x, 0.5f) && near(fresh->align.y, 0.0f));
        CHECK(space->add(fresh) == Result::Success);
    }
    // A gap of zero and no tick length leave the label touching the axis.
    space->tickLength = 0.0f;
    CHECK(scene->update(space) == Result::Success);
    CHECK(space->place(bar, 1u, 1.0f, {}, 0.0f) == Result::Success);
    CHECK(centered(bar, -0.5f, 1.0f));

    // Refusals leave the label where it was.
    Bounds before;
    CHECK(root->bounds(before));
    auto refused = [&](Result result, Result expected, const char* reason) {
        auto said = scene->diagnostic() && std::strstr(scene->diagnostic(), reason);
        if (!said) std::printf("  diagnostic: %s\n", scene->diagnostic() ? scene->diagnostic() : "(none)");
        return result == expected && said;
    };
    CHECK(refused(space->place(root, 3u, 1.0f), Result::InvalidArguments, "axis must be 0, 1 or 2"));
    CHECK(refused(space->place(root, 0u, std::numeric_limits<float>::quiet_NaN()), Result::InvalidArguments,
                  "value finite"));
    CHECK(refused(space->place(root, 0u, 1.0f, {}, -1.0f), Result::InvalidArguments, "gap"));
    CHECK(refused(space->place(root, 0u, 1.0f, {0.0f, std::numeric_limits<float>::infinity(), 0.0f}),
                  Result::InvalidArguments, "direction"));
    CHECK(refused(space->place(space, 0u, 1.0f), Result::InvalidArguments, "not the space or one above it"));
    CHECK(refused(space->place(nullptr, 0u, 1.0f), Result::InvalidArguments, "label must be another object"));
    auto holding = Group::gen();
    auto held = Text::gen("e");
    CHECK(holding && held && holding->add(held) == Result::Success);
    if (holding && held) CHECK(refused(space->place(held, 0u, 1.0f), Result::InvalidArguments, "no parent"));
    delete holding;
    auto empty = Group::gen();
    CHECK(empty && scene->add(empty) == Result::Success);
    if (empty) CHECK(refused(space->place(empty, 0u, 1.0f), Result::InsufficientCondition, "no bounds"));
    // A space squashed flat along y cannot set a label below its x axis.
    auto kept = space->model;
    space->model = Mat4::scale({1.0f, 0.0f, 1.0f});
    CHECK(refused(space->place(root, 0u, 1.0f), Result::InvalidArguments, "flatten"));
    space->model = kept;
    Bounds after;
    CHECK(root->bounds(after) && near(after.center().x, before.center().x) && near(after.center().y, before.center().y));
    // Once the timeline begins, an attached text can no longer be placed and keeps its align,
    // while one not attached yet still can.
    CHECK(scene->play(Animation::fadeIn(right), 0.5f) == Result::Success);
    CHECK(refused(space->place(right, 0u, 1.0f), Result::InsufficientCondition, "before the first animation"));
    CHECK(near(right->align.x, 0.0f) && near(right->align.y, 0.5f));
    auto late = Text::gen("f");
    CHECK(late && space->place(late, 1u, 1.0f) == Result::Success && space->add(late) == Result::Success);
    if (late) CHECK(centered(late, -2.0f / 30.0f, 1.0f) && near(late->align.x, 1.0f) && near(late->align.y, 0.5f));
    delete scene;

    // A space cannot place the group above it, nor an object of another scene, and a space
    // that is not attached cannot tell how long a pixel is.
    scene = Scene::gen(config);
    auto other = Scene::gen(config);
    auto holder = Group::gen();
    auto inner = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    auto foreign = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && other && holder && inner && foreign);
    if (scene && other && holder && inner && foreign) {
        CHECK(holder->add(inner) == Result::Success && scene->add(holder) == Result::Success);
        CHECK(other->add(foreign) == Result::Success);
        CHECK(refused(inner->place(holder, 0u, 1.0f), Result::InvalidArguments, "one above it"));
        CHECK(inner->place(foreign, 0u, 1.0f) == Result::InvalidArguments);
        auto detached = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
        auto mark = Text::gen("g");
        CHECK(detached && mark && detached->place(mark, 0u, 1.0f) == Result::InsufficientCondition);
        delete mark;
        delete detached;
    } else {
        delete foreign;
        delete inner;
        delete holder;
    }
    delete other;
    delete scene;
}

// A space passes its opacity and creation progress to the labels it placed below it, at any
// depth, as a group passes them to its children, also when it draws into a layer of its own;
// the other objects below it and the labels it placed elsewhere keep their own.
void spaceScreenLabels(Renderer* renderer)
{
    constexpr auto pi = 3.14159265359f;
    Config config;
    config.width = 960;
    config.height = 540;
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.fov = 42.0f * pi / 180.0f;
    config.camera.up = {0.0f, 0.0f, 1.0f};
    auto polar = 65.0f * pi / 180.0f;
    auto eye = [polar](float azimuth) {
        return Vec3{8.0f * std::sin(polar) * std::cos(azimuth),
                    8.0f * std::sin(polar) * std::sin(azimuth), 8.0f * std::cos(polar)};
    };
    config.camera.eye = eye(-pi / 3.0f);
    auto scene = Scene::gen(config);
    auto space = Space::gen({-3.0f, 3.0f, 1.0f}, {-3.0f, 3.0f, 1.0f}, {-2.0f, 2.0f, 1.0f});
    CHECK(scene && space && scene->add(space) == Result::Success);
    if (!scene || !space) { delete scene; return; }
    Text* labels[6];
    const char* names[] = {"x", "y", "z"};
    for (auto i = 0u; i < 6u; i++) {
        labels[i] = Text::gen(names[i / 2u], {0.4f, 0.2f, 0.0f});
        labels[i]->size = 18.0f;
        labels[i]->slant = 0.25f;
        CHECK(space->placeScreen(labels[i], static_cast<uint8_t>(i / 2u),
                                 (i % 2u ? 1.0f : -1.0f) * (i < 4u ? 3.0f : 2.0f), 10.0f) == Result::Success);
        CHECK(space->add(labels[i]) == Result::Success);
    }
    auto centered = Text::gen("x");
    centered->size = 18.0f;
    CHECK(space->add(centered) == Result::Success);
    CHECK(space->placeScreen(centered, 0u, 0.0f, 10.0f) == Result::Success);
    LayoutReport initial;
    CHECK(renderer->layout(scene, 0.0f, initial) == Result::Success);
    auto centerVisual = _layoutVisual(initial, centered);
    CHECK(centerVisual && near(centerVisual->paintBounds.y + centerVisual->paintBounds.height, 256.0f, 0.02f));
    // World placement removes the retained screen placement, leaving the ordinary layout.
    auto ordinary = Text::gen("x");
    ordinary->size = 18.0f;
    CHECK(space->add(ordinary) == Result::Success);
    CHECK(space->place(centered, 0u, 0.0f) == Result::Success);
    CHECK(space->place(ordinary, 0u, 0.0f) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, initial) == Result::Success);
    centerVisual = _layoutVisual(initial, centered);
    auto ordinaryVisual = _layoutVisual(initial, ordinary);
    CHECK(centerVisual && ordinaryVisual && near(centerVisual->paintBounds.x, ordinaryVisual->paintBounds.x)
          && near(centerVisual->paintBounds.y, ordinaryVisual->paintBounds.y));
    auto root = Text::gen("root");
    CHECK(scene->add(root) == Result::Success);
    CHECK(space->placeScreen(root, 0u, 1.0f) == Result::InvalidArguments);
    CHECK(space->placeScreen(labels[0], 3u, 1.0f) == Result::InvalidArguments);
    CHECK(space->placeScreen(labels[0], 0u, 1.0f, -1.0f) == Result::InvalidArguments);
    auto other = Scene::gen(config);
    auto foreign = Text::gen("foreign");
    CHECK(other->add(foreign) == Result::Success);
    CHECK(space->placeScreen(foreign, 0u, 1.0f) == Result::InvalidArguments);
    delete other;
    auto plane = Text::gen("plane");
    plane->orientation = TextOrientation::Plane;
    CHECK(space->placeScreen(plane, 0u, 1.0f) == Result::InvalidArguments);
    delete plane;
    CHECK(scene->orbit(pi * 0.5f, {0.0f, 0.0f, 1.0f}, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(space->placeScreen(labels[0], 0u, 1.0f) == Result::InsufficientCondition);
    auto grow = AnimationTarget::from(labels[0]);
    grow.size(28.0f);
    CHECK(scene->play(&grow, 1u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    const float times[] = {0.0f, 1.25f, 0.2f, 2.0f, 2.5f, 3.0f, 1.25f};
    for (auto time : times) {
        auto camera = config.camera;
        camera.eye = eye(-pi / 3.0f + pi * 0.5f * std::fmin(time / 2.0f, 1.0f));
        for (auto i = 0u; i < 6u; i++) {
            Vec3 at;
            auto value = (i % 2u ? 1.0f : -1.0f) * (i < 4u ? 3.0f : 2.0f);
            if (i / 2u == 0u) at.x = value;
            else if (i / 2u == 1u) at.y = value;
            else at.z = value;
            Vec3 ndc;
            CHECK(camera.project(at, 960.0f / 540.0f, ndc));
            Vec2 anchor{(ndc.x + 1.0f) * 480.0f, (1.0f - ndc.y) * 270.0f};
            auto direction = anchor - Vec2{480.0f, 270.0f};
            direction = direction / direction.length();
            LayoutReport report;
            CHECK(renderer->layout(scene, time, report) == Result::Success);
            auto visual = _layoutVisual(report, labels[i]);
            CHECK(visual && visual->visible);
            if (!visual) continue;
            auto box = visual->paintBounds;
            auto center = box.center();
            auto support = std::fabs(direction.x) * box.width * 0.5f
                         + std::fabs(direction.y) * box.height * 0.5f;
            auto gap = (center.x - anchor.x) * direction.x + (center.y - anchor.y) * direction.y - support;
            CHECK(near(gap, 14.0f, 0.02f));
        }
    }
    Object* copy = nullptr;
    CHECK(scene->clone(labels[1], copy) == Result::Success && copy && copy->parent() == space);
    auto late = Text::gen("late");
    CHECK(space->placeScreen(late, 0u, 3.0f, 10.0f) == Result::Success);
    CHECK(space->add(late) == Result::Success);
    LayoutReport final;
    CHECK(renderer->layout(scene, 3.0f, final) == Result::Success);
    auto originalVisual = _layoutVisual(final, labels[1]);
    auto copyVisual = _layoutVisual(final, copy);
    auto lateVisual = _layoutVisual(final, late);
    CHECK(originalVisual && copyVisual && lateVisual && lateVisual->visible
          && near(originalVisual->paintBounds.x, copyVisual->paintBounds.x)
          && near(originalVisual->paintBounds.y, copyVisual->paintBounds.y));
    delete scene;
    // An origin behind the eye cannot supply an outward projection, but a visible anchor
    // still gets its gap in the deterministic upward fallback direction.
    config.camera.eye = {0.0f, 0.0f, 10.0f};
    config.camera.up = {0.0f, 1.0f, 0.0f};
    scene = Scene::gen(config);
    space = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {-3.0f, -1.0f, 1.0f});
    space->model = Mat4::translate({0.0f, 0.0f, 11.0f});
    auto clippedOrigin = Text::gen("x");
    clippedOrigin->size = 18.0f;
    CHECK(scene->add(space) == Result::Success);
    CHECK(space->placeScreen(clippedOrigin, 2u, -3.0f, 10.0f) == Result::Success);
    CHECK(space->add(clippedOrigin) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, initial) == Result::Success);
    centerVisual = _layoutVisual(initial, clippedOrigin);
    CHECK(centerVisual && centerVisual->visible
          && near(centerVisual->paintBounds.y + centerVisual->paintBounds.height, 256.0f, 0.02f));
    delete scene;
}

void spaceScreenLabelWarps(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    for (auto nested : {false, true}) {
        auto scene = Scene::gen(config);
        auto space = Space::gen({-2.0f, 2.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
        auto label = Text::gen("x");
        label->size = 18.0f;
        if (nested) {
            auto group = Group::gen();
            group->model = Mat4::translate({1.0f, 0.0f, 0.0f}) * Mat4::scale({2.0f, 2.0f, 1.0f});
            CHECK(group->add(space) == Result::Success && scene->add(group) == Result::Success);
        } else CHECK(scene->add(space) == Result::Success);
        CHECK(space->placeScreen(label, 0u, -2.0f, 10.0f) == Result::Success);
        CHECK(space->add(label) == Result::Success);
        Object* targets[] = {space};
        auto right = Vec3{3.0f, 0.0f, 0.0f};
        auto left = Vec3{-1.0f, 0.0f, 0.0f};
        CHECK(scene->warp(targets, 1u, _moved, &right, 1.0f, linear) == Result::Success);
        CHECK(scene->warp(targets, 1u, _moved, &left, 1.0f, linear) == Result::Success);
        CHECK(scene->warp(targets, 1u, _squared, nullptr, 1.0f, linear) == Result::Success);
        for (auto time : {0.0f, 1.0f, 0.75f, 0.25f, 2.0f, 1.5f, 3.0f, 2.5f, 1.0f}) {
            auto x = -2.0f + 3.0f * std::fmin(time, 1.0f)
                   - std::fmin(std::fmax(time - 1.0f, 0.0f), 1.0f);
            auto anchor = 160.0f + 30.0f * ((nested ? 1.0f : 0.0f) + (nested ? 2.0f : 1.0f) * x);
            LayoutReport report;
            CHECK(renderer->layout(scene, time, report) == Result::Success);
            auto visual = _layoutVisual(report, label);
            CHECK(visual && visual->visible);
            if (visual) {
                CHECK(near(visual->paintBounds.x + visual->paintBounds.width, anchor - 14.0f, 0.02f));
                CHECK(near(visual->paintBounds.center().y, 90.0f, 0.02f));
            }
        }
        auto late = Text::gen("late");
        CHECK(space->placeScreen(late, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "before the space's first warp"));
        CHECK(space->placeScreen(label, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
        CHECK(scene->seek(0.0f) == Result::Success);
        CHECK(space->placeScreen(late, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
        delete late;
        delete scene;
    }
    auto scene = Scene::gen(config);
    auto space = Space::gen({2.0f, 3.0f, 1.0f}, {2.0f, 3.0f, 1.0f}, {2.0f, 3.0f, 1.0f});
    auto label = Text::gen("x");
    label->size = 18.0f;
    CHECK(scene->add(space) == Result::Success);
    CHECK(space->placeScreen(label, 0u, 2.0f) == Result::Success && space->add(label) == Result::Success);
    auto radial = [](const Vec3& point, Vec3& image, void*) noexcept {
        image = {point.dot(point) + 3.0f, point.y, point.z};
        return true;
    };
    Object* targets[] = {space};
    CHECK(scene->warp(targets, 1u, radial, nullptr, 1.0f, linear) == Result::Success);
    Bounds bounds;
    CHECK(space->bounds(bounds) && near(bounds.min.x, 7.0f));
    delete scene;
    // A space warped without screen labels never captured their origin. World labels can
    // still be placed later, while screen placement must fail before changing the text.
    scene = Scene::gen(config);
    space = Space::gen({-2.0f, 2.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    CHECK(scene->add(space) == Result::Success);
    auto right = Vec3{3.0f, 0.0f, 0.0f};
    targets[0] = space;
    CHECK(scene->warp(targets, 1u, _moved, &right, 1.0f, linear) == Result::Success);
    auto late = Text::gen("late");
    CHECK(space->placeScreen(late, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
    CHECK(space->place(late, 0u, -2.0f) == Result::Success && space->add(late) == Result::Success);
    delete scene;
}

void spaceLabelOpacity(Renderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    // 30 pixels to a unit: the point (x, y) falls on column 120 + 30 x and row 60 - 30 y. The
    // red channel of a white fill shows how much of it is drawn.
    auto red = [](const Surface& surface, float x, float y) -> uint32_t {
        auto column = static_cast<size_t>(120.0f + 30.0f * x);
        auto row = static_cast<size_t>(60.0f - 30.0f * y);
        return surface.data() ? surface.data()[row * surface.stride() + column] & 0xffu : 0u;
    };
    auto half = [](uint32_t value) { return value >= 120u && value <= 136u; };
    // A label sits 8 pixels, 4 / 15 of a unit, from its axis, and its box half as far again.
    constexpr auto away = 8.0f / 30.0f + 0.2f;
    const bool layers[] = {false, true};
    for (auto isolated : layers) {
        auto scene = Scene::gen(config);
        auto space = Space::gen({-3.0f, 3.0f, 1.0f}, {-1.5f, 1.5f, 0.5f});
        auto holder = Group::gen();
        auto label = Rectangle::gen({}, {0.6f, 0.4f});
        auto deep = Rectangle::gen({}, {0.6f, 0.4f});
        auto fresh = Rectangle::gen({}, {0.6f, 0.4f});
        auto other = Rectangle::gen({-2.0f, 0.8f, 0.0f}, {0.6f, 0.4f});
        auto loose = Rectangle::gen({}, {0.6f, 0.4f});
        CHECK(scene && space && holder && label && deep && fresh && other && loose);
        if (!scene || !space || !holder || !label || !deep || !fresh || !other || !loose) {
            delete loose;
            delete other;
            delete fresh;
            delete deep;
            delete label;
            delete holder;
            delete space;
            delete scene;
            return;
        }
        // The space draws nothing of its own, so only the rectangles show.
        space->style.stroke.a = 0;
        space->axisX.a = 0;
        space->axisY.a = 0;
        Rectangle* rectangles[] = {label, deep, fresh, other, loose};
        for (auto rectangle : rectangles) {
            rectangle->style.stroke.a = 0;
            rectangle->fill(Color::hex("#ffffff"));
        }
        CHECK(scene->add(space) == Result::Success && space->add(label) == Result::Success);
        CHECK(space->add(holder) == Result::Success && holder->add(deep) == Result::Success);
        CHECK(space->add(other) == Result::Success && scene->add(loose) == Result::Success);
        if (isolated) CHECK(space->isolate() == Result::Success);
        // Labels below the space, one inside a group and one placed before it was attached, and
        // a label at the root, which stays the root's.
        CHECK(space->place(label, 0u, 2.0f) == Result::Success);
        CHECK(space->place(deep, 0u, 0.0f) == Result::Success);
        CHECK(space->place(fresh, 0u, -2.0f) == Result::Success && space->add(fresh) == Result::Success);
        CHECK(space->place(loose, 1u, 1.0f) == Result::Success);
        CHECK(scene->play(Animation::fadeIn(space), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        CHECK(scene->play(Animation::uncreate(space), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        Surface surface;
        // Halfway through the fade the labels below the space show at half strength.
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(half(red(surface, 2.0f, -away)) && half(red(surface, 0.0f, -away)) && half(red(surface, -2.0f, -away)));
        CHECK(red(surface, -2.0f, 0.8f) == 255u && red(surface, -away - 0.1f, 1.0f) == 255u);
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        CHECK(red(surface, 2.0f, -away) == 255u && red(surface, 0.0f, -away) == 255u);
        // Once the space is uncreated, its labels are gone with it, and the rest stays.
        CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
        CHECK(red(surface, 2.0f, -away) == 0u && red(surface, 0.0f, -away) == 0u && red(surface, -2.0f, -away) == 0u);
        CHECK(red(surface, -2.0f, 0.8f) == 255u && red(surface, -away - 0.1f, 1.0f) == 255u);
        delete scene;
    }
}

// The cursor reckons each wait and play from the decimals a script writes, so a wait of 0.5 and
// twenty-nine of 0.3 end on the float nearest 9.2, which frame 276 samples at 30 frames a second,
// and what starts there shows on that frame. Added up as floats they ended three ulps late, and it
// showed a frame late. A play of items that begin a lag apart moves the cursor as far.
void cursorDecimals(Renderer* renderer)
{
    Config config;
    config.width = 40;
    config.height = 20;
    config.fps = 30;
    config.camera.orthoHeight = 2.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto card = Rectangle::gen({}, {8.0f, 4.0f});
    Object* dots[] = {Circle::gen({-1.0f, 0.6f, 0.0f}, 0.1f), Circle::gen({0.0f, 0.6f, 0.0f}, 0.1f),
                      Circle::gen({1.0f, 0.6f, 0.0f}, 0.1f)};
    CHECK(scene && card && dots[0] && dots[1] && dots[2]);
    if (!scene || !card || !dots[0] || !dots[1] || !dots[2]) {
        for (auto dot : dots) delete dot;
        delete card;
        delete scene;
        return;
    }
    card->style.stroke.a = 0;
    card->fill(Color::hex("#ff0000"));
    CHECK(scene->add(card) == Result::Success);
    for (auto dot : dots) CHECK(scene->add(dot) == Result::Success);
    CHECK(scene->wait(0.5f) == Result::Success);
    for (auto i = 0; i < 29; i++) CHECK(scene->wait(0.3f) == Result::Success);
    CHECK(scene->cursor() == 9.2f);
    CHECK(scene->update(AnimationTarget::from(card).fill(Color::hex("#ffffff"))) == Result::Success);
    Surface surface;
    auto middle = [&](uint32_t frame) {
        auto time = static_cast<float>(frame) / static_cast<float>(config.fps);
        if (renderer->render(scene, time, surface) != Result::Success || !surface.data()) return 0u;
        return surface.data()[10u * surface.stride() + 20u];
    };
    CHECK(middle(275u) == pixel(Color::hex("#ff0000")));
    CHECK(middle(276u) == pixel(Color::hex("#ffffff")));
    // Three fades of 0.3 that begin 0.4 of it apart end 0.54 on.
    Animation fades[] = {Animation::fade(dots[0], 0.5f), Animation::fade(dots[1], 0.5f), Animation::fade(dots[2], 0.5f)};
    CHECK(scene->play(fades, 3u, 0.3f, AnimCurve::preset(AnimCurvePreset::Linear), 0.4f) == Result::Success);
    CHECK(scene->cursor() == 9.74f);
    delete scene;
}

// A match reckons when its pieces begin and end from the decimals a script writes, as a play does,
// so the last of them ends where the cursor goes: one of 0.3 from 0.05 ends at the float nearest
// 0.35. Added up as floats, its pieces ended an ulp past the cursor, and a fade of the target from
// there found the target still fading.
void matchDecimals()
{
    auto scene = Scene::gen();
    auto from = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
    auto to = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    CHECK(scene && from && to);
    if (!scene || !from || !to) {
        delete to;
        delete from;
        delete scene;
        return;
    }
    CHECK(scene->add(from) == Result::Success && scene->wait(0.05f) == Result::Success);
    CHECK(scene->add(to) == Result::Success && scene->matchTransform(from, to, 0.3f) == Result::Success);
    CHECK(scene->cursor() == 0.35f);
    CHECK(scene->play(Animation::fade(to, 0.5f), 0.5f) == Result::Success);
    delete scene;
}
