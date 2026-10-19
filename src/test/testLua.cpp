#include <clocale>

#include "testLuaChecks.h"

static constexpr auto _scene2d = R"lua(
if type(table) ~= "table" or type(string) ~= "table" or type(math) ~= "table" or type(utf8) ~= "table" then
    error("safe libraries are unavailable")
end
for _, name in ipairs({"io", "os", "package", "debug", "dofile", "loadfile", "load", "require", "pairs", "next", "pcall",
                       "xpcall", "print", "warn", "collectgarbage", "getmetatable", "setmetatable"}) do
    if rawget(_G, name) ~= nil then error("unsafe library is available") end
end
if type(rawget(math, "randomseed")) ~= "function" then error("math.randomseed is unavailable") end
if rawget(string, "find") or rawget(string, "match") or rawget(string, "gmatch") or rawget(string, "gsub") then
    error("unbounded pattern library is available")
end
if undefined_global ~= nil or math.undefined ~= nil or string.undefined ~= nil then error("a missing name is nil") end
if tostring(1.5) ~= "1.5" or string.format("%.1f", 1.5) ~= "1.5" then error("deterministic formatting is unavailable") end
local s = vis.scene {
    width = 320, height = 180, fps = 24, loop = true, background = "#0d1117",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 6}
}
local space = s:space {
    x = {-5, 5, 1}, y = {-3, 3, 1}, axis_x = "#ef5350", axis_y = "#66bb6a", id = "plane"
}
local arrow = space:arrow {
    from = {0, 0}, to = {2.5, 1.5}, tail = 8, tip = 10,
    color = "#ffd166", width = 4, id = "vector"
}
local point = space:point {
    point = {2.5, 1.5}, fill = "#4cc9f0", radius = 7, id = "tip"
}
s:create({space, arrow, point}, 0.75, "linear", 0.1)
s:shift(arrow, {0.5, -0.25}, 0.5, "smooth")
s:fade(point, 0.5, 0.25, "ease_out")
s:wait(0.25)
if math.abs(s:duration() - 1.9) > 0.0001 then error("duration mismatch") end
return s
)lua";

static constexpr auto _scene3d = R"lua(
local s = vis.scene {
    width = 320, height = 180, background = "#0d1117",
    camera = {mode = "fixed", view = "3d",
        eye = {6, 5, 7}, target = {0, 0, 0}, up = {0, 1, 0},
        projection = "perspective", fov = 0.8, near = 0.1, far = 100
    }
}
local space = s:space {
    x = {-3, 3, 1}, y = {-3, 3, 1}, z = {-3, 3, 1}, id = "space"
}
local vector = space:vector {
    value = {2.5, 1.5, 2}, tail = 9, tip = 14,
    color = "#ffd166", width = 4, id = "space-vector"
}
local line = space:line {
    from = {-2, -1, -1}, to = {2, 1, 1}, color = "#4cc9f0", id = "diagonal"
}
s:create({space, vector, line}, 0.6, "smooth", 0.2)
s:shift(vector, {0, 0.5, 0}, 0.4, "linear")
return s
)lua";

static constexpr auto _plot = R"lua(
local s = vis.scene {
    width = 320, height = 180, background = "#0d1117",
    camera = {view = "2d", height = 6}
}
local axes = s:space {x = {-4, 4, 1}, y = {-3, 3, 1}}
local sine = axes:plot {
    fn = function(x) return math.sin(x) end,
    x_range = {-3.14, 3.14, 0.08}, color = "#4cc9f0", width = 3, id = "sine"
}
local samples = axes:plot {
    points = {{-2, 2}, {-1, 0.5}, {0, 0}, {1, 0.5}, {2, 2}},
    color = "#ffd166", width = 2, id = "samples"
}
s:create({axes, sine, samples}, 0.5, "ease_in_out", 0.1)
return s
)lua";

static constexpr auto _route = R"lua(
local s = vis.scene {
    width = 320, height = 180, background = "#0d1117",
    camera = {view = "2d", height = 6}
}
local axes = s:space {x = {-4, 4, 1}, y = {-3, 3, 1}}
local route = axes:route {
    points = {{-3, 1}, {0, 1}, {0, -1}, {3, -1}},
    tail = 10, tip = 18, stroke = "#4cc9f0", width = 4,
    dash = {10, 6}, dash_offset = 0, id = "route"
}
local marker = axes:point {point = {0, 0}, radius = 4, fill = "#ffd166"}
s:create({axes, route, marker}, 0.5, "linear")
s:play({target = route, dash_offset = -8, tail = 14, tip = 22}, 0.5, "linear")
return s
)lua";

static constexpr auto _path = R"lua(
local s = vis.scene {
    width = 320, height = 180, background = "#0d1117",
    camera = {view = "2d", height = 6}
}
local axes = s:space {x = {-4, 4, 1}, y = {-3, 3, 1}}
local outline = axes:path {
    commands = {
        {type = "move", to = {-2.5, -1}},
        {type = "line", to = {-1.7, 1}},
        {type = "quadratic", control = {-0.7, 2}, to = {0, 1}},
        {type = "cubic", control1 = {0.7, 0.2}, control2 = {1.4, -1.7}, to = {2.5, -0.8}},
        {type = "close"}
    },
    samples = 16, stroke = "#4cc9f0", fill = "#4cc9f044", width = 3, id = "outline"
}
local bezier = axes:curve {
    from = {-2.4, -1.8}, control1 = {-0.8, 0.8}, control2 = {0.8, -0.8}, to = {2.4, 1.8},
    samples = 32, stroke = "#ffd166", width = 4, id = "bezier"
}
s:create({axes, outline, bezier}, 0.5, "ease_in_out", 0.1)
return s
)lua";

static constexpr auto _surface = R"lua(
local s = vis.scene {
    width = 320, height = 180, background = "#f7f8fc",
    camera = {view = "3d", eye = {4, 3, 5}, target = {0, 0, 0},
        up = {0, 1, 0}, projection = "perspective", near = 0.1, far = 100}
}
local surface = s:surface {
    points = {{-1, 0, -1}, {1, 0, -1}, {-1, 0.5, 1}, {1, 0.5, 1}},
    size = {2, 2}, mode = "solid", shading = false, fill = "#5271e8", id = "surface"
}
local label = s:text {
    text = "plane", point = {0, -0.2, 1.3}, orientation = "plane",
    size = 0.4, fill = "#172033", id = "plane-label"
}
local contour = s:path {
    commands = {{type="move", to={-1, -1}}, {type="line", to={1, -1}},
        {type="line", to={1, 1}}, {type="line", to={-1, 1}}, {type="close"}},
    fill = "#ef6461", stroke = "#ef6461", id = "contour"
}
s:create(surface, 0.2, "linear")
s:write(contour, 0.4, "gentle")
return s
)lua";


static void _dash(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local scene = vis.scene {
    width = 200, height = 100, background = "#0d1117",
    camera = {view = "2d", height = 4}
}
scene:line {
    from = {-3, 0}, to = {3, 0}, stroke = "#ffffff", width = 2,
    dash = {8, 4}, dash_offset = 2, id = "dashed-line"
}
return scene
)lua";
    auto scene = _loaded(source, "dash.lua");
    if (scene) {
        auto line = scene->object("dashed-line");
        CHECK(line != nullptr);
        if (line) {
            CHECK(line->style.dashCount == 2u);
            CHECK(_near(line->style.dash[0], 8.0f));
            CHECK(_near(line->style.dash[1], 4.0f));
            CHECK(_near(line->style.dashOffset, 2.0f));
        }
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, scene->config().background) > 100u);
    }
    delete scene;

}

static void _render(Renderer* renderer, const char* source, const char* name, const char* tag,
                    Type type, float duration, bool loop, const char* output)
{
    auto scene = _loaded(source, name);
    if (!scene) return;

    CHECK(std::fabs(scene->duration() - duration) < 1.0e-4f);
    CHECK(scene->config().loop == loop);
    CHECK(scene->count() == 3);
    auto object = scene->object(tag);
    CHECK(object != nullptr);
    if (object) CHECK(object->type() == type);

    Surface surface;
    auto status = renderer->render(scene, scene->duration(), surface);
    CHECK(status == Result::Success);
    if (status == Result::Success) {
        CHECK(surface.width() == scene->config().width);
        CHECK(surface.height() == scene->config().height);
        CHECK(changed(surface, scene->config().background) > 100);
        CHECK(Saver::png(surface, output) == Result::Success);
    }
    delete scene;
}

static void _diagnostics()
{
    // The sandbox says why a name is missing.
    _scriptError("local s=vis.scene{}; for k in pairs({}) do end; return s",
                 "pairs is not available in viscript scripts: its order can change between runs; use ipairs");
    _scriptError("local s=vis.scene{}; local f=next; return s", "next is not available in viscript scripts");
    _scriptError("local s=vis.scene{}; print(1); return s", "diagnostic.lua:1: print is not available in viscript scripts");
    _scriptError("local s=vis.scene{}; local u=require('util'); return s",
                 "require is not available in viscript scripts: a scene script is one file");
    _scriptError("local s=vis.scene{}; local t=os.time(); return s", "os is not available in viscript scripts");
    _scriptError("local s=vis.scene{}; setmetatable({}, {}); return s",
                 "setmetatable is not available in viscript scripts: metatables are reserved");
    _scriptError("local s=vis.scene{}; local i=('abc'):find('b'); return s",
                 "string.find is not available in viscript scripts: patterns can run without bound");
    // A seed is a number the script chooses; Lua would take one from the clock.
    _scriptError("local s=vis.scene{}; math.randomseed(); return s",
                 "math.randomseed needs a seed, such as math.randomseed(42), so that every render of a scene matches");
    _scriptError("local s=vis.scene{}; math.randomseed(nil); return s", "math.randomseed needs a seed");
    _scriptError("local s=vis.scene{}; math.randomseed(1.5); return s",
                 "bad argument #1 to 'randomseed' (number has no integer representation)");
    _scriptError("local s=vis.scene{}; math.randomseed(1, 'two'); return s", "bad argument #2 to 'randomseed'");
    // string.format checks its values, then Lua's formats them; an error of either names the line.
    _scriptError("local s=vis.scene{}\nlocal t=string.format('%d', 'x')\nreturn s",
                 "diagnostic.lua:2: bad argument #2 to 'format' (number expected, got string)");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:wait(1); s:remove(c);"
                 " s:shift(c,{1,0},1); return s",
                 "could not play animation: the object was removed earlier in the timeline");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:arrow{from={0,0},to={1,0}};"
                 " s:morph(a,b,1); return s",
                 "morph needs a line, circle, rectangle, polygon, plot, path or curve on both sides");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:plot{points={{0,0},{1,1}}};"
                 " s:morph(a,b,1); return s",
                 "morph outlines must both be closed (circle, rectangle, polygon) or both be open");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=1};"
                 " s:wait(1); a:next_to(b,{1,0}); return s",
                 "could not place object: layout and direct edits must come before the first play");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=1};"
                 " s:fade_in({a,b}); return s",
                 "expected one object handle, got a table");
    _scriptError("local s=vis.scene{}; local g=s:group{}; g:arrange(); return s",
                 "could not arrange group: the group has no children with bounds to arrange");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=1};"
                 " s:fade_in(b); s:fade_transform(a,b,1); return s",
                 "add the replacing object right before the cross-fade");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1};"
                 " s:play({target=a,tip=4},1); return s",
                 "tail and tip need an arrow, vector, connector or route");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; a:next_to(a,{1,0}); return s",
                 "could not place object: the reference must be a different object with the same parent");
    _scriptError("local s=vis.scene{}; s:text{text='a', size=0}; return s", "size must be positive");
}

static void _directRoot()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
scene:point {point = {1, 2}, id = "root-point"}
return scene
)lua";
    auto scene = _loaded(source, "scene-object.lua");
    if (scene) {
        CHECK(scene->count() == 1);
        auto point = scene->object("root-point");
        CHECK(point && point->type() == Type::Point);
        if (point) CHECK(point->parent() == nullptr && point->space() == nullptr);
    }
    delete scene;
}

static void _deterministic()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local world = s:space {}
world:point {point = {math.random(), math.random()}, id = "random"}
return s
)lua";
    auto first = _loaded(source, "random.lua");
    auto second = _loaded(source, "random.lua");
    if (first && second) {
        auto a = static_cast<Point*>(first->object("random"));
        auto b = static_cast<Point*>(second->object("random"));
        CHECK(a && b);
        if (a && b) CHECK(a->point.x == b->point.x && a->point.y == b->point.y);
    }
    delete first;
    delete second;

    // A seed starts math.random over, the same numbers on every platform: these are those of Lua
    // 5.4's xoshiro256** from the seeds, worked out apart from Lua.
    static constexpr auto seededSource = R"lua(
local function draw()
    local values = {}
    for i = 1, 3 do values[#values + 1] = string.format("%.17g", math.random()) end
    for _, range in ipairs({{1, 100}, {1000000}, {-5, 5}, {0}, {3, 3}, {1, 6}}) do
        values[#values + 1] = tostring(math.random(table.unpack(range)))
    end
    return table.concat(values, " ")
end
local unseeded = draw()
local first, second = math.randomseed(42)
if first ~= 42 or second ~= 0 then error("math.randomseed returns its seed") end
local drawn = {draw()}
math.randomseed(42)
if draw() ~= drawn[1] then error("a seed starts the numbers over") end
math.randomseed(42, 7)
drawn[2] = draw()
math.randomseed(-3)
drawn[3] = draw()
math.randomseed(0)
if draw() ~= unseeded then error("the numbers start from the seed 0") end
local expected = {
    "0.93081217803956817 0.45178389935924312 0.54688311243421495 86 545718 1 -5331535219522992527 3 3",
    "0.83191383383816586 0.97213906090725322 0.16884000478541139 16 25461 4 -5019691753703773177 3 2",
    "0.3196627899223482 0.57617040687375631 0.82167927079365166 32 910274 0 1368148269441301287 3 1",
}
for i = 1, 3 do
    if drawn[i] ~= expected[i] then error("seed " .. i .. " drew " .. drawn[i]) end
end
if unseeded ~= "0.2469118419609948 0.23482927841848023 0.069275939529084729 22 158896 2 7965804023693400120 3 1" then
    error("unseeded drew " .. unseeded)
end
return vis.scene {}
)lua";
    delete _loaded(seededSource, "seeded.lua");

    static constexpr auto scaledCameraSource = R"lua(
return vis.scene {
    camera = {view = "3d",
        eye = {0, 0, 0}, target = {0, 0, -1e-7}, up = {0, 1e-7, 0},
        near = 1e-8, far = 2
    }
}
)lua";
    delete _loaded(scaledCameraSource, "scaled-camera.lua");

    static constexpr auto inclusivePlotSource = R"lua(
local s = vis.scene {}
local world = s:space {}
world:plot {fn = function(x) return x end, x_range = {0, 1, 0.1}, id = "inclusive"}
return s
)lua";
    auto inclusivePlot = _loaded(inclusivePlotSource, "inclusive-plot.lua");
    if (inclusivePlot) {
        auto plot = static_cast<Plot*>(inclusivePlot->object("inclusive"));
        CHECK(plot != nullptr);
        if (plot) {
            CHECK(plot->count() == 11);
            CHECK(plot->points()[10].x == 1.0f && plot->points()[10].y == 1.0f);
        }
    }
    delete inclusivePlot;

    static constexpr auto nestedSource = R"lua(
local s = vis.scene {}
local world = s:space {id = "world"}
local frame = world:space {id = "frame", opacity = 0}
frame:point {point = {1, 2, 3}, id = "joint"}
return s
)lua";
    auto nested = _loaded(nestedSource, "nested.lua");
    if (nested) {
        auto world = nested->object("world");
        auto frame = nested->object("frame");
        auto joint = nested->object("joint");
        CHECK(world && frame && joint);
        if (world && frame && joint) CHECK(frame->space() == world && joint->space() == frame);
    }
    delete nested;

    static constexpr auto numberedSource = R"lua(
local s = vis.scene {}
local world = s:space {
    numbers = true, number_mode = "relative", number_size = 13,
    number_color = "#123456", id = "numbered"
}
return s
)lua";
    auto numbered = _loaded(numberedSource, "numbered.lua");
    if (numbered) {
        auto space = static_cast<Space*>(numbered->object("numbered"));
        CHECK(space != nullptr);
        if (space) {
            CHECK(space->numbers);
            CHECK(space->numberMode == NumberMode::Relative);
            CHECK(space->numberSize == 13.0f);
            CHECK(space->numberColor.r == 0x12 && space->numberColor.g == 0x34 && space->numberColor.b == 0x56);
        }
    }
    delete numbered;
}


static void _localeIndependent()
{
    auto current = std::setlocale(LC_NUMERIC, nullptr);
    if (!current || std::strlen(current) >= 128) return;
    char previous[128];
    std::memcpy(previous, current, std::strlen(current) + 1);

    const char* candidates[] = {"de_DE.UTF-8", "fr_FR.UTF-8", "fr_BE.UTF-8", nullptr};
    auto changed = false;
    for (auto candidate = candidates; *candidate; candidate++) {
        if (!std::setlocale(LC_NUMERIC, *candidate)) continue;
        auto info = std::localeconv();
        if (info && info->decimal_point && std::strcmp(info->decimal_point, ".")) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        std::setlocale(LC_NUMERIC, previous);
        return;
    }

    static constexpr auto source = R"lua(
if tostring(1.5) ~= "1.5" then error("localized tostring") end
if "" .. 1.5 ~= "1.5" then error("localized concatenation") end
if string.format("%.1f", 1.5) ~= "1.5" then error("localized format") end
return vis.scene {}
)lua";
    delete _loaded(source, "locale.lua");
    CHECK(std::localeconv() && std::strcmp(std::localeconv()->decimal_point, "."));
    std::setlocale(LC_NUMERIC, previous);
}

static const Asset* _asset(const char* name, void* data)
{
    return std::strcmp(name, "pixels") == 0 ? static_cast<const Asset*>(data) : nullptr;
}

static void _assets(Renderer* renderer)
{
    uint32_t pixels[] = {0xff5040ff, 0xffffa040, 0xff66bb66, 0xffffd166};
    Asset* asset = nullptr;
    CHECK(AssetLoader::load(pixels, 2, 2, asset) == Result::Success);
    if (!asset) return;
    static constexpr auto source = R"lua(
local scene = vis.scene {
    width = 320, height = 180, antialiasing = false,
    camera = {view = "2d", height = 6}
}
local world = scene:space {x = {-3, 3, 1}, y = {-2, 2, 1}, opacity = 0}
local image = world:picture {
    asset = "pixels", center = {1.5, 0}, width = 2, id = "image"
}
local buffer = world:picture {
    pixels = {"#ff4050", "#40a0ff", "#66bb66", "#ffd166"}, size = {2, 2},
    center = {0, 0}, width = 2, filter = "nearest", id = "buffer"
}
local cells = world:cell {
    origin = {-2.5, -1}, size = {2, 2}, padding = 0.05,
    texture = "pixels", source = {0, 0, 2, 2}, destination = {0, 0, 2, 2}, id = "cells"
}
scene:create({image, cells}, 0.4, "linear")
return scene
)lua";
    auto scene = _loaded(source, "assets.lua", _asset, asset);
    if (scene) {
        CHECK(!scene->config().antialiasing);
        CHECK(scene->object("image") && scene->object("image")->type() == Type::Picture);
        CHECK(scene->object("buffer") && scene->object("buffer")->type() == Type::Picture);
        auto buffer = static_cast<Picture*>(scene->object("buffer"));
        CHECK(buffer && buffer->pixelWidth() == 2u && buffer->pixelHeight() == 2u);
        CHECK(buffer && buffer->pixels()[0] == pixel(Color::hex("#ff4050")));
        CHECK(buffer && buffer->pixels()[3] == pixel(Color::hex("#ffd166")));
        CHECK(scene->object("cells") && scene->object("cells")->type() == Type::Cell);
        Surface surface;
        CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        CHECK(changed(surface, scene->config().background) > 2000);
    }
    delete scene;

    _error("local s=vis.scene{}; s:picture{asset='pixels',filter={}}; return s",
           "picture-filter.lua", _asset, asset);
    _error("local s=vis.scene{}; s:picture{asset='pixels',pixels={'#fff'},size={1,1}}; return s",
           "picture-source.lua", _asset, asset);
    _error("local s=vis.scene{}; s:picture{pixels={'#fff'}}; return s",
           "picture-size.lua");
    _error("local s=vis.scene{}; s:picture{pixels={'#fff'},size={2,1}}; return s",
           "picture-pixels.lua");
    _error("local s=vis.scene{}; s:cell{size={2,2},texture='pixels',source={'bad'}}; return s",
           "cell-region.lua", _asset, asset);
    _error("local s=vis.scene{}; s:cell{size={2,2},patches={42}}; return s",
           "cell-patch.lua");
    delete asset;
}

int main()
{
    auto renderer = Renderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
        CHECK(renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
        _render(renderer, _scene2d, "source-2d.lua", "vector", Type::Arrow, 1.9f, true, "test-lua-2d.png");
        _render(renderer, _scene3d, "source-3d.lua", "space-vector", Type::Vector, 1.24f, false, "test-lua-3d.png");
        _render(renderer, _plot, "source-plot.lua", "sine", Type::Plot, 0.6f, false, "test-lua-plot.png");
        _render(renderer, _route, "source-route.lua", "route", Type::Route, 1.0f, false, "test-lua-route.png");
        _render(renderer, _path, "source-path.lua", "outline", Type::Path, 0.6f, false, "test-lua-path.png");
        _render(renderer, _path, "source-curve.lua", "bezier", Type::Curve, 0.6f, false, "test-lua-curve.png");
        _render(renderer, _surface, "source-surface.lua", "surface", Type::SurfaceMesh, 0.6f, false, "test-lua-surface.png");
        _dash(renderer);
        _assets(renderer);
        _viewports(renderer);
        _views(renderer);
        _sceneTransitions(renderer);
        _transitionStyles(renderer);
        _sceneTree(renderer);
        _animationMethods(renderer);
        _dashTimeline(renderer);
        _themes(renderer);
        _cellVoxelBindings(renderer);
        _motion(renderer);
        _diagnostics();
        _crossFades();
        _textSizes();
        _groupStyles();
        _evaluating();
        _composition();
        _arcsAndAngles();
    _shapeProperties();
        _instantSets();
        _plotBreaks();
        _plotEndpoints(renderer);
        _plotEndpointProgress(renderer);
        _plotDiscontinuities();
        _spaceTicksAndLabels();
        _spaceScreenLabels(renderer);
        _spaceLabelIds();
        _areas();
        _riemannSums();
        _surfaceColors();
        _vectorFields();
        _contours();
        _contourBindings(renderer);
        _contourLabels(renderer);
        _streamLines();
        _shapeMorphs();
        _contourPaths();
        _regions();
        _arranging();
        _advecting();
        _textChanges();
        _layering(renderer);
        _savingStates(renderer);
        _pieceMorphs(renderer);
        _surfaceMorph(renderer);
        _emphasizing(renderer);
        _cloning(renderer);
        _copyTransforms(renderer);
        _warping(renderer);
        _warpedClones(renderer);
        _trimmedSegments(renderer);
        _boundGeometry(renderer);
        _springResponses(renderer);
        _longTracks(renderer);
        _replotting(renderer);
        _functionPlay(renderer);
        _animatedPlotGaps(renderer);
        _overlappingTracks(renderer);
        _pendingReads();
        _measuring(renderer);
        _scalingTexts(renderer);
        _slanting(renderer);
        _spanning();
        _spanLeading();
        _baselines();
        _counting(renderer);
        _matchingBillboard(renderer);
    _matchingPath(renderer);
        _matching(renderer);
        _coding();
        _codeBaselines();
        _highlighting();
        _searching();
        _editing();
        _surrounding();
        _boundsQueries();
        _bracing(renderer);
        _framingTexts(renderer);
        _matrices();
        _tables();
    }
    delete renderer;

    _directRoot();
    _tableHelpers();
    _queries();
    _rowQueries();
    _gradients();
    _trackerPrecision();
    _trackerTiming();
    _trackers();
    _trackerLines();
    _timeline();
    _cameraCenters();
    _cameraHome();

    _error("return vis.scene {", "syntax.lua");
    _error("error('runtime sentinel')", "runtime.lua");
    _error("while true do end", "quota.lua");
    _error("return vis.scene {camera={view='2d', heights=8}}", "camera-option.lua");
    _error("return vis.scene {loop=1}", "loop-type.lua");
    _error("local s=vis.scene{}; s:grid{}; return s", "legacy-grid.lua");
    _error("local s=vis.scene{}; s:path{commands={{type='line',to={0,0}},{type='line',to={1,1}}}}; return s", "path-move.lua");
    _error("local s=vis.scene{}; s:path{commands={{type='move',to={0,0}},{type='close'},{type='line',to={1,1}}}}; return s", "path-close.lua");
    _error("local s=vis.scene{}; s:path{commands={{type='move',to={0,0}},{type='arc',to={1,1}}}}; return s", "path-verb.lua");
    _error("local s=vis.scene{}; s:curve{from={0,0},control1={0,1},control2={1,1},to={1,0},samples=0}; return s", "curve-samples.lua");
    _error("local s=vis.scene{}; s:surface{points={{0,0},{1,0}},size={2,1}}; return s", "surface-size.lua");
    _error("local s=vis.scene{}; s:surface{points={{0,0},{1,0},{0,1},{1,1}},size={2,2},mode='cells'}; return s", "surface-mode.lua");
    _error("local s=vis.scene{}; s:surface{points={{0,0},{1,0},{0,1},{1,1}},size={2,2},shading='flat'}; return s", "surface-shading.lua");
    _error("local s=vis.scene{}; s:text{text='x',orientation='wall'}; return s", "text-orientation.lua");
    _error("local s=vis.scene{}; s:text{text='x',role='caption'}; return s", "text-role.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; r:label{}; return s", "label-text.lua");
    _error("return vis.scene{theme='editorial'}", "theme-preset.lua");
    _error("return vis.scene{theme='default'}", "theme-default-renamed.lua");
    _error("return vis.scene{theme={objects={}}}", "theme-palette-empty.lua");
    _error("return vis.scene{theme={object_width=0}}", "theme-object-width.lua");
    _error("return vis.scene{theme={roundness=-0.1}}", "theme-roundness.lua");
    _error("return vis.scene{theme={gradient=1}}", "theme-gradient-type.lua");
    _error("return vis.scene{theme={text={h1={font=''}}}}", "theme-font-empty.lua");
    _error("return vis.scene{theme={axis={w='#fff'}}}", "theme-axis-option.lua");
    _error("local s=vis.scene{}; s:line{from={0,0},to={1,0},width=-1}; return s", "width-negative.lua");
    _error("local s=vis.scene{}; s:text{text='x',gradient=true}; return s", "text-gradient.lua");
    _error("local s=vis.scene{}; s:circle{gradient=1}; return s", "shape-gradient-type.lua");
    _error("local s=vis.scene{}; s:line{dash={}}; return s", "dash-empty.lua");
    _error("local s=vis.scene{}; s:line{dash={1,2,3,4,5}}; return s", "dash-limit.lua");
    _error("local s=vis.scene{}; s:line{dash={0,0}}; return s", "dash-zero.lua");
    _error("local s=vis.scene{}; s:line{dash={4,-1}}; return s", "dash-negative.lua");
    _error("local s=vis.scene{}; s:line{dash_offset=1}; return s", "dash-offset.lua");
    _error("local s=vis.scene{}; s:route{points={{0,0},{1,0}},tip=-1}; return s", "route-tip.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:play({target=r,tip=2}); return s", "play-tip-target.lua");
    _error("local s=vis.scene{}; local a=s:arrow{}; s:play({target=a,tail=-1}); return s", "play-tail-negative.lua");
    _error("local s=vis.scene{}; local l=s:line{}; s:play({target=l,dash_offset=1}); return s", "dash-play-solid.lua");
    _error("local s=vis.scene{}; s:space{matrixx={}}; return s", "object-option.lua");
    _error("local s=vis.scene{}; local g=s:space{}; g:arrow{from={0,0},to={1,0},tail=-1}; return s", "tail.lua");
    _error("local s=vis.scene{}; local g=s:space{}; local a=g:point{}; local b=g:point{}; s:transition(a,b); return s", "transition-opacity.lua");
    _error("local s=vis.scene{}; local a=s:point{}; s:fade_transform(a,a); return s", "fade-transform-same.lua");
    _error("local s=vis.scene{}; local a=s:point{}; local b=s:point{}; s:wait(0.1); s:fade_transform(a,b); return s", "fade-transform-stale.lua");
    _error("local s=vis.scene{}; local r=s:circle{}; s:create(r,1,'linear',0,'sideways'); return s", "create-direction.lua");
    _error("local s=vis.scene{}; local r=s:circle{}; s:uncreate(r,1,'linear',0,{}); return s", "uncreate-direction-type.lua");
    _error("local s=vis.scene{}; local r=s:circle{}; s:create(r,1,'linear',0,'forward'..string.char(0)..'x'); return s", "create-direction-zero.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:fade_in(r,{scale=0}); return s", "fade-in-scale.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:fade_out(r,{unknown=true}); return s", "fade-out-option.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:fade_in(r,0.5); return s", "fade-in-options-type.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:indicate(r,{scale=1}); return s", "indicate-scale.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:indicate(r,{easing='warp'}); return s", "indicate-easing.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:shift(r,{1,0},1,{preset='back',strength=2.1}); return s", "curve-strength.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:shift(r,{1,0},1,{preset='back',reverse=1}); return s", "curve-reverse.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:shift(r,{1,0},1,'smooth'..string.char(0)..'x'); return s", "curve-zero.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:shift(r,{1,0},1,{bezier={-0.1,0,1,1}}); return s", "curve-bezier-x.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:shift(r,{1,0},1,{preset='back',bezier={0,0,1,1}}); return s", "curve-choice.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:fade_in(r,{easing='smooth',curve={preset='back'}}); return s", "curve-alias.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; s:grow_from_center(r,0); return s", "grow-duration.lua");
    _error(R"lua(
local s = vis.scene {}
local a = s:polygon {points = {{0, 0}, {1, 0}, {0, 1}}}
local b = s:plot {points = {{0, 0}, {1, 0}, {1, 1}, {0, 1}}}
s:morph(a, b)
return s
)lua",
           "morph-closed.lua");
    _error(R"lua(
local s = vis.scene {}
local a = s:plot {points = {{0, 0}, {1, 1}}}
local b = s:plot {points = {{0, 0}, {1, -1}}}
s:morph({a}, {b, a})
return s
)lua",
           "morph-array-size.lua");
    _error(R"lua(
local s = vis.scene {}
local a = s:plot {points = {{0, 0}, {1, 1}}}
local b = s:plot {points = {{0, 0}, {1, -1}}}
s:morph({a}, b)
return s
)lua",
           "morph-array-kind.lua");
    _error(R"lua(
local s = vis.scene {}
local a = s:plot {points = {{0, 0}, {1, 1}}}
local b = s:plot {points = {{0, 0}, {1, -1}}}
local c = s:plot {points = {{0, 0}, {1, 0}}}
s:morph({a, a}, {b, c})
return s
)lua",
           "morph-array-duplicate.lua");
    _error(R"lua(
local a = vis.scene {}
local b = vis.scene {}
local source = a:plot {points = {{0, 0}, {1, 1}}}
local target = b:plot {points = {{0, 0}, {1, -1}}}
a:morph(source, target)
return a
)lua",
           "replacement-foreign-target.lua");
    _error(R"lua(
local s = vis.scene {}
local r = s:rectangle {}
s:play({
    {target = r, shift = {1, 0}},
    {target = r, opacity = 0.5}
})
return s
)lua",
           "play-duplicate-target.lua");
    _error(R"lua(
local s = vis.scene {}
s:play({{shift = {1, 0}}})
return s
)lua",
           "play-missing-target.lua");
    _error(R"lua(
local s = vis.scene {}
local r = s:rectangle {}
s:play({target = r})
return s
)lua",
           "play-no-property.lua");
    _error(R"lua(
local s = vis.scene {}
local r = s:rectangle {}
s:play({
    target = r,
    shift = {1, 0},
    transform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}
})
return s
)lua",
           "play-shift-transform.lua");
    _error(R"lua(
local a = vis.scene {}
local b = vis.scene {}
local r = a:rectangle {}
b:play({target = r, shift = {1, 0}})
return b
)lua",
           "play-foreign-target.lua");
    _error(R"lua(
local s = vis.scene {}
local r = s:rectangle {}
s:play({target = r, opacity = 0 / 0})
return s
)lua",
           "play-nonfinite.lua");
    _error(R"lua(
local s = vis.scene {}
local r = s:rectangle {}
s:play({target = r, opacity = 1.1})
return s
)lua",
           "play-opacity.lua");
    _error(R"lua(
local a = vis.scene {}
local b = vis.scene {}
local left = a:rectangle {}
local right = b:rectangle {}
a:connector {from = left, to = right}
return a
)lua",
           "connector-cross-scene.lua");
    _error(R"lua(
local s = vis.scene {}
local left = s:rectangle {center = {-1, 0}}
local right = s:rectangle {center = {1, 0}}
local connector = s:connector {from = left, to = right}
s:shift(connector, {1, 0})
return s
)lua",
           "connector-transform.lua");
    _error(R"lua(
local s = vis.scene {}
local r = s:rectangle {}
r:arrange()
return s
)lua",
           "arrange-non-group.lua");
    _error(R"lua(
local s = vis.scene {}
local r = s:rectangle {}
s:wait(0.1)
r:move_to({1, 0})
return s
)lua",
           "layout-after-timeline.lua");
    _error("local p=vis.scene{}; local c=vis.scene{}; p:viewport(c,{x=.8,width=.5}); return p", "viewport-bounds.lua");
    _error("local p=vis.scene{}; local c=vis.scene{}; p:viewport(c,{}); p:viewport(c,{}); return p", "viewport-owner.lua");
    _error("local p=vis.scene{}; local c=vis.scene{}; p:viewport(c,{}); c:wait(1); return p", "viewport-sealed.lua");
    _error(R"lua(
local parent = vis.scene {}
local child = vis.scene {}
local handle = child:rectangle {}
parent:viewport(child, {})
handle:move_to({1, 0})
return parent
)lua",
           "viewport-sealed-handle.lua");
    _error("local p=vis.scene{}; local c=vis.scene{}; p:viewport(c,{}); return c", "viewport-root.lua");
    _error("local p=vis.scene{}; p:scene_transition({},{}); return p", "transition-empty.lua");
    _error("local p=vis.scene{}; local c=vis.scene{}; p:scene_transition({c,c},{}); return p", "transition-duplicate.lua");
    _error("local p=vis.scene{}; local a=vis.scene{}; local b=vis.scene{}; p:scene_transition({a,b},{hold=-1}); return p", "transition-hold.lua");
    _error("local p=vis.scene{}; local a=vis.scene{}; local b=vis.scene{}; p:scene_transition({a,b},{}); a:wait(1); return p", "transition-sealed.lua");
    _error(R"lua(
local root = vis.scene {}
local left = vis.scene {}
local right = vis.scene {}
for i = 1, 32 do
    left:viewport(vis.scene {}, {})
    right:viewport(vis.scene {}, {})
end
root:viewport(left, {})
root:viewport(right, {})
return root
)lua",
           "viewport-limit.lua");
    _error(R"lua(
local scenes = {}
for i = 1, 66 do scenes[i] = vis.scene {} end
return scenes[1]
)lua",
           "live-scene-limit.lua");
    _error(R"lua(
local s = vis.scene {}
local cursor = s:group {}
for i = 1, 65 do
    cursor = cursor:group {}
end
return s
)lua",
           "incremental-depth.lua");
    _error(R"lua(
local s = vis.scene {}
local g = s:space {}
local p = g:point {point = {0, 0}}
s:shift(p, {1, 2, 3, 4})
return s
)lua",
           "dimension.lua");
    _error(R"lua(
local s = vis.scene {}
local g = s:space {}
g:point {point = {0, 0}, id = "same"}
g:point {point = {1, 1}, id = "same"}
return s
)lua",
           "duplicate-id.lua");
    _error(R"lua(
local s = vis.scene {}
local g = s:space {}
for i = 1, 4097 do g:point {point = {i, 0}} end
return s
)lua",
           "object-limit.lua");
    _error(R"lua(
local a = vis.scene {}
local b = vis.scene {}
local ga = a:space {}
local gb = b:space {}
for i = 1, 2500 do
    ga:point {point = {i, 0}}
    gb:point {point = {-i, 0}}
end
return a
)lua",
           "global-object-limit.lua");
    _error(R"lua(
local s = vis.scene {}
local g = s:space {}
local point = g:point {point = {0, 0}}
local group = {}
for i = 1, 1025 do group[i] = point end
s:create(group)
return s
)lua",
           "group-limit.lua");
    _error(R"lua(
local s = vis.scene {}
local g = s:space {}
g:picture {asset = "formula.svg", color = "#fff"}
return s
)lua",
           "picture-style.lua");
    _error(R"lua(
local s = vis.scene {}
local g = s:space {}
g:point {point = {0, 0}, id = string.char(255)}
return s
)lua",
           "invalid-id.lua");
    _error("return tostring({})", "nondeterministic-tostring.lua");
    _error("return string.format('%p', 'value')", "pointer-format.lua");
    _error("return string.format('%s', {})", "nondeterministic-format.lua");
    _error("local s=vis.scene{}; local g=s:space{}; g:cell{size={0,2}}; return s", "cell-size.lua");
    _error("local s=vis.scene{}; local g=s:space{}; g:voxel(1); return s",
           "voxel-callback.lua");
    _error("local s=vis.scene{}; local g=s:space{}; g:cell(1); return s",
           "cell-callback.lua");
    _error("local s=vis.scene{}; local g=s:space{}; "
           "g:cell(function() return 1 end); return s", "cell-return.lua");
    _error("local s=vis.scene{}; local g=s:space{}; "
           "g:voxel(function() return 1 end); return s", "voxel-return.lua");
    _error("local s=vis.scene{}; local g=s:space{}; "
           "g:voxel(function() error('sample') end); return s", "voxel-error.lua");
    _error("local s=vis.scene{}; local g=s:space{x={0,0,1},y={0,0,1},z={0,0,1}}; "
           "g:voxel(function() s:point{point={0,0}} return '#fff' end); return s",
           "voxel-authoring.lua");
    _error("local s=vis.scene{}; local g=s:space{}; "
           "g:voxel(function() return '#fff' end,{padding=.5}); return s",
           "voxel-padding.lua");
    _error("local s=vis.scene{}; local g=s:space{}; "
           "g:cell(function() return '#fff' end,{duration=-1}); return s",
           "cell-duration.lua");
    _error("local s=vis.scene{}; local g=s:space{}; "
           "g:voxel(function() return '#fff' end,{duration=1,fps=0}); return s",
           "voxel-fps.lua");
    _error("local s=vis.scene{}; local g=s:space{}; "
           "g:voxel(function() return '#fff' end,{duration=200,fps=30}); return s",
           "voxel-frame-limit.lua");
    _error("local s=vis.scene{}; local g=s:space{x={-25,25,1},y={-25,25,1}}; "
           "g:cell(function() return '#fff' end,{duration=4,fps=30}); return s",
           "cell-color-limit.lua");
    _error(R"lua(
local s = vis.scene {}
local g = s:space {x = {0, 0, 1}, y = {0, 0, 1}}
local a = g:cell(function() return '#fff' end)
local b = g:cell(function() return '#000' end)
s:morph(a, b)
return s
)lua",
           "cell-morph.lua");
    _error("local s=vis.scene{}; local g=s:group{}; "
           "g:voxel(function() return '#fff' end); return s", "voxel-space.lua");
    _error("local s=vis.scene{}; "
           "local g=s:space{x={-100,100,1},y={-100,100,1},z={0,0,1}}; "
           "g:voxel(function() return '#fff' end); return s", "voxel-limit.lua");
    _error("local s=vis.scene{}; s:space{number_mode='screen'}; return s", "number-mode.lua");
    _error("local s=vis.scene{}; local r=s:rectangle{}; "
           "s:grow_from_edge(r,'center'); return s", "growth-edge.lua");
    _error("local s=vis.scene{}; s:space{numbers=1}; return s", "numbers-type.lua");
    _error("local s=vis.scene{}; s:line{from={0,0},to={1,1},stroke='unknown'}; return s",
           "theme-color-role.lua");
    _error("local s=vis.scene{theme={colors={accent='red'}}}; return s",
           "theme-semantic-literal.lua");
    _deterministic();
    _localeIndependent();
    return report();
}
