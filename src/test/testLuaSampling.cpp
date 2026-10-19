#include "testLuaChecks.h"

// A rectangle around objects takes the bounds they have where it joins, grown by padding.
void _plotBreaks()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local world = s:space {x = {-3, 3, 1}, y = {-4, 4, 1}}
world:plot {id = "tan", fn = math.tan, x_range = {-3, 3, 0.05}, y_range = {-4, 4}}
world:plot {id = "pole", fn = function(x) return 1 / x end, x_range = {-1, 1, 0.5}, y_range = {-4, 4}}
world:plot {id = "steep", points = {{0, -10}, {1, 10}, {2, -10}}, y_range = {-5, 5}}
world:plot {id = "inside", fn = function(x) return x end, x_range = {0, 1, 0.5}, y_range = {-1, 1}}
world:plot {id = "holes", points = {{0, 0}, {1, 1}, {2, 0 / 0}, {3, 1}, {x = 4, y = math.huge}, {5, 0}, {6, 1}}}
world:plot {id = "root", fn = math.sqrt, x_range = {-1, 1, 0.3}}
world:plot {id = "partial", fn = function(x) if x <= 0.5 then return x end end, x_range = {0, 1, 0.2}}
world:plot {id = "plain", fn = function(x) return x * x end, x_range = {0, 1, 0.25}}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "breaks.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "breaks.lua: %s\n", error);
    if (scene) {
        auto plot = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::Plot ? static_cast<Plot*>(object) : nullptr;
        };
        auto at = [](const Plot* shape, uint32_t i, float x, float y) {
            return shape && i < shape->count() && std::fabs(shape->points()[i].x - x) < 1.0e-5f
                && std::fabs(shape->points()[i].y - y) < 1.0e-5f;
        };
        // tan leaves the band at its edges and comes back across them past each pole, in one
        // plot of three pieces whose cut ends lie exactly on y = 4 and y = -4; no stroke joins
        // +4 to -4 at a pole.
        auto tan = plot("tan");
        CHECK(tan && tan->contours() == 3u && tan->flat);
        if (tan && tan->contours() == 3u) {
            auto first = tan->contourEnd(0), second = tan->contourEnd(1);
            auto atan4 = 1.3258176636680326f, pi = 3.14159265358979f;
            CHECK(at(tan, 0, -3.0f, std::tan(-3.0f)));
            CHECK(at(tan, first - 1u, atan4 - pi, 4.0f) && tan->points()[first - 1u].y == 4.0f);
            CHECK(at(tan, first, -atan4, -4.0f) && tan->points()[first].y == -4.0f);
            CHECK(at(tan, second - 1u, atan4, 4.0f) && tan->points()[second - 1u].y == 4.0f);
            CHECK(at(tan, second, pi - atan4, -4.0f) && tan->points()[second].y == -4.0f);
            CHECK(at(tan, tan->count() - 1u, 3.0f, std::tan(3.0f)));
            auto bounded = true;
            for (auto i = 0u; i < tan->count(); i++) bounded = bounded && std::fabs(tan->points()[i].y) <= 4.0f;
            CHECK(bounded);
        }
        // 1/x is infinite at the sample x = 0: the halving stops where the graph leaves the band.
        auto pole = plot("pole");
        CHECK(pole && pole->contours() == 2u && pole->count() == 6u && pole->contourEnd(0) == 3u);
        CHECK(at(pole, 0, -1.0f, -1.0f) && at(pole, 2, -0.25f, -4.0f) && at(pole, 3, 0.25f, 4.0f));
        CHECK(at(pole, 5, 1.0f, 1.0f));
        // A polyline through points is cut where its segments cross the edges, here twice.
        auto steep = plot("steep");
        CHECK(steep && steep->contours() == 2u && steep->count() == 4u);
        CHECK(at(steep, 0, 0.25f, -5.0f) && at(steep, 1, 0.75f, 5.0f));
        CHECK(at(steep, 2, 1.25f, 5.0f) && at(steep, 3, 1.75f, -5.0f));
        // A plot inside its band is one piece, which ends flat all the same.
        auto inside = plot("inside");
        CHECK(inside && inside->contours() == 1u && inside->count() == 3u && inside->flat);
        // A point that is not finite breaks the polyline, and a lone point between two such has
        // nothing to draw. Without y_range the ends stay round.
        auto holes = plot("holes");
        CHECK(holes && holes->contours() == 2u && holes->count() == 4u && !holes->flat);
        CHECK(at(holes, 1, 1.0f, 1.0f) && at(holes, 2, 5.0f, 0.0f));
        // Where fn stops being defined, nan or nil, the graph runs on to the edge of its domain.
        auto root = plot("root");
        CHECK(root && root->contours() == 1u && root->count() == 4u && !root->flat);
        if (root && root->count() == 4u) {
            CHECK(root->points()[0].x >= 0.0f && root->points()[0].x < 1.0e-6f && root->points()[0].y < 1.0e-3f);
            CHECK(at(root, 1, 0.2f, std::sqrt(0.2f)));
        }
        auto partial = plot("partial");
        CHECK(partial && partial->contours() == 1u && partial->count() == 4u && at(partial, 3, 0.5f, 0.5f));
        // Without the new options a plot keeps every sample as before.
        auto plain = plot("plain");
        CHECK(plain && plain->contours() == 1u && plain->count() == 5u && !plain->flat);
        CHECK(at(plain, 1, 0.25f, 0.0625f) && at(plain, 4, 1.0f, 1.0f));
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local function f(x) return x end; ";
    char script[320];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:plot {fn = f, x_range = {0, 1, 0.1}, y_range = {1, 0}}", "y_range must have lo below hi");
    fails("s:plot {fn = f, x_range = {0, 1, 0.1}, y_range = {0}}", "y_range must be {lo, hi}");
    fails("s:plot {fn = f, x_range = {0, 1, 0.1}, y_range = 'up'}", "y_range must be {lo, hi}");
    fails("s:plot {fn = f, x_range = {0, 1, 0.1}, y_range = {0, 0 / 0}}", "y_range must hold two finite numbers");
    fails("s:plot {fn = function(x) return {} end, x_range = {0, 1, 0.1}}",
          "plot fn must return a number, or nil where it is undefined, not table");
    fails("s:plot {fn = function(x) return 2 end, x_range = {0, 1, 0.5}, y_range = {-1, 1}}",
          "plot has nothing to draw inside y_range");
    fails("s:plot {fn = function(x) end, x_range = {0, 1, 0.5}}",
          "plot has nothing to draw: it needs two defined values in a row");
    fails("s:plot {points = {{0, 0}, {1, 0 / 0}, {2, 0}}}", "plot has nothing to draw: it needs two defined values in a row");
    fails("s:plot {points = {{0, 0}, {1}}}", "point must contain 2 or 3 numbers");
}

void _plotEndpoints(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width=320, height=180, camera={view="2d", height=6}}
local graph = s:plot {id="graph", fn=function(x) return 2*x end,
    x_range={-2,2,1}, y_range={-1,1}, endpoint_radius=3, width=0, progress=0}
s:clone(graph, {id="copy", shift={0,2}})
s:wait(1)
s:play({target=graph, fn=function(x) return 2*x+1 end}, 1, "linear")
s:wait(1)
s:set {target=graph, points={{-1,0},{1,0}}}
s:wait(1)
s:set {target=graph, points={}}
s:wait(1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "plot-endpoint.lua",
                    &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "plot-endpoint.lua: %s\n", error);
    if (scene) {
        auto graph = static_cast<Plot*>(scene->object("graph"));
        auto copy = static_cast<Plot*>(scene->object("copy"));
        CHECK(graph && copy && graph->endpointRadius == 3 && copy->endpointRadius == 3);
        BBox bounds;
        CHECK(renderer->bounds(scene, graph, 0, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 145) < 0.1f && std::fabs(bounds.center().y - 120) < 0.1f);
        CHECK(renderer->bounds(scene, copy, 0, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().y - 60) < 0.1f);
        CHECK(renderer->bounds(scene, graph, 1.5f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 137.5f) < 0.1f && std::fabs(bounds.center().y - 120) < 0.1f);
        CHECK(renderer->bounds(scene, graph, 2, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 130) < 0.1f && std::fabs(bounds.center().y - 120) < 0.1f);
        CHECK(renderer->bounds(scene, graph, 3, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 130) < 0.1f && std::fabs(bounds.center().y - 90) < 0.1f);
        CHECK(renderer->bounds(scene, graph, 4, bounds) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, graph, 0, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 145) < 0.1f);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:plot{points={{0,0},{1,1}},endpoint_radius=-1}; return s",
                 "endpoint_radius must be zero or more");
    _scriptError("local s=vis.scene{}; s:plot{points={{0,0},{1,1}},endpoint_radius=0/0}; return s",
                 "endpoint_radius must be a finite number");
    _scriptError("local s=vis.scene{}; s:plot{points={{0,0},{1,1}},endpoint_radius=true}; return s",
                 "number expected");
    _scriptError("local s=vis.scene{}; s:plot{points={{0,0}},endpoint_radius=3}; return s",
                 "points must contain at least 2 vectors");
}

void _plotEndpointProgress(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s=vis.scene{width=320,height=180,camera={view="2d",height=6}}
local down=s:plot{id="down",points={{-2,-1.5},{2,-1.5}},endpoint_radius=4,width=0,progress=1}
local up=s:plot{id="up",points={{-2,-0.5},{2,-0.5}},endpoint_radius=4,width=0,progress=0}
local stay=s:plot{id="stay",points={{-2,0.5},{2,0.5}},endpoint_radius=4,width=0,progress=0}
local group=s:group{}
local grouped=group:plot{id="grouped",points={{-2,1.5},{2,1.5}},endpoint_radius=4,width=0,progress=0}
local space=s:space{x={-3,3,1},y={-3,3,1},stroke="#00000000",axis_x="#00000000",axis_y="#00000000"}
local spaced=space:plot{id="spaced",points={{-2,2.5},{2,2.5}},endpoint_radius=4,width=0,progress=0}
local reset=s:plot{id="reset",points={{-2,-2.5},{2,-2.5}},endpoint_radius=4,width=0,progress=1}
local future=s:plot{id="future",points={{-1,0},{1,0}},endpoint_radius=4,width=0,progress=0}
s:uncreate(reset,1,"linear")
s:at(1,function()
    s:play({{target=down,progress=0},{target=up,progress=1},{target=stay,progress=0}},1,"linear")
end)
s:at(1,function() s:create({group,space},1,"linear") end)
s:at(1.5,function() s:play({target=reset,progress=0},1,"linear") end)
s:at(2,function() s:uncreate({group,space},1,"linear") end)
s:at(1,function() s:play({target=future,progress=0},1,"linear") end)
s:at(3,function() s:create(future,1,"linear") end)
s:at(3.5,function() s:create(reset,1,"linear") end)
s:at(4,function() s:create({group,space},1,"linear") end)
s:seek(5)
return s
)lua";
    Scene* scene = nullptr;
    char error[256] = {};
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "endpoint-progress.lua",
                    &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "endpoint-progress.lua: %s\n", error);
    if (scene) {
        for (auto time : {0.0f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 0.0f}) {
            for (auto id : {"down", "up", "stay", "grouped", "spaced", "reset", "future"}) {
                auto visible = true;
                if (std::strcmp(id, "grouped") == 0) visible = (time > 1 && time < 3) || time > 4;
                if (std::strcmp(id, "reset") == 0) visible = !(time >= 1 && time < 1.5f) && time != 3.5f;
                if (std::strcmp(id, "future") == 0) visible = time > 3;
                BBox bounds;
                auto result = renderer->bounds(scene, scene->object(id), time, bounds);
                CHECK(result == (visible ? Result::Success : Result::InsufficientCondition));
                if (visible) CHECK(std::fabs(bounds.width - 8) < 0.01f && std::fabs(bounds.height - 8) < 0.01f);
            }
        }
    }
    delete scene;
}

void _plotDiscontinuities()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local world = s:space {}
world:plot {id = "floor", fn = math.floor, x_range = {0, 3, 0.25}, discontinuities = {2, 1}}
world:plot {id = "risers", fn = math.floor, x_range = {0, 3, 0.25}}
world:plot {id = "tan", fn = math.tan, x_range = {-1, 2, 0.1}, discontinuities = {math.pi / 2}}
world:plot {id = "points", points = {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}}, discontinuities = {1.5, 3}}
world:plot {id = "last", fn = math.floor, x_range = {0, 1, 0.25}, discontinuities = {1, 5}}
world:plot {id = "first", fn = math.ceil, x_range = {0, 1, 0.25}, discontinuities = {-1, 0}}
world:plot {id = "twice", fn = function(x) return x end, x_range = {0, 1, 0.1}, discontinuities = {0.5, 0.5}}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "jumps.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "jumps.lua: %s\n", error);
    if (scene) {
        auto plot = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::Plot ? static_cast<Plot*>(object) : nullptr;
        };
        auto at = [](const Plot* shape, uint32_t i, float x, float y) {
            return shape && i < shape->count() && std::fabs(shape->points()[i].x - x) < 1.0e-5f
                && std::fabs(shape->points()[i].y - y) < 1.0e-5f;
        };
        // floor breaks at each discontinuity, in any order: every step ends a thousandth of
        // the sampling step before it and the next starts as far after it, flat, without the
        // riser that joins them otherwise.
        auto floor = plot("floor");
        CHECK(floor && floor->contours() == 3u && floor->count() == 15u && floor->flat);
        if (floor && floor->contours() == 3u) {
            CHECK(floor->contourEnd(0) == 5u && floor->contourEnd(1) == 10u);
            CHECK(at(floor, 4, 0.99975f, 0.0f) && at(floor, 5, 1.00025f, 1.0f));
            CHECK(at(floor, 9, 1.99975f, 1.0f) && at(floor, 10, 2.00025f, 2.0f) && at(floor, 13, 2.75f, 2.0f));
        }
        auto risers = plot("risers");
        CHECK(risers && risers->contours() == 1u && risers->count() == 13u && !risers->flat);
        // Without y_range the values next to a pole are drawn as they are, but not joined.
        auto tan = plot("tan");
        CHECK(tan && tan->contours() == 2u);
        if (tan && tan->contours() == 2u) {
            auto end = tan->contourEnd(0);
            CHECK(std::fabs(tan->points()[end - 1u].x - (1.5707963f - 1.0e-4f)) < 1.0e-5f);
            CHECK(tan->points()[end - 1u].y > 9000.0f && tan->points()[end].y < -9000.0f);
            CHECK(std::fabs(tan->points()[end].x - (1.5707963f + 1.0e-4f)) < 1.0e-5f);
        }
        // Points break between two on either side of a discontinuity, and drop one on it.
        auto points = plot("points");
        CHECK(points && points->contours() == 2u && points->count() == 4u && points->flat);
        CHECK(at(points, 1, 1.0f, 1.0f) && at(points, 2, 4.0f, 4.0f));
        // A discontinuity on the last sample ends the graph before it, without the riser to the
        // step that begins there, and one on the first starts the graph after it; those past
        // the samples are ignored, and two at one place share one gap.
        auto last = plot("last");
        CHECK(last && last->contours() == 1u && last->count() == 5u && last->flat);
        CHECK(at(last, 3, 0.75f, 0.0f) && at(last, 4, 0.99975f, 0.0f));
        auto first = plot("first");
        CHECK(first && first->contours() == 1u && first->count() == 5u && first->flat);
        CHECK(at(first, 0, 0.00025f, 1.0f) && at(first, 1, 0.25f, 1.0f) && at(first, 4, 1.0f, 1.0f));
        auto twice = plot("twice");
        CHECK(twice && twice->contours() == 2u && twice->count() == 12u);
        CHECK(at(twice, 5, 0.4999f, 0.4999f) && at(twice, 6, 0.5001f, 0.5001f));
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local function f(x) return x end; ";
    char script[320];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:plot {fn = f, x_range = {0, 1, 0.1}, discontinuities = 1}", "discontinuities must be a list of numbers");
    fails("s:plot {fn = f, x_range = {0, 1, 0.1}, discontinuities = {0 / 0}}", "discontinuities must be finite numbers");
    fails("s:plot {fn = f, x_range = {0, 1, 0.1}, discontinuities = {'a'}}", "discontinuities must be finite numbers");
}

void _spaceTicksAndLabels()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 600, height = 300, camera = {mode = "fixed", view = "2d", height = 6}}
local world = s:space {id = "world", x = {-3, 3, 1}, y = {-2, 2, 0.5}, ticks = {x = 2, y = true}, tick_length = 12, axis_width = 2,
    labels = {x = {{1, "one"}, {-2, "minus two"}}, y = {{1, "up"}}}}
s:space {id = "every", x = {-3, 3, 1}, y = {-2, 2, 0.5}, z = {-1, 1, 0.25}, ticks = true}
local bar = s:rectangle {id = "bar", size = {1, 0.5}}
local made = world:labels {x = {{2, bar}, {-1, "left"}}, y = {{-1, "down"}}, side = {x = "above", y = "right"},
    gap = 10, size = 20, color = "#ff0000"}
if #made ~= 3 or made[1] ~= bar or made[2] ~= world:children()[4] then error("labels returned the wrong handles") end
s:wait(1)
local late = world:labels {x = {{3, "late"}}}
if #late ~= 1 or late[1] ~= world:children()[6] then error("late labels returned the wrong handle") end
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "axes.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "axes.lua: %s\n", error);
    if (scene) {
        auto world = scene->object("world");
        auto every = scene->object("every");
        auto bar = scene->object("bar");
        CHECK(world && world->type() == Type::Space && every && every->type() == Type::Space && bar);
        if (world && world->type() == Type::Space && every && every->type() == Type::Space && bar) {
            auto space = static_cast<Space*>(world);
            auto ticks = static_cast<Space*>(every)->ticks;
            CHECK(space->ticks.x == 2.0f && space->ticks.y == 0.5f && space->ticks.z == 0.0f);
            CHECK(space->tickLength == 12.0f && space->axisWidth == 2.0f);
            CHECK(static_cast<Space*>(every)->axisWidth == -1.0f);
            CHECK(ticks.x == 1.0f && ticks.y == 0.5f && ticks.z == 0.25f);
            // 50 pixels to a unit: a label sits 6 + 4 pixels, 0.2, from its axis by default, and
            // 6 + 10, 0.32, with gap = 10.
            auto placed = [](const Object* object, float x, float y) {
                Bounds box;
                if (!object || !object->bounds(box)) return false;
                auto fits = std::fabs(box.center().x - x) < 1.0e-4f && std::fabs(box.center().y - y) < 1.0e-4f;
                if (!fits) {
                    std::fprintf(stderr, "  placed at (%.5f, %.5f), expected (%.5f, %.5f)\n",
                                 static_cast<double>(box.center().x), static_cast<double>(box.center().y),
                                 static_cast<double>(x), static_cast<double>(y));
                }
                return fits;
            };
            struct Expected
            {
                const char* text;
                float x, y, alignX, alignY, size;
                bool red;
            };
            const Expected expected[] = {
                {"one", 1.0f, -0.2f, 0.5f, 0.0f, 15.0f, false},
                {"minus two", -2.0f, -0.2f, 0.5f, 0.0f, 15.0f, false},
                {"up", -0.2f, 1.0f, 1.0f, 0.5f, 15.0f, false},
                {"left", -1.0f, 0.32f, 0.5f, 1.0f, 20.0f, true},
                {"down", 0.32f, -1.0f, 0.0f, 0.5f, 20.0f, true},
                {"late", 3.0f, -0.2f, 0.5f, 0.0f, 15.0f, false},
            };
            CHECK(world->childCount() == 6u);
            for (auto i = 0u; i < 6u && i < world->childCount(); i++) {
                auto& want = expected[i];
                auto child = world->childAt(i);
                auto text = child && child->type() == Type::Text ? static_cast<Text*>(child) : nullptr;
                CHECK(text && text->text() && std::strcmp(text->text(), want.text) == 0);
                if (!text) continue;
                CHECK(placed(text, want.x, want.y));
                CHECK(text->align.x == want.alignX && text->align.y == want.alignY && text->size == want.size);
                auto color = want.red ? Color{255, 0, 0, 255} : space->numberColor;
                CHECK(text->style.fill.r == color.r && text->style.fill.g == color.g && text->style.fill.b == color.b
                      && text->style.fill.a == color.a);
            }
            // A handle keeps its parent and places its bounds: a bar at the root above x = 2.
            CHECK(bar->parent() == nullptr && placed(bar, 2.0f, 0.57f));
        }
    }
    delete scene;

    // Labels are made and placed as other factories and layout calls are, without playing the
    // tracks that wait: a track after them still merges with one from before them where both
    // move a binding, as it would with nothing between them.
    static constexpr auto tracked = R"lua(
local s = vis.scene {width = 160, height = 90, fps = 10}
local x = s:tracker(0)
local z = s:tracker(1)
local n = s:number {value = 0, decimals = 2, id = "n"}
s:bind(n, {x, z}, function(xv, zv) return {value = 100 * xv + zv} end)
local axes = s:space {id = "axes", x = {-2, 2, 1}, y = {-1, 1, 1}, ticks = true}
s:parallel(function() s:track(x, 4, 4, "linear") end,
    function() s:wait(1) axes:labels {x = {{1, "one"}}} s:track(z, 3, 1, "linear") end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(tracked, static_cast<uint32_t>(std::strlen(tracked)), "tracked.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "tracked.lua: %s\n", error);
    if (scene) {
        auto number = scene->object("n");
        auto axes = scene->object("axes");
        char written[32] = {};
        if (number && number->type() == Type::Text) {
            scene->text(static_cast<Text*>(number), 1.5f, written, sizeof(written));
        }
        CHECK(std::strcmp(written, "152.00") == 0);
        Bounds box;
        CHECK(axes && axes->childCount() == 1u && axes->childAt(0u)->bounds(box)
              && std::fabs(box.center().x - 1.0f) < 1.0e-4f && box.max.y < 0.0f);
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local w=s:space{}; ";
    char script[400];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:space{axis_width = -1}", "axis_width must be zero or more pixels");
    fails("s:space{axis_width = 'wide'}", "number expected, got string");
    fails("s:space{ticks = 1}", "ticks must be true or {x = step, y = step, z = step}");
    fails("s:space{ticks = {x = 'a'}}", "ticks must be true or {x = step, y = step, z = step}");
    fails("s:space{ticks = {x = -1}}", "ticks x must be zero or more");
    fails("s:space{ticks = {w = 1}}", "unknown option 'w'");
    fails("s:space{tick_length = -1}", "tick_length must be zero or more pixels");
    fails("s:space{labels = 5}", "labels must be a table of x, y and z lists of {value, label}");
    fails("w:labels(5)", "table expected");
    fails("w:labels{x = 5}", "labels x must be a list of {value, label}");
    fails("w:labels{x = {5}}", "labels x entry 1 must be {value, label}, a number and a string or handle");
    fails("w:labels{x = {{1}}}", "labels x entry 1 must be {value, label}, a number and a string or handle");
    fails("w:labels{y = {{1, 'a'}, {'b', 'c'}}}", "labels y entry 2 must be {value, label}");
    fails("w:labels{x = {{0 / 0, 'a'}}}", "a label's value must be a finite number");
    fails("w:labels{x = {{1, 'a\\0b'}}}", "a label cannot contain zero bytes");
    fails("w:labels{side = 'left'}", "labels side must be {x = 'below' or 'above'");
    fails("w:labels{side = {x = 'left'}}", "labels side must be {x = 'below' or 'above'");
    fails("w:labels{gap = -1}", "labels gap must be zero or more pixels");
    fails("w:labels{size = 0}", "labels size must be positive");
    fails("w:labels{q = 1}", "unknown option 'q'");
    fails("w:labels{x = {{1, w}}}", "could not place label: the label must be another object of the space's scene");
    fails("local g = s:group{}; local v = g:space{}; v:labels{x = {{1, g}}}", "not the space or one above it");
    fails("local o = vis.scene{}; w:labels{x = {{1, o:rectangle{}}}}", "handle belongs to another scene");
    fails("local r = s:rectangle{}; s:wait(1); w:labels{x = {{1, r}}}",
          "could not place label: layout and direct edits must come before the first play");
}

void _spaceScreenLabels(Renderer* renderer)
{
#ifdef VIS_TEST_FONT
    static constexpr auto script = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {mode = "fixed", view = "2d", height = 6}}
local axes = s:space {id = "axes", x = {-2, 2, 1}, y = {-1, 1, 1},
    labels = {placement = "screen", x = {{-2, "x"}}, gap = 10, size = 18}}
local supplied = axes:text {id = "supplied", text = "y", size = 24}
local labels = axes:labels {placement = "screen", y = {{1, supplied}}, gap = 10}
if labels[1] ~= supplied then error("screen labels changed a supplied handle") end
s:look({height = 4}, 1, "linear")
axes:labels {placement = "screen", x = {{2, "late"}}, gap = 10}
s:warp(axes, function(x, y, z) return x + 3, y, z end, 1, "linear")
return s
)lua";
    Scene* scene = nullptr;
    char error[512];
    CHECK(Lua::load(script, static_cast<uint32_t>(std::strlen(script)), "screen-labels.lua",
                    &scene, error, sizeof(error)) == Result::Success);
    if (scene) {
        auto label = scene->object("axes/label/x/1");
        CHECK(label && label->parent() == scene->object("axes"));
        CHECK(label && scene->object("axes/label/x/2") && scene->object("axes/label/x/2")->parent() == label->parent());
        for (auto time : {0.0f, 1.0f, 0.5f, 1.0f}) {
            LayoutReport report;
            CHECK(renderer->layout(scene, time, report) == Result::Success);
            auto anchor = 160.0f - 360.0f / (6.0f - 2.0f * time);
            for (auto i = 0u; i < report.objectCount(); i++) {
                auto object = report.objectAt(i);
                if (object->object == label) {
                    CHECK(object->visible && std::fabs(object->paintBounds.x + object->paintBounds.width
                                                      - (anchor - 14.0f)) < 0.02f);
                }
            }
        }
        for (auto time : {2.0f, 1.75f, 1.0f, 2.0f}) {
            LayoutReport report;
            CHECK(renderer->layout(scene, time, report) == Result::Success);
            auto anchor = 160.0f + 45.0f * (-2.0f + 3.0f * (time - 1.0f));
            for (auto i = 0u; i < report.objectCount(); i++) {
                auto object = report.objectAt(i);
                if (object->object == label) {
                    CHECK(object->visible && std::fabs(object->paintBounds.x + object->paintBounds.width
                                                      - (anchor - 14.0f)) < 0.02f);
                }
            }
        }
    }
    delete scene;
#else
    (void) renderer;
#endif
    _scriptError("local s=vis.scene{}; local a=s:space{}; a:labels{placement='other'}; return s",
                 "labels placement must be 'world' or 'screen'");
    _scriptError("local s=vis.scene{}; local a=s:space{}; a:labels{placement='screen',side={x='above'}}; return s",
                 "screen labels do not take side");
    _scriptError("local s=vis.scene{}; local a=s:space{}; a:labels{placement='screen',x={{1,s:rectangle{}}}}; return s",
                 "a screen label must be a billboard Text");
    _scriptError("local s=vis.scene{}; local a=s:space{}; a:labels{placement='screen',x={{1,s:text{text='x'}}}}; return s",
                 "a screen label must be a direct text child");
    _scriptError("local s=vis.scene{}; local a=s:space{}; local g=a:group{}; a:labels{placement='screen',x={{1,g:text{text='x'}}}}; return s",
                 "a screen label must be a direct text child");
    _scriptError("local s=vis.scene{}; local a=s:space{}; a:labels{placement='screen',x={{1,a:text{text='x',orientation='plane'}}}}; return s",
                 "a screen label must use billboard orientation");
    _scriptError("local s=vis.scene{}; local a=s:space{}; local t=a:text{text='x'}; s:wait(1); a:labels{placement='screen',x={{1,t}}}; return s",
                 "before the first play");
    _scriptError("local s=vis.scene{}; local a=s:space{}; s:warp(a,function(x,y,z) return x+3,y,z end); a:labels{placement='screen',x={{-2,'late'}}}; return s",
                 "before the space's first warp");
    _scriptError("local s=vis.scene{}; local a=s:space{labels={placement='screen',x={{-2,'x'}}}}; s:warp(a,function(x,y,z) return x+3,y,z end); a:labels{placement='screen',x={{-2,'late'}}}; return s",
                 "before the space's first warp");
}

void _spaceLabelIds()
{
    static constexpr auto source = R"lua(
local s = vis.scene {camera = {mode = "fixed", view = "2d", height = 6}}
local kept = s:text {id = "axes/label/x/1", text = "kept"}
local axes = s:space {id = "axes", x = {-2, 2, 1}, y = {-2, 2, 1}, z = {-2, 2, 1},
    labels = {x = {{1, kept}, {-1, "first"}}, y = {{1, "up"}}, z = {{1, "depth"}}}}
local first = s:find("axes/label/x/2")
if first ~= axes:children()[1] then error("generated label id was not registered") end
local more = axes:labels {x = {{1, "second"}, {-1, kept}}, y = {{-1, "down"}}}
local last = axes:labels {x = {{2, "third"}}}
if s:find("axes/label/x/1") ~= kept or more[2] ~= kept then error("supplied label id changed") end
if s:find("axes/label/x/2") ~= first or s:find("axes/label/x/3") ~= more[1]
    or s:find("axes/label/x/4") ~= last[1] then error("repeated labels changed lookup") end
if s:find("axes/label/y/1") ~= axes:children()[2]
    or s:find("axes/label/y/2") ~= more[3]
    or s:find("axes/label/z/1") ~= axes:children()[3] then error("axis label ids differ") end
local anonymous = s:space {labels = {x = {{1, "anonymous"}}}}
if #anonymous:labels {x = {{2, "another"}}} ~= 1 then error("id-less labels failed") end
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "label-ids.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "label-ids.lua: %s\n", error);
    if (scene) {
        auto axes = scene->object("axes");
        CHECK(axes && axes->childCount() == 6u);
        auto anonymous = scene->objectAt(scene->count() - 3u);
        CHECK(anonymous && anonymous->childCount() == 2u);
        if (anonymous && anonymous->childCount() == 2u) {
            CHECK(!anonymous->childAt(0)->tag() && !anonymous->childAt(1)->tag());
        }
    }
    delete scene;
}

void _areas()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local space = s:space {x = {0, 5, 1}, y = {-1, 3, 1}}
local function f(x) return 1 + x end
space:area {id = "under", fn = f, x_range = {0, 1, 0.5}, fill = "#2563eb55"}
space:area {id = "lifted", fn = f, x_range = {0, 1, 0.4}, base = -1}
space:area {id = "between", fn = f, x_range = {1, 2, 0.5}, base = function(x) return x / 2 end}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "area.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "area.lua: %s\n", error);
    if (scene) {
        auto polygon = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::Polygon ? static_cast<Polygon*>(object) : nullptr;
        };
        auto point = [](const Polygon* shape, uint32_t i, float x, float y) {
            return _near(shape->points()[i].x, x) && _near(shape->points()[i].y, y);
        };
        // The graph from a to b, then two corners on the axis back to a.
        auto under = polygon("under");
        CHECK(under && under->count() == 5u && under->parent() && under->parent()->type() == Type::Space);
        if (under) {
            CHECK(point(under, 0, 0.0f, 1.0f) && point(under, 1, 0.5f, 1.5f) && point(under, 2, 1.0f, 2.0f));
            CHECK(point(under, 3, 1.0f, 0.0f) && point(under, 4, 0.0f, 0.0f));
        }
        // A step that does not divide the range ends short at b, here on a base of -1.
        auto lifted = polygon("lifted");
        CHECK(lifted && lifted->count() == 6u);
        if (lifted) {
            CHECK(point(lifted, 2, 0.8f, 1.8f) && point(lifted, 3, 1.0f, 2.0f));
            CHECK(point(lifted, 4, 1.0f, -1.0f) && point(lifted, 5, 0.0f, -1.0f));
        }
        // A second function runs the lower edge back from b to a.
        auto between = polygon("between");
        CHECK(between && between->count() == 6u);
        if (between) {
            CHECK(point(between, 2, 2.0f, 3.0f) && point(between, 3, 2.0f, 1.0f));
            CHECK(point(between, 4, 1.5f, 0.75f) && point(between, 5, 1.0f, 0.5f));
        }
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local function f(x) return x end; ";
    char script[256];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:area {x_range = {0, 1, 0.1}}", "area requires fn, a function");
    fails("s:area {fn = f}", "area requires x_range");
    fails("s:area {fn = f, x_range = {0, 1, 0.1}, base = 'axis'}", "base must be a number or a function");
    fails("s:area {fn = f, x_range = {1, 1, 0.1}}", "x_range must cover from 1 to");
    fails("s:area {fn = f, x_range = {0, 1, 0.1}, spin = 1}", "unknown option 'spin'");
}

void _riemannSums()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local space = s:space {x = {0, 5, 1}, y = {-1, 3, 1}}
local function f(x) return x - 1 end
space:riemann {id = "left", fn = f, x_range = {0, 2, 0.5}, fill = "#f59e0b", negative = "#ef4444", layer = 3}
space:riemann {id = "right", fn = f, x_range = {0, 2, 0.5}, sample = "right"}
space:riemann {id = "middle", fn = f, x_range = {0, 1.2, 0.5}, sample = "midpoint"}
space:riemann {id = "given", values = {2, 0}, x_range = {0, 1, 0.5}, stroke = "#ffffff", width = 1}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "riemann.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "riemann.lua: %s\n", error);
    if (scene) {
        auto bar = [&](const char* id, float x, float y, float width, float height) {
            auto object = scene->object(id);
            if (!object || object->type() != Type::Rectangle) return false;
            auto rectangle = static_cast<Rectangle*>(object);
            return _near(rectangle->center.x, x) && _near(rectangle->center.y, y) && _near(rectangle->size.x, width)
                   && _near(rectangle->size.y, height);
        };
        // A group of rectangles from the left, each as tall as f at the left end of its step.
        auto sum = scene->object("left");
        CHECK(sum && sum->type() == Type::Group && sum->childCount() == 4u && sum->parent()
              && sum->parent()->type() == Type::Space);
        CHECK(bar("left/1", 0.25f, -0.5f, 0.5f, 1.0f) && bar("left/2", 0.75f, -0.25f, 0.5f, 0.5f));
        CHECK(bar("left/4", 1.75f, 0.25f, 0.5f, 0.5f) && sum && sum->childAt(0) == scene->object("left/1"));
        // Below the axis the negative color fills; the colors and the layer reach every bar.
        auto first = scene->object("left/1");
        auto last = scene->object("left/4");
        CHECK(first && first->style.fill.r == 0xef && first->layer == 3);
        CHECK(last && last->style.fill.r == 0xf5 && last->layer == 3 && sum && sum->layer == 3);
        // A height of zero stays as a flat bar, so the count never changes.
        auto flat = scene->object("left/3");
        CHECK(flat && flat->type() == Type::Rectangle && static_cast<Rectangle*>(flat)->size.y < 1.0e-5f);
        CHECK(bar("right/1", 0.25f, -0.25f, 0.5f, 0.5f) && bar("right/4", 1.75f, 0.5f, 0.5f, 1.0f));
        // The last step ends short at the end of the range, read at its own midpoint.
        CHECK(bar("middle/3", 1.1f, 0.05f, 0.2f, 0.1f));
        CHECK(bar("given/1", 0.25f, 1.0f, 0.5f, 2.0f));
        auto given = scene->object("given/2");
        CHECK(given && given->style.width == 1.0f && given->style.stroke.r == 0xff);
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local function f(x) return x end; ";
    char script[256];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:riemann {fn = f}", "riemann requires x_range");
    fails("s:riemann {x_range = {0, 1, 0.5}}", "riemann requires either fn or values");
    fails("s:riemann {fn = f, values = {1, 2}, x_range = {0, 1, 0.5}}", "riemann requires either fn or values");
    fails("s:riemann {values = {1}, x_range = {0, 1, 0.5}}", "values must hold one height for each of the 2 steps");
    fails("s:riemann {values = {1, 2}, x_range = {0, 1, 0.5}, sample = 'right'}", "sample needs fn");
    fails("s:riemann {fn = f, x_range = {0, 1, 0.5}, sample = 'center'}", "sample must be 'left', 'right' or 'midpoint'");
    fails("s:riemann {fn = f, x_range = {0, 2000, 1}}", "x_range must cover from 1 to 1024 steps");
    fails("s:riemann {fn = f, x_range = {0, 1, 0.5}, radius = 1}", "unknown option 'radius'");
}

// A surface takes its colors from a colormap's name over its own heights, a colormap table with
// a range and an axis, a function of each point, or a list of one color per point.
void _surfaceColors()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {view = "3d", eye = {3, -4, 3}, up = {0, 0, 1}}}
local points = {{0, 0, -1}, {1, 0, 0.5}, {0, 1, 0.5}, {1, 1, 3}}
s:surface {id = "named", points = points, size = {2, 2}, colors = "plasma"}
s:surface {id = "table", points = points, size = {2, 2}, colors = {map = "magma", range = {2, 0}, axis = "x"}}
s:surface {id = "tilted", points = points, size = {2, 2}, colors = {map = "cividis", axis = {3, 4, 0}}}
s:surface {id = "flat", points = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, size = {2, 2}, colors = "inferno"}
s:surface {id = "function", points = points, size = {2, 2},
           colors = function(x, y, z) if z > 1 then return "#ff0000" end return "accent" end}
s:surface {id = "list", points = points, size = {2, 2}, colors = {"#000", "#fff", "#00ff0080", "danger"}}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "colors.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "colors.lua: %s\n", error);
    if (scene) {
        auto surface = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::SurfaceMesh ? static_cast<SurfaceMesh*>(object) : nullptr;
        };
        auto same = [](const Color& a, const Color& b) {
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        };
        // A name runs the map over the heights of the points, from the lowest to the highest.
        auto named = surface("named");
        CHECK(named && named->colormap.map == Colormap::Plasma && !named->colors());
        CHECK(named && _near(named->colormap.min, -1.0f) && _near(named->colormap.max, 3.0f));
        CHECK(named && _near(named->colormap.axis.z, 1.0f) && _near(named->colormap.axis.x, 0.0f));
        // A table names the map, its range, which may run the other way, and its axis.
        auto table = surface("table");
        CHECK(table && table->colormap.map == Colormap::Magma);
        CHECK(table && _near(table->colormap.min, 2.0f) && _near(table->colormap.max, 0.0f));
        CHECK(table && _near(table->colormap.axis.x, 1.0f) && _near(table->colormap.axis.z, 0.0f));
        // An axis given as a vector is made a unit long, and the range defaults to the extent
        // of the points along it.
        auto tilted = surface("tilted");
        CHECK(tilted && tilted->colormap.map == Colormap::Cividis);
        CHECK(tilted && _near(tilted->colormap.axis.x, 0.6f) && _near(tilted->colormap.axis.y, 0.8f));
        CHECK(tilted && _near(tilted->colormap.min, 0.0f) && _near(tilted->colormap.max, 1.4f));
        // A flat surface takes the middle of the map.
        auto flat = surface("flat");
        CHECK(flat && _near(flat->colormap.min, -1.0f) && _near(flat->colormap.max, 1.0f));
        // A function colors every point, in hex or as a theme role.
        auto function = surface("function");
        CHECK(function && function->colormap.map == Colormap::None && function->colors());
        if (function && function->colors()) {
            auto accent = scene->theme().color(ThemeColorRole::Accent);
            CHECK(same(function->colors()[0], accent) && same(function->colors()[2], accent));
            CHECK(same(function->colors()[3], Color::hex("#ff0000")));
        }
        auto list = surface("list");
        CHECK(list && list->colors());
        if (list && list->colors()) {
            CHECK(same(list->colors()[1], Color::hex("#ffffff")));
            CHECK(same(list->colors()[2], Color{0, 255, 0, 128}));
            CHECK(same(list->colors()[3], scene->theme().color(ThemeColorRole::Danger)));
        }
    }
    delete scene;

    static constexpr auto prefix =
        "local s=vis.scene{}; local p={{0,0,0},{1,0,0},{0,1,0},{1,1,1}}; ";
    char script[400];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:surface{points=p, size={2,2}, colors='jet'}",
          "surface colors must name a colormap: 'viridis', 'plasma', 'inferno', 'magma' or 'cividis'");
    fails("s:surface{points=p, size={2,2}, colors=3}",
          "surface colors must be a colormap's name, {map = ...}, a function or a list of colors");
    fails("s:surface{points=p, size={2,2}, colors={map='viridis', range={1, 1}}}",
          "surface colors range must have two different ends");
    fails("s:surface{points=p, size={2,2}, colors={map='viridis', range={1}}}",
          "surface colors range must be {low, high}");
    fails("s:surface{points=p, size={2,2}, colors={map='viridis', axis='w'}}",
          "surface colors axis must be 'x', 'y', 'z' or a vector");
    fails("s:surface{points=p, size={2,2}, colors={map='viridis', axis={0, 0, 0}}}",
          "surface colors axis must not be zero");
    fails("s:surface{points=p, size={2,2}, colors={map='viridis', low=0}}", "unknown option 'low'");
    fails("s:surface{points=p, size={2,2}, colors={'#000', '#fff'}}",
          "surface colors must list one color per point, 4 in all");
    fails("s:surface{points=p, size={2,2}, colors={'#000', '#fff', '#000', 'teal'}}",
          "surface colors must be a hex color or Theme color role");
    fails("s:surface{points=p, size={2,2}, colors=function(x, y, z) return 1 end}",
          "surface colors must return a hex color or Theme color role; at (0.0, 0.0, 0.0) it did not");
    fails("s:surface{points=p, size={2,2}, colors=function(x, y, z) error('no color here') end}",
          "no color here");

    // mesh = {u, v} draws u lines across the columns and v across the rows.
    static constexpr auto meshed = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {view = "3d", eye = {3, -4, 3}, up = {0, 0, 1}}}
local points = {}
for j = 0, 4 do for i = 0, 6 do points[#points + 1] = {i, j, 0} end end
s:surface {id = "wire", points = points, size = {7, 5}, mode = "mesh", mesh = {4, 0}}
s:surface {id = "lined", points = points, size = {7, 5}, mode = "solid_mesh", mesh = {7, 2}}
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(meshed, static_cast<uint32_t>(std::strlen(meshed)), "mesh.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "mesh.lua: %s\n", error);
    if (scene) {
        auto wire = static_cast<SurfaceMesh*>(scene->object("wire"));
        auto lined = static_cast<SurfaceMesh*>(scene->object("lined"));
        CHECK(wire && wire->meshColumns == 4u && wire->meshRows == 0u && wire->mode == SurfaceMode::Mesh);
        CHECK(lined && lined->meshColumns == 7u && lined->meshRows == 2u);
    }
    delete scene;
    fails("s:surface{points=p, size={2,2}, mesh={2,2}}", "surface mesh needs mode 'mesh' or 'solid_mesh'");
    fails("s:surface{points=p, size={2,2}, mode='mesh', mesh=2}", "surface mesh must be {columns, rows}, the lines across each");
    fails("s:surface{points=p, size={2,2}, mode='mesh', mesh={1,2}}",
          "surface mesh takes 0 or 2 to 2 lines across the columns and 0 or 2 to 2 across the rows");
    fails("s:surface{points=p, size={2,2}, mode='mesh', mesh={2,3}}",
          "surface mesh takes 0 or 2 to 2 lines across the columns and 0 or 2 to 2 across the rows");
    fails("s:surface{points=p, size={2,2}, mode='mesh', mesh={2,1.5}}", "surface mesh must be an integer");

    // light = {direction, ambient} sets a light fixed in the world, ambient a half by default.
    static constexpr auto lit = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {view = "3d", eye = {3, -4, 3}, up = {0, 0, 1}}}
local points = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 1}}
s:surface {id = "lit", points = points, size = {2, 2}, light = {direction = {-1, 1, 1}}}
s:surface {id = "dim", points = points, size = {2, 2}, light = {direction = {x = 0, y = 0, z = 2}, ambient = 0.2}}
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(lit, static_cast<uint32_t>(std::strlen(lit)), "light.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "light.lua: %s\n", error);
    if (scene) {
        auto first = static_cast<SurfaceMesh*>(scene->object("lit"));
        auto second = static_cast<SurfaceMesh*>(scene->object("dim"));
        CHECK(first && _near(first->light.direction.x, -1.0f) && _near(first->light.direction.z, 1.0f));
        CHECK(first && _near(first->light.ambient, 0.5f) && first->shading);
        CHECK(second && _near(second->light.direction.z, 2.0f) && _near(second->light.ambient, 0.2f));
    }
    delete scene;
    fails("s:surface{points=p, size={2,2}, light={ambient=0.3}}", "surface light requires a direction");
    fails("s:surface{points=p, size={2,2}, light={direction={0,0,0}}}", "surface light direction must not be zero");
    fails("s:surface{points=p, size={2,2}, light={direction={0,0,1}, ambient=2}}",
          "surface light ambient must be between 0 and 1");
    fails("s:surface{points=p, size={2,2}, light={direction={0,0,1}, color='#fff'}}", "unknown option 'color'");
    fails("s:surface{points=p, size={2,2}, light={0,0,1}}",
          "surface light must be {direction = {x, y, z}, ambient = a}");
    fails("s:surface{points=p, size={2,2}, light='sun'}",
          "surface light must be {direction = {x, y, z}, ambient = a}");
    fails("s:surface{points=p, size={2,2}, shading=false, light={direction={0,0,1}}}", "surface light needs shading");
}

void _contours()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local axes = s:space {x = {-2, 2, 1}, y = {-2, 2, 1}, stroke = "#00000000", axis_x = "#00000000", axis_y = "#00000000", axis_z = "#00000000"}
local calls = 0
local c = axes:contour {id = "c", fn = function(x, y) calls = calls + 1; return x*x + y*y end,
  levels = {1, 0.25, 10}, x_range = {-1.5, 1.5, 0.125}, y_range = {-1.5, 1.5, 0.125},
  colors = {"#ff0000", "#00ff00", "#0000ff"}, width = 3, layer = 2, opacity = 0.5}
assert(#c:children() == 3 and calls > 625)
s:contour {id = "v", values = {0, 1, 1, 2}, levels = {0.5, 1.5}, x_range = {0, 1, 1}, y_range = {0, 1, 1},
  colors = "viridis", stroke = "#ff0000"}
s:contour {id = "plain", fn = function(x, y) return x+y end, levels = {1},
  x_range = {0, 1, 1}, y_range = {0, 1, 1}, stroke = "#123456", dash = {3, 2}}
local p = c:children()[1]
s:create(p, 0.2)
s:indicate(p, {duration = 0.2})
s:passing_flash(p, {duration = 0.2})
s:fade_in(c, {duration = 0.2})
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "contour.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "contour.lua: %s\n", error);
    if (scene) {
        auto group = scene->object("c");
        CHECK(group && group->type() == Type::Group && group->childCount() == 3u);
        CHECK(group && group->parent() && group->parent()->type() == Type::Space);
        for (auto i = 0u; group && i < 3u; i++) {
            auto plot = static_cast<Plot*>(group->childAt(i));
            CHECK(plot->type() == Type::Plot && plot->flat && plot->layer == 2);
            CHECK(_near(plot->style.width, 3.0f));
            if (i == 2u) CHECK(!plot->count() && !plot->contours());
            else {
                CHECK(plot->contours() == 1u);
                auto level = i ? 0.25 : 1.0;
                for (auto j = 0u; j < plot->count(); j++) {
                    auto& p = plot->points()[j];
                    CHECK(std::fabs(static_cast<double>(p.x)*p.x + static_cast<double>(p.y)*p.y - level) < 1.0e-6);
                }
            }
        }
        CHECK(scene->object("c/1") == group->childAt(0));
        CHECK(_sameColor(scene->object("c/1")->style.stroke, Color{255, 0, 0, 255}));
        CHECK(_sameColor(scene->object("c/2")->style.stroke, Color{0, 255, 0, 255}));
        CHECK(_sameColor(scene->object("v/1")->style.stroke, Color{68, 1, 84, 255}));
        CHECK(_sameColor(scene->object("v/2")->style.stroke, Color{253, 231, 37, 255}));
        CHECK(_sameColor(scene->object("plain/1")->style.stroke, Color::hex("#123456")));
    }
    delete scene;
    auto fails = [](const char* options, const char* expected) {
        char script[1024];
        std::snprintf(script, sizeof(script), "local s=vis.scene{}; s:contour {%s}; return s", options);
        _scriptError(script, expected);
    };
    fails("levels={}", "levels must hold from 1 to 1024 finite numbers");
    fails("levels={0/0}", "levels must be finite numbers");
    fails("levels={0}", "contour requires x_range");
    fails("levels={0}, x_range={0,1,1}", "contour requires y_range");
    fails("levels={0}, x_range={0,0,1}, y_range={0,1,1}", "contour needs at least two samples along each range");
    fails("levels={0}, x_range={0,300,1}, y_range={0,300,1}", "a contour grid holds at most 65536 samples");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}", "contour requires either fn or values");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, values={0,1,1,2}, fn=function() return 0 end", "contour requires either fn or values");
    fails("levels={0}, x_range={16777216,16777220,1}, y_range={0,1,1}", "contour grid samples must have distinct coordinates");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, fn=1", "fn must be a function");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, values={1}", "values must hold one finite number for each of the 4 samples");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, values={0,1,1,1/0}", "at (1.0, 1.0) one was not");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, fn=function(x,y) return 0/0 end", "at (0.0, 0.0) it did not");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, fn=function(x,y) return 'x' end", "must return a finite number");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, values={0,1,1,2}, colors={}", "contour colors must hold one color per level");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, values={0,1,1,2}, colors='rainbow'", "contour colors must name");
    fails("levels={0}, labels='yes'", "labels must be a boolean or a table");
    // An error only during refinement is propagated after the core releases its buffers.
    fails("levels={0.25}, x_range={0,1,1}, y_range={0,1,1}, fn=function(x,y) if x>0 and x<1 then error('edge failed') end return x end", "edge failed");
    fails("levels={0.25}, x_range={0,1,1}, y_range={0,1,1}, fn=function(x,y) if x>0 and x<1 then return 1/0 end return x end", "at (0.5, 0.0) it did not");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, fn=function() s:circle{}; return 0 end", "scene authoring is unavailable inside a sampling callback");
    fails("levels={0}, x_range={0,1,1}, y_range={0,1,1}, fn=function() while true do end end", "instruction limit exceeded");
    _scriptError("local s=vis.scene{}; local l={}; for i=1,16 do l[i]=i end; s:contour {levels=l, x_range={0,255,1}, y_range={0,255,1}, fn=function(x,y) return x end}; return s",
                 "contour samples times levels must be at most 1000000");
    _scriptError("local s=vis.scene{}; for i=1,16 do s:contour {levels={-1}, x_range={0,255,1}, y_range={0,255,1}, fn=function() return 0 end} end; return s",
                 "scene point limit exceeded");
}

void _contourLabels(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width=320, height=180, fps=4, camera={mode="fixed", height=6}}
local a = s:tracker(0)
local c = s:contour {id="labels", fn=function(x,y) return x+y end, levels={0},
  x_range={-2,2,0.25}, y_range={-2,2,0.25},
  labels={format="%.2f", size=0.2, padding=0.05, upright=true}}
local child = c:children()[1]
assert(#c:children()==1 and #child:children()==1)
s:bind(child, a, function(v) return {fn=function(x,y) return x+y end, level=v} end)
s:track(a, 1, 1, "linear")
local copy = s:clone(child, {id="copy", opacity=0})
assert(#copy:children()==1)
local empty = s:contour {id="empty", fn=function(x,y) return (x*x-1)^2+y*y end,
  levels={20}, x_range={-2,2,0.125}, y_range={-1.5,1.5,0.125}, labels=true, opacity=0}
assert(#empty:children()[1]:children()==0)
s:set {target=empty:children()[1], fn=function(x,y) return (x*x-1)^2+y*y end, level=0.5}
assert(#empty:children()[1]:children()==2)
s:play({target=empty:children()[1], fn=function(x,y) return (x*x-1)^2+y*y end, level=1.5}, 0.5, "linear")
s:play({target=empty:children()[1], fn=function(x,y) return (x*x-1)^2+y*y end, level=-1}, 0.5, "linear")
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "contour-labels.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "contour-labels.lua: %s\n", error);
    if (scene) {
        auto child = static_cast<Plot*>(scene->object("labels/1"));
        auto label = static_cast<Text*>(scene->object("labels/1/label/1"));
        CHECK(child && label && label->parent() == child && label->orientation == TextOrientation::Plane);
        auto copy = static_cast<Plot*>(scene->object("copy"));
        CHECK(copy && copy->contourLabels() && copy->childCount() == 1u);
        CHECK(copy && std::strcmp(copy->contourLabels()->format, "%.2f") == 0);
        CHECK(scene->object("empty/1/label/1") && scene->object("empty/1/label/2"));
        float times[] = {0.5f, 0.0f, 1.0f, 0.25f, 0.5f};
        for (auto time : times) {
            LayoutReport report;
            CHECK(renderer->layout(scene, time, report) == Result::Success);
            auto visible = false;
            for (auto i = 0u; i < report.objectCount(); i++) {
                auto object = report.objectAt(i);
                if (object->object != label) continue;
                visible = object->visible;
                auto x = object->paintBounds.x + object->paintBounds.width * 0.5f;
                auto y = object->paintBounds.y + object->paintBounds.height * 0.5f;
                CHECK(std::fabs(x - y - 70.0f - 30.0f * time) / std::sqrt(2.0f) < 0.6f);
            }
            CHECK(visible);
            char value[32];
            CHECK(scene->text(label, time, value, sizeof(value)) == 4u);
            CHECK(std::strcmp(value, time == 0.0f ? "0.00" : time == 1.0f ? "1.00" : time == 0.25f ? "0.25" : "0.50") == 0);
        }
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:contour{fn=function(x,y) return x+y end, levels={0}, x_range={-1,1,1}, y_range={-1,1,1}, labels=1}; return s", "labels must be a boolean or a table");
    _scriptError("local s=vis.scene{}; s:contour{fn=function(x,y) return x+y end, levels={0}, x_range={-1,1,1}, y_range={-1,1,1}, labels={format='%n'}}; return s", "label format must be %.0f through %.6f");
    _scriptError("local s=vis.scene{}; s:contour{fn=function(x,y) return x+y end, levels={0}, x_range={-1,1,1}, y_range={-1,1,1}, labels={spacing=-1}}; return s", "label spacing and padding must be zero or more");
    _scriptError("local s=vis.scene{}; s:contour{fn=function(x,y) return x+y end, levels={0}, x_range={-1,1,1}, y_range={-1,1,1}, labels={size=0}}; return s", "label size must be positive");
    _scriptError("local s=vis.scene{}; s:contour{fn=function(x,y) return x+y end, levels={0}, x_range={-1,1,1}, y_range={-1,1,1}, labels={extra=true}}; return s", "unknown option 'extra'");
}

void _contourBindings(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {fps = 8}
local a = s:tracker(0.75)
local ellipse = s:contour {id = "ellipse", fn = function(x,y) return x*x/(0.75*0.75)+y*y-1 end,
  levels = {0}, x_range = {-2,2,0.125}, y_range = {-1.5,1.5,0.125}}
s:bind(ellipse:children()[1], {a}, function(v)
  return {fn = function(x,y) return x*x/(v*v)+y*y-1 end}
end)
s:track(a, 1.5, 1, "linear")
local k = s:tracker(0.5)
local function wells(x,y) return (x*x-1)^2+y*y end
local topology = s:contour {id = "topology", fn = wells, levels = {0.5},
  x_range = {-2,2,0.125}, y_range = {-1.5,1.5,0.125}}
local child = topology:children()[1]
s:bind(child, k, function(v) return {fn = wells, level = v} end)
s:track(k, 1.1, 0.25, "linear")
s:track(k, -0.2, 0.25, "linear")
s:track(k, 0.5, 0.25, "linear")
local copy = s:clone(child, {id = "copy", opacity = 0})
s:set {target = copy, values = {0,1,1,2}, level = 0.5, x_range = {0,1,1}, y_range = {0,1,1}}
s:play({target = copy, values = {0,1,1,2}, level = 1.5}, 0.25, "linear")
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "bound-contour.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "bound-contour.lua: %s\n", error);
    if (scene) {
        auto ellipse = scene->object("ellipse/1");
        uint32_t order[] = {4u, 0u, 2u, 1u, 3u};
        for (auto i : order) {
            CHECK(scene->seek(i*0.25f) == Result::Success);
            Object* copy = nullptr;
            CHECK(scene->clone(ellipse, copy) == Result::Success);
            auto plot = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
            CHECK(plot && plot->contours() == 1u && plot->count() > 20u);
            auto a = 0.75 + i*0.1875;
            for (auto j = 0u; plot && j < plot->count(); j++) {
                auto& p = plot->points()[j];
                CHECK(std::fabs(static_cast<double>(p.x)*p.x/(a*a) + static_cast<double>(p.y)*p.y - 1.0) < 1.0e-6);
            }
        }
        float times[] = {1.0f, 1.25f, 1.5f, 1.75f, 1.0f};
        uint32_t pieces[] = {2u, 1u, 0u, 2u, 2u};
        auto topology = scene->object("topology/1");
        for (auto i = 0u; i < 5u; i++) {
            LayoutReport report;
            CHECK(renderer->layout(scene, times[i], report) == Result::Success);
            auto count = 0u;
            for (auto j = 0u; j < report.pathCount(); j++) {
                auto path = report.pathAt(j);
                if (report.objectAt(report.visualAt(path->visual)->object)->object == topology) count++;
            }
            CHECK(count == pieces[i]);
        }
        auto copy = static_cast<Plot*>(scene->object("copy"));
        CHECK(copy && copy->count() == 2u && copy->contours() == 1u);
        CHECK(copy && _near(copy->points()[0].x, 0.5f) && _near(copy->points()[1].y, 0.5f));
    }
    delete scene;
    static constexpr auto prefix = "local s=vis.scene{}; local g=s:contour {values={0,1,1,2}, levels={0.5}, x_range={0,1,1}, y_range={0,1,1}}; local p=g:children()[1]; ";
    auto fails = [](const char* body, const char* expected) {
        char script[1024];
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:set {target=p,level=1}", "contour level needs fn or values");
    fails("s:set {target=p,fn=function(x,y) return x+y end,values={0,1,1,2}}", "either fn or values");
    fails("s:set {target=p,points={{0,0},{1,1}}}", "not points or discontinuities");
    fails("s:set {target=p,fn=function(x,y) return x+y end,level=0/0}", "contour level must be a finite number");
    fails("s:bind(p,s:tracker(0),function(v) return {values={0,1,1,1/0}} end)", "at (1.0, 1.0) one was not");
    fails("s:bind(p,s:tracker(0),function(v) return {fn=function(x,y) s:circle{} return x+y end} end)", "scene authoring is unavailable inside a sampling callback");
    fails("s:bind(p,s:tracker(0),function(v) return {fn=function(x,y) while true do end end} end)", "instruction limit exceeded");
    fails("s:set {target=p,fn=function(x,y) return x+y end,x_range={0,0,1}}", "at least two samples");
    fails("s:set {target=s:plot{points={{0,0},{1,1}}},values={0,1,1,2}}", "level and values need a contour child");
    _scriptError("local s=vis.scene{fps=16}; local g=s:contour {fn=function() return 0 end,levels={-1},x_range={0,255,1},y_range={0,255,1}}; local t=s:tracker(0); s:bind(g:children()[1],t,function(v) return {fn=function() return 0 end} end); s:track(t,1,1); return s", "scene point limit exceeded");
}

void _vectorFields()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local function F(x, y) return -y, x end
s:vector_field {id = "f", fn = F, x_range = {-1, 1, 1}, y_range = {-1, 1, 1}, layer = 2, opacity = 0.5}
s:vector_field {id = "m", fn = function(x, y) return {x, 0} end, x_range = {0, 2, 1}, y_range = {0, 0, 1},
                anchor = "middle", scale = 0.5, colors = {"#000000", "#ffffff"}, tip = 4}
s:vector_field {id = "v", values = {{3, 4}, {0, 1}}, x_range = {0, 1, 1}, y_range = {5, 5, 1}, color = "#ff0000",
                length = 1, width = 3}
s:vector_field {id = "r", fn = F, x_range = {1, 2, 1}, y_range = {0, 0, 1}, magnitude = {0, 4},
                colors = {"#000000", "#ffffff"}}
s:vector_field {id = "n", fn = function(x, y) return x, 0 end, x_range = {-4.2, 4.2, 0.7}, y_range = {0, 0, 1}}
s:vector_field {id = "t", values = {{1e-4, 0}, {1, 0}}, x_range = {0, 1, 1}, y_range = {0, 0, 1}, length = 1}
s:vector_field {id = "k", values = {{1e-4, 0}, {1, 0}}, x_range = {0, 1, 1}, y_range = {0, 0, 1}, scale = 2}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "field.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "field.lua: %s\n", error);
    if (scene) {
        auto& theme = scene->theme();
        auto arrow = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::Vector ? static_cast<Vector*>(object) : nullptr;
        };
        // One arrow for every sample but the origin, where the field vanishes, row by row from
        // the bottom left.
        auto field = scene->object("f");
        CHECK(field && field->type() == Type::Group && field->childCount() == 8u);
        CHECK(field && field->childAt(0) == scene->object("f/1/1") && field->childAt(4) == scene->object("f/3/2"));
        CHECK(!scene->object("f/2/2"));
        CHECK(field && field->layer == 2 && _near(field->opacity, 0.5f));
        // Arrows start at their samples and approach 0.8 of the smaller step as they grow.
        auto corner = arrow("f/1/1");
        auto drawn = 0.8f * std::tanh(std::sqrt(2.0f) / 0.8f);
        CHECK(corner && _near(corner->origin.x, -1.0f) && _near(corner->origin.y, -1.0f));
        CHECK(corner && _near(corner->value.x, drawn / std::sqrt(2.0f)));
        CHECK(corner && _near(corner->value.y, -drawn / std::sqrt(2.0f)));
        CHECK(corner && corner->layer == 2 && _near(corner->opacity, 1.0f) && _near(corner->tip, 10.0f));
        // The greatest magnitude takes the last of the theme's field colors.
        CHECK(corner && _sameColor(corner->style.stroke, theme.color(ThemeColorRole::Danger)));
        auto edge = arrow("f/2/1");
        CHECK(edge && _near(edge->value.x, 0.8f * std::tanh(1.0f / 0.8f)) && _near(edge->value.y, 0.0f));
        // An arrow anchored in the middle is centered on its sample; scale keeps lengths in
        // proportion.
        auto one = arrow("m/2/1");
        auto two = arrow("m/3/1");
        CHECK(scene->object("m") && scene->object("m")->childCount() == 2u);
        CHECK(one && _near(one->value.x, 0.5f) && _near(one->origin.x, 0.75f) && _near(one->tip, 4.0f));
        CHECK(two && _near(two->value.x, 1.0f) && _near(two->origin.x, 1.5f));
        CHECK(one && _sameColor(one->style.stroke, Color{128, 128, 128, 255}));
        CHECK(two && _sameColor(two->style.stroke, Color{255, 255, 255, 255}));
        // Given vectors, and one color for every arrow.
        auto given = arrow("v/1/1");
        CHECK(given && _near(given->value.x, 0.6f * std::tanh(5.0f)) && _near(given->value.y, 0.8f * std::tanh(5.0f)));
        CHECK(given && _sameColor(given->style.stroke, Color{255, 0, 0, 255}) && _near(given->style.width, 3.0f));
        auto up = arrow("v/2/1");
        CHECK(up && _near(up->value.y, std::tanh(1.0f)) && _sameColor(up->style.stroke, Color{255, 0, 0, 255}));
        // magnitude sets the range the colors spread over.
        auto low = arrow("r/1/1");
        auto high = arrow("r/2/1");
        CHECK(low && _sameColor(low->style.stroke, Color{64, 64, 64, 255}));
        CHECK(high && _sameColor(high->style.stroke, Color{128, 128, 128, 255}));
        // A grid symmetric about zero samples the origin a float rounding away from it; the
        // arrow there, and any under a thousandth of the length or of the longest scaled arrow,
        // would be a speck and is left out.
        CHECK(scene->object("n") && scene->object("n")->childCount() == 12u && !scene->object("n/7/1"));
        CHECK(scene->object("t") && scene->object("t")->childCount() == 1u && arrow("t/2/1"));
        CHECK(scene->object("k") && scene->object("k")->childCount() == 1u && arrow("k/2/1"));
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local function F(x, y) return -y, x end; ";
    char script[320];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:vector_field {fn = F, y_range = {0, 1, 1}}", "vector_field requires x_range");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}}", "vector_field requires y_range");
    fails("s:vector_field {x_range = {0, 1, 1}, y_range = {0, 1, 1}}", "vector_field requires either fn or values");
    fails("s:vector_field {values = {{1, 0}}, x_range = {0, 1, 1}, y_range = {0, 1, 1}}",
          "values must hold one {x, y} for each of the 4 samples of x_range and y_range");
    fails("s:vector_field {fn = function(x, y) return x end, x_range = {0, 1, 1}, y_range = {0, 1, 1}}",
          "vector_field fn must return two finite numbers or {x, y}; at (0.0, 0.0) it did not");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}, y_range = {0, 1, 1}, length = 0}",
          "length must be a positive number of world units");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}, y_range = {0, 1, 1}, scale = -1}",
          "scale must be a positive number");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}, y_range = {0, 1, 1}, scale = 1, length = 1}",
          "length and scale cannot be used together");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}, y_range = {0, 1, 1}, anchor = 'tip'}",
          "anchor must be 'tail' or 'middle'");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}, y_range = {0, 1, 1}, colors = {'#000'}}",
          "colors must be a list of 2 to 8 colors");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}, y_range = {0, 1, 1}, magnitude = {1, 1}}",
          "magnitude must be {low, high} with low below high");
    fails("s:vector_field {fn = F, x_range = {0, 100, 1}, y_range = {0, 100, 1}}",
          "a vector field holds at most 1024 arrows; widen the steps of x_range and y_range");
    fails("s:vector_field {fn = F, x_range = {0, 2000, 1}, y_range = {0, 0, 1}}",
          "x_range must hold from 1 to 1024 samples");
    fails("s:vector_field {fn = F, x_range = {0, 1, 1}, y_range = {0, 1, 1}, radius = 1}", "unknown option 'radius'");
}

void _streamLines()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local function G(x, y) return -y, x end
local function U(x, y) return {1, 0} end
s:stream_lines {id = "c", fn = G, seeds = {{1, 0}, {0, 0}, {2, 0}}, time = 2 * math.pi, dt = math.pi / 64,
                colors = {"#000000", "#ffffff"}, layer = 2, width = 3}
s:stream_lines {id = "u", fn = U, x_range = {0, 1, 1}, y_range = {0, 1, 1}, time = 10, dt = 0.25}
s:stream_lines {id = "b", fn = U, seeds = {{0, 0}}, time = 1, dt = 0.5, direction = "backward", color = "#ff0000"}
s:stream_lines {id = "w", fn = U, seeds = {{0, 0}}, time = 1, dt = 0.5, direction = "both"}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "stream.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "stream.lua: %s\n", error);
    if (scene) {
        auto line = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::Plot ? static_cast<Plot*>(object) : nullptr;
        };
        auto at = [](const Plot* plot, uint32_t i, float x, float y) {
            return plot && i < plot->count() && _near(plot->points()[i].x, x) && _near(plot->points()[i].y, y);
        };
        // A line for every seed the field carries, traced for time in steps of dt: a full turn
        // of the rotation field comes back to its seed. The seed where the field vanishes has
        // none.
        auto circles = scene->object("c");
        CHECK(circles && circles->type() == Type::Group && circles->childCount() == 2u && !scene->object("c/2"));
        auto unit = line("c/1");
        CHECK(unit && unit->count() == 129u);
        CHECK(at(unit, 0, 1.0f, 0.0f) && at(unit, 32, 0.0f, 1.0f));
        CHECK(at(unit, 64, -1.0f, 0.0f) && at(unit, 128, 1.0f, 0.0f));
        CHECK(at(line("c/3"), 32, 0.0f, 2.0f));
        // Each line takes the color of its mean speed over the range from zero to the fastest.
        CHECK(unit && _sameColor(unit->style.stroke, Color{128, 128, 128, 255}));
        CHECK(line("c/3") && _sameColor(line("c/3")->style.stroke, Color{255, 255, 255, 255}));
        CHECK(unit && unit->layer == 2 && _near(unit->style.width, 3.0f) && circles && circles->layer == 2);
        // A grid seeds the lines and bounds them: lines stop at its edge, and the seeds on the
        // edge the field leaves by have none.
        auto grid = scene->object("u");
        CHECK(grid && grid->childCount() == 2u && line("u/1") && line("u/3") && !scene->object("u/2"));
        CHECK(line("u/1") && line("u/1")->count() == 5u && at(line("u/1"), 4, 1.0f, 0.0f));
        CHECK(at(line("u/3"), 2, 0.5f, 1.0f));
        CHECK(line("u/1") && _sameColor(line("u/1")->style.stroke, scene->theme().color(ThemeColorRole::Danger)));
        // Backward lines still run with the field, toward their seeds.
        auto back = line("b/1");
        CHECK(back && back->count() == 3u && at(back, 0, -1.0f, 0.0f) && at(back, 2, 0.0f, 0.0f));
        CHECK(back && _sameColor(back->style.stroke, Color{255, 0, 0, 255}));
        auto both = line("w/1");
        CHECK(both && both->count() == 5u && at(both, 0, -1.0f, 0.0f));
        CHECK(at(both, 2, 0.0f, 0.0f) && at(both, 4, 1.0f, 0.0f));
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local function F(x, y) return -y, x end; ";
    char script[320];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:stream_lines {seeds = {{1, 0}}}", "stream_lines requires fn, a function of x and y");
    fails("s:stream_lines {fn = F}", "stream_lines requires seeds, or x_range and y_range to seed a grid");
    fails("s:stream_lines {fn = F, x_range = {0, 1, 1}}", "stream_lines takes both x_range and y_range, or neither");
    fails("s:stream_lines {fn = F, seeds = {}}", "seeds must be a list of {x, y} points");
    fails("s:stream_lines {fn = F, seeds = {1}}", "seeds must be a list of {x, y} points");
    fails("s:stream_lines {fn = F, seeds = {{1, 0}}, time = 0}", "time must be a positive number");
    fails("s:stream_lines {fn = F, seeds = {{1, 0}}, dt = -1}", "dt must be a positive number");
    fails("s:stream_lines {fn = F, seeds = {{1, 0}}, time = 100, dt = 0.01}", "time / dt must be at most 4096 steps");
    fails("s:stream_lines {fn = F, seeds = {{1, 0}}, direction = 'up'}",
          "direction must be 'forward', 'backward' or 'both'");
    fails("s:stream_lines {fn = function() return 'a' end, seeds = {{1, 0}}}",
          "stream_lines fn must return two finite numbers or {x, y}; at (1.0, 0.0) it did not");
    fails("s:stream_lines {fn = F, x_range = {0, 100, 1}, y_range = {0, 100, 1}}",
          "stream_lines draws at most 1024 lines; widen the steps of x_range and y_range");
    fails("s:stream_lines {fn = F, seeds = {{1, 0}}, fill = '#ff0000'}", "unknown option 'fill'");
}

void _shapeMorphs()
{
    // A circle becomes a square and the square a star, each of its own kind and point count.
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local shape = s:circle {id = "circle", radius = 1}
s:wait(0.5)
local square = s:rectangle {id = "square", size = {2, 2}}
s:morph(shape, square, 1)
local points = {}
for k = 0, 9 do
    local a = math.pi / 2 + k * math.pi / 5
    local r = k % 2 == 0 and 1.4 or 0.6
    points[#points + 1] = {r * math.cos(a), r * math.sin(a)}
end
local star = s:polygon {id = "star", points = points}
s:morph(square, star, 1)
local line = s:line {id = "line", from = {-2, -2}, to = {2, -2}}
local graph = s:plot {id = "graph", points = {{2, -1}, {0, -1.5}, {-2, -1}}}
s:morph(line, graph, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "shape-morph.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "shape-morph.lua: %s\n", error);
    if (scene) {
        CHECK(std::fabs(scene->duration() - 3.5f) < 1.0e-4f);
        CHECK(scene->clipCount() == 3u);
        CHECK(scene->object("circle") && scene->object("square") && scene->object("star"));
    }
    delete scene;
}

void _contourPaths()
{
    // Each move starts a contour: a square with a square hole is two closed contours.
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local function square(r)
    return {type = "move", to = {-r, -r}}, {type = "line", to = {r, -r}}, {type = "line", to = {r, r}},
           {type = "line", to = {-r, r}}, {type = "close"}
end
local commands = {square(2)}
for _, command in ipairs({square(1)}) do commands[#commands + 1] = command end
s:path {id = "ring", commands = commands, fill = "#2563eb"}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "ring.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "ring.lua: %s\n", error);
    if (scene) {
        auto ring = scene->object("ring");
        CHECK(ring && ring->type() == Type::Path);
        if (ring && ring->type() == Type::Path) {
            auto path = static_cast<Path*>(ring);
            CHECK(path->contours() == 2u && path->count() == 8u && path->closed());
            CHECK(path->contourEnd(0) == 4u);
        }
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:path{commands={{type='move',to={0,0}},{type='line',to={1,0}},"
                 "{type='line',to={1,1}},{type='close'},{type='line',to={2,2}}}}; return s",
                 "a path command after close must be move, starting the next contour");
    _scriptError("local s=vis.scene{}; s:path{commands={{type='move',to={0,0}},{type='line',to={1,0}},"
                 "{type='line',to={1,1}},{type='close'},{type='move',to={2,2}},{type='line',to={3,2}}}}; return s",
                 "the contours of a path must all close or all stay open");
    _scriptError("local s=vis.scene{}; s:path{commands={{type='move',to={0,0}},{type='move',to={1,0}},"
                 "{type='line',to={2,0}}}}; return s",
                 "every path contour needs at least two points, three if it closes");
}

void _regions()
{
    // Two overlapping squares give each region of a Venn diagram as a path, and a shape in a
    // group combines in the group's units or the scene's.
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local a = s:rectangle {center = {-0.5, 0}, size = {2, 2}}
local b = s:rectangle {center = {0.5, 0}, size = {2, 2}}
s:union {id = "union", of = {a, b}, fill = "#2563eb"}
s:intersection {id = "intersection", of = {a, b}}
s:difference {id = "difference", of = {a, b}}
s:exclusion {id = "exclusion", of = {a, b}}
local holder = s:group {id = "holder", matrix = {2, 0, 0, 3, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}
local inner = holder:rectangle {center = {0, 0}, size = {1, 1}}
local hole = holder:circle {center = {0, 0}, radius = 0.25}
holder:difference {id = "local", of = {inner, hole}}
s:difference {id = "world", of = {inner, hole}}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "regions.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "regions.lua: %s\n", error);
    if (scene) {
        struct Expected { const char* id; uint32_t contours; uint32_t count; float minX; float maxX; };
        Expected regions[] = {{"union", 1u, 4u, -1.5f, 1.5f},     {"intersection", 1u, 4u, -0.5f, 0.5f},
                              {"difference", 1u, 4u, -1.5f, -0.5f}, {"exclusion", 2u, 8u, -1.5f, 1.5f},
                              {"local", 2u, 68u, -0.5f, 0.5f},     {"world", 2u, 68u, 2.0f, 4.0f}};
        for (auto& region : regions) {
            auto object = scene->object(region.id);
            CHECK(object && object->type() == Type::Path);
            if (!object || object->type() != Type::Path) continue;
            auto path = static_cast<Path*>(object);
            Bounds bounds;
            CHECK(path->closed() && path->contours() == region.contours && path->count() == region.count);
            CHECK(path->bounds(bounds) && _near(bounds.min.x, region.minX) && _near(bounds.max.x, region.maxX));
            if (path->contours() != region.contours) std::fprintf(stderr, "  %s: %u contours\n", region.id, path->contours());
        }
        auto joined = scene->object("union");
        CHECK(joined && joined->style.fill.r == 37 && joined->style.fill.g == 99 && joined->style.fill.b == 235);
        auto holder = scene->object("holder");
        auto local = scene->object("local");
        CHECK(holder && local && local->parent() == holder);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; s:union{of={a}}; return s",
                 "union of takes from 2 to 64 shapes");
    _scriptError("local s=vis.scene{}; s:union{}; return s", "union requires of, a list of shapes");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; s:union{of={a, 3}}; return s",
                 "union of must list shape handles");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local l=s:line{from={0,0},to={1,1}};"
                 "s:intersection{of={a, l}}; return s",
                 "intersection takes circles, rectangles, polygons and closed paths");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{center={5,0},radius=1};"
                 "s:intersection{of={a, b}}; return s",
                 "the intersection of these shapes is empty");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=2};"
                 "s:difference{of={a, b}, points={}}; return s",
                 "unknown option 'points'");
}
