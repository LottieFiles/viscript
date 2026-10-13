#include "testLuaChecks.h"

void _boundGeometry(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 90, fps = 10}
local x = s:tracker(0)
local bow = s:curve {from = {-3, 0}, control1 = {-1, 0}, control2 = {1, 0}, to = {3, 0}, id = "bow"}
s:bind(bow, x, function(v) return {control2 = {1, v}} end)
local beam = s:line {from = {-3, -2}, to = {3, -2}, id = "beam"}
s:bind(beam, x, function(v) return {to = {3, -2 + v}} end)
local hand = s:vector {value = {1, 0}, origin = {-2, 1}, id = "hand"}
s:bind(hand, x, function(v) return {value = {0, v}} end)
local wave = s:plot {points = {{-3, 2}, {0, 2}, {3, 2}}, id = "wave"}
s:bind(wave, x, function(v) return {points = {{-3, 2}, {0, 2 + v}, {3, 2}}, opacity = 1 - v / 4} end)
s:track(x, 2, 1, "linear")
local wedge = s:polygon {points = {{0, 0}, {1, 0}, {0, 1}}, id = "wedge"}
s:play({target = wedge, points = {{0, 0}, {2, 0}, {0, 2}}}, 0.5)
local pointer = s:arrow {from = {0, 0}, to = {1, 0}, id = "pointer"}
s:set {target = pointer, to = {0, 1}}
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "geometry.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "geometry.lua: %s\n", error);
    if (scene) {
        auto bow = static_cast<Curve*>(scene->object("bow"));
        auto beam = static_cast<Line*>(scene->object("beam"));
        auto hand = static_cast<Vector*>(scene->object("hand"));
        auto wave = static_cast<Plot*>(scene->object("wave"));
        auto wedge = static_cast<Polygon*>(scene->object("wedge"));
        auto pointer = static_cast<Arrow*>(scene->object("pointer"));
        CHECK(bow && beam && hand && wave && wedge && pointer);
        if (bow && beam && hand && wave && wedge && pointer) {
            // The keys a state leaves out keep the geometry the shape has.
            CHECK(_near(bow->controls()[2].y, 2.0f) && _near(bow->controls()[1].x, -1.0f));
            CHECK(bow->count() == 33u && _near(bow->points()[16].y, 0.75f));
            CHECK(_near(beam->to.y, 0.0f) && _near(beam->from.y, -2.0f));
            CHECK(_near(hand->value.x, 0.0f) && _near(hand->value.y, 2.0f) && _near(hand->origin.x, -2.0f));
            CHECK(wave->count() == 3u && _near(wave->points()[1].y, 4.0f) && _near(wave->opacity, 0.5f));
            CHECK(_near(wedge->points()[1].x, 2.0f) && _near(wedge->points()[2].y, 2.0f));
            CHECK(_near(pointer->to.x, 0.0f) && _near(pointer->to.y, 1.0f));
            CHECK(std::fabs(scene->duration() - 1.5f) < 1.0e-4f);
            // The curve bends a frame at a time while the tracker runs.
            BBox flat;
            BBox half;
            BBox full;
            CHECK(renderer->bounds(scene, bow, 0.0f, flat) == Result::Success);
            CHECK(renderer->bounds(scene, bow, 0.5f, half) == Result::Success);
            CHECK(renderer->bounds(scene, bow, 1.0f, full) == Result::Success);
            CHECK(half.height > flat.height + 2.0f && full.height > half.height + 2.0f);
        }
    }
    delete scene;

    // A shape other than a plot keeps its point count, and the keys must suit it.
    _scriptError("local s = vis.scene {} local p = s:polygon {points = {{0, 0}, {1, 1}, {2, 0}}} local t = s:tracker(0)\n"
                 "s:bind(p, t, function(v) return {points = {{0, v}, {1, 1}}} end) return s",
                 "diagnostic.lua:2: points must have 3 points, as the polygon has, not 2; a shape keeps its point count");
    _scriptError("local s = vis.scene {}\nlocal p = s:polygon {points = {{0, 0}, {1, 1}, {2, 0}}}\nlocal t = s:tracker(0)\n"
                 "s:bind(p, t, function(v) local q = {{0, v}, {1, 1}, {2, 0}} if v > 0.5 then q[4] = {3, 0} end "
                 "return {points = q} end)\ns:track(t, 1, 1)\nreturn s",
                 "diagnostic.lua:5: points must have 3 points, as the polygon has, not 4");
    _scriptError("local s = vis.scene {} local c = s:circle {radius = 1} local t = s:tracker(0) "
                 "s:bind(c, t, function(v) return {from = {v, 0}} end) return s",
                 "from needs a line, arrow, curve or angle; move other objects with shift");
    _scriptError("local s = vis.scene {} local l = s:line {from = {0, 0}, to = {1, 0}} "
                 "s:play({target = l, origin = {1, 1}}, 1) return s", "origin needs a vector");
    _scriptError("local s = vis.scene {} local l = s:line {from = {0, 0}, to = {1, 0}} "
                 "s:set {target = l, control1 = {1, 1}} return s", "control1 needs a curve");
    _scriptError("local s = vis.scene {} local l = s:line {from = {0, 0}, to = {1, 0}} "
                 "s:play({target = l, points = {{1, 1}, {2, 2}}}, 1) return s",
                 "points needs a polygon, plot, route or path of one contour");
    _scriptError("local s = vis.scene {} local p = s:path {commands = {{type = 'move', to = {0, 0}}, "
                 "{type = 'line', to = {1, 0}}, {type = 'line', to = {0, 1}}, {type = 'close'}, "
                 "{type = 'move', to = {2, 0}}, {type = 'line', to = {3, 0}}, {type = 'line', to = {2, 1}}, "
                 "{type = 'close'}}} s:play({target = p, points = {}}, 1) return s",
                 "points cannot reshape a path of several contours");
    _scriptError("local s = vis.scene {} local l = s:line {from = {0, 0}, to = {1, 0}} "
                 "s:play({target = l, to = {0 / 0, 1}}, 1) return s", "vector component must be a finite number");

    // A copy keeps the points a bound shape shows as it is made, and a warp maps the points a
    // track ends at.
    static constexpr auto warped = R"lua(
local s = vis.scene {width = 160, height = 90, fps = 10}
local x = s:tracker(0)
local beam = s:line {from = {-3, -2}, to = {3, -2}, id = "beam"}
s:bind(beam, x, function(v) return {to = {3, -2 + v}} end)
s:track(x, 2, 1, "linear")
s:at(0.5, function() s:clone(beam, {id = "copy"}) end)
s:warp(beam, function(px, py) return px, py + 1 end, 1, "linear")
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(warped, static_cast<uint32_t>(std::strlen(warped)), "warped.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "warped.lua: %s\n", error);
    if (scene) {
        auto beam = static_cast<Line*>(scene->object("beam"));
        auto copy = static_cast<Line*>(scene->object("copy"));
        CHECK(beam && copy);
        Bounds box;
        if (beam && copy) {
            CHECK(_near(copy->from.y, -2.0f) && _near(copy->to.y, -1.0f));
            CHECK(_near(beam->to.y, 0.0f) && beam->bounds(box) && _near(box.min.y, -1.0f) && _near(box.max.y, 1.0f));
        }
    }
    delete scene;
    // A warp waits for a track that runs, and a warped shape, bound or not, takes no new points.
    _scriptError("local s = vis.scene {}\nlocal x = s:tracker(0)\nlocal beam = s:line {from = {-3, -2}, to = {3, -2}, id = 'beam'}\n"
                 "s:bind(beam, x, function(v) return {to = {3, -2 + v}} end)\ns:track(x, 2, 1, 'linear')\ns:seek(0.5)\n"
                 "s:warp(beam, function(px, py) return px, py + 1 end)\nreturn s",
                 "'beam' already animates its geometry until 1.00 s");
    _scriptError("local s = vis.scene {}\nlocal x = s:tracker(0)\nlocal beam = s:line {from = {-3, -2}, to = {3, -2}}\n"
                 "s:bind(beam, x, function(v) return {to = {3, -2 + v}} end)\n"
                 "s:warp(beam, function(px, py) return px, py + 1 end)\ns:track(x, 2, 1)\nreturn s",
                 "diagnostic.lua:6: could not play animation: a warped shape keeps the outline its warp gave");
    _scriptError("local s = vis.scene {} local l = s:line {from = {0, 0}, to = {1, 0}} "
                 "s:warp(l, function(px, py) return px, py + 1 end) s:set {target = l, to = {1, 1}} return s",
                 "a warped shape keeps the outline its warp gave, so the points that define it no longer change");
    // A scene call that fails while a track bakes names the author's track call.
    _scriptError("local s = vis.scene {}\nlocal c = s:circle {radius = 1}\nlocal t = s:tracker(1)\n"
                 "s:bind(c, t, function(v) return {opacity = v} end)\ns:fade(c, 0.5, 5)\ns:seek(0)\n"
                 "s:track(t, 0, 1)\nreturn s",
                 "diagnostic.lua:7: could not play animation: the object already animates its opacity until 5.00 s");
}

// Tracks of different trackers may overlap on one binding: it shows the values of both at
// every sample, whether the tracks come from the branches of a parallel or from an at.
void _overlappingTracks(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 90, fps = 10}
local x = s:tracker(0)
local z = s:tracker(1)
local n = s:number {value = 0, decimals = 2, id = "n"}
s:bind(n, {x, z}, function(xv, zv) return {value = 100 * xv + zv} end)
local dot = s:circle {radius = 0.2, id = "dot"}
s:bind(dot, {x, z}, function(xv, zv) return {shift = {xv, 0}, radius = 0.2 * zv} end)
local other = s:circle {radius = 0.1, id = "other"}
s:bind(other, x, function(xv) return {opacity = 1 - xv / 8} end)
s:parallel(function() s:track(x, 4, 4, "linear") end, function() s:wait(1) s:track(z, 3, 1, "linear") end)
if math.abs(s:time() - 4) > 1e-4 or math.abs(s:duration() - 4) > 1e-4 then error("parallel timeline") end
s:fade(other, 0.2, 1)
if math.abs(s:time() - 5) > 1e-4 then error("a flush keeps the authoring time") end
local w = s:tracker(0)
local m = s:number {value = 0, decimals = 2, id = "m"}
s:bind(m, {x, w}, function(xv, wv) return {value = 10 * xv + wv} end)
s:track(x, 0, 2, "linear")
s:at(5.5, function() s:track(w, 1, 1, "linear") end)
if math.abs(s:time() - 7) > 1e-4 then error("at keeps the authoring time") end
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "overlap.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "overlap.lua: %s\n", error);
    if (scene) {
        auto n = static_cast<Text*>(scene->object("n"));
        auto m = static_cast<Text*>(scene->object("m"));
        auto dot = static_cast<Circle*>(scene->object("dot"));
        CHECK(n && m && dot);
        CHECK(std::fabs(scene->duration() - 7.0f) < 1.0e-4f);
        auto shows = [scene](const Text* text, float time, const char* expected) {
            char written[32] = {};
            scene->text(text, time, written, sizeof(written));
            if (std::strcmp(written, expected) != 0) {
                std::fprintf(stderr, "  at %.2f s expected %s, got %s\n", time, expected, written);
            }
            return std::strcmp(written, expected) == 0;
        };
        if (n && m && dot) {
            // Before, during and after the overlap, the number follows both trackers.
            CHECK(shows(n, 0.5f, "51.00"));
            CHECK(shows(n, 1.5f, "152.00"));
            CHECK(shows(n, 2.5f, "253.00"));
            CHECK(shows(n, 4.0f, "403.00"));
            // A track an at overlays on a track before it merges in the same way.
            CHECK(shows(m, 5.5f, "30.00"));
            CHECK(shows(m, 6.0f, "20.50"));
            CHECK(shows(m, 6.5f, "11.00"));
            CHECK(shows(m, 7.0f, "1.00"));
            CHECK(std::fabs(dot->radius - 0.6f) < 1.0e-4f);
            // The shift keeps its pace through the overlap while the radius grows.
            BBox early;
            BBox middle;
            BBox late;
            CHECK(renderer->bounds(scene, dot, 0.5f, early) == Result::Success);
            CHECK(renderer->bounds(scene, dot, 1.5f, middle) == Result::Success);
            CHECK(renderer->bounds(scene, dot, 2.5f, late) == Result::Success);
            auto center = [](const BBox& box) { return box.x + box.width * 0.5f; };
            CHECK(std::fabs((center(middle) - center(early)) - (center(late) - center(middle))) < 0.05f);
            CHECK(middle.width > early.width + 1.0f && late.width > middle.width + 1.0f);
        }
    }
    delete scene;

    // A tracker has one value at a time, and a track cannot merge with one that has played.
    _scriptError("local s = vis.scene {}\nlocal x = s:tracker(0)\n"
                 "s:bind(s:circle {radius = 1}, x, function(v) return {opacity = v} end)\n"
                 "s:parallel(function() s:track(x, 1, 2) end,\nfunction() s:wait(1) s:track(x, 0, 2) end)\nreturn s",
                 "diagnostic.lua:5: this track and the track at diagnostic.lua:4 move the same tracker at once, "
                 "until 2.00 s; a tracker has one value at a time");
    _scriptError("local s = vis.scene {}\nlocal x = s:tracker(0)\nlocal z = s:tracker(0)\n"
                 "s:bind(s:circle {radius = 1}, {x, z}, function(a, b) return {opacity = a, radius = 1 + b} end)\n"
                 "s:track(x, 1, 2)\ns:fade(s:circle {radius = 0.5}, 0.5, 1)\ns:seek(1)\ns:track(z, 1, 1)\nreturn s",
                 "diagnostic.lua:8: this track overlaps the track at diagnostic.lua:5, whose segments have already "
                 "played on the same object");
    // A merged sample that fails names the track that made the overlap.
    _scriptError("local s = vis.scene {}\nlocal x = s:tracker(0)\nlocal z = s:tracker(0)\n"
                 "s:bind(s:circle {radius = 1}, {x, z}, function(a, b) if b > 0.5 then return 1 end "
                 "return {opacity = a} end)\ns:parallel(function() s:track(x, 1, 2) end,\n"
                 "function() s:wait(1) s:track(z, 1, 1) end)\nreturn s",
                 "diagnostic.lua:6: a bound function must return a state table");
}

// The calls that read what the clips make, a clone, a view's camera aimed at an object and a
// warp among them, play the tracks that wait first, so they find the points and the places
// the tracks give at their time, merged ones too.
void _pendingReads()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 90, fps = 10, camera = {height = 6}}
local x = s:tracker(0)
local z = s:tracker(0)
local bow = s:curve {from = {-3, 0}, control1 = {-1, 0}, control2 = {1, 0}, to = {3, 0}, id = "bow"}
s:bind(bow, {x, z}, function(a, b) return {control1 = {-1, a}, control2 = {1, b}} end)
local dot = s:circle {radius = 0.25, id = "dot"}
s:bind(dot, x, function(a) return {shift = {a, -1}} end)
local beam = s:line {from = {-2, 2}, to = {2, 2}, id = "beam"}
s:bind(beam, z, function(b) return {to = {2, 2 + b}} end)
local lens = s:view({x = 0.5, y = 0.5, width = 0.5, height = 0.5}, {camera = {height = 2}})
s:parallel(function() s:track(x, 2, 2, "linear") end, function() s:wait(1) s:track(z, 1, 2, "linear") end)
s:at(1.5, function() s:clone(bow, {id = "copy"}) end)
s:track(x, 3, 1, "linear")
lens:look({target = dot}, 0.5)
s:track(x, 4, 1, "linear")
s:view({x = 0, y = 0, width = 0.5, height = 0.5}, {camera = {target = dot, height = 2}})
s:track(z, 0, 1, "linear")
s:warp(beam, function(px, py) return px, py + 1 end, 1, "linear")
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "pending.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "pending.lua: %s\n", error);
    if (!scene) return;
    CHECK(std::fabs(scene->duration() - 7.5f) < 1.0e-4f);
    // The copy takes the curve as both overlapping tracks bend it at 1.5 s.
    auto copy = static_cast<Curve*>(scene->object("copy"));
    CHECK(copy && copy->type() == Type::Curve);
    if (copy && copy->type() == Type::Curve) {
        auto controls = copy->controls();
        CHECK(_near(controls[1].x, -1.0f) && _near(controls[1].y, 1.5f));
        CHECK(_near(controls[2].x, 1.0f) && _near(controls[2].y, 0.25f));
    }
    // The lens aims at the dot where the track before the look leaves it, and the view made
    // after the next track at where that one leaves it.
    SceneView view;
    CHECK(scene->viewAt(0u, view) && _near(view.camera.target.x, 3.0f) && _near(view.camera.target.y, -1.0f));
    CHECK(scene->viewAt(1u, view) && _near(view.camera.target.x, 4.0f) && _near(view.camera.target.y, -1.0f));
    // The warp lifts the beam the last track lays flat.
    auto beam = static_cast<Line*>(scene->object("beam"));
    Bounds box;
    CHECK(beam && _near(beam->to.y, 2.0f) && beam->bounds(box) && _near(box.min.y, 3.0f) && _near(box.max.y, 3.0f));
    delete scene;
}

void _longTracks(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 90, fps = %u}
local t = s:tracker(0)
local n = s:number {value = 0, decimals = 0, id = "n"}
local calls = 0
s:bind(n, t, function(v)
    calls = calls + 1
    return {value = math.floor(v * %u + 0.5)}
end)
if %u == 30 then
    local line = s:line {from = {0, 0}, to = {1, 0}, id = "line"}
    s:bind(line, t, function(v) return {to = {1, v * v}} end)
end
s:track(t, 1, %u, "linear")
if calls ~= %u + 1 then error("track must sample every frame") end
if t.get() ~= 1 then error("tracker endpoint") end
return s
)lua";
    uint32_t rates[] = {30u, 60u};
    for (auto fps : rates) {
        auto duration = fps == 30u ? 20u : 60u;
        auto frames = fps * duration;
        char script[2048], error[512] = {};
        std::snprintf(script, sizeof(script), source, fps, frames, fps, duration, frames);
        Scene* scene = nullptr;
        CHECK(Lua::load(script, static_cast<uint32_t>(std::strlen(script)), "long-track.lua", &scene,
                        error, sizeof(error)) == Result::Success);
        if (!scene) std::fprintf(stderr, "long-track.lua: %s\n", error);
        if (scene) {
            CHECK(std::fabs(scene->duration() - duration) < 1.0e-3f);
            CHECK(scene->clipCount() == frames * (fps == 30u ? 2u : 1u));
            BBox initial;
            if (fps == 30u) CHECK(renderer->bounds(scene, scene->object("line"), 0.0f, initial) == Result::Success);
            // Late adjacent frames and a backwards seek read their own baked values.
            uint32_t samples[] = {frames - 3u, frames - 2u, frames - 1u, frames - 3u};
            for (auto frame : samples) {
                auto time = static_cast<float>(frame) / fps;
                char expected[32];
                std::snprintf(expected, sizeof(expected), "%u", frame);
                CHECK(std::strcmp(_shows(scene, "n", time), expected) == 0);
                if (fps == 30u) {
                    BBox bounds;
                    CHECK(renderer->bounds(scene, scene->object("line"), time, bounds) == Result::Success);
                    auto v = static_cast<double>(frame) / frames;
                    CHECK(std::fabs(bounds.height - initial.height - v * v * 90.0 / 8.0) < 1.0e-4);
                }
            }
        }
        delete scene;
    }
    // Reused point tables keep temporary Lua storage small; saved states still use native memory.
    _scriptError("local s=vis.scene{fps=30}; local pts={}; for i=1,12000 do pts[i]={i/12000,0} end; "
                 "local p=s:polygon{points=pts}; local t=s:tracker(0); "
                 "s:bind(p,t,function(v) return {points=pts,opacity=v} end); s:track(t,1,20); return s",
                 "scene native memory limit exceeded: retained states share the 64 MiB budget");
    _scriptError("local s=vis.scene{fps=30}; local vals={}; for i=1,2500 do vals[i]=0 end; "
                 "local g=s:contour{values=vals,levels={1},x_range={0,49,1},y_range={0,49,1}}; "
                 "local t=s:tracker(0); s:bind(g:children()[1],t,function(v) return {values=vals} end); "
                 "s:track(t,1,20); return s",
                 "scene point limit exceeded: contour grids share the 1000000-point budget");
}

void _springResponses(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local function close(actual, expected)
  if math.abs(actual - expected) > 1e-10 * (1 + math.abs(expected)) then error("physical spring sample") end
end
for _, time in ipairs({0.01, 0.25, 1, 4}) do
  close(vis.spring_response(time, {mass=2, stiffness=8, damping=0, displacement=3, velocity=5}),
    3 * math.cos(2 * time) + 2.5 * math.sin(2 * time))
  close(vis.spring_response(time, {mass=1, stiffness=16, damping=10, displacement=3, velocity=5}),
    (29 * math.exp(-2 * time) - 11 * math.exp(-8 * time)) / 6)
  for _, damping in ipairs({8 - 1e-10, 8, 8 + 1e-10}) do
    close(vis.spring_response(time, {mass=2, stiffness=8, damping=damping, displacement=3, velocity=5}),
      (3 + 11 * time) * math.exp(-2 * time))
  end
end
close(vis.spring_response(1e300, {mass=1, stiffness=1, damping=1e300, displacement=1}), math.exp(-1))
local residual = math.exp(math.log(1e308) + math.log(751) - 750)
if math.abs(vis.spring_response(750, {mass=1, stiffness=1, damping=2, displacement=1e308}) / residual - 1) > 1e-12 then
  error("spring finite decay residual")
end
local underdamped = math.exp(math.log(1e308) - 750) * math.cos(750)
if math.abs(vis.spring_response(750, {mass=1, stiffness=2, damping=2, displacement=1e308, velocity=-1e308}) / underdamped - 1) > 1e-12 then
  error("spring underdamped finite decay residual")
end
local overdamped = math.exp(math.log(1e308) + math.log(4/3) - 750)
if math.abs(vis.spring_response(750, {mass=1, stiffness=4, damping=5, displacement=1e308}) / overdamped - 1) > 1e-12 then
  error("spring overdamped finite decay residual")
end
local tiny = math.exp(math.log(1e308) + math.log(1431) - 1430)
if tiny <= 0 or math.abs(vis.spring_response(1430, {mass=1, stiffness=1, damping=2, displacement=1e308}) / tiny - 1) > 1e-12 then
  error("spring subnormal decay residual")
end
for _, sign in ipairs({-1, 1}) do
  local options = {mass=1, stiffness=1e-308, damping=0, displacement=-sign*1e308, velocity=sign*1e308}
  if math.abs(vis.spring_response(2, options) / 1e308 - sign) > 1e-12 then error("spring finite cancellation") end
  if vis.spring_response(1, options) ~= 0 then error("spring exact cancellation") end
end
if vis.spring_response(0, {mass=1, stiffness=1, damping=1, displacement=-3.25, velocity=5}) ~= -3.25 then
  error("spring initial state")
end
local s = vis.scene {width=960, height=540, fps=30, camera={view="2d", height=540}}
local seconds = s:tracker(0)
local omega = 2 * math.pi
for i, zeta in ipairs({0.15, 1}) do
  local mass = s:rectangle {id="mass_" .. i, size={50,50}, width=0, fill="#409cff"}
  s:bind(mass, seconds, function(time)
    return {shift={vis.spring_response(time, {mass=1, stiffness=omega*omega,
      damping=2*zeta*omega, displacement=200}), i == 1 and 120 or -120}}
  end)
end
s:wait(0.5)
s:track(seconds, 7.5, 7.5, "linear")
s:seek(8)
return s
)lua";
    Scene* scene = nullptr;
    char error[512] = {};
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "spring.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "spring.lua: %s\n", error);
    if (scene) {
        CHECK(scene->duration() == 8.0f);
        CHECK(scene->clipCount() == 450u);
        auto omega = 2.0 * 3.14159265358979323846;
        auto damped = omega * std::sqrt(1.0 - 0.15 * 0.15);
        // Reverse seeks read baked frames and retain the physical endpoint.
        for (auto frame = 240; frame >= 0; frame--) {
            auto time = std::fmax(0.0, frame / 30.0 - 0.5);
            double expected[] = {
                200.0 * std::exp(-0.15 * omega * time)
                    * (std::cos(damped * time) + 0.15 * omega / damped * std::sin(damped * time)),
                200.0 * (1.0 + omega * time) * std::exp(-omega * time)};
            for (auto i = 0u; i < 2u; i++) {
                BBox bounds;
                CHECK(renderer->bounds(scene, scene->object(i == 0u ? "mass_1" : "mass_2"),
                                       static_cast<float>(frame) / 30.0f, bounds) == Result::Success);
                CHECK(std::fabs(bounds.center().x - 480.0 - expected[i]) < 0.002);
            }
        }
    }
    delete scene;
    for (auto key : {"mass", "stiffness", "damping", "displacement", "velocity"}) {
        for (auto value : {"1/0", "0/0", "'1'", "true"}) {
            char invalid[512];
            char expected[128];
            std::snprintf(invalid, sizeof(invalid), "local p={mass=1,stiffness=1,damping=1,displacement=1}; "
                          "p.%s=%s; vis.spring_response(0,p); return vis.scene{}", key, value);
            std::snprintf(expected, sizeof(expected), "%s must be a finite number", key);
            _scriptError(invalid, expected);
        }
    }
    for (auto key : {"mass", "stiffness", "damping", "displacement"}) {
        char invalid[512];
        char expected[128];
        std::snprintf(invalid, sizeof(invalid), "local p={mass=1,stiffness=1,damping=1,displacement=1}; "
                      "p.%s=nil; vis.spring_response(0,p); return vis.scene{}", key);
        std::snprintf(expected, sizeof(expected), "%s must be a finite number", key);
        _scriptError(invalid, expected);
    }
    _scriptError("vis.spring_response(-1,{mass=1,stiffness=1,damping=1,displacement=1})",
                 "seconds must be a finite non-negative number");
    _scriptError("vis.spring_response(0,{mass=0,stiffness=1,damping=1,displacement=1})",
                 "mass and stiffness must be positive and damping non-negative");
    _scriptError("vis.spring_response(0,{mass=1,stiffness=1,damping=1,displacement=1,duration=8})",
                 "unknown option 'duration'");
    _scriptError("vis.spring_response(0,{mass=1e-300,stiffness=1e300,damping=1,displacement=1})",
                 "spring response is outside the finite numeric range");
    _scriptError("vis.spring_response(0,{mass=1e300,stiffness=1e-300,damping=1,displacement=1})",
                 "spring response is outside the finite numeric range");
    _scriptError("vis.spring_response(1e300,{mass=1,stiffness=1e300,damping=0,displacement=1})",
                 "spring response is outside the finite numeric range");
    _scriptError("vis.spring_response(2,{mass=1,stiffness=1e-308,damping=0,displacement=1e308,velocity=1e308})",
                 "spring response is outside the finite numeric range");
}

void _trackerPrecision()
{
    static constexpr auto source = R"lua(
local s = vis.scene {fps = 30}
local count = s:tracker(0)
local n = s:number {value = 0, id = "n"}
local step = 0
s:bind(n, count, function(v)
    if v ~= 2000 * (step / 240) then error("linear sample lost precision") end
    step = step + 1
    return {value = math.floor(v)}
end)
s:wait(1)
s:track(count, 2000, 8, "linear")
s:wait(1)
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "tracker-precision.lua",
                    &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "tracker-precision.lua: %s\n", error);
    if (scene) {
        for (auto frame = 0u; frame <= 300u; frame++) {
            auto expected = frame <= 30u ? 0u : frame >= 270u ? 2000u : (frame - 30u) * 2000u / 240u;
            char value[32];
            std::snprintf(value, sizeof(value), "%u", expected);
            CHECK(std::strcmp(_shows(scene, "n", static_cast<float>(frame) / 30u), value) == 0);
        }
        CHECK(std::strcmp(_shows(scene, "n", 2.8f), "450") == 0);
        CHECK(std::strcmp(_shows(scene, "n", 8.6f), "1900") == 0);
        CHECK(std::strcmp(_shows(scene, "n", 2.8f), "450") == 0);
    }
    delete scene;

    for (const auto* curve : {"{preset='linear',reverse=true}", "{preset='smooth',strength=0}",
                              "{bezier={0.2,0.3,0.8,0.9},strength=0,reverse=true}"}) {
        char script[1024];
        std::snprintf(script, sizeof(script), R"lua(
local s = vis.scene {fps = 30}
local t = s:tracker(0)
local step = 0
s:bind(s:circle {radius=1}, t, function(v)
    if v ~= 2000 * (step / 240) then error("linear curve changed progress") end
    step = step + 1
    return {opacity=0.5}
end)
s:track(t, 2000, 8, %s)
return s
)lua", curve);
        scene = nullptr;
        CHECK(Lua::load(script, static_cast<uint32_t>(std::strlen(script)), "linear-curve.lua",
                        &scene, error, sizeof(error)) == Result::Success);
        if (!scene) std::fprintf(stderr, "linear-curve.lua: %s\n", error);
        delete scene;
    }
}

void _trackerTiming()
{
    for (auto fps : {24u, 30u, 60u}) {
        char source[1024], error[512] = {};
        std::snprintf(source, sizeof(source), R"lua(
local s = vis.scene {fps = %u}
local seconds = s:tracker(0)
local n = s:number {value = 0, decimals = 6, id = "n"}
s:bind(n, seconds, function(v) return {value = v} end)
s:wait(0.5)
s:track(seconds, 7.5, 7.5, "linear")
s:mark("end")
return s
)lua", fps);
        Scene* scene = nullptr;
        CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "tracker-timing.lua",
                        &scene, error, sizeof(error)) == Result::Success);
        if (!scene) std::fprintf(stderr, "tracker-timing.lua: %s\n", error);
        if (scene) {
            CHECK(scene->duration() == 8.0f);
            CHECK(scene->cursor() == 8.0f);
            float end = 0.0f;
            CHECK(scene->marker("end", end) && end == 8.0f);
            CHECK(std::strcmp(_shows(scene, "n", 8.0f), "7.500000") == 0);
        }
        delete scene;
    }
}

void _trackers()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 90, fps = 10}
local dot = s:circle {center = {0, 0}, radius = 0.2, id = "dot"}
local line = s:plot {fn = function(x) return x end, x_range = {0, 2, 0.1}, id = "line"}
local x = s:tracker(0)
s:bind(dot, x, function(v) return {shift = {v, 0}, opacity = 1 - v / 4} end)
s:bind(line, x, function(v) return {progress = v / 2} end)
s:track(x, 2, 1, "linear")
if math.abs(x.get() - 2) > 1e-6 then error("tracker value") end
s:track({{x, 1}}, 0.5)
local idle = s:tracker(0)
s:track(idle, 1, 0.25)
s:focus(dot, {padding = 0.5, duration = 0.25})
s:focus({dot, line})
s:focus(nil, {duration = 0.5})
if math.abs(s:duration() - 3.5) > 1e-4 then error("tracker duration") end
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "tracker.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "tracker.lua: %s\n", error);
    else CHECK(std::fabs(scene->duration() - 3.5f) < 1.0e-4f);
    delete scene;

    _error("local s = vis.scene {} local c = s:circle {radius = 1} s:fade(c, 0.5, 2) s:seek(1) "
           "s:bind(c, s:tracker(0), function(v) return {opacity = v} end) return s", "bind-inside-play.lua");
    _error("local s = vis.scene {} local c = s:circle {radius = 1} local t = s:tracker(0) "
           "s:bind(c, t, function(v) return {spin = v} end) return s", "bind-unknown-key.lua");

    // A bound state applies every play property, also once the timeline has started.
    static constexpr auto late = R"lua(
local s = vis.scene {width = 160, height = 90, fps = 10}
local dot = s:circle {radius = 0.2, id = "dot"}
local dashed = s:line {from = {-1, 0}, to = {1, 0}, dash = {0.2, 0.2}, id = "dashed"}
local box = s:rectangle {size = {1, 1}, id = "box"}
local arrow = s:arrow {from = {-1, 1}, to = {1, 1}, id = "arrow"}
local x = s:tracker(0)
s:bind(dashed, x, function(v) return {dash_offset = 0.25 + v, width = 2 + v} end)
s:bind(arrow, s:tracker(0), function(v) return {tip = 9 + v, tail = 3} end)
s:fade(dot, 0.5, 1)
s:bind(dot, x, function(v) return {radius = 0.5 + v} end)
s:bind(box, x, function(v) return {size = {1 + v, 1}, corner = 0.1} end)
s:track(x, 1, 1, "linear")
return s
)lua";
    char lateError[512] = {};
    Scene* lateScene = nullptr;
    CHECK(Lua::load(late, static_cast<uint32_t>(std::strlen(late)), "tracker-late.lua", &lateScene, lateError,
                    sizeof(lateError)) == Result::Success);
    if (!lateScene) std::fprintf(stderr, "tracker-late.lua: %s\n", lateError);
    if (lateScene) {
        auto dashed = lateScene->object("dashed");
        auto dot = static_cast<Circle*>(lateScene->object("dot"));
        auto box = static_cast<Rectangle*>(lateScene->object("box"));
        CHECK(dashed && std::fabs(dashed->style.dashOffset - 1.25f) < 1.0e-4f && std::fabs(dashed->style.width - 3.0f) < 1.0e-4f);
        CHECK(dot && std::fabs(dot->radius - 1.5f) < 1.0e-4f);
        CHECK(box && std::fabs(box->size.x - 2.0f) < 1.0e-4f && box->corner == 0.1f);
        CHECK(std::fabs(lateScene->duration() - 2.0f) < 1.0e-4f);
        // A binding whose tracker never moves keeps its first state.
        auto arrow = static_cast<Arrow*>(lateScene->object("arrow"));
        CHECK(arrow && arrow->tip == 9.0f && arrow->tail == 3.0f);
    }
    delete lateScene;
    _scriptError("local s = vis.scene {} local c = s:circle {radius = 1} local t = s:tracker(0) "
                 "s:bind(c, t, function(v) return {size = v + 1} end) return s",
                 "could not apply the bound state: size needs a rectangle");
}

// track and bind are written in Lua, in a chunk of their own, yet their failures name the line
// of the script that called them, as the failures of the scene methods written in C do.
void _trackerLines()
{
    // A curve the scene does not know, with a binding to sample and without one.
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\n"
                 "s:bind(s:circle {radius = 1}, t, function(v) return {opacity = v} end)\n"
                 "s:track(t, 1, 1, \"nope\")\nreturn s",
                 "diagnostic.lua:4: unknown animation curve preset");
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\ns:track(t, 1, 1, \"nope\")\nreturn s",
                 "diagnostic.lua:3: unknown animation curve preset");
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\ns:track(t, 1, 1, true)\nreturn s",
                 "diagnostic.lua:3: animation curve must be a preset name or a table");
    // An entry of the array form that is no {tracker, value} pair.
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\ns:track({1, 2}, 1)\nreturn s",
                 "diagnostic.lua:3: track requires a tracker or an array of {tracker, value}");
    // A shift that holds no numbers, from the first state bind applies and from a sample of a
    // track.
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\n"
                 "s:bind(s:circle {radius = 1}, t, function(v) return {shift = {\"a\", 0}} end)\nreturn s",
                 "diagnostic.lua:3: a bound shift must be a table of numbers");
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\n"
                 "s:bind(s:circle {radius = 1}, t, function(v) return {shift = {v > 0.5 and \"a\" or v, 0}} end)\n"
                 "s:track(t, 1, 1)\nreturn s",
                 "diagnostic.lua:4: a bound shift must be a table of numbers");
    // A scene given to a viewport takes no more tracks.
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\nlocal p = vis.scene {}\n"
                 "p:viewport(s, {x = 0, y = 0, width = 1, height = 1})\ns:track(t, 1, 1)\nreturn p",
                 "diagnostic.lua:5: scene is no longer available");
    // Segments that fail as they play name the track that made them, whether a later call or
    // the end of the script plays them.
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\nlocal c = s:circle {radius = 1}\n"
                 "s:bind(c, t, function(v) return {opacity = v} end)\ns:remove(c)\ns:track(t, 1, 1)\n"
                 "s:wait(1)\ns:fade(s:circle {}, 0.5, 1)\nreturn s",
                 "diagnostic.lua:6: could not play animation: the object was removed earlier in the timeline");
    _scriptError("local s = vis.scene {}\nlocal t = s:tracker(0)\nlocal c = s:circle {radius = 1}\n"
                 "s:bind(c, t, function(v) return {opacity = v} end)\ns:remove(c)\ns:track(t, 1, 1)\nreturn s",
                 "diagnostic.lua:6: could not play animation: the object was removed earlier in the timeline");
}

void _timeline()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 90}
local a = s:circle {radius = 0.5, id = "a"}
local b = s:rectangle {size = {1, 1}, id = "b"}
s:parallel(
    function() s:shift(a, {2, 0}, 3, "linear") end,
    function()
        s:fade_in(b, {duration = 1})
        s:mark("b-in")
        s:indicate(b)
    end)
if math.abs(s:time() - 3) > 1e-6 then error("parallel continues after the longest track") end
if math.abs(s:time("b-in") - 1) > 1e-6 then error("marker time") end
s:at("b-in", function() s:fill(a, "#ff0000", 0.5) end)
if math.abs(s:time() - 3) > 1e-6 then error("at returns to the current time") end
s:at(4, function() s:fade(a, 0.5, 1) end)
if math.abs(s:duration() - 5) > 1e-6 then error("duration keeps the latest time") end
s:parallel({
    function() s:look({height = 4}, 1) end,
    function() s:shift(b, {0, 1}, 2) end,
})
s:mark("end")
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "timeline.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) {
        std::fprintf(stderr, "timeline.lua: %s\n", error);
        return;
    }
    CHECK(std::fabs(scene->duration() - 5.0f) < 1.0e-4f);
    CHECK(std::fabs(scene->cursor() - 5.0f) < 1.0e-4f);
    CHECK(scene->markerCount() == 2u);
    auto time = 0.0f;
    CHECK(scene->marker("end", time) && std::fabs(time - 5.0f) < 1.0e-4f);
    delete scene;

    _error("local s = vis.scene {} local a = s:circle {radius = 1} "
           "s:parallel(function() s:fade(a, 0.5, 2) end, function() s:fade(a, 1, 1) end) return s",
           "parallel-same-property.lua");
    // Camera moves in parallel tracks add up, view and lens changes included.
    static constexpr auto zoom = R"lua(
local s = vis.scene {}
s:parallel(
    function() s:look({target = {4, 0}}, 2) end,
    function()
        s:wait(0.5)
        s:look({height = 4}, 3)
    end)
return s
)lua";
    Scene* zooming = nullptr;
    CHECK(Lua::load(zoom, static_cast<uint32_t>(std::strlen(zoom)), "zoom.lua", &zooming, error, sizeof(error))
          == Result::Success);
    if (!zooming) std::fprintf(stderr, "zoom.lua: %s\n", error);
    if (zooming) {
        auto& camera = zooming->config().camera;
        CHECK(std::fabs(zooming->duration() - 3.5f) < 1.0e-4f);
        CHECK(std::fabs(camera.target.x - 4.0f) < 1.0e-4f && std::fabs(camera.eye.x - 4.0f) < 1.0e-4f);
        CHECK(std::fabs(camera.orthoHeight - 4.0f) < 1.0e-4f);
    }
    delete zooming;
    // A 2D pan stands the eye straight above its new target, after a look as after a focus, also
    // where adding the shift to the eye rounds away from the target.
    static constexpr auto pan = R"lua(
local s = vis.scene {}
local dot = s:circle {center = {-1.4, -1.1}, radius = 0.5}
s:look({target = {-5, -5}}, 1)
s:look({target = {-1.9, -1.6}}, 1)
s:look({target = {-5, -5}}, 1)
s:focus(dot)
return s
)lua";
    Scene* panning = nullptr;
    CHECK(Lua::load(pan, static_cast<uint32_t>(std::strlen(pan)), "pan.lua", &panning, error, sizeof(error))
          == Result::Success);
    if (!panning) std::fprintf(stderr, "pan.lua: %s\n", error);
    if (panning) {
        auto& camera = panning->config().camera;
        CHECK(camera.eye.x == camera.target.x && camera.eye.y == camera.target.y);
        CHECK(std::fabs(camera.target.x + 1.4f) < 1.0e-5f && std::fabs(camera.target.y + 1.1f) < 1.0e-5f);
    }
    delete panning;
    static constexpr auto lens = R"lua(
local s = vis.scene {}
s:parallel(
    function() s:look({height = 4}, 2) end,
    function() s:look({view = '3d', fov = 0.5, far = 50}, 1) end)
return s
)lua";
    Scene* lensed = nullptr;
    CHECK(Lua::load(lens, static_cast<uint32_t>(std::strlen(lens)), "lens.lua", &lensed, error, sizeof(error))
          == Result::Success);
    if (!lensed) std::fprintf(stderr, "lens.lua: %s\n", error);
    if (lensed) {
        auto& camera = lensed->config().camera;
        CHECK(std::fabs(lensed->duration() - 2.0f) < 1.0e-4f);
        CHECK(lensed->view(0.0f) == CameraView::TwoD && lensed->view(0.5f) == CameraView::ThreeD
              && lensed->view(2.0f) == CameraView::ThreeD);
        CHECK(std::fabs(camera.orthoHeight - 4.0f) < 1.0e-4f && std::fabs(camera.fov - 0.5f) < 1.0e-6f
              && std::fabs(camera.far - 50.0f) < 1.0e-4f);
    }
    delete lensed;
    // A look back to the 2D view faces the plane straight on only once the moves it adds to end.
    _scriptError("local s = vis.scene {camera = {view = '3d', eye = {0, -8, 6}}} "
                 "s:parallel(function() s:look({height = 4}, 2) end, "
                 "function() s:look({view = '2d', eye = {0, 0, 10}, projection = 'orthographic'}, 1) end) return s",
                 "could not animate camera: the camera already moves until 2.00 s; a look that returns to the 2D "
                 "view ends at that time or later");
    // An orbit turns through any angle as one camera move, in parallel with object tracks.
    static constexpr auto orbit = R"lua(
local s = vis.scene {width = 160, height = 90,
                       camera = {mode = "fixed", view = "3d", eye = {0, 0, 10}, target = {0, 0, 0}, up = {0, 1, 0},
                                 projection = "perspective"}}
local dot = s:point {point = {2, 0, 0}}
s:parallel(
    function() s:orbit(2 * math.pi, {duration = 4, curve = "linear"}) end,
    function() s:fade(dot, 0.5, 1) end)
s:orbit(math.pi, {axis = {1, 0, 0}, duration = 1})
return s
)lua";
    Scene* orbiting = nullptr;
    CHECK(Lua::load(orbit, static_cast<uint32_t>(std::strlen(orbit)), "orbit.lua", &orbiting, error, sizeof(error))
          == Result::Success);
    if (!orbiting) std::fprintf(stderr, "orbit.lua: %s\n", error);
    if (orbiting) {
        auto& camera = orbiting->config().camera;
        CHECK(std::fabs(orbiting->duration() - 5.0f) < 1.0e-4f);
        CHECK(std::fabs(camera.eye.z + 10.0f) < 1.0e-3f && std::fabs(camera.up.y + 1.0f) < 1.0e-4f);
    }
    delete orbiting;
    _scriptError("local s = vis.scene {} s:orbit(1) return s", "could not orbit camera: only a 3D camera orbits");
    _scriptError("local s = vis.scene {camera = {mode = 'fixed', view = '3d'}} s:orbit(1, {about = {0, 0, 0}}) return s",
                 "unknown option 'about'");
    _scriptError("local s = vis.scene {camera = {mode = 'fixed', view = '3d'}} s:orbit(0) return s",
                 "the orbit angle must be a finite, non-zero number of radians");
    _scriptError("local s = vis.scene {camera = {mode = 'fixed', view = '3d'}} s:orbit(1, {axis = {0, 0, 0}}) return s",
                 "the orbit axis must be a finite, non-zero vector");
    _error("local s = vis.scene {} s:at('missing', function() end) return s", "at-marker.lua");
    _error("local s = vis.scene {} s:at(-1, function() end) return s", "at-negative.lua");
    _error("local s = vis.scene {} s:at(1) return s", "at-function.lua");
    _error("local s = vis.scene {} s:parallel() return s", "parallel-empty.lua");
    _error("local s = vis.scene {} s:parallel(function() end, 1) return s", "parallel-track.lua");
    _error("local s = vis.scene {} s:mark('x') s:mark('x') return s", "mark-duplicate.lua");
    _error("local s = vis.scene {} s:mark('') return s", "mark-empty.lua");
    _error("local s = vis.scene {} local a = s:circle {radius = 1} "
           "s:at(1, function() s:shift(a, {1, 0}, 2) end) s:remove(a) return s", "remove-animating.lua");
    _error("local s = vis.scene {} s:wait(2) local a = s:circle {radius = 1} "
           "s:at(0, function() s:fade_in(a) end) return s", "animate-before-add.lua");
}
