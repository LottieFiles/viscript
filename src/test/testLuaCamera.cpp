#include "testLuaChecks.h"

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

// Loads a scene whose camera sample reads at each _cameraAt call.
static Scene* _cameraScene(const char* source, const char* name, CameraSample& sample)
{
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) {
        std::fprintf(stderr, "%s: %s\n", name, error);
        return nullptr;
    }
    RuntimeModifier hook;
    hook.camera = _takeCamera;
    hook.data = &sample;
    CHECK(scene->runtime(&hook) == Result::Success);
    return scene;
}

static bool _cameraAt(Scene* scene, float time, CameraSample& sample)
{
    sample.taken = false;
    scene->view(time);
    return sample.taken;
}

// A look whose target names objects aims at the center of their bounds, as focus frames them,
// and keeps the camera's direction, distance and height, also while another camera move runs.
void _cameraCenters()
{
    // In 2D it pans onto the box halfway through a pan elsewhere, and the moves add up.
    static constexpr auto pan = R"lua(
local s = vis.scene {width = 160, height = 90}
local box = s:rectangle {center = {3, 1}, size = {2, 1}}
s:parallel(
    function() s:look({target = {-4, 0}}, 2, "linear") end,
    function()
        s:wait(1)
        s:look({target = box}, 2, "linear")
    end)
return s
)lua";
    CameraSample sample;
    auto panning = _cameraScene(pan, "center-pan.lua", sample);
    if (panning) {
        CHECK(_cameraAt(panning, 0.0f, sample));
        auto opening = sample.camera;
        CHECK(_cameraAt(panning, 1.0f, sample) && _near(sample.camera.target, -2.0f, 0.0f, 0.0f)
              && _near(sample.camera.eye, -2.0f, 0.0f, opening.eye.z));
        CHECK(_cameraAt(panning, 2.0f, sample) && _near(sample.camera.target, -0.5f, 0.5f, 0.0f)
              && _near(sample.camera.eye, -0.5f, 0.5f, opening.eye.z));
        CHECK(_cameraAt(panning, 3.0f, sample) && _near(sample.camera.target, 3.0f, 1.0f, 0.0f)
              && _near(sample.camera.eye, 3.0f, 1.0f, opening.eye.z) && sample.view == CameraView::TwoD
              && sample.camera.orthoHeight == opening.orthoHeight);
        auto& camera = panning->config().camera;
        CHECK(camera.target.x == 3.0f && camera.target.y == 1.0f && camera.eye.x == 3.0f && camera.eye.y == 1.0f);
        CHECK(std::fabs(panning->duration() - 3.0f) < 1.0e-4f);
    }
    delete panning;

    // In 3D it aims at the edge's center while an orbit turns, keeping the eye's distance, and
    // the orbit goes on turning about the new target.
    static constexpr auto orbit = R"lua(
local s = vis.scene {width = 160, height = 90,
                       camera = {mode = "fixed", view = "3d", eye = {0, 0, 10}, target = {0, 0, 0}, up = {0, 1, 0},
                                 projection = "perspective"}}
local edge = s:line {from = {1, 0, -2}, to = {3, 2, 0}}
s:parallel(
    function() s:orbit(2 * math.pi, {duration = 4, curve = "linear"}) end,
    function()
        s:wait(1)
        s:look({target = edge}, 2, "linear")
    end)
return s
)lua";
    auto orbiting = _cameraScene(orbit, "center-orbit.lua", sample);
    if (orbiting) {
        CHECK(_cameraAt(orbiting, 1.0f, sample) && _near(sample.camera.target, 0.0f, 0.0f, 0.0f, 1.0e-3f)
              && _near(sample.camera.eye.y, 0.0f) && std::fabs(std::fabs(sample.camera.eye.x) - 10.0f) < 1.0e-3f);
        CHECK(_cameraAt(orbiting, 2.0f, sample) && _near(sample.camera.target, 1.0f, 0.5f, -0.5f, 1.0e-3f)
              && _near(sample.camera.eye, 1.0f, 0.5f, -10.5f, 1.0e-3f));
        CHECK(_cameraAt(orbiting, 3.0f, sample) && _near(sample.camera.target, 2.0f, 1.0f, -1.0f, 1.0e-3f)
              && std::fabs(std::fabs(sample.camera.eye.x - 2.0f) - 10.0f) < 1.0e-3f
              && std::fabs(sample.camera.eye.y - 1.0f) < 1.0e-3f && std::fabs(sample.camera.eye.z + 1.0f) < 1.0e-3f);
        CHECK(_cameraAt(orbiting, 4.0f, sample) && _near(sample.camera.target, 2.0f, 1.0f, -1.0f, 1.0e-3f)
              && _near(sample.camera.eye, 2.0f, 1.0f, 9.0f, 1.0e-3f) && sample.view == CameraView::ThreeD);
        auto& camera = orbiting->config().camera;
        CHECK(_near(camera.target, 2.0f, 1.0f, -1.0f, 1.0e-3f) && _near(camera.eye, 2.0f, 1.0f, 9.0f, 1.0e-3f));
    }
    delete orbiting;

    // A list aims at the center of all their bounds, from an eye the look gives, and the other
    // fields apply as in any look.
    static constexpr auto aim = R"lua(
local s = vis.scene {camera = {mode = "fixed", view = "3d", eye = {0, -8, 6}}}
local disc = s:circle {center = {-1, 0}, radius = 1}
local edge = s:line {from = {2, 2, 1}, to = {3, 1, 1}}
s:look({target = {disc, edge}, eye = {6, -6, 6}, fov = 0.5}, 1)
s:look({target = {disc}, height = 3}, 1)
return s
)lua";
    auto aiming = _cameraScene(aim, "center-aim.lua", sample);
    if (aiming) {
        CHECK(_cameraAt(aiming, 1.0f, sample) && _near(sample.camera.target, 0.5f, 0.5f, 0.5f)
              && _near(sample.camera.eye, 6.0f, -6.0f, 6.0f) && _near(sample.camera.fov, 0.5f));
        auto& camera = aiming->config().camera;
        CHECK(_near(camera.target, -1.0f, 0.0f, 0.0f) && _near(camera.eye, 4.5f, -6.5f, 5.5f)
              && _near(camera.orthoHeight, 3.0f) && _near(camera.fov, 0.5f));
    }
    delete aiming;

    // The bounds are where the moves authored so far leave the object, so a look beside its
    // shift, with the same duration and curve, follows it.
    static constexpr auto follow = R"lua(
local s = vis.scene {}
local dot = s:circle {radius = 0.25}
s:parallel(
    function() s:shift(dot, {3, 1}, 2, "linear") end,
    function() s:look({target = dot}, 2, "linear") end)
return s
)lua";
    auto following = _cameraScene(follow, "center-follow.lua", sample);
    if (following) {
        CHECK(_cameraAt(following, 0.5f, sample) && _near(sample.camera.target, 0.75f, 0.25f, 0.0f));
        CHECK(_cameraAt(following, 1.0f, sample) && _near(sample.camera.target, 1.5f, 0.5f, 0.0f));
    }
    delete following;

    _scriptError("local s = vis.scene {} local g = s:group {} s:look({target = g}) return s",
                 "look targets have no bounds");
    _scriptError("local s = vis.scene {} local c = vis.scene {}:circle {radius = 1} s:look({target = c}) return s",
                 "handle belongs to another scene");
    _scriptError("local s = vis.scene {} local c = s:circle {radius = 1} s:look({target = {c, 2}}) return s",
                 "expected an object handle, got number");
}

static bool _sameCamera(const Camera& first, const Camera& second)
{
    auto same = [](const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
    return same(first.eye, second.eye) && same(first.target, second.target) && same(first.up, second.up)
        && first.projection == second.projection && first.fov == second.fov && first.orthoHeight == second.orthoHeight
        && first.near == second.near && first.far == second.far;
}

// focus(nil) returns to the camera before the first camera move from either view: its whole
// pose, lens, view and projection, also while another camera move runs.
void _cameraHome()
{
    // A 3D camera comes back after an orbit and a look that turned it and widened the lens.
    static constexpr auto turned = R"lua(
local s = vis.scene {width = 160, height = 90,
                       camera = {mode = "fixed", view = "3d", eye = {4, -6, 5}, target = {1, 0, 0}, up = {0, 0, 1},
                                 projection = "perspective", fov = 0.6}}
s:orbit(math.pi / 2, {duration = 1})
s:look({target = {2, 1, 0}, fov = 0.9, far = 50}, 1)
s:focus(nil, {duration = 1})
return s
)lua";
    CameraSample sample;
    auto turning = _cameraScene(turned, "home-turned.lua", sample);
    if (turning) {
        CHECK(_cameraAt(turning, 0.0f, sample));
        auto opening = sample.camera;
        CHECK(_cameraAt(turning, 2.0f, sample) && _near(sample.camera.fov, 0.9f));
        CHECK(_cameraAt(turning, 3.0f, sample) && sample.view == CameraView::ThreeD
              && _near(sample.camera.eye, 4.0f, -6.0f, 5.0f) && _near(sample.camera.target, 1.0f, 0.0f, 0.0f)
              && _near(sample.camera.up, 0.0f, 0.0f, 1.0f) && _near(sample.camera.fov, 0.6f)
              && _near(sample.camera.far, opening.far));
        CHECK(_sameCamera(turning->config().camera, opening));
    }
    delete turning;

    // During an orbit it adds its turn back to the opening view, so both end where the scene began.
    static constexpr auto orbiting = R"lua(
local s = vis.scene {width = 160, height = 90,
                       camera = {mode = "fixed", view = "3d", eye = {0, 0, 10}, target = {0, 0, 0}, up = {0, 1, 0},
                                 projection = "perspective"}}
s:parallel(
    function() s:orbit(math.pi / 2, {duration = 2, curve = "linear"}) end,
    function()
        s:wait(0.5)
        s:focus(nil, {duration = 2, curve = "linear"})
    end)
return s
)lua";
    auto circling = _cameraScene(orbiting, "home-orbit.lua", sample);
    if (circling) {
        CHECK(_cameraAt(circling, 0.0f, sample));
        auto opening = sample.camera;
        // At 1.5 s the orbit has turned 3/8 of pi and the return has undone 1/4 of pi.
        CHECK(_cameraAt(circling, 1.5f, sample) && _near(sample.camera.target, 0.0f, 0.0f, 0.0f, 1.0e-3f)
              && std::fabs(std::fabs(sample.camera.eye.x) - 10.0f * std::sin(0.125f * 3.14159265f)) < 1.0e-3f
              && std::fabs(sample.camera.eye.y) < 1.0e-3f
              && std::fabs(sample.camera.eye.z - 10.0f * std::cos(0.125f * 3.14159265f)) < 1.0e-3f);
        CHECK(_cameraAt(circling, 2.5f, sample) && _near(sample.camera.eye, 0.0f, 0.0f, 10.0f, 1.0e-3f)
              && _near(sample.camera.up, 0.0f, 1.0f, 0.0f, 1.0e-3f));
        CHECK(_sameCamera(circling->config().camera, opening));
    }
    delete circling;

    // A scene that opened in 2D turns back into it at the end of the return, exactly.
    static constexpr auto planar = R"lua(
local s = vis.scene {width = 160, height = 90, camera = {mode = "fixed", view = "2d", target = {1, 2}, height = 6}}
s:look({view = "3d", eye = {1, -6, 8}, projection = "perspective", fov = 0.7}, 1)
s:orbit(math.pi / 3, {axis = {0, 0, 1}, duration = 1})
s:focus(nil, {duration = 1})
return s
)lua";
    auto flattening = _cameraScene(planar, "home-planar.lua", sample);
    if (flattening) {
        CHECK(_cameraAt(flattening, 0.0f, sample) && sample.view == CameraView::TwoD);
        auto opening = sample.camera;
        CHECK(flattening->view(2.5f) == CameraView::ThreeD);
        CHECK(_cameraAt(flattening, 3.0f, sample) && sample.view == CameraView::TwoD
              && _sameCamera(sample.camera, opening));
        CHECK(flattening->config().cameraView == CameraView::TwoD && _sameCamera(flattening->config().camera, opening));
    }
    delete flattening;

    // Over a running orbit a return to the 2D view ends no earlier than the orbit.
    static constexpr auto late = R"lua(
local s = vis.scene {width = 160, height = 90}
s:look({view = "3d", eye = {0, -8, 6}}, 1)
s:parallel(
    function() s:orbit(math.pi, {axis = {0, 0, 1}, duration = 2}) end,
    function() s:focus(nil, {duration = 2}) end)
return s
)lua";
    auto settling = _cameraScene(late, "home-late.lua", sample);
    if (settling) {
        CHECK(_cameraAt(settling, 0.0f, sample));
        auto opening = sample.camera;
        CHECK(_cameraAt(settling, 2.0f, sample) && sample.view == CameraView::ThreeD);
        CHECK(_cameraAt(settling, 3.0f, sample) && sample.view == CameraView::TwoD
              && _sameCamera(sample.camera, opening));
    }
    delete settling;
    _scriptError("local s = vis.scene {} s:look({view = '3d', eye = {0, -8, 6}}, 1) "
                 "s:parallel(function() s:orbit(math.pi, {axis = {0, 0, 1}, duration = 2}) end, "
                 "function() s:focus(nil, {duration = 1}) end) return s",
                 "could not animate camera: the camera already moves until 3.00 s; a look that returns to the 2D "
                 "view ends at that time or later");

    // Before any camera move it holds the camera; framing targets still needs the 2D view.
    static constexpr auto still = R"lua(
local s = vis.scene {camera = {mode = "fixed", view = "3d", eye = {3, -4, 5}}}
s:focus()
return s
)lua";
    auto holding = _cameraScene(still, "home-still.lua", sample);
    if (holding) {
        CHECK(_cameraAt(holding, 0.0f, sample));
        CHECK(std::fabs(holding->duration() - 1.0f) < 1.0e-4f && _sameCamera(holding->config().camera, sample.camera));
    }
    delete holding;
    _scriptError("local s = vis.scene {camera = {mode = 'fixed', view = '3d'}} local c = s:circle {radius = 1} "
                 "s:focus(c) return s",
                 "focus requires a 2D camera; use look in 3D");
}

// The scale of the view at index in the layout report at time, or 0 when it is not shown.
static float _viewScale(Renderer* renderer, Scene* scene, float time, uint32_t index)
{
    LayoutReport report;
    if (renderer->layout(scene, time, report) != Result::Success) return -1.0f;
    char path[32];
    std::snprintf(path, sizeof(path), "root/view:%u", index);
    for (auto i = 0u; i < report.sceneCount(); i++) {
        auto item = report.sceneAt(i);
        if (item->kind == LayoutSceneKind::View && item->path && std::strcmp(item->path, path) == 0) return item->scaleX;
    }
    return 0.0f;
}

// scene:view shows the scene again through a camera of its own; the view it returns takes look,
// focus, orbit and fade as the scene does.
void _views(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 200, height = 100, background = "#000000", antialiasing = false,
                       camera = {height = 10}}
local dot = s:point {point = {2, 1}, radius = 2, fill = "#ff0000", stroke = "#ff0000", width = 2}
local ghost = s:circle {id = "ghost", center = {1.5, 1.25}, radius = 0.1, fill = "#ffff00", stroke = "#ffff00"}
s:rectangle {center = {7.5, -3.5}, size = {1, 1}, fill = "#00ff00", stroke = "#00ff00", layer = 1}
local box = s:rectangle {center = {-6, -2}, size = {4, 1}, fill = "#404040", stroke = "#404040"}
local lens = s:view({x = 0.5, y = 0.5, width = 0.5, height = 0.5},
                    {camera = {target = dot, height = 1.25}, layer = 0, background = "#0000ff", hide = ghost})
local strip = s:view({x = 0, y = 0, width = 0.25, height = 0.5}, {opacity = 0.5})
s:view({x = 0.25, y = 0, width = 0.25, height = 0.5}, {optical = false})
local deep = s:view({x = 0, y = 0.5, width = 0.25, height = 0.5},
                    {camera = {view = "3d", eye = {0, -8, 6}, target = {0, 0, 0}, up = {0, 0, 1},
                               projection = "perspective"}, background = "#00000000"})
s:at(4.5, function(t) t:view({x = 0.75, y = 0, width = 0.25, height = 0.25}) end)
lens:look({height = 2.5}, 1)
strip:focus(box, {padding = 0, duration = 1})
deep:orbit(math.pi / 2, {axis = {0, 0, 1}, duration = 1})
lens:fade(0.5, 1)
strip:focus()
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "views.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "views.lua: %s\n", error);
    if (scene) {
        CHECK(scene->viewCount() == 5u && std::fabs(scene->duration() - 5.0f) < 1.0e-4f);
        SceneView view;
        // The lens keeps what it was made with and the camera and opacity of its last calls.
        CHECK(scene->viewAt(0u, view));
        CHECK(_near(view.viewport.x, 0.5f) && _near(view.viewport.width, 0.5f) && view.layer == 0 && view.optical);
        CHECK(view.background.b == 255u && view.background.r == 0u && view.background.a == 255u);
        CHECK(_near(view.camera.target, 2.0f, 1.0f, 0.0f) && _near(view.camera.orthoHeight, 2.5f));
        CHECK(_near(view.opacity, 0.5f) && view.hiddenCount == 1u && view.hidden
              && view.hidden[0] == scene->object("ghost") && !view.follow);
        // A view takes the scene's background and draws on top of every layer unless told.
        CHECK(scene->viewAt(1u, view));
        CHECK(view.background.a == 255u && view.background.r == 0u && view.background.b == 0u);
        CHECK(view.layer == INT32_MAX && view.optical && _near(view.opacity, 0.5f));
        CHECK(_near(view.camera.target, 0.0f, 0.0f, 0.0f) && _near(view.camera.orthoHeight, 10.0f));
        CHECK(scene->viewAt(2u, view) && !view.optical);
        CHECK(scene->viewAt(3u, view) && view.cameraView == CameraView::ThreeD && view.background.a == 0u);
        CHECK(std::fabs(std::fabs(view.camera.eye.x) - 8.0f) < 1.0e-3f && std::fabs(view.camera.eye.y) < 1.0e-3f
              && std::fabs(view.camera.eye.z - 6.0f) < 1.0e-3f);
        // An optical view scales pixel sizes by the world units a pixel spans for the scene's
        // camera over those for its own: 10/100 over 1.25/50 at first, 2.5/50 after the look.
        CHECK(std::fabs(_viewScale(renderer, scene, 0.0f, 0u) - 4.0f) < 1.0e-3f);
        CHECK(std::fabs(_viewScale(renderer, scene, 1.0f, 0u) - 2.0f) < 1.0e-3f);
        // focus frames the 4 by 1 box in the square strip, 4 units high, not 2 as the frame
        // would, and focus() takes the strip back to the scene's camera.
        CHECK(std::fabs(_viewScale(renderer, scene, 2.0f, 1u) - 1.25f) < 1.0e-3f);
        CHECK(std::fabs(_viewScale(renderer, scene, 5.0f, 1u) - 0.5f) < 1.0e-3f);
        CHECK(std::fabs(_viewScale(renderer, scene, 3.0f, 2u) - 1.0f) < 1.0e-3f);
        // A view made inside an at block shows from that time.
        CHECK(_viewScale(renderer, scene, 4.0f, 4u) == 0.0f && _viewScale(renderer, scene, 4.5f, 4u) > 0.0f);
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        if (surface.data()) {
            auto at = [&](uint32_t x, uint32_t y) { return surface.data()[y * surface.stride() + x]; };
            // The dot at the center of the lens, the hidden circle left out of it, the square on
            // a higher layer over it, and the dot where the scene's camera puts it.
            CHECK(at(150u, 75u) == _pixel(Color::hex("#ff0000")));
            CHECK(at(130u, 65u) == _pixel(Color::hex("#0000ff")));
            CHECK(at(175u, 85u) == _pixel(Color::hex("#00ff00")));
            CHECK(at(120u, 40u) == _pixel(Color::hex("#ff0000")));
            CHECK(at(115u, 37u) == _pixel(Color::hex("#ffff00")));
        }
    }
    delete scene;

    static constexpr auto rect = "{x = 0, y = 0, width = 0.5, height = 0.5}";
    char script[1024];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "local s = vis.scene {} local rect = %s %s return s", rect, body);
        _scriptError(script, expected);
    };
    fails("s:view({x = 0.8, y = 0, width = 0.5, height = 0.5})",
          "could not add view: the view must lie inside the unit square with a positive size");
    fails("s:view(rect, {zoom = 2})", "unknown option 'zoom'");
    fails("s:view(rect, {opacity = 2})", "opacity must be between 0 and 1");
    fails("s:view(rect, {layer = 1 << 40})", "layer is out of range");
    fails("local o = vis.scene {} s:view(rect, {hide = o:circle {}})", "handle belongs to another scene");
    fails("local list = {} for i = 1, 257 do list[i] = s:circle {} end s:view(rect, {hide = list})",
          "a view hides at most 256 objects");
    fails("for i = 1, 9 do s:view(rect) end", "a scene holds at most 8 views in a script, as each draws the scene again");
    fails("s:view(rect):orbit(1)", "could not orbit view camera: only a 3D camera orbits");
    fails("local v s:at(2, function() v = s:view(rect) end) v:look({height = 1}, 1)",
          "could not animate view camera: the view is made after this time");
    fails("local v = s:view(rect) s:parallel(function() v:fade(0, 2) end, function() s:wait(1) v:fade(1, 1) end)",
          "could not fade view: the view already fades until 2.00 s");
    fails("local v = s:view(rect, {camera = {view = '3d', eye = {0, -8, 6}}}) v:focus(s:circle {})",
          "focus requires a 2D camera; use look in 3D");
    fails("local child = vis.scene {} local v = child:view(rect) s:viewport(child, rect) v:look({height = 1})",
          "scene is no longer available");
}
