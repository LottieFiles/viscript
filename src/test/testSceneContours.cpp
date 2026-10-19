#include "testSceneChecks.h"

// Scalar contours join cell segments into loops and open pieces, refined against the field.
void contourPlots(Renderer* renderer)
{
    auto circle = [](double x, double y, double& value, void* data) noexcept {
        if (data) ++*static_cast<uint32_t*>(data);
        value = x * x + y * y;
        return true;
    };
    Range range{-1.5f, 1.5f, 0.125f};
    double values[25u * 25u];
    for (auto y = 0u; y < 25u; y++) {
        for (auto x = 0u; x < 25u; x++) {
            auto px = -1.5 + x * 0.125, py = -1.5 + y * 0.125;
            values[y * 25u + x] = px * px + py * py;
        }
    }
    Plot* plot = nullptr;
    uint32_t calls = 0;
    CHECK(Plot::contour(range, range, values, 625u, 1.0, plot, circle, &calls) == Result::Success);
    CHECK(plot && plot->contours() == 1u && plot->count() > 40u && calls > 100u);
    if (plot && plot->count()) {
        auto& first = plot->points()[0];
        auto& last = plot->points()[plot->count() - 1u];
        CHECK(first.x == last.x && first.y == last.y);
        for (auto i = 0u; i < plot->count(); i++) {
            auto& p = plot->points()[i];
            CHECK(std::fabs(static_cast<double>(p.x) * p.x + static_cast<double>(p.y) * p.y - 1.0) < 1.0e-6);
        }
        auto scene = Scene::gen();
        CHECK(scene && scene->add(plot) == Result::Success);
        if (scene) {
            CHECK(scene->play(Animation::create(plot), 0.5f) == Result::Success);
            CHECK(scene->play(Animation::indicate(plot), 0.5f) == Result::Success);
            CHECK(scene->emphasize(Emphasis::passingFlash(plot), 0.5f) == Result::Success);
            auto sampled = calls;
            LayoutReport a, b;
            CHECK(renderer->layout(scene, 0.5f, a) == Result::Success);
            CHECK(renderer->layout(scene, 1.25f, b) == Result::Success);
            CHECK(renderer->layout(scene, 0.5f, b) == Result::Success);
            CHECK(calls == sampled && a.pathCount() == b.pathCount() && a.pathCount() == 1u);
            if (a.pathCount() == 1u && b.pathCount() == 1u) {
                CHECK(a.pathAt(0)->count == b.pathAt(0)->count);
                CHECK(a.pathAt(0)->points[0].x == b.pathAt(0)->points[0].x);
            }
        } else delete plot;
        delete scene;
    } else delete plot;
    plot = nullptr;
    // Two wells have two loops in one plot, with no line joining them.
    auto wells = [](double x, double y, double& value, void*) noexcept {
        value = std::fmin((x - 1.0) * (x - 1.0), (x + 1.0) * (x + 1.0)) + y * y;
        return true;
    };
    Range wide{-2.0f, 2.0f, 0.125f};
    double pairs[33u * 25u];
    for (auto y = 0u; y < 25u; y++) {
        for (auto x = 0u; x < 33u; x++) wells(-2.0 + x * 0.125, -1.5 + y * 0.125, pairs[y * 33u + x], nullptr);
    }
    CHECK(Plot::contour(wide, range, pairs, 825u, 0.16, plot, wells) == Result::Success);
    CHECK(plot && plot->contours() == 2u);
    if (plot) {
        auto begin = 0u;
        for (auto piece = 0u; piece < plot->contours(); piece++) {
            auto end = plot->contourEnd(piece);
            CHECK(plot->points()[begin].x == plot->points()[end - 1u].x
                  && plot->points()[begin].y == plot->points()[end - 1u].y);
            for (auto i = begin; i < end; i++) {
                double value;
                wells(plot->points()[i].x, plot->points()[i].y, value, nullptr);
                CHECK(std::fabs(value - 0.16) < 1.0e-6);
            }
            begin = end;
        }
    }
    delete plot;
    Range cell{0.0f, 1.0f, 1.0f};
    // Both signs of the saddle determinant and its fixed zero tie pairing.
    for (auto pass = 0u; pass < 3u; pass++) {
        double saddle[] = {pass == 0u ? 2.0 : 1.0, -1.0, -1.0, pass == 1u ? 0.5 : 1.0};
        plot = nullptr;
        CHECK(Plot::contour(cell, cell, saddle, 4u, 0.0, plot) == Result::Success);
        CHECK(plot && plot->contours() == 2u && plot->count() == 4u);
        if (plot && plot->count() == 4u) {
            CHECK(plot->points()[0].y == 0.0f);
            CHECK(pass == 1u ? plot->points()[1].x == 0.0f : plot->points()[1].x == 1.0f);
        }
        delete plot;
    }
    plot = nullptr;
    CHECK(Plot::contour(range, range, values, 625u, 10.0, plot) == Result::Success);
    CHECK(plot && !plot->count() && !plot->contours());
    delete plot;
    plot = nullptr;
    CHECK(Plot::contour(range, range, values, 625u, 1.0, plot, circle, nullptr, 8u) == Result::NonSupport && !plot);
    CHECK(Plot::contour(cell, cell, values, 3u, 0.0, plot) == Result::InvalidArguments && !plot);
    CHECK(Plot::contour({0, 0, 1}, cell, values, 4u, 0.0, plot) == Result::InvalidArguments);
    CHECK(Plot::contour({0, 1000, 1}, {0, 1000, 1}, values, 4u, 0.0, plot) == Result::NonSupport);
    double invalid[] = {0, 1, NAN, 1};
    CHECK(Plot::contour(cell, cell, invalid, 4u, 0.5, plot) == Result::InvalidArguments);
    auto fails = [](double, double, double&, void*) noexcept { return false; };
    CHECK(Plot::contour(range, range, values, 625u, 1.0, plot, fails) == Result::InvalidArguments && !plot);
}

// Labels remain centered on sampled curves, with retained text and real stroke gaps.
void contourLabels(Renderer* renderer)
{
    Range range{-2.0f, 2.0f, 0.25f};
    double values[289];
    for (auto y = 0u; y < 17u; y++) for (auto x = 0u; x < 17u; x++) values[y * 17u + x] = -4.0 + (x + y) * 0.25;
    Plot* curve = nullptr;
    CHECK(Plot::contour(range, range, values, 289u, 0.0, curve) == Result::Success);
    if (!curve) return;
    curve->tag("labelled").stroke({120, 160, 220, 255}, 2.0f);
    ContourLabelOptions options;
    options.size = 0.2f;
    auto invalid = options;
    invalid.format = "%s";
    CHECK(curve->labelContours(invalid) == Result::InvalidArguments && !curve->childCount());
    invalid = options; invalid.spacing = -1.0f;
    CHECK(curve->labelContours(invalid) == Result::InvalidArguments && !curve->childCount());
    invalid = options; invalid.size = 0.0f;
    CHECK(curve->labelContours(invalid) == Result::InvalidArguments && !curve->childCount());
    CHECK(curve->labelContours(options) == Result::Success && curve->childCount() == 1u);
    auto label = curve->childCount() ? static_cast<Text*>(curve->childAt(0)) : nullptr;
    CHECK(label && label->type() == Type::Text && label->orientation == TextOrientation::Plane);
    CHECK(label && std::strcmp(label->tag(), "labelled/label/1") == 0 && std::strcmp(label->text(), "0.0") == 0);
    Config config;
    config.width = 320; config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    CHECK(scene && scene->add(curve) == Result::Success);
    if (!scene) { delete curve; return; }
    auto count = curve->count();
    auto moved = new Vec3[count];
    for (auto i = 0u; i < count; i++) moved[i] = curve->points()[i] + Vec3{0.5f, 0.5f, 0.0f};
    auto grid = curve->contourGrid();
    grid.level = 1.0;
    CHECK(scene->play(AnimationTarget::from(curve).geometry(moved, count, nullptr, 1u, nullptr, &grid),
                      1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    delete[] moved;
    CHECK(curve->labelContours(options) == Result::InsufficientCondition);
    LayoutReport initial, again;
    for (auto time : {0.5f, 1.0f, 0.0f, 0.5f}) {
        LayoutReport report;
        CHECK(renderer->layout(scene, time, report) == Result::Success);
        const LayoutObject* text = nullptr;
        auto textIndex = 0u, curveIndex = 0u;
        for (auto i = 0u; i < report.objectCount(); i++) {
            if (report.objectAt(i)->object == label) { text = report.objectAt(i); textIndex = i; }
            if (report.objectAt(i)->object == curve) curveIndex = i;
        }
        CHECK(text && text->visible);
        if (!text) continue;
        auto center = Vec2{text->paintBounds.x + text->paintBounds.width * 0.5f,
                           text->paintBounds.y + text->paintBounds.height * 0.5f};
        // The diagonal x+y=time projects to x-y=70+30*time in this camera.
        CHECK(std::fabs(center.x - center.y - 70.0f - 30.0f * time) / std::sqrt(2.0f) < 0.6f);
        CHECK(std::fabs(text->paintBounds.width - text->paintBounds.height) < 1.5f);
        auto paths = 0u;
        auto closest = 1000.0f;
        for (auto i = 0u; i < report.pathCount(); i++) {
            auto path = report.pathAt(i);
            if (report.visualAt(path->visual)->object != curveIndex) continue;
            paths++;
            for (auto j = 1u; j < path->count; j++) {
                auto a = path->points[j - 1u], b = path->points[j];
                auto dx = b.x - a.x, dy = b.y - a.y;
                auto den = dx * dx + dy * dy;
                auto t = den > 0.0f ? std::fmax(0.0f, std::fmin(1.0f, ((center.x-a.x)*dx + (center.y-a.y)*dy) / den)) : 0.0f;
                closest = std::fmin(closest, std::hypot(center.x-a.x-t*dx, center.y-a.y-t*dy));
            }
        }
        CHECK(paths == 2u && closest > 3.0f);
        char value[32];
        CHECK(scene->text(label, time, value, sizeof(value)) == 3u);
        CHECK(std::strcmp(value, time == 0.0f ? "0.0" : time == 1.0f ? "1.0" : "0.5") == 0);
        CHECK(textIndex != curveIndex);
    }
    CHECK(renderer->layout(scene, 0.5f, initial) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, again) == Result::Success);
    CHECK(renderer->layout(scene, 0.5f, again) == Result::Success);
    CHECK(initial.pathCount() == again.pathCount());
    for (auto i = 0u; i < initial.pathCount() && i < again.pathCount(); i++) {
        CHECK(initial.pathAt(i)->count == again.pathAt(i)->count);
        CHECK(initial.pathAt(i)->points[0].x == again.pathAt(i)->points[0].x);
    }
    Plot* directed = nullptr;
    CHECK(Plot::contour(range, range, values, 289u, 0.0, directed) == Result::Success);
    auto other = Scene::gen(config);
    if (directed && other) {
        directed->tag("directed").stroke({120, 160, 220, 255}, 2.0f);
        options.upright = false;
        CHECK(directed->labelContours(options) == Result::Success);
        CHECK(other->add(directed) == Result::Success);
        Surface upright, tracing;
        CHECK(renderer->render(scene, 0.0f, upright) == Result::Success);
        CHECK(renderer->render(other, 0.0f, tracing) == Result::Success);
        CHECK(checksum(upright) != checksum(tracing));
    } else delete directed;
    delete other;
    delete scene;

    // A closed curve gains repeat labels; collision boxes keep their text apart.
    for (auto y = 0u; y < 17u; y++) for (auto x = 0u; x < 17u; x++) {
        auto px = -2.0 + x * 0.25, py = -2.0 + y * 0.25;
        values[y * 17u + x] = px * px + py * py;
    }
    curve = nullptr;
    CHECK(Plot::contour(range, range, values, 289u, 1.0, curve) == Result::Success);
    if (!curve) return;
    curve->tag("loop"); options.spacing = 2.0f;
    CHECK(curve->labelContours(options) == Result::Success && curve->childCount() == 3u);
    scene = Scene::gen(config);
    CHECK(scene && scene->add(curve) == Result::Success);
    if (scene) {
        LayoutReport report;
        CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
        auto visible = 0u;
        for (auto i = 0u; i < report.objectCount(); i++) {
            auto object = report.objectAt(i);
            if (object->object->type() == Type::Text && object->visible) visible++;
        }
        CHECK(visible == 3u);
        for (auto i = 0u; i < report.collisionCount(); i++) {
            auto collision = report.collisionAt(i);
            CHECK(!(report.objectAt(collision->first)->object->type() == Type::Text
                    && report.objectAt(collision->second)->object->type() == Type::Text && collision->overlapping));
        }
    } else delete curve;
    delete scene;
}

void contourStates(Renderer* renderer)
{
    struct Field { double a = 1.0; bool wells = false; uint32_t calls = 0; } field;
    auto fn = [](double x, double y, double& value, void* data) noexcept {
        auto f = static_cast<Field*>(data);
        f->calls++;
        value = f->wells ? (x*x - 1.0)*(x*x - 1.0) + y*y : x*x/(f->a*f->a) + y*y - 1.0;
        return true;
    };
    Range x{-2.0f, 2.0f, 0.125f}, y{-1.5f, 1.5f, 0.125f};
    auto trace = [&](double level) {
        double values[825];
        for (auto row = 0u; row < 25u; row++) {
            for (auto column = 0u; column < 33u; column++) {
                fn(-2.0 + column*0.125, -1.5 + row*0.125, values[row*33u + column], &field);
            }
        }
        Plot* plot = nullptr;
        CHECK(Plot::contour(x, y, values, 825u, level, plot, fn, &field) == Result::Success);
        return plot;
    };
    auto scene = Scene::gen();
    field.a = 0.75;
    auto ellipse = trace(0.0);
    CHECK(scene && ellipse && scene->add(ellipse) == Result::Success);
    if (scene && ellipse) {
        for (auto i = 1u; i <= 4u; i++) {
            field.a = 0.75 + i*0.1875;
            auto next = trace(0.0);
            CHECK(next && scene->play(AnimationTarget::from(ellipse).geometry(next->points(), next->count(),
                  nullptr, next->contours()), 0.25f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
            delete next;
        }
        auto calls = field.calls;
        // Copies expose the actual sampled geometry, without projection rounding.
        uint32_t order[] = {4u, 0u, 2u, 1u, 3u};
        for (auto i : order) {
            CHECK(scene->seek(i*0.25f) == Result::Success);
            Object* copy = nullptr;
            CHECK(scene->clone(ellipse, copy) == Result::Success);
            auto sampled = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
            CHECK(sampled && sampled->contours() == 1u && sampled->count() > 20u);
            auto a = 0.75 + i*0.1875;
            for (auto j = 0u; sampled && j < sampled->count(); j++) {
                auto& p = sampled->points()[j];
                CHECK(std::fabs(static_cast<double>(p.x)*p.x/(a*a) + static_cast<double>(p.y)*p.y - 1.0) < 1.0e-6);
            }
            LayoutReport report;
            CHECK(renderer->layout(scene, i*0.25f, report) == Result::Success);
        }
        CHECK(field.calls == calls);
    } else delete ellipse;
    delete scene;

    scene = Scene::gen();
    field.wells = true;
    auto curve = trace(0.5);
    CHECK(scene && curve && curve->contours() == 2u && scene->add(curve) == Result::Success);
    if (scene && curve) {
        double levels[] = {1.1, -0.2, 0.5};
        for (auto level : levels) {
            auto next = trace(level);
            uint32_t ends[2];
            for (auto i = 0u; next && i < next->contours(); i++) ends[i] = next->contourEnd(i);
            CHECK(next && scene->play(AnimationTarget::from(curve).geometry(next->points(), next->count(),
                  next->contours() > 1u ? ends : nullptr, next->contours()), 0.25f,
                  AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
            delete next;
        }
        auto calls = field.calls;
        float times[] = {0.7f, 0.1f, 0.2f, 0.35f, 0.45f, 0.6f, 0.75f};
        uint32_t pieces[] = {2u, 2u, 1u, 1u, 0u, 0u, 2u};
        for (auto i = 0u; i < 7u; i++) {
            LayoutReport report;
            CHECK(renderer->layout(scene, times[i], report) == Result::Success);
            CHECK(report.pathCount() == pieces[i]);
        }
        CHECK(field.calls == calls);
    } else delete curve;
    delete scene;
}

// A plot in pieces draws nothing between them, ends them flat when it asks to, and draws on
// through one piece after another.
void plotEndpoints(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    const Vec3 points[] = {{-3, -1, 0}, {-1, -1, 0}, {1, 1, 0}, {3, 1, 0}};
    const uint32_t ends[] = {2u, 4u};
    CHECK(Plot::gen(points, 1u) == nullptr);
    CHECK(Plot::gen(nullptr, 0u) == nullptr);
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    for (auto pass = 0u; pass < 3u; pass++) {
        auto scene = Scene::gen(config);
        auto plot = Plot::gen(points, 4u, ends, 2u);
        CHECK(scene && plot);
        if (!scene || !plot) { delete scene; delete plot; continue; }
        plot->progress = 0.0f;
        plot->stroke({0, 0, 0, 255}, 0.0f);
        plot->endpointRadius = pass ? 4.0f : 0.0f;
        CHECK(scene->add(plot) == Result::Success);
        auto shown = [&](float time, Vec3 point) { return _shownOpacity(renderer, scene, time, point); };
        BBox bounds;
        if (!pass) {
            CHECK(near(shown(0, points[0]), 0, 0.02f));
            CHECK(renderer->bounds(scene, plot, 0, bounds) == Result::InsufficientCondition);
        } else {
            // At manually set progress 0 only the active vertex has paint and bounds.
            CHECK(shown(0, points[0]) > 0.9f && near(shown(0, points[1]), 0, 0.02f));
            CHECK(renderer->bounds(scene, plot, 0, bounds) == Result::Success);
            CHECK(near(bounds.width, 8, 0.5f) && near(bounds.height, 8, 0.5f));
            plot->endpointRadius = 8;
            CHECK(renderer->bounds(scene, plot, 0, bounds) == Result::InvalidArguments);
            plot->endpointRadius = 4;
            Object* copy = nullptr;
            CHECK(scene->clone(plot, copy) == Result::Success);
            CHECK(copy && static_cast<Plot*>(copy)->endpointRadius == 4.0f);
            CHECK(scene->remove(copy) == Result::Success);
            if (pass == 1u) {
                // Segment progress stops on the preceding piece at an exact gap boundary.
                CHECK(scene->play(AnimationTarget::from(plot).progress(1), 1, linear) == Result::Success);
                CHECK(shown(0.25f, {-2, -1, 0}) > 0.9f);
                CHECK(shown(0.5f, points[1]) > 0.9f && near(shown(0.5f, points[2]), 0, 0.02f));
                CHECK(shown(0.75f, {2, 1, 0}) > 0.9f && shown(1, points[3]) > 0.9f);
                CHECK(shown(0.25f, {-2, -1, 0}) > 0.9f);
                CHECK(scene->play(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 0u), 1, linear) == Result::Success);
                CHECK(renderer->bounds(scene, plot, 2, bounds) == Result::InsufficientCondition);
                Object* empty = nullptr;
                CHECK(scene->clone(plot, empty) == Result::Success);
                CHECK(empty && static_cast<Plot*>(empty)->count() == 0u
                      && static_cast<Plot*>(empty)->endpointRadius == 4.0f);
            } else {
                CHECK(scene->seek(1) == Result::Success);
                CHECK(scene->play(Animation::create(plot, DrawDirection::Reverse), 1, linear) == Result::Success);
                CHECK(near(shown(0, points[0]), 0, 0.02f) && near(shown(1, points[3]), 0, 0.02f));
                CHECK(shown(1.25f, {2, 1, 0}) > 0.9f && shown(2, points[0]) > 0.9f);
                CHECK(scene->play(Animation::uncreate(plot, DrawDirection::Reverse), 1, linear) == Result::Success);
                CHECK(near(shown(3, points[3]), 0, 0.02f));
                CHECK(scene->update(AnimationTarget::from(plot).progress(0)) == Result::Success);
                CHECK(shown(3, points[0]) > 0.9f);
            }
        }
        delete scene;
    }
    // Invalid static options fail attachment without transferring ownership.
    for (auto radius : {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto scene = Scene::gen(config);
        auto plot = Plot::gen(points, 2u);
        plot->endpointRadius = radius;
        CHECK(scene->add(plot) == Result::InvalidArguments);
        plot->endpointRadius = 4;
        CHECK(scene->add(plot) == Result::Success);
        delete scene;
    }
    auto scene = Scene::gen(config);
    auto group = Group::gen();
    auto plot = Plot::gen(points, 2u);
    plot->endpointRadius = 4;
    plot->progress = 0;
    plot->stroke({0, 0, 0, 255}, 0);
    group->opacity = 0.5f;
    group->layer = 7;
    CHECK(group->add(plot) == Result::Success && scene->add(group) == Result::Success);
    CHECK(near(_shownOpacity(renderer, scene, 0, points[0]), 0.5f, 0.02f));
    LayoutReport report;
    CHECK(renderer->layout(scene, 0, report) == Result::Success);
    CHECK(report.objectCount() == 2u && report.objectAt(1)->visible
          && report.objectAt(1)->layer == 0);
    CHECK(scene->play(AnimationTarget::from(group).layer(9), 1, linear) == Result::Success);
    CHECK(renderer->layout(scene, 1, report) == Result::Success);
    CHECK(report.objectAt(1)->visible && report.objectAt(1)->layer == 2);
    CHECK(scene->play(AnimationTarget::from(group).opacity(0), 1, linear) == Result::Success);
    CHECK(near(_shownOpacity(renderer, scene, 2, points[0]), 0, 0.02f));
    BBox bounds;
    CHECK(renderer->bounds(scene, plot, 2, bounds) == Result::InsufficientCondition);
    delete scene;
}

void plotEndpointColor(Renderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 100;
    config.camera.orthoHeight = 5;
    config.background = {255, 255, 255, 255};
    const Vec3 points[] = {{-2, 0, 0}, {2, 0, 0}};
    auto scene = Scene::gen(config);
    auto plot = Plot::gen(points, 2u);
    CHECK(scene && plot);
    if (!scene || !plot) { delete scene; delete plot; return; }
    plot->stroke({255, 0, 0, 255}, 4);
    plot->endpointRadius = 6;
    Gradient gradient;
    gradient.type = GradientType::Linear;
    gradient.stopCount = 2;
    gradient.stops[0] = {0, {0, 0, 255, 255}};
    gradient.stops[1] = {1, {0, 255, 0, 255}};
    CHECK(plot->gradient(gradient) == Result::Success);
    CHECK(scene->add(plot) == Result::Success);
    const Object* candidates[] = {plot};
    PixelSample sample;
    CHECK(renderer->sample(scene, 0, {120.5f, 50.5f}, candidates, 1u, sample) == Result::Success);
    CHECK(sample.color.r == 255 && sample.color.g == 0 && sample.color.b == 0 && sample.color.a == 255);
    // The marker's solid fill leaves the Plot's blue/green gradient stroke unchanged.
    CHECK(renderer->sample(scene, 0, {80.5f, 50.5f}, candidates, 1u, sample) == Result::Success);
    CHECK(sample.color.r == 0 && sample.color.g > 0 && sample.color.b > 0 && sample.color.a == 255);
    CHECK(plot->style.gradient.enabled());
    CHECK(scene->wait(1) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).progress(0), 1, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(renderer->sample(scene, 2, {40.5f, 50.5f}, candidates, 1u, sample) == Result::Success);
    CHECK(sample.color.r == 255 && sample.color.g == 0 && sample.color.b == 0 && sample.color.a == 255);
    delete scene;
}

void plotEndpointProgress(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    const Vec3 points[] = {{-2, -1, 0}, {2, -1, 0}};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto make = [&](float progress) {
        auto plot = Plot::gen(points, 2u);
        plot->endpointRadius = 4;
        plot->progress = progress;
        plot->stroke({0, 0, 0, 255}, 0);
        return plot;
    };
    // Explicit animated progress retains the zero-progress vertex before and after the clip.
    for (auto pass = 0u; pass < 3u; pass++) {
        auto scene = Scene::gen(config);
        auto plot = make(pass == 0u ? 1 : 0);
        auto target = pass == 1u ? 1 : 0;
        CHECK(scene->add(plot) == Result::Success && scene->wait(1) == Result::Success);
        CHECK(scene->play(AnimationTarget::from(plot).progress(target), 1, linear) == Result::Success);
        CHECK(scene->wait(1) == Result::Success);
        CHECK(_shownOpacity(renderer, scene, 0, points[pass == 0u ? 1 : 0]) > 0.9f);
        CHECK(_shownOpacity(renderer, scene, 1, points[pass == 0u ? 1 : 0]) > 0.9f);
        CHECK(_shownOpacity(renderer, scene, 2, points[target ? 1 : 0]) > 0.9f);
        CHECK(_shownOpacity(renderer, scene, 3, points[target ? 1 : 0]) > 0.9f);
        delete scene;
    }
    auto scene = Scene::gen(config);
    auto plot = make(1);
    CHECK(scene->add(plot) == Result::Success);
    CHECK(scene->play(Animation::uncreate(plot), 1, linear) == Result::Success);
    CHECK(scene->wait(0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).progress(0), 1, linear) == Result::Success);
    CHECK(scene->wait(1) == Result::Success);
    CHECK(scene->play(Animation::create(plot), 1, linear) == Result::Success);
    CHECK(near(_shownOpacity(renderer, scene, 1.25f, points[0]), 0, 0.02f));
    CHECK(_shownOpacity(renderer, scene, 1.5f, points[0]) > 0.9f);
    CHECK(_shownOpacity(renderer, scene, 2.5f, points[0]) > 0.9f);
    CHECK(near(_shownOpacity(renderer, scene, 3.5f, points[0]), 0, 0.02f));
    CHECK(_shownOpacity(renderer, scene, 4.5f, points[1]) > 0.9f);
    delete scene;

    // A first create retroactively hides even explicit progress clips before its start.
    scene = Scene::gen(config);
    plot = make(0);
    CHECK(scene->add(plot) == Result::Success && scene->wait(1) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).progress(0), 1, linear) == Result::Success);
    CHECK(scene->wait(1) == Result::Success);
    CHECK(scene->play(Animation::create(plot), 1, linear) == Result::Success);
    for (auto time : {0.0f, 1.0f, 2.0f, 3.0f})
        CHECK(near(_shownOpacity(renderer, scene, time, points[0]), 0, 0.02f));
    CHECK(_shownOpacity(renderer, scene, 3.5f, {0, -1, 0}) > 0.9f);
    CHECK(_shownOpacity(renderer, scene, 4, points[1]) > 0.9f);
    delete scene;

    // Group progress reaches its child; ordinary Space progress does not.
    for (auto pass = 0u; pass < 2u; pass++) {
        scene = Scene::gen(config);
        Object* parent = pass ? static_cast<Object*>(Space::gen()) : static_cast<Object*>(Group::gen());
        parent->stroke({0, 0, 0, 0}, 0);
        plot = make(0);
        CHECK(parent->add(plot) == Result::Success && scene->add(parent) == Result::Success);
        CHECK(scene->wait(1) == Result::Success);
        CHECK(scene->play(Animation::create(parent), 1, linear) == Result::Success);
        auto shown = [&](float time) { return _shownOpacity(renderer, scene, time, points[0]); };
        CHECK(pass ? shown(0) > 0.9f : near(shown(0), 0, 0.02f));
        CHECK(pass ? shown(1) > 0.9f : near(shown(1), 0, 0.02f));
        CHECK(shown(1.5f) > 0.9f && shown(2) > 0.9f);
        CHECK(scene->play(Animation::uncreate(parent), 1, linear) == Result::Success);
        CHECK(shown(2.5f) > 0.9f);
        CHECK(pass ? shown(3) > 0.9f : near(shown(3), 0, 0.02f));
        CHECK(scene->wait(1) == Result::Success);
        CHECK(scene->play(Animation::create(parent), 1, linear) == Result::Success);
        CHECK(pass ? shown(4) > 0.9f : near(shown(4), 0, 0.02f));
        CHECK(shown(4.5f) > 0.9f && shown(5) > 0.9f);
        delete scene;
    }
}

void plotPieces(Renderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    // Two runs of three segments, a unit below the axis and a unit above it.
    Vec3 points[] = {{-4.0f, -1.0f, 0.0f}, {-3.0f, -1.0f, 0.0f}, {-2.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f},
                     {1.0f, 1.0f, 0.0f},   {2.0f, 1.0f, 0.0f},   {3.0f, 1.0f, 0.0f},   {4.0f, 1.0f, 0.0f}};
    uint32_t ends[] = {4u, 8u};
    uint32_t lonely[] = {1u, 8u};
    uint32_t early[] = {4u, 7u};
    uint32_t falling[] = {6u, 4u};
    CHECK(Plot::gen(points, 8u, lonely, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, early, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, falling, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, nullptr, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, ends, 0u) == nullptr);
    auto whole = Plot::gen(points, 8u, nullptr, 1u);
    CHECK(whole && whole->contours() == 1u && whole->contourEnd(0) == 8u && !whole->flat);
    delete whole;
    auto make = [&](bool flat, float width) {
        auto plot = Plot::gen(points, 8u, ends, 2u);
        if (plot) {
            plot->stroke({0, 0, 0, 255}, width);
            plot->flat = flat;
        }
        return plot;
    };
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);

    auto scene = Scene::gen(config);
    auto plot = make(true, 4.0f);
    auto circle = Circle::gen({}, 0.5f);
    CHECK(scene && plot && circle);
    if (scene && plot && circle) {
        circle->opacity = 0.0f;
        CHECK(scene->add(plot) == Result::Success && scene->add(circle) == Result::Success);
        CHECK(plot->contours() == 2u && plot->contourEnd(0) == 4u && plot->contourEnd(1) == 8u);
        // Nothing joins (-1, -1) to (1, 1).
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {-2.5f, -1.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {2.5f, 1.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {-0.5f, -0.5f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.0f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.5f, 0.5f, 0.0f}), 0.0f, 0.02f));
        // Travel and a flash still need one continuous outline.
        CHECK(scene->play(Animation::moveAlong(circle, plot), 1.0f) == Result::InvalidArguments);
        CHECK(scene->emphasize(Emphasis::passingFlash(plot), 1.0f) == Result::InvalidArguments);
        // A copy keeps the pieces and the flat ends.
        Object* copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        auto twin = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
        CHECK(twin && twin->flat && twin->count() == 8u && twin->contours() == 2u && twin->contourEnd(0) == 4u);
    } else {
        delete plot;
        delete circle;
    }
    delete scene;

    // A flat end stops at its last point; a round one reaches half the width past it. The layout
    // report records each piece as a path of its own.
    for (auto pass = 0u; pass < 2u; pass++) {
        auto flat = pass == 0u;
        scene = Scene::gen(config);
        plot = make(flat, 12.0f);
        CHECK(scene && plot);
        if (scene && plot) {
            CHECK(scene->add(plot) == Result::Success);
            auto past = _shownOpacity(renderer, scene, 0.0f, {4.1f, 1.0f, 0.0f});
            CHECK(flat ? near(past, 0.0f, 0.02f) : past > 0.9f);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
            CHECK(report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                CHECK(report.pathAt(0)->count == 4u && !report.pathAt(0)->closed);
                CHECK(report.pathAt(1)->count == 4u && near(report.pathAt(1)->points[0].x, 190.0f, 0.5f));
            }
        } else {
            delete plot;
        }
        delete scene;
    }

    // While it draws, progress runs through the six segments in turn and the gap takes none
    // of it; reversed, it starts at the end of the last piece.
    for (auto pass = 0u; pass < 2u; pass++) {
        auto reverse = pass == 1u;
        scene = Scene::gen(config);
        plot = make(true, 6.0f);
        CHECK(scene && plot);
        if (scene && plot) {
            CHECK(scene->add(plot) == Result::Success);
            auto direction = reverse ? DrawDirection::Reverse : DrawDirection::Forward;
            CHECK(scene->play(Animation::create(plot, direction), 1.0f, linear) == Result::Success);
            auto sign = reverse ? -1.0f : 1.0f;
            auto shown = [&](float time, float x, float y) {
                return _shownOpacity(renderer, scene, time, {sign * x, sign * y, 0.0f});
            };
            CHECK(near(shown(0.25f, -3.5f, -1.0f), 1.0f, 0.05f));
            CHECK(near(shown(0.25f, -2.0f, -1.0f), 0.0f, 0.02f));
            CHECK(near(shown(0.25f, 1.5f, 1.0f), 0.0f, 0.02f));
            CHECK(near(shown(0.5f, -1.2f, -1.0f), 1.0f, 0.05f));
            CHECK(near(shown(0.5f, 1.2f, 1.0f), 0.0f, 0.02f));
            CHECK(near(shown(0.75f, 2.0f, 1.0f), 1.0f, 0.05f));
            CHECK(near(shown(0.75f, 3.0f, 1.0f), 0.0f, 0.02f));
            CHECK(near(shown(1.0f, 3.5f, 1.0f), 1.0f, 0.05f));
        } else {
            delete plot;
        }
        delete scene;
    }

    // A warp carries each piece on its own, and still nothing joins them.
    scene = Scene::gen(config);
    plot = make(true, 4.0f);
    CHECK(scene && plot);
    if (scene && plot) {
        CHECK(scene->add(plot) == Result::Success);
        Vec3 up = {0.0f, 1.0f, 0.0f};
        Object* targets[] = {plot};
        CHECK(scene->warp(targets, 1u, _moved, &up, 1.0f) == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {-2.5f, 0.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {2.5f, 2.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.0f, 1.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.5f, 1.5f, 0.0f}), 0.0f, 0.02f));
    } else {
        delete plot;
    }
    delete scene;
}

// The area a path encloses, its counterclockwise contours adding and its clockwise ones taking
// away; only contour when one is named.
static double _pathArea(const Path* path, uint32_t contour = UINT32_MAX)
{
    auto area = 0.0;
    auto start = 0u;
    for (auto i = 0u; i < path->contours(); i++) {
        auto end = path->contourEnd(i);
        for (auto j = start; (contour == UINT32_MAX || contour == i) && j < end; j++) {
            auto& a = path->points()[j];
            auto& b = path->points()[j + 1u < end ? j + 1u : start];
            area += 0.5 * (static_cast<double>(a.x) * b.y - static_cast<double>(a.y) * b.x);
        }
        start = end;
    }
    return area;
}

static Result _combined(Object* a, Object* b, PathOperation operation, Path*& output,
                        const Object* parent = nullptr)
{
    const Object* shapes[] = {a, b};
    return Path::combine(shapes, 2u, operation, parent, output);
}

void combinedShapes(Renderer* renderer)
{
    // Two unit disks a unit apart: whatever the operation, the areas add up as sets do.
    auto left = Circle::gen({-0.5f, 0.0f, 0.0f}, 1.0f);
    auto right = Circle::gen({0.5f, 0.0f, 0.0f}, 1.0f);
    Path* lens = nullptr;
    Path* both = nullptr;
    Path* crescent = nullptr;
    Path* apart = nullptr;
    CHECK(left && right);
    if (left && right) {
        CHECK(_combined(left, right, PathOperation::Intersection, lens) == Result::Success);
        CHECK(_combined(left, right, PathOperation::Union, both) == Result::Success);
        CHECK(_combined(left, right, PathOperation::Difference, crescent) == Result::Success);
        CHECK(_combined(left, right, PathOperation::Exclusion, apart) == Result::Success);
    }
    CHECK(lens && both && crescent && apart);
    if (lens && both && crescent && apart) {
        // A circle draws as a regular polygon of 64 sides.
        auto disk = 32.0 * std::sin(6.283185307179586 / 64.0);
        auto shared = _pathArea(lens);
        // Two unit circles a unit apart share 2 pi / 3 - sqrt(3) / 2 of area.
        CHECK(near(static_cast<float>(shared), 1.22837f, 0.01f));
        CHECK(lens->closed() && lens->contours() == 1u && both->contours() == 1u && crescent->contours() == 1u);
        CHECK(near(static_cast<float>(_pathArea(both) + shared), static_cast<float>(2.0 * disk), 1.0e-4f));
        CHECK(near(static_cast<float>(_pathArea(crescent) + shared), static_cast<float>(disk), 1.0e-4f));
        CHECK(near(static_cast<float>(_pathArea(apart)), static_cast<float>(_pathArea(both) - shared), 1.0e-4f));
        // A shape joined with itself, all of its edges on top of each other, is itself.
        Path* same = nullptr;
        CHECK(_combined(left, left, PathOperation::Union, same) == Result::Success);
        CHECK(same && same->contours() == 1u && same->count() == 64u);
        if (same) CHECK(near(static_cast<float>(_pathArea(same)), static_cast<float>(disk), 1.0e-4f));
        delete same;
        // The two crescents only touch where the circles cross, so they stay two contours.
        CHECK(apart->contours() == 2u);
        auto east = -10.0f;
        for (auto i = 0u; i < crescent->count(); i++)
            east = std::fmax(east, crescent->points()[i].x);
        CHECK(near(east, 0.0f, 1.0e-3f));
        Bounds box;
        CHECK(lens->bounds(box));
        CHECK(near(box.min.x, -0.5f, 1.0e-3f) && near(box.max.x, 0.5f, 1.0e-3f));
        CHECK(near(box.max.y, 0.866f, 0.01f) && near(box.min.y, -0.866f, 0.01f));
    }
    delete lens;
    delete both;
    delete crescent;
    delete apart;

    // A square inside another leaves a ring: the outer contour runs counterclockwise and the
    // hole clockwise.
    auto outer = Rectangle::gen({}, {4.0f, 4.0f});
    auto inner = Rectangle::gen({}, {2.0f, 2.0f});
    Path* ring = nullptr;
    CHECK(outer && inner);
    if (outer && inner) CHECK(_combined(outer, inner, PathOperation::Difference, ring) == Result::Success);
    CHECK(ring && ring->contours() == 2u && ring->count() == 8u);
    if (ring && ring->contours() == 2u) {
        auto first = _pathArea(ring, 0u);
        auto second = _pathArea(ring, 1u);
        CHECK(near(static_cast<float>(first > 0.0 ? first : second), 16.0f, 1.0e-4f));
        CHECK(near(static_cast<float>(first > 0.0 ? second : first), -4.0f, 1.0e-4f));
    }
    Path* empty = nullptr;
    if (outer && inner) {
        // Nothing is left of a shape taken from itself, nor of a shape inside a hole.
        CHECK(_combined(inner, outer, PathOperation::Difference, empty) == Result::InsufficientCondition);
        CHECK(empty == nullptr);
        CHECK(_combined(outer, outer, PathOperation::Exclusion, empty) == Result::InsufficientCondition);
        if (ring) {
            auto core = Rectangle::gen({}, {1.0f, 1.0f});
            CHECK(core && _combined(ring, core, PathOperation::Intersection, empty) == Result::InsufficientCondition);
            delete core;
        }
    }

    // Squares sharing an edge become one rectangle, the points along its long sides gone.
    auto west = Rectangle::gen({-0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto east = Rectangle::gen({0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto corner = Rectangle::gen({1.5f, 1.0f, 0.0f}, {1.0f, 1.0f});
    auto far = Rectangle::gen({5.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(west && east && corner && far);
    if (west && east && corner && far) {
        Path* joined = nullptr;
        CHECK(_combined(west, east, PathOperation::Union, joined) == Result::Success);
        CHECK(joined && joined->contours() == 1u && joined->count() == 4u);
        if (joined) CHECK(near(static_cast<float>(_pathArea(joined)), 2.0f, 1.0e-4f));
        delete joined;
        joined = nullptr;
        // Squares meeting at a corner stay two contours.
        CHECK(_combined(east, corner, PathOperation::Union, joined) == Result::Success);
        CHECK(joined && joined->contours() == 2u);
        if (joined) CHECK(near(static_cast<float>(_pathArea(joined)), 2.0f, 1.0e-4f));
        delete joined;
        joined = nullptr;
        CHECK(_combined(west, far, PathOperation::Union, joined) == Result::Success);
        CHECK(joined && joined->contours() == 2u);
        delete joined;
        CHECK(_combined(west, far, PathOperation::Intersection, empty) == Result::InsufficientCondition);
    }

    // Edges that run along each other part where either ends, and a corner that lands on an
    // edge parts it there.
    auto wide = Rectangle::gen({1.0f, 0.5f, 0.0f}, {2.0f, 1.0f});
    auto shifted = Rectangle::gen({2.0f, 0.5f, 0.0f}, {2.0f, 1.0f});
    auto unit = Rectangle::gen({0.5f, 0.5f, 0.0f}, {1.0f, 1.0f});
    auto tab = Rectangle::gen({1.5f, 0.5f, 0.0f}, {1.0f, 0.5f});
    CHECK(wide && shifted && unit && tab);
    if (wide && shifted && unit && tab) {
        PathOperation operations[] = {PathOperation::Union, PathOperation::Intersection, PathOperation::Difference,
                                      PathOperation::Exclusion};
        float areas[] = {3.0f, 1.0f, 1.0f, 2.0f};
        uint32_t contours[] = {1u, 1u, 1u, 2u};
        for (auto i = 0u; i < 4u; i++) {
            Path* overlap = nullptr;
            CHECK(_combined(wide, shifted, operations[i], overlap) == Result::Success);
            CHECK(overlap && overlap->contours() == contours[i] && overlap->count() == 4u * contours[i]);
            if (overlap) CHECK(near(static_cast<float>(_pathArea(overlap)), areas[i], 1.0e-4f));
            delete overlap;
        }
        Path* tabbed = nullptr;
        CHECK(_combined(unit, tab, PathOperation::Union, tabbed) == Result::Success);
        CHECK(tabbed && tabbed->contours() == 1u && tabbed->count() == 8u);
        if (tabbed) CHECK(near(static_cast<float>(_pathArea(tabbed)), 1.5f, 1.0e-4f));
        delete tabbed;
    }
    delete wide;
    delete shifted;
    delete unit;
    delete tab;

    // A square and the same square turned an eighth of a turn share a regular octagon.
    auto square = Rectangle::gen({}, {2.0f, 2.0f});
    auto turned = Rectangle::gen({}, {2.0f, 2.0f});
    CHECK(square && turned);
    if (square && turned) {
        turned->model = Mat4::rotateZ(0.78539816f);
        Path* octagon = nullptr;
        CHECK(_combined(square, turned, PathOperation::Intersection, octagon) == Result::Success);
        CHECK(octagon && octagon->contours() == 1u && octagon->count() == 8u);
        if (octagon) CHECK(near(static_cast<float>(_pathArea(octagon)), 3.31371f, 1.0e-4f));
        delete octagon;
    }

    // A path of several contours covers by the even-odd rule, its hole included.
    PathCommand rings[] = {PathCommand::move({-2.0f, -2.0f, 0.0f}), PathCommand::line({2.0f, -2.0f, 0.0f}),
                           PathCommand::line({2.0f, 2.0f, 0.0f}), PathCommand::line({-2.0f, 2.0f, 0.0f}),
                           PathCommand::close(), PathCommand::move({-1.0f, -1.0f, 0.0f}),
                           PathCommand::line({1.0f, -1.0f, 0.0f}), PathCommand::line({1.0f, 1.0f, 0.0f}),
                           PathCommand::line({-1.0f, 1.0f, 0.0f}), PathCommand::close()};
    auto holed = Path::gen(rings, 10u);
    auto middle = Rectangle::gen({}, {3.0f, 3.0f});
    CHECK(holed && middle);
    if (holed && middle) {
        Path* frame = nullptr;
        CHECK(_combined(holed, middle, PathOperation::Intersection, frame) == Result::Success);
        CHECK(frame && frame->contours() == 2u);
        if (frame) CHECK(near(static_cast<float>(_pathArea(frame)), 5.0f, 1.0e-4f));
        delete frame;
    }

    // More shapes fold from the left: a square less two others, and the part three share.
    auto pieces = Rectangle::gen({}, {4.0f, 4.0f});
    auto bite = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto nibble = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto up = Rectangle::gen({0.5f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto over = Rectangle::gen({0.0f, 0.5f, 0.0f}, {2.0f, 2.0f});
    CHECK(pieces && bite && nibble && up && over);
    if (pieces && bite && nibble && up && over && square) {
        const Object* taken[] = {pieces, bite, nibble};
        Path* holes = nullptr;
        CHECK(Path::combine(taken, 3u, PathOperation::Difference, nullptr, holes) == Result::Success);
        CHECK(holes && holes->contours() == 3u);
        if (holes) CHECK(near(static_cast<float>(_pathArea(holes)), 14.0f, 1.0e-4f));
        delete holes;
        const Object* shared[] = {square, up, over};
        Path* common = nullptr;
        CHECK(Path::combine(shared, 3u, PathOperation::Intersection, nullptr, common) == Result::Success);
        CHECK(common && common->contours() == 1u);
        if (common) CHECK(near(static_cast<float>(_pathArea(common)), 2.25f, 1.0e-4f));
        delete common;
    }

    // The result lies in the space of the parent it is given, or of the world.
    auto group = Group::gen();
    auto first = Rectangle::gen({-0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto second = Rectangle::gen({0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(group && first && second);
    if (group && first && second) {
        group->model = Mat4::translate({3.0f, 0.0f, 0.0f}) * Mat4::scale({2.0f, 2.0f, 1.0f});
        CHECK(group->add(first) == Result::Success && group->add(second) == Result::Success);
        Path* local = nullptr;
        Path* world = nullptr;
        CHECK(_combined(first, second, PathOperation::Union, local, group) == Result::Success);
        CHECK(_combined(first, second, PathOperation::Union, world) == Result::Success);
        Bounds box;
        CHECK(local && local->bounds(box) && near(box.min.x, -1.0f, 1.0e-4f) && near(box.max.x, 1.0f, 1.0e-4f));
        CHECK(world && world->bounds(box) && near(box.min.x, 1.0f, 1.0e-4f) && near(box.max.x, 5.0f, 1.0e-4f));
        if (local && world) {
            CHECK(near(static_cast<float>(_pathArea(local)), 2.0f, 1.0e-4f));
            CHECK(near(static_cast<float>(_pathArea(world)), 8.0f, 1.0e-4f));
        }
        delete local;
        delete world;
    } else {
        delete first;
        delete second;
    }
    delete group;

    // Only closed shapes enclose a region, and it takes two of them.
    Vec3 wave[] = {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    auto plot = Plot::gen(wave, 3u);
    auto segment = Line::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    CHECK(plot && segment && square);
    if (plot && segment && square) {
        CHECK(_combined(square, plot, PathOperation::Union, empty) == Result::InvalidArguments);
        CHECK(_combined(segment, square, PathOperation::Union, empty) == Result::InvalidArguments);
        CHECK(_combined(square, nullptr, PathOperation::Union, empty) == Result::InvalidArguments);
        CHECK(_combined(square, turned, static_cast<PathOperation>(9), empty) == Result::InvalidArguments);
        const Object* alone[] = {square};
        CHECK(Path::combine(alone, 1u, PathOperation::Union, nullptr, empty) == Result::InvalidArguments);
        CHECK(empty == nullptr);
    }
    delete plot;
    delete segment;
    delete pieces;
    delete bite;
    delete nibble;
    delete up;
    delete over;
    delete holed;
    delete middle;
    delete square;
    delete turned;
    delete west;
    delete east;
    delete corner;
    delete far;
    delete outer;
    delete inner;
    delete ring;
    delete left;
    delete right;

    // The result draws, animates and morphs like any path.
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto scene = Scene::gen(config);
    outer = Rectangle::gen({}, {4.0f, 4.0f});
    inner = Rectangle::gen({}, {2.0f, 2.0f});
    ring = nullptr;
    if (outer && inner) CHECK(_combined(outer, inner, PathOperation::Difference, ring) == Result::Success);
    delete outer;
    delete inner;
    CHECK(scene && ring);
    if (scene && ring) {
        ring->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        CHECK(scene->add(ring) == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.0f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {1.5f, 0.0f, 0.0f}), 1.0f, 0.02f));
        CHECK(near(static_cast<float>(_inked(renderer, scene, 0.0f)), 10800.0f, 120.0f));
    } else {
        delete ring;
    }
    delete scene;

    scene = Scene::gen(config);
    left = Circle::gen({-0.5f, 0.0f, 0.0f}, 1.0f);
    right = Circle::gen({0.5f, 0.0f, 0.0f}, 1.0f);
    lens = nullptr;
    if (left && right) CHECK(_combined(left, right, PathOperation::Intersection, lens) == Result::Success);
    auto target = Circle::gen({}, 1.5f);
    CHECK(scene && lens && target);
    if (scene && lens && target) {
        lens->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        target->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        CHECK(scene->add(lens) == Result::Success && scene->add(target) == Result::Success);
        CHECK(scene->play(Animation::morph(lens, target), 1.0f) == Result::Success);
        auto start = _inked(renderer, scene, 0.0f);
        auto end = _inked(renderer, scene, 1.0f);
        CHECK(near(static_cast<float>(start), 1105.0f, 60.0f) && end > start * 5u);
    } else {
        delete lens;
        delete target;
    }
    delete left;
    delete right;
    delete scene;
}
