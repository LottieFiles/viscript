#include "testSceneChecks.h"

void matchedShapes(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};

    // A shape paired with one of its kind becomes it: the source leaves at once, and its pair
    // starts in its place, size, and colors, then travels to its own.
    auto scene = Scene::gen(config);
    auto from = Group::gen();
    auto to = Group::gen();
    auto wide = Rectangle::gen({-2.0f, 0.5f, 0.0f}, {2.0f, 1.0f}, 0.2f);
    auto tall = Rectangle::gen({2.0f, 0.5f, 0.0f}, {0.6f, 2.0f});
    auto small = Circle::gen({-2.0f, -1.5f, 0.0f}, 0.3f);
    auto large = Circle::gen({2.0f, -1.5f, 0.0f}, 0.6f);
    CHECK(scene && from && to && wide && tall && small && large);
    if (!scene || !from || !to || !wide || !tall || !small || !large) {
        delete scene;
        delete from;
        delete to;
        delete wide;
        delete tall;
        delete small;
        delete large;
        return;
    }
    Object* shapes[] = {wide, tall, small, large};
    for (auto shape : shapes) shape->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
    tall->fill({0, 0, 0, 128});
    CHECK(from->add(wide) == Result::Success && from->add(small) == Result::Success);
    CHECK(to->add(tall) == Result::Success && to->add(large) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->wait(1.0f) == Result::Success);
    CHECK(scene->add(to) == Result::Success);
    MatchOptions options;
    options.pairs = shapes;
    options.pairCount = 2;
    CHECK(scene->matchTransform(from, to, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
          == Result::Success);
    CHECK(scene->clipCount() == 2u);
    BBox before;
    BBox start;
    BBox middle;
    BBox end;
    CHECK(renderer->bounds(scene, wide, 0.9f, before) == Result::Success);
    CHECK(renderer->bounds(scene, wide, 1.0f, start) == Result::InsufficientCondition);
    CHECK(renderer->bounds(scene, tall, 1.0f, start) == Result::Success);
    CHECK(near(start.x, before.x, 0.5f) && near(start.y, before.y, 0.5f));
    CHECK(near(start.width, before.width, 0.5f) && near(start.height, before.height, 0.5f));
    CHECK(near(_shownOpacity(renderer, scene, 1.0f, {-2.0f, 0.5f, 0.0f}), 1.0f, 0.02f));
    // Halfway it is halfway in place, size, and fill.
    CHECK(renderer->bounds(scene, tall, 2.0f, middle) == Result::Success);
    CHECK(renderer->bounds(scene, tall, 3.0f, end) == Result::Success);
    CHECK(near(middle.center().x, (start.center().x + end.center().x) * 0.5f, 0.5f));
    CHECK(near(middle.width, (start.width + end.width) * 0.5f, 0.5f));
    CHECK(near(middle.height, (start.height + end.height) * 0.5f, 0.5f));
    CHECK(near(_shownOpacity(renderer, scene, 2.0f, {0.0f, 0.5f, 0.0f}), 0.75f, 0.03f));
    CHECK(near(end.width, 18.0f, 0.5f) && near(end.height, 60.0f, 0.5f));
    CHECK(near(_shownOpacity(renderer, scene, 3.0f, {2.0f, 0.5f, 0.0f}), 0.5f, 0.02f));
    // A circle grows into the circle it pairs with.
    CHECK(renderer->bounds(scene, small, 0.9f, before) == Result::Success);
    CHECK(renderer->bounds(scene, large, 1.0f, start) == Result::Success);
    CHECK(near(start.x, before.x, 0.5f) && near(start.width, before.width, 0.5f));
    CHECK(renderer->bounds(scene, large, 3.0f, end) == Result::Success);
    CHECK(near(end.width, 36.0f, 0.5f));
    delete scene;

    // Shapes pair in order, kind by kind, when asked. Two bars split into four: the first of
    // each run becomes its bar, and the second starts unseen in its place and fades in on the
    // way to its own; four merging into two fade the second of each run out into its bar.
    const bool splits[] = {true, false};
    for (auto split : splits) {
        auto bars = Scene::gen(config);
        auto two = Group::gen();
        auto four = Group::gen();
        auto dot = Circle::gen({0.0f, 2.0f, 0.0f}, 0.2f);
        Rectangle* halves[4] = {};
        Rectangle* wholes[2] = {};
        auto made = bars && two && four && dot;
        for (auto i = 0u; i < 4u; i++) {
            halves[i] = Rectangle::gen({(i < 2u ? -2.5f : 1.5f) + static_cast<float>(i % 2u), 0.0f, 0.0f}, {0.8f, 1.0f});
            made = made && halves[i];
        }
        for (auto i = 0u; i < 2u; i++) {
            wholes[i] = Rectangle::gen({i ? 2.0f : -2.0f, 0.0f, 0.0f}, {2.0f, 1.0f});
            made = made && wholes[i];
        }
        CHECK(made);
        if (!made) {
            delete bars;
            delete two;
            delete four;
            delete dot;
            for (auto half : halves) delete half;
            for (auto whole : wholes) delete whole;
            continue;
        }
        for (auto whole : wholes) {
            whole->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(two->add(whole) == Result::Success);
        }
        for (auto half : halves) {
            half->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(four->add(half) == Result::Success);
        }
        CHECK((split ? two : four)->add(dot) == Result::Success);
        auto from = split ? two : four;
        auto to = split ? four : two;
        CHECK(bars->add(from) == Result::Success && bars->wait(1.0f) == Result::Success);
        CHECK(bars->add(to) == Result::Success);
        MatchOptions shaped;
        shaped.shapes = true;
        CHECK(bars->matchTransform(from, to, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear), shaped)
              == Result::Success);
        // Four bars play, and the lone circle of the source fades out.
        CHECK(bars->clipCount() == 5u);
        BBox first;
        BBox second;
        BBox whole;
        if (split) {
            CHECK(renderer->bounds(bars, wholes[0], 0.9f, whole) == Result::Success);
            CHECK(renderer->bounds(bars, halves[0], 1.0f, first) == Result::Success);
            CHECK(near(first.x, whole.x, 0.5f) && near(first.width, whole.width, 0.5f));
            CHECK(renderer->bounds(bars, halves[1], 1.0f, second) == Result::InsufficientCondition);
            CHECK(renderer->bounds(bars, halves[1], 2.0f, second) == Result::Success);
            CHECK(near(second.center().x, 107.5f, 0.5f) && near(second.width, 42.0f, 0.5f));
            CHECK(renderer->bounds(bars, halves[3], 3.0f, second) == Result::Success);
            CHECK(near(second.center().x, 235.0f, 0.5f) && near(second.width, 24.0f, 0.5f));
        } else {
            CHECK(renderer->bounds(bars, halves[0], 1.0f, first) == Result::InsufficientCondition);
            CHECK(renderer->bounds(bars, wholes[0], 1.0f, whole) == Result::Success);
            CHECK(near(whole.center().x, 85.0f, 0.5f) && near(whole.width, 24.0f, 0.5f));
            CHECK(renderer->bounds(bars, halves[1], 2.0f, second) == Result::Success);
            CHECK(near(second.center().x, 107.5f, 0.5f) && near(second.width, 42.0f, 0.5f));
            CHECK(renderer->bounds(bars, halves[3], 2.99f, second) == Result::Success);
            CHECK(near(second.center().x, 220.0f, 1.0f) && near(second.width, 60.0f, 1.0f));
            CHECK(renderer->bounds(bars, halves[3], 3.0f, second) == Result::InsufficientCondition);
        }
        delete bars;
    }
}

void morphedOutlines(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);

    // A circle becomes a larger square: each side bulges out of the circle and its corners
    // come from the points at 45 degrees, so halfway the shape reaches 1.25 units out.
    {
        auto scene = Scene::gen(config);
        auto circle = Circle::gen({}, 1.0f);
        auto square = Rectangle::gen({}, {3.0f, 3.0f});
        CHECK(scene && circle && square);
        if (scene && circle && square) {
            circle->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            square->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(circle) == Result::Success && scene->wait(1.0f) == Result::Success);
            CHECK(scene->add(square) == Result::Success);
            CHECK(scene->play(Animation::morph(circle, square), 2.0f, linear) == Result::Success);
            BBox middle;
            CHECK(renderer->bounds(scene, circle, 2.0f, middle) == Result::Success);
            CHECK(near(middle.width, 75.0f, 1.0f) && near(middle.height, 75.0f, 1.0f));
            CHECK(near(_shownOpacity(renderer, scene, 0.9f, {1.2f, 0.0f, 0.0f}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 2.0f, {1.2f, 0.0f, 0.0f}), 1.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 2.0f, {0.0f, 0.0f, 0.0f}), 1.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 2.0f, {1.2f, 1.2f, 0.0f}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 3.0f, {1.4f, 1.4f, 0.0f}), 1.0f, 0.02f));
            CHECK(renderer->bounds(scene, circle, 3.0f, middle) == Result::InsufficientCondition);
        } else {
            delete circle;
            delete square;
        }
        delete scene;
    }

    // A star listed clockwise runs against the square's outline; the morph turns it around
    // instead of pulling the shape through its center.
    {
        auto scene = Scene::gen(config);
        auto square = Rectangle::gen({}, {3.0f, 3.0f});
        Vec3 points[10];
        for (auto i = 0u; i < 10u; i++) {
            auto angle = 1.5707963f - 0.62831853f * static_cast<float>(i);
            auto radius = i % 2u ? 0.7f : 1.6f;
            points[i] = {radius * std::cos(angle), radius * std::sin(angle), 0.0f};
        }
        auto star = Polygon::gen(points, 10u);
        CHECK(scene && square && star);
        if (scene && square && star) {
            square->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            star->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(square) == Result::Success && scene->add(star) == Result::Success);
            CHECK(scene->play(Animation::morph(square, star), 2.0f, linear) == Result::Success);
            auto start = _inked(renderer, scene, 0.0f);
            auto middle = _inked(renderer, scene, 1.0f);
            auto end = _inked(renderer, scene, 2.0f);
            CHECK(near(static_cast<float>(start), 8100.0f, 200.0f));
            CHECK(near(static_cast<float>(end), 2962.0f, 150.0f));
            CHECK(middle > end && middle < start);
        } else {
            delete square;
            delete star;
        }
        delete scene;
    }

    // An open outline starts at either end of the other: a line becomes a plot drawn from right
    // to left without folding through its middle.
    {
        auto scene = Scene::gen(config);
        auto line = Line::gen({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
        Vec3 points[] = {{2.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f}};
        auto plot = Plot::gen(points, 3u);
        auto circle = Circle::gen({}, 0.5f);
        CHECK(scene && line && plot && circle);
        if (scene && line && plot && circle) {
            CHECK(scene->add(line) == Result::Success && scene->add(plot) == Result::Success);
            CHECK(scene->add(circle) == Result::Success);
            CHECK(scene->play(Animation::morph(circle, plot), 1.0f) == Result::NonSupport);
            CHECK(scene->play(Animation::morph(line, plot), 2.0f, linear) == Result::Success);
            BBox middle;
            CHECK(renderer->bounds(scene, line, 1.0f, middle) == Result::Success);
            CHECK(middle.width >= 120.0f && middle.width < 126.0f);
            CHECK(near(middle.center().y, 75.0f, 1.0f));
        } else {
            delete line;
            delete plot;
            delete circle;
        }
        delete scene;
    }

    // Two polygons with the same number of points keep the pairing they were given: corner i
    // goes to corner i, so a square whose corners are listed one place on turns into a diamond
    // halfway.
    {
        auto scene = Scene::gen(config);
        Vec3 corners[] = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
        Vec3 turned[] = {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f}};
        auto from = Polygon::gen(corners, 4u);
        auto to = Polygon::gen(turned, 4u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            from->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            to->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            CHECK(scene->play(Animation::morph(from, to), 2.0f, linear) == Result::Success);
            CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.9f, 0.9f, 0.0f}), 1.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.9f, 0.9f, 0.0f}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.0f, 0.0f, 0.0f}), 1.0f, 0.02f));
        } else {
            delete from;
            delete to;
        }
        delete scene;
    }
}

static double _morphArea(const LayoutPath* path)
{
    auto area = 0.0;
    for (auto i = 0u; i < path->count; i++) {
        auto a = path->points[i];
        auto b = path->points[(i + 1u) % path->count];
        area += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
    }
    return area * 0.5;
}

void pieceMorphs(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto move = [](float x, float y) { return PathCommand::move({x, y, 0.0f}); };
    auto line = [](float x, float y) { return PathCommand::line({x, y, 0.0f}); };

    // Piece order differs across kinds. Automatic pairs stay on their own side; explicit
    // pairs cross over. A rejected pair leaves both objects and the timeline untouched.
    for (auto explicitPairs : {false, true}) {
        PathCommand commands[] = {move(-2, 0), line(-1, 0), move(1, 0), line(3, 0)};
        Vec3 points[] = {{3, 2, 0}, {1, 2, 0}, {-1, 2, 0}, {-2, 2, 0}};
        uint32_t ends[] = {2, 4};
        auto scene = Scene::gen(config);
        auto from = Path::gen(commands, 4u);
        auto to = Plot::gen(points, 4u, ends, 2u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            uint32_t duplicate[] = {0, 0, 0, 1};
            auto animation = Animation::morph(from, to);
            animation.pairs = duplicate;
            animation.pairCount = 2;
            CHECK(scene->play(animation, 1.0f, linear) == Result::InvalidArguments);
            CHECK(near(scene->duration(), 0.0f) && scene->clipCount() == 0u);
            uint32_t pairs[] = {0, 0, 1, 1};
            animation.pairs = explicitPairs ? pairs : nullptr;
            animation.pairCount = explicitPairs ? 2u : 0u;
            CHECK(scene->play(animation, 1.0f, linear) == Result::Success);
            // The caller's pair array is no longer needed after play.
            pairs[0] = 100u;
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.5f, report) == Result::Success && report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                auto right = report.pathAt(0)->paintBounds.center().x;
                auto left = report.pathAt(1)->paintBounds.center().x;
                CHECK(explicitPairs ? near(right, 167.5f, 0.1f) && near(left, 167.5f, 0.1f)
                                    : near(right, 220.0f, 0.1f) && near(left, 115.0f, 0.1f));
            }
        } else { delete from; delete to; }
        delete scene;
    }

    // Keep centroid distances in double precision: a float-rounded best distance could
    // select the farther, larger target even after the exact-size target was found.
    {
        Vec3 a[] = {{-1, 0, 0}, {1, 0, 0}, {-6, 0, 0}, {6, 0, 0}};
        Vec3 b[] = {{1, 0, 0}, {3, 0, 0}, {-4, 0, 0}, {0, 0, 0}, {-6, 0, 0}, {6, 0, 0}};
        uint32_t fromEnds[] = {2, 4};
        uint32_t toEnds[] = {2, 4, 6};
        auto scene = Scene::gen(config);
        auto from = Plot::gen(a, 4u, fromEnds, 2u);
        auto to = Plot::gen(b, 6u, toEnds, 3u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            auto animation = Animation::morph(from, to);
            uint32_t pairs[] = {1, 2};
            animation.pairs = pairs;
            animation.pairCount = 1;
            CHECK(scene->play(animation, 1.0f, linear) == Result::Success);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.5f, report) == Result::Success && report.pathCount() == 3u);
            if (report.pathCount() == 3u) CHECK(near(report.pathAt(1)->paintBounds.center().x, 190.0f, 0.1f));
        } else { delete from; delete to; }
        delete scene;
    }

    // Reserve open targets for open sources before a closed source takes a mixed pair.
    {
        Vec3 points[] = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}, {-1, -1, 0},
                         {3, 2, 0}, {5, 2, 0}};
        uint32_t ends[] = {5, 7};
        auto scene = Scene::gen(config);
        auto source = Plot::gen(points, 7u, ends, 2u);
        auto target = Line::gen({-1, 2, 0}, {1, 2, 0});
        CHECK(scene && source && target);
        if (scene && source && target) {
            CHECK(scene->add(source) == Result::Success && scene->add(target) == Result::Success);
            CHECK(scene->play(Animation::morph(source, target), 1.0f, linear) == Result::Success);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.5f, report) == Result::Success && report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                CHECK(report.pathAt(0)->closed && !report.pathAt(1)->closed);
                CHECK(near(static_cast<float>(std::fabs(_morphArea(report.pathAt(0)))), 900.0f, 0.1f));
                CHECK(near(report.pathAt(1)->paintBounds.center().x, 220.0f, 0.1f));
            }
        } else { delete source; delete target; }
        delete scene;
    }

    PathCommand ring[] = {move(-2, -2), line(2, -2), line(2, 2), line(-2, 2), PathCommand::close(),
                          move(-1, -1), line(-1, 1), line(1, 1), line(1, -1), PathCommand::close()};
    // A hole shrinks to its own centroid, or grows from it, quadratically in area and without
    // reversing its winding. Endpoint pixels are exactly those of the target on its own.
    for (auto grow : {false, true}) {
        auto scene = Scene::gen(config);
        auto frame = Path::gen(ring, 10u);
        auto square = Rectangle::gen({}, {4, 4});
        auto alone = Scene::gen(config);
        auto final = grow ? static_cast<Object*>(Path::gen(ring, 10u))
                          : static_cast<Object*>(Rectangle::gen({}, {4, 4}));
        CHECK(scene && frame && square && alone && final);
        if (scene && frame && square && alone && final) {
            for (auto object : {static_cast<Object*>(frame), static_cast<Object*>(square), final})
                object->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            auto from = grow ? static_cast<Object*>(square) : static_cast<Object*>(frame);
            auto to = grow ? static_cast<Object*>(frame) : static_cast<Object*>(square);
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            CHECK(alone->add(final) == Result::Success);
            CHECK(scene->play(Animation::morph(from, to), 1.0f, linear) == Result::Success);
            for (auto t : {0.0f, 0.25f, 0.5f, 0.75f, 0.999f}) {
                LayoutReport report;
                CHECK(renderer->layout(scene, t, report) == Result::Success && report.pathCount() == 2u);
                if (report.pathCount() != 2u) continue;
                auto outer = _morphArea(report.pathAt(0));
                auto hole = _morphArea(report.pathAt(1));
                auto scale = grow ? t : 1.0f - t;
                CHECK(near(static_cast<float>(std::fabs(hole)), 3600.0f * scale * scale, 0.1f));
                CHECK(hole == 0.0 || outer * hole < 0.0);
                auto center = report.pathAt(1)->paintBounds.center();
                CHECK(near(center.x, 160.0f, 0.1f) && near(center.y, 90.0f, 0.1f));
            }
            CHECK(near(_shownOpacity(renderer, scene, 0.5f, {}), 0.0f, 0.02f));
            Surface actual, expected;
            CHECK(renderer->render(scene, 1.0f, actual) == Result::Success);
            CHECK(renderer->render(alone, 0.0f, expected) == Result::Success);
            CHECK(checksum(actual) == checksum(expected));
            CHECK(renderer->render(scene, 0.25f, actual) == Result::Success);
            auto hash = checksum(actual);
            CHECK(renderer->render(scene, 0.75f, actual) == Result::Success);
            CHECK(renderer->render(scene, 0.25f, actual) == Result::Success && checksum(actual) == hash);
        } else { delete frame; delete square; delete final; }
        delete scene;
        delete alone;
    }

    // Filled open contours share even-odd coverage too; implicit closing edges still cut holes.
    {
        PathCommand open[] = {move(-2, -2), line(2, -2), line(2, 2), line(-2, 2),
                              move(-1, -1), line(-1, 1), line(1, 1), line(1, -1)};
        auto scene = Scene::gen(config);
        auto from = Path::gen(open, 8u);
        auto to = Path::gen(open, 8u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            from->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            to->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            CHECK(scene->play(Animation::morph(from, to), 1.0f, linear) == Result::Success);
            CHECK(near(_shownOpacity(renderer, scene, 0.5f, {}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 0.5f, {1.5f, 0, 0}), 1.0f, 0.02f));
        } else { delete from; delete to; }
        delete scene;
    }

    // An incoming stroke grows with its points rather than showing a round cap at time zero.
    {
        auto scene = Scene::gen(config);
        auto source = Line::gen({-2, 0, 0}, {-1, 0, 0});
        Vec3 points[] = {{-2, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {2, 0, 0}};
        uint32_t ends[] = {2, 4};
        auto target = Plot::gen(points, 4u, ends, 2u);
        CHECK(scene && source && target);
        if (scene && source && target) {
            source->stroke({0, 0, 0, 255});
            target->stroke({0, 0, 0, 255});
            CHECK(scene->add(source) == Result::Success);
            Surface before, after;
            CHECK(renderer->render(scene, 0.0f, before) == Result::Success);
            CHECK(scene->add(target) == Result::Success);
            CHECK(scene->play(Animation::morph(source, target), 1.0f, linear) == Result::Success);
            CHECK(renderer->render(scene, 0.0f, after) == Result::Success && checksum(before) == checksum(after));
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.25f, report) == Result::Success && report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                CHECK(near(report.pathAt(1)->strokeWidth, report.pathAt(0)->strokeWidth * 0.25f));
                CHECK(near(report.pathAt(1)->points[1].x - report.pathAt(1)->points[0].x, 7.5f));
            }
        } else { delete source; delete target; }
        delete scene;
    }

    // Copies keep all contours and array lag still delays each object pair.
    auto scene = Scene::gen(config);
    Vec3 two[] = {{-2, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    uint32_t ends[] = {2, 4};
    auto source = Plot::gen(two, 4u, ends, 2u);
    auto first = Plot::gen(two, 2u);
    auto second = Plot::gen(two, 4u, ends, 2u);
    CHECK(scene && source && first && second);
    if (scene && source && first && second) {
        CHECK(scene->add(source) == Result::Success);
        Object* copy = nullptr;
        CHECK(scene->clone(source, copy) == Result::Success);
        CHECK(copy && static_cast<Plot*>(copy)->contours() == 2u);
        CHECK(scene->add(first) == Result::Success && scene->add(second) == Result::Success);
        Animation animations[] = {Animation::morph(source, first), Animation::morph(source, second)};
        animations[0].copy = animations[1].copy = true;
        CHECK(scene->play(animations, 2u, 1.0f, linear, 0.25f) == Result::Success);
        CHECK(near(scene->duration(), 1.25f));
        BBox bounds;
        CHECK(renderer->bounds(scene, source, 1.25f, bounds) == Result::Success);
        CHECK(renderer->bounds(scene, first, 1.25f, bounds) == Result::Success);
        CHECK(renderer->bounds(scene, second, 1.25f, bounds) == Result::Success);
    } else { delete source; delete first; delete second; }
    delete scene;
}

void contouredPaths(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto move = [](float x, float y) { return PathCommand::move({x, y, 0.0f}); };
    auto line = [](float x, float y) { return PathCommand::line({x, y, 0.0f}); };

    // Each move starts a contour, and the contours close all or none.
    PathCommand ring[] = {move(-2, -2), line(2, -2), line(2, 2), line(-2, 2), PathCommand::close(),
                          move(-1, -1), line(1, -1), line(1, 1), line(-1, 1), PathCommand::close()};
    PathCommand strokes[] = {move(-2, -2), line(2, -2), move(-2, 2), line(2, 2)};
    PathCommand mixed[] = {move(0, 0), line(1, 0), line(1, 1), PathCommand::close(), move(2, 0), line(3, 0)};
    PathCommand after[] = {move(0, 0), line(1, 0), line(1, 1), PathCommand::close(), line(2, 2)};
    PathCommand lonely[] = {move(0, 0), move(1, 0), line(2, 0)};
    CHECK(Path::gen(mixed, 6u) == nullptr);
    CHECK(Path::gen(after, 5u) == nullptr);
    CHECK(Path::gen(lonely, 3u) == nullptr);
    auto pair = Path::gen(strokes, 4u);
    CHECK(pair && pair->contours() == 2u && !pair->closed() && pair->contourEnd(0) == 2u
          && pair->contourEnd(1) == 4u);
    delete pair;

    // The inner square runs the same way as the outer one and still cuts a hole.
    auto scene = Scene::gen(config);
    auto path = Path::gen(ring, 10u);
    auto circle = Circle::gen({}, 0.5f);
    CHECK(scene && path && circle);
    if (scene && path && circle) {
        CHECK(path->contours() == 2u && path->count() == 8u && path->closed());
        CHECK(path->contourEnd(0) == 4u && path->contourEnd(1) == 8u && path->contourEnd(2) == 8u);
        path->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        circle->opacity = 0.0f;
        CHECK(scene->add(path) == Result::Success && scene->add(circle) == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.0f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {1.5f, 0.0f, 0.0f}), 1.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {2.5f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(static_cast<float>(_inked(renderer, scene, 0.0f)), 10800.0f, 120.0f));
        BBox box;
        CHECK(renderer->bounds(scene, path, 0.0f, box) == Result::Success);
        CHECK(near(box.width, 120.0f, 0.5f) && near(box.height, 120.0f, 0.5f));
        // The layout report records each contour as a path of its own.
        LayoutReport report;
        CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
        CHECK(report.pathCount() == 2u);
        if (report.pathCount() == 2u) {
            CHECK(report.pathAt(0)->count == 4u && report.pathAt(1)->count == 4u);
            CHECK(report.pathAt(0)->closed && report.pathAt(1)->closed);
            CHECK(near(report.pathAt(0)->paintBounds.width, 120.0f, 0.5f));
            CHECK(near(report.pathAt(1)->paintBounds.width, 60.0f, 0.5f));
        }
        // Travel still needs one continuous outline.
        CHECK(scene->play(Animation::moveAlong(circle, path), 1.0f) == Result::InvalidArguments);
    } else {
        delete path;
        delete circle;
    }
    delete scene;

    // While it draws, every contour shows the same share of its own outline.
    scene = Scene::gen(config);
    path = Path::gen(ring, 10u);
    CHECK(scene && path);
    if (scene && path) {
        path->stroke({0, 0, 0, 255}).fill({0, 0, 0, 0});
        path->style.width = 6.0f;
        CHECK(scene->add(path) == Result::Success);
        CHECK(scene->play(Animation::create(path), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, -2.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, -1.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, 2.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, 1.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.0f, 1.0f, 0.0f}), 1.0f, 0.05f));
    } else {
        delete path;
    }
    delete scene;
}
