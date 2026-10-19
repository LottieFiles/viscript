#include "testSceneChecks.h"

// The bowl z = (x^2 + y^2) / 4 over [-2, 2]^2 on an n by n grid, row by row from the bottom left.
static void bowlPoints(Vec3* points, uint32_t n)
{
    for (auto j = 0u; j < n; j++) {
        for (auto i = 0u; i < n; i++) {
            auto x = -2.0f + 4.0f * static_cast<float>(i) / static_cast<float>(n - 1u);
            auto y = -2.0f + 4.0f * static_cast<float>(j) / static_cast<float>(n - 1u);
            points[j * n + i] = {x, y, (x * x + y * y) / 4.0f};
        }
    }
}

// A colormap colors each face of a surface at the value of its middle, so the bottom of a bowl
// takes the first color of viridis and the face at a corner the color near its last; colors
// given per point average over each face, and both follow the opacity. A map that cannot run
// and a count that is not one color per point fail.
void surfaceColors(Renderer* renderer)
{
    static constexpr auto N = 41u;
    static Vec3 points[N * N];
    bowlPoints(points, N);
    auto config = topView();
    // Renders the bowl as configure leaves it, alone in a scene, into frame.
    auto render = [&](auto configure, Surface& frame) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(points, N, N);
        CHECK(scene && surface);
        if (scene && surface) {
            surface->shading = false;
            configure(surface);
            CHECK(scene->add(surface) == Result::Success);
            CHECK(renderer->render(scene, 0.0f, frame) == Result::Success);
        } else {
            delete surface;
        }
        delete scene;
        return frame.data() != nullptr;
    };

    Surface frame;
    auto viridis = [](SurfaceMesh* surface) {
        surface->colormap = {Colormap::Viridis, {0.0f, 0.0f, 1.0f}, 0.0f, 2.0f};
    };
    if (render(viridis, frame)) {
        // The face beside the bottom, z from 0 to 0.005: share 0.00125 of the map.
        CHECK(close(framePixel(frame, 102u, 98u), {68, 1, 84, 255}, 1));
        // The faces at the four corners, z from 1.805 to 2: share 0.95125, 0.61 of the way from
        // the eighth stop (171, 220, 50) to the last (253, 231, 37).
        static constexpr uint32_t corners[4][2] = {{178, 22}, {22, 22}, {22, 178}, {178, 178}};
        for (auto& corner : corners) {
            CHECK(close(framePixel(frame, corner[0], corner[1]), {221, 227, 42, 255}, 1));
        }
        // Halfway up the side, x = 1.45 and y = 0.05: z about 0.53, a teal near the third stop.
        auto side = framePixel(frame, 158u, 98u);
        CHECK(side.g > side.r && side.b > side.r && side.g > 80u);
    }

    // The range runs the other way when min is above max.
    auto reversed = [](SurfaceMesh* surface) {
        surface->colormap = {Colormap::Viridis, {0.0f, 0.0f, 1.0f}, 2.0f, 0.0f};
    };
    if (render(reversed, frame)) {
        CHECK(close(framePixel(frame, 102u, 98u), {253, 231, 37, 255}, 1));
        CHECK(close(framePixel(frame, 178u, 22u), {69, 18, 99, 255}, 1));
    }

    // A color for every point: each face takes the mean of its corners, and opacity scales it.
    static Color colors[N * N];
    for (auto j = 0u; j < N; j++) {
        for (auto i = 0u; i < N; i++) {
            colors[j * N + i] = {static_cast<uint8_t>(i * 6u), static_cast<uint8_t>(j * 6u), 128, 255};
        }
    }
    auto pointColors = [&](SurfaceMesh* surface) {
        CHECK(surface->colors(colors, N * N - 1u) == Result::InvalidArguments);
        CHECK(surface->colors() == nullptr);
        CHECK(surface->colors(colors, N * N) == Result::Success);
        CHECK(surface->colors() != nullptr && surface->colors() != colors);
    };
    if (render(pointColors, frame)) {
        CHECK(close(framePixel(frame, 102u, 98u), {123, 123, 128, 255}, 0));
        CHECK(close(framePixel(frame, 22u, 178u), {3, 3, 128, 255}, 0));
    }
    auto faded = [&](SurfaceMesh* surface) {
        pointColors(surface);
        surface->opacity = 0.5f;
    };
    if (render(faded, frame)) CHECK(close(framePixel(frame, 102u, 98u), {70, 70, 72, 255}, 2));
    // A colormap wins over the colors of the points, and nullptr takes the colors away.
    auto both = [&](SurfaceMesh* surface) {
        pointColors(surface);
        viridis(surface);
    };
    if (render(both, frame)) CHECK(close(framePixel(frame, 102u, 98u), {68, 1, 84, 255}, 1));
    auto cleared = [&](SurfaceMesh* surface) {
        pointColors(surface);
        CHECK(surface->colors(nullptr, 0u) == Result::Success);
        CHECK(surface->colors() == nullptr);
        surface->fill(Color::hex("#ff8000"));
    };
    if (render(cleared, frame)) CHECK(close(framePixel(frame, 102u, 98u), {255, 128, 0, 255}, 0));

    // A map past the last, a zero axis, a range of no length and a range past float fail to add.
    auto invalid = [&](SurfaceColormap colormap) {
        auto other = Scene::gen(config);
        auto mesh = SurfaceMesh::gen(points, N, N);
        CHECK(other && mesh);
        if (other && mesh) {
            mesh->colormap = colormap;
            CHECK(other->add(mesh) == Result::InvalidArguments);
        }
        delete mesh;
        delete other;
    };
    invalid({static_cast<Colormap>(6), {0.0f, 0.0f, 1.0f}, 0.0f, 1.0f});
    invalid({Colormap::Magma, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f});
    invalid({Colormap::Magma, {0.0f, 0.0f, 1.0f}, 1.0f, 1.0f});
    auto largest = std::numeric_limits<float>::max();
    invalid({Colormap::Magma, {0.0f, 0.0f, 1.0f}, -largest, largest});
    invalid({Colormap::Magma, {0.0f, 0.0f, 1.0f}, 0.0f, std::numeric_limits<float>::quiet_NaN()});
}

// Mesh lines at a resolution of their own: a 41 by 41 plane over [-2, 2]^2 seen from above,
// 40 pixels to a unit, draws lines only where the counts put them, between the samples where
// they fall there; a Mesh surface draws nothing but them; a nearer surface hides the pieces
// below it while the rest stay; and counts of 1 or past the points fail.
void surfaceMeshLines(Renderer* renderer)
{
    static constexpr auto N = 41u;
    static Vec3 plane[N * N];
    for (auto j = 0u; j < N; j++) {
        for (auto i = 0u; i < N; i++) {
            plane[j * N + i] = {-2.0f + 0.1f * static_cast<float>(i), -2.0f + 0.1f * static_cast<float>(j), 0.0f};
        }
    }
    auto config = topView();
    auto fill = Color::hex("#2050c0");
    auto stroke = Color::hex("#ffffff");
    auto background = config.background;
    // Renders the plane as configure leaves it, with a cover added if given, into frame.
    auto render = [&](auto configure, Object* cover, Surface& frame) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, N, N);
        CHECK(scene && surface);
        auto drawn = false;
        if (scene && surface) {
            surface->shading = false;
            surface->mode = SurfaceMode::SolidMesh;
            surface->fill(fill);
            surface->stroke(stroke, 2.0f);
            configure(surface);
            CHECK(scene->add(surface) == Result::Success);
            if (cover) CHECK(scene->add(cover) == Result::Success);
            drawn = renderer->render(scene, 0.0f, frame) == Result::Success && frame.data();
            CHECK(drawn);
            cover = nullptr;
        } else {
            delete surface;
        }
        delete cover;
        delete scene;
        return drawn;
    };

    Surface frame;
    // Five lines each way fall on the samples at -2, -1, 0, 1 and 2: x = 0 is pixel column 100,
    // while x = 0.5, where every face drew its outline before, keeps the fill.
    auto five = [](SurfaceMesh* surface) {
        surface->meshColumns = 5u;
        surface->meshRows = 5u;
    };
    if (render(five, nullptr, frame)) {
        CHECK(close(framePixel(frame, 100u, 130u), stroke, 8));
        CHECK(close(framePixel(frame, 130u, 100u), stroke, 8));
        CHECK(close(framePixel(frame, 120u, 130u), fill, 0));
        CHECK(close(framePixel(frame, 122u, 122u), fill, 0));
    }
    auto outlines = [](SurfaceMesh*) {};
    if (render(outlines, nullptr, frame)) CHECK(close(framePixel(frame, 120u, 130u), stroke, 8));
    // Four lines across 40 faces fall at 13.33 faces apart, between the samples: x = -0.667 is
    // pixel 73.3 and x = 0.667 pixel 126.7, and only one column, the columns, takes lines.
    auto four = [](SurfaceMesh* surface) { surface->meshColumns = 4u; };
    if (render(four, nullptr, frame)) {
        CHECK(framePixel(frame, 126u, 150u).r > 160u && framePixel(frame, 126u, 150u).g > 160u);
        CHECK(framePixel(frame, 73u, 150u).r > 160u);
        CHECK(close(framePixel(frame, 100u, 150u), fill, 0));
        CHECK(close(framePixel(frame, 150u, 100u), fill, 0));
    }
    // A Mesh surface draws only its lines.
    auto wire = [](SurfaceMesh* surface) {
        surface->mode = SurfaceMode::Mesh;
        surface->meshColumns = 5u;
        surface->meshRows = 5u;
    };
    if (render(wire, nullptr, frame)) {
        CHECK(close(framePixel(frame, 100u, 130u), stroke, 8));
        CHECK(close(framePixel(frame, 120u, 130u), background, 0));
    }
    // The elevated plane hides only the covered part of the line.
    Vec3 above[] = {{0.5f, -1.0f, 1.0f}, {1.5f, -1.0f, 1.0f}, {0.5f, 1.0f, 1.0f}, {1.5f, 1.0f, 1.0f}};
    auto cover = SurfaceMesh::gen(above, 2u, 2u);
    CHECK(cover != nullptr);
    if (cover) {
        cover->shading = false;
        cover->fill(Color::hex("#c03020"));
    }
    if (cover && render(five, cover, frame)) {
        CHECK(close(framePixel(frame, 140u, 100u), Color::hex("#c03020"), 0));
        CHECK(close(framePixel(frame, 140u, 160u), stroke, 8));
        CHECK(close(framePixel(frame, 140u, 40u), stroke, 8));
    }

    auto invalid = [&](uint32_t columns, uint32_t rows) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, N, N);
        CHECK(scene && surface);
        if (scene && surface) {
            surface->mode = SurfaceMode::Mesh;
            surface->meshColumns = columns;
            surface->meshRows = rows;
            CHECK(scene->add(surface) == Result::InvalidArguments);
        }
        delete surface;
        delete scene;
    };
    invalid(1u, 0u);
    invalid(0u, 1u);
    invalid(N + 1u, 2u);
    invalid(2u, N + 1u);
}

// A surface morphs into another on its grid point by point. Its colormap is read again at the
// points between the two, so halfway from the bowl to the saddle (x^2 - y^2) / 4, where the
// surface is x^2 / 4, the face over (0.1, 1.5) takes the middle of viridis rather than a blend
// of the bowl's color there with the saddle's. Colors sampled between the two blend between
// steps and mix into the target's by the progress. Surfaces on different grids, a surface and a
// circle, and sampled colors that do not fit fail to play.
void surfaceMorph(Renderer* renderer)
{
    static constexpr auto N = 21u;
    static Vec3 bowl[N * N];
    static Vec3 saddle[N * N];
    bowlPoints(bowl, N);
    for (auto i = 0u; i < N * N; i++) {
        auto& p = bowl[i];
        saddle[i] = {p.x, p.y, (p.x * p.x - p.y * p.y) / 4.0f};
    }
    auto config = topView();
    auto colormap = [](SurfaceMesh* surface) {
        surface->shading = false;
        surface->colormap = {Colormap::Viridis, {0.0f, 0.0f, 1.0f}, -1.0f, 1.0f};
    };
    {
        auto scene = Scene::gen(config);
        auto source = SurfaceMesh::gen(bowl, N, N);
        auto target = SurfaceMesh::gen(saddle, N, N);
        CHECK(scene && source && target);
        if (scene && source && target) {
            colormap(source);
            colormap(target);
            CHECK(scene->add(source) == Result::Success);
            CHECK(scene->add(target) == Result::Success);
            CHECK(scene->play(Animation::morph(source, target), 1.0f, Easing::Linear) == Result::Success);
            Surface frame;
            Color colors[3];
            auto drawn = 0u;
            float times[3] = {0.0f, 0.5f, 1.0f};
            for (auto i = 0u; i < 3u; i++) {
                if (renderer->render(scene, times[i], frame) == Result::Success && frame.data()) {
                    colors[i] = framePixel(frame, 104u, 40u);
                    drawn++;
                }
            }
            CHECK(drawn == 3u);
            if (drawn == 3u) {
                // The bowl's face at a mean height of 0.57, the share 0.785.
                CHECK(close(colors[0], {115, 206, 85, 255}, 2));
                // A mean height of 0.005, against the blend (87, 139, 111) of the two ends.
                CHECK(close(colors[1], {33, 144, 140, 255}, 2));
                CHECK(!close(colors[1], {87, 139, 111, 255}, 20));
                // The saddle's face at a mean height of -0.56.
                CHECK(close(colors[2], {62, 73, 135, 255}, 2));
            }
        } else {
            delete source;
            delete target;
        }
        delete scene;
    }
    // Colors sampled at three steps, red, green and blue, mix into the target's white.
    Vec3 low[] = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    Vec3 high[] = {{-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
    Color steps[12];
    for (auto i = 0u; i < 4u; i++) {
        steps[i] = Color::hex("#ff0000");
        steps[4u + i] = Color::hex("#00ff00");
        steps[8u + i] = Color::hex("#0000ff");
    }
    Color white[4] = {Color::hex("#ffffff"), Color::hex("#ffffff"), Color::hex("#ffffff"), Color::hex("#ffffff")};
    // Plays a morph of the two squares with sampled colors set up by configure, returning its
    // result and, when it plays, the middle of the frame at times 0.25 and 0.5.
    auto sampled = [&](auto configure, Color* middle) {
        auto scene = Scene::gen(config);
        auto source = SurfaceMesh::gen(low, 2u, 2u);
        auto target = SurfaceMesh::gen(high, 2u, 2u);
        auto result = Result::Unknown;
        if (scene && source && target) {
            source->shading = false;
            target->shading = false;
            target->colors(white, 4u);
            scene->add(source);
            scene->add(target);
            auto animation = Animation::morph(source, target);
            animation.sourceColors = steps;
            animation.colorSteps = 2u;
            animation.colorCount = 12u;
            configure(animation);
            result = scene->play(animation, 1.0f, Easing::Linear);
            Surface frame;
            if (result == Result::Success && middle) {
                if (renderer->render(scene, 0.25f, frame) == Result::Success && frame.data()) {
                    middle[0] = framePixel(frame, 100u, 100u);
                }
                if (renderer->render(scene, 0.5f, frame) == Result::Success && frame.data()) {
                    middle[1] = framePixel(frame, 100u, 100u);
                }
            }
        } else {
            delete source;
            delete target;
        }
        delete scene;
        return result;
    };
    Color middle[2];
    CHECK(sampled([](Animation&) {}, middle) == Result::Success);
    // A quarter: halfway from red to green, a quarter of the way to white.
    CHECK(close(middle[0], {159, 159, 64, 255}, 1));
    // A half: green, halfway to white.
    CHECK(close(middle[1], {128, 255, 128, 255}, 1));
    CHECK(sampled([](Animation& a) { a.colorSteps = 0u; }, nullptr) == Result::InvalidArguments);
    CHECK(sampled([](Animation& a) { a.colorSteps = 65u; a.colorCount = 66u * 4u; }, nullptr)
          == Result::InvalidArguments);
    CHECK(sampled([](Animation& a) { a.colorCount = 8u; }, nullptr) == Result::InvalidArguments);
    CHECK(sampled([](Animation& a) { a.sourceColors = nullptr; }, nullptr) == Result::InvalidArguments);
    CHECK(sampled([&](Animation& a) { a.targetColors = steps; }, nullptr) == Result::Success);
    // A surface cannot morph into another grid or into a circle, and only surfaces take colors.
    auto refused = [&](Object* source, Object* target, bool colored) {
        auto scene = Scene::gen(config);
        auto result = Result::Unknown;
        if (scene && source && target) {
            scene->add(source);
            scene->add(target);
            auto animation = Animation::morph(source, target);
            if (colored) {
                animation.sourceColors = steps;
                animation.colorSteps = 2u;
                animation.colorCount = 12u;
            }
            result = scene->play(animation, 1.0f, Easing::Linear);
        } else {
            delete source;
            delete target;
        }
        delete scene;
        return result;
    };
    Vec3 wide[] = {{-1.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
                   {-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    CHECK(refused(SurfaceMesh::gen(low, 2u, 2u), SurfaceMesh::gen(wide, 3u, 2u), false) == Result::NonSupport);
    CHECK(refused(SurfaceMesh::gen(low, 2u, 2u), Circle::gen({}, 1.0f), false) == Result::NonSupport);
    CHECK(refused(Circle::gen({}, 1.0f), SurfaceMesh::gen(low, 2u, 2u), false) == Result::NonSupport);
    CHECK(refused(Circle::gen({}, 1.0f), Circle::gen({}, 2.0f), true) == Result::InvalidArguments);
}

// A light fixed in the world shades a tilted plane the same before and after the camera orbits
// a quarter turn around it, where the headlight shades it by the view; the side the camera
// sees takes the light, a light behind it leaves the ambient share, no shading leaves the fill,
// and a light that cannot shine fails to add.
void surfaceLight(Renderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    // z = x / 2 over [-1, 1]^2: its normal is (-1, 0, 2) / sqrt(5).
    Vec3 plane[] = {{-1.0f, -1.0f, -0.5f}, {1.0f, -1.0f, 0.5f}, {-1.0f, 1.0f, -0.5f}, {1.0f, 1.0f, 0.5f}};
    Config config;
    config.width = 120;
    config.height = 120;
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {0.0f, -6.0f, 6.0f};
    config.camera.target = {};
    config.camera.up = {0.0f, 0.0f, 1.0f};
    config.background = Color::hex("#101010");
    // The middle of the plane at the start of a linear quarter orbit about +z and at its end,
    // where the eye stands at (6, 0, 6).
    auto sample = [&](auto configure, Color& start, Color& end) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, 2u, 2u);
        CHECK(scene && surface);
        auto drawn = false;
        if (scene && surface) {
            surface->fill(Color::hex("#c86432"));
            configure(surface);
            CHECK(scene->add(surface) == Result::Success);
            CHECK(scene->orbit(PI / 2.0f, {0.0f, 0.0f, 1.0f}, 1.0f, Easing::Linear) == Result::Success);
            Surface frame;
            if (renderer->render(scene, 0.0f, frame) == Result::Success && frame.data()) {
                start = framePixel(frame, 60u, 60u);
                if (renderer->render(scene, 1.0f, frame) == Result::Success && frame.data()) {
                    end = framePixel(frame, 60u, 60u);
                    drawn = true;
                }
            }
            CHECK(drawn);
        } else {
            delete surface;
        }
        delete scene;
        return drawn;
    };
    Color start;
    Color end;
    // Lit from above: 0.5 + 0.5 * 2 / sqrt(5) = 0.947 of (200, 100, 50) from either eye.
    auto above = [](SurfaceMesh* surface) { surface->light = {{0.0f, 0.0f, 3.0f}, 0.5f}; };
    if (sample(above, start, end)) {
        CHECK(close(start, {189, 95, 47, 255}, 1));
        CHECK(close(end, start, 0));
    }
    // The headlight follows the eye: 0.58 + 0.42 cos of 0.632 at the start and of 0.316 at the
    // end.
    auto headlight = [](SurfaceMesh*) {};
    if (sample(headlight, start, end)) {
        CHECK(close(start, {169, 85, 42, 255}, 1));
        CHECK(close(end, {143, 71, 36, 255}, 1));
    }
    // From below, the light does not reach the side the camera sees, which keeps the ambient
    // share; the default ambient is a half.
    auto below = [](SurfaceMesh* surface) { surface->light.direction = {0.0f, 0.0f, -1.0f}; };
    if (sample(below, start, end)) {
        CHECK(close(start, {100, 50, 25, 255}, 1));
        CHECK(close(end, start, 0));
    }
    auto flat = [](SurfaceMesh* surface) {
        surface->light.direction = {0.0f, 0.0f, 1.0f};
        surface->shading = false;
    };
    if (sample(flat, start, end)) CHECK(close(start, {200, 100, 50, 255}, 0) && close(end, start, 0));

    auto invalid = [&](SurfaceLight light) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, 2u, 2u);
        CHECK(scene && surface);
        if (scene && surface) {
            surface->light = light;
            CHECK(scene->add(surface) == Result::InvalidArguments);
        }
        delete surface;
        delete scene;
    };
    invalid({{0.0f, 0.0f, 1.0f}, -0.1f});
    invalid({{0.0f, 0.0f, 1.0f}, 1.5f});
    invalid({{0.0f, 0.0f, 1.0f}, std::numeric_limits<float>::quiet_NaN()});
    invalid({{0.0f, std::numeric_limits<float>::infinity(), 1.0f}, 0.5f});
}

// Adds count objects to a scene of config in order, orbits its camera by angle about +z over a
// second, and renders time into frame; the scene owns what it takes and the rest is deleted.
static bool depthRender(Renderer* renderer, const Config& config, Object* const* objects, uint32_t count,
                        float angle, float time, Surface& frame)
{
    auto scene = Scene::gen(config);
    auto complete = scene != nullptr;
    for (auto i = 0u; i < count; i++) complete = complete && objects[i];
    auto drawn = false;
    if (complete) {
        for (auto i = 0u; i < count; i++) CHECK(scene->add(objects[i]) == Result::Success);
        if (angle != 0.0f) CHECK(scene->orbit(angle, {0.0f, 0.0f, 1.0f}, 1.0f, Easing::Linear) == Result::Success);
        drawn = renderer->render(scene, time, frame) == Result::Success && frame.data();
    } else {
        for (auto i = 0u; i < count; i++) delete objects[i];
    }
    CHECK(drawn);
    delete scene;
    return drawn;
}

// The raw pixel of a frame at x and y.
static uint32_t depthPixel(const Surface& frame, uint32_t x, uint32_t y)
{
    return frame.data()[static_cast<size_t>(y) * frame.stride() + x];
}

// Split strokes only at same-layer depth crossings, preserving dashes, caps and drawing progress.
void depthSplitStrokes(Renderer* renderer)
{
    auto config = topView();
    auto plate = Color::hex("#2050c0");
    auto white = Color::hex("#ffffff");
    auto background = config.background;
    // The line z = x from x = -2 to 2 along y = 0, pixels 20 to 180 of row 100.
    auto line = [&](bool reversed, Color color, float width) {
        Vec3 from = {-2.0f, 0.0f, -2.0f};
        Vec3 to = {2.0f, 0.0f, 2.0f};
        auto made = reversed ? Line::gen(to, from) : Line::gen(from, to);
        if (made) made->stroke(color, width);
        return made;
    };
    Surface frame;
    Surface reference;
    // Below the plane from x = -1 to a little short of 0, where the line comes within the plane's
    // tolerance, and in front of it from there to 1.
    auto through = [&](const Surface& image) {
        CHECK(close(framePixel(image, 80u, 100u), plate, 8));
        CHECK(close(framePixel(image, 86u, 100u), plate, 8));
        CHECK(close(framePixel(image, 120u, 100u), white, 8));
        CHECK(close(framePixel(image, 40u, 100u), white, 8));
        CHECK(close(framePixel(image, 160u, 100u), white, 8));
        CHECK(close(framePixel(image, 80u, 70u), plate, 0));
    };
    {
        Object* objects[] = {depthPlate(0.0f, plate), line(false, white, 4.0f)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) through(frame);
    }
    {
        Object* objects[] = {line(false, white, 4.0f), depthPlate(0.0f, plate)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) through(frame);
    }
    {
        Object* objects[] = {depthPlate(0.0f, plate), line(true, white, 4.0f)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) through(frame);
    }
    // A layer of its own keeps the line whole: over the plane from above, under it from below.
    {
        auto over = line(false, white, 4.0f);
        if (over) over->layer = 1;
        Object* objects[] = {depthPlate(0.0f, plate), over};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 80u, 100u), white, 8));
        }
        auto under = line(false, white, 4.0f);
        if (under) under->layer = -1;
        Object* others[] = {depthPlate(0.0f, plate), under};
        if (depthRender(renderer, config, others, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 120u, 100u), plate, 0));
            CHECK(close(framePixel(frame, 40u, 100u), white, 8));
        }
    }
    // A dashed line draws the same dashes over the plane and beside it as with no plane, where
    // pieces that started the pattern again would shift them.
    {
        float pattern[] = {10.0f, 6.0f};
        auto dashed = line(false, white, 4.0f);
        auto alone = line(false, white, 4.0f);
        if (dashed) CHECK(dashed->dash(pattern, 2u, 3.0f) == Result::Success);
        if (alone) CHECK(alone->dash(pattern, 2u, 3.0f) == Result::Success);
        Object* objects[] = {depthPlate(0.0f, plate), dashed};
        Object* others[] = {alone};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            auto shown = 0u;
            for (auto x = 18u; x < 183u; x++) {
                for (auto y = 97u; y <= 103u; y++) {
                    if (x < 58u || x > 142u) {
                        same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                    } else if (x > 104u && x < 138u) {
                        auto drawn = framePixel(reference, x, y);
                        if (close(drawn, white, 0)) {
                            same = same && close(framePixel(frame, x, y), white, 0);
                            shown++;
                        } else if (close(drawn, background, 0)) {
                            same = same && close(framePixel(frame, x, y), plate, 0);
                        }
                    } else if (x > 62u && x < 90u) {
                        same = same && close(framePixel(frame, x, y), plate, 0);
                    }
                }
            }
            CHECK(same);
            CHECK(shown > 40u);
        }
    }
    // Seven eighths drawn, the line ends at x = 1.5 with or without the plane.
    {
        auto partial = line(false, white, 4.0f);
        auto alone = line(false, white, 4.0f);
        if (partial) partial->progress = 0.875f;
        if (alone) alone->progress = 0.875f;
        Object* objects[] = {depthPlate(0.0f, plate), partial};
        Object* others[] = {alone};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            for (auto x = 142u; x < 200u; x++) {
                for (auto y = 96u; y <= 104u; y++) {
                    same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                }
            }
            CHECK(same);
            CHECK(close(framePixel(frame, 159u, 100u), white, 8));
            CHECK(close(framePixel(frame, 165u, 100u), background, 0));
            CHECK(close(framePixel(frame, 80u, 100u), plate, 8));
        }
    }
    // A translucent line keeps its round ends, and over the plane it is even from where it comes
    // out from under it. Its pieces meet without overlapping, where the line would draw twice and
    // lighten: seen through a translucent plane, no pixel of it over the plane is lighter than
    // where it lies in front.
    {
        auto pale = Color::hex("#ffffff80");
        Object* objects[] = {depthPlate(0.0f, plate), line(false, pale, 6.0f)};
        Object* others[] = {line(false, pale, 6.0f)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            for (auto y = 95u; y <= 105u; y++) {
                for (auto x = 14u; x < 50u; x++) {
                    same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                }
                for (auto x = 150u; x < 187u; x++) {
                    same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                }
            }
            CHECK(same);
            auto out = 60u;
            while (out < 140u && close(framePixel(frame, out, 100u), plate, 0)) out++;
            CHECK(out > 80u && out < 100u);
            auto even = true;
            for (auto x = out + 2u; x < 138u; x++) {
                even = even && close(framePixel(frame, x, 100u), framePixel(frame, 125u, 100u), 2);
            }
            CHECK(even);
            CHECK(!close(framePixel(frame, 125u, 100u), plate, 8));
        }
        Object* veiled[] = {depthPlate(0.0f, Color::hex("#2050c080")), line(false, pale, 6.0f)};
        if (depthRender(renderer, config, veiled, 2u, 0.0f, 0.0f, frame)) {
            auto front = static_cast<int>(framePixel(frame, 125u, 100u).r);
            auto lightest = 0;
            for (auto x = 62u; x < 138u; x++) {
                auto red = static_cast<int>(framePixel(frame, x, 100u).r);
                if (red > lightest) lightest = red;
            }
            CHECK(lightest <= front + 2);
            CHECK(static_cast<int>(framePixel(frame, 75u, 100u).r) + 20 < front);
        }
    }
    // An arrow's head draws whole with its tip: over the plane where the tip is above it, and
    // under it where the tip is below.
    {
        auto up = Arrow::gen({-0.8f, 0.0f, -1.0f}, {0.8f, 0.0f, 1.0f});
        if (up) up->stroke(white, 4.0f);
        Object* objects[] = {depthPlate(0.0f, plate), up};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 128u, 100u), white, 8));
            CHECK(close(framePixel(frame, 76u, 100u), plate, 0));
        }
        auto down = Arrow::gen({0.8f, 0.0f, 1.0f}, {-0.8f, 0.0f, -1.0f});
        if (down) down->stroke(white, 4.0f);
        Object* others[] = {depthPlate(0.0f, plate), down};
        if (depthRender(renderer, config, others, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 72u, 100u), plate, 0));
            CHECK(close(framePixel(frame, 76u, 98u), plate, 0));
            CHECK(close(framePixel(frame, 124u, 100u), white, 8));
        }
    }
    // A point on the plane stays on top although added first; one below it hides and one above
    // shows.
    {
        auto red = Color::hex("#e02020");
        auto point = [&](const Vec3& at) {
            auto made = Point::gen(at);
            if (made) {
                made->fill(red);
                made->stroke(red, 1.0f);
            }
            return made;
        };
        Object* objects[] = {point({0.5f, 0.5f, 0.0f}), point({-0.5f, 0.5f, -0.5f}), point({-0.5f, -0.5f, 0.3f}),
                             depthPlate(0.0f, plate)};
        if (depthRender(renderer, config, objects, 4u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 120u, 80u), red, 4));
            CHECK(close(framePixel(frame, 80u, 80u), plate, 0));
            CHECK(close(framePixel(frame, 80u, 120u), red, 4));
        }
    }
    // A stroke away from the plane draws as with no plane while another is cut beside it.
    {
        auto far = [&]() {
            auto made = Line::gen({-2.0f, 1.8f, 0.3f}, {2.0f, 1.8f, -0.3f});
            if (made) {
                float pattern[] = {7.0f, 5.0f};
                made->stroke(Color::hex("#ffcc0080"), 3.0f);
                CHECK(made->dash(pattern, 2u) == Result::Success);
            }
            return made;
        };
        Object* objects[] = {depthPlate(0.0f, plate), line(false, white, 4.0f), far()};
        Object* others[] = {far()};
        if (depthRender(renderer, config, objects, 3u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            for (auto y = 20u; y <= 36u; y++) {
                for (auto x = 0u; x < 200u; x++) same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
            }
            CHECK(same);
        }
    }
    // A 2D scene keeps the painter's order: the line added after the plane draws over all of it.
    {
        auto flat = config;
        flat.cameraView = CameraView::TwoD;
        Object* objects[] = {depthPlate(0.0f, plate), line(false, white, 4.0f)};
        if (depthRender(renderer, flat, objects, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 80u, 100u), white, 8));
            CHECK(close(framePixel(frame, 80u, 70u), plate, 0));
        }
    }
}

// A space in the layer of a surface lowers only the parts of its axes the surface hides, so
// they draw behind the surface there and over the space's own layer elsewhere; while the camera
// orbits a dome, the axis hides inside and behind the dome in every frame and shows in front of
// it; and a stroke that crosses more folds than it can be cut into still draws.
void depthSplitSpace(Renderer* renderer)
{
    auto plate = Color::hex("#2050c0");
    // Colors apart from the theme's axis colors, which a theme would replace.
    auto red = Color::hex("#f02828");
    auto green = Color::hex("#28d028");
    auto clear = Color{0, 0, 0, 0};
    auto space = [&](float z) {
        auto made = Space::gen({-2.0f, 2.0f, 1.0f}, {-2.0f, 2.0f, 1.0f}, {-z, z, 1.0f});
        if (made) {
            made->layer = 0;
            // Axes 4 pixels wide cover the pixels the checks read whole.
            made->stroke(clear, 4.0f);
            made->axisX = red;
            made->axisY = green;
        }
        return made;
    };
    Surface frame;
    // Seen from above, the plane z = 0.5 hides the axes below it; past it they show, and over
    // a line of the space's layer nearer the camera, which they cover there as their own layer.
    {
        auto config = topView();
        auto cross = Line::gen({1.5f, -0.5f, 1.0f}, {1.5f, 0.5f, 1.0f});
        if (cross) cross->stroke(Color::hex("#ffffff"), 4.0f);
        Object* objects[] = {space(1.0f), depthPlate(0.5f, plate), cross};
        if (depthRender(renderer, config, objects, 3u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 80u, 100u), plate, 0));
            CHECK(close(framePixel(frame, 100u, 80u), plate, 0));
            CHECK(close(framePixel(frame, 40u, 100u), red, 8));
            CHECK(close(framePixel(frame, 100u, 40u), green, 8));
            CHECK(close(framePixel(frame, 160u, 100u), red, 8));
            CHECK(close(framePixel(frame, 160u, 85u), Color::hex("#ffffff"), 8));
        }
    }
    // The dome z = 1 - (x^2 + y^2) / 2 over [-1, 1]^2 from (0, -6, 4), orbiting to (6, 0, 4).
    {
        static constexpr auto N = 21u;
        static Vec3 dome[N * N];
        for (auto j = 0u; j < N; j++) {
            for (auto i = 0u; i < N; i++) {
                auto x = -1.0f + 0.1f * static_cast<float>(i);
                auto y = -1.0f + 0.1f * static_cast<float>(j);
                dome[j * N + i] = {x, y, 1.0f - (x * x + y * y) / 2.0f};
            }
        }
        Config config;
        config.width = 200;
        config.height = 200;
        config.cameraView = CameraView::ThreeD;
        config.camera.projection = Projection::Perspective;
        config.camera.eye = {0.0f, -6.0f, 4.0f};
        config.camera.target = {};
        config.camera.up = {0.0f, 0.0f, 1.0f};
        config.background = Color::hex("#101010");
        constexpr auto PI = 3.14159265358979f;
        // The pixel of world at time, where the eye has turned a quarter of time about +z.
        auto pixel = [&](float time, const Vec3& world, uint32_t& x, uint32_t& y) {
            auto camera = config.camera;
            auto angle = PI / 2.0f * time;
            camera.eye = {6.0f * std::sin(angle), -6.0f * std::cos(angle), 4.0f};
            Vec3 ndc;
            if (!camera.project(world, 1.0f, ndc)) return false;
            x = static_cast<uint32_t>((ndc.x + 1.0f) * 100.0f);
            y = static_cast<uint32_t>((1.0f - ndc.y) * 100.0f);
            return x < 200u && y < 200u;
        };
        float times[] = {0.0f, 0.5f, 1.0f};
        for (auto time : times) {
            auto surface = SurfaceMesh::gen(dome, N, N);
            if (surface) {
                surface->shading = false;
                surface->fill(plate);
            }
            Object* objects[] = {space(0.5f), surface};
            if (!depthRender(renderer, config, objects, 2u, PI / 2.0f, time, frame)) continue;
            uint32_t x;
            uint32_t y;
            // The middle of the axis lies inside the dome. The dome's color is read within 6, as
            // a pixel on a seam between its faces is a little darker.
            CHECK(pixel(time, {0.0f, 0.0f, 0.0f}, x, y) && close(framePixel(frame, x, y), plate, 6));
            // The end at x = 1.7 lies beside the dome, then in front of it.
            CHECK(pixel(time, {1.7f, 0.0f, 0.0f}, x, y) && close(framePixel(frame, x, y), red, 12));
            // The end at x = -1.7 lies beside the dome, then behind it.
            CHECK(pixel(time, {-1.7f, 0.0f, 0.0f}, x, y));
            if (time < 0.25f) CHECK(close(framePixel(frame, x, y), red, 12));
            if (time > 0.75f) CHECK(close(framePixel(frame, x, y), plate, 6));
        }
    }
    // Folds 6 pixels apart under a 1200 pixel line cut it into more pieces than a stroke takes;
    // the last piece takes the rest, and the frame still draws.
    {
        static constexpr auto COLUMNS = 2401u;
        static Vec3 folds[COLUMNS * 2u];
        for (auto j = 0u; j < 2u; j++) {
            for (auto i = 0u; i < COLUMNS; i++) {
                auto x = -6.0f + 0.005f * static_cast<float>(i);
                folds[j * COLUMNS + i] = {x, -0.2f + 0.4f * static_cast<float>(j), 0.2f * std::sin(x * 104.72f)};
            }
        }
        Config config;
        config.width = 1200;
        config.height = 100;
        config.cameraView = CameraView::ThreeD;
        config.camera.eye = {0.0f, 0.0f, 10.0f};
        config.camera.target = {};
        config.camera.up = {0.0f, 1.0f, 0.0f};
        config.camera.projection = Projection::Orthographic;
        config.camera.orthoHeight = 1.0f;
        config.background = Color::hex("#101010");
        auto surface = SurfaceMesh::gen(folds, COLUMNS, 2u);
        if (surface) {
            surface->shading = false;
            surface->fill(plate);
        }
        auto across = Line::gen({-6.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f});
        if (across) across->stroke(Color::hex("#ffffff"), 2.0f);
        Object* objects[] = {surface, across};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
            auto lit = 0u;
            for (auto x = 0u; x < 1200u; x++) lit += close(framePixel(frame, x, 50u), Color::hex("#ffffff"), 40);
            CHECK(lit > 100u && lit < 1100u);
        }
    }
}

// Each piece of a plot hides behind a surface where it lies behind it, as a stroke of one
// contour does, where a stroke of several contours would draw whole. Seen from above, the piece
// z = x along y = 0.5 hides under the plane z = 0 left of x = 0 and the piece z = -x along
// y = -0.5 right of it, also while the plot draws; drawn to three quarters, the second piece
// ends at x = 0.
void depthSplitPlotPieces(Renderer* renderer)
{
    auto config = topView();
    auto plate = Color::hex("#2050c0");
    auto white = Color::hex("#ffffff");
    auto plot = [&](float progress) {
        Vec3 points[] = {{-2.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 0.0f}, {2.0f, 0.5f, 2.0f},
                         {-2.0f, -0.5f, 2.0f}, {0.0f, -0.5f, 0.0f}, {2.0f, -0.5f, -2.0f}};
        uint32_t ends[] = {3u, 6u};
        auto made = Plot::gen(points, 6u, ends, 2u);
        if (made) {
            made->stroke(white, 4.0f);
            made->progress = progress;
        }
        return made;
    };
    Surface frame;
    Object* objects[] = {depthPlate(0.0f, plate), plot(1.0f)};
    if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
        CHECK(close(framePixel(frame, 80u, 80u), plate, 8));
        CHECK(close(framePixel(frame, 120u, 80u), white, 8));
        CHECK(close(framePixel(frame, 80u, 120u), white, 8));
        CHECK(close(framePixel(frame, 120u, 120u), plate, 8));
        CHECK(close(framePixel(frame, 40u, 80u), white, 8));
        CHECK(close(framePixel(frame, 160u, 120u), white, 8));
    }
    Object* drawing[] = {depthPlate(0.0f, plate), plot(0.75f)};
    if (depthRender(renderer, config, drawing, 2u, 0.0f, 0.0f, frame)) {
        CHECK(close(framePixel(frame, 80u, 80u), plate, 8));
        CHECK(close(framePixel(frame, 120u, 80u), white, 8));
        CHECK(close(framePixel(frame, 80u, 120u), white, 8));
        CHECK(close(framePixel(frame, 160u, 120u), config.background, 0));
    }
}

// The ticks of a space draw over its grid with its axes and hide where a surface in the
// grid's layer covers them, as the axes do. Seen from above, the plane z = 0.5 over
// [-1, 1]^2 hides the ticks 20 pixels long at x = 0.5 and y = 0.5 below it, while those at
// 1.5 beside it show.
void depthSplitSpaceTicks(Renderer* renderer)
{
    auto config = topView();
    auto plate = Color::hex("#2050c0");
    auto red = Color::hex("#f02828");
    auto green = Color::hex("#28d028");
    auto space = Space::gen({-2.0f, 2.0f, 1.0f}, {-2.0f, 2.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    if (space) {
        space->layer = 0;
        space->stroke(Color{0, 0, 0, 0}, 4.0f);
        space->axisX = red;
        space->axisY = green;
        space->ticks = {0.5f, 0.5f, 0.0f};
        space->tickLength = 20.0f;
    }
    Surface frame;
    Object* objects[] = {space, depthPlate(0.5f, plate)};
    if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
        // Rows 90 to 110 across x = 0.5 and 1.5, beside the axis, which covers rows 98 to 102.
        CHECK(close(framePixel(frame, 120u, 92u), plate, 0));
        CHECK(close(framePixel(frame, 160u, 92u), red, 8));
        // Columns 90 to 110 across y = 0.5 and 1.5.
        CHECK(close(framePixel(frame, 92u, 80u), plate, 0));
        CHECK(close(framePixel(frame, 92u, 40u), green, 8));
    }
}

void surfacePlaneTextAndWrite(Renderer* renderer)
{
    Vec3 points[] = {
        {-1.5f, 0.0f, -1.5f}, {0.0f, 0.4f, -1.5f}, {1.5f, 0.0f, -1.5f},
        {-1.5f, 0.2f, 0.0f}, {0.0f, 0.9f, 0.0f}, {1.5f, 0.2f, 0.0f},
        {-1.5f, 0.0f, 1.5f}, {0.0f, 0.4f, 1.5f}, {1.5f, 0.0f, 1.5f}};
    CHECK(SurfaceMesh::gen(nullptr, 3u, 3u) == nullptr);
    CHECK(SurfaceMesh::gen(points, 1u, 3u) == nullptr);
    auto surface = SurfaceMesh::gen(points, 3u, 3u);
    CHECK(surface != nullptr);
    if (surface) {
        CHECK(surface->type() == Type::SurfaceMesh);
        CHECK(surface->columns() == 3u && surface->rows() == 3u);
        CHECK(std::strcmp(type(surface->type()), "surface") == 0);
        surface->mode = SurfaceMode::Solid;
        Bounds bounds;
        CHECK(surface->bounds(bounds));
        CHECK(bounds.min.x == -1.5f && bounds.max.y == 0.9f && bounds.max.z == 1.5f);
    }

    Config config;
    config.width = 180;
    config.height = 140;
    config.cameraView = CameraView::ThreeD;
    config.camera.eye = {4.0f, 3.4f, 5.0f};
    config.camera.target = {};
    config.camera.projection = Projection::Perspective;
    config.camera.near = 0.1f;
    config.camera.far = 100.0f;
    config.background = Color::hex("#f7f8fc");
    auto scene = Scene::gen(config);
    auto text = Text::gen("plane", {0.0f, -0.1f, 1.8f});
    CHECK(scene && surface && text);
    if (scene && surface && text) {
        text->orientation = TextOrientation::Plane;
        text->size = 0.45f;
        text->fill(Color::hex("#172033"));
        CHECK(scene->add(surface) == Result::Success);
        CHECK(scene->add(text) == Result::Success);
        CHECK(scene->play(Animation::create(surface), 0.6f, Easing::Linear)
              == Result::Success);
        CHECK(scene->play(Animation::fadeIn(text), 0.2f, Easing::Linear)
              == Result::Success);
        Surface frame;
        CHECK(renderer->render(scene, scene->duration(), frame) == Result::Success);
        CHECK(changed(frame, config.background) > 400u);
        BBox textBounds;
        CHECK(renderer->bounds(scene, text, scene->duration(), textBounds) == Result::Success);
        CHECK(textBounds.width > 0.0f && textBounds.height > 0.0f);
    } else {
        delete surface;
        delete text;
    }
    delete scene;

    config.cameraView = CameraView::TwoD;
    config.camera = {};
    config.camera.orthoHeight = 4.0f;
    config.width = 120;
    config.height = 120;
    config.background = Color::hex("#101010");
    PathCommand commands[] = {
        PathCommand::move({-1.0f, -1.0f, 0.0f}),
        PathCommand::line({1.0f, -1.0f, 0.0f}),
        PathCommand::line({1.0f, 1.0f, 0.0f}),
        PathCommand::line({-1.0f, 1.0f, 0.0f}),
        PathCommand::close()};
    scene = Scene::gen(config);
    auto path = Path::gen(commands, 5u, 8u);
    CHECK(scene && path);
    if (scene && path) {
        auto fill = Color::hex("#ef6461");
        path->stroke(Color::hex("#ef6461"), 3.0f).fill(fill);
        CHECK(scene->add(path) == Result::Success);
        CHECK(scene->play(Animation::write(path), 1.0f, Easing::Linear)
              == Result::Success);
        Surface frame;
        auto center = static_cast<size_t>(60u) * 120u + 60u;
        CHECK(renderer->render(scene, 0.20f, frame) == Result::Success);
        CHECK(frame.data()[center] == pixel(config.background));
        CHECK(renderer->render(scene, 0.65f, frame) == Result::Success);
        CHECK(frame.data()[center] != pixel(config.background));
        CHECK(renderer->render(scene, 1.0f, frame) == Result::Success);
        CHECK(frame.data()[center] == pixel(fill));
    } else {
        delete path;
    }
    delete scene;
}
