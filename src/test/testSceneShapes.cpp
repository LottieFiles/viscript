#include "testSceneChecks.h"

static const LayoutPath* anglePath(const LayoutReport& report, const Object* object)
{
    auto visual = _layoutVisual(report, object);
    for (auto i = 0u; i < report.pathCount(); i++) {
        auto path = report.pathAt(i);
        if (path && report.visualAt(path->visual) == visual) return path;
    }
    return nullptr;
}

void arcsAndAngles(Renderer* renderer)
{
    constexpr auto pi = 3.14159265359f;
    auto arc = Arc::gen({1.0f, 2.0f, 0.0f}, 2.0f, 0.0f, pi * 0.5f);
    CHECK(arc && Arc::gen({}, -1.0f, 0.0f, pi) == nullptr);
    CHECK(Arc::gen({}, 1.0f, 0.0f, 7.0f) == nullptr);
    Bounds bounds;
    if (arc) {
        CHECK(arc->style.fill.a == 0u && arc->bounds(bounds));
        CHECK(near(bounds.min.x, 1.0f) && near(bounds.max.x, 3.0f)
              && near(bounds.min.y, 2.0f) && near(bounds.max.y, 4.0f));
        arc->start = 0.2f; arc->angle = -pi;
        CHECK(arc->bounds(bounds) && near(bounds.min.y, 0.0f));
        arc->sector = true;
        CHECK(arc->bounds(bounds) && bounds.min.x <= arc->center.x && bounds.max.y >= arc->center.y);
        delete arc;
    }
    Config config;
    config.width = 200; config.height = 100; config.camera.orthoHeight = 10.0f;
    auto scene = Scene::gen(config);
    arc = Arc::gen({}, 2.0f, 0.0f, pi * 0.5f);
    auto angle = Angle::gen({}, {-1.0f, 0.2f, 0.0f}, {-1.0f, -0.2f, 0.0f}, 1.0f);
    auto label = Point::gen({});
    CHECK(scene && arc && angle && label);
    if (!scene || !arc || !angle || !label) { delete scene; delete arc; delete angle; delete label; return; }
    angle->right = RightAngle::Never; angle->labelDistance = 2.0f;
    CHECK(near(angle->labelPoint().x, -2.0f) && near(angle->labelPoint().y, 0.0f));
    CHECK(angle->add(label) == Result::Success);
    CHECK(scene->add(arc) == Result::Success && scene->add(angle) == Result::Success);
    Vec3 center[] = {{2.0f, 0.0f, 0.0f}};
    Vec3 rays[] = {{1.0f, 0.0f, 0.0f}, {0.0f, 0.2f, 0.0f}, {0.0f, -0.2f, 0.0f}};
    AnimationTarget changes[] = {AnimationTarget::from(arc).geometry(center, 1u).radius(4.0f).start(pi * 0.5f).angle(pi),
                                 AnimationTarget::from(angle).geometry(rays, 3u)};
    CHECK(scene->play(changes, 2u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    LayoutReport report;
    CHECK(renderer->layout(scene, 0.5f, report) == Result::Success);
    auto path = anglePath(report, arc);
    CHECK(path && path->count > 2u && !path->closed);
    if (path) {
        // Center (1, 0), radius 3, start pi/4, sweep 3pi/4 at the midpoint.
        CHECK(near(path->points[0].x, 110.0f + 30.0f / std::sqrt(2.0f), 0.001f));
        CHECK(near(path->points[0].y, 50.0f - 30.0f / std::sqrt(2.0f), 0.001f));
        CHECK(near(path->points[path->count - 1u].x, 80.0f, 0.001f));
        for (auto i = 0u; i < path->count; i++) {
            auto x = path->points[i].x - 110.0f; auto y = path->points[i].y - 50.0f;
            CHECK(near(std::hypot(x, y), 30.0f, 0.001f));
        }
    }
    path = anglePath(report, angle);
    CHECK(path && path->count > 3u);
    if (path) {
        for (auto i = 0u; i < path->count; i++) CHECK(path->points[i].x < 100.0f);
    }
    BBox labelBounds;
    CHECK(renderer->bounds(scene, label, 0.5f, labelBounds) == Result::Success);
    CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 85.0f, 0.001f));
    // Sampling backwards gives the original vertex and label rather than the stored final state.
    CHECK(renderer->bounds(scene, label, 0.0f, labelBounds) == Result::Success);
    CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 80.0f, 0.001f));
    Object* copy = nullptr;
    CHECK(scene->clone(angle, copy) == Result::Success && copy && copy->type() == Type::Angle);
    if (copy) {
        CHECK(copy->childCount() == 1u);
        CHECK(renderer->bounds(scene, copy->childAt(0), 1.0f, labelBounds) == Result::Success);
        CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 90.0f, 0.001f));
    }
    Object* arcs[] = {arc};
    CHECK(scene->save(arcs, 1u) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(arc).radius(2.0f).start(0.0f).angle(pi * 0.5f), 0.5f) == Result::Success);
    CHECK(scene->restore(arcs, 1u, 0.5f) == Result::Success);
    CHECK(near(arc->radius, 4.0f) && near(arc->start, pi * 0.5f) && near(arc->angle, pi));
    delete scene;

    const bool sides[] = {false, true};
    const float offsets[] = {-0.001f, 0.001f};
    for (auto other : sides) {
        for (auto side : offsets) {
            scene = Scene::gen(config);
            auto first = Vec3{-1.0f, side, 0.0f};
            auto second = Vec3{-1.0f, -0.2f, 0.0f};
            // Reverse one pair so side selection also handles a raw sweep over pi.
            angle = side < 0.0f ? Angle::gen({}, second, first) : Angle::gen({}, first, second);
            angle->right = RightAngle::Never; angle->other = other;
            CHECK(scene->add(angle) == Result::Success);
            CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
            path = anglePath(report, angle);
            CHECK(path && path->count > 2u);
            if (path) {
                auto crossesRight = false;
                for (auto i = 0u; i < path->count; i++) crossesRight |= path->points[i].x > 100.0f;
                CHECK(crossesRight == other);
            }
            delete scene;
        }
    }
    scene = Scene::gen(config);
    angle = Angle::gen({}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    CHECK(scene->add(angle) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
    path = anglePath(report, angle);
    CHECK(path && path->count == 3u && near(path->points[1].x, 110.0f) && near(path->points[1].y, 40.0f));
    delete scene;

    scene = Scene::gen(config);
    arc = Arc::gen({}, 1.0f, 0.0f, pi); arc->sector = true; arc->fill(Color::hex("#ffffff"));
    auto follower = Point::gen({});
    CHECK(scene->add(arc) == Result::Success && scene->add(follower) == Result::Success);
    CHECK(scene->play(Animation::create(arc), 0.5f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(scene->play(Animation::moveAlong(follower, arc), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(renderer->layout(scene, 1.5f, report) == Result::Success);
    path = anglePath(report, arc);
    CHECK(path && path->closed && path->filled);
    CHECK(renderer->bounds(scene, follower, 1.5f, labelBounds) == Result::Success);
    CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 100.0f, 0.001f));
    auto flash = Emphasis::passingFlash(arc); flash.window = 0.2f;
    CHECK(scene->emphasize(flash, 0.5f) == Result::Success);
    CHECK(renderer->layout(scene, 1.75f, report) == Result::Success);
    delete scene;
}

// Characters the font lacks are set in the bundled math or default font; one neither has is
// missing and takes no room.
void glyphFallback(Renderer* renderer)
{
    Config config; config.width = 400; config.height = 100; config.camera.orthoHeight = 100.0f;
    auto scene = Scene::gen(config);
    auto text = Text::gen("\xe2\x88\xa0" "APB = 35\xc2\xb0", {});
    text->size = 21.0f;
    CHECK(text->font("Pretendard") == Result::Success);
    CHECK(scene->add(text) == Result::Success);
    TextExtent full; TextExtent without; TextExtent symbols;
    CHECK(scene->measure(text, full) == Result::Success && full.missing == 0u);
    CHECK(scene->measure(text, "APB = 35\xc2\xb0", without) == Result::Success && full.width > without.width + 10.0f);
    CHECK(scene->measure(text, "\xe2\x88\xa0\xc2\xb0\xe2\x8a\xa5\xe2\x88\xa5\xe2\x96\xb3\xe2\x89\x85\xe2\x88\xbc\xcf\x80\xce\xb8", symbols)
          == Result::Success && symbols.missing == 0u && symbols.width > 100.0f);
    Surface surface; CHECK(surface.resize(400, 100) == Result::Success);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    auto first = checksum(surface);
    CHECK(scene->play(AnimationTarget::from(text).text("APB = 35\xc2\xb0"), 1.0f) == Result::Success);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success && checksum(surface) != first);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success && checksum(surface) == first);
#ifdef VIS_TEST_MONO_FONT
    CHECK(renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
    auto mono = Text::gen("a\xe2\x82\x90", {});
    mono->size = 21.0f;
    CHECK(mono->font("JetBrainsMono") == Result::Success);
    CHECK(scene->add(mono) == Result::Success);
    TextExtent subscript; TextExtent letter; TextExtent hangul;
    CHECK(scene->measure(mono, subscript) == Result::Success && subscript.missing == 0u && !subscript.fallback);
    CHECK(scene->measure(mono, "a", letter) == Result::Success && subscript.width > letter.width + 5.0f);
    CHECK(scene->measure(mono, "a\xed\x95\x9c", hangul) == Result::Success && hangul.missing == 1u
          && std::strcmp(hangul.firstMissing, "\xed\x95\x9c") == 0 && hangul.width == letter.width);
#endif
    delete scene;
}

void shapeProperties(Renderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto black = pixel(config.background);
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto yellow = Color::hex("#ffff00");
    auto white = Color::hex("#ffffff");
    auto none = Color{0, 0, 0, 0};

    // 20 pixels per unit with the origin at pixel (80, 40).
    auto scene = Scene::gen(config);
    auto circle = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto box = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto line = Line::gen({-3.5f, -1.75f, 0.0f}, {3.5f, -1.75f, 0.0f});
    auto dot = Point::gen({0.0f, 1.5f, 0.0f});
    Object* objects[] = {circle, box, line, dot};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    circle->stroke(red);
    circle->fill(red);
    box->stroke(none);
    box->fill(blue);
    line->stroke(yellow);
    dot->stroke(white);
    dot->fill(white);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);
    AnimationTarget targets[] = {
        AnimationTarget::from(circle).radius(1.5f),
        AnimationTarget::from(box).size(Vec2{3.0f, 2.0f}).corner(0.5f),
        AnimationTarget::from(line).strokeWidth(10.0f),
        AnimationTarget::from(dot).radius(9.0f),
    };
    CHECK(scene->play(targets, 4u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(near(circle->radius, 1.5f) && near(dot->style.radius, 9.0f) && near(line->style.width, 10.0f));
    CHECK(near(box->size.x, 3.0f) && near(box->size.y, 2.0f) && near(box->corner, 0.5f));

    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 55u, 40u) == black);
    CHECK(_pixelAt(surface, 138u, 40u) == black);
    CHECK(_pixelAt(surface, 80u, 71u) == black);
    CHECK(_pixelAt(surface, 87u, 10u) == black);
    // Halfway the circle's radius is 1, the rectangle is 2 wide and the line 6 pixels thick.
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 55u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 65u, 40u) == black);
    CHECK(_pixelAt(surface, 138u, 40u) == pixel(blue));
    CHECK(_pixelAt(surface, 80u, 71u) == black);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 65u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 80u, 71u) == pixel(yellow));
    CHECK(_pixelAt(surface, 87u, 10u) == pixel(white));
    // The rounded corner leaves the rectangle's corner pixel empty.
    CHECK(_pixelAt(surface, 146u, 24u) == pixel(blue));
    CHECK(_pixelAt(surface, 149u, 21u) == black);
    // Size and corner are separate tracks, so they may overlap in time.
    auto mark = scene->cursor();
    CHECK(scene->play(AnimationTarget::from(box).size(Vec2{2.0f, 2.0f}), 1.0f) == Result::Success);
    CHECK(scene->seek(mark) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(box).corner(0.25f), 1.0f) == Result::Success);
    CHECK(scene->seek(mark + 0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(box).size(Vec2{1.0f, 1.0f}), 1.0f) == Result::InsufficientCondition);
    CHECK(scene->seek(mark + 1.0f) == Result::Success);

    // Values that cannot reach the shape are explained.
    CHECK(scene->play(AnimationTarget::from(line).radius(1.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "radius needs a circle"));
    CHECK(scene->play(AnimationTarget::from(circle).size(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "size needs a rectangle"));
    CHECK(scene->play(AnimationTarget::from(box).corner(1.5f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "half the rectangle's shorter side"));
    CHECK(scene->play(AnimationTarget::from(box).size(Vec2{0.4f, 2.0f}), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "half the rectangle's shorter side"));
    CHECK(scene->play(AnimationTarget::from(line).strokeWidth(-1.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "stroke width"));
    CHECK(scene->play(AnimationTarget::from(box).gradient(white), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keep the current gradient's type"));
    delete scene;

    // A connector meets the sampled edge of a growing shape, not its final size.
    auto linked = Scene::gen(config);
    auto source = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto sink = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto link = source && sink ? Connector::gen(source, sink) : nullptr;
    CHECK(linked && source && sink && link);
    if (linked && source && sink && link) {
        source->stroke(none);
        sink->stroke(none);
        link->stroke(white);
        link->strokeWidth(4.0f);
        CHECK(linked->add(source) == Result::Success);
        CHECK(linked->add(sink) == Result::Success);
        CHECK(linked->add(link) == Result::Success);
        CHECK(linked->play(AnimationTarget::from(source).radius(1.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        CHECK(renderer->render(linked, 0.0f, surface) == Result::Success);
        CHECK(_pixelAt(surface, 60u, 40u) == pixel(white));
        CHECK(renderer->render(linked, 1.0f, surface) == Result::Success);
        CHECK(_pixelAt(surface, 60u, 40u) == black);
        CHECK(_pixelAt(surface, 80u, 40u) == pixel(white));
    } else {
        delete link;
        delete source;
        delete sink;
    }
    delete linked;

    // Text size, gradient stops and a group's width.
    auto styled = Scene::gen(config);
    auto bar = Rectangle::gen({0.0f, 1.0f, 0.0f}, {6.0f, 1.0f});
    auto ramp = Rectangle::gen({0.0f, -0.5f, 0.0f}, {6.0f, 0.5f});
    auto group = Group::gen();
    auto heir = Line::gen({-3.0f, -1.5f, 0.0f}, {3.0f, -1.5f, 0.0f});
    auto own = Line::gen({-3.0f, -1.8f, 0.0f}, {3.0f, -1.8f, 0.0f});
    auto label = Text::gen("size", {0.0f, 10.0f, 0.0f});
    Object* parts[] = {bar, ramp, group, heir, own, label};
    created = styled != nullptr;
    for (auto object : parts) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : parts) delete object;
        delete styled;
        return;
    }
    Gradient stops;
    stops.type = GradientType::Linear;
    stops.stopCount = 2;
    stops.stops[0] = {0.0f, red};
    stops.stops[1] = {1.0f, blue};
    bar->stroke(none);
    bar->fill(white);
    CHECK(bar->gradient(stops) == Result::Success);
    ramp->stroke(none);
    ramp->fill(red);
    ramp->gradient(blue);
    own->strokeWidth(1.0f);
    CHECK(group->add(heir) == Result::Success);
    CHECK(group->add(own) == Result::Success);
    label->size = 20.0f;
    Object* roots[] = {bar, ramp, group, label};
    for (auto object : roots) CHECK(styled->add(object) == Result::Success);
    auto shifted = stops;
    shifted.stops[0].color = Color::hex("#00ff00");
    shifted.stops[1].color = yellow;
    AnimationTarget looks[] = {
        AnimationTarget::from(bar).gradient(shifted),
        AnimationTarget::from(ramp).gradient(Color::hex("#00ff00")),
        AnimationTarget::from(group).strokeWidth(8.0f),
        AnimationTarget::from(label).size(60.0f),
    };
    CHECK(styled->play(looks, 4u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(styled->diagnostic() == nullptr);
    CHECK(same(bar->style.gradient.stops[1].color, yellow) && same(ramp->style.gradientEnd, Color::hex("#00ff00")));
    CHECK(near(heir->style.width, 8.0f) && near(own->style.width, 1.0f) && near(label->size, 60.0f));
    CHECK(renderer->render(styled, 0.5f, surface) == Result::Success);
    // Red and green meet halfway at the left end of the bar.
    auto left = _pixelAt(surface, 21u, 20u);
    CHECK((left & 0xffu) > 110u && (left & 0xffu) < 145u);
    CHECK(((left >> 8u) & 0xffu) > 110u && ((left >> 8u) & 0xffu) < 145u && ((left >> 16u) & 0xffu) < 20u);
    BBox small;
    BBox large;
    CHECK(renderer->bounds(styled, label, 0.0f, small) == Result::Success);
    CHECK(renderer->bounds(styled, label, 1.0f, large) == Result::Success);
    CHECK(large.width > small.width * 2.5f && large.height > small.height * 2.5f);

    // A gradient with stops owns the colors, so only their alpha may change.
    CHECK(styled->play(AnimationTarget::from(bar).fill(red), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "a gradient with stops sets this shape's colors"));
    CHECK(styled->play(Animation::stroke(bar, red), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "a gradient with stops sets this shape's colors"));
    CHECK(styled->play(AnimationTarget::from(bar).gradient(red), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "an end color animates a two-stop gradient"));
    auto three = shifted;
    three.stopCount = 3;
    three.stops[2] = {1.0f, white};
    CHECK(styled->play(AnimationTarget::from(bar).gradient(three), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "stop count"));
    CHECK(styled->play(AnimationTarget::from(label).strokeWidth(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "no stroke width"));
    CHECK(styled->play(AnimationTarget::from(label).size(Vec2{10.0f, 20.0f}), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "one positive size"));
    CHECK(styled->play(AnimationTarget::from(bar).fill(Color{255, 255, 255, 128}), 1.0f) == Result::Success);
    delete styled;
}

// Geometry clips carry the points that define a shape point by point, and leave the shape with its
// new points for whatever is authored next.
void trimmedSegments(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    auto scene = Scene::gen(config);
    auto line = Line::gen({-4.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f});
    auto arrow = Arrow::gen({-4.0f, 2.0f, 0.0f}, {4.0f, 2.0f, 0.0f});
    auto vector = Vector::gen({8.0f, 0.0f, 0.0f}, {-4.0f, -2.0f, 0.0f});
    CHECK(scene && line && arrow && vector);
    if (!scene || !line || !arrow || !vector) {
        delete scene;
        delete line;
        delete arrow;
        delete vector;
        return;
    }
    line->trimDistance = {1.0f, 2.0f};
    arrow->trimDistance = vector->trimDistance = {1.0f, 1.0f};
    Bounds own;
    CHECK(line->bounds(own) && near(own.min.x, -3.0f) && near(own.max.x, 2.0f));
    CHECK(arrow->bounds(own) && near(own.min.x, -3.0f) && near(own.max.x, 3.0f));
    CHECK(vector->bounds(own) && near(own.min.x, -3.0f) && near(own.max.x, 3.0f));
    Object* objects[] = {line, arrow, vector};
    for (auto object : objects) {
        object->stroke(Color::hex("#ffffff"));
        object->strokeWidth(2.0f);
        CHECK(scene->add(object) == Result::Success);
    }
    Object* lines[] = {line};
    CHECK(scene->save(lines, 1u) == Result::Success);
    Vec3 raised[] = {{-4.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(line).geometry(raised, 2u), 1.0f,
                      AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(line).trimDistance(Vec2{2.0f, 1.0f}), 1.0f,
                      AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    BBox box;
    CHECK(renderer->bounds(scene, line, 0.5f, box) == Result::Success);
    auto length = std::sqrt(73.0f);
    CHECK(near(box.x, 59.0f + 120.0f / length, 0.01f));
    CHECK(near(box.width, 82.0f - 240.0f / length, 0.01f));
    CHECK(near(box.height, 32.0f - 90.0f / length, 0.01f));
    Object* copy = nullptr;
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->clone(line, copy) == Result::Success);
    if (copy) {
        auto twin = static_cast<Line*>(copy);
        CHECK(near(twin->trimDistance.x, 1.5f) && near(twin->trimDistance.y, 1.5f));
        CHECK(near(twin->to.y, 3.0f));
    }
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->restore(lines, 1u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(near(line->trimDistance.x, 1.0f) && near(line->trimDistance.y, 2.0f));
    CHECK(scene->update(AnimationTarget::from(line).trimDistance(Vec2{-1.0f, -1.0f})) == Result::InvalidArguments);
    CHECK(scene->update(AnimationTarget::from(line).trimDistance(Vec2{20.0f, 20.0f})) == Result::Success);
    CHECK(!line->bounds(own));
    CHECK(renderer->bounds(scene, line, 2.0f, box) == Result::InsufficientCondition);
    delete scene;

    // Equality and over-trimming suppress round caps and arrow markers as well as the shaft.
    for (auto kind = 0; kind < 3; kind++) {
        auto empty = Scene::gen(config);
        Object* object = kind == 0 ? static_cast<Object*>(Line::gen({-1, 0, 0}, {1, 0, 0}))
                       : kind == 1 ? static_cast<Object*>(Arrow::gen({-1, 0, 0}, {1, 0, 0}))
                                   : static_cast<Object*>(Vector::gen({2, 0, 0}, {-1, 0, 0}));
        CHECK(empty && object);
        if (!empty || !object) { delete empty; delete object; continue; }
        CHECK(empty->add(object) == Result::Success);
        auto trim = kind == 0 ? 1.0f : 3.0f;
        CHECK(empty->update(AnimationTarget::from(object).trimDistance(Vec2{trim, trim})) == Result::Success);
        Surface surface;
        CHECK(renderer->render(empty, 0.0f, surface) == Result::Success);
        auto black = pixel(config.background);
        for (auto i = 0u; i < surface.width() * surface.height(); i++) CHECK(surface.data()[i] == black);
        delete empty;
    }
}

void zeroLengthArrows(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 5.0f;
    config.background = Color::hex("#000000");

    for (auto kind = 0; kind < 2; kind++) {
        for (auto degenerate = 0; degenerate < 4; degenerate++) {
            auto scene = Scene::gen(config);
            auto length = degenerate == 0 ? 0.0f : degenerate == 1 ? 0.000004f : 2.0f;
            Object* object = kind == 0 ? static_cast<Object*>(Arrow::gen({}, {length, 0, 0}))
                                      : static_cast<Object*>(Vector::gen({length, 0, 0}));
            CHECK(scene && object);
            if (!scene || !object) { delete scene; delete object; continue; }
            object->stroke(Color::hex("#ffffff"), 4.0f);
            CHECK(scene->add(object) == Result::Success);
            if (degenerate >= 2) {
                auto trim = degenerate == 2 ? 1.0f : 2.0f;
                CHECK(scene->update(AnimationTarget::from(object).trimDistance(Vec2{trim, trim})) == Result::Success);
            }
            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
            CHECK(report.objectCount() == 1u);
            if (report.objectCount() == 1u) {
                auto own = report.objectAt(0);
                CHECK(!own->visible && !own->familyVisible);
                CHECK(own->paintBounds.width == 0.0f && own->paintBounds.height == 0.0f);
            }
            BBox bounds;
            CHECK(renderer->bounds(scene, object, 0.0f, bounds) == Result::InsufficientCondition);
            delete scene;
        }
    }

    // Subpixel arrows can still paint at fractional origins and in mounted scenes.
    for (auto height : {5.0f, 9.0f}) {
        config.camera.orthoHeight = height;
        auto scale = static_cast<float>(config.height) / height;
        for (auto offset : {-0.51f, 0.0f, 0.51f}) {
            for (auto length : {0.00008f, 0.015624f, 0.03125f}) {
                for (auto direction : {0.0f, 1.0f}) {
                    for (auto mode : {0, 1, 2}) {
                        auto scene = Scene::gen(config);
                        auto arrow = Arrow::gen({offset / scale, offset / scale, 0},
                                               {(offset + length) / scale, (offset + length * direction) / scale, 0});
                        CHECK(scene && arrow);
                        if (!scene || !arrow) { delete scene; delete arrow; continue; }
                        arrow->stroke(Color::hex("#ffffff"), 4.0f);
                        CHECK(scene->add(arrow) == Result::Success);
                        Surface surface;
                        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
                        auto nativePainted = changed(surface, config.background) > 0u;
                        if (mode == 1) {
                            SceneView view;
                            view.viewport = {0.5f, 0.0f, 0.5f, 0.5f};
                            view.camera.orthoHeight = height * 0.25f;
                            CHECK(scene->view(view) == Result::Success);
                        } else if (mode == 2) {
                            auto parent = Scene::gen(config);
                            CHECK(parent);
                            if (!parent) { delete scene; continue; }
                            CHECK(parent->viewport(scene, {0.5f, 0.0f, 0.5f, 0.5f}) == Result::Success);
                            scene = parent;
                        }
                        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
                        LayoutReport report;
                        CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
                        auto own = report.objectAt(0);
                        // SceneView redraws have scene records rather than own object bounds.
                        auto painted = mode == 1 ? nativePainted : changed(surface, config.background) > 0u;
                        CHECK(own && own->visible == painted);
                        delete scene;
                    }
                }
            }
        }
    }
    config.camera.orthoHeight = 5.0f;

    // A transformed vector crosses zero with a float residual while its label keeps drawing.
    auto scene = Scene::gen(config);
    auto vector = Vector::gen({2, 1, 0});
    auto label = Text::gen("v", {0, -1, 0});
    CHECK(scene && vector && label);
    if (!scene || !vector || !label) { delete scene; delete vector; delete label; return; }
    label->font("Pretendard");
    vector->model = Mat4::translate({-1, 0, 0}) * Mat4::rotateZ(0.5f);
    vector->stroke(Color::hex("#ff0000"), 4.0f);
    CHECK(vector->label(label) == Result::Success);
    CHECK(scene->add(vector) == Result::Success);
    const Vec3 reversed[] = {{-1.999992f, -1, 0}, {0, 0, 0}};
    CHECK(scene->play(AnimationTarget::from(vector).geometry(reversed, 2u), 2.0f,
                      AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    for (auto time : {0.99f, 1.0f, 1.01f}) {
        LayoutReport report;
        CHECK(renderer->layout(scene, time, report) == Result::Success);
        auto own = report.objectAt(0);
        auto child = report.objectAt(1);
        CHECK(own && child && own->object == vector && child->object == label);
        if (!own || !child) continue;
        CHECK(own->visible == (time != 1.0f));
        CHECK(own->familyVisible && child->visible);
        if (time == 1.0f) {
            CHECK(own->paintBounds.width == 0.0f && own->paintBounds.height == 0.0f);
            CHECK(near(own->familyBounds.x, child->paintBounds.x));
            CHECK(near(own->familyBounds.width, child->paintBounds.width));
            CHECK(near(own->familyBounds.height, child->paintBounds.height));
            BBox bounds;
            CHECK(renderer->bounds(scene, vector, time, bounds) == Result::Success);
            CHECK(near(bounds.x, child->paintBounds.x) && near(bounds.width, child->paintBounds.width));
        }
        Surface surface;
        CHECK(renderer->render(scene, time, surface) == Result::Success);
        auto red = 0u;
        for (auto y = 0u; y < surface.height(); y++) {
            for (auto x = 0u; x < surface.width(); x++) {
                auto value = surface.data()[y * surface.stride() + x];
                if ((value & 0xffu) > ((value >> 8u) & 0xffu)) red++;
            }
        }
        CHECK((red > 0u) == (time != 1.0f));
    }
    delete scene;
}

void geometryClips(Renderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto black = pixel(config.background);
    auto yellow = Color::hex("#ffff00");
    auto cyan = Color::hex("#00ffff");

    // 20 pixels per unit with the origin at pixel (80, 40).
    auto scene = Scene::gen(config);
    auto line = Line::gen({-3.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f});
    auto curve = Curve::gen({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, 30u);
    const Vec3 samples[] = {{-3.0f, 1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    auto plot = Plot::gen(samples, 3u);
    auto vector = Vector::gen({1.0f, 0.0f, 0.0f}, {0.0f, -1.5f, 0.0f});
    auto circle = Circle::gen({3.0f, 1.5f, 0.0f}, 0.25f);
    Object* objects[] = {line, curve, plot, vector, circle};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    line->stroke(yellow);
    line->strokeWidth(4.0f);
    curve->stroke(cyan);
    curve->strokeWidth(4.0f);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);

    // Before the timeline the points become the starting geometry.
    const Vec3 lowered[] = {{-3.0f, -1.5f, 0.0f}, {-1.0f, -1.5f, 0.0f}};
    CHECK(scene->update(AnimationTarget::from(line).geometry(lowered, 2u)) == Result::Success);
    CHECK(near(line->from.y, -1.5f) && near(line->to.y, -1.5f));

    // The line rises from y = -1.5 to y = 0.5 after a second, and the curve bends upward.
    CHECK(scene->wait(1.0f) == Result::Success);
    const Vec3 raised[] = {{-3.0f, 0.5f, 0.0f}, {-1.0f, 0.5f, 0.0f}};
    const Vec3 bent[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, {2.0f, 2.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
    const Vec3 turned[] = {{0.0f, 1.0f, 0.0f}, {0.0f, -1.5f, 0.0f}};
    AnimationTarget targets[] = {
        AnimationTarget::from(line).geometry(raised, 2u),
        AnimationTarget::from(curve).geometry(bent, 4u),
        AnimationTarget::from(vector).geometry(turned, 2u),
    };
    CHECK(scene->play(targets, 3u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    // The objects hold the geometry their clips end at.
    CHECK(near(line->from.y, 0.5f) && near(line->to.y, 0.5f));
    CHECK(near(curve->controls()[1].y, 2.0f) && near(curve->controls()[2].x, 2.0f));
    CHECK(curve->count() == 31u && near(curve->points()[15].x, 1.5f) && near(curve->points()[15].y, 1.5f));
    CHECK(near(vector->value.y, 1.0f) && near(vector->value.x, 0.0f) && near(vector->origin.y, -1.5f));

    Surface surface;
    // Until the clips begin the shapes keep the geometry they started with.
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 70u) == pixel(yellow));
    CHECK(_pixelAt(surface, 40u, 30u) == black);
    CHECK(_pixelAt(surface, 110u, 40u) == pixel(cyan));
    CHECK(_pixelAt(surface, 110u, 25u) == black);
    // Halfway the line is at y = -0.5 and the middle of the curve at y = 0.75.
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 70u) == black);
    CHECK(_pixelAt(surface, 40u, 50u) == pixel(yellow));
    CHECK(_pixelAt(surface, 110u, 40u) == black);
    CHECK(_pixelAt(surface, 110u, 25u) == pixel(cyan));
    BBox half;
    BBox full;
    CHECK(renderer->bounds(scene, curve, 1.5f, half) == Result::Success);
    CHECK(renderer->bounds(scene, curve, 2.0f, full) == Result::Success);
    CHECK(half.height > 14.0f && half.height < 20.0f && full.height > 29.0f && full.height < 35.0f);
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 30u) == pixel(yellow));
    CHECK(_pixelAt(surface, 110u, 10u) == pixel(cyan));

    // A second change of the same geometry waits for the first, but a move may run with it.
    auto mark = scene->cursor();
    const Vec3 flat[] = {{-3.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(line).geometry(flat, 2u), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->seek(mark) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(line).shift({0.0f, -0.5f, 0.0f}), 1.0f,
                      AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->seek(mark + 0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(line).geometry(raised, 2u), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "already animates its geometry until 3.00 s"));
    CHECK(scene->seek(mark + 1.0f) == Result::Success);
    // The line ends flat at y = 0 and moved down by 0.5.
    CHECK(renderer->render(scene, 3.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 50u) == pixel(yellow));
    CHECK(_pixelAt(surface, 40u, 40u) == black);

    // After the timeline starts an update switches the geometry at the cursor.
    const Vec3 short_[] = {{-3.0f, 0.0f, 0.0f}, {-2.5f, 0.0f, 0.0f}};
    CHECK(scene->update(AnimationTarget::from(line).geometry(short_, 2u)) == Result::Success);
    CHECK(near(line->to.x, -2.5f));
    CHECK(scene->wait(0.5f) == Result::Success);
    CHECK(renderer->render(scene, 2.99f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 50u) == pixel(yellow));
    CHECK(renderer->render(scene, 3.2f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 50u) == black);
    CHECK(_pixelAt(surface, 25u, 50u) == pixel(yellow));

    // A restore carries the geometry back to the one the save kept.
    Object* curves[] = {curve};
    CHECK(scene->save(curves, 1u) == Result::Success);
    const Vec3 dipped[] = {{0.0f, 0.0f, 0.0f}, {1.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(curve).geometry(dipped, 4u), 1.0f) == Result::Success);
    CHECK(near(curve->points()[15].y, -1.5f));
    CHECK(scene->restore(curves, 1u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(near(curve->points()[15].y, 1.5f) && near(curve->controls()[1].y, 2.0f));
    auto end = scene->cursor();
    CHECK(renderer->render(scene, end - 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 70u) == pixel(cyan));
    CHECK(renderer->render(scene, end, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 10u) == pixel(cyan));
    CHECK(_pixelAt(surface, 110u, 70u) == black);
    // Repeating the geometry the shape has keeps it as it is.
    CHECK(scene->play(AnimationTarget::from(curve).geometry(bent, 4u), 0.5f) == Result::Success);
    CHECK(renderer->render(scene, end + 0.25f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 10u) == pixel(cyan));

    // Geometry that cannot reach the shape is explained.
    const Vec3 two[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(plot).geometry(two, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keep the object's point count"));
    CHECK(scene->play(AnimationTarget::from(curve).geometry(two, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "4 for a curve"));
    CHECK(scene->play(AnimationTarget::from(circle).geometry(two, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "geometry needs a line"));
    const Vec3 broken[] = {{0.0f, 0.0f, 0.0f}, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(line).geometry(broken, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "must be finite"));
    const Vec3 points[] = {{-3.0f, 1.0f, 0.0f}, {-2.0f, 1.5f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(plot).geometry(points, 3u), 1.0f) == Result::Success);
    CHECK(near(plot->points()[1].y, 1.5f));
    delete scene;

    // In the 3D view a surface hides a stroke where its points on their way pass behind it, not
    // where the points it started or ends with lie. Seen from above, a line along y = 0 and a plot
    // along y = 0.5 tilt from z = 1 over the plane z = 0 to z = 2x - 1: before the clips both draw
    // whole over the plane; halfway they are z = x, hidden at x = -0.5 and shown at x = 0.3, where
    // the end would hide them.
    auto view = topView();
    auto plate = Color::hex("#2050c0");
    auto white = Color::hex("#ffffff");
    auto tilted = Scene::gen(view);
    auto plane = depthPlate(0.0f, plate);
    auto beam = Line::gen({-2.0f, 0.0f, 1.0f}, {2.0f, 0.0f, 1.0f});
    const Vec3 level[] = {{-2.0f, 0.5f, 1.0f}, {0.0f, 0.5f, 1.0f}, {2.0f, 0.5f, 1.0f}};
    auto trace = Plot::gen(level, 3u);
    Object* parts[] = {plane, beam, trace};
    auto complete = tilted != nullptr;
    for (auto part : parts) complete = complete && part;
    CHECK(complete);
    if (!complete) {
        for (auto part : parts) delete part;
        delete tilted;
        return;
    }
    beam->stroke(white, 4.0f);
    trace->stroke(white, 4.0f);
    for (auto part : parts) CHECK(tilted->add(part) == Result::Success);
    const Vec3 slant[] = {{-2.0f, 0.0f, -5.0f}, {2.0f, 0.0f, 3.0f}};
    const Vec3 slope[] = {{-2.0f, 0.5f, -5.0f}, {0.0f, 0.5f, -1.0f}, {2.0f, 0.5f, 3.0f}};
    AnimationTarget tilts[] = {AnimationTarget::from(beam).geometry(slant, 2u),
                               AnimationTarget::from(trace).geometry(slope, 3u)};
    CHECK(tilted->play(tilts, 2u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    // 40 pixels per unit with the origin at pixel (100, 100): the line on row 100, the plot on 80.
    const uint32_t rows[] = {100u, 80u};
    Surface frame;
    CHECK(renderer->render(tilted, 0.0f, frame) == Result::Success);
    if (frame.data()) {
        for (auto row : rows) CHECK(close(framePixel(frame, 80u, row), white, 8));
    }
    CHECK(renderer->render(tilted, 0.5f, frame) == Result::Success);
    if (frame.data()) {
        for (auto row : rows) {
            CHECK(close(framePixel(frame, 80u, row), plate, 8));
            CHECK(close(framePixel(frame, 112u, row), white, 8));
            CHECK(close(framePixel(frame, 40u, row), white, 8));
        }
    }
    CHECK(renderer->render(tilted, 1.0f, frame) == Result::Success);
    if (frame.data()) {
        for (auto row : rows) {
            CHECK(close(framePixel(frame, 112u, row), plate, 8));
            CHECK(close(framePixel(frame, 130u, row), white, 8));
        }
    }
    delete tilted;
}

// Function columns keep x fixed and blend y before clipping, including as pieces leave or
// enter the band. A copy retains the sampled column and discontinuity gaps.
void plotFunctionPlay(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    const Vec3 zero[] = {{-2, 0, 0}, {-1, 0, 0}, {0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    const Vec3 square[] = {{-2, 8, 0}, {-1, 2, 0}, {0, 0, 0}, {1, 2, 0}, {2, 8, 0}};
    const Vec3 cut[] = {{-0.5f, 1, 0}, {0, 0, 0}, {0.5f, 1, 0}};
    PlotSamples from{zero, 5u, true, {-1, 1}}, to{square, 5u, true, {-1, 1}};
    auto scene = Scene::gen(config);
    auto plot = Plot::gen(zero, 5u, nullptr, 1u, &from);
    CHECK(scene && plot && scene->add(plot) == Result::Success);
    if (!scene || !plot) { delete scene; return; }
    Object* initial = nullptr;
    CHECK(scene->clone(plot, initial) == Result::Success);
    CHECK(initial && static_cast<Plot*>(initial)->samples().count == 5u);
    CHECK(scene->play(AnimationTarget::from(plot).geometry(cut, 3u, nullptr, 1u, &to), 1, linear) == Result::Success);
    auto copyAt = [&](float time) {
        CHECK(scene->seek(time) == Result::Success);
        Object* copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        return copy ? static_cast<Plot*>(copy) : nullptr;
    };
    auto quarter = copyAt(0.25f);
    CHECK(quarter && quarter->count() == 5u && quarter->contours() == 1u);
    if (quarter && quarter->count() == 5u) {
        CHECK(near(quarter->points()[0].x, -4.0f / 3.0f) && near(quarter->points()[0].y, 1));
        CHECK(near(quarter->points()[1].x, -1) && near(quarter->points()[1].y, 0.5f));
        CHECK(near(quarter->points()[2].x, 0) && near(quarter->points()[2].y, 0));
        CHECK(near(quarter->samples().points[0].y, 2));
    }
    auto half = copyAt(0.5f);
    CHECK(half && half->count() == 3u && near(half->points()[0].x, -1) && near(half->points()[0].y, 1));
    BBox bounds;
    CHECK(renderer->bounds(scene, plot, 0.25f, bounds) == Result::Success && bounds.width > 80);
    CHECK(renderer->bounds(scene, plot, 0.5f, bounds) == Result::Success && bounds.width < 65);
    // Sampling out of order keeps the same column and cut.
    auto again = copyAt(0.25f);
    CHECK(again && again->count() == quarter->count()
          && std::memcmp(again->points(), quarter->points(), sizeof(Vec3) * quarter->count()) == 0);
    delete scene;

    // A gap remains a gap even when clipping changes the visible pieces and leaves none.
    auto nan = std::numeric_limits<float>::quiet_NaN();
    const Vec3 apart[] = {{-2, 0, 0}, {-1, 0, 0}, {nan, nan, 0}, {1, 0, 0}, {2, 0, 0}};
    const Vec3 raised[] = {{-2, 4, 0}, {-1, 4, 0}, {nan, nan, 0}, {1, 0, 0}, {2, 0, 0}};
    const Vec3 visible[] = {{-2, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    const uint32_t ends[] = {2u, 4u};
    from = {apart, 5u, true, {-1, 1}};
    to = {raised, 5u, true, {-1, 1}};
    scene = Scene::gen(config);
    plot = Plot::gen(visible, 4u, ends, 2u, &from);
    CHECK(scene && plot && scene->add(plot) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).geometry(visible + 2, 2u, nullptr, 1u, &to), 1, linear) == Result::Success);
    auto edge = copyAt(0.25f);
    CHECK(edge && edge->contours() == 2u && near(edge->points()[0].y, 1));
    auto leaving = copyAt(0.5f);
    CHECK(leaving && leaving->count() == 2u && leaving->contours() == 1u && near(leaving->points()[0].x, 1));
    CHECK(near(_shownOpacity(renderer, scene, 0.25f, {0, 0, 0}), 0, 0.01f));
    // Reverse play makes the lost piece enter the band again.
    CHECK(scene->seek(1) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).geometry(visible, 4u, ends, 2u, &from), 1, linear) == Result::Success);
    auto entering = copyAt(1.75f);
    CHECK(entering && entering->contours() == 2u && near(entering->points()[0].y, 1));
    const Vec3 invalid[] = {{1, 0, 0}, {0, 0, 0}};
    to = {invalid, 2u, false, {}};
    CHECK(scene->play(AnimationTarget::from(plot).geometry(visible, 4u, ends, 2u, &to), 1) == Result::InvalidArguments);
    delete scene;
}

// A plot that takes points in other pieces switches halfway; matching pieces travel.
// Copies, saves and clips of one frame each go by the shape the plot shows.
void plotReplot(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    // A run a unit below the axis; two pieces a unit above it, apart around x = 0; the same two
    // pieces on the axis.
    const Vec3 low[] = {{-4.0f, -1.0f, 0.0f}, {-2.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
                        {2.0f, -1.0f, 0.0f}, {4.0f, -1.0f, 0.0f}};
    const Vec3 high[] = {{-4.0f, 1.0f, 0.0f}, {-2.5f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f},
                         {1.0f, 1.0f, 0.0f}, {2.5f, 1.0f, 0.0f}, {4.0f, 1.0f, 0.0f}};
    const Vec3 level[] = {{-4.0f, 0.0f, 0.0f}, {-2.5f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
                          {1.0f, 0.0f, 0.0f}, {2.5f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}};
    const uint32_t apart[] = {3u, 6u};
    const Vec3 corners[] = {{4.5f, 2.0f, 0.0f}, {5.0f, 2.0f, 0.0f}, {5.0f, 2.5f, 0.0f}};
    Scene* scene = nullptr;
    Plot* plot = nullptr;
    auto shown = [&](float time, float x, float y) { return _shownOpacity(renderer, scene, time, {x, y, 0.0f}); };
    // The pixel row the middle of the plot's bounds lies on at time, or -1 without bounds.
    auto row = [&](float time) {
        BBox bounds;
        if (renderer->bounds(scene, plot, time, bounds) != Result::Success) return -1.0f;
        return bounds.center().y;
    };
    auto start = [&](const Vec3* points, uint32_t count, const uint32_t* ends, uint32_t pieces) {
        scene = Scene::gen(config);
        plot = Plot::gen(points, count, ends, pieces);
        CHECK(scene && plot);
        if (!scene || !plot) {
            delete plot;
            delete scene;
            return false;
        }
        plot->stroke({0, 0, 0, 255}, 4.0f);
        CHECK(scene->add(plot) == Result::Success);
        return true;
    };

    if (start(low, 5u, nullptr, 1u)) {
        // Other pieces: the run shows until halfway, then the two pieces, with nothing between.
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, apart, 2u), 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 6u && plot->contours() == 2u && plot->contourEnd(0) == 3u);
        CHECK(near(shown(0.25f, 0.0f, -1.0f), 1.0f, 0.05f) && near(shown(0.25f, -3.0f, 1.0f), 0.0f, 0.02f));
        CHECK(near(shown(0.45f, 3.0f, -1.0f), 1.0f, 0.05f));
        CHECK(near(shown(0.55f, 3.0f, -1.0f), 0.0f, 0.02f));
        CHECK(near(shown(0.55f, -3.0f, 1.0f), 1.0f, 0.05f) && near(shown(0.55f, 3.0f, 1.0f), 1.0f, 0.05f));
        CHECK(near(shown(0.55f, 0.0f, 1.0f), 0.0f, 0.02f));
        CHECK(near(row(0.25f), 120.0f, 1.0f) && near(row(0.75f), 60.0f, 1.0f));
        // The same pieces: the points travel.
        CHECK(scene->play(AnimationTarget::from(plot).geometry(level, 6u, apart, 2u), 1.0f, linear) == Result::Success);
        CHECK(near(shown(1.5f, -3.0f, 0.5f), 1.0f, 0.05f) && near(shown(1.5f, 0.0f, 0.5f), 0.0f, 0.02f));
        CHECK(near(row(1.5f), 75.0f, 1.0f));
        // No points leave the plot empty, with no bounds, until points come back; points of the
        // old count no longer fit it.
        CHECK(scene->play(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 0u), 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 0u && plot->contours() == 0u);
        CHECK(near(shown(2.25f, -3.0f, 0.0f), 1.0f, 0.05f) && near(shown(2.75f, -3.0f, 0.0f), 0.0f, 0.02f));
        CHECK(row(2.25f) > 0.0f && row(2.75f) < 0.0f);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(level, 6u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(low, 5u, nullptr, 1u), 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 5u && plot->contours() == 1u);
        CHECK(near(shown(3.25f, 0.0f, -1.0f), 0.0f, 0.02f) && near(shown(3.75f, 0.0f, -1.0f), 1.0f, 0.05f));
        // A copy takes the shape shown at the cursor, pieces and all, or none.
        CHECK(scene->seek(0.75f) == Result::Success);
        Object* copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        auto twin = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
        CHECK(twin && twin->count() == 6u && twin->contours() == 2u && twin->contourEnd(0) == 3u);
        CHECK(twin && near(twin->points()[3].x, 1.0f) && near(twin->points()[3].y, 1.0f));
        CHECK(scene->seek(2.75f) == Result::Success);
        copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        auto hollow = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
        CHECK(hollow && hollow->count() == 0u && hollow->contours() == 0u);
        delete scene;
    }

    if (start(high, 6u, apart, 2u)) {
        // A restore brings the saved pieces back halfway through its clip, and creation runs
        // through them.
        auto triangle = Polygon::gen(corners, 3u);
        CHECK(triangle && scene->add(triangle) == Result::Success);
        Object* plots[] = {plot};
        CHECK(scene->save(plots, 1u) == Result::Success);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(low, 5u, nullptr, 1u), 1.0f, linear) == Result::Success);
        CHECK(scene->restore(plots, 1u, 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 6u && plot->contours() == 2u && plot->contourEnd(0) == 3u);
        CHECK(near(shown(1.25f, 0.0f, -1.0f), 1.0f, 0.05f));
        CHECK(near(shown(1.75f, 0.0f, -1.0f), 0.0f, 0.02f) && near(shown(1.75f, -3.0f, 1.0f), 1.0f, 0.05f));
        CHECK(scene->play(Animation::create(plot), 1.0f, linear) == Result::Success);
        CHECK(near(shown(2.75f, 2.0f, 1.0f), 1.0f, 0.05f) && near(shown(2.75f, 3.0f, 1.0f), 0.0f, 0.02f));
        // Pieces are a plot's alone and must cover its points.
        CHECK(scene->play(AnimationTarget::from(triangle).geometry(corners, 3u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        CHECK(_says(scene, "only a plot takes pieces"));
        const uint32_t lonely[] = {1u, 6u};
        const uint32_t short_[] = {3u, 5u};
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, lonely, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(_says(scene, "each holding two points or more"));
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, short_, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, nullptr, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, nullptr, 0u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 1u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        delete scene;
    }

    if (start(low, 5u, nullptr, 1u)) {
        // Before the timeline the points and pieces become the plot's own; afterwards a set
        // switches them at the cursor.
        CHECK(scene->update(AnimationTarget::from(plot).geometry(high, 6u, apart, 2u)) == Result::Success);
        CHECK(plot->count() == 6u && plot->contours() == 2u);
        CHECK(near(shown(0.0f, -3.0f, 1.0f), 1.0f, 0.05f) && near(shown(0.0f, 0.0f, 1.0f), 0.0f, 0.02f));
        CHECK(scene->wait(1.0f) == Result::Success);
        CHECK(scene->update(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 0u)) == Result::Success);
        CHECK(near(shown(0.5f, -3.0f, 1.0f), 1.0f, 0.05f) && near(shown(1.0f, -3.0f, 1.0f), 0.0f, 0.02f));
        // A warp of the empty plot keeps it empty, as a warp keeps the outline of any shape.
        Vec3 up = {0.0f, 1.0f, 0.0f};
        Object* targets[] = {plot};
        CHECK(scene->warp(targets, 1u, _moved, &up, 1.0f) == Result::Success);
        CHECK(near(shown(2.0f, -3.0f, 2.0f), 0.0f, 0.02f) && row(2.0f) < 0.0f);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(low, 5u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        delete scene;
    }

    // Clips of a frame each, as a track plays them, show at every frame the shape of its time,
    // also where the frame falls just before the end of its clip.
    config.fps = 60;
    const Vec3 rest[] = {{-2.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}};
    if (start(rest, 2u, nullptr, 1u)) {
        auto early = 0u;
        for (auto k = 1u; k <= 30u; k++) {
            // A run 0.1 higher each frame, of two points and of three in turn.
            auto y = -2.0f + 0.1f * static_cast<float>(k);
            auto three = k % 2u == 1u;
            Vec3 run[] = {{-2.0f, y, 0.0f}, {three ? 0.0f : 2.0f, y, 0.0f}, {2.0f, y, 0.0f}};
            CHECK(scene->play(AnimationTarget::from(plot).geometry(run, three ? 3u : 2u, nullptr, 1u), 1.0f / 60.0f,
                              linear) == Result::Success);
            auto frame = static_cast<float>(k) / 60.0f;
            if (frame < scene->cursor()) early++;
            CHECK(near(row(frame), 90.0f - 30.0f * y, 1.0f));
        }
        CHECK(early > 0u);
        delete scene;
    }
}
