#include "testSceneChecks.h"

// The camera and view a scene shows, read through a runtime camera hook.
struct CameraSample
{
    Camera camera;
    CameraView view = CameraView::TwoD;
    bool taken = false;
};

static Result _takeCamera(float, Camera& camera, CameraView& view, void* data) noexcept
{
    auto sample = static_cast<CameraSample*>(data);
    sample->camera = camera;
    sample->view = view;
    sample->taken = true;
    return Result::Success;
}

// Whether the scene, whose runtime hook is _takeCamera on sample, shows a valid camera at time.
static bool _cameraAt(Scene* scene, float time, CameraSample& sample)
{
    sample.taken = false;
    scene->view(time);
    return sample.taken;
}

// A camera move may begin while another runs: it adds its shift, turn, zoom and lens change to
// that move, and switches the view and projection when a plain look would.
void cameraLayers(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto mark = Rectangle::gen({4.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(scene && mark);
    if (!scene || !mark) {
        delete mark;
        delete scene;
        return;
    }
    mark->style.stroke.a = 0;
    mark->fill(Color::hex("#ffffff"));
    CHECK(scene->add(mark) == Result::Success);

    // A zoom over three seconds begins half a second into a pan over two: the target travels
    // while the view height shrinks by the same factor every second.
    auto camera = scene->config().camera;
    camera.target = {4.0f, 0.0f, 0.0f};
    camera.eye = {4.0f, 0.0f, 10.0f};
    CHECK(scene->look(camera, CameraView::TwoD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    camera.orthoHeight = 2.5f;
    CHECK(scene->look(camera, CameraView::TwoD, 3.0f, Easing::Linear) == Result::Success);
    // _placed reads a frame 10 units high around the origin, so the mark's offset and size
    // scale by 10 over the view height.
    auto framed = [&](float time, float target, float height) {
        auto zoom = 10.0f / height;
        return _placed(renderer, scene, mark, time, (4.0f - target) * zoom, 0.0f, zoom);
    };
    CHECK(framed(0.5f, 1.0f, 10.0f));
    CHECK(framed(1.0f, 2.0f, 10.0f * std::pow(0.25f, 1.0f / 6.0f)));
    CHECK(framed(2.0f, 4.0f, 5.0f));
    CHECK(framed(3.5f, 4.0f, 2.5f));

    // A look after both starts where they leave the camera.
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.eye = {0.0f, 0.0f, 10.0f};
    camera.orthoHeight = 10.0f;
    CHECK(scene->seek(3.5f) == Result::Success);
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(framed(4.0f, 2.0f, 6.25f));

    // Clip planes added to that look ease from the current camera's to their own while it pans
    // and zooms, and the mark, 10 units in front of the eye, goes once the near plane passes it.
    CameraSample sample;
    RuntimeModifier hook;
    hook.camera = _takeCamera;
    hook.data = &sample;
    CHECK(scene->runtime(&hook) == Result::Success);
    CHECK(scene->seek(4.0f) == Result::Success);
    auto planes = camera;
    planes.near = 15.0f;
    planes.far = 40.0f;
    CHECK(scene->look(planes, CameraView::TwoD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(scene, 4.25f, sample) && near(sample.camera.near, 0.05f + 14.95f * 0.25f, 1.0e-3f)
          && near(sample.camera.far, 760.0f, 1.0e-2f) && near(sample.camera.target.x, 1.0f, 1.0e-3f));
    CHECK(framed(4.25f, 1.0f, 8.125f));
    CHECK(framed(4.5f, 0.0f, 10.0f));
    Surface surface;
    CHECK(renderer->render(scene, 4.5f, surface) == Result::Success);
    CHECK(changed(surface, Color::hex("#000000")) == 400u);
    CHECK(renderer->render(scene, 4.9f, surface) == Result::Success);
    CHECK(changed(surface, Color::hex("#000000")) == 0u);
    CHECK(_cameraAt(scene, 5.0f, sample) && near(sample.camera.near, 15.0f) && near(sample.camera.far, 40.0f)
          && sample.view == CameraView::TwoD);
    delete scene;

    // In 3D a zoom adds to an orbit, and an orbit turns about the target a pan moves.
    config.cameraView = CameraView::ThreeD;
    auto spatial = Scene::gen(config);
    auto dot = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(spatial && dot);
    if (!spatial || !dot) {
        delete dot;
        delete spatial;
        return;
    }
    dot->stroke(Color::hex("#ffffff")).fill(Color::hex("#ffffff"));
    dot->style.radius = 3.0f;
    CHECK(spatial->add(dot) == Result::Success);
    constexpr auto QUARTER = 1.5707963f;
    CHECK(spatial->orbit(QUARTER, {0.0f, 1.0f, 0.0f}, 2.0f, Easing::Linear) == Result::Success);
    CHECK(spatial->seek(1.0f) == Result::Success);
    auto zoomed = spatial->config().camera;
    zoomed.orthoHeight = 5.0f;
    CHECK(spatial->look(zoomed, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    // After a turn of angle about +y the dot sits 2 cos(angle) right of the target.
    CHECK(_placed(renderer, spatial, dot, 1.5f, 2.0f * std::cos(0.75f * QUARTER) * std::sqrt(2.0f), 0.0f));
    CHECK(_placed(renderer, spatial, dot, 2.0f, 0.0f, 0.0f));
    // An orbit added to a pan up circles the eye about the rising target.
    auto raised = spatial->config().camera;
    raised.target.y += 1.0f;
    raised.eye.y += 1.0f;
    CHECK(spatial->look(raised, CameraView::ThreeD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(spatial->seek(3.0f) == Result::Success);
    CHECK(spatial->orbit(QUARTER, {0.0f, 1.0f, 0.0f}, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, spatial, dot, 3.5f, 4.0f * std::cos(1.5f * QUARTER), -1.5f));
    CHECK(_placed(renderer, spatial, dot, 4.0f, -4.0f, -2.0f));
    auto& last = spatial->config().camera;
    CHECK(near(last.target.y, 1.0f) && near(last.eye.x, 0.0f, 1.0e-3f) && near(last.eye.y, 1.0f, 1.0e-3f)
          && near(last.eye.z, -10.0f, 1.0e-3f));
    delete spatial;

    // A look that turns the view during a pan turns about the moving target, part way along
    // its shortest turn.
    auto turning = Scene::gen(config);
    auto spot = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(turning && spot);
    if (!turning || !spot) {
        delete spot;
        delete turning;
        return;
    }
    spot->stroke(Color::hex("#ffffff")).fill(Color::hex("#ffffff"));
    spot->style.radius = 3.0f;
    CHECK(turning->add(spot) == Result::Success);
    auto pan = turning->config().camera;
    pan.target.y += 1.0f;
    pan.eye.y += 1.0f;
    CHECK(turning->look(pan, CameraView::ThreeD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(turning->seek(0.5f) == Result::Success);
    auto side = pan;
    side.eye = {10.0f, 1.0f, 0.0f};
    CHECK(turning->look(side, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, turning, spot, 1.0f, 2.0f * std::cos(0.5f * QUARTER), -0.5f));
    CHECK(_placed(renderer, turning, spot, 1.5f, 0.0f, -0.75f));
    CHECK(_placed(renderer, turning, spot, 2.0f, 0.0f, -1.0f));
    delete turning;

    // A projection switch added to a pan switches half way, as a plain look does, while the pan
    // goes on. 10 units from the eye a perspective camera fits 20 tan(fov / 2) units in the frame
    // height, where the orthographic one fits 10.
    auto switched = Scene::gen(config);
    auto square = Rectangle::gen({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    CHECK(switched && square);
    if (!switched || !square) {
        delete square;
        delete switched;
        return;
    }
    square->style.stroke.a = 0;
    square->fill(Color::hex("#ffffff"));
    CHECK(switched->add(square) == Result::Success);
    CHECK(switched->runtime(&hook) == Result::Success);
    auto panned = switched->config().camera;
    panned.target.x += 1.0f;
    panned.eye.x += 1.0f;
    CHECK(switched->look(panned, CameraView::ThreeD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(switched->seek(0.5f) == Result::Success);
    auto perspective = panned;
    perspective.projection = Projection::Perspective;
    CHECK(switched->look(perspective, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(switched, 0.9f, sample) && sample.camera.projection == Projection::Orthographic);
    CHECK(_cameraAt(switched, 1.0f, sample) && sample.camera.projection == Projection::Perspective);
    auto widened = 0.5f / std::tan(0.5f * 0.7853982f);
    CHECK(_placed(renderer, switched, square, 0.9f, -0.45f, 0.0f, 2.0f));
    CHECK(_placed(renderer, switched, square, 1.1f, -0.55f * widened, 0.0f, 2.0f * widened));
    CHECK(_placed(renderer, switched, square, 2.0f, -widened, 0.0f, 2.0f * widened));
    delete switched;

    // A turn into the 3D view added to a pan shows that view once it starts. A turn back to 2D
    // shows the plane straight on only at its end, so it must not end before the moves it adds
    // to, and a turn needs the 3D view for as long as it runs.
    config.cameraView = CameraView::TwoD;
    auto tilted = Scene::gen(config);
    auto tile = Rectangle::gen({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    CHECK(tilted && tile);
    if (!tilted || !tile) {
        delete tile;
        delete tilted;
        return;
    }
    tile->style.stroke.a = 0;
    tile->fill(Color::hex("#ffffff"));
    CHECK(tilted->add(tile) == Result::Success);
    CHECK(tilted->runtime(&hook) == Result::Success);
    auto across = tilted->config().camera;
    across.target = {2.0f, 0.0f, 0.0f};
    across.eye = {2.0f, 0.0f, 10.0f};
    CHECK(tilted->look(across, CameraView::TwoD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(tilted->seek(0.5f) == Result::Success);
    auto oblique = across;
    oblique.eye = {2.0f, -8.0f, 6.0f};
    oblique.projection = Projection::Perspective;
    CHECK(tilted->look(oblique, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(tilted, 0.5f, sample) && sample.view == CameraView::TwoD
          && sample.camera.projection == Projection::Orthographic);
    CHECK(_cameraAt(tilted, 0.6f, sample) && sample.view == CameraView::ThreeD
          && sample.camera.projection == Projection::Perspective && near(sample.camera.target.x, 0.6f, 1.0e-3f));
    // The eye has turned to the side of the target that the look faces from, as far as the pan
    // has carried the target.
    CHECK(_cameraAt(tilted, 1.5f, sample) && near(sample.camera.eye.x, 1.5f, 1.0e-3f)
          && near(sample.camera.eye.y, -8.0f, 1.0e-3f) && near(sample.camera.eye.z, 6.0f, 1.0e-3f));
    CHECK(tilted->seek(0.0f) == Result::Success);
    CHECK(tilted->orbit(QUARTER, {0.0f, 0.0f, 1.0f}, 1.0f) == Result::InsufficientCondition);
    CHECK(tilted->diagnostic()
          && std::strcmp(tilted->diagnostic(),
                         "the camera is in the 2D view at 0.00 s, while this move would turn it or change its "
                         "projection, which only the 3D view allows") == 0);
    CHECK(tilted->seek(1.0f) == Result::Success);
    CHECK(tilted->look(across, CameraView::TwoD, 0.5f, Easing::Linear) == Result::InsufficientCondition);
    CHECK(tilted->diagnostic()
          && std::strcmp(tilted->diagnostic(),
                         "the camera already moves until 2.00 s; a look that returns to the 2D view ends at "
                         "that time or later") == 0);
    CHECK(tilted->look(across, CameraView::TwoD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(tilted, 1.9f, sample) && sample.view == CameraView::ThreeD
          && sample.camera.projection == Projection::Perspective);
    CHECK(_cameraAt(tilted, 2.0f, sample) && sample.view == CameraView::TwoD);
    CHECK(_placed(renderer, tilted, tile, 2.0f, -2.0f, 0.0f, 2.0f));
    delete tilted;

    // A turn needs the 3D view for as long as it runs, so a layered orbit or turning look fails
    // while a move stored earlier still shows the 2D view, and runs once none shows it.
    config.cameraView = CameraView::TwoD;
    auto rising = Scene::gen(config);
    CHECK(rising);
    if (!rising) return;
    CHECK(rising->runtime(&hook) == Result::Success);
    auto slanted = rising->config().camera;
    slanted.eye = {0.0f, -8.0f, 6.0f};
    CHECK(rising->seek(1.0f) == Result::Success);
    CHECK(rising->look(slanted, CameraView::ThreeD, 1.0f) == Result::Success);
    CHECK(rising->seek(0.0f) == Result::Success);
    CHECK(rising->orbit(QUARTER, {0.0f, 0.0f, 1.0f}, 2.0f) == Result::InsufficientCondition);
    CHECK(rising->diagnostic() && std::strstr(rising->diagnostic(), "the camera is in the 2D view at 0.00 s"));
    auto sideways = slanted;
    sideways.eye = {8.0f, 0.0f, 6.0f};
    CHECK(rising->look(sideways, CameraView::ThreeD, 2.0f) == Result::InsufficientCondition);
    CHECK(rising->diagnostic() && std::strstr(rising->diagnostic(), "the camera is in the 2D view at 0.00 s"));
    // A zoom does not turn, so it may add to the 2D view.
    auto zoom = slanted;
    zoom.orthoHeight = 5.0f;
    CHECK(rising->look(zoom, CameraView::ThreeD, 1.0f) == Result::Success);
    CHECK(_cameraAt(rising, 0.5f, sample) && sample.view == CameraView::TwoD);
    CHECK(rising->seek(1.5f) == Result::Success);
    CHECK(rising->orbit(QUARTER, {0.0f, 0.0f, 1.0f}, 1.0f) == Result::Success);
    CHECK(_cameraAt(rising, 1.75f, sample) && sample.view == CameraView::ThreeD);
    delete rising;
}

// A shift, turn, scale, travel or effect may begin while another clip moves the object: it adds
// its change on top, turning and scaling about a point of the object that travels with it.
void layeredMotions(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto grown = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto spun = Rectangle::gen({-2.0f, 3.0f, 0.0f}, {1.0f, 1.0f});
    auto orbited = Rectangle::gen({-2.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto pulsed = Rectangle::gen({-2.0f, -1.5f, 0.0f}, {1.0f, 1.0f});
    auto carried = Rectangle::gen({-2.0f, 2.0f, 0.0f}, {1.0f, 1.0f});
    auto track = Line::gen({-2.0f, 2.0f, 0.0f}, {2.0f, 2.0f, 0.0f});
    auto summed = Rectangle::gen({-2.0f, -1.0f, 0.0f}, {1.0f, 1.0f});
    auto flat = Rectangle::gen({3.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto shape = Circle::gen({0.0f, 0.0f, 0.0f}, 0.5f);
    Object* objects[] = {grown, spun, orbited, pulsed, carried, track, summed, flat, shape};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    for (auto object : objects) {
        if (object == shape) continue;
        object->style.stroke.a = object == track ? 255 : 0;
        if (object != track) object->fill(Color::hex("#ffffff"));
        CHECK(scene->add(object) == Result::Success);
    }

    // A scale while the object travels grows it about its moving center, and a turn played
    // after both starts from where they left it.
    CHECK(scene->play(Animation::shift(grown, {4.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::rescale(grown, {2.0f, 2.0f, 1.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, grown, 0.25f, -1.5f, 0.0f, 1.0f));
    CHECK(_placed(renderer, scene, grown, 1.0f, 0.0f, 0.0f, 1.5f));
    CHECK(_placed(renderer, scene, grown, 1.75f, 1.5f, 0.0f, 2.0f));
    CHECK(_placed(renderer, scene, grown, 2.0f, 2.0f, 0.0f, 2.0f));
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(grown, 1.5707963f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, grown, 2.0f, 2.0f, 0.0f, 2.0f));
    CHECK(_placed(renderer, scene, grown, 2.5f, 2.0f, 0.0f));
    CHECK(_placed(renderer, scene, grown, 3.0f, 2.0f, 0.0f, 2.0f));

    // A shift during a turn carries the turning object.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(spun, 1.5707963f), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::shift(spun, {2.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, spun, 1.0f, -1.0f, 3.0f));
    CHECK(_placed(renderer, scene, spun, 2.0f, 0.0f, 3.0f, 1.0f));

    // A pivot outside the object stays where it is while the object travels.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(orbited, {1.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(orbited, 3.14159265f, {0.0f, -3.0f, 0.0f}, {0.0f, 0.0f, 1.0f}), 1.0f,
                      Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, orbited, 1.5f, 0.0f, -4.25f));
    CHECK(_placed(renderer, scene, orbited, 2.0f, 1.0f, -3.0f, 1.0f));

    // An indication pulses the moving object about its center and leaves it as it was.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(pulsed, {4.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::indicate(pulsed), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, pulsed, 1.0f, 0.0f, -1.5f, 1.2f));
    CHECK(_placed(renderer, scene, pulsed, 2.0f, 2.0f, -1.5f, 1.0f));
    CHECK(near(pulsed->model.e[0], 1.0f) && near(pulsed->model.e[3], 4.0f));

    // A travel along a path while the object grows keeps its center on the path.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::rescale(carried, {2.0f, 2.0f, 1.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::moveAlong(carried, track), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, carried, 0.25f, -2.0f, 2.0f, 1.125f));
    CHECK(_placed(renderer, scene, carried, 1.0f, 0.0f, 2.0f, 1.5f));
    CHECK(_placed(renderer, scene, carried, 2.0f, 2.0f, 2.0f, 2.0f));

    // Shifts add up, whether made by shift, by a play target, or by a transform that only moves.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(summed, {2.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(summed, {0.0f, 2.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(summed).shift({1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->play(Animation::transform(summed, Mat4::translate({0.0f, -1.0f, 0.0f}) * summed->model), 0.5f,
                      Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, summed, 2.0f, 0.5f, 0.0f, 1.0f));
    CHECK(_placed(renderer, scene, summed, 3.0f, 1.0f, 0.0f, 1.0f));
    CHECK(near(summed->model.e[3], 3.0f) && near(summed->model.e[7], 1.0f));

    // A morph that begins during a motion must not end before it.
    CHECK(scene->add(shape) == Result::Success);
    CHECK(scene->play(Animation::morph(summed, shape), 0.25f) == Result::InsufficientCondition);
    CHECK(scene->diagnostic()
          && std::strcmp(scene->diagnostic(), "the object already animates its transform until 3.00 s; a morph that "
                                              "begins before then must end at that time or later") == 0);

    // A turn cannot add to a motion that flattens the object, though a shift can.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::rescale(flat, {0.0f, 1.0f, 1.0f}), 2.0f) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(flat, 1.0f), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    CHECK(scene->play(Animation::shift(flat, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    delete scene;
}

// Whether the points of a probe at its origin and one unit along x and y stand where model puts
// them, in a 200 by 200 scene ten units high.
static bool _probed(SwRenderer* renderer, Scene* scene, Group* probe, float time, const Mat4& model)
{
    const Vec3 points[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    auto placed = true;
    for (auto i = 0u; i < 3u; i++) {
        auto point = model * points[i];
        placed = _placed(renderer, scene, probe->childAt(i), time, point.x, point.y) && placed;
    }
    return placed;
}

// The matrix a transform without an arc passes through: every entry travels straight.
static Mat4 _blend(const Mat4& from, const Mat4& to, float progress)
{
    Mat4 blended;
    for (auto i = 0u; i < 16u; i++) blended.e[i] = from.e[i] + (to.e[i] - from.e[i]) * progress;
    return blended;
}

// Whether two matrices agree entry by entry.
static bool _matches(const Mat4& lhs, const Mat4& rhs)
{
    for (auto i = 0u; i < 16u; i++) {
        if (!near(lhs.e[i], rhs.e[i])) return false;
    }
    return true;
}

// A transform that turns, scales or skews may begin while another clip moves the object: it adds
// its change from the authored model on top, about the object's moving center, and travels as it
// would alone, so the object lands on its matrix once every motion ends.
void layeredTransforms(SwRenderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto turned = Group::gen();
    auto swung = Group::gen();
    auto sheared = Group::gen();
    auto jumped = Group::gen();
    auto flat = Group::gen();
    Group* probes[] = {turned, swung, sheared, jumped, flat};
    Object* points[15] = {};
    for (auto i = 0u; i < 15u; i++) points[i] = Point::gen({i % 3u == 1u ? 1.0f : 0.0f, i % 3u == 2u ? 1.0f : 0.0f, 0.0f});
    auto created = scene != nullptr;
    for (auto probe : probes) created = created && probe;
    for (auto point : points) created = created && point;
    CHECK(created);
    if (!created) {
        for (auto probe : probes) delete probe;
        for (auto point : points) delete point;
        delete scene;
        return;
    }
    for (auto i = 0u; i < 15u; i++) CHECK(probes[i / 3u]->add(points[i]) == Result::Success);
    turned->model = Mat4::translate({-3.0f, 3.0f, 0.0f});
    swung->model = Mat4::translate({-3.0f, 0.0f, 0.0f});
    sheared->model = Mat4::translate({-3.0f, -3.0f, 0.0f});
    jumped->model = Mat4::translate({-3.0f, -1.0f, 0.0f});
    flat->model = Mat4::translate({3.0f, -3.0f, 0.0f});
    for (auto probe : probes) CHECK(scene->add(probe) == Result::Success);
    Animation moves[] = {
        Animation::shift(turned, {4.0f, 0.0f, 0.0f}),
        Animation::shift(swung, {4.0f, 0.0f, 0.0f}),
        Animation::rotate(sheared, PI * 0.5f),
        Animation::shift(jumped, {4.0f, 0.0f, 0.0f}),
        Animation::rescale(flat, {0.0f, 1.0f, 1.0f}),
    };
    CHECK(scene->play(moves, 5u, 2.0f, Easing::Linear) == Result::Success);

    // A quarter turn during a shift: without an arc it shrinks midway as it would alone, and
    // with an arc equal to the turn it stays rigid, while the shift carries it either way.
    auto quarter = Mat4::rotateZ(PI * 0.5f);
    auto straight = Mat4::translate({1.0f, 3.0f, 0.0f}) * quarter;
    auto arced = Mat4::translate({1.0f, 0.0f, 0.0f}) * quarter;
    Animation turns[] = {Animation::transform(turned, straight), Animation::transform(swung, arced)};
    turns[1].arc = PI * 0.5f;
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(turns, 2u, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_probed(renderer, scene, turned, 0.25f, Mat4::translate({-2.5f, 3.0f, 0.0f})));
    CHECK(_probed(renderer, scene, turned, 1.0f,
                  Mat4::translate({-1.0f, 3.0f, 0.0f}) * _blend(Mat4::identity(), quarter, 0.5f)));
    CHECK(_probed(renderer, scene, turned, 1.5f, Mat4::translate({0.0f, 3.0f, 0.0f}) * quarter));
    CHECK(_probed(renderer, scene, turned, 2.0f, straight));
    CHECK(_probed(renderer, scene, swung, 1.0f, Mat4::translate({-1.0f, 0.0f, 0.0f}) * Mat4::rotateZ(PI * 0.25f)));
    CHECK(_probed(renderer, scene, swung, 1.5f, Mat4::translate({0.0f, 0.0f, 0.0f}) * quarter));
    CHECK(_probed(renderer, scene, swung, 2.0f, arced));
    CHECK(_matches(turned->model, straight) && _matches(swung->model, arced));

    // A play target skews about the center of a turning object.
    auto start = Mat4::translate({-3.0f, -3.0f, 0.0f});
    auto skew = Mat4::identity();
    skew.e[1] = 0.5f;
    auto skewed = Mat4::translate({-2.5f, -2.5f, 0.0f}) * skew * Mat4::translate({2.5f, 2.5f, 0.0f});
    auto target = skewed * sheared->model;
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(sheared).transform(target), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_probed(renderer, scene, sheared, 1.0f,
                  _blend(Mat4::identity(), skewed, 0.5f) * _rotationAbout(PI * 0.25f, {-2.5f, -2.5f, 0.0f}) * start));
    CHECK(_probed(renderer, scene, sheared, 1.5f, skewed * _rotationAbout(PI * 0.375f, {-2.5f, -2.5f, 0.0f}) * start));
    CHECK(_probed(renderer, scene, sheared, 2.0f, target));

    // A change at the cursor jumps on top of the running shift.
    auto jump = Mat4::translate({1.0f, -1.0f, 0.0f}) * quarter * Mat4::scale({0.5f, 0.5f, 1.0f});
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(jumped).transform(jump)) == Result::Success);
    CHECK(_probed(renderer, scene, jumped, 0.99f, Mat4::translate({-1.02f, -1.0f, 0.0f})));
    CHECK(_probed(renderer, scene, jumped, 1.0f, Mat4::translate({-2.0f, 0.0f, 0.0f}) * jump));
    CHECK(_probed(renderer, scene, jumped, 1.5f, Mat4::translate({-1.0f, 0.0f, 0.0f}) * jump));
    CHECK(_probed(renderer, scene, jumped, 2.0f, jump));
    CHECK(_matches(jumped->model, jump));

    // Only a shift adds to a motion that flattens the object.
    auto turn = Mat4::rotateZ(0.5f) * flat->model;
    CHECK(scene->play(Animation::transform(flat, turn), 0.5f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it, so only a shift can add to it"));
    CHECK(scene->play(AnimationTarget::from(flat).transform(turn), 0.5f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    CHECK(scene->update(AnimationTarget::from(flat).transform(turn)) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    CHECK(scene->play(Animation::transform(flat, Mat4::translate({0.0f, 1.0f, 0.0f}) * flat->model), 0.5f)
          == Result::Success);
    delete scene;
}

// A morph may begin while its source moves: it adds the move to the place of the shape it becomes
// on top of that motion, and ends no sooner, so the shape takes over where the source lands.
void layeredMorphs(SwRenderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto square = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto twin = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto bar = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 0.5f});
    auto barTwin = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 0.5f});
    auto faded = Rectangle::gen({-2.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto flat = Rectangle::gen({3.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto disc = Circle::gen({0.0f, -3.0f, 0.0f}, 0.5f);
    Object* objects[] = {square, twin, bar, barTwin, faded, flat, disc};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    for (auto object : objects) {
        object->style.stroke.a = 0;
        object->fill(Color::hex("#ffffff"));
    }
    // The twin morphs at rest from where the square lands, into a twin of the bar.
    twin->model = Mat4::translate({4.0f, 0.0f, 0.0f});
    bar->model = Mat4::translate({2.0f, 2.0f, 0.0f}) * Mat4::rotateZ(PI * 0.5f);
    barTwin->model = bar->model;
    Object* early[] = {square, twin, faded, flat};
    for (auto object : early) CHECK(scene->add(object) == Result::Success);
    Animation moves[] = {
        Animation::shift(square, {4.0f, 0.0f, 0.0f}),
        Animation::fade(faded, 0.5f),
        Animation::rescale(flat, {0.0f, 1.0f, 1.0f}),
    };
    CHECK(scene->play(moves, 3u, 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    Object* targets[] = {bar, barTwin, disc};
    for (auto object : targets) CHECK(scene->add(object) == Result::Success);
    Animation morphs[] = {Animation::morph(square, bar), Animation::morph(twin, barTwin)};
    CHECK(scene->play(morphs, 2u, 1.0f, Easing::Linear) == Result::Success);

    // The morph during the shift is the morph at rest, carried back by the part of the shift
    // still to come, and the bar takes over where both end.
    CHECK(_placed(renderer, scene, square, 1.0f, 0.0f, 0.0f, 1.0f));
    const float times[] = {1.25f, 1.5f, 1.75f};
    for (auto time : times) {
        BBox moving;
        BBox resting;
        CHECK(renderer->bounds(scene, square, time, moving) == Result::Success);
        CHECK(renderer->bounds(scene, twin, time, resting) == Result::Success);
        CHECK(near(moving.center().x, resting.center().x + 20.0f * (2.0f * time - 4.0f), 1.0f));
        CHECK(near(moving.center().y, resting.center().y, 1.0f));
        CHECK(near(moving.width, resting.width, 1.0f) && near(moving.height, resting.height, 1.0f));
    }
    CHECK(_placed(renderer, scene, bar, 2.0f, 2.0f, 2.0f));
    BBox bounds;
    CHECK(renderer->bounds(scene, bar, 2.0f, bounds) == Result::Success);
    CHECK(near(bounds.width, 10.0f, 1.0f) && near(bounds.height, 20.0f, 1.0f));

    // A morph still waits for another property of its source, and cannot add to a motion that
    // flattens it.
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::morph(faded, disc), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "the object already animates its opacity until 2.00 s; begin at that time or later"));
    CHECK(scene->play(Animation::morph(flat, disc), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    delete scene;
}

void instantUpdates(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto green = Color::hex("#00ff00");
    auto scene = Scene::gen(config);
    auto circle = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto other = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto group = Group::gen();
    auto heir = Line::gen({-3.0f, -1.5f, 0.0f}, {3.0f, -1.5f, 0.0f});
    auto line = Line::gen({-3.0f, 1.5f, 0.0f}, {3.0f, 1.5f, 0.0f});
    Object* objects[] = {circle, other, group, heir, line};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    circle->stroke(Color{0, 0, 0, 0});
    CHECK(group->add(heir) == Result::Success);
    Object* roots[] = {circle, other, group, line};
    for (auto object : roots) CHECK(scene->add(object) == Result::Success);

    // Before the timeline starts the values become the starting state, without clips.
    CHECK(scene->update(AnimationTarget::from(circle).fill(red).radius(1.0f)) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(group).strokeWidth(6.0f)) == Result::Success);
    CHECK(scene->clipCount() == 0u);
    CHECK(near(circle->radius, 1.0f) && same(circle->style.fill, red) && near(heir->style.width, 6.0f));
    // A failed update leaves the object as it was.
    heir->style.fill = blue;
    CHECK(scene->update(AnimationTarget::from(group).strokeWidth(2.0f)) == Result::InvalidArguments);
    CHECK(_says(scene, "takes its color from the group was edited directly"));
    CHECK(near(group->style.width, 6.0f) && near(heir->style.width, 6.0f));
    CHECK(scene->update(heir) == Result::Success);

    // Afterwards they switch at the cursor, for a group's shapes too.
    CHECK(scene->play(AnimationTarget::from(other).opacity(0.5f), 1.0f) == Result::Success);
    auto clips = scene->clipCount();
    CHECK(scene->update(AnimationTarget::from(circle).fill(blue)) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(group).stroke(green)) == Result::Success);
    CHECK(scene->clipCount() == clips + 3u);
    CHECK(near(scene->cursor(), 1.0f) && near(scene->duration(), 1.0f));
    Surface surface;
    CHECK(renderer->render(scene, 0.99f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(red));
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(blue));
    CHECK(_pixelAt(surface, 80u, 70u) == pixel(green));

    // A change at the cursor must not land inside a running animation of the same property,
    // except a change of the transform, which jumps on top of a running motion.
    CHECK(scene->play(AnimationTarget::from(circle).shift({0.0f, 1.0f, 0.0f}).opacity(0.5f), 2.0f, Easing::Linear)
          == Result::Success);
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(circle).opacity(1.0f)) == Result::InsufficientCondition);
    CHECK(_says(scene, "the object already animates its opacity until 3.00 s"));
    CHECK(scene->update(AnimationTarget::from(circle).shift({1.0f, 0.0f, 0.0f})) == Result::Success);
    BBox bounds;
    CHECK(renderer->bounds(scene, circle, 1.99f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 40.0f, 1.0f));
    CHECK(renderer->bounds(scene, circle, 2.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 60.0f, 1.0f) && near(bounds.center().y, 25.0f, 1.0f));
    CHECK(renderer->bounds(scene, circle, 3.0f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 60.0f, 1.0f) && near(bounds.center().y, 20.0f, 1.0f));
    CHECK(near(circle->model.e[3], 1.0f) && near(circle->model.e[7], 1.0f));
    CHECK(scene->update(AnimationTarget::from(circle).fill(red)) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(circle).shift({1.0f, 0.0f, 0.0f}).arc(1.0f)) == Result::InvalidArguments);
    CHECK(_says(scene, "no travel for an arc"));
    CHECK(scene->update(AnimationTarget::from(line).radius(1.0f)) == Result::InvalidArguments);
    CHECK(_says(scene, "radius needs a circle"));
    CHECK(scene->update(AnimationTarget::from(nullptr).opacity(0.5f)) == Result::InvalidArguments);
    CHECK(_says(scene, "not part of this scene"));
    CHECK(scene->remove(line) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(line).opacity(0.5f)) == Result::InvalidArguments);
    CHECK(_says(scene, "removed earlier"));
    delete scene;
}
