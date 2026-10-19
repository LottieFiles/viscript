#include "testLuaChecks.h"

// A function play blends vertically on matching grids without running callbacks during
// rendering, and keeps the midpoint switch for points and incompatible grids.
void _functionPlay(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {mode = "fixed", view = "2d", height = 6}}
local calls, finished = 0, false
local function square(x)
  assert(not finished, "render called fn")
  calls = calls + 1
  return 2 * x * x
end
local p = s:plot {id = "graph", fn = function() return 0 end, x_range = {-2, 2, 0.5}, y_range = {-1, 1}}
s:play({target = p, fn = square}, 1, "linear")
s:at(0.25, function() s:clone(p, {id = "quarter"}) end)
s:at(0.5, function() s:clone(p, {id = "half"}) end)
assert(calls < 200, "fn sampled more than once per grid and boundary")
finished = true
-- A plot from points has no source column, so different pieces still switch halfway.
local q = s:plot {id = "points", points = {{-2, -0.5}, {2, -0.5}}}
s:play({target = q, fn = function(x) return x * x end, x_range = {-2, 2, 0.5}}, 1, "linear")
s:at(1.25, function() s:clone(q, {id = "points_before"}) end)
s:at(1.75, function() s:clone(q, {id = "points_after"}) end)
local r = s:plot {fn = function() return 0 end, x_range = {-2, 2, 1}}
s:play({target = r, fn = function() return 1 end, x_range = {-2, 2, 0.5}}, 1, "linear")
s:at(2.25, function() s:clone(r, {id = "grid_before"}) end)
s:at(2.75, function() s:clone(r, {id = "grid_after"}) end)
return s
)lua";
    auto scene = _loaded(source, "function-play.lua");
    if (scene) {
        auto plot = [&](const char* id) { return static_cast<Plot*>(scene->object(id)); };
        auto at = [](const Plot* plot, float x, float y) {
            for (auto i = 0u; plot && i < plot->count(); i++) {
                if (std::fabs(plot->points()[i].x - x) < 1e-5f && std::fabs(plot->points()[i].y - y) < 1e-5f) return true;
            }
            return false;
        };
        CHECK(at(plot("quarter"), -1, 0.5f) && at(plot("quarter"), 0, 0) && at(plot("quarter"), 1, 0.5f));
        CHECK(at(plot("half"), -1, 1) && at(plot("half"), 0, 0) && at(plot("half"), 1, 1));
        CHECK(plot("points_before")->count() == 2u && at(plot("points_before"), -2, -0.5f));
        CHECK(plot("points_after")->count() == 9u && at(plot("points_after"), -2, 4));
        CHECK(plot("grid_before")->count() == 5u && at(plot("grid_before"), -2, 0));
        CHECK(plot("grid_after")->count() == 9u && at(plot("grid_after"), -2, 1));
        BBox bounds;
        CHECK(renderer->bounds(scene, scene->object("graph"), 0.25f, bounds) == Result::Success && bounds.width > 80);
    }
    delete scene;
    _scriptError("local s = vis.scene {} s:plot {fn = {points = {{0,0},{1,0}}, samples = {{1,0},{0,0}}}, x_range = {0,1,1}} return s",
                 "plot samples need an increasing x grid");
}

// Poles found between finite samples stay broken at baked frames and interior seek times.
void _animatedPlotGaps(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 960, height = 540, fps = 30, camera = {height = 540}}
local finished = false
local function shifted(p)
  return function(x)
    assert(not finished, "render called fn")
    return 50 * math.tan(x * math.pi / 210 - p)
  end
end
local phase = s:tracker(0)
local graph = s:plot {id = "graph", fn = shifted(0), x_range = {-420, 420, 0.8}, y_range = {-200, 200}}
s:bind(graph, phase, function(p) return {fn = shifted(p)} end)
local steep = s:plot {id = "steep", fn = function(x) return 80 * x end,
  x_range = {-1, 1, 2}, y_range = {-4, 4}}
local cubic = s:plot {id = "cubic", fn = function(x) return 80 * x * x * x end,
  x_range = {-1, 1, 2}, y_range = {-4, 4}}
s:at(4, function()
  s:play({{target = steep, fn = function(x) return 160 * x end},
    {target = cubic, fn = function(x) return 160 * x * x * x end}}, 2, "linear")
end)
s:at(4, function() s:track(phase, math.pi / 4, 2, "ease_in_out") end)
s:wait(6.1)
s:clone(graph, {id = "baked", opacity = 0})
finished = true
return s
)lua";
    auto scene = _loaded(source, "animated-poles.lua");
    if (!scene) return;
    auto graph = static_cast<Plot*>(scene->object("graph"));
    auto samples = graph->samples();
    auto gaps = 0u;
    for (auto i = 0u; i < samples.count; i++) gaps += !std::isfinite(samples.points[i].x);
    CHECK(gaps == 4u);
    // Half-frame seeks include every baked frame, sampled in reverse to catch stale geometry.
    for (auto k = 120; k >= 0; k--) {
        auto time = 4.0f + static_cast<float>(k) / 60.0f;
        CHECK(scene->seek(time) == Result::Success);
        Object* copy = nullptr;
        CHECK(scene->clone(graph, copy) == Result::Success);
        auto plot = copy ? static_cast<Plot*>(copy) : nullptr;
        CHECK(plot && plot->contours() == 5u);
        auto start = 0u;
        for (auto piece = 0u; plot && piece < plot->contours(); piece++) {
            auto end = plot->contourEnd(piece);
            for (auto i = start + 1u; i < end; i++) {
                auto delta = plot->points()[i] - plot->points()[i - 1u];
                CHECK(std::fabs(delta.y) < 399.0f || std::fabs(delta.x) > 2.0f);
            }
            start = end;
        }
    }
    CHECK(scene->seek(5.0f) == Result::Success);
    const char* continuous[] = {"steep", "cubic"};
    for (auto id : continuous) {
        Object* copy = nullptr;
        CHECK(scene->clone(scene->object(id), copy) == Result::Success);
        auto plot = copy ? static_cast<Plot*>(copy) : nullptr;
        CHECK(plot && plot->contours() == 1u && plot->count() >= 2u);
        CHECK(plot && _near(plot->points()[0].y, -4) && _near(plot->points()[plot->count() - 1u].y, 4));
    }
    // The center of the first pole gap has no graph stroke at the lesson's midpoint.
    const Object* targets[] = {graph};
    PixelSample sample;
    CHECK(renderer->sample(scene, 5.0f, {191.25f, 270.0f}, targets, 1u, sample) == Result::InsufficientCondition);
    CHECK(sample.color.a == 0u);
    delete scene;
}

// A plot takes fn or points of any count from play, set and bound states, and cuts them as it was
// cut when it was made: its graph follows a tracker frame by frame even as poles move through it.
void _replotting(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180, fps = 10, camera = {mode = "fixed", view = "2d", height = 9}}
local phi = s:tracker(0)
local function shifted(p) return function(x) return math.tan(x - p) end end
local graph = s:plot {id = "graph", fn = shifted(0), x_range = {-3, 3, 0.05}, y_range = {-4, 4}}
s:bind(graph, phi, function(p) return {fn = shifted(p)} end)
-- A run from -3 to the tracker's value, whose samples grow in number as it does.
local run = s:plot {id = "run", fn = function(x) if x <= 0 then return 0 end end, x_range = {-3, 3, 0.05}}
s:bind(run, phi, function(p) return {fn = function(x) if x <= p then return 0 end end} end)
-- Points of any count, cut to the band the plot was made with, before the timeline.
local cut = s:plot {id = "cut", points = {{0, 0}, {1, 0.25}, {2, 0}}, y_range = {-0.5, 0.5}}
s:set {target = cut, points = {{0, 0}, {1, 2}, {2, 0}, {3, 2}, {4, 0}}}
s:track(phi, 1, 1, "linear")
s:plot {id = "after", fn = shifted(1), x_range = {-3, 3, 0.05}, y_range = {-4, 4}}
-- fn for a plot of points takes x_range, which it keeps with the discontinuities for later states.
local steps = s:plot {id = "steps", points = {{0, 0}, {3, 3}}}
s:play({target = steps, fn = math.floor, x_range = {0, 3, 0.25}, discontinuities = {2, 1}}, 1)
s:play({target = steps, fn = math.ceil}, 1)
-- Nothing left inside the band leaves the plot empty until points come back, from 3 s on.
local gone = s:plot {id = "gone", fn = math.sin, x_range = {0, 3, 0.5}, y_range = {-1, 1}}
s:play({target = gone, fn = function(x) return 5 end}, 1)
s:play({target = gone, points = {{0, 0}, {1, 0}}}, 1)
-- A copy cuts fn as its original does, over its x_range and to its band.
local twin = s:clone(graph, {id = "twin"})
s:play({target = twin, fn = shifted(-1)}, 1)
s:plot {id = "before", fn = shifted(-1), x_range = {-3, 3, 0.05}, y_range = {-4, 4}}
return s
)lua";
    auto scene = _loaded(source, "replot.lua");
    if (scene) {
        auto plot = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::Plot ? static_cast<Plot*>(object) : nullptr;
        };
        auto at = [](const Plot* shape, uint32_t i, float x, float y) {
            return shape && i < shape->count() && std::fabs(shape->points()[i].x - x) < 1.0e-5f
                && std::fabs(shape->points()[i].y - y) < 1.0e-5f;
        };
        // The graph ends at the graph of tan(x - 1), cut as a plot made of it is: the same
        // points in the same pieces.
        auto graph = plot("graph");
        auto after = plot("after");
        CHECK(graph && after && graph->contours() == after->contours() && graph->count() == after->count());
        if (graph && after && graph->contours() == after->contours() && graph->count() == after->count()) {
            auto same = graph->flat && graph->contours() == 3u;
            for (auto i = 0u; i + 1u < graph->contours(); i++) same = same && graph->contourEnd(i) == after->contourEnd(i);
            for (auto i = 0u; i < graph->count(); i++) same = same && at(graph, i, after->points()[i].x, after->points()[i].y);
            CHECK(same);
        }
        // Each frame of the track shows the run of its value: its right end moves 2 pixels a
        // frame, a frame of 0.1 at 20 pixels a unit.
        auto run = plot("run");
        BBox start;
        CHECK(run && renderer->bounds(scene, run, 0.0f, start) == Result::Success);
        for (auto k = 1u; run && k <= 10u; k++) {
            BBox frame;
            CHECK(renderer->bounds(scene, run, static_cast<float>(k) / 10.0f, frame) == Result::Success);
            CHECK(std::fabs(frame.x + frame.width - (start.x + start.width) - 2.0f * static_cast<float>(k)) < 0.75f);
            CHECK(std::fabs(frame.x - start.x) < 0.01f);
        }
        // Five points two above the band leave three pieces of it, cut at its edge, which the plot
        // shows from the start, as its own.
        auto cut = plot("cut");
        CHECK(cut && cut->count() == 7u && cut->contours() == 3u && cut->contourEnd(0) == 2u && cut->contourEnd(1) == 5u);
        BBox crossing;
        CHECK(cut && renderer->bounds(scene, cut, 0.0f, crossing) == Result::Success && crossing.width > 70.0f);
        CHECK(at(cut, 1, 0.25f, 0.5f) && at(cut, 2, 1.75f, 0.5f) && at(cut, 3, 2.0f, 0.0f) && at(cut, 6, 4.0f, 0.0f));
        // ceil keeps the x_range and the discontinuities floor gave: three steps, each ending a
        // thousandth of a sample before the next, and the plot keeps its round ends.
        auto steps = plot("steps");
        CHECK(steps && steps->count() == 15u && steps->contours() == 3u && !steps->flat);
        CHECK(steps && steps->contourEnd(0) == 5u && steps->contourEnd(1) == 10u);
        CHECK(at(steps, 4, 0.99975f, 1.0f) && at(steps, 5, 1.00025f, 2.0f) && at(steps, 14, 3.0f, 3.0f));
        // The blend leaves the band before the midpoint, then points come back halfway
        // through the next clip, since that state has no function column to blend.
        auto gone = plot("gone");
        CHECK(gone && gone->count() == 2u && gone->contours() == 1u);
        BBox bounds;
        CHECK(renderer->bounds(scene, gone, 3.1f, bounds) == Result::Success);
        CHECK(renderer->bounds(scene, gone, 3.25f, bounds) == Result::Success);
        CHECK(renderer->bounds(scene, gone, 3.4f, bounds) != Result::Success);
        CHECK(renderer->bounds(scene, gone, 3.75f, bounds) != Result::Success);
        CHECK(renderer->bounds(scene, gone, 4.25f, bounds) != Result::Success);
        CHECK(renderer->bounds(scene, gone, 4.75f, bounds) == Result::Success);
        // The copy of the graph ends at the graph of tan(x + 1), cut as a plot made of it is.
        auto twin = plot("twin");
        auto before = plot("before");
        CHECK(twin && before && twin->count() == before->count() && twin->contours() == before->contours());
        if (twin && before && twin->count() == before->count() && twin->contours() == before->contours()) {
            auto same = twin->flat && twin->contours() == 3u;
            for (auto i = 0u; i + 1u < twin->contours(); i++) same = same && twin->contourEnd(i) == before->contourEnd(i);
            for (auto i = 0u; i < twin->count(); i++) same = same && at(twin, i, before->points()[i].x, before->points()[i].y);
            CHECK(same);
        }
    }
    delete scene;

    static constexpr auto prefix = "local s = vis.scene {} local function f(x) return x end "
                                   "local p = s:plot {points = {{0, 0}, {1, 1}}} local q = s:polygon {points = {{0, 0}, {1, 0}, {0, 1}}} ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:play({target = p, fn = f}, 1)", "plot fn requires x_range");
    fails("s:play({target = p, points = {{0, 0}, {1, 1}}, x_range = {0, 1, 0.5}}, 1)", "x_range needs fn");
    fails("s:play({target = p, y_range = {0, 1}}, 1)", "y_range needs fn or points");
    fails("s:play({target = p, discontinuities = {1}}, 1)", "discontinuities needs fn or points");
    fails("s:play({target = p, fn = f, x_range = {0, 1, 0.5}, points = {{0, 0}, {1, 1}}}, 1)",
          "a plot takes either points or fn, not both");
    fails("s:play({target = p, fn = 3}, 1)", "fn must be a function");
    fails("s:play({target = p, fn = f, x_range = {0, 1, 0.5}, y_range = {1, 0}}, 1)", "y_range must have lo below hi");
    fails("s:play({target = q, fn = f}, 1)", "fn needs a plot");
    fails("s:set {target = q, x_range = {0, 1, 0.5}}", "x_range needs a plot");
    fails("local t = s:tracker(0) s:bind(p, t, function(v) return {fn = f} end)", "plot fn requires x_range");

    // A plot of 950001 points leaves 49997 a plot of two may grow by. It grows by 19999 once for
    // 20001 points: states that hold no more take none, also after it shows two between them,
    // where a charge for the points past those it shows would pass the limit, and 40001 points
    // take only the 20000 past its room. 60001 then take 20000 more than the 9998 left.
    static constexpr auto growing = "local s = vis.scene {} local function zero() return 0 end "
                                    "s:plot {fn = zero, x_range = {0, 1, 1 / 950000}} "
                                    "local p = s:plot {points = {{0, 0}, {1, 0}}} "
                                    "s:play({target = p, fn = zero, x_range = {0, 1, 1 / 20000}}, 0.1) "
                                    "s:play({target = p, fn = zero}, 0.1) "
                                    "s:play({target = p, points = {{0, 0}, {1, 0}}}, 0.1) "
                                    "s:play({target = p, fn = zero}, 0.1) "
                                    "s:play({target = p, fn = zero, x_range = {0, 1, 1 / 40000}}, 0.1) ";
    char room[1024];
    std::snprintf(room, sizeof(room), "%sreturn s", growing);
    scene = _loaded(room, "room.lua");
    delete scene;
    std::snprintf(room, sizeof(room), "%ss:play({target = p, fn = zero, x_range = {0, 1, 1 / 60000}}, 0.1) return s",
                  growing);
    _scriptError(room, "scene point limit exceeded");
}

void _trimmedSegments(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 200, height = 100, fps = 10,
  camera = {mode = "fixed", view = "2d", height = 10}}
local l = s:line {from = {-4, 0}, to = {4, 0}, trim_distance = {1, 2}, width = 2, id = "line"}
local a = s:arrow {from = {-4, 2}, to = {4, 2}, trim_distance = 1, id = "arrow"}
local v = s:vector {origin = {-4, -2}, value = {8, 0}, trim_distance = {2, 1}, id = "vector"}
s:set {target = v, trim_distance = 1}
local x = s:tracker(0)
s:bind(l, x, function(t) return {to = {4, 6 * t}, trim_distance = {1 + t, 2 - t}} end)
s:track(x, 1, 1, "linear")
return s
)lua";
    auto scene = _loaded(source, "trim-distance.lua");
    if (scene) {
        auto line = static_cast<Line*>(scene->object("line"));
        auto arrow = static_cast<Arrow*>(scene->object("arrow"));
        auto vector = static_cast<Vector*>(scene->object("vector"));
        CHECK(line && arrow && vector);
        if (line && arrow && vector) {
            CHECK(_near(line->trimDistance.x, 2.0f) && _near(line->trimDistance.y, 1.0f));
            CHECK(_near(arrow->trimDistance.x, 1.0f) && _near(vector->trimDistance.y, 1.0f));
            BBox bounds;
            CHECK(renderer->bounds(scene, line, 0.5f, bounds) == Result::Success);
            CHECK(_near(bounds.x, 59.0f + 120.0f / std::sqrt(73.0f)));
        }
    }
    delete scene;
    _scriptError("local s = vis.scene {} s:line {from = {0, 0}, to = {1, 0}, trim_distance = -1} return s",
                 "trim_distance cannot be negative");
    _scriptError("local s = vis.scene {} s:arrow {from = {0, 0}, to = {1, 0}, trim_distance = {0, -1}} return s",
                 "trim_distance cannot be negative");
    _scriptError("local s = vis.scene {} s:vector {value = {1, 0}, trim_distance = {1}} return s",
                 "trim_distance must be a number or {start, end}");
    _scriptError("local s = vis.scene {} s:line {from = {0, 0}, to = {1, 0}, trim_distance = '1'} return s",
                 "trim_distance must be a number or {start, end}");
    _scriptError("local s = vis.scene {} local c = s:circle {radius = 1} s:play({target = c, trim_distance = 1}, 1) return s",
                 "trim distance needs a line, arrow or vector and non-negative distances");
    _scriptError("local s = vis.scene {} s:line {from = {0, 0}, to = {1, 0}, buff = 1} return s",
                 "unknown option 'buff'");
    _scriptError("local s = vis.scene {} s:arrow {from = {0, 0}, to = {1, 0}, buff = 1} return s",
                 "unknown option 'buff'");
    _scriptError("local s = vis.scene {} s:vector {value = {1, 0}, buff = 1} return s", "unknown option 'buff'");
    _scriptError("local s = vis.scene {} s:rectangle {size = {1, 1}, buff = 1} return s", "unknown option 'buff'");
    _scriptError("local s = vis.scene {} local l = s:line {from = {0, 0}, to = {1, 0}} "
                 "s:play({target = l, buff = 1}, 1) return s", "unknown option 'buff'");
    _scriptError("local s = vis.scene {} local l = s:line {from = {0, 0}, to = {1, 0}} "
                 "local t = s:tracker(0) s:bind(l, t, function(v) return {buff = v} end) return s",
                 "a bound state cannot set 'buff'");
}
