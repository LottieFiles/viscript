#include <cmath>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "vis.h"

using namespace vis;

static auto _failures = 0;

static void _check(bool condition, const char* expression, int line)
{
    if (condition) return;
    std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, line, expression);
    _failures++;
}

#define CHECK(condition) _check((condition), #condition, __LINE__)

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

static uint32_t _changed(const Surface& surface, Color background);
static bool _near(float lhs, float rhs);
static uint32_t _spanTexts(Scene* scene, const char* id, Text** texts, uint32_t capacity);

static void _dash(SwRenderer* renderer)
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
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "dash.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "dash.lua: %s\n", error);
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
        CHECK(_changed(surface, scene->config().background) > 100u);
    }
    delete scene;

}

static uint32_t _changed(const Surface& surface, Color background)
{
    auto pixel = static_cast<uint32_t>(background.a) << 24 | static_cast<uint32_t>(background.b) << 16 | static_cast<uint32_t>(background.g) << 8 | background.r;
    auto count = 0u;
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] != pixel) count++;
        }
    }
    return count;
}

static uint32_t _pixel(Color color)
{
    return static_cast<uint32_t>(color.a) << 24 | static_cast<uint32_t>(color.b) << 16
        | static_cast<uint32_t>(color.g) << 8 | color.r;
}

static bool _near(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 1.0e-4f;
}

static void _render(SwRenderer* renderer, const char* source, const char* name, const char* tag,
                    Type type, float duration, bool loop, const char* output)
{
    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success || !scene) {
        std::fprintf(stderr, "%s: %s\n", name, error);
        delete scene;
        return;
    }

    CHECK(std::fabs(scene->duration() - duration) < 1.0e-4f);
    CHECK(scene->config().loop == loop);
    CHECK(scene->count() == 3);
    auto object = scene->object(tag);
    CHECK(object != nullptr);
    if (object) CHECK(object->type() == type);

    Surface surface;
    status = renderer->render(scene, scene->duration(), surface);
    CHECK(status == Result::Success);
    if (status == Result::Success) {
        CHECK(surface.width() == scene->config().width);
        CHECK(surface.height() == scene->config().height);
        CHECK(_changed(surface, scene->config().background) > 100);
        CHECK(Saver::png(surface, output) == Result::Success);
    }
    delete scene;
}

static void _pngCompression(SwRenderer* renderer)
{
    static constexpr auto source = "return vis.scene {width=960, height=540, background='#112233'}";
    Scene* scene = nullptr;
    char error[512] = {};
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "png.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) return;
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(Saver::png(surface, "test-lua-png.png") == Result::Success);
    auto file = std::fopen("test-lua-png.png", "rb");
    CHECK(file != nullptr);
    if (file) {
        CHECK(std::fseek(file, 0, SEEK_END) == 0);
        auto size = std::ftell(file);
        CHECK(size > 0 && size < 10000);
        std::fclose(file);
    }
    std::remove("test-lua-png.png");
    delete scene;
}

static void _error(const char* source, const char* name, Lua::AssetResolver resolver = nullptr,
                   void* resolverData = nullptr)
{
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error,
                            sizeof(error), resolver, resolverData);
    CHECK(status == Result::ScriptError);
    CHECK(scene == nullptr);
    CHECK(error[0] != '\0');
    auto location = std::strstr(error, name);
    CHECK(location != nullptr);
    if (!location) std::fprintf(stderr, "  %s does not name its script: %s\n", name, error);
    if (location) CHECK(location[std::strlen(name)] == ':');
    delete scene;
}

// The script must fail and the message must explain why.
static void _scriptError(const char* source, const char* expected)
{
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "diagnostic.lua", &scene,
                            error, sizeof(error));
    CHECK(status == Result::ScriptError);
    CHECK(std::strstr(error, expected) != nullptr);
    if (!std::strstr(error, expected)) std::fprintf(stderr, "  expected '%s', got: %s\n", expected, error);
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

// A cross-fade takes its duration and curve as other plays do, a second and smooth when left out.
static void _crossFades()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local a = s:circle {radius = 1}
local b = s:circle {radius = 0.5, opacity = 0}
s:transition(a, b)
local c = s:circle {radius = 0.25, opacity = 0}
s:transition(b, c, 0.5)
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "cross-fades.lua", &scene, error,
                            sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "cross-fades.lua: %s\n", error);
    CHECK(scene && scene->duration() == 1.5f);
    delete scene;
}

// An explicit size is kept as given, and an omitted one follows the theme role.
static void _textSizes()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
scene:text {text = "sized", size = 28, id = "sized"}
scene:text {text = "body", id = "body"}
scene:text {text = "title", role = "h1", id = "title"}
return scene
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "text-size.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "text-size.lua: %s\n", error);
    if (scene) {
        auto theme = Theme::preset(ThemePreset::ProWhite);
        auto sized = static_cast<Text*>(scene->object("sized"));
        auto body = static_cast<Text*>(scene->object("body"));
        auto title = static_cast<Text*>(scene->object("title"));
        CHECK(sized && sized->size == 28.0f);
        CHECK(body && body->size == theme.text.size);
        CHECK(title && title->size == theme.h1.size);
    }
    delete scene;
}

static bool _same(Color first, Color second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b && first.a == second.a;
}

static void _groupStyles()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
local g = scene:group {stroke = "#00ff00", fill = "#ff0000", width = 6, id = "g"}
g:circle {radius = 1, center = {-2, 0}, id = "circle"}
g:rectangle {size = {1, 1}, center = {2, 0}, stroke = "#ffff00", id = "own"}
g:text {text = "label", id = "label"}
scene:play({target = g, fill = "#0000ff"}, 1)
scene:stroke(g, "#ffffff", 0.5)
return scene
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "group-style.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "group-style.lua: %s\n", error);
    if (scene) {
        auto circle = scene->object("circle");
        auto own = scene->object("own");
        auto label = scene->object("label");
        CHECK(circle && _same(circle->style.stroke, Color::hex("#ffffff")));
        CHECK(circle && _same(circle->style.fill, Color::hex("#0000ff")) && circle->style.width == 6.0f);
        CHECK(own && _same(own->style.stroke, Color::hex("#ffff00")) && _same(own->style.fill, Color::hex("#0000ff")));
        CHECK(label && _same(label->style.fill, Theme::preset(ThemePreset::ProWhite).text.color));
    }
    delete scene;

    // The texts of a typeset block take the fill a group in the block sets, as shapes do.
    static constexpr auto blocks = R"lua(
local scene = vis.scene {}
local block = scene:group {typeset = true, fill = "#ff0000", id = "block"}
block:text {text = "set", id = "set"}
local inner = block:group {fill = "#00ff00", id = "inner"}
inner:text {text = "inner", id = "innerText"}
inner:text {text = "own", color = "#ffff00", id = "ownText"}
local outer = scene:group {fill = "#ff00ff", id = "outer"}
outer:group {typeset = true, id = "plain"}:text {text = "plain", id = "plainText"}
local row = scene:spans {"alpha ", {"beta", color = "#00ffff", id = "beta"}, split = "words", id = "row"}
scene:play({target = row, fill = "#0000ff"}, 1)
scene:set {target = inner, fill = "#ffffff"}
return scene
)lua";
    scene = nullptr;
    status = Lua::load(blocks, static_cast<uint32_t>(std::strlen(blocks)), "group-blocks.lua", &scene, error,
                       sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "group-blocks.lua: %s\n", error);
    if (scene) {
        auto role = Theme::preset(ThemePreset::ProWhite).text.color;
        auto text = [scene](const char* tag) { return scene->object(tag); };
        CHECK(text("set") && _same(text("set")->style.fill, Color::hex("#ff0000")));
        CHECK(text("innerText") && _same(text("innerText")->style.fill, Color::hex("#ffffff")));
        CHECK(text("ownText") && _same(text("ownText")->style.fill, Color::hex("#ffff00")));
        // A fill set above the block stays out of it, as it stays out of any text.
        CHECK(text("plainText") && _same(text("plainText")->style.fill, role));
        Text* words[2] = {};
        CHECK(_spanTexts(scene, "row", words, 2) == 2u);
        CHECK(words[0] && _same(words[0]->style.fill, Color::hex("#0000ff")));
        CHECK(words[1] && _same(words[1]->style.fill, Color::hex("#00ffff")));
        // The play adds a clip for the row and its first word, the set one for the group and its text.
        CHECK(scene->clipCount() == 4u);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:group{color='#ff0000'}; return s", "a group draws nothing itself");
    _scriptError("local s=vis.scene{}; s:group{dash={4,2}}; return s", "a group draws nothing itself");
    // Every shape that takes the group's color adds a clip to the script's budget.
    _scriptError("local s=vis.scene{}; local g=s:group{}; for i=1,1000 do g:point{point={0,0}} end;"
                 " for i=1,66 do s:play({target=g, fill=i%2==0 and '#ff0000' or '#00ff00'}, 0.1) end; return s",
                 "scene animation limit exceeded");
}

// A frame reads each clip for its object and for every object below it, and the scene may make
// it read four million: thousands of points with a clip each load, as clips on a group that
// many objects sit in fail the script, whether the objects or the clips come last.
static void _evaluating()
{
    // The points of a Monte Carlo estimate appear one a frame, each set visible at its time.
    static constexpr auto source = R"lua(
local s = vis.scene {fps = 60}
local points = {}
for i = 1, 2000 do
    points[i] = s:point {point = {2 * math.random() - 1, 2 * math.random() - 1}, opacity = 0}
end
for i, p in ipairs(points) do
    s:at(i / 60, function() s:set {target = p, opacity = 1} end)
end
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "points.lua", &scene,
                            error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "points.lua: %s\n", error);
    CHECK(scene && scene->count() == 2000u && scene->clipCount() == 2000u && scene->sampleWork() == 2000u);
    delete scene;
    // Each move of a group of a thousand points is read for the group and each point.
    _scriptError("local s=vis.scene{}; local g=s:group{}; for i=1,1000 do g:point{point={0,0}} end;"
                 " for i=1,4000 do s:play({target=g, shift={0.001, 0}}, 0.01) end; return s",
                 "scene evaluation limit exceeded");
    _scriptError("local s=vis.scene{}; local g=s:group{};"
                 " for i=1,4000 do s:play({target=g, shift={0.001, 0}}, 0.01) end;"
                 " for i=1,1000 do g:point{point={0,0}} end; return s",
                 "scene evaluation limit exceeded");
    // The scenes of viewports add their work to the scene that shows them.
    static constexpr auto nested = R"lua(
local function half()
    local s = vis.scene {}
    local g = s:group {}
    for i = 1, 500 do g:point {point = {0, 0}} end
    for i = 1, 4000 do s:play({target = g, shift = {0.001, 0}}, 0.01) end
    return s
end
local root = vis.scene {}
root:viewport(half(), {x = 0, y = 0, width = 0.5, height = 1})
root:viewport(half(), {x = 0.5, y = 0, width = 0.5, height = 1})
return root
)lua";
    _scriptError(nested, "scene evaluation limit exceeded");
}

// Composition options reach the objects, and wrong values explain themselves.
static void _composition()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
local card = scene:group {isolate = true, opacity = 0.5, id = "card"}
card:rectangle {size = {2, 1}, id = "panel"}
card:text {text = "label", isolate = false, id = "label"}
local hole = scene:circle {radius = 0.5, id = "hole"}
scene:rectangle {size = {2, 2}, mask = hole, id = "image"}
local wipe = scene:rectangle {size = {1, 1}, id = "wipe"}
scene:rectangle {size = {2, 1}, mask = {target = wipe, mode = "inverse_alpha"}, id = "revealed"}
local spot = card:circle {radius = 0.25, id = "spot"}
card:mask(spot, "luma")
scene:circle {radius = 0.5, blend = "color_dodge", id = "shade"}
local card2 = scene:rectangle {size = {2, 1}, shadow = {}, id = "card2"}
local glass = scene:rectangle {size = {2, 1}, blur = 3, shadow = {color = "#ff000080", offset = {6, -2}, blur = 0},
                               id = "glass"}
scene:play({{target = card2, blur = 5, shadow = {offset = {0, -12}}}, {target = glass, shadow = {blur = 10}}}, 1)
return scene
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "composition.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "composition.lua: %s\n", error);
    if (scene) {
        auto card = scene->object("card");
        auto panel = scene->object("panel");
        auto label = scene->object("label");
        CHECK(card && card->isolated());
        CHECK(panel && !panel->isolated());
        CHECK(label && !label->isolated());
        auto image = scene->object("image");
        auto revealed = scene->object("revealed");
        CHECK(image && image->mask() == scene->object("hole") && image->maskMode() == MaskMode::Alpha);
        CHECK(revealed && revealed->mask() == scene->object("wipe")
              && revealed->maskMode() == MaskMode::InverseAlpha);
        CHECK(card && card->mask() == scene->object("spot") && card->maskMode() == MaskMode::Luma);
        auto shade = scene->object("shade");
        CHECK(shade && shade->blend() == BlendMode::ColorDodge);
        CHECK(card && card->blend() == BlendMode::Normal);
        // Objects hold the values at the cursor, after the play.
        auto card2 = scene->object("card2");
        CHECK(card2 && card2->blur == 5.0f && card2->shadow.color.a == 102 && card2->shadow.offset.x == 0.0f
              && card2->shadow.offset.y == -12.0f && card2->shadow.blur == 8.0f);
        auto glass = scene->object("glass");
        CHECK(glass && glass->blur == 3.0f && glass->shadow.color.r == 255 && glass->shadow.color.a == 128
              && glass->shadow.offset.x == 6.0f && glass->shadow.blur == 10.0f);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:group{isolate=1}; return s", "isolate must be a boolean");
    _scriptError("local s=vis.scene{}; s:circle{blur=300}; return s", "blur must be between 0 and 256 pixels");
    _scriptError("local s=vis.scene{}; s:circle{shadow=1}; return s", "shadow must be a table with color, offset and blur");
    _scriptError("local s=vis.scene{}; s:circle{shadow={spread=2}}; return s", "unknown option 'spread'");
    _scriptError("local s=vis.scene{}; s:circle{shadow={blur=-1}}; return s",
                 "shadow blur must be between 0 and 256 pixels");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:play({target=c, blur=-2}); return s",
                 "blur must be between 0 and 256 pixels");
    _scriptError("local s=vis.scene{}; s:circle{blend=1}; return s", "blend must be a string");
    _scriptError("local s=vis.scene{}; s:circle{blend='glow'}; return s", "blend must be 'normal', 'multiply'");
    _scriptError("local s=vis.scene{}; s:rectangle{mask=1}; return s", "expected an object handle, got number");
    _scriptError("local s=vis.scene{}; s:rectangle{mask={mode='alpha'}}; return s", "mask requires a target");
    _scriptError("local s=vis.scene{}; local h=s:circle{}; s:rectangle{mask={target=h, mode='glow'}}; return s",
                 "mask mode must be 'alpha', 'inverse_alpha', 'luma' or 'inverse_luma'");
    _scriptError("local s=vis.scene{}; local h=s:circle{}; s:rectangle{mask={target=h, soft=true}}; return s",
                 "unknown option 'soft'");
    _scriptError("local s=vis.scene{}; local h=s:circle{}; s:rectangle{mask=h}; s:rectangle{mask=h}; return s",
                 "could not mask object: the target already masks another object");
    _scriptError("local s=vis.scene{}; local g=s:group{}; local c=g:circle{}; c:mask(g); return s",
                 "could not mask object: an object cannot be masked by itself or by an object that contains it");
    _scriptError("local s=vis.scene{}; local h=s:circle{}; s:rectangle{mask=h}; h:circle{mask=s:circle{}}; return s",
                 "could not mask object: a mask target and the objects below it cannot be masked");
}

static void _arcsAndAngles()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 200, height = 100, fps = 10, camera = {height = 10, view = "2d"}}
local a = s:arc {radius = 2, start = 0.2, angle = -math.pi, sector = true, fill = "#ffffff", dash = {4, 2}}
local m = s:angle {vertex = {0, 0}, from = {-1, 0.2}, to = {-1, -0.2}, radius = 0.5,
  other = true, right = "never", label_distance = 2, label = {text = "theta", id = "theta"}}
if #m:children() ~= 1 or s:find("theta") ~= m:children()[1] then error("angle label is not retained") end
local t = s:tracker(0)
s:bind(a, t, function(v) return {center = {v, 0}, radius = 2 + v, start = v, angle = math.pi - v} end)
s:bind(m, t, function(v) return {vertex = {v, 0}, from = {v - 1, 0.2}, to = {v - 1, -0.2}} end)
s:track(t, 1, 1, "linear")
return s
)lua";
    Scene* scene = nullptr; char error[1024];
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "arcs-and-angles.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "%s\n", error);
    if (scene) {
        CHECK(scene->objectAt(0)->type() == Type::Arc && scene->objectAt(1)->type() == Type::Angle);
        auto arc = static_cast<Arc*>(scene->objectAt(0));
        auto angle = static_cast<Angle*>(scene->objectAt(1));
        CHECK(std::fabs(arc->center.x - 1.0f) < 1.0e-5f && std::fabs(arc->radius - 3.0f) < 1.0e-5f);
        CHECK(std::fabs(arc->start - 1.0f) < 1.0e-5f && std::fabs(arc->angle - 2.14159265f) < 1.0e-5f);
        CHECK(angle->other && angle->right == RightAngle::Never && std::fabs(angle->vertex.x - 1.0f) < 1.0e-5f);
    }
    delete scene;
    _scriptError("local s = vis.scene {} s:arc {radius = 0} return s", "radius must be positive");
    _scriptError("local s = vis.scene {} s:arc {angle = 7} return s", "angle must be between -2 pi and 2 pi");
    _scriptError("local s = vis.scene {} s:angle {right = 'yes'} return s", "right must be 'auto', 'never' or 'always'");
    _scriptError("local s = vis.scene {} s:angle {label_distance = -1} return s", "label_distance must be zero or more");
    _scriptError("local s = vis.scene {} s:angle {label = 1} return s", "label must be text or a text or tex table");
    _scriptError("local s = vis.scene {} local a = s:circle {} s:play {target = a, start = 1} return s", "start needs an arc and finite radians");
}

static void _shapeProperties()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
local c = scene:circle {radius = 0.5, id = "circle"}
local r = scene:rectangle {size = {1, 1}, id = "box"}
local t = scene:text {text = "size", size = 20, id = "label"}
local p = scene:point {point = {0, 2}, id = "dot"}
local g = scene:group {}
g:line {from = {-1, -2}, to = {1, -2}, id = "heir"}
local bar = scene:rectangle {size = {4, 1}, fill = "#ffffff", id = "bar",
    gradient = {type = "linear", stops = {"#ff0000", "#0000ff"}}}
local ramp = scene:rectangle {size = {4, 1}, fill = "#ff0000", gradient = "#0000ff", id = "ramp"}
scene:play({
    {target = c, radius = 1.5},
    {target = r, size = {3, 2}, corner = 0.5},
    {target = t, size = 40},
    {target = p, radius = 9},
    {target = g, width = 7},
    {target = bar, gradient = {type = "linear", stops = {"#00ff00", "#ffff00"}}},
    {target = ramp, gradient = "#00ff00"},
}, 1)
scene:play({target = r, size = 2}, 1)
return scene
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "shape-properties.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "shape-properties.lua: %s\n", error);
    if (scene) {
        auto circle = static_cast<Circle*>(scene->object("circle"));
        auto box = static_cast<Rectangle*>(scene->object("box"));
        auto label = static_cast<Text*>(scene->object("label"));
        auto dot = scene->object("dot");
        auto heir = scene->object("heir");
        auto bar = scene->object("bar");
        auto ramp = scene->object("ramp");
        CHECK(circle && circle->radius == 1.5f);
        CHECK(box && box->size.x == 2.0f && box->size.y == 2.0f && box->corner == 0.5f);
        CHECK(label && label->size == 40.0f);
        CHECK(dot && dot->style.radius == 9.0f);
        CHECK(heir && heir->style.width == 7.0f);
        CHECK(bar && _same(bar->style.gradient.stops[0].color, Color::hex("#00ff00")));
        CHECK(ramp && _same(ramp->style.gradientEnd, Color::hex("#00ff00")));
    }
    delete scene;
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:play({target=c, size=2}, 1); return s",
                 "could not play animation: size needs a rectangle");
    _scriptError("local s=vis.scene{}; local l=s:line{from={0,0},to={1,0}}; s:play({target=l, radius=1}, 1);"
                 " return s",
                 "could not play animation: radius needs a circle");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:play({target=r, corner=0.8}, 1);"
                 " return s",
                 "half the rectangle's shorter side");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}, gradient={type='linear', stops={'#000', '#fff'}}};"
                 " s:play({target=r, gradient={type='radial', stops={'#000', '#fff'}}}, 1); return s",
                 "keep the current gradient's type");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:play({target=r, size=true}, 1); return s",
                 "size must be a number for text or {width, height} for a rectangle");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:play({target=r, size={1}}, 1); return s",
                 "size must contain exactly 2 numbers");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:play({target=c, radius=0}, 1); return s",
                 "radius must be positive");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:play({target=r, corner=-1}, 1); return s",
                 "corner cannot be negative");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:play({target=c, width=-1}, 1); return s",
                 "width cannot be negative");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:play({target=c, gradient=true}, 1); return s",
                 "gradient must be an end-stop color or a gradient table");
}

static void _instantSets()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local c = s:circle {radius = 0.5, id = "c"}
local r = s:rectangle {size = {1, 1}, id = "r"}
local g = s:group {}
g:line {from = {0, 0}, to = {1, 0}, id = "heir"}
s:set {target = c, opacity = 0.5, radius = 1}
s:play({target = r, fill = "#ff0000"}, 1)
s:set {{target = r, size = {2, 1}}, {target = g, width = 5}}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "instant-set.lua", &scene,
                            error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "instant-set.lua: %s\n", error);
    if (scene) {
        auto circle = static_cast<Circle*>(scene->object("c"));
        auto box = static_cast<Rectangle*>(scene->object("r"));
        auto heir = scene->object("heir");
        CHECK(circle && circle->opacity == 0.5f && circle->radius == 1.0f);
        CHECK(box && box->size.x == 2.0f && box->size.y == 1.0f);
        CHECK(heir && heir->style.width == 5.0f);
        // The play adds one clip; the later set adds one for the rectangle, the group and its line.
        CHECK(scene->clipCount() == 4u && scene->duration() == 1.0f);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:set({target=c, opacity=0.5}, 1); return s",
                 "set takes only descriptors; use play to animate the change");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1};"
                 " s:set{{target=c, opacity=1}, {target=c, fill='#ffffff'}}; return s",
                 "animation targets must be distinct");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:set{target=c, shift={1,0}, arc=1}; return s",
                 "could not set values: the values apply at once, so there is no travel for an arc to carry");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:set{target=c, size=2}; return s",
                 "could not set values: size needs a rectangle");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:fade(c, 0.5, 2); s:seek(1);"
                 " s:set{target=c, opacity=1}; return s",
                 "could not set values: the object already animates its opacity until 2.00 s; begin at that time "
                 "or later");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1, id='dot'};"
                 " s:parallel(function() s:fill(c, 'danger', 2) end, function() s:wait(0.5); s:fill(c, 'success', 1) end);"
                 " return s",
                 "could not play animation: 'dot' already animates its fill color until 2.00 s; begin at that time or later");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1, id='dot'}; s:shift(c, {1,0}, 2); s:seek(1);"
                 " local b=s:rectangle{size={1,1}}; s:morph(c, b, 0.5); return s",
                 "could not play animation: 'dot' already animates its transform until 2.00 s; a morph that begins "
                 "before then must end at that time or later");
}

static void _directRoot()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
scene:point {point = {1, 2}, id = "root-point"}
return scene
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "scene-object.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "scene-object.lua: %s\n", error);
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
    char error[256] = {};
    Scene* first = nullptr;
    Scene* second = nullptr;
    auto size = static_cast<uint32_t>(std::strlen(source));
    CHECK(Lua::load(source, size, "random.lua", &first, error, sizeof(error)) == Result::Success);
    CHECK(Lua::load(source, size, "random.lua", &second, error, sizeof(error)) == Result::Success);
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
    Scene* seeded = nullptr;
    size = static_cast<uint32_t>(std::strlen(seededSource));
    auto status = Lua::load(seededSource, size, "seeded.lua", &seeded, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "seeded.lua: %s\n", error);
    delete seeded;

    static constexpr auto scaledCameraSource = R"lua(
return vis.scene {
    camera = {view = "3d",
        eye = {0, 0, 0}, target = {0, 0, -1e-7}, up = {0, 1e-7, 0},
        near = 1e-8, far = 2
    }
}
)lua";
    Scene* scaledCamera = nullptr;
    size = static_cast<uint32_t>(std::strlen(scaledCameraSource));
    CHECK(Lua::load(scaledCameraSource, size, "scaled-camera.lua", &scaledCamera, error, sizeof(error)) == Result::Success);
    CHECK(scaledCamera != nullptr);
    delete scaledCamera;

    static constexpr auto inclusivePlotSource = R"lua(
local s = vis.scene {}
local world = s:space {}
world:plot {fn = function(x) return x end, x_range = {0, 1, 0.1}, id = "inclusive"}
return s
)lua";
    Scene* inclusivePlot = nullptr;
    size = static_cast<uint32_t>(std::strlen(inclusivePlotSource));
    CHECK(Lua::load(inclusivePlotSource, size, "inclusive-plot.lua", &inclusivePlot, error, sizeof(error)) == Result::Success);
    CHECK(inclusivePlot != nullptr);
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
    Scene* nested = nullptr;
    size = static_cast<uint32_t>(std::strlen(nestedSource));
    CHECK(Lua::load(nestedSource, size, "nested.lua", &nested, error, sizeof(error)) == Result::Success);
    CHECK(nested != nullptr);
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
    Scene* numbered = nullptr;
    size = static_cast<uint32_t>(std::strlen(numberedSource));
    CHECK(Lua::load(numberedSource, size, "numbered.lua", &numbered, error, sizeof(error)) == Result::Success);
    CHECK(numbered != nullptr);
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

// vis.merge and vis.keys stand in for pairs, whose order may change between runs.
static void _tableHelpers()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local defaults = {a = 1, b = 2, size = {1, 2}}
local merged = vis.merge(defaults, nil, {b = 3, c = 4})
if merged.a ~= 1 or merged.b ~= 3 or merged.c ~= 4 or merged.size ~= defaults.size or defaults.b ~= 2 then
    error("merge")
end
local keys = vis.keys({zeta = 1, alpha = 2, [3] = 0, [1] = 0, [2.5] = 0, Beta = 1, [-1] = 0})
local names = {}
for i, key in ipairs(keys) do names[i] = tostring(key) end
if table.concat(names, ",") ~= "-1,1,2.5,3,Beta,alpha,zeta" then error("keys: " .. table.concat(names, ",")) end
if #vis.keys({}) ~= 0 or #vis.keys(vis.merge()) ~= 0 then error("empty keys") end
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "tables.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "tables.lua: %s\n", error);
    delete scene;
    _scriptError("local s=vis.scene{}; vis.keys({[true]=1}); return s",
                 "keys orders only number and string keys, got a boolean key");
    _scriptError("local s=vis.scene{}; vis.keys(1); return s", "(table expected, got number)");
    _scriptError("local s=vis.scene{}; vis.merge({}, 3); return s",
                 "merge accepts only tables and nil, got number for argument 2");
}

// find and children return the handle that created an object, so handles compare and index
// tables by object.
static void _queries()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local card = s:group {id = "card"}
local body = card:rectangle {size = {2, 1}, id = "body"}
local title = card:text {text = "T", id = "title"}
local kids = card:children()
if #kids ~= 2 or kids[1] ~= body or kids[2] ~= title or #body:children() ~= 0 then error("children") end
if s:find("body") ~= body or s:find("card") ~= card or s:find("missing") ~= nil then error("find") end
local seen = {[body] = true}
if not seen[s:find("body")] then error("table key") end
s:find("card"):arrange({1, 0}, 0.2)
s:play({{target = s:find("body"), fill = "#ff0000"}, {target = kids[2], opacity = 0.5}}, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "queries.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "queries.lua: %s\n", error);
    if (scene) CHECK(scene->clipCount() == 2u);
    delete scene;
    _scriptError("local s=vis.scene{}; s:find(); return s", "bad argument #1 to 'find'");
}

// The rows of a group are the groups directly below it, and row, entry and column reach them
// and what they hold, passing over the other objects between them.
static void _rowQueries()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local grid = s:group {id = "grid"}
local a = grid:group {id = "a"}
local between = grid:rectangle {size = {1, 1}, id = "between"}
local b = grid:group {id = "b"}
local a1 = a:text {text = "a1", id = "a1"}
local a2 = a:text {text = "a2", id = "a2"}
local b1 = b:text {text = "b1", id = "b1"}
if grid:row(1) ~= a or grid:row(2) ~= b or grid:row(2.0) ~= b then error("rows are the groups below") end
if grid:entry(1, 2) ~= a2 or grid:entry(2, 1) ~= b1 or grid:entry(1, 1) ~= s:find("a1") then error("entries") end
local first = grid:column(1)
if #first ~= 2 or first[1] ~= a1 or first[2] ~= b1 then error("the first column") end
local second = grid:column(2)
if #second ~= 1 or second[1] ~= a2 then error("a shorter row has no entry there") end
if grid:children()[2] ~= between then error("the rectangle stays a child") end
s:play({{target = grid:entry(1, 2), fill = "#ff0000"}, {target = grid:row(2), opacity = 0.5}}, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "rows.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "rows.lua: %s\n", error);
    if (scene) CHECK(scene->clipCount() == 2u);
    delete scene;
    static constexpr auto grid = "local s=vis.scene{}; local g=s:group{}; local a=g:group{}; a:point{point={0,0}}; "
                                 "a:point{point={1,0}}; g:group{}:point{point={0,1}}; ";
    char script[512];
    auto fails = [&](const char* tail, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s; return s", grid, tail);
        _scriptError(script, expected);
    };
    fails("g:row(3)", "row 3 is past the last of the group's 2 rows");
    fails("g:row(0)", "row takes a whole number from 1");
    fails("g:row(1.5)", "row takes a whole number from 1");
    fails("g:row('1')", "number expected, got string");
    fails("g:entry(2, 2)", "entry 2 is past the last of the 1 objects in row 2");
    fails("g:entry(1, -1)", "entry column takes a whole number from 1");
    fails("g:column(3)", "no row of the group has an object 3");
    fails("s:group{}:row(1)", "the group has no rows; its rows are the groups directly below it");
    fails("a:column(1)", "the group has no rows; its rows are the groups directly below it");
}

// A text measures in its loaded font, and the box it reports holds the glyphs the renderer
// paints for it.
static void _measuring(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local label = s:text {text = "Measured text", point = {0, 1}, size = 28, id = "label"}
local w, h = label:measure()
local longer = s:text {text = "Measured text, longer", point = {0, 0}, size = 28}
local plane = s:text {text = "Measured text", point = {0, -1}, size = 0.5, orientation = "plane"}
local pw, ph = plane:measure()
local _, twoLines = s:text {text = "one\ntwo", point = {0, -2}, size = 28}:measure()
if longer:measure() <= w then error("a longer text is wider") end
if twoLines <= 1.5 * h or twoLines >= 2.5 * h then error("two lines are about twice as tall") end
s:rectangle {size = {w, h}, id = "box"}
s:rectangle {size = {pw, ph}, id = "plane-box"}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "measure.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "measure.lua: %s\n", error);
    if (scene) {
        auto label = static_cast<Text*>(scene->object("label"));
        auto box = static_cast<Rectangle*>(scene->object("box"));
        auto planeBox = static_cast<Rectangle*>(scene->object("plane-box"));
        TextExtent extent;
        CHECK(label && label->measure(extent) == Result::Success);
        CHECK(extent.lines == 1u && extent.ascent > 0.0f && extent.descent > 0.0f);
        CHECK(std::fabs(extent.height - extent.ascent - extent.descent) < 0.01f);
        // Billboard text is measured in pixels; the script sees world units at its camera.
        auto pixelWorld = scene->config().camera.orthoHeight / static_cast<float>(scene->config().height);
        CHECK(box && std::fabs(box->size.x - extent.width * pixelWorld) < 1.0e-4f);
        CHECK(box && std::fabs(box->size.y - extent.height * pixelWorld) < 1.0e-4f);
        // Plane text is measured in world units, in proportion to its size.
        CHECK(planeBox && std::fabs(planeBox->size.x - 0.5f * extent.width / 28.0f) < 1.0e-3f);
        // The glyphs paint inside the advance box and fill most of it.
        BBox ink;
        CHECK(label && renderer->bounds(scene, label, 0.0f, ink) == Result::Success);
        CHECK(ink.width <= extent.width + 1.0f && ink.width > 0.85f * extent.width);
        CHECK(ink.height <= extent.height + 1.0f && ink.height > 0.5f * extent.height);
        // The measured outline box is the painted one, placed from where the baseline starts:
        // the box's left edge, and its top edge one ascent above the baseline.
        auto left = 320.0f - 0.5f * extent.width;
        auto baseline = 180.0f - 1.0f / pixelWorld - 0.5f * extent.height + extent.ascent;
        CHECK(extent.left >= 0.0f && extent.right <= extent.width && extent.bottom <= 0.0f && extent.top > 0.0f);
        CHECK(std::fabs(ink.x - left - extent.left) < 1.0f && std::fabs(ink.width - extent.right + extent.left) < 1.0f);
        CHECK(std::fabs(ink.y - baseline + extent.top) < 1.0f && std::fabs(ink.height - extent.top + extent.bottom) < 1.0f);
        TextExtent world;
        CHECK(scene->measure(label, world) == Result::Success);
        CHECK(std::fabs(world.right - extent.right * pixelWorld) < 1.0e-4f
              && std::fabs(world.bottom - extent.bottom * pixelWorld) < 1.0e-4f);
    }
    delete scene;

    // A measured text lays out by its box: arrange leaves exactly the gap between line boxes.
    static constexpr auto boxed = R"lua(
local s = vis.scene {width = 640, height = 360}
local row = s:group {id = "row"}
row:text {text = "wide words", size = 28, measure = true, id = "wide"}
row:text {text = "x", size = 28, measure = true, align = {0, 0}, id = "narrow"}
row:arrange({1, 0}, 0.1)
s:text {text = "anchor", point = {1, 2}, align = {0, 1}, measure = true, id = "corner"}
s:fade_in(row)
local late = s:text {text = "late", point = {0, -2}, measure = true, id = "late"}
s:play({target = late, opacity = 0.5}, 0.5)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(boxed, static_cast<uint32_t>(std::strlen(boxed)), "boxed.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "boxed.lua: %s\n", error);
    if (scene) {
        auto pixelWorld = scene->config().camera.orthoHeight / static_cast<float>(scene->config().height);
        auto wide = static_cast<Text*>(scene->object("wide"));
        auto narrow = static_cast<Text*>(scene->object("narrow"));
        auto corner = static_cast<Text*>(scene->object("corner"));
        TextExtent extent;
        CHECK(wide && wide->measure(extent) == Result::Success);
        CHECK(wide && std::fabs(wide->box.x - extent.width * pixelWorld) < 1.0e-4f);
        CHECK(wide && std::fabs(wide->box.y - extent.height * pixelWorld) < 1.0e-4f);
        Bounds first;
        Bounds second;
        CHECK(wide && wide->bounds(first) && narrow && narrow->bounds(second));
        CHECK(std::fabs(first.size().x - wide->box.x) < 1.0e-4f);
        CHECK(std::fabs(second.min.x - first.max.x - 0.1f) < 1.0e-4f);
        CHECK(std::fabs(second.center().y - first.center().y) < 1.0e-4f);
        // align pins the box as it pins the glyphs: {0, 1} hangs the box up from the bottom left.
        Bounds anchored;
        CHECK(corner && corner->bounds(anchored));
        CHECK(std::fabs(anchored.min.x - 1.0f) < 1.0e-4f && std::fabs(anchored.min.y - 2.0f) < 1.0e-4f);
        CHECK(corner && std::fabs(anchored.max.y - 2.0f - corner->box.y) < 1.0e-4f);
        // A measured text keeps animating like any other, also one added mid-timeline.
        CHECK(scene->clipCount() == 2u);
        if (scene->clipCount() != 2u) std::fprintf(stderr, "  boxed.lua clips: %u\n", scene->clipCount());
        auto late = static_cast<Text*>(scene->object("late"));
        CHECK(late && late->box.x > 0.0f && late->box.y > 0.0f);

        // The scene measures a detached text in the font and size its theme would give it.
        auto detached = Text::gen("wide words");
        if (detached) detached->size = 28.0f;
        TextExtent world;
        CHECK(detached && scene->measure(detached, world) == Result::Success);
        CHECK(wide && std::fabs(world.width - wide->box.x) < 1.0e-5f && std::fabs(world.height - wide->box.y) < 1.0e-5f);
        if (detached) {
            CHECK(detached->font("Missing") == Result::Success);
            CHECK(scene->measure(detached, world) == Result::InsufficientCondition);
            CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "its font 'Missing' is not loaded"));
        }
        delete detached;
        CHECK(scene->measure(nullptr, world) == Result::InvalidArguments);
    }
    delete scene;

    // A text measures only once it has a font and a size, which attaching gives it.
    auto loose = Text::gen("loose");
    TextExtent extent;
    CHECK(loose && loose->measure(extent) == Result::InvalidArguments);
    if (loose) {
        CHECK(loose->font("Missing") == Result::Success);
        loose->size = 20.0f;
        CHECK(loose->measure(extent) == Result::InsufficientCondition);
        CHECK(loose->font("Pretendard") == Result::Success);
        CHECK(loose->measure(extent) == Result::Success && extent.width > 0.0f);
        CHECK(extent.missing == 0u && extent.firstMissing[0] == '\0');
        // The font has no glyph for the intersection sign, so the text leaves it out.
        auto width = extent.width;
        CHECK(loose->text("loo\xe2\x88\xa9se\xe2\x88\xaa\n") == Result::Success);
        CHECK(loose->measure(extent) == Result::Success && extent.missing == 2u);
        CHECK(std::strcmp(extent.firstMissing, "\xe2\x88\xa9") == 0 && std::fabs(extent.width - width) < 1.0e-3f);
        // A second look finds it missing too, and a joiner, drawn as nothing by design, is not.
        CHECK(loose->text("\xe2\x88\xa9\xe2\x80\x8d") == Result::Success);
        CHECK(loose->measure(extent) == Result::Success && extent.missing == 1u);
        CHECK(loose->text("lo\xa9se") == Result::Success);
        CHECK(loose->measure(extent) == Result::Success && extent.missing == 1u && extent.firstMissing[0] == '\0');
        // A sequence cut short is malformed too, and a C1 control, like a line feed, draws nothing.
        CHECK(loose->text("lo\xe2\x88") == Result::Success);
        CHECK(loose->measure(extent) == Result::Success && extent.missing == 1u && extent.firstMissing[0] == '\0');
        CHECK(loose->text("lo\xc2\x85se") == Result::Success);
        CHECK(loose->measure(extent) == Result::Success && extent.missing == 0u);
    }
    delete loose;
    _scriptError("local s=vis.scene{}; s:text{text='a', font='Missing'}:measure(); return s",
                 "could not measure text: its font 'Missing' is not loaded; load Missing.ttf");
    _scriptError("local s=vis.scene{}; s:circle{radius=1}:measure(); return s", "measure requires a text");
}

// The width of the ink an object paints at time, or zero when it paints none.
static float _inkWidth(SwRenderer* renderer, Scene* scene, const char* id, float time)
{
    auto object = scene->object(id);
    BBox ink;
    if (!object || renderer->bounds(scene, object, time, ink) != Result::Success) return 0.0f;
    return ink.width;
}

static void _scalingTexts(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local label = s:text {text = "Scaled", point = {-4, 1.5}, size = 24, id = "label"}
local turned = s:text {text = "Scaled", point = {0, 1.5}, size = 24, id = "turned"}
local group = s:group {}
group:text {text = "Scaled", point = {4, 1.5}, size = 24, id = "inner"}
local doubled = s:group {matrix = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}
doubled:text {text = "Scaled", point = {-1, -0.25}, size = 24, id = "authored"}
local pulsed = s:text {text = "Scaled", point = {-4, -2}, size = 24, id = "pulsed"}
local plane = s:text {text = "Scaled", point = {2, -2}, size = 0.5, orientation = "plane", id = "plane"}
local grown = s:text {text = "Scaled", point = {-4, -3.5}, size = 24, id = "grown"}
local stretched = s:text {text = "Scaled", point = {0, -3.5}, size = 24, id = "stretched"}
s:parallel(
    function() s:scale(label, 2, {duration = 1}) end,
    function() s:rotate(turned, math.pi / 2, {duration = 1}) end,
    function() s:scale(group, 1.5, {duration = 1}) end,
    function() s:indicate(pulsed, {scale = 1.5, duration = 1}) end,
    function() s:indicate(plane, {scale = 1.5, duration = 1}) end,
    function() s:grow_from_edge(grown, "bottom", 1, "linear") end,
    function() s:scale(stretched, {3, 1.5}, {duration = 1}) end)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "scaled-texts.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "scaled-texts.lua: %s\n", error);
    if (!scene) return;
    auto near = [](float value, float expected) { return std::fabs(value - expected) <= 2.0f; };
    // A billboard grows with a scale of its own or of a group above it, and a turn keeps its size.
    auto label = _inkWidth(renderer, scene, "label", 0.0f);
    CHECK(label > 20.0f);
    CHECK(near(_inkWidth(renderer, scene, "label", 1.0f), 2.0f * label));
    CHECK(near(_inkWidth(renderer, scene, "label", 0.5f), 1.5f * label));
    CHECK(near(_inkWidth(renderer, scene, "turned", 1.0f), label));
    CHECK(near(_inkWidth(renderer, scene, "inner", 0.0f), label));
    CHECK(near(_inkWidth(renderer, scene, "inner", 1.0f), 1.5f * label));
    // The size given is the size seen in the scene as authored, whatever the groups above it scale.
    CHECK(near(_inkWidth(renderer, scene, "authored", 0.0f), label));
    // An effect enlarges a text by its factor once, whichever way the text faces.
    CHECK(near(_inkWidth(renderer, scene, "pulsed", 0.5f), 1.5f * label));
    CHECK(near(_inkWidth(renderer, scene, "grown", 0.5f), 0.5f * label));
    auto plane = _inkWidth(renderer, scene, "plane", 0.0f);
    CHECK(plane > 20.0f);
    CHECK(near(_inkWidth(renderer, scene, "plane", 0.5f), 1.5f * plane));
    CHECK(near(_inkWidth(renderer, scene, "plane", 1.0f), plane));
    // A billboard cannot stretch, so it takes the lesser stretch and still fits its scaled box.
    CHECK(near(_inkWidth(renderer, scene, "stretched", 1.0f), 1.5f * label));
    delete scene;
}

// A slanted text leans about its first baseline: what stands on the baseline stays where it
// is and the top moves right by the slant times its height, billboard or plane, moving or not.
static void _slanting(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
s:text {text = "H", point = {-4, 0}, size = 48, id = "upright"}
local leaning = s:text {text = "H", point = {-2, 0}, size = 48, slant = 0.25, id = "leaning"}
s:text {text = "H", point = {2, 0}, size = 1, orientation = "plane", id = "flat"}
s:text {text = "H", point = {4, 0}, size = 1, orientation = "plane", slant = 0.25, id = "plane"}
s:text {text = "H", point = {-1, -1.5}, size = 48, align = {0, 0}, slant = -0.25, id = "line"}
s:text {text = "H\nH", point = {1, -1.5}, size = 48, align = {0, 0}, slant = -0.25, id = "lines"}
s:text {text = "b", point = {0, 2}, size = 48, slant = 0.25, id = "bowl"}
s:shift(leaning, {0, 1}, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "slanted.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "slanted.lua: %s\n", error);
    if (!scene) return;
    auto pixels = 1.0f / (scene->config().camera.orthoHeight / static_cast<float>(scene->config().height));
    BBox upright;
    BBox leaning;
    BBox flat;
    BBox plane;
    BBox line;
    BBox lines;
    CHECK(renderer->bounds(scene, scene->object("upright"), 0.0f, upright) == Result::Success);
    CHECK(renderer->bounds(scene, scene->object("leaning"), 0.0f, leaning) == Result::Success);
    CHECK(renderer->bounds(scene, scene->object("flat"), 0.0f, flat) == Result::Success);
    CHECK(renderer->bounds(scene, scene->object("plane"), 0.0f, plane) == Result::Success);
    CHECK(std::fabs(leaning.x - upright.x - 2.0f * pixels) < 1.0f && std::fabs(leaning.y - upright.y) < 1.0f);
    CHECK(std::fabs(leaning.width - upright.width - 0.25f * upright.height) < 1.5f);
    CHECK(std::fabs(plane.x - flat.x - 2.0f * pixels) < 1.0f && std::fabs(plane.width - flat.width - 0.25f * flat.height) < 1.5f);
    // A negative slant leans left, about the first line's baseline: the line below moves right.
    CHECK(renderer->bounds(scene, scene->object("line"), 0.0f, line) == Result::Success);
    CHECK(renderer->bounds(scene, scene->object("lines"), 0.0f, lines) == Result::Success);
    CHECK(std::fabs(lines.x - line.x - 2.0f * pixels) < 1.0f && std::fabs(lines.y - line.y) < 1.0f);
    CHECK(lines.width > line.width + 0.25f * upright.height);
    // The outline box measure() reports leans with the glyphs: an H keeps its foot, and its top
    // reaches right by the slant times its height, as painted, on one line or two. The line
    // box leaves the lean out.
    TextExtent standing;
    TextExtent leaned;
    TextExtent two;
    auto uprightText = static_cast<Text*>(scene->object("upright"));
    auto leaningText = static_cast<Text*>(scene->object("leaning"));
    auto linesText = static_cast<Text*>(scene->object("lines"));
    CHECK(uprightText && uprightText->measure(standing) == Result::Success);
    CHECK(leaningText && leaningText->measure(leaned) == Result::Success);
    CHECK(linesText && linesText->measure(two) == Result::Success);
    CHECK(std::fabs(leaned.left - standing.left) < 0.5f);
    CHECK(std::fabs(leaned.right - standing.right - 0.25f * standing.top) < 0.5f);
    CHECK(leaned.width == standing.width && leaned.height == standing.height && leaned.top == standing.top
          && leaned.bottom == standing.bottom);
    CHECK(std::fabs(leaned.right - leaned.left - leaning.width) < 1.0f);
    CHECK(std::fabs(two.right - two.left - lines.width) < 1.0f && two.right - two.left > leaned.right - leaned.left);
    // The top of a b leans past its bowl, so its outlines reach less far right than the corner of
    // its upright box carried by the slant.
    TextExtent bowl;
    TextExtent standingBowl;
    BBox painted;
    auto bowlText = static_cast<Text*>(scene->object("bowl"));
    CHECK(bowlText && bowlText->measure(bowl) == Result::Success);
    CHECK(uprightText && uprightText->measure("b", standingBowl) == Result::Success);
    CHECK(renderer->bounds(scene, bowlText, 0.0f, painted) == Result::Success);
    CHECK(std::fabs(bowl.right - bowl.left - painted.width) < 1.0f);
    CHECK(bowl.right < standingBowl.right + 0.25f * standingBowl.top - 2.0f);
    // A renderer that keeps its paints between frames draws a moving slanted text as a fresh one.
    Surface kept;
    Surface fresh;
    auto other = SwRenderer::gen();
    CHECK(other && renderer->render(scene, 0.0f, kept) == Result::Success);
    CHECK(renderer->render(scene, 0.5f, kept) == Result::Success);
    CHECK(other && other->render(scene, 0.5f, fresh) == Result::Success);
    CHECK(kept.width() == fresh.width() && kept.height() == fresh.height()
          && std::memcmp(kept.data(), fresh.data(), sizeof(uint32_t) * kept.stride() * kept.height()) == 0);
    delete other;
    delete scene;
    _scriptError("local s=vis.scene{}; s:text{text='a', slant=2}; return s", "slant must be from -1 to 1");
    auto text = Text::gen("a");
    scene = Scene::gen();
    if (text) text->slant = 1.5f;
    CHECK(scene && text && scene->add(text) == Result::InvalidArguments);
    delete text;
    delete scene;
}

// The texts of a spans group, in order.
static uint32_t _spanTexts(Scene* scene, const char* id, Text** texts, uint32_t capacity)
{
    auto row = scene->object(id);
    if (!row) return 0;
    auto count = row->childCount();
    for (auto i = 0u; i < count && i < capacity; i++) {
        auto child = row->childAt(i);
        texts[i] = child && child->type() == Type::Text ? static_cast<Text*>(child) : nullptr;
    }
    return count;
}

static void _spanning()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local row = s:spans {"The quick ", {"brown", color = "#a0522d", id = "brown"}, " fox", split = "words",
                     point = {0, 1}, size = 28, id = "row"}
if not rawequal(row:children()[3], s:find("brown")) then error("a part keeps its id") end
if #row:children() ~= 4 then error("words split into four texts") end
s:spans {"a", {"2", rise = 0.35, size = 16, id = "sup"}, " + b", point = {0, -1}, size = 28, id = "math"}
s:spans {"one two three four five six seven", split = "words", wrap = 2.5, size = 28, point = {-3, 0},
         align = {0, 1}, id = "para"}
s:spans {"ab c", split = "letters", point = {0, -2}, size = 28, id = "letters"}
s:spans {"one\n\nthree", point = {3, 0}, size = 28, id = "gap"}
s:spans {"x", {"y", color = "#ff0000"}, color = "#0000ff", opacity = 0.5, size = 28, point = {0, 2}, id = "tint"}
s:spans {"x", {"y", layer = 5}, layer = 3, size = 28, id = "layered"}
s:spans {"\nlead", point = {3, 2}, size = 28, id = "lead"}
s:spans {"abcdefghijklmnopqrstuvwxyz", split = "letters", size = 20, point = {0, 3}, id = "alphabet"}
local named = s:spans {"a b", {"c", id = "own"}, split = "words", size = 28, point = {-3, 3}, id = "named"}
if not rawequal(s:find("named/2"), named:children()[2]) then error("a row names its texts") end
local holder = s:group {id = "holder"}
holder:spans {"inside", {text = "group"}, split = "words", size = 28, id = "nested"}
s:group {typeset = true, id = "block"}
s:wait(1)
local late = s:spans {"late words", split = "words", size = 28, point = {0, -3}, id = "late"}
s:write(late:children(), 1, "smooth", 0.2)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "spans.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "spans.lua: %s\n", error);
    if (scene) {
        // The blank a space leaves and the line a row takes, in the row's size.
        auto blank = Text::gen(" ");
        if (blank) blank->size = 28.0f;
        TextExtent space;
        CHECK(blank && scene->measure(blank, space) == Result::Success);
        // A space has an advance but draws no outline.
        CHECK(space.width > 0.0f && !space.left && !space.right && !space.top && !space.bottom);
        delete blank;
        auto lineHeight = space.ascent + space.descent;
        Text* texts[8] = {};

        // Words sit side by side on one baseline, a space apart, each a measured text.
        CHECK(_spanTexts(scene, "row", texts, 8) == 4u);
        const char* words[] = {"The", "quick", "brown", "fox"};
        for (auto i = 0u; i < 4u; i++) {
            CHECK(texts[i] && std::strcmp(texts[i]->text(), words[i]) == 0);
            if (!texts[i]) continue;
            TextExtent extent;
            CHECK(scene->measure(texts[i], extent) == Result::Success);
            CHECK(std::fabs(texts[i]->box.x - extent.width) < 1.0e-5f);
            CHECK(std::fabs(texts[i]->box.y - extent.ascent - extent.descent) < 1.0e-5f);
            CHECK(std::fabs(texts[i]->point.y - texts[0]->point.y) < 1.0e-5f);
        }
        Bounds rowBounds;
        CHECK(scene->object("row") && scene->object("row")->bounds(rowBounds));
        CHECK(std::fabs(rowBounds.center().x) < 1.0e-4f && std::fabs(rowBounds.center().y - 1.0f) < 1.0e-4f);
        for (auto i = 1u; i < 4u; i++) {
            Bounds before;
            Bounds after;
            CHECK(texts[i - 1] && texts[i - 1]->bounds(before) && texts[i] && texts[i]->bounds(after));
            CHECK(std::fabs(after.min.x - before.max.x - space.width) < 1.0e-4f);
        }
        // A part keeps its color; the others take the theme's text color.
        CHECK(texts[2] && texts[2]->style.fill.r == 0xa0 && texts[2]->style.fill.g == 0x52);
        CHECK(texts[0] && texts[2] && !(texts[0]->style.fill.r == texts[2]->style.fill.r
                                         && texts[0]->style.fill.g == texts[2]->style.fill.g));

        // A risen part sits higher by a share of the line, right after the text before it.
        CHECK(_spanTexts(scene, "math", texts, 8) == 3u);
        if (texts[0] && texts[1] && texts[2]) {
            CHECK(std::fabs(texts[1]->point.y - texts[0]->point.y - 0.35f * lineHeight) < 1.0e-4f);
            CHECK(std::fabs(texts[2]->point.y - texts[0]->point.y) < 1.0e-5f);
            Bounds base;
            Bounds raised;
            Bounds rest;
            CHECK(texts[0]->bounds(base) && texts[1]->bounds(raised) && texts[2]->bounds(rest));
            CHECK(std::fabs(raised.min.x - base.max.x) < 1.0e-4f && std::fabs(rest.min.x - raised.max.x) < 1.0e-4f);
            CHECK(texts[1]->size == 16.0f && texts[2]->size == 28.0f);
        }

        // Wrapped words start new lines at the left edge, and no line passes the wrap width.
        auto count = _spanTexts(scene, "para", texts, 8);
        CHECK(count == 7u);
        auto lines = 1u;
        auto lineLeft = 0.0f;
        for (auto i = 0u; i < count && i < 8u; i++) {
            Bounds bounds;
            CHECK(texts[i] && texts[i]->bounds(bounds));
            if (!texts[i]) continue;
            if (i == 0 || std::fabs(texts[i]->point.y - texts[i - 1]->point.y) > 1.0e-5f) {
                if (i > 0) {
                    lines++;
                    CHECK(std::fabs(texts[i - 1]->point.y - texts[i]->point.y - lineHeight) < 1.0e-4f);
                }
                lineLeft = bounds.min.x;
                CHECK(std::fabs(lineLeft + 3.0f) < 1.0e-4f);
            }
            CHECK(bounds.max.x - lineLeft <= 2.5f + 1.0e-4f);
        }
        CHECK(lines >= 3u);
        // align {0, 1} hangs the rows up from the point, as it hangs a text.
        CHECK(scene->object("para") && scene->object("para")->bounds(rowBounds));
        CHECK(std::fabs(rowBounds.min.y) < 1.0e-4f);
        CHECK(std::fabs(rowBounds.max.y - static_cast<float>(lines) * lineHeight) < 1.0e-3f);

        // Letters split one text per character, and a space still leaves its blank.
        CHECK(_spanTexts(scene, "letters", texts, 8) == 3u);
        if (texts[0] && texts[1] && texts[2]) {
            CHECK(std::strcmp(texts[2]->text(), "c") == 0);
            Bounds b;
            Bounds c;
            CHECK(texts[1]->bounds(b) && texts[2]->bounds(c));
            CHECK(std::fabs(c.min.x - b.max.x - space.width) < 1.0e-4f);
        }

        // Two line feeds leave a blank line between the texts.
        CHECK(_spanTexts(scene, "gap", texts, 8) == 2u);
        if (texts[0] && texts[1]) {
            CHECK(std::fabs(texts[0]->point.y - texts[1]->point.y - 2.0f * lineHeight) < 1.0e-4f);
        }

        // The row's color reaches the parts that set none; its opacity stays on the group.
        CHECK(_spanTexts(scene, "tint", texts, 8) == 2u);
        CHECK(texts[0] && texts[0]->style.fill.b == 0xff && texts[0]->style.fill.r == 0);
        CHECK(texts[1] && texts[1]->style.fill.r == 0xff && texts[1]->style.fill.b == 0);
        CHECK(scene->object("tint") && scene->object("tint")->opacity == 0.5f);
        CHECK(texts[0] && texts[0]->opacity == 1.0f);
        // A group's layer orders no drawings below it, so the row's layer reaches its texts.
        CHECK(_spanTexts(scene, "layered", texts, 8) == 2u);
        CHECK(texts[0] && texts[0]->layer == 3 && texts[1] && texts[1]->layer == 5);
        // Many letters still run left to right, each touching the next.
        Text* letters[26] = {};
        CHECK(_spanTexts(scene, "alphabet", letters, 26) == 26u);
        for (auto i = 1u; i < 26u; i++) {
            Bounds before;
            Bounds after;
            CHECK(letters[i - 1] && letters[i - 1]->bounds(before) && letters[i] && letters[i]->bounds(after));
            CHECK(std::fabs(after.min.x - before.max.x) < 1.0e-4f);
        }
        CHECK(letters[25] && std::strcmp(letters[25]->text(), "z") == 0);
        // With an id, the row names each text that has none of its own after its place.
        CHECK(_spanTexts(scene, "named", texts, 8) == 3u);
        CHECK(texts[0] && texts[0]->tag() && std::strcmp(texts[0]->tag(), "named/1") == 0);
        CHECK(texts[2] && texts[2]->tag() && std::strcmp(texts[2]->tag(), "own") == 0);
        // The row is typeset, so the layout audit reads it as one block of text.
        auto namedRow = scene->object("named");
        CHECK(namedRow && namedRow->type() == Type::Group && static_cast<Group*>(namedRow)->typeset);
        CHECK(scene->object("holder") && !static_cast<Group*>(scene->object("holder"))->typeset);
        CHECK(scene->object("block") && static_cast<Group*>(scene->object("block"))->typeset);
        // A leading line feed leaves a blank first line, as it does in a text.
        CHECK(_spanTexts(scene, "lead", texts, 8) == 1u);
        CHECK(texts[0] && std::fabs(texts[0]->point.y - 2.0f + space.ascent) < 1.0e-4f);

        // Spans nest below a group like any object, and a row added mid-timeline animates.
        auto nested = scene->object("nested");
        CHECK(nested && nested->parent() == scene->object("holder") && nested->childCount() == 2u);
        CHECK(_spanTexts(scene, "late", texts, 8) == 2u);
        CHECK(scene->clipCount() == 2u);
        CHECK(scene->duration() > 2.1f);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; s:spans{}; return s", "spans need at least one string or part");
    _scriptError("local s=vis.scene{}; s:group{typeset=1}; return s", "typeset must be a boolean");
    _scriptError("local s=vis.scene{}; s:spans{'a', stroke='red'}; return s",
                 "spans hold only text, so stroke is unsupported");
    _scriptError("local s=vis.scene{}; s:spans{{'a', width=2}}; return s",
                 "spans hold only text, so width is unsupported");
    _scriptError("local s=vis.scene{}; s:spans{{'a', text='b'}}; return s",
                 "a spans part takes its string first or as text, not both");
    _scriptError("local s=vis.scene{}; s:spans{{size=3}}; return s", "a spans part needs a string, first or as text");
    _scriptError("local s=vis.scene{}; s:spans{{'a', bogus=1}}; return s", "unknown option 'bogus'");
    _scriptError("local s=vis.scene{}; s:spans{'a', split='lines'}; return s",
                 "split must be 'none', 'words' or 'letters'");
    _scriptError("local s=vis.scene{}; s:spans{'a', wrap=-1}; return s", "wrap must be a non-negative width");
    _scriptError("local s=vis.scene{}; s:spans{'  ', split='words'}; return s",
                 "spans need at least one character that is not blank");
    _scriptError("local s=vis.scene{}; s:spans{5}; return s", "a spans part must be a string or a table, got number");
    _scriptError("local s=vis.scene{}; s:spans{[3]='a'}; return s", "spans parts must form a list");
    _scriptError("local s=vis.scene{}; s:spans{'a', font='Missing'}; return s",
                 "could not measure text: its font 'Missing' is not loaded");
    // A formula part needs the tex module, which this script runs without, before any font.
    _scriptError("local s=vis.scene{}; s:spans{'a ', {tex='x^2'}, font='Missing'}; return s",
                 "a tex part needs the tex module: build viscript with -Dmodules=tex");
}

// The baseline of a text, and the ink of a text set alone in a size, in pixels.
static float _baselineOf(Scene* scene, const char* id)
{
    auto object = scene->object(id);
    CHECK(object && object->type() == Type::Text);
    return object && object->type() == Type::Text ? static_cast<Text*>(object)->point.y : 0.0f;
}

static TextExtent _inkOf(Scene* scene, const char* value, float size)
{
    TextExtent extent;
    auto text = Text::gen(value);
    if (text) text->size = size;
    CHECK(text && scene->measure(text, extent) == Result::Success);
    delete text;
    return extent;
}

// leading sets the baselines of a row that many line heights apart, and a line moves down
// only as far as keeps the ink of its texts out of the ink of the line above.
static void _spanLeading()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
s:spans {"one\ntwo three\nfour", split = "words", leading = 2, size = 28, align = {0, 0}, id = "even"}
s:spans {"one two three four five six", split = "words", wrap = 150, leading = 1.25, size = 28,
         point = {0, 200}, align = {0, 1}, id = "wrapped"}
s:spans {"g\n", {"H", size = 84}, "\nx", leading = 1, size = 28, point = {-300, 0}, id = "big"}
s:spans {"a", {"b", rise = -1}, "\nc", leading = 1, size = 28, point = {300, 0}, id = "risen"}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "spans-leading.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "spans-leading.lua: %s\n", error);
    if (scene) {
        auto blank = _inkOf(scene, " ", 28.0f);
        auto lineHeight = blank.ascent + blank.descent;
        // Two line heights from baseline to baseline; align {0, 0} hangs the first line's box
        // from the point.
        CHECK(std::fabs(_baselineOf(scene, "even/1") + blank.ascent) < 1.0e-3f);
        CHECK(std::fabs(_baselineOf(scene, "even/1") - _baselineOf(scene, "even/2") - 2.0f * lineHeight) < 1.0e-3f);
        CHECK(std::fabs(_baselineOf(scene, "even/3") - _baselineOf(scene, "even/2")) < 1.0e-5f);
        CHECK(std::fabs(_baselineOf(scene, "even/2") - _baselineOf(scene, "even/4") - 2.0f * lineHeight) < 1.0e-3f);

        // Wrapped lines keep the pitch, and align {0, 1} stands the last line's box on the point.
        Text* texts[8] = {};
        auto count = _spanTexts(scene, "wrapped", texts, 8);
        CHECK(count == 6u);
        auto lines = 1u;
        for (auto i = 1u; i < count && i < 8u; i++) {
            if (!texts[i] || !texts[i - 1] || std::fabs(texts[i]->point.y - texts[i - 1]->point.y) < 1.0e-5f) continue;
            lines++;
            CHECK(std::fabs(texts[i - 1]->point.y - texts[i]->point.y - 1.25f * lineHeight) < 1.0e-3f);
        }
        CHECK(lines >= 3u);
        if (count == 6u && texts[5]) CHECK(std::fabs(texts[5]->point.y - 200.0f - blank.descent) < 1.0e-3f);

        // A capital three times the size reaches past the pitch, so its line moves down until its
        // ink meets the descender above; the line after it keeps the pitch.
        auto g = _inkOf(scene, "g", 28.0f);
        auto capital = _inkOf(scene, "H", 84.0f);
        CHECK(capital.top - g.bottom > lineHeight);
        CHECK(std::fabs(_baselineOf(scene, "big/1") - _baselineOf(scene, "big/2") - (capital.top - g.bottom)) < 1.0e-3f);
        CHECK(std::fabs(_baselineOf(scene, "big/2") - _baselineOf(scene, "big/3") - lineHeight) < 1.0e-3f);

        // rise moves a text's ink with it: lowered a line, b pushes the line below down.
        auto b = _inkOf(scene, "b", 28.0f);
        auto c = _inkOf(scene, "c", 28.0f);
        CHECK(std::fabs(_baselineOf(scene, "risen/1") - _baselineOf(scene, "risen/2") - lineHeight) < 1.0e-3f);
        CHECK(std::fabs(_baselineOf(scene, "risen/1") - _baselineOf(scene, "risen/3") - (c.top - b.bottom + lineHeight))
              < 1.0e-3f);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; s:spans{'a', leading=0}; return s",
                 "leading must be a positive multiple of the line height");
    _scriptError("local s=vis.scene{}; s:spans{'a', leading=-1}; return s",
                 "leading must be a positive multiple of the line height");
    _scriptError("local s=vis.scene{}; s:spans{'a', leading=1/0}; return s", "leading must be a finite number");
}

// Where a text's first baseline lies as the scene pins it: align places its box, or else the
// text as measured, and the first baseline lies its ascent below the top.
static float _firstBaseline(Scene* scene, const char* id)
{
    auto object = scene->object(id);
    CHECK(object && object->type() == Type::Text);
    if (!object || object->type() != Type::Text) return 0.0f;
    auto text = static_cast<Text*>(object);
    TextExtent extent;
    CHECK(scene->measure(text, extent) == Result::Success);
    auto height = text->box.y > 0.0f ? text->box.y : extent.height;
    return text->point.y + text->align.y * height - extent.ascent;
}

// baseline = true pins the first baseline of a text, number or row at its point, so that
// texts of any size, and formulas, set at one height share a line.
static void _baselines()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
s:text {text = "Baseline", point = {0, 100}, size = 28, baseline = true, id = "plain"}
s:text {text = "Big", point = {200, 100}, size = 60, baseline = true, align = {0, 1}, id = "big"}
s:text {text = "Two\nlines", point = {0, 0}, size = 28, baseline = true, align = {0, 0.5}, id = "two"}
s:text {text = "Boxed", point = {0, -100}, size = 28, baseline = true, measure = true, id = "boxed"}
local dot = s:circle {center = {-300, 0}, radius = 10}
dot:label {text = "Label", point = {-300, 50}, size = 28, baseline = true, id = "label"}
s:number {value = 42, prefix = "n = ", point = {0, -200}, size = 28, baseline = true, id = "count"}
s:spans {"one two", {"three", size = 40}, "\nfour", split = "words", point = {300, 100}, size = 28,
         baseline = true, align = {0, 1}, id = "row"}
s:spans {"x\ny", point = {300, -100}, size = 28, baseline = true, leading = 2, align = {1, 0}, id = "led"}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "baselines.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "baselines.lua: %s\n", error);
    if (scene) {
        CHECK(std::fabs(_firstBaseline(scene, "plain") - 100.0f) < 1.0e-3f);
        CHECK(std::fabs(_firstBaseline(scene, "big") - 100.0f) < 1.0e-3f);
        CHECK(std::fabs(_firstBaseline(scene, "two") - 0.0f) < 1.0e-3f);
        CHECK(std::fabs(_firstBaseline(scene, "boxed") + 100.0f) < 1.0e-3f);
        CHECK(std::fabs(_firstBaseline(scene, "label") - 50.0f) < 1.0e-3f);
        CHECK(std::fabs(_firstBaseline(scene, "count") + 200.0f) < 1.0e-3f);
        // align.x still places a text along its baseline.
        auto big = static_cast<Text*>(scene->object("big"));
        CHECK(big && big->align.x == 0.0f && big->point.x == 200.0f);
        // A row's first baseline lies on the point whatever align and the sizes in it, and align
        // places its rows along it.
        const char* first[] = {"row/1", "row/2", "row/3"};
        for (auto id : first) CHECK(std::fabs(_baselineOf(scene, id) - 100.0f) < 1.0e-5f);
        CHECK(_baselineOf(scene, "row/4") < 100.0f);
        auto one = static_cast<Text*>(scene->object("row/1"));
        CHECK(one && std::fabs(one->point.x - 300.0f) < 1.0e-4f);
        auto blank = _inkOf(scene, " ", 28.0f);
        CHECK(std::fabs(_baselineOf(scene, "led/1") + 100.0f) < 1.0e-5f);
        CHECK(std::fabs(_baselineOf(scene, "led/1") - _baselineOf(scene, "led/2") - 2.0f * (blank.ascent + blank.descent))
              < 1.0e-3f);
        auto x = static_cast<Text*>(scene->object("led/1"));
        CHECK(x && std::fabs(x->point.x + x->box.x - 300.0f) < 1.0e-4f);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; s:text{text='a', baseline=1}; return s", "baseline must be a boolean");
    _scriptError("local s=vis.scene{}; s:number{value=1, baseline='yes'}; return s", "baseline must be a boolean");
    _scriptError("local s=vis.scene{}; s:spans{'a', baseline=1}; return s", "baseline must be a boolean");
    _scriptError("local s=vis.scene{}; s:spans{{'a', baseline=true}}; return s", "unknown option 'baseline'");
}

static void _counting(SwRenderer* renderer)
{
    {
        constexpr auto source = R"lua(
local s=vis.scene{width=320,height=180,camera={height=180}}
local n=s:number{value=1,tabular=true,reserve="0000",align={1,0.5},id="n"}
s:text{text="1 9",tabular=true,measure=true,id="t"}
s:group{}:label{text="0",tabular=true,id="l"}
s:matrix{rows={{1,"9"}},tabular=true,reserve="0000",id="m"}
s:play({target=n,value=1000},1,"linear")
return s
)lua";
        Scene* scene = nullptr;
        char error[256] = {};
        CHECK(Lua::load(source, std::strlen(source), "tabular.lua", &scene, error, sizeof(error)) == Result::Success);
        if (scene) {
            auto n = static_cast<Text*>(scene->object("n"));
            TextExtent one, nine;
            CHECK(n && n->tabular);
            CHECK(n->measure("1", one) == Result::Success && n->measure("9", nine) == Result::Success);
            CHECK(_near(one.width, nine.width) && _near(n->box.x, 4.0f * one.width));
            CHECK(static_cast<Text*>(scene->object("t"))->tabular);
            CHECK(static_cast<Text*>(scene->object("l"))->tabular);
            const char* ids[] = {"m/1/1", "m/1/2"};
            for (auto id : ids) {
                auto entry = static_cast<Text*>(scene->object(id));
                CHECK(entry->tabular && _near(entry->box.x, n->box.x));
            }
            Surface frame;
            CHECK(renderer->render(scene, 0.5f, frame) == Result::Success);
            delete scene;
        }
        _scriptError("local s=vis.scene{}; s:number{tabular=1}; return s", "tabular must be a boolean");
        _scriptError("local s=vis.scene{}; s:matrix{rows={{1}},tabular='yes'}; return s", "tabular must be a boolean");
    }
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local counter = s:number {value = 0, separator = ",", point = {0, 1}, size = 28, id = "counter"}
local label = s:number {value = -2, decimals = 2, prefix = "x = ", suffix = " m", sign = true, size = 28,
                        point = {0, -1}, id = "label"}
local start = s:number {value = 7, size = 28, point = {0, -2}, measure = true, id = "start"}
s:set {target = start, value = 42}
s:text {text = "500", point = {3, 1}, size = 28, id = "half"}
s:text {text = "1,000", point = {3, -2}, size = 28, id = "whole"}
local x = s:tracker(-2)
s:bind(label, x, function(v) return {value = v, shift = {v, 0}} end)
s:play({target = counter, value = 1000}, 2, "linear")
s:track(x, 1.5, 1, "linear")
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "number.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "number.lua: %s\n", error);
    if (scene) {
        auto counter = static_cast<Text*>(scene->object("counter"));
        auto label = static_cast<Text*>(scene->object("label"));
        auto start = static_cast<Text*>(scene->object("start"));
        CHECK(counter && counter->counting() && std::strcmp(counter->text(), "1,000") == 0);
        CHECK(label && label->counting() && std::strcmp(label->text(), "x = +1.50 m") == 0);
        // set gives the starting value before the timeline; measure boxes the number it starts with.
        CHECK(start && start->value() == 42.0f && std::strcmp(start->text(), "42") == 0);
        CHECK(start && start->box.x > 0.0f && start->box.y > 0.0f);
        // Halfway through, the counter writes the number it has reached.
        BBox counted;
        BBox expected;
        CHECK(counter && renderer->bounds(scene, counter, 1.0f, counted) == Result::Success);
        CHECK(renderer->bounds(scene, scene->object("half"), 1.0f, expected) == Result::Success);
        CHECK(std::fabs(counted.width - expected.width) < 0.5f);
        CHECK(renderer->bounds(scene, counter, 2.0f, counted) == Result::Success);
        CHECK(renderer->bounds(scene, scene->object("whole"), 2.0f, expected) == Result::Success);
        CHECK(std::fabs(counted.width - expected.width) < 0.5f);
        // The bound label counts with its tracker and moves with it.
        BBox before;
        BBox after;
        CHECK(label && renderer->bounds(scene, label, 0.0f, before) == Result::Success);
        CHECK(renderer->bounds(scene, label, 3.0f, after) == Result::Success);
        CHECK(after.x > before.x + 50.0f);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:play({target=c, value=3}, 1); return s",
                 "value needs a number from scene:number");
    _scriptError("local s=vis.scene{}; local t=s:text{text='7'}; s:set{target=t, value=3}; return s",
                 "value needs a number from scene:number");
    _scriptError("local s=vis.scene{}; s:number{decimals=7}; return s", "decimals must be a whole number from 0 to 6");
    _scriptError("local s=vis.scene{}; s:number{decimals=1.5}; return s", "decimals must be a whole number from 0 to 6");
    _scriptError("local s=vis.scene{}; s:number{separator=',,'}; return s",
                 "separator must be one printable ASCII character");
    _scriptError("local s=vis.scene{}; s:number{text='7'}; return s", "unknown option 'text'");
    _scriptError("local s=vis.scene{}; local x=s:tracker(0); local t=s:text{text='7'}; "
                 "s:bind(t, x, function(v) return {value=v} end); return s",
                 "value needs a number from scene:number");
}

// The string a text shows at time, or "?" when it is longer than the buffer.
static const char* _shows(Scene* scene, const char* id, float time)
{
    static char output[64];
    auto text = static_cast<const Text*>(scene->object(id));
    if (!text || scene->text(text, time, output, sizeof(output)) >= sizeof(output)) return "?";
    return output;
}

static void _textChanges()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180}
local word = s:text {text = "cat", point = {0, 1}, size = 24, id = "word"}
local box = s:rectangle {size = {1, 1}, id = "box"}
box:label {text = "Hello world", point = {0, -1}, size = 18, id = "tag"}
local start = s:text {text = "x", point = {0, -2}, size = 18, id = "start"}
s:set {target = start, text = "abc"}
s:play({{target = word, text = "horse", shift = {1, 0}},
        {target = s:find("tag"), text = "Hello there", retype = true}}, 1, "linear")
s:at(0.5, function() s:fade(word, 0.5, 1) end)
s:set {target = start, text = "done"}
s:play({target = word, text = ""}, 1, "linear")
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "text.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "text.lua: %s\n", error);
    if (scene) {
        // Characters turn from the left while the length grows, and the fade overlaps them.
        CHECK(std::strcmp(_shows(scene, "word", 0.0f), "cat") == 0);
        CHECK(std::strcmp(_shows(scene, "word", 0.25f), "hat") == 0);
        CHECK(std::strcmp(_shows(scene, "word", 0.5f), "hots") == 0);
        CHECK(std::strcmp(_shows(scene, "word", 1.0f), "horse") == 0);
        CHECK(std::strcmp(_shows(scene, "word", 1.5f), "hor") == 0);
        CHECK(std::strcmp(_shows(scene, "word", 2.0f), "") == 0);
        // The label erases back to what both strings begin with, then types.
        CHECK(std::strcmp(_shows(scene, "tag", 0.25f), "Hello wor") == 0);
        CHECK(std::strcmp(_shows(scene, "tag", 0.5f), "Hello ") == 0);
        CHECK(std::strcmp(_shows(scene, "tag", 0.75f), "Hello th") == 0);
        // set gives the starting string before the timeline and switches it at the cursor after.
        CHECK(std::strcmp(_shows(scene, "start", 0.0f), "abc") == 0);
        CHECK(std::strcmp(_shows(scene, "start", 0.99f), "abc") == 0);
        CHECK(std::strcmp(_shows(scene, "start", 1.0f), "done") == 0);
        auto word = static_cast<Text*>(scene->object("word"));
        CHECK(word && std::strcmp(word->text(), "") == 0);
        const char* texts[4] = {};
        CHECK(word && word->texts(texts, 4u) == 3u);
        CHECK(texts[0] && texts[2] && std::strcmp(texts[0], "cat") == 0 && std::strcmp(texts[2], "") == 0);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:play({target=c, text='a'}, 1); return s",
                 "text needs a text or label");
    _scriptError("local s=vis.scene{}; local n=s:number{value=1}; s:play({target=n, text='a'}, 1); return s",
                 "text cannot change a number from scene:number; animate its value");
    _scriptError("local s=vis.scene{}; local g=s:group{typeset=true}; g:text{text='a'}; "
                 "s:play({target=g, text='b'}, 1); return s",
                 "text changes one text, and a spans row or other block of texts holds several");
    _scriptError("local s=vis.scene{}; local t=s:text{text='a'}; s:play({target=t, text=5}, 1); return s",
                 "text must be a string");
    _scriptError("local s=vis.scene{}; local t=s:text{text='a'}; s:set{target=t, text='a\\0b'}; return s",
                 "text cannot contain zero bytes");
    _scriptError("local s=vis.scene{}; local t=s:text{text='a'}; "
                 "s:play({target=t, text=string.rep('a', 1048577)}, 1); return s",
                 "text exceeds the 1 MiB limit");
    _scriptError("local s=vis.scene{}; local t=s:text{text='a'}; s:play({target=t, opacity=0, retype=true}, 1); "
                 "return s",
                 "retype needs text");
    _scriptError("local s=vis.scene{}; local t=s:text{text='a'}; s:play({target=t, text='b', retype=1}, 1); return s",
                 "retype must be a boolean");
    _scriptError("local s=vis.scene{}; local t=s:text{text='a'}; s:play({target=t, text='b'}, 2); "
                 "s:at(1, function() s:play({target=t, text='c'}, 1) end); return s",
                 "already animates its text until 2.00 s");
    _scriptError("local s=vis.scene{}; local x=s:tracker(0); local t=s:text{text='7'}; "
                 "s:bind(t, x, function(v) return {text='a'} end); return s",
                 "a bound state cannot set 'text'");
#ifdef VIS_TEST_FONT
    _scriptError("local s=vis.scene{}; local r=s:spans{'a ', 'b', size=28}; s:play({target=r, text='c'}, 1); "
                 "return s",
                 "text changes one text, and a spans row or other block of texts holds several");
#endif
}

static bool _sameColor(const Color& first, const Color& second);

static bool _tagged(const Object* object, const char* tag)
{
    return object && object->tag() && std::strcmp(object->tag(), tag) == 0;
}

static void _cloning(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180}
local row = s:group {id = "row"}
local a = row:circle {center = {-1, 0}, radius = 0.5, id = "row/a"}
local b = row:rectangle {center = {1, 0}, size = {1, 1}, stroke = "#0000ff", id = "b"}
local word = s:text {text = "cat", point = {0, 2}, id = "word"}
local n = s:number {value = 0, point = {2, -2}, id = "n"}
local r = s:rectangle {center = {-3, -2}, size = {2, 1}, id = "r"}
local twin, copies = s:clone(row)
if twin ~= s:find("row:copy") or copies[row] ~= twin then error("the copy of the source") end
if copies[a] ~= s:find("row:copy/a") or copies[b] ~= s:find("row:copy/b") then error("the copies below") end
if copies[word] ~= nil or copies[1] ~= nil then error("only objects below the source have copies") end
if #twin:children() ~= 2 then error("a group copies its children") end
if s:clone(r) ~= s:find("r:copy") or s:clone(r) ~= s:find("r:copy2") then error("a taken id takes a number") end
s:clone(row, {id = "pair", stroke = "#ff0000", shift = {0, -2}, opacity = 0.5})
local inside = s:clone(a, {parent = twin})
if inside ~= s:find("row/a:copy") or #twin:children() ~= 3 then error("the given parent") end
s:clone(word, {id = "masked", mask = r})
s:play({{target = word, text = "horse"}, {target = n, value = 100}, {target = a, shift = {2, 0}}}, 1, "linear")
s:at(0.5, function()
    s:clone(word, {id = "half"})
    s:clone(n, {id = "count"})
    s:clone(a, {id = "mid"})
end)
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "clone.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "clone.lua: %s\n", error);
    if (scene) {
        // The overrides apply to the copy, and a group passes its stroke to the copies that
        // take theirs from it.
        auto pair = scene->object("pair");
        auto red = Color::hex("#ff0000");
        CHECK(pair && pair->childCount() == 2u && _near(pair->model.e[7], -2.0f) && _near(pair->opacity, 0.5f));
        CHECK(pair && _sameColor(pair->childAt(0)->style.stroke, red));
        CHECK(pair && _sameColor(pair->childAt(1)->style.stroke, Color::hex("#0000ff")));
        CHECK(pair && _tagged(pair->childAt(0), "pair/a") && _tagged(pair->childAt(1), "pair/b"));
        auto masked = scene->object("masked");
        CHECK(masked && masked->mask() == scene->object("r"));
        // Mid-timeline a copy takes what the source shows at the cursor and shows from there.
        auto half = static_cast<Text*>(scene->object("half"));
        CHECK(half && std::strcmp(half->text(), "hots") == 0);
        auto count = static_cast<Text*>(scene->object("count"));
        CHECK(count && count->counting() && _near(count->value(), 50.0f) && std::strcmp(count->text(), "50") == 0);
        auto mid = scene->object("mid");
        CHECK(mid && _near(mid->model.e[3], 1.0f) && mid->parent() == scene->object("row"));
        BBox bounds;
        CHECK(renderer->bounds(scene, mid, 0.25f, bounds) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, mid, 0.5f, bounds) == Result::Success);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:clone(c, {size=2}); return s",
                 "unknown option 'size'");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1, id='c'}; s:clone(c, {id='c'}); return s",
                 "duplicate id 'c'");
    // The options apply to the copy while the scene clones, and their errors name the line.
    _scriptError("local s=vis.scene{}\nlocal c=s:circle{radius=1}\ns:clone(c, {opacity=2})\nreturn s",
                 "diagnostic.lua:3: opacity must be between 0 and 1");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=1};"
                 " local k=s:connector{from=a, to=b}\ns:clone(k, {shift={1,0}}); return s",
                 "diagnostic.lua:2: a connector follows its endpoints and takes no shift");
    _scriptError("local s=vis.scene{}; local p=s:space{}; s:clone(p); return s",
                 "could not clone space: a space cannot be cloned yet");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:wait(1); s:remove(c); s:clone(c); return s",
                 "could not clone circle: the object does not show at the cursor");
    _scriptError("local s=vis.scene{}; local o=vis.scene{}; local c=o:circle{radius=1}; s:clone(c); return s",
                 "handle belongs to another scene");
}

// morph, replacement_transform and match_transform with copy = true transform a copy of the
// source and leave the source in place.
static void _copyTransforms(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180}
local square = s:polygon {points = {{-4, 2}, {-2, 2}, {-2, 4}, {-4, 4}}, id = "square"}
local disc = s:circle {center = {3, 3}, radius = 1, id = "disc"}
s:morph(square, disc, 1, "linear", {copy = true})
local left = s:circle {center = {-3, -3}, radius = 0.5, id = "left"}
local right = s:circle {center = {3, -3}, radius = 0.5, id = "right"}
s:replacement_transform({square, disc}, {left, right}, 1, "linear", {lag = 0.5, copy = true})
local from = s:group {id = "from"}
from:circle {center = {-3, -1}, radius = 0.5, id = "from/dot"}
s:wait(1)
local to = s:group {id = "to"}
to:circle {center = {3, -1}, radius = 0.5, id = "to/dot"}
s:match_transform(from, to, 1, "linear", {copy = true, shapes = true})
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "copy.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "copy.lua: %s\n", error);
    if (scene) {
        // Each source keeps showing to the end while its copies transform and leave.
        CHECK(std::fabs(scene->duration() - 4.5f) < 1.0e-4f);
        BBox bounds;
        const char* stays[] = {"square", "disc", "left", "right", "from", "to"};
        for (auto id : stays) {
            auto object = scene->object(id);
            CHECK(object && renderer->bounds(scene, object, 4.5f, bounds) == Result::Success);
        }
        const char* leaves[] = {"square:copy", "square:copy2", "disc:copy", "from:copy"};
        for (auto id : leaves) {
            auto object = scene->object(id);
            CHECK(object && renderer->bounds(scene, object, 4.5f, bounds) == Result::InsufficientCondition);
        }
        CHECK(scene->object("from:copy/dot") && scene->object("from:copy")->parent() == nullptr);
        CHECK(renderer->bounds(scene, scene->object("square:copy"), 0.5f, bounds) == Result::Success);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=2};"
                 " s:morph(a, b, 1, 'smooth', {copy=true, spin=1}); return s",
                 "unknown option 'spin'");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=2};"
                 " s:morph(a, b, 1, 'smooth', {copy=1}); return s",
                 "copy must be a boolean");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=2};"
                 " s:replacement_transform(a, b, 1, 'smooth', {lag=-1}); return s",
                 "lag cannot be negative");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=2}; s:wait(1);"
                 " s:morph(a, b, 1, 'smooth', {copy=true}); return s",
                 "could not play animation: add the morph target right before the morph");
    _scriptError("local s=vis.scene{}; local a=s:group{}; local b=s:group{};"
                 " s:match_transform(a, b, 1, 'smooth', {copy='yes'}); return s",
                 "copy must be a boolean");
}

// Measured billboard text preserves its ink bounds across scaling, cloning and matching.
static void _matchingBillboard(SwRenderer* renderer)
{
    const char* variants[] = {"", ", {copy=true}", ", {path=guide}", ", {copy=true,path=guide}"};
    for (auto variant : variants) {
        char source[1800];
        std::snprintf(source, sizeof(source), R"lua(
local s=vis.scene {width=960,height=540,camera={height=540}}
local a=s:text {text="copy",font="Pretendard",size=40,measure=true,point={-200,0},id="source"}
s:wait(.1)
s:scale(a,.2,{duration=.2,curve="linear"})
s:wait(.2)
local snapshot=s:clone(a,{id="snapshot"})
local b=s:text {text="copy",font="Pretendard",size=40,measure=true,point={200,0},id="target"}
local guide=s:route {points={{-200,0},{0,80},{200,0}},opacity=0}
s:match_transform(snapshot,b,1,"linear"%s)
return s
)lua", variant);
        char error[256] = {};
        Scene* scene = nullptr;
        CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "match-billboard.lua",
                        &scene, error, sizeof(error)) == Result::Success);
        if (scene) {
            BBox before; BBox actual;
            CHECK(renderer->bounds(scene, scene->object("source"), 0.499f, before) == Result::Success);
            // The original stays in place; the target arrives in the snapshot's same look
            // even when its authored size is five times larger.
            for (auto time : {1.5f, 0.5f, 0.499f, 0.5f}) {
                auto object = scene->object(time == 0.5f ? "target" : "source");
                CHECK(renderer->bounds(scene, object, time, actual) == Result::Success);
                CHECK(std::fabs(actual.x - before.x) < 0.01f && std::fabs(actual.y - before.y) < 0.01f
                      && std::fabs(actual.width - before.width) < 0.01f
                      && std::fabs(actual.height - before.height) < 0.01f);
            }
        }
        delete scene;
    }
}

static void _matchingPath(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {height = 6}}
local guide = s:route {points = {{0, 0}, {0, 2}, {2, 2}}, opacity = 0, id = "guide"}
local from = s:group {id = "from"}
from:circle {center = {-2, -1}, radius = 0.2, id = "a"}
from:circle {center = {0, -1}, radius = 0.2, id = "b"}
s:wait(1)
local to = s:group {id = "to"}
to:circle {center = {0, 1}, radius = 0.2, id = "c"}
to:circle {center = {2, 1}, radius = 0.2, id = "d"}
s:match_transform(from, to, 1, "linear", {copy = true, shapes = true, path = guide})
s:shift(guide, {1, -1}, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "match-path.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (scene) {
        BBox start;
        BBox middle;
        BBox end;
        auto target = scene->object("c");
        CHECK(renderer->bounds(scene, target, 2.0f, end) == Result::Success);
        CHECK(renderer->bounds(scene, target, 1.5f, middle) == Result::Success);
        CHECK(renderer->bounds(scene, target, 1.0f, start) == Result::Success);
        CHECK(std::fabs(start.center().x - 100.0f) < 0.01f && std::fabs(start.center().y - 120.0f) < 0.01f);
        CHECK(std::fabs(middle.center().x - 100.0f) < 0.01f && std::fabs(middle.center().y - 60.0f) < 0.01f);
        CHECK(std::fabs(end.center().x - 160.0f) < 0.01f && std::fabs(end.center().y - 60.0f) < 0.01f);
        CHECK(renderer->bounds(scene, scene->object("a"), 1.5f, end) == Result::Success);
        CHECK(std::fabs(end.center().x - start.center().x) < 0.01f && std::fabs(end.center().y - start.center().y) < 0.01f);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; local a=s:circle{}; local b=s:circle{};"
                 "local p=s:line{from={0,0},to={1,1}}; s:match_transform(a,b,1,'linear',{path=p,arc=1}); return s",
                 "path and arc cannot be used together");
    _scriptError("local s=vis.scene{}; local a=s:circle{}; local b=s:circle{};"
                 "s:match_transform(a,b,1,'linear',{path={}}); return s", "expected one object handle");
    _scriptError("local s=vis.scene{}; local a=s:circle{}; local b=s:circle{center={1,0}};"
                 "local p=s:connector{from=a,to=b}; s:match_transform(a,b,1,'linear',{path=p}); return s",
                 "a match path must be a snapshot outline, not a live connector");
}

static void _matching(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local first = s:spans {"a²", " + ", "b²", " = ", {"c²", color = "#ff0000"}, size = 28, point = {0, 1}, id = "first"}
s:wait(1)
local second = s:spans {{"c²", color = "#ff0000"}, " = ", "a²", " + ", "b²", size = 28, point = {0, 1}, id = "second"}
s:match_transform(first, second, 2, "linear", {arc = math.pi / 2})
local third = s:spans {{"c", color = "#ff0000"}, " = ", "√(", "a²", " + ", "b²", ")", size = 28, point = {0, -1},
                       id = "third"}
s:match_transform(second, third, 1, "smooth", {pairs = {{s:find("second/1"), s:find("third/1")}}, lag = 0.1})
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "match.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "match.lua: %s\n", error);
    if (scene) {
        // Seven pieces of the last row, a tenth of a second apart.
        CHECK(std::fabs(scene->duration() - 4.6f) < 1.0e-4f);
        auto from = scene->object("first/5");
        auto to = scene->object("second/1");
        BBox before;
        BBox start;
        BBox middle;
        BBox end;
        // The c² of the second row starts as the first row's c², which leaves, and travels left.
        CHECK(from && renderer->bounds(scene, from, 0.9f, before) == Result::Success);
        CHECK(renderer->bounds(scene, from, 1.0f, start) == Result::InsufficientCondition);
        CHECK(to && renderer->bounds(scene, to, 1.0f, start) == Result::Success);
        CHECK(std::fabs(start.x - before.x) < 0.5f && std::fabs(start.y - before.y) < 0.5f);
        CHECK(renderer->bounds(scene, to, 2.0f, middle) == Result::Success);
        CHECK(renderer->bounds(scene, to, 3.0f, end) == Result::Success);
        CHECK(end.x < start.x - 50.0f);
        // Both ends share a baseline, so the arc lifts the middle off it.
        CHECK(std::fabs(middle.center().y - start.center().y) > 10.0f);
        // The root fades in; c² and c cross-fade.
        BBox seen;
        CHECK(renderer->bounds(scene, scene->object("third/3"), 3.0f, seen) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, scene->object("third/3"), 4.6f, seen) == Result::Success);
        CHECK(renderer->bounds(scene, to, 3.5f, seen) == Result::Success);
        CHECK(renderer->bounds(scene, scene->object("third/1"), 3.5f, seen) == Result::Success);
        CHECK(renderer->bounds(scene, to, 4.6f, seen) == Result::InsufficientCondition);
    }
    delete scene;

    // strings = false keeps only the given pairs, so twin texts fade instead of traveling.
    static constexpr auto twins = R"lua(
local s = vis.scene {}
local from = s:group {}
from:text {text = "x"}
s:wait(1)
local to = s:group {}
to:text {text = "x", point = {1, 0}}
s:match_transform(from, to, 1, "smooth", {strings = false})
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(twins, static_cast<uint32_t>(std::strlen(twins)), "twins.lua", &scene, error, sizeof(error))
          == Result::Success);
    CHECK(scene && scene->clipCount() == 2u);
    delete scene;

    // diff = true keeps the order of code lines: the words that cross the kept line fade, two
    // clips each, where strings would carry them in one.
    const char* const diffs[] = {"false", "true"};
    for (auto diff : diffs) {
        char edited[320];
        std::snprintf(edited, sizeof(edited), R"lua(
local s = vis.scene {}
local before = s:code {text = "a b\nc", language = "lua"}
s:wait(1)
local after = s:code {text = "c\nb a", language = "lua"}
s:match_transform(before, after, 1, "smooth", {diff = %s, fade = 0.4})
return s
)lua", diff);
        scene = nullptr;
        CHECK(Lua::load(edited, static_cast<uint32_t>(std::strlen(edited)), "edited.lua", &scene, error, sizeof(error))
              == Result::Success);
        CHECK(scene && scene->clipCount() == (diff[0] == 't' ? 5u : 3u));
        delete scene;
    }

    // shapes = true pairs bars in order, so two bars split into four in four clips, where six
    // fade the two out and the four in.
    const char* const shaped[] = {"false", "true"};
    for (auto shapes : shaped) {
        char split[400];
        std::snprintf(split, sizeof(split), R"lua(
local s = vis.scene {}
local two = s:group {}
for i = 1, 2 do two:rectangle {center = {2 * i - 3, 0}, size = {2, 1}} end
s:wait(1)
local four = s:group {}
for i = 1, 4 do four:rectangle {center = {i - 2.5, 0}, size = {0.8, 1}} end
s:match_transform(two, four, 1, "smooth", {shapes = %s})
return s
)lua", shapes);
        scene = nullptr;
        CHECK(Lua::load(split, static_cast<uint32_t>(std::strlen(split)), "split.lua", &scene, error, sizeof(error))
              == Result::Success);
        CHECK(scene && scene->clipCount() == (shapes[0] == 't' ? 4u : 6u));
        delete scene;
    }

    static constexpr auto pieces = "local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=1}; ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", pieces, body);
        _scriptError(script, expected);
    };
    fails("s:match_transform(a, b, 1, 'smooth', {spin = 1})", "unknown option 'spin'");
    fails("s:match_transform(a, b, 1, 'smooth', {pairs = {{a}}})", "pairs must be an array of {source_piece, target_piece}");
    fails("s:match_transform(a, b, 1, 'smooth', {pairs = {{b, a}}})", "each pair needs a piece of each object");
    fails("s:match_transform(a, b, 1, 'smooth', {arc = 7})", "arc must be greater than -2*pi and less than 2*pi radians");
    fails("s:match_transform(a, b, 1, 'smooth', {lag = -1})", "lag cannot be negative");
    fails("s:match_transform(a, b, 1, 'smooth', {strings = 1})", "strings must be a boolean");
    fails("s:match_transform(a, b, 1, 'smooth', {diff = 'yes'})", "diff must be a boolean");
    fails("s:match_transform(a, b, 1, 'smooth', {shapes = 1})", "shapes must be a boolean");
    fails("s:match_transform(a, b, 1, 'smooth', {fade = 0})", "fade must be a share of the duration greater than 0 and at most 1");
    fails("s:wait(1); s:match_transform(a, b)",
          "could not match transform: add the object to match into right before the match");
}

static bool _sameColor(const Color& first, const Color& second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b && first.a == second.a;
}

static void _coding()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
s:code {id = "py", language = "python", size = 20, point = {-3, 2}, align = {0, 0}, text = [[
def area(r):
    """Circle
    area."""
    return 3.14 * r ** 2  # pi

print(area(2))
]]}
s:code {id = "lua", language = "lua", size = 20, point = {0, -2}, leading = 1.5,
        text = "--[==[ a\nb ]==] x = [[s]]\nlocal t = {1, 'two'}"}
s:code {id = "tint", language = "lua", size = 20, point = {3, 3}, color = "#0000ff",
        colors = {keyword = "accent", comment = "#00ff00"}, text = "local x -- c"}
s:code {id = "flat", size = 20, point = {-3, 3}, text = "a  =\tb"}
local holder = s:group {id = "holder"}
holder:code {id = "nested", text = "x", size = 20}
s:wait(1)
local late = s:code {id = "late", text = "one two", size = 20, point = {0, 3}}
s:write(late:children()[1]:children(), 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "code.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "code.lua: %s\n", error);
    if (scene) {
        auto& theme = scene->theme();
        auto blank = Text::gen(" ");
        if (blank) {
            blank->size = 20.0f;
            blank->role = TextRole::Code;
        }
        TextExtent space;
        CHECK(blank && scene->measure(blank, space) == Result::Success);
        delete blank;
        auto lineHeight = space.ascent + space.descent;
        Text* texts[8] = {};

        // One group per line, blank lines too; the line feed at the end opens none.
        auto block = scene->object("py");
        CHECK(block && block->type() == Type::Group && static_cast<Group*>(block)->typeset);
        CHECK(block && block->childCount() == 6u);
        CHECK(scene->object("py/5") && scene->object("py/5")->childCount() == 0u);
        CHECK(scene->object("py/1") && !static_cast<Group*>(scene->object("py/1"))->typeset);

        // Names, brackets and punctuation each make a token, colored by what they are.
        CHECK(_spanTexts(scene, "py/1", texts, 8) == 6u);
        const char* first[] = {"def", "area", "(", "r", ")", ":"};
        for (auto i = 0u; i < 6u; i++) CHECK(texts[i] && std::strcmp(texts[i]->text(), first[i]) == 0);
        CHECK(texts[0] && texts[0]->tag() && std::strcmp(texts[0]->tag(), "py/1/1") == 0);
        CHECK(texts[0] && _sameColor(texts[0]->style.fill, theme.color(ThemeColorRole::Secondary)));
        CHECK(texts[1] && _sameColor(texts[1]->style.fill, theme.color(ThemeColorRole::Accent)));
        CHECK(texts[2] && _sameColor(texts[2]->style.fill, theme.color(ThemeColorRole::Foreground)));
        CHECK(texts[0] && texts[0]->role == TextRole::Code && texts[0]->size == 20.0f);
        auto lead = texts[0];
        // A space leaves the blank of the code font, and neighbors without one touch.
        Bounds def;
        Bounds area;
        Bounds open;
        CHECK(texts[0] && texts[0]->bounds(def) && texts[1] && texts[1]->bounds(area) && texts[2]->bounds(open));
        CHECK(std::fabs(area.min.x - def.max.x - space.width) < 1.0e-4f);
        CHECK(std::fabs(open.min.x - area.max.x) < 1.0e-4f);
        // align {0, 0} hangs the block from its point.
        Bounds blockBounds;
        CHECK(block && block->bounds(blockBounds));
        CHECK(std::fabs(blockBounds.min.x + 3.0f) < 1.0e-4f && std::fabs(blockBounds.max.y - 2.0f) < 1.0e-3f);

        // A string runs on across lines, and its later lines keep their indent.
        CHECK(_spanTexts(scene, "py/2", texts, 8) == 1u);
        CHECK(texts[0] && std::strcmp(texts[0]->text(), "\"\"\"Circle") == 0);
        CHECK(texts[0] && _sameColor(texts[0]->style.fill, theme.color(ThemeColorRole::Success)));
        CHECK(_spanTexts(scene, "py/3", texts, 8) == 1u);
        CHECK(texts[0] && std::strcmp(texts[0]->text(), "area.\"\"\"") == 0);
        Bounds indented;
        CHECK(texts[0] && texts[0]->bounds(indented));
        CHECK(std::fabs(indented.min.x - def.min.x - 4.0f * space.width) < 1.0e-4f);
        CHECK(lead && texts[0] && std::fabs(lead->point.y - texts[0]->point.y - 2.0f * lineHeight) < 1.0e-4f);
        // Numbers, operators and a comment that ends the line.
        CHECK(_spanTexts(scene, "py/4", texts, 8) == 7u);
        const char* fourth[] = {"return", "3.14", "*", "r", "**", "2", "# pi"};
        for (auto i = 0u; i < 7u; i++) CHECK(texts[i] && std::strcmp(texts[i]->text(), fourth[i]) == 0);
        CHECK(texts[1] && _sameColor(texts[1]->style.fill, theme.color(ThemeColorRole::Danger)));
        CHECK(texts[6] && _sameColor(texts[6]->style.fill, theme.color(ThemeColorRole::Muted)));
        // A builtin reads as a type; a name called is a function.
        CHECK(_spanTexts(scene, "py/6", texts, 8) == 7u);
        CHECK(texts[0] && _sameColor(texts[0]->style.fill, theme.color(ThemeColorRole::Warning)));
        CHECK(texts[2] && _sameColor(texts[2]->style.fill, theme.color(ThemeColorRole::Accent)));

        // Lua long comments close mid-line; leading spreads the lines.
        CHECK(_spanTexts(scene, "lua/1", texts, 8) == 1u);
        CHECK(_spanTexts(scene, "lua/2", texts, 8) == 4u);
        CHECK(texts[0] && std::strcmp(texts[0]->text(), "b ]==]") == 0);
        CHECK(texts[3] && std::strcmp(texts[3]->text(), "[[s]]") == 0);
        auto second = texts[0];
        CHECK(_spanTexts(scene, "lua/3", texts, 8) == 8u);
        CHECK(texts[6] && std::strcmp(texts[6]->text(), "'two'") == 0);
        CHECK(second && texts[0] && std::fabs(second->point.y - texts[0]->point.y - 1.5f * lineHeight) < 1.0e-4f);

        // color sets every class, and colors sets the classes it names.
        CHECK(_spanTexts(scene, "tint", texts, 8) == 1u);
        CHECK(_spanTexts(scene, "tint/1", texts, 8) == 3u);
        CHECK(texts[0] && _sameColor(texts[0]->style.fill, theme.color(ThemeColorRole::Accent)));
        CHECK(texts[1] && texts[1]->style.fill.b == 0xff && texts[1]->style.fill.r == 0);
        CHECK(texts[2] && texts[2]->style.fill.g == 0xff && texts[2]->style.fill.r == 0);

        // Plain code splits but colors nothing, and a tab leaves four spaces.
        CHECK(_spanTexts(scene, "flat/1", texts, 8) == 3u);
        Bounds a;
        Bounds equals;
        Bounds b;
        CHECK(texts[0] && texts[0]->bounds(a) && texts[1] && texts[1]->bounds(equals) && texts[2]
              && texts[2]->bounds(b));
        CHECK(std::fabs(equals.min.x - a.max.x - 2.0f * space.width) < 1.0e-4f);
        CHECK(std::fabs(b.min.x - equals.max.x - 4.0f * space.width) < 1.0e-4f);
        CHECK(texts[1] && _sameColor(texts[1]->style.fill, theme.color(ThemeColorRole::Foreground)));

        // Code nests below a group, and a block added mid-timeline animates.
        auto nested = scene->object("nested");
        CHECK(nested && nested->parent() == scene->object("holder"));
        CHECK(scene->object("late/1") && scene->object("late/1")->childCount() == 2u);
        CHECK(scene->clipCount() == 2u);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; s:code{}; return s", "missing field 'text'");
    _scriptError("local s=vis.scene{}; s:code{text='a', stroke='red'}; return s",
                 "code holds only text, so stroke is unsupported; color and colors set its colors");
    _scriptError("local s=vis.scene{}; s:code{text='a', bogus=1}; return s", "unknown option 'bogus'");
    _scriptError("local s=vis.scene{}; s:code{text='a', language='rust'}; return s",
                 "language must be 'plain', 'lua', 'python', 'javascript', 'typescript', 'c' or 'cpp'");
    _scriptError("local s=vis.scene{}; s:code{text='a', leading=0}; return s",
                 "leading must be a positive multiple of the line height");
    _scriptError("local s=vis.scene{}; s:code{text='a', colors=1}; return s",
                 "colors must be a table of token classes and colors");
    _scriptError("local s=vis.scene{}; s:code{text='a', colors={bold='#fff'}}; return s",
                 "colors may set only plain, keyword, string, number, comment, function, type and operator");
    _scriptError("local s=vis.scene{}; s:code{text='a', colors={keyword='bright'}}; return s",
                 "keyword must be a hex color or Theme color role");
    _scriptError("local s=vis.scene{}; s:code{text=' \\n\\t'}; return s",
                 "code needs at least one character that is not blank");
    _scriptError("local s=vis.scene{}; s:code{text='a', orientation='up'}; return s",
                 "text orientation must be 'billboard' or 'plane'");
    _scriptError("local s=vis.scene{}; s:code{text='a', font='Missing'}; return s",
                 "could not measure text: its font 'Missing' is not loaded");
}

// A block pins its first baseline and keeps a world-unit pitch, including blank lines.
static void _codeBaselines()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 960, height = 540, camera = {height = 540}}
s:code {text = "iii WWW\n\nMM", point = {20, 110}, align = {1, 0.9}, baseline = true,
        pitch = 36, size = 18, id = "pin"}
s:code {text = "a\nb", point = {0, 0}, align = {0, 0}, pitch = 36, size = 18, id = "box"}
s:code {text = "a\nb", point = {0, 20}, align = {0, 1}, baseline = true, leading = 2, size = 18, id = "led"}
local g = s:group {}
local b = g:code {text = "a\n\nb", point = {5, 7}, align = {0, 1}, baseline = true,
                  pitch = 3, orientation = "plane", size = 0.5, id = "plane"}
s:edit(b, {replace = "a", with = "wide"}, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "code-baselines.lua", &scene,
                    error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "code-baselines.lua: %s\n", error);
    if (scene) {
        CHECK(std::fabs(_baselineOf(scene, "pin/1/1") - 110.0f) < 1.0e-4f);
        CHECK(std::fabs(_baselineOf(scene, "pin/1/2") - 110.0f) < 1.0e-4f);
        CHECK(std::fabs(_baselineOf(scene, "pin/3/1") - 38.0f) < 1.0e-4f);
        auto thin = static_cast<Text*>(scene->object("pin/1/1"));
        auto wide = static_cast<Text*>(scene->object("pin/1/2"));
        CHECK(thin && wide && std::strcmp(thin->font(), "JetBrainsMono") == 0);
        CHECK(thin && wide && std::fabs(thin->box.x - wide->box.x) < 1.0e-4f);
        Bounds bounds;
        CHECK(scene->object("pin")->bounds(bounds));
        CHECK(std::fabs(bounds.max.x - 20.0f) < 1.0e-4f);
        CHECK(scene->object("box")->bounds(bounds));
        CHECK(std::fabs(bounds.max.y) < 1.0e-4f);
        CHECK(std::fabs(_baselineOf(scene, "box/1/1") - _baselineOf(scene, "box/2/1") - 36.0f) < 1.0e-4f);
        CHECK(std::fabs(_baselineOf(scene, "led/1/1") - 20.0f) < 1.0e-4f);
        CHECK(thin && std::fabs(_baselineOf(scene, "led/1/1") - _baselineOf(scene, "led/2/1") - 2.0f * thin->box.y) < 1.0e-4f);
        CHECK(std::fabs(_baselineOf(scene, "plane/3/1") - 1.0f) < 1.0e-4f);
        CHECK(std::fabs(_baselineOf(scene, "plane.2/1/1") - 7.0f) < 1.0e-4f);
        CHECK(std::fabs(_baselineOf(scene, "plane.2/3/1") - 1.0f) < 1.0e-4f);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:code{text='a', baseline=1}; return s", "baseline must be a boolean");
    _scriptError("local s=vis.scene{}; s:code{text='a', pitch=0}; return s", "pitch must be a positive distance");
    _scriptError("local s=vis.scene{}; s:code{text='a', pitch=-1}; return s", "pitch must be a positive distance");
    _scriptError("local s=vis.scene{}; s:code{text='a', pitch=math.huge}; return s", "pitch must be a finite number");
    _scriptError("local s=vis.scene{}; s:code{text='a', pitch='x'}; return s", "number expected");
    _scriptError("local s=vis.scene{}; s:code{text='a', pitch=3, leading=1}; return s", "code takes pitch or leading, not both");
}

static void _highlighting()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local block = s:code {id = "c", text = "a = 1\nb = 2\nc = 3\nd = 4", size = 20}
s:highlight(block, 2, 3, {duration = 1})
s:mark("lines")
s:highlight(block, s:find("c/1/3"))
s:mark("word")
s:highlight(block, {s:find("c/4/1"), s:find("c/4/2")}, {curve = "linear", dim = 0.5})
s:mark("words")
s:highlight(block, {duration = 0.25})
s:mark("clear")
s:highlight(block)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "highlight.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "highlight.lua: %s\n", error);
    if (scene) {
        // Lines 2 and 3 stay, the other six tokens dim; then one token of the first line
        // brightens while lines 2 and 3 dim; then two tokens of the last line take over,
        // dimming the rest to 0.5; clearing brings those ten back, and a last call waits.
        float time;
        CHECK(scene->marker("lines", time) && std::fabs(time - 1.0f) < 1.0e-5f);
        CHECK(scene->marker("word", time) && std::fabs(time - 1.5f) < 1.0e-5f);
        CHECK(scene->marker("words", time) && std::fabs(time - 2.0f) < 1.0e-5f);
        CHECK(scene->marker("clear", time) && std::fabs(time - 2.25f) < 1.0e-5f);
        CHECK(std::fabs(scene->duration() - 2.75f) < 1.0e-5f);
        CHECK(scene->clipCount() == 6u + 7u + 12u + 10u);
        for (auto line = 1; line <= 4; line++) {
            for (auto word = 1; word <= 3; word++) {
                char id[16];
                std::snprintf(id, sizeof(id), "c/%d/%d", line, word);
                CHECK(scene->object(id) && scene->object(id)->opacity == 1.0f);
            }
        }
        CHECK(scene->object("c/2")->opacity == 1.0f && scene->object("c")->opacity == 1.0f);
    }
    delete scene;

    static constexpr auto group = "local s=vis.scene{}; local g=s:group{}; g:circle{radius=1}; ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", group, body);
        _scriptError(script, expected);
    };
    fails("s:highlight(g, 2)", "first and last must count the target's children from 1 to 1, first no later than last");
    fails("s:highlight(g, 1, 0)", "first and last must count the target's children from 1 to 1");
    fails("s:highlight(g, 1.5)", "first and last must count the target's children from 1 to 1");
    fails("s:highlight(g, s:circle{radius=1})",
          "could not highlight: a highlight selects the target or objects below it");
    fails("s:highlight(g, 1, {dim = 2})", "dim must be an opacity from 0 to 1");
    fails("s:highlight(g, {spin = 1})", "unknown option 'spin'");
    fails("s:highlight(g, 1, {duration = 0})", "duration must be positive");
    fails("s:highlight(g, 'x')", "a highlight selects children by number, a handle or a list of handles");
    fails("s:highlight(g, {g, dim = 1})", "a highlight takes a list of handles, and its options in a table of their own");
}

// A search finds the tokens of a code block that the matches of a text cover, as handles that
// highlight, indicate and around take.
static void _searching()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local block = s:code {id = "sum", language = "lua", size = 20, text = [[
local total = 0
for i = 1, 10 do
  total = total + i
end
print(total)
]]}
local function same(list, ...)
    local ids = {...}
    if #list ~= #ids then error(#list .. " handles where " .. #ids .. " were due") end
    for i = 1, #ids do
        if list[i] ~= s:find(ids[i]) then error("handle " .. i .. " is not " .. ids[i]) end
    end
end
-- The first match, the last, a given one, or all of them.
same(block:search("total"), "sum/1/2")
same(block:search("total", {match = "last"}), "sum/5/3")
same(block:search("total", {match = 3}), "sum/3/3")
same(block:search("total", {match = "all"}), "sum/1/2", "sum/3/1", "sum/3/3", "sum/5/3")
-- Part of a token takes the token whole; a match across tokens or lines takes each.
same(block:search("ota"), "sum/1/2")
same(block:search("1, 10"), "sum/2/4", "sum/2/5", "sum/2/6")
same(block:search("do\n  total"), "sum/2/7", "sum/3/1")
-- lines narrows the search, and whole passes over matches that cut a token.
same(block:search("total", {lines = {3, 5}, match = "all"}), "sum/3/1", "sum/3/3", "sum/5/3")
same(block:search("total", {lines = 3, match = "last"}), "sum/3/3")
same(block:search("i", {whole = true, match = "all"}), "sum/2/2", "sum/3/5")
-- The bytes of UTF-8 belong to names, and a block below a group searches the same.
local holder = s:group {}
local word = holder:code {id = "word", language = "python", size = 20, point = {0, -2},
                          text = "café = 'naïve'\nprint(café)"}
same(word:search("é", {match = "all"}), "word/1/1", "word/2/3")
same(word:search("ï"), "word/1/3")
same(word:search("café", {whole = true, match = "last"}), "word/2/3")
s:rectangle {id = "box", around = block:search("1, 10"), padding = 0.1}
s:wait(1)
s:highlight(block, block:search("total", {match = "all"}))
s:indicate(block:search("total", {match = "all"}), {duration = 0.5})
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "search.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "search.lua: %s\n", error);
    if (scene) {
        // The highlight dims the 17 other tokens, and the four found indicate at once.
        CHECK(std::fabs(scene->duration() - 2.0f) < 1.0e-5f);
        CHECK(scene->clipCount() == 17u + 4u);
        // The rectangle covers the three tokens of 1, 10.
        Bounds covered;
        auto measured = true;
        const char* found[] = {"sum/2/4", "sum/2/5", "sum/2/6"};
        for (auto id : found) {
            Bounds token;
            auto object = scene->object(id);
            measured = measured && object && object->bounds(token);
            if (!measured) break;
            if (id == found[0]) covered = token;
            covered.min.x = std::fmin(covered.min.x, token.min.x);
            covered.min.y = std::fmin(covered.min.y, token.min.y);
            covered.max.x = std::fmax(covered.max.x, token.max.x);
            covered.max.y = std::fmax(covered.max.y, token.max.y);
        }
        auto box = static_cast<Rectangle*>(scene->object("box"));
        CHECK(measured && box);
        if (measured && box) {
            CHECK(_near(box->center.x, (covered.min.x + covered.max.x) * 0.5f));
            CHECK(_near(box->center.y, (covered.min.y + covered.max.y) * 0.5f));
            CHECK(_near(box->size.x, covered.max.x - covered.min.x + 0.2f));
            CHECK(_near(box->size.y, covered.max.y - covered.min.y + 0.2f));
        }
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local b=s:code{text='a = 1\\nb = a', size=20}; ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("b:search('')", "search needs a text to find");
    fails("b:search('z')", "the code has no 'z'");
    fails("b:search('b', {lines = 1})", "line 1 of the code has no 'b'");
    fails("b:search('z', {lines = {1, 2}})", "lines 1 to 2 of the code have no 'z'");
    fails("b:search('= 1\\nb =', {whole = true, lines = 2})", "line 2 of the code has no '= 1\nb ='");
    fails("b:search(' =', {whole = true, match = 3})", "match 3 is past the last of the 2 matches of ' ='");
    fails("b:search('a', {match = 'most'})", "match must be 'first', 'last', 'all' or a match number from 1");
    fails("b:search('a', {match = 0})", "match must be 'first', 'last', 'all' or a match number from 1");
    fails("b:search('a', {lines = {2, 1}})", "lines takes a line number or {first, last}, counting from 1");
    fails("b:search('a', {lines = 3})", "line 3 is past the last of the code's 2 lines");
    fails("b:search('a', {spin = 1})", "unknown option 'spin'");
    fails("b:search(' ')", "' ' matches only blanks, which no token holds");
    fails("s:group{}:search('a')", "search needs a block that code made");
    fails("b:children()[1]:search('a')", "search needs a block that code made");
    fails("s:indicate({})", "handle array cannot be empty");
    _scriptError("local s=vis.scene{}; local b=s:code{text='ab = b1'}; b:search('b', {whole = true, lines = 1}); return s",
                 "line 1 of the code has no 'b' made of whole tokens");
}

// An edit applies its changes in order to the text of a code block and carries the block into
// the edited one, which keeps its look, its place and its group.
static void _editing()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local holder = s:group {id = "holder"}
local v1 = holder:code {id = "sum", language = "lua", size = 20, point = {-3, 1}, align = {0, 0},
                        colors = {keyword = "#ff0000"}, text = [[
local total = 0
for i = 1, 10 do
  total = total + i
end
print(total)
]]}
s:shift(v1, {1, 0}, 1)
-- Each edit reads the text the one before left, so 10 turns into n before the line that sets
-- n to 10 comes in.
local v2 = s:edit(v1, {
    {replace = "10", with = "n"},
    {insert = "local n = 10", before = 1},
    {remove = 6},
    {append = "return total"},
})
if v2 ~= s:find("sum.2") or #v2:search("n", {whole = true, match = "all"}) ~= 2 then error("the first edit") end
local v3 = s:edit(v2, {
    {insert = " * i", after = "+ i"},
    {replace = "total", with = "sum", match = "all", whole = true},
    {replace = {1, 2}, with = "local n, sum = 10, 0"},
    {prepend = "-- squares"},
    {insert = "  -- add", after = 3},
}, 2, "linear", {fade = 0.5})
if v3 ~= s:find("sum.3") then error("an edit counts on from the id it edits") end
local v4 = s:edit(v3, {{remove = {4, 4}}, {remove = " * i"}, {replace = "sum", with = "total", match = "last"}},
                  0.5, "smooth", {id = "final"})
if v4 ~= s:find("final") then error("an edit takes a given id") end
-- A block without an id at the root makes one without an id there too.
local bare = s:code {text = "x = 1", point = {0, -2}}
local edited = s:edit(bare, {replace = "1", with = "2"}, 0.5)
if edited:search("2")[1] ~= edited:children()[1]:children()[3] then error("a bare edit") end
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "edit.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "edit.lua: %s\n", error);
    if (scene) {
        CHECK(std::fabs(scene->duration() - 5.0f) < 1.0e-5f);
        auto holder = scene->object("holder");
        const char* blocks[] = {"sum.2", "sum.3", "final"};
        for (auto id : blocks) {
            auto block = scene->object(id);
            CHECK(block && block->type() == Type::Group && block->parent() == holder);
        }
        CHECK(scene->object("sum.2") && scene->object("sum.2")->childCount() == 6u);
        CHECK(scene->object("sum.3") && scene->object("sum.3")->childCount() == 7u);
        auto last = scene->object("final");
        CHECK(last && last->childCount() == 6u);
        // The language, the colors and the size carry over: a Lua comment is one token, and a
        // keyword takes the color given to keywords.
        Text* texts[8] = {};
        CHECK(_spanTexts(scene, "final/1", texts, 8) == 1u);
        CHECK(texts[0] && std::strcmp(texts[0]->text(), "-- squares") == 0);
        CHECK(_spanTexts(scene, "final/2", texts, 8) == 8u);
        CHECK(texts[0] && std::strcmp(texts[0]->text(), "local") == 0 && texts[0]->size == 20.0f);
        CHECK(texts[0] && texts[0]->style.fill.r == 0xff && texts[0]->style.fill.g == 0 && texts[0]->style.fill.b == 0);
        CHECK(_spanTexts(scene, "final/4", texts, 8) == 5u);
        const char* fourth[] = {"sum", "=", "sum", "+", "i"};
        for (auto i = 0u; i < 5u; i++) CHECK(texts[i] && std::strcmp(texts[i]->text(), fourth[i]) == 0);
        CHECK(_spanTexts(scene, "final/6", texts, 8) == 2u);
        CHECK(texts[1] && std::strcmp(texts[1]->text(), "total") == 0);
        // The place carries over too: the shift before the edits, and the point the block hangs from.
        Bounds bounds;
        CHECK(last && _near(last->model.e[3], 1.0f) && _near(last->model.e[7], 0.0f));
        CHECK(last && last->bounds(bounds));
        CHECK(std::fabs(bounds.min.x + 2.0f) < 1.0e-4f && std::fabs(bounds.max.y - 1.0f) < 1.0e-3f);
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local b=s:code{id='b', text='a = 1\\nb = a', size=20}; ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:edit(b, {})", "an edit takes one of replace, insert, remove, append and prepend");
    fails("s:edit(b, {replace = 'a', insert = 'x', before = 1})",
          "an edit takes one of replace, insert, remove, append and prepend");
    fails("s:edit(b, {replace = 'a'})", "replace needs with, the text to put in its place");
    fails("s:edit(b, {replace = 'z', with = 'c'})", "the code has no 'z'");
    fails("s:edit(b, {{append = 'c'}, {remove = 'z'}})", "edit 2: the code has no 'z'");
    fails("s:edit(b, {replace = 'a', with = 'c', match = 3})", "match 3 is past the last of the 2 matches of 'a'");
    fails("s:edit(b, {remove = 'b', lines = 1})", "line 1 of the code has no 'b'");
    fails("s:edit(b, {insert = 'x'})", "insert needs one of before and after");
    fails("s:edit(b, {insert = 'x', before = 1, after = 1})", "insert needs one of before and after");
    fails("s:edit(b, {insert = 'x', after = {1, 2}})", "before and after take a text to find or a line number");
    fails("s:edit(b, {remove = 3})", "line 3 is past the last of the code's 2 lines");
    fails("s:edit(b, {remove = {2, 1}})", "remove takes a text to find, a line number or {first, last}");
    fails("s:edit(b, {remove = 1, whole = true})", "match, lines and whole need a text to find, not line numbers");
    fails("s:edit(b, {remove = 'a', with = 'c'})", "unknown option 'with' for remove");
    fails("s:edit(b, {append = 1})", "append takes a string");
    fails("s:edit(b, {remove = ''})", "the text to find cannot be empty");
    fails("s:edit(b, {remove = {1, 2}})", "code needs at least one character that is not blank");
    fails("s:edit(b, {{append = 'c'}, 5})", "edit takes an edit table or a list of edit tables");
    fails("s:edit(b, {append = 'c'}, 1, 'smooth', {spin = 1})", "unknown option 'spin'");
    fails("s:edit(b, {append = 'c'}, 1, 'smooth', {id = 'b'})", "duplicate id 'b'");
    fails("s:edit(b, {append = 'c'}, 1, 'smooth', {fade = 0})",
          "fade must be a share of the duration greater than 0 and at most 1");
    fails("s:edit(s:circle{radius = 1}, {append = 'c'})", "edit needs a block that code made");
    // edit makes the new block and the match through calls of its own, whose errors still name
    // the line of the edit, and edit in a bad argument.
    fails("\ns:edit(b, {append = 'c'}, 1, 'smooth', {id = 'b'})", "diagnostic.lua:2: duplicate id 'b'");
    fails("\ns:edit(b, {append = 'c'}, 1, 'smooth', {fade = 2})", "diagnostic.lua:2: fade must be a share");
    fails("\ns:edit(b, {append = 'c'}, 'long')",
          "diagnostic.lua:2: bad argument #3 to 'edit' (number expected, got string)");
    fails("s:shift(b, {1, 0}, 1); s:seek(0.5)\ns:edit(b, {append = 'c'})",
          "diagnostic.lua:2: could not match transform: the matched object and its pieces must be at rest");
}

// A rectangle around objects takes the bounds they have where it joins, grown by padding.
static void _plotBreaks()
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

static void _plotEndpoints(SwRenderer* renderer)
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

static void _plotEndpointProgress(SwRenderer* renderer)
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

static void _plotDiscontinuities()
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

static void _spaceTicksAndLabels()
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

static void _spaceScreenLabels(SwRenderer* renderer)
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

static void _spaceLabelIds()
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

static void _areas()
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

static void _riemannSums()
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
static void _surfaceColors()
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

static void _contours()
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

static void _contourLabels(SwRenderer* renderer)
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

static void _contourBindings(SwRenderer* renderer)
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

static void _vectorFields()
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

static void _streamLines()
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

static void _shapeMorphs()
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
s:replacement_transform(square, star, 1)
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

static void _contourPaths()
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

static void _regions()
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

static void _arranging()
{
    // A rectangle added mid-row grows into its slot while the others slide, and a group lays
    // its children out in a column around a given center.
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local row = s:group {id = "row"}
local a = row:rectangle {id = "a", size = {1, 1}}
local b = row:rectangle {id = "b", size = {1, 1}}
row:arrange({1, 0}, 0.5)
s:wait(0.5)
local c = row:rectangle {id = "c", size = {1, 1}}
s:arrange({a, c, b}, 1, "linear", {gap = 0.5})
s:arrange(row, 0.5, "smooth", {direction = {0, -1}, gap = 0, center = {2, 0}})
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "arrange.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "arrange.lua: %s\n", error);
    if (scene) {
        CHECK(_near(scene->duration(), 2.0f));
        struct Expected { const char* id; float x; float y; };
        Expected places[] = {{"a", 2.0f, 1.0f}, {"b", 2.0f, 0.0f}, {"c", 2.0f, -1.0f}};
        for (auto& place : places) {
            auto object = scene->object(place.id);
            CHECK(object && _near(object->model.e[3], place.x) && _near(object->model.e[7], place.y));
        }
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:arrange('row'); return s", "arrange takes a group or a list of handles");
    _scriptError("local s=vis.scene{}; s:arrange(s:group{}); return s", "arrange needs a group with children");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=1};"
                 "s:arrange({a, b}, 1, 'smooth', {gap = -1}); return s",
                 "could not arrange: the arrange gap must be zero or positive");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=1};"
                 "s:arrange({a, b}, 1, 'smooth', {spin = 1}); return s",
                 "unknown option 'spin'");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local g=s:group{}; local b=g:circle{radius=1};"
                 "s:arrange({a, b}); return s",
                 "arrange takes distinct objects that share one parent");
}

static void _advecting()
{
    // Points dropped in a rotation field turn a quarter, the one at its center stays, and a flow
    // runs backward with a negative speed; every frame bakes one linear shift.
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360, fps = 30}
local function G(x, y) return -y, x end
local function U(x, y) return {1, 0} end
local a = s:point {point = {1, 0}, id = "a"}
local b = s:point {point = {0, 2}, id = "b"}
local c = s:point {point = {0, 0}, id = "c"}
s:advect({a, b, c}, G, {duration = 1, speed = math.pi / 2})
local e = s:point {point = {1, 0}, id = "e"}
s:advect(e, G, {duration = 1, speed = -math.pi / 2})
local d = s:circle {center = {3, 0}, radius = 0.1, id = "d"}
s:advect(d, U, {duration = 0.5, curve = "smooth"})
local f = s:point {point = {0, -1}, id = "f"}
s:advect(f, U, {duration = 10})
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "advect.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "advect.lua: %s\n", error);
    if (scene) {
        auto moved = [&](const char* id, float x, float y) {
            auto object = scene->object(id);
            return object && _near(object->model.e[3], x) && _near(object->model.e[7], y);
        };
        CHECK(_near(scene->duration(), 12.5f));
        CHECK(moved("a", -1.0f, 1.0f) && moved("b", -2.0f, -2.0f) && moved("c", 0.0f, 0.0f));
        CHECK(moved("e", -1.0f, -1.0f) && moved("d", 0.5f, 0.0f));
        // A frame a shift; a target the field leaves still has none.
        CHECK(moved("f", 10.0f, 0.0f));
        CHECK(scene->clipCount() == 30u + 30u + 30u + 15u + 300u);
    }
    delete scene;

    static constexpr auto prefix =
        "local s=vis.scene{}; local function G(x, y) return -y, x end; local a=s:point{point={1, 0}}; ";
    char script[320];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:advect(a)", "advect requires fn, a function of x and y");
    fails("s:advect(a, G, {speed = 0})", "speed must be a nonzero number; a negative one runs the flow backward");
    fails("s:advect(a, G, {lag = 1})", "unknown option 'lag'");
    fails("s:advect(a, function() return 'x' end)",
          "advect fn must return two finite numbers or {x, y}; at (1.0, 0.0) it did not");
    fails("s:advect(s:group{}, G)", "advect moves objects by their bounds; target 1 has none");
    fails("s:advect({a, a}, G)", "animation targets must be distinct");
    fails("s:advect(a, G, {duration = 1e30})", "advect needs one sample per frame within the 65536-clip budget");
}

static void _emphasizing(SwRenderer* renderer)
{
    // A wiggle may begin while a shift carries the box: it grows and turns about the moving
    // center and leaves the box as the shift does. A frame 10 units high on 200 pixels makes
    // a unit 20 pixels.
    static constexpr auto source = R"lua(
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local box = s:rectangle {id = "box", center = {-2, 0}, size = {1, 1}, fill = "#ffffff"}
s:parallel(
    function() s:shift(box, {4, 0}, 2, "linear") end,
    function()
        s:wait(0.5)
        s:wiggle(box, {scale = 1.5, angle = 0.5, swings = 1, duration = 1, curve = "linear"})
    end)
s:wiggle(box)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "wiggle.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "wiggle.lua: %s\n", error);
    if (scene) {
        auto box = scene->object("box");
        CHECK(_near(scene->duration(), 4.0f));
        CHECK(box && _near(box->model.e[0], 1.0f) && _near(box->model.e[3], 4.0f));
        BBox bounds;
        auto side = 20.0f * 1.5f * (std::cos(0.5f) + std::sin(0.5f));
        CHECK(box && renderer->bounds(scene, box, 1.0f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 100.0f) <= 1.0f && std::fabs(bounds.width - side) <= 2.0f);
        CHECK(box && renderer->bounds(scene, box, 4.0f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 140.0f) <= 1.0f && std::fabs(bounds.width - 20.0f) <= 2.0f);
    }
    delete scene;
    static constexpr auto prefix = "local s=vis.scene{}; local r=s:rectangle{}; ";
    char script[256];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:wiggle(r, {scale = 0})", "wiggle scale must be positive");
    fails("s:wiggle(r, {swings = 0})", "wiggle swings must be positive");
    fails("s:wiggle(r, {spin = 1})", "unknown option 'spin'");
    fails("s:wiggle(r, {easing = 'smooth', curve = 'linear'})", "use either 'easing' or 'curve', not both");
    fails("s:wiggle(r, 2)", "table expected");

    // A circumscribe inside parallel frames the box where it stands when the track begins,
    // with shapes at the root that are gone when it ends, while the shift carries the box on.
    static constexpr auto framing = R"lua(
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local box = s:rectangle {id = "box", center = {0, -2}, size = {2, 1}, fill = "#ffffff"}
s:circumscribe(box, {padding = 0, width = 1, window = 0.5, curve = "linear"})
s:parallel(
    function() s:shift(box, {2, 0}, 2, "linear") end,
    function() s:circumscribe(box, {shape = "ellipse", color = "#ff0000", duration = 1.5}) end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(framing, static_cast<uint32_t>(std::strlen(framing)), "circumscribe.lua", &scene, error,
                    sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "circumscribe.lua: %s\n", error);
    if (scene) {
        auto first = scene->object("box.circumscribe.1.frame");
        auto ellipse = scene->object("box.circumscribe.2.frame");
        auto flash = scene->object("box.circumscribe.2.flash");
        CHECK(_near(scene->duration(), 3.0f) && scene->count() == 5u);
        CHECK(first && ellipse && flash && !ellipse->parent() && ellipse->style.stroke.r == 255);
        BBox bounds;
        CHECK(first && renderer->bounds(scene, first, 0.5f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().y - 140.0f) <= 1.0f && std::fabs(bounds.width - 41.0f) <= 1.5f
              && std::fabs(bounds.height - 21.0f) <= 1.5f);
        CHECK(first && renderer->bounds(scene, first, 1.0f, bounds) == Result::InsufficientCondition);
        CHECK(ellipse && renderer->bounds(scene, ellipse, 1.75f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 100.0f) <= 1.0f && std::fabs(bounds.center().y - 140.0f) <= 1.0f);
        CHECK(ellipse && renderer->bounds(scene, ellipse, 2.5f, bounds) == Result::InsufficientCondition);
        auto box = scene->object("box");
        CHECK(box && _near(box->model.e[3], 2.0f));
    }
    delete scene;
    fails("s:circumscribe(r, {shape = 'circle'})", "circumscribe shape must be 'rectangle' or 'ellipse'");
    fails("s:circumscribe(r, {window = 0})", "circumscribe window must be greater than 0 and at most 1");
    fails("s:circumscribe(r, {width = 0})", "circumscribe width must be positive");
    fails("s:circumscribe(r, {padding = -1})", "circumscribe padding must be zero or more");
    fails("s:circumscribe(r, {spin = 1})", "unknown option 'spin'");
    fails("s:circumscribe(s:group {})", "could not play circumscribe: the target has no geometry at the cursor");

    // A passing flash inside parallel runs along the connector where it is drawn when its track
    // begins, while the other track shifts a box on, and its shape is gone when it ends.
    static constexpr auto passing = R"lua(
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local a = s:rectangle {id = "a", center = {-3, 2}, size = {1, 1}, fill = "#ffffff"}
local b = s:rectangle {id = "b", center = {3, 2}, size = {1, 1}, fill = "#ffffff"}
local edge = s:connector {id = "edge", from = a, to = b}
local mover = s:rectangle {id = "mover", center = {0, -2}, size = {1, 1}, fill = "#ffffff"}
s:parallel(
    function() s:shift(mover, {2, 0}, 2, "linear") end,
    function() s:passing_flash(edge, {color = "#ff0000", width = 2, window = 0.2, curve = "linear"}) end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(passing, static_cast<uint32_t>(std::strlen(passing)), "passing.lua", &scene, error,
                    sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "passing.lua: %s\n", error);
    if (scene) {
        auto flash = scene->object("edge.passing_flash.1.flash");
        CHECK(_near(scene->duration(), 2.0f) && scene->count() == 5u);
        CHECK(flash && !flash->parent() && flash->style.stroke.r == 255 && _near(flash->style.width, 2.0f));
        BBox bounds;
        // Halfway, the stretch spans 0.4 to 0.6 of the connector from x = -2.5 to 2.5.
        CHECK(flash && renderer->bounds(scene, flash, 0.5f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 100.0f) <= 1.0f && std::fabs(bounds.center().y - 60.0f) <= 1.0f
              && std::fabs(bounds.width - 22.0f) <= 1.5f);
        CHECK(flash && renderer->bounds(scene, flash, 1.0f, bounds) == Result::InsufficientCondition);
        auto mover = scene->object("mover");
        CHECK(mover && _near(mover->model.e[3], 2.0f));
    }
    delete scene;
    fails("s:passing_flash(r, {window = 1.5})", "passing_flash window must be greater than 0 and at most 1");
    fails("s:passing_flash(r, {width = -1})", "passing_flash width must be positive");
    fails("s:passing_flash(r, {padding = 1})", "unknown option 'padding'");
    fails("s:passing_flash(s:group {})",
          "could not play passing_flash: the target has no outline of one stroke at the cursor");

    // A flash inside parallel bursts out of the box where it stands when its track begins,
    // while the shift carries the box on, and a flash at a point bursts there; their lines are
    // gone when they end.
    static constexpr auto bursting = R"lua(
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local box = s:rectangle {id = "box", center = {0, 0}, size = {2, 1}, fill = "#ffffff"}
s:parallel(
    function() s:shift(box, {0, 3}, 2, "linear") end,
    function() s:flash(box, {lines = 4, padding = 0.5, size = 1, width = 1, curve = "linear"}) end,
    function() s:flash({-3, -3}, {lines = 6, color = "#ff0000"}) end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(bursting, static_cast<uint32_t>(std::strlen(bursting)), "flash.lua", &scene, error,
                    sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "flash.lua: %s\n", error);
    if (scene) {
        auto line = scene->object("box.flash.1.1");
        auto spark = scene->object("flash.1.6");
        CHECK(_near(scene->duration(), 2.0f) && scene->count() == 11u && !scene->object("box.flash.1.5"));
        CHECK(line && spark && !spark->parent() && spark->style.stroke.r == 255);
        BBox bounds;
        // Halfway the first line is whole: it runs from 1.5 to 2.5 along the x axis.
        CHECK(line && renderer->bounds(scene, line, 0.5f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 140.0f) <= 1.0f && std::fabs(bounds.center().y - 100.0f) <= 1.0f
              && std::fabs(bounds.width - 21.0f) <= 1.5f);
        CHECK(line && renderer->bounds(scene, line, 1.0f, bounds) == Result::InsufficientCondition);
        CHECK(spark && renderer->bounds(scene, spark, 1.0f, bounds) == Result::InsufficientCondition);
        auto box = scene->object("box");
        CHECK(box && _near(box->model.e[7], 3.0f));
    }
    delete scene;
    fails("s:flash(r, {lines = 0})", "flash lines must be from 1 to 64");
    fails("s:flash(r, {lines = 2.5})", "lines must be an integer");
    fails("s:flash(r, {size = 0})", "flash size must be positive");
    fails("s:flash(r, {window = 0.5})", "unknown option 'window'");
    fails("s:flash({1})", "flash point must contain 2 or 3 numbers");

    // A ripple inside parallel grows from the box where it stands when its track begins, along
    // snappy unless told otherwise, while the shift carries the box on, and its ring is gone
    // when it ends.
    static constexpr auto rippling = R"lua(
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local box = s:rectangle {id = "box", center = {0, 0}, size = {2, 1}, corner = 0.2, fill = "#ffffff"}
s:parallel(
    function() s:shift(box, {0, 3}, 2, "linear") end,
    function() s:ripple(box, {size = 1, width = 1}) end,
    function() s:wait(1); s:ripple(box, {padding = 0.5, color = "#ff0000", curve = "linear"}) end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(rippling, static_cast<uint32_t>(std::strlen(rippling)), "ripple.lua", &scene, error,
                    sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "ripple.lua: %s\n", error);
    if (scene) {
        auto first = scene->object("box.ripple.1.ring");
        auto second = scene->object("box.ripple.2.ring");
        CHECK(_near(scene->duration(), 2.0f) && scene->count() == 3u);
        CHECK(first && second && !second->parent() && second->style.stroke.r == 255);
        BBox bounds;
        // Halfway along snappy the first ring has grown 1 - 0.5^4 of the way, to 3.875 wide.
        CHECK(first && renderer->bounds(scene, first, 0.5f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - 100.0f) <= 1.0f && std::fabs(bounds.center().y - 100.0f) <= 1.0f
              && std::fabs(bounds.width - 78.5f) <= 1.5f);
        CHECK(first && renderer->bounds(scene, first, 1.0f, bounds) == Result::InsufficientCondition);
        // The second starts 0.5 out from the box at y = 1.5 and grows 0.4 on every side.
        CHECK(second && renderer->bounds(scene, second, 1.5f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().y - 70.0f) <= 1.0f && std::fabs(bounds.width - 71.0f) <= 1.5f
              && std::fabs(bounds.height - 51.0f) <= 1.5f);
        CHECK(second && renderer->bounds(scene, second, 2.0f, bounds) == Result::InsufficientCondition);
        auto box = scene->object("box");
        CHECK(box && _near(box->model.e[7], 3.0f));
    }
    delete scene;
    fails("s:ripple(r, {size = 0})", "ripple size must be positive");
    fails("s:ripple(r, {padding = -1})", "ripple padding must be zero or more");
    fails("s:ripple(r, {width = 0})", "ripple width must be positive");
    fails("s:ripple(r, {window = 0.5})", "unknown option 'window'");
    fails("s:ripple(s:group {})", "could not play ripple: the target has no geometry at the cursor");
}

// A text without a box counts with its glyphs as the scene sets them, so the frame of a
// circumscribe hugs the same box a measured twin of the text takes.
static void _framingTexts(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local word = s:text {id = "word", text = "word", point = {1, 2}, size = 28}
s:text {id = "twin", text = "word", point = {1, -2}, size = 28, measure = true}
s:circumscribe(word, {padding = 0, width = 1})
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "frame-text.lua", &scene, error,
                    sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "frame-text.lua: %s\n", error);
    if (scene) {
        auto frame = scene->object("word.circumscribe.1.frame");
        auto twin = scene->object("twin");
        Bounds box;
        BBox bounds;
        CHECK(frame && twin && twin->bounds(box) && box.size().x > 0.5f);
        CHECK(frame && renderer->bounds(scene, frame, 0.5f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().x - (100.0f + 20.0f * box.center().x)) <= 1.0f);
        CHECK(std::fabs(bounds.center().y - (100.0f - 20.0f * (box.center().y + 4.0f))) <= 1.0f);
        CHECK(std::fabs(bounds.width - (20.0f * box.size().x + 1.0f)) <= 1.5f);
        CHECK(std::fabs(bounds.height - (20.0f * box.size().y + 1.0f)) <= 1.5f);
    }
    delete scene;
}

static void _surrounding()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local word = s:text {id = "word", text = "word", point = {1, 1}, align = {0, 0}, size = 20, measure = true}
local dot = s:circle {id = "dot", center = {-2, -1}, radius = 0.5}
local holder = s:group {id = "holder", matrix = {2, 0, 0, 1, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}
local inner = holder:rectangle {id = "inner", center = {0, 0}, size = {1, 1}}
s:rectangle {id = "hug", around = word}
s:rectangle {id = "both", around = {word, dot}, padding = {0.2, 0}}
s:rectangle {id = "scaled", around = inner, padding = 0}
holder:rectangle {id = "local", around = inner, padding = 0}
s:wait(1)
s:rectangle {id = "late", around = dot, padding = 0, corner = 0.1}
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "around.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "around.lua: %s\n", error);
    if (scene) {
        auto rectangle = [&](const char* id) { return static_cast<Rectangle*>(scene->object(id)); };
        auto word = static_cast<Text*>(scene->object("word"));
        CHECK(word && word->box.x > 0.0f && word->box.y > 0.0f);
        auto hug = rectangle("hug");
        if (word && hug) {
            CHECK(_near(hug->center.x, 1.0f + word->box.x * 0.5f) && _near(hug->center.y, 1.0f - word->box.y * 0.5f));
            CHECK(_near(hug->size.x, word->box.x + 0.2f) && _near(hug->size.y, word->box.y + 0.2f));
        }
        auto both = rectangle("both");
        if (word && both) {
            CHECK(_near(both->size.x, 1.0f + word->box.x + 2.5f + 0.4f) && _near(both->size.y, 2.5f));
            CHECK(_near(both->center.x, (-2.5f + 1.0f + word->box.x) * 0.5f) && _near(both->center.y, -0.25f));
        }
        // Bounds pass through the groups between, so a rectangle at the root covers the scaled
        // one, and one in the same group covers it in the group's own units.
        auto scaled = rectangle("scaled");
        CHECK(scaled && _near(scaled->center.x, 1.0f) && _near(scaled->center.y, 0.0f));
        CHECK(scaled && _near(scaled->size.x, 2.0f) && _near(scaled->size.y, 2.0f));
        auto inside = rectangle("local");
        CHECK(inside && inside->parent() == scene->object("holder"));
        CHECK(inside && _near(inside->center.x, 0.0f) && _near(inside->size.x, 1.0f) && _near(inside->size.y, 1.0f));
        // Later in the timeline it is born around them.
        auto late = rectangle("late");
        CHECK(late && _near(late->center.x, -2.0f) && _near(late->center.y, -1.0f) && _near(late->size.x, 1.0f));
        CHECK(late && _near(late->corner, 0.1f));
    }
    delete scene;

    static constexpr auto prefix =
        "local s=vis.scene{}; local word=s:text{text='w', measure=true}; local holder=s:group{}; ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:rectangle{around = word, center = {0, 0}}", "around sets the center and size, so leave out center and size");
    fails("s:rectangle{around = word, size = {1, 1}}", "around sets the center and size, so leave out center and size");
    fails("s:rectangle{padding = 1}", "padding needs around");
    fails("s:rectangle{around = 5}", "around must be a handle or a list of handles");
    fails("s:rectangle{around = {}}", "around must be a handle or a list of handles");
    fails("s:rectangle{around = {word, 1}}", "around must be a handle or a list of handles");
    fails("s:rectangle{around = word, padding = -1}", "padding must be a number or {x, y} of zero or more");
    fails("holder:rectangle{around = word}", "around takes objects below the group the rectangle joins");
    fails("s:rectangle{around = s:group{}}", "around needs objects with bounds, such as texts set with measure = true");
}

// scene:bounds reports in world units what scene:rectangle{around} frames, through the groups
// above, where everything authored so far leaves the objects.
static void _boundsQueries()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local word = s:text {text = "word", point = {1, 1}, align = {0, 0}, size = 20, measure = true}
local dot = s:circle {center = {-2, -1}, radius = 0.5}
local holder = s:group {matrix = {2, 0, 0, 1, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}
local inner = holder:rectangle {center = {0, 0}, size = {1, 1}}
local function near(a, b) return math.abs(a - b) < 1e-4 end
local function same(a, b)
    return near(a.left, b.left) and near(a.right, b.right) and near(a.bottom, b.bottom) and near(a.top, b.top)
end
local b = s:bounds(dot)
if not (near(b.left, -2.5) and near(b.right, -1.5) and near(b.bottom, -1.5) and near(b.top, -0.5)) then
    error("sides")
end
if not (near(b.width, 1) and near(b.height, 1)) then error("size") end
if not (near(b.center[1], -2) and near(b.center[2], -1) and near(b.center[3], 0)) then error("center") end
if not (near(b.min[1], -2.5) and near(b.min[2], -1.5) and near(b.max[1], -1.5) and near(b.max[2], -0.5)) then
    error("corners")
end
local w = s:bounds(word)
if not (near(w.left, 1) and near(w.top, 1) and w.width > 0.1 and w.height > 0.1) then error("word") end
local i = s:bounds(inner)
if not (near(i.left, 0) and near(i.right, 2) and near(i.bottom, -1) and near(i.top, 1)) then error("groups") end
local both = s:bounds({word, dot})
if not (near(both.left, -2.5) and near(both.right, w.right) and near(both.top, 1)) then error("list") end
local frame = s:rectangle {around = {word, dot}, padding = 0}
if not same(s:bounds(frame), both) then error("around") end
s:shift(dot, {1, 0}, 1)
local moved = s:bounds(dot)
if not (near(moved.left, -1.5) and near(moved.right, -0.5)) then error("moved") end
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "bounds.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "bounds.lua: %s\n", error);
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; ";
    char script[256];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:bounds()", "bounds takes a handle or a list of handles");
    fails("s:bounds(5)", "bounds takes a handle or a list of handles");
    fails("s:bounds({})", "bounds takes a handle or a list of handles");
    fails("s:bounds({s:circle{}, 1})", "bounds takes a handle or a list of handles");
    fails("s:bounds(s:group{})", "bounds needs objects with bounds, such as texts set with measure = true");
}

// A brace lies along the side of what it spans that faces its direction, buff off it, its ends
// where that side ends and its tip depth out; it is set with its text, so its sizes follow the
// text's size, and a label stands gap off its tip, a string as the ink of a text, a handle by
// its bounds.
static void _bracing(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 400, height = 300, camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 300}}
local function near(a, b) return math.abs(a - b) < 1e-3 end
local function sides(b, left, right, bottom, top)
    return near(b.left, left) and near(b.right, right) and near(b.bottom, bottom) and near(b.top, top)
end
local dot = s:circle {id = "dot", center = {0, 50}, radius = 30}
-- One world unit is a pixel, so a size of 20 sets the brace 5 off the circle and 8 deep.
local under, none = s:brace {around = dot, id = "under", size = 20}
if none ~= nil then error("a brace without a label returns it alone") end
if not sides(s:bounds(under), -30, 30, 7, 15) then error("under") end
if not sides(s:bounds(s:brace {around = dot, direction = "right", buff = 2, depth = 6}), 32, 38, 20, 80) then
    error("right")
end
if not sides(s:bounds(s:brace {around = dot, direction = "up", buff = 0, depth = 3}), -30, 30, 80, 83) then
    error("up")
end
if not sides(s:bounds(s:brace {around = dot, direction = {-2, 0}, buff = 1, depth = 4}), -35, -31, 20, 80) then
    error("left")
end
-- A label string stands gap off the tip by its ink, its middle on the tip.
local over, label = s:brace {around = dot, direction = "up", label = "radius", size = 20, id = "over"}
local l, o = s:bounds(label), s:bounds(over)
if not (near(l.bottom, o.top + 4) and near(l.left + l.right, 0)) then error("label") end
if s:find("over/label") ~= label then error("label id") end
-- A label handle moves there by its bounds, and comes back as the second value.
local note = s:text {text = "r", point = {100, 100}, measure = true, size = 20}
local _, moved = s:brace {around = dot, direction = "left", label = note, gap = 3, depth = 6, buff = 2}
if moved ~= note then error("label handle") end
local n = s:bounds(note)
if not (near(n.right, -41) and near(n.bottom + n.top, 100)) then error("label place") end
-- Between two points, below a segment drawn left to right unless a direction says otherwise.
if not sides(s:bounds(s:brace {from = {-100, -100}, to = {100, -100}, buff = 0, depth = 10}), -100, 100, -110, -100) then
    error("segment")
end
if not sides(s:bounds(s:brace {from = {-100, -100}, to = {100, -100}, direction = {1, 1}, buff = 2, depth = 10}),
             -100, 100, -98, -88) then
    error("segment side")
end
-- A slanted brace meets the corner of a square: its ends lie on the line through the corner.
local b = s:bounds(s:brace {around = s:rectangle {center = {0, -50}, size = {20, 20}}, direction = {1, 1}, buff = 0, depth = 4})
if not (near(b.left, 0) and near(b.bottom, -50)) then error("slanted") end
-- In a group, in the group's units.
local holder = s:group {matrix = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}
local inner = holder:rectangle {center = {0, -100}, size = {10, 4}}
if not sides(s:bounds(holder:brace {around = inner, buff = 1, depth = 2}), -10, 10, -210, -206) then error("group") end
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "brace.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "brace.lua: %s\n", error);
    if (scene) {
        auto under = scene->object("under");
        CHECK(under && under->type() == Type::Path && static_cast<Path*>(under)->closed());
        if (under) {
            // Filled as the text it is set with, without an outline.
            auto& color = scene->theme().text.color;
            auto& fill = under->style.fill;
            CHECK(fill.r == color.r && fill.g == color.g && fill.b == color.b && fill.a == color.a);
            CHECK(under->style.stroke.a == 0u);
        }
        // The label's ink, as painted, stands 4 pixels above the brace.
        BBox brace;
        BBox label;
        CHECK(renderer->bounds(scene, scene->object("over"), 0.0f, brace) == Result::Success);
        CHECK(renderer->bounds(scene, scene->object("over/label"), 0.0f, label) == Result::Success);
        CHECK(std::fabs(brace.y - label.y - label.height - 4.0f) < 0.75f);
        CHECK(std::fabs(label.x + 0.5f * label.width - brace.x - 0.5f * brace.width) < 0.75f);
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local c=s:circle{}; ";
    char script[320];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    constexpr auto direction = "brace direction must be 'down', 'up', 'left', 'right' or a vector in the xy plane";
    fails("s:brace{}", "brace needs around, or from and to");
    fails("s:brace{around=c, from={0,0}, to={1,0}}", "brace takes around, or from and to, not both");
    fails("s:brace{from={0,0}}", "brace needs both from and to");
    fails("s:brace{from={1,1}, to={1,1}}", "brace from and to must be different points");
    fails("s:brace{from={0,0}, to={1,0}, direction='right'}", "brace direction must not run along from and to");
    fails("s:brace{from={0,0,0}, to={1,0,1}}", "brace from and to must lie at one depth");
    fails("s:brace{around=c, direction='sideways'}", direction);
    fails("s:brace{around=c, direction={0,0}}", direction);
    fails("s:brace{around=c, direction={0,0,1}}", direction);
    fails("s:brace{around=c, direction=3}", direction);
    fails("s:brace{around=c, label=5}", "brace label must be a string or a handle");
    fails("s:brace{around=c, depth=0}", "brace depth must be positive");
    fails("s:brace{around=c, buff=-1}", "brace buff and gap must be zero or more");
    fails("s:brace{around=c, gap=-1}", "brace buff and gap must be zero or more");
    fails("s:brace{around=c, align={0,0}}", "unknown option 'align'");
    fails("s:brace{around={}}", "around must be a handle or a list of handles");
    fails("s:brace{around={c, 1}}", "around must be a handle or a list of handles");
    fails("s:brace{around=s:group{}}", "around needs objects with bounds, such as texts set with measure = true");
    fails("s:brace{around=s:point{point={0,0}}}", "brace needs something to span across its direction");
    fails("local g=s:group{}; g:brace{around=c}", "around takes objects below the group the brace joins");
    fails("local g=s:group{}; s:brace{around=c, label=g:circle{}}",
          "a brace label handle must be a child of the group the brace joins");
    fails("s:brace{around=c, label=s:group{}}", "the brace label has no bounds yet");
}

// A matrix sets its entries in columns as wide as their widest entry, on shared baselines,
// between brackets that stand clear of them, as a group of rows and then the brackets.
static void _matrices()
{
    {
        constexpr auto source = R"lua(
local s = vis.scene {width=640,height=360,camera={height=360}}
local n = s:number{value=1.5,decimals=2,reserve="-000.00",align={1,0.5},id="n"}
s:text{text="x",reserve="long text",id="t"}
local m = s:matrix{rows={{1,2},{3,4}},reserve={"-000.00","00"},justify="right",id="m"}
s:matrix{rows={{1},{2}},reserve="-000.00",id="u"}
s:play({target=n,value=12345678},1,"linear")
return s
)lua";
        Scene* reserved = nullptr;
        char error[256] = {};
        CHECK(Lua::load(source, std::strlen(source), "reserved.lua", &reserved, error, sizeof(error)) == Result::Success);
        if (reserved) {
            auto n = static_cast<Text*>(reserved->object("n"));
            TextExtent sample;
            CHECK(reserved->measure(n, "-000.00", sample) == Result::Success);
            CHECK(_near(n->box.x, sample.width) && _near(n->align.x, 1));
            auto a = static_cast<Text*>(reserved->object("m/1/1"));
            auto b = static_cast<Text*>(reserved->object("m/2/1"));
            CHECK(_near(a->box.x, sample.width) && _near(b->box.x, sample.width));
            CHECK(_near(a->align.x, 1) && _near(b->align.x, 1) && _near(a->point.x, b->point.x));
            CHECK(_near(static_cast<Text*>(reserved->object("u/1/1"))->box.x, sample.width));
            auto t = static_cast<Text*>(reserved->object("t"));
            CHECK(reserved->measure(t, "long text", sample) == Result::Success && _near(t->box.x, sample.width));
            CHECK(n->measure(sample) == Result::Success && sample.width > n->box.x);
            delete reserved;
        }
        _scriptError("local s=vis.scene{}; s:number{reserve=false}; return s", "reserve must be a string");
        _scriptError("local s=vis.scene{}; s:matrix{rows={{1,2}},reserve={'0'}}; return s",
                     "matrix reserve needs one string per column");
    }
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local m = s:matrix {rows = {{2, -1, 0}, {1, 3, 4}, {0, 5, -2}}, point = {-2, 1}, size = 24, id = "m"}
s:matrix {rows = {{"a", "bb"}, {"ccc", "d"}}, point = {2, 1}, brackets = "round", justify = {"left", "right"},
          gap = {0.5, 0.2}, size = 24, color = "#0000ff", id = "r"}
s:matrix {rows = {{0.5, 2.25}}, brackets = "bar", point = {-2, -2}, size = 24, stroke = "#ff0000", width = 3, id = "b"}
s:matrix {rows = {{1}}, brackets = "none", decimals = 2, point = {2, -2}, align = {0, 1}, size = 24, id = "n"}
s:group {}:matrix {rows = {{1, 2}}}
if m:entry(2, 3) ~= s:find("m/2/3") or m:row(1) ~= s:find("m/1") then error("names") end
if #m:children() ~= 5 or m:children()[4] ~= s:find("m/left") then error("rows, then brackets") end
if #m:column(2) ~= 3 or m:column(2)[3] ~= s:find("m/3/2") then error("a column") end
s:play({target = m:entry(2, 3), value = 7}, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "matrix.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "matrix.lua: %s\n", error);
    if (scene) {
        auto text = [&](const char* id) { return static_cast<Text*>(scene->object(id)); };
        auto path = [&](const char* id) { return static_cast<Path*>(scene->object(id)); };
        auto blank = Text::gen(" ");
        if (blank) blank->size = 24.0f;
        TextExtent space;
        CHECK(blank && scene->measure(blank, space) == Result::Success);
        delete blank;
        auto line = space.ascent + space.descent;
        auto left = [](const Text* t) { return t->point.x - t->align.x * t->box.x; };
        auto top = [](const Text* t) { return t->point.y + t->align.y * t->box.y; };

        auto matrix = scene->object("m");
        CHECK(matrix && matrix->type() == Type::Group && static_cast<Group*>(matrix)->typeset);
        CHECK(matrix && matrix->childCount() == 5u);
        CHECK(scene->object("m/2") && scene->object("m/2")->childCount() == 3u);
        // Numbers count, written with the fewest digits they need.
        auto minus = text("m/1/2");
        CHECK(minus && minus->counting() && _near(minus->value(), -1.0f) && std::strcmp(minus->text(), "-1") == 0);
        // A column is as wide as its widest entry, and the next starts a line height later.
        auto a = text("m/1/1");
        auto b = text("m/1/2");
        auto c = text("m/3/2");
        CHECK(a && b && c && _near(b->box.x, c->box.x) && b->box.x > a->box.x);
        if (a && b) {
            CHECK(_near(left(a) + a->box.x + line, left(b)));
            CHECK(_near(top(a), top(b)) && _near(a->box.y, line));
        }
        // Rows stack half a line height apart, and every entry sits centered in its cell.
        auto below = text("m/2/1");
        if (a && below) CHECK(_near(top(a) - line - 0.5f * line, top(below)));
        CHECK(a && _near(a->align.x, 0.5f));
        // The brackets reach past the entries and turn toward them, and the whole matrix is
        // centered on its point.
        Bounds box;
        CHECK(matrix && matrix->bounds(box));
        CHECK(_near((box.min.x + box.max.x) * 0.5f, -2.0f) && _near((box.min.y + box.max.y) * 0.5f, 1.0f));
        auto bracket = path("m/left");
        CHECK(bracket && bracket->type() == Type::Path && !bracket->closed() && bracket->count() == 4u);
        if (bracket && a) {
            auto points = bracket->points();
            CHECK(_near(points[0].x - points[1].x, 0.3f * line) && _near(points[1].x, box.min.x));
            CHECK(_near(left(a) - points[0].x, 0.3f * line) && _near(points[0].y, top(a) + 0.15f * line));
            CHECK(_near(bracket->style.width, 2.0f));
            CHECK(_sameColor(bracket->style.stroke, scene->theme().text.color));
        }
        auto right = path("m/right");
        CHECK(right && right->count() == 4u && right->points()[1].x > right->points()[0].x);

        // Round brackets bulge outward halfway down; justify sets each column across.
        auto paren = path("r/left");
        CHECK(paren && paren->count() > 4u);
        if (paren) {
            auto points = paren->points();
            auto middle = points[paren->count() / 2u];
            CHECK(middle.x < points[0].x - 0.2f * line);
            CHECK(_sameColor(paren->style.stroke, Color{0, 0, 255, 255}));
        }
        auto first = text("r/1/1");
        auto last = text("r/2/2");
        auto widest = text("r/2/1");
        CHECK(first && _near(first->align.x, 0.0f) && widest && _near(left(first), left(widest)));
        CHECK(last && _near(last->align.x, 1.0f));
        CHECK(first && _sameColor(first->style.fill, Color{0, 0, 255, 255}));
        if (first && widest) CHECK(_near(top(first) - widest->box.y - 0.2f, top(widest)));

        // A bar is one stroke; decimals set every number, and none leaves the rows alone.
        auto bar = path("b/right");
        CHECK(bar && bar->count() == 2u && _near(bar->style.width, 3.0f));
        CHECK(bar && _sameColor(bar->style.stroke, Color{255, 0, 0, 255}));
        CHECK(text("b/1/1") && std::strcmp(text("b/1/1")->text(), "0.5") == 0);
        CHECK(text("b/1/2") && std::strcmp(text("b/1/2")->text(), "2.25") == 0);
        auto lone = scene->object("n");
        CHECK(lone && lone->childCount() == 1u && !scene->object("n/left"));
        auto one = text("n/1/1");
        CHECK(one && std::strcmp(one->text(), "1.00") == 0);
        if (one) CHECK(_near(left(one), 2.0f) && _near(top(one) - one->box.y, -2.0f));
        CHECK(scene->clipCount() == 1u);
    }
    delete scene;

    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "local s=vis.scene{}; %s return s", body);
        _scriptError(script, expected);
    };
    fails("s:matrix{}", "matrix rows must be a list of rows, each a list of entries");
    fails("s:matrix{rows = {}}", "matrix rows must be a list of rows, each a list of entries");
    fails("s:matrix{rows = {{1}, 2}}", "matrix row 2 must be a list of at least one entry");
    fails("s:matrix{rows = {{1, 2}, {3}}}", "matrix rows must all hold as many entries as the first, 2; row 2 holds 1");
    fails("s:matrix{rows = {{true}}}", "a matrix entry must be a number or a string, got boolean");
    fails("s:matrix{rows = {{1/0}}}", "matrix entries must be finite numbers");
    fails("s:matrix{rows = {{1}}, brackets = 'curly'}", "brackets must be 'square', 'round', 'bar' or 'none'");
    fails("s:matrix{rows = {{1}}, gap = -1}", "gap must be a number or {x, y} of zero or more");
    fails("s:matrix{rows = {{1, 2}}, justify = {'left'}}",
          "matrix justify must be 'left', 'center' or 'right', or a list of one for each of its 2 columns");
    fails("s:matrix{rows = {{1}}, justify = 'middle'}", "matrix justify must be 'left', 'center' or 'right'");
    fails("s:matrix{rows = {{1}}, decimals = 7}", "decimals must be a whole number from 0 to 6");
    fails("s:matrix{rows = {{1}}, dash = {1}}", "unknown option 'dash'");
    fails("local rows = {} for i = 1, 33 do rows[i] = {} for j = 1, 32 do rows[i][j] = 0 end end s:matrix{rows = rows}",
          "a matrix holds at most 1024 entries");
}

// A table tiles its cells, the header's first, with stripes one layer below every second row
// under the header and rules on the edges between cells.
static void _tables()
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 640, height = 360}
local t = s:table {header = {"n", "n²"}, rows = {{1, 1}, {2, 4}, {3, 9}}, point = {-2, 1}, size = 20, stripes = true,
                   rules = "columns", id = "t"}
s:table {rows = {{"a", "bb"}, {"ccc", "d"}}, point = {2, 1}, padding = {0.2, 0.1}, justify = {"left", "right"},
         rules = "grid", stroke = "#ff0000", width = 3, stripes = "#00ff00", fill = "#0000ff", layer = 2, id = "g"}
s:table {rows = {{1}}, point = {2, -2}, align = {0, 1}, padding = 0, size = 20, id = "bare"}
if t:row(1) ~= s:find("t/1") or t:entry(4, 2) ~= s:find("t/4/2") then error("names") end
if #t:column(1) ~= 4 or #t:children() ~= 7 then error("the header is the first row, then stripes and rules") end
s:play({target = s:find("bare/1/1"), value = 16}, 1)
return s
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "table.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "table.lua: %s\n", error);
    if (scene) {
        auto& theme = scene->theme();
        auto text = [&](const char* id) { return static_cast<Text*>(scene->object(id)); };
        auto blank = Text::gen(" ");
        if (blank) blank->size = 20.0f;
        TextExtent space;
        CHECK(blank && scene->measure(blank, space) == Result::Success);
        delete blank;
        auto line = space.ascent + space.descent;
        auto left = [](const Text* t) { return t->point.x - t->align.x * t->box.x; };
        auto top = [](const Text* t) { return t->point.y + t->align.y * t->box.y; };

        auto table = scene->object("t");
        CHECK(table && table->type() == Type::Group && static_cast<Group*>(table)->typeset);
        CHECK(table && table->childCount() == 7u);
        // The header takes the accent color, and numbers count.
        auto head = text("t/1/1");
        CHECK(head && std::strcmp(head->text(), "n") == 0);
        CHECK(head && _sameColor(head->style.fill, theme.color(ThemeColorRole::Accent)));
        auto one = text("t/2/1");
        CHECK(one && one->counting() && _near(one->value(), 1.0f));
        // Cells tile: each is its column's widest entry and the padding wide, and a line
        // height and the padding tall.
        auto beside = text("t/2/2");
        auto below = text("t/3/1");
        if (one && beside && below && head) {
            CHECK(_near(left(one) + one->box.x, left(beside)) && _near(top(one) - one->box.y, top(below)));
            CHECK(_near(one->box.y, 1.6f * line) && _near(top(head) - head->box.y, top(one)));
            auto widest = 0.0f;
            const char* column2[] = {"t/1/2", "t/2/2", "t/3/2", "t/4/2"};
            for (auto id : column2) {
                TextExtent glyphs;
                CHECK(text(id) && scene->measure(text(id), glyphs) == Result::Success);
                if (glyphs.width > widest) widest = glyphs.width;
            }
            CHECK(_near(beside->box.x, widest + 1.6f * line));
            CHECK(_near(one->align.x, 0.5f) && _near(one->align.y, 0.5f));
        }
        Bounds box;
        CHECK(table && table->bounds(box));
        CHECK(_near((box.min.x + box.max.x) * 0.5f, -2.0f) && _near((box.min.y + box.max.y) * 0.5f, 1.0f));
        // One stripe behind the second row under the header, then the rule under the header
        // and the one between the columns.
        auto stripe = table ? table->childAt(4u) : nullptr;
        CHECK(stripe && stripe->type() == Type::Rectangle && stripe->layer == -1);
        if (stripe && below) {
            auto band = static_cast<Rectangle*>(stripe);
            CHECK(_sameColor(band->style.fill, theme.color(ThemeColorRole::Surface)) && band->style.stroke.a == 0);
            CHECK(_near(band->center.y, top(below) - 0.5f * below->box.y) && _near(band->size.x, box.max.x - box.min.x));
        }
        auto rule = table ? table->childAt(5u) : nullptr;
        CHECK(rule && rule->type() == Type::Line);
        if (rule && one) {
            auto ruled = static_cast<Line*>(rule);
            CHECK(_near(ruled->from.y, top(one)) && _near(ruled->to.y, top(one)) && _near(ruled->from.x, box.min.x));
            CHECK(_sameColor(ruled->style.stroke, theme.color(ThemeColorRole::Foreground)));
            CHECK(_near(ruled->style.width, 1.5f));
        }
        auto column = table ? table->childAt(6u) : nullptr;
        CHECK(column && column->type() == Type::Line);
        if (column && beside) {
            auto ruled = static_cast<Line*>(column);
            CHECK(_near(ruled->from.x, left(beside)));
            CHECK(_near(ruled->from.y, box.max.y) && _near(ruled->to.y, box.min.y));
            CHECK(_sameColor(ruled->style.stroke, theme.color(ThemeColorRole::Border)));
            CHECK(_near(ruled->style.width, 1.0f));
        }

        // A grid adds the frame; stroke and width set every rule, fill every text, and layer
        // every part but the stripes, one below.
        auto grid = scene->object("g");
        CHECK(grid && grid->childCount() == 9u);
        if (grid && grid->childCount() == 9u) {
            CHECK(grid->childAt(2u)->type() == Type::Rectangle && grid->childAt(2u)->layer == 1);
            CHECK(_sameColor(grid->childAt(2u)->style.fill, Color{0, 255, 0, 255}));
            for (auto i = 3u; i < 9u; i++) {
                CHECK(grid->childAt(i)->type() == Type::Line && grid->childAt(i)->layer == 2);
                CHECK(_sameColor(grid->childAt(i)->style.stroke, Color{255, 0, 0, 255}));
                CHECK(_near(grid->childAt(i)->style.width, 3.0f));
            }
        }
        auto a = text("g/1/1");
        auto d = text("g/2/2");
        CHECK(a && a->layer == 2 && _sameColor(a->style.fill, Color{0, 0, 255, 255}));
        if (a && d) {
            TextExtent glyphs;
            CHECK(scene->measure(a, glyphs) == Result::Success);
            CHECK(_near(a->point.x - a->align.x * glyphs.width, left(a) + 0.2f));
            CHECK(scene->measure(d, glyphs) == Result::Success);
            CHECK(_near(d->point.x + (1.0f - d->align.x) * glyphs.width, left(d) + d->box.x - 0.2f));
        }
        // Without a header nothing is ruled, and a table without padding hugs its glyphs.
        auto bare = scene->object("bare");
        CHECK(bare && bare->childCount() == 1u);
        auto lone = text("bare/1/1");
        CHECK(lone && _near(left(lone), 2.0f) && _near(top(lone) - lone->box.y, -2.0f) && _near(lone->box.y, line));
        CHECK(scene->clipCount() == 1u);
    }
    delete scene;

    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "local s=vis.scene{}; %s return s", body);
        _scriptError(script, expected);
    };
    fails("s:table{}", "table rows must be a list of rows, each a list of entries");
    fails("s:table{rows = {{1, 2}}, header = {'a'}}",
          "table header must be a list of one entry for each of its 2 columns");
    fails("s:table{rows = {{1}}, header = {true}}", "a table entry must be a number or a string, got boolean");
    fails("s:table{rows = {{1}}, rules = 'dots'}", "rules must be 'header', 'rows', 'columns', 'grid' or 'none'");
    fails("s:table{rows = {{1}}, stripes = 3}", "stripes must be true or a color");
    fails("s:table{rows = {{1}}, padding = {-1, 0}}", "padding must be a number or {x, y} of zero or more");
    fails("s:table{rows = {{1}}, brackets = 'round'}", "unknown option 'brackets'");
    fails("local rows, head = {}, {} "
          "for i = 1, 32 do head[i] = 'h' rows[i] = {} for j = 1, 32 do rows[i][j] = 0 end end "
          "s:table{rows = rows, header = head}",
          "a table holds at most 1024 entries");
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
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "locale.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "locale.lua: %s\n", error);
    delete scene;
    CHECK(std::localeconv() && std::strcmp(std::localeconv()->decimal_point, "."));
    std::setlocale(LC_NUMERIC, previous);
}

static const Asset* _asset(const char* name, void* data)
{
    return std::strcmp(name, "pixels") == 0 ? static_cast<const Asset*>(data) : nullptr;
}

static void _assets(SwRenderer* renderer)
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
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "assets.lua",
                            &scene, error, sizeof(error), _asset, asset);
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "assets.lua: %s\n", error);
    if (scene) {
        CHECK(!scene->config().antialiasing);
        CHECK(scene->object("image") && scene->object("image")->type() == Type::Picture);
        CHECK(scene->object("buffer") && scene->object("buffer")->type() == Type::Picture);
        auto buffer = static_cast<Picture*>(scene->object("buffer"));
        CHECK(buffer && buffer->pixelWidth() == 2u && buffer->pixelHeight() == 2u);
        CHECK(buffer && buffer->pixels()[0] == _pixel(Color::hex("#ff4050")));
        CHECK(buffer && buffer->pixels()[3] == _pixel(Color::hex("#ffd166")));
        CHECK(scene->object("cells") && scene->object("cells")->type() == Type::Cell);
        Surface surface;
        CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 2000);
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

static void _viewports(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local root = vis.scene {width = 200, height = 100, background = "#101820"}
local left = vis.scene {width = 100, height = 100, background = "#d84040", camera = {height = 4}}
local leftSpace = left:space {x = {-2, 2, 1}, y = {-2, 2, 1}, opacity = 0}
local leftVector = leftSpace:vector {value = {1.5, 1}, color = "#ffffff", width = 3}
left:create(leftVector, 0.5, "linear")
local right = vis.scene {width = 100, height = 100, background = "#4080d8", camera = {height = 8}}
local rightSpace = right:space {x = {-4, 4, 1}, y = {-4, 4, 1}, opacity = 0}
local rightVector = rightSpace:vector {value = {-2, 2}, color = "#ffffff", width = 3}
right:create(rightVector, 1.2, "linear")
local edge = vis.scene {width = 20, height = 20, background = "#00000000"}
root:viewport(left, {x = 0, y = 0, width = 0.5, height = 1})
root:viewport(right, {x = 0.5, y = 0, width = 0.5, height = 1})
root:viewport(edge, {x = 0.8, y = 0, width = 0.2, height = 0.2})
for i = 4, 64 do
    local child = vis.scene {width = 1, height = 1, background = "#00000000"}
    root:viewport(child, {x = 0, y = 0, width = 0.01, height = 0.01})
end
return root
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "viewports.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "viewports.lua: %s\n", error);
    if (scene) {
        CHECK(scene->viewportCount() == 64);
        CHECK(scene->count() == 0);
        CHECK(scene->sceneAt(0) && scene->sceneAt(0)->count() == 2);
        CHECK(scene->sceneAt(1) && scene->sceneAt(1)->count() == 2);
        CHECK(std::fabs(scene->duration() - 1.2f) < 1.0e-4f);
        Surface surface;
        CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        if (surface.data()) {
            CHECK(surface.data()[50u * surface.stride() + 20u] == _pixel(Color::hex("#d84040")));
            CHECK(surface.data()[50u * surface.stride() + 180u] == _pixel(Color::hex("#4080d8")));
        }
    }
    delete scene;

}

static void _sceneTransitions(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local root = vis.scene {width = 200, height = 100, background = "#080d14", antialiasing = false}
local first = vis.scene {width = 100, height = 100, background = "#101820", camera = {height = 4}}
local dot = first:circle {center = {-0.8, 0}, radius = 0.65, fill = "#4cc9f0", id = "hero"}
first:line {from = {-1.6, -1.2}, to = {0, -1.2}, stroke = "#7bd88f", width = 3, id = "guide-a"}
first:shift(dot, {1, 0}, 2, "linear")
local second = vis.scene {width = 200, height = 100, background = "#1c1730", camera = {height = 5}}
second:rectangle {center = {1.1, 0.2}, size = {1.8, 1.2}, corner = 0.16,
    fill = "#f72585", id = "hero"}
second:line {from = {0.2, -1.5}, to = {2, -1.5}, stroke = "#ffd166", width = 3, id = "guide-b"}
root:scene_transition({first, second}, {
    duration = 0.6, hold = {0.1, 0.3}, curve = "linear",
    viewport = {x = 0.1, y = 0.1, width = 0.8, height = 0.8}
})
if math.abs(root:duration() - 1.0) > 0.0001 then error("transition duration mismatch") end
return root
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                            "scene-transition.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "scene-transition.lua: %s\n", error);
    if (scene) {
        LayoutReport report;
        CHECK(renderer->layout(scene, 0.05f, report) == Result::Success);
        CHECK(report.sceneCount() == 2u && std::fabs(report.sceneAt(1)->time - 0.05f) < 1e-4f);
        CHECK(renderer->layout(scene, 0.4f, report) == Result::Success);
        CHECK(report.sceneCount() == 3u && std::fabs(report.sceneAt(1)->time - 0.4f) < 1e-4f);
        CHECK(report.sceneCount() == 3u && report.sceneAt(2)->time == 0);
        CHECK(scene->count() == 0u);
        CHECK(std::fabs(scene->duration() - 1.0f) < 1.0e-4f);
        Surface surface;
        CHECK(renderer->render(scene, 0.1f, surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 100u);
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 100u);
        CHECK(renderer->render(scene, 0.9f, surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 100u);
    }
    delete scene;
}

static void _transitionStyles(SwRenderer* renderer)
{
    // A style for every transition, or one for each.
    static constexpr auto source = R"lua(
local function stage(background)
    local scene = vis.scene {width = 120, height = 60, background = background}
    scene:rectangle {center = {0, 0}, size = {1, 1}, fill = "#ffffff", id = "card"}
    return scene
end
local root = vis.scene {width = 120, height = 60}
local inner = vis.scene {width = 120, height = 60}
inner:scene_transition({stage("#000000"), stage("#202020")}, {style = "fade"})
root:viewport(inner, {})
root:scene_transition({stage("#000000"), stage("#101010"), stage("#303030")}, {
    duration = 0.5, styles = {"fade", {style = "match"}}
})
return root
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                            "transition-styles.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "transition-styles.lua: %s\n", error);
    if (scene) {
        Surface surface;
        CHECK(renderer->render(scene, 0.25f, surface) == Result::Success);
        CHECK(renderer->render(scene, 0.75f, surface) == Result::Success);
    }
    delete scene;

    // Slides from the top and then from the left, a quarter of the way in each.
    static constexpr auto slides = R"lua(
local function plain(background)
    return vis.scene {width = 120, height = 60, background = background, antialiasing = false}
end
local root = vis.scene {width = 120, height = 60, background = "#ff0000", antialiasing = false}
root:scene_transition({plain("#000000"), plain("#ffffff"), plain("#000000")}, {
    curve = "linear", styles = {{style = "slide", from = "top"}, {style = "slide", from = "left"}}
})
return root
)lua";
    scene = nullptr;
    status = Lua::load(slides, static_cast<uint32_t>(std::strlen(slides)), "transition-slides.lua",
                       &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "transition-slides.lua: %s\n", error);
    if (scene) {
        auto white = _pixel(Color::hex("#ffffff"));
        auto black = _pixel(Color::hex("#000000"));
        Surface surface;
        CHECK(renderer->render(scene, 0.25f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[5u * surface.stride() + 60u] == white);
        CHECK(surface.data() && surface.data()[40u * surface.stride() + 60u] == black);
        CHECK(renderer->render(scene, 1.25f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 10u] == black);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 90u] == white);
    }
    delete scene;

    // Zooms into a card given by its handle, out of a card given by its id and into a part of
    // the viewport, each card at the middle of its scene.
    static constexpr auto zooms = R"lua(
local function plain(background, fill)
    local scene = vis.scene {width = 120, height = 60, background = background, antialiasing = false}
    return scene, scene:rectangle {center = {0, 0}, size = {1, 1}, fill = fill, id = "card"}
end
local first, card = plain("#000000", "#0000ff")
local root = vis.scene {width = 120, height = 60, background = "#ff0000", antialiasing = false}
root:scene_transition({first, plain("#ffffff", "#ffffff"), plain("#000000", "#000000"),
                       (plain("#ffffff", "#ffffff"))}, {
    curve = "linear", styles = {
        {style = "zoom_in", area = card},
        {style = "zoom_out", area = "card"},
        {style = "zoom_in", area = {x = 0.25, y = 0.25, width = 0.5, height = 0.5}}
    }
})
return root
)lua";
    scene = nullptr;
    status = Lua::load(zooms, static_cast<uint32_t>(std::strlen(zooms)), "transition-zooms.lua",
                       &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "transition-zooms.lua: %s\n", error);
    if (scene) {
        auto white = _pixel(Color::hex("#ffffff"));
        auto black = _pixel(Color::hex("#000000"));
        Surface surface;
        // Before a tenth of the way the incoming scene has not shown; halfway it fills the card.
        CHECK(renderer->render(scene, 0.05f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 60u] == _pixel(Color::hex("#0000ff")));
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 60u] == white);
        // Halfway out, the outgoing scene lies over the middle and the incoming one around it.
        CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 60u] == white);
        CHECK(surface.data() && surface.data()[2u * surface.stride() + 2u] == black);
        // Halfway into the middle half, the incoming scene covers x 18 to 102 and y 9 to 51.
        CHECK(renderer->render(scene, 2.5f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 25u] == white);
        CHECK(surface.data() && surface.data()[5u * surface.stride() + 5u] == black);
    }
    delete scene;

    // Zooming out, an id names the card of the incoming scene before that of the outgoing one:
    // the outgoing scene shrinks into the small card in the middle, not into its own large card
    // on the left, so halfway it covers x 28.5 to 91.5 with the incoming scene around it.
    static constexpr auto zoomOut = R"lua(
local function plain(background, fill, x, size)
    local scene = vis.scene {width = 120, height = 60, background = background, antialiasing = false}
    scene:rectangle {center = {x, 0}, size = {size, size}, fill = fill, id = "card"}
    return scene
end
local root = vis.scene {width = 120, height = 60, background = "#ff0000", antialiasing = false}
root:scene_transition({plain("#ffffff", "#0000ff", -6, 4), plain("#000000", "#00ff00", 0, 2)}, {
    curve = "linear", style = "zoom_out", area = "card"
})
return root
)lua";
    scene = nullptr;
    status = Lua::load(zoomOut, static_cast<uint32_t>(std::strlen(zoomOut)), "transition-zoom-out.lua",
                       &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "transition-zoom-out.lua: %s\n", error);
    if (scene) {
        Surface surface;
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 5u] == _pixel(Color::hex("#000000")));
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 60u] == _pixel(Color::hex("#ffffff")));
    }
    delete scene;

    // Exact object regions, by handle and id, use geometry rather than stroke extents and
    // match the corresponding normalized area for both zoom directions.
    const char* exactKinds[] = {"zoom_in", "zoom_out"};
    const float exactWidths[] = {0.0f, 3.0f, 18.0f};
    for (auto kind : exactKinds) {
        for (auto width : exactWidths) {
            Surface expected;
            for (auto mode = 0u; mode < 3u; mode++) {
                char source[2048];
                auto area = mode == 0u ? "{x=0.625,y=0.175,width=0.25,height=0.25}"
                                      : mode == 1u ? "card" : "'card'";
                std::snprintf(source, sizeof(source), R"lua(
local function stage(background)
    return vis.scene {width=200,height=100,background=background,antialiasing=false,
        camera={mode='fixed',view='2d',height=100}}
end
local a,b=stage('#000000'),stage('#ffffff')
local owner='%s'=='zoom_in' and a or b
local card=owner:rectangle {id='card',center={50,20},size={50,25},
    fill='#00ff00',stroke='#ff0000',width=%g}
local root=stage('#202020')
root:scene_transition({a,b},{duration=1,curve='linear',style='%s',area=%s%s})
return root
)lua", kind, static_cast<double>(width), kind, area, mode ? ",area_margin=0" : "");
                scene = nullptr;
                status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                                   "transition-exact-area.lua", &scene, error, sizeof(error));
                CHECK(status == Result::Success);
                if (status != Result::Success) std::fprintf(stderr, "transition-exact-area.lua: %s\n", error);
                if (scene) {
                    LayoutReport report;
                    CHECK(renderer->layout(scene, 0.5f, report) == Result::Success);
                    CHECK(report.sceneCount() == 3u);
                    auto outerScene = scene->transitionAt(std::strcmp(kind, "zoom_in") == 0 ? 0u : 1u);
                    const LayoutScene* outer = nullptr;
                    const LayoutScene* inner = nullptr;
                    for (auto i = 1u; i < report.sceneCount(); i++) {
                        auto entry = report.sceneAt(i);
                        if (entry->scene == outerScene) outer = entry;
                        else inner = entry;
                    }
                    CHECK(outer && std::fabs(outer->bounds.x + 500.0f / 3.0f) < 0.01f
                          && std::fabs(outer->bounds.y + 70.0f / 3.0f) < 0.01f
                          && std::fabs(outer->bounds.width - 400.0f) < 0.01f);
                    CHECK(inner && std::fabs(inner->bounds.x - 250.0f / 3.0f) < 0.01f
                          && std::fabs(inner->bounds.y - 35.0f / 3.0f) < 0.01f
                          && std::fabs(inner->bounds.width - 100.0f) < 0.01f);
                    Surface pixels;
                    CHECK(renderer->render(scene, 0.5f, mode ? pixels : expected) == Result::Success);
                    if (mode) {
                        CHECK(pixels.data() && expected.data() && pixels.stride() == expected.stride()
                              && std::memcmp(pixels.data(), expected.data(),
                                             pixels.height() * pixels.stride() * sizeof(uint32_t)) == 0);
                    }
                }
                delete scene;
            }
        }
    }

    // A slide that also matches keeps the shared card in the middle, halfway to black, while
    // the scenes slide past beneath it.
    static constexpr auto pairs = R"lua(
local function plain(background, fill)
    local scene = vis.scene {width = 120, height = 60, background = background, antialiasing = false}
    scene:rectangle {center = {0, 0}, size = {4, 4}, fill = fill, id = "card"}
    return scene
end
local root = vis.scene {width = 120, height = 60, background = "#ff0000", antialiasing = false}
root:scene_transition({plain("#000000", "#ffffff"), plain("#ffffff", "#000000")}, {
    curve = "linear", style = "slide", match = true
})
return root
)lua";
    scene = nullptr;
    status = Lua::load(pairs, static_cast<uint32_t>(std::strlen(pairs)), "transition-pairs.lua",
                       &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "transition-pairs.lua: %s\n", error);
    if (scene) {
        Surface surface;
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        auto pixel = surface.data() ? surface.data()[30u * surface.stride() + 55u] : 0u;
        auto gray = [](uint32_t channel) { return channel >= 126u && channel <= 129u; };
        CHECK(gray(pixel & 0xffu) && gray((pixel >> 8u) & 0xffu) && gray((pixel >> 16u) & 0xffu));
    }
    delete scene;

    static constexpr auto prefix =
        "local p=vis.scene{}; local a=vis.scene{}; local b=vis.scene{}; local c=vis.scene{}; ";
    auto fails = [](const char* body, const char* expected) {
        char script[512];
        std::snprintf(script, sizeof(script), "%s%s; return p", prefix, body);
        _scriptError(script, expected);
    };
    fails("p:scene_transition({a,b},{hold={1}})", "hold must list one duration for each of the 2 stages");
    fails("p:scene_transition({a,b},{hold={1,-1}})", "hold must be non-negative");
    fails("p:scene_transition({a,b},{hold={1,'slow'}})", "number expected, got string");
    fails("p:scene_transition({a,b},{live=1})", "live must be a boolean");
    fails("p:scene_transition({a,b},{style='wipe'})",
          "unknown transition style 'wipe'; use 'match', 'fade', 'slide', 'zoom_in' or 'zoom_out'");
    fails("p:scene_transition({a,b},{style='zoom_in'})",
          "a zoom needs an area: an object, its id or {x, y, width, height}");
    fails("p:scene_transition({a,b,c},{styles={'fade','zoom_out'}})",
          "a zoom needs an area: an object, its id or {x, y, width, height}");
    fails("p:scene_transition({a,b},{style='fade', area={x=0,y=0,width=1,height=1}})",
          "area applies only to the zoom styles");
    fails("p:scene_transition({a,b},{style='zoom_in', area=3})",
          "area must be an object, its id or {x, y, width, height}");
    fails("p:scene_transition({a,b},{style='zoom_in', area='nothing'})",
          "zoom area 'nothing' is the id of no object in either scene of its transition");
    fails("p:scene_transition({a,b},{style='zoom_in', area={x=2,y=0,width=0.5,height=0.5}})",
          "a zoom needs an area of positive size that overlaps the viewport");
    fails("p:scene_transition({a,b},{style='zoom_in', area={x=0,y=0,width=0.5,height=0.5,depth=1}})",
          "unknown option 'depth'");
    fails("p:scene_transition({a,b,c},{area='card', styles={'fade','fade'}})",
          "use either area or styles, not both");
    fails("p:scene_transition({a,b},{style='zoom_in',area='card',area_margin=-1})",
          "area_margin must be non-negative");
    fails("p:scene_transition({a,b},{style='zoom_in',area='card',area_margin=1/0})",
          "area_margin must be a finite number");
    fails("p:scene_transition({a,b},{style='zoom_in',area='card',area_margin=0/0})",
          "area_margin must be a finite number");
    fails("p:scene_transition({a,b},{style='zoom_in',area='card',area_margin=1e300})",
          "area_margin must be a finite number");
    fails("p:scene_transition({a,b},{style='zoom_in',area='card',area_margin=true})",
          "number expected, got boolean");
    fails("p:scene_transition({a,b},{style='zoom_in',area={x=0,y=0,width=1,height=1},area_margin=0})",
          "area_margin applies only to an object zoom area");
    fails("p:scene_transition({a,b},{style='fade',area_margin=0})",
          "area_margin applies only to an object zoom area");
    fails("p:scene_transition({a,b},{area_margin=0,styles={'fade'}})",
          "use either area_margin or styles, not both");
    fails("p:scene_transition({a,b},{styles={{style='zoom_out',area='card',area_margin=-1}}})",
          "area_margin must be non-negative");
    fails("p:scene_transition({a,b},{style='fade', match=true})",
          "match applies only to the slide and zoom styles");
    fails("p:scene_transition({a,b},{style='slide', match=1})", "match must be true or false");
    fails("p:scene_transition({a,b,c},{match=true, styles={'slide','slide'}})",
          "use either match or styles, not both");
    fails("p:scene_transition({a,b},{style='slide', from='up'})",
          "unknown slide direction 'up'; use 'left', 'right', 'top' or 'bottom'");
    fails("p:scene_transition({a,b},{style='slide', from=1})", "from must be a side such as 'left'");
    fails("p:scene_transition({a,b},{style='fade', from='left'})", "from applies only to the slide style");
    fails("p:scene_transition({a,b,c},{from='left', styles={'slide','slide'}})",
          "use either from or styles, not both");
    fails("p:scene_transition({a,b},{style=1})", "a transition style must be a name such as 'fade'");
    fails("p:scene_transition({a,b,c},{style='fade', styles={'fade','fade'}})",
          "use either style or styles, not both");
    fails("p:scene_transition({a,b,c},{styles={'fade'}})",
          "styles must list one style for each of the 2 transitions");
    fails("p:scene_transition({a,b,c},{styles={'fade', 2}})",
          "each entry of styles must be a style name or a table with a style");
    fails("p:scene_transition({a,b,c},{styles={'fade', {style='fade', speed=2}}})",
          "unknown option 'speed'");

    // Options given as nil are an empty table, as when they are left out.
    const char* plainSources[] = {"p:scene_transition({a,b})", "p:scene_transition({a,b}, nil)",
                                  "p:scene_transition({a,b}, {live=false})"};
    for (auto source : plainSources) {
        char script[512];
        std::snprintf(script, sizeof(script), "%s%s; return p", prefix, source);
        scene = nullptr;
        status = Lua::load(script, static_cast<uint32_t>(std::strlen(script)), "transition-no-options.lua", &scene,
                           error, sizeof(error));
        CHECK(status == Result::Success);
        if (status != Result::Success) std::fprintf(stderr, "%s: %s\n", source, error);
        CHECK(scene && scene->duration() == 1.0f);
        delete scene;
    }
}

static void _sceneTree(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local scene = vis.scene {
    width = 240, height = 160, background = "#101820", antialiasing = false,
    camera = {view = "2d", height = 10}
}

local root = scene:group {id = "root"}
local box = root:rectangle {
    center = {-2, 2}, size = {1.4, 0.8}, corner = 0.15,
    stroke = "#ef5350", fill = "#ef535044", width = 3, id = "box"
}
root:text {
    text = "root", point = {-2, 2}, fill = "#ffffff", progress = 0, id = "label"
}
local object_label = box:label {
    text = "box", point = {-2, 2.65}, fill = "#ffd166", role = "text",
    align = {0.5, 1}, size = 18, layer = 31, id = "object-label"
}
object_label:move_to({-2, 2.65})
box:circle {
    center = {-2, 2}, radius = 0.18, stroke = "#00000000", fill = "#ffd166", id = "shape-child"
}

local outer = scene:group {id = "outer"}
local coordinates = outer:space {
    x = {-3, 3, 1}, y = {-3, 3, 1}, opacity = 0, id = "coordinates"
}
local inner = coordinates:group {id = "inner"}
local mixed = inner:rectangle {
    center = {2, 2}, size = {0.8, 0.8}, stroke = "#42a5f5",
    fill = "#42a5f544", id = "mixed"
}

local pair = scene:group {id = "pair"}
local anchor = pair:rectangle {
    center = {-2, -1}, size = {1, 1}, fill = "#40d890", id = "anchor"
}
local moving = pair:rectangle {
    center = {2, -1}, size = {0.8, 1.4}, fill = "#b060e8", id = "moving"
}
moving:move_to({0, -1})
moving:next_to(anchor, {1, 0}, 0.3)
moving:align_to(anchor, {0, 1})

local row = scene:group {id = "row"}
local row_a = row:rectangle {size = {1, 0.6}, fill = "#ef5350", id = "row-a"}
local row_b = row:rectangle {size = {1.5, 0.6}, fill = "#66bb6a", id = "row-b"}
local row_c = row:rectangle {size = {0.5, 0.6}, fill = "#42a5f5", id = "row-c"}
row:arrange({1, 0}, 0.2)
row:move_to({2.5, -1.5})

local grid = scene:group {id = "grid"}
local grid_a = grid:rectangle {size = {1, 0.6}, fill = "#ef5350", id = "grid-a"}
local grid_b = grid:rectangle {size = {1, 0.6}, fill = "#66bb6a", id = "grid-b"}
local grid_c = grid:rectangle {size = {1, 0.6}, fill = "#42a5f5", id = "grid-c"}
local grid_d = grid:rectangle {size = {1, 0.6}, fill = "#ffd166", id = "grid-d"}
grid:arrange_grid(2, 0.25, 0.4)
grid:move_to({-3, -3})

scene:connector {
    from = anchor, to = moving, padding = 0.05,
    stroke = "#ffffff", width = 3, id = "connector"
}

scene:play({
    {target = box, shift = {0.5, 0}, opacity = 0.6,
     stroke = "#ffd166", fill = "#40d890"},
    {target = mixed, shift = {-0.5, 0}, fill = "#b060e8"}
}, 1.2, "linear", 0.25)
if math.abs(scene:duration() - 1.5) > 0.0001 then error("composite duration mismatch") end
return scene
)lua";

    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "scene-tree.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "scene-tree.lua: %s\n", error);
    if (!scene) return;

    CHECK(scene->count() == 22);
    CHECK(_near(scene->duration(), 1.5f));
    auto root = scene->object("root");
    auto box = scene->object("box");
    auto label = scene->object("label");
    auto objectLabel = scene->object("object-label");
    auto shapeChild = scene->object("shape-child");
    auto outer = scene->object("outer");
    auto coordinates = scene->object("coordinates");
    auto inner = scene->object("inner");
    auto mixed = scene->object("mixed");
    CHECK(root && box && label && objectLabel && shapeChild && outer && coordinates && inner && mixed);
    if (root && box && label && objectLabel && shapeChild && outer && coordinates && inner && mixed) {
        CHECK(root->type() == Type::Group && root->parent() == nullptr && root->space() == nullptr);
        CHECK(box->type() == Type::Rectangle && box->parent() == root && box->space() == nullptr);
        CHECK(label->type() == Type::Text && label->parent() == root && label->space() == nullptr);
        CHECK(objectLabel->type() == Type::Text && objectLabel->parent() == box
              && objectLabel->space() == nullptr);
        auto text = static_cast<Text*>(objectLabel);
        CHECK(std::strcmp(text->text(), "box") == 0 && text->role == TextRole::Text);
        CHECK(_near(text->point.x, -2.0f) && _near(text->point.y, 2.65f));
        CHECK(_near(text->align.x, 0.5f) && _near(text->align.y, 1.0f));
        CHECK(_near(text->size, 18.0f) && text->layer == 31);
        CHECK(text->style.fill.r == 255u && text->style.fill.g == 209u
              && text->style.fill.b == 102u && text->style.fill.a == 255u);
        CHECK(shapeChild->type() == Type::Circle && shapeChild->parent() == box
              && shapeChild->space() == nullptr);
        CHECK(outer->type() == Type::Group && outer->parent() == nullptr);
        CHECK(coordinates->type() == Type::Space && coordinates->parent() == outer
              && coordinates->space() == nullptr);
        CHECK(inner->type() == Type::Group && inner->parent() == coordinates
              && inner->space() == coordinates);
        CHECK(mixed->type() == Type::Rectangle && mixed->parent() == inner
              && mixed->space() == coordinates);
        CHECK(_near(box->model.e[3], 0.5f) && _near(box->opacity, 0.6f));
        CHECK(box->style.stroke.r == 0xff && box->style.stroke.g == 0xd1);
        CHECK(box->style.fill.g == 0xd8 && box->style.fill.b == 0x90);
        CHECK(_near(mixed->model.e[3], -0.5f));
        CHECK(mixed->style.fill.r == 0xb0 && mixed->style.fill.b == 0xe8);
    }

    auto anchor = scene->object("anchor");
    auto moving = scene->object("moving");
    auto connector = scene->object("connector");
    CHECK(anchor && moving && connector);
    if (anchor && moving) {
        Bounds anchorBounds;
        Bounds movingBounds;
        CHECK(anchor->bounds(anchorBounds) && moving->bounds(movingBounds));
        CHECK(_near(movingBounds.min.x - anchorBounds.max.x, 0.3f));
        CHECK(_near(movingBounds.max.y, anchorBounds.max.y));
    }
    if (connector) {
        CHECK(connector->type() == Type::Connector);
        auto value = static_cast<Connector*>(connector);
        CHECK(value->from() == anchor && value->to() == moving);
        CHECK(_near(value->padding, 0.05f));
    }

    auto rowA = scene->object("row-a");
    auto rowB = scene->object("row-b");
    auto rowC = scene->object("row-c");
    CHECK(rowA && rowB && rowC);
    if (rowA && rowB && rowC) {
        Bounds a;
        Bounds b;
        Bounds c;
        CHECK(rowA->bounds(a) && rowB->bounds(b) && rowC->bounds(c));
        CHECK(_near(b.min.x - a.max.x, 0.2f));
        CHECK(_near(c.min.x - b.max.x, 0.2f));
    }

    auto gridA = scene->object("grid-a");
    auto gridB = scene->object("grid-b");
    auto gridC = scene->object("grid-c");
    auto gridD = scene->object("grid-d");
    CHECK(gridA && gridB && gridC && gridD);
    if (gridA && gridB && gridC && gridD) {
        Bounds a;
        Bounds b;
        Bounds c;
        Bounds d;
        CHECK(gridA->bounds(a) && gridB->bounds(b) && gridC->bounds(c) && gridD->bounds(d));
        CHECK(_near(b.min.x - a.max.x, 0.25f));
        CHECK(_near(d.min.x - c.max.x, 0.25f));
        CHECK(_near(a.min.y - c.max.y, 0.4f));
        CHECK(_near(b.min.y - d.max.y, 0.4f));
    }

    Surface surface;
    CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
    CHECK(_changed(surface, scene->config().background) > 500);
    delete scene;
}

static void _animationMethods(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local scene = vis.scene {
    width = 320, height = 180, background = "#101820", antialiasing = false,
    camera = {view = "2d", height = 8}
}

local forward = scene:line {from = {-3, 3}, to = {-1, 3}, stroke = "#4cc9f0", id = "forward"}
local reverse = scene:line {from = {1, 3}, to = {3, 3}, stroke = "#f72585", id = "reverse"}
scene:create({forward, reverse}, 0.1, "linear", 0.5, "forward")
scene:uncreate({forward, reverse}, 0.1, "linear", 0.5, "reverse")

local circle = scene:circle {center = {0, 2}, radius = 0.6, stroke = "#ffd166", id = "circle"}
scene:create(circle, 0.1, "linear", 0, "clockwise")
scene:uncreate(circle, 0.1, "linear", 0, "counterclockwise")

local fill_reveal = scene:rectangle {
    center = {-2.5, 2}, size = {0.8, 0.8}, fill = "#4cc9f0", stroke = "#f4f7fb",
    id = "fill-reveal"
}
scene:create(fill_reveal, 0.1, "linear")
scene:fill_reveal(fill_reveal, 0.1, "linear")

local border_fill = scene:rectangle {
    center = {2.5, 2}, size = {0.8, 0.8}, fill = "#f72585", stroke = "#f4f7fb",
    id = "border-fill"
}
scene:draw_border_then_fill(border_fill, 0.2, "linear", 0, "reverse")

local default_fade = scene:rectangle {
    center = {-2.5, 0.5}, size = {1.2, 0.7}, fill = "#4cc9f0", id = "default-fade"
}
scene:fade_in(default_fade)
scene:fade_out(default_fade)

local shifted_fade = scene:rectangle {
    center = {-0.8, 0.5}, size = {1.2, 0.7}, fill = "#f72585", id = "shifted-fade"
}
scene:fade_in(shifted_fade, {
    shift = {0, -0.4}, scale = 0.8, duration = 0.15,
    curve = {preset = "snappy", strength = 0.8}
})
scene:fade_out(shifted_fade, {
    shift = {0, 0.4}, scale = 1.1, duration = 0.15,
    curve = {bezier = {0.42, 0, 0.58, 1}, strength = 1.0, reverse = true}
})

local growing = scene:circle {
    center = {1, 0.5}, radius = 0.5, fill = "#7bd88f", id = "growing"
}
scene:grow_from_center(growing, 0.1, "smooth")
scene:grow_from_edge(growing, "bottom", 0.1, "smooth")
scene:shrink_to_center(growing, 0.1, "smooth")

local indicated = scene:rectangle {
    center = {2.5, 0.5}, size = {1.0, 0.7}, fill = "#4cc9f0", id = "indicated"
}
scene:indicate(indicated, {color = "#ffd166", scale = 1.25, duration = 0.2, easing = "smooth"})

local morph_source = scene:polygon {
    points = {{-3, -2.5}, {-1.8, -2.5}, {-2.4, -1.3}},
    fill = "#4cc9f055", stroke = "#4cc9f0", id = "morph-source"
}
local morph_target = scene:polygon {
    points = {{-3, -2.5}, {-1.8, -2.5}, {-1.8, -1.3}},
    fill = "#7bd88f55", stroke = "#7bd88f", id = "morph-target"
}
scene:morph(morph_source, morph_target, 0.2, "ease_in_out")
scene:indicate(morph_target, {duration = 0.1, scale = 1.1})

local replacement_source = scene:plot {
    points = {{0.5, -2.5}, {1.2, -1.3}, {1.9, -2.5}},
    stroke = "#ff6b6b", id = "replacement-source"
}
local replacement_target = scene:plot {
    points = {{0.5, -1.3}, {1.2, -2.5}, {1.9, -1.3}},
    stroke = "#7bd88f", id = "replacement-target"
}
scene:replacement_transform(replacement_source, replacement_target, 0.2, "ease_in_out")
scene:indicate(replacement_target, {color = "#ffd166", scale = 1.1, duration = 0.1})

local fade_source = scene:rectangle {
    center = {2.4, -2.0}, size = {0.8, 0.6}, fill = "#ff6b6b", id = "fade-source"
}
local fade_target = scene:circle {
    center = {2.4, -2.0}, radius = 0.4, fill = "#4cc9f0", id = "fade-target"
}
scene:fade_transform(fade_source, fade_target, 0.2, "gentle")

local batch_source_a = scene:plot {
    points = {{-3, -3.2}, {-2.4, -2.8}, {-1.8, -3.2}},
    stroke = "#4cc9f0", id = "batch-source-a"
}
local batch_source_b = scene:plot {
    points = {{0.5, -3.2}, {1.2, -2.8}, {1.9, -3.2}},
    stroke = "#f72585", id = "batch-source-b"
}
local batch_target_a = scene:plot {
    points = {{-3, -2.8}, {-2.4, -3.2}, {-1.8, -2.8}},
    stroke = "#7bd88f", id = "batch-target-a"
}
local batch_target_b = scene:plot {
    points = {{0.5, -2.8}, {1.2, -3.2}, {1.9, -2.8}},
    stroke = "#ffd166", id = "batch-target-b"
}
scene:morph(
    {batch_source_a, batch_source_b},
    {batch_target_a, batch_target_b},
    0.2, "ease_in_out", 0.25
)

if math.abs(scene:duration() - 4.75) > 0.0001 then error("animation duration mismatch") end
return scene
)lua";

    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                            "animation-methods.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "animation-methods.lua: %s\n", error);
    if (scene) {
        CHECK(scene->count() == 19u);
        CHECK(_near(scene->duration(), 4.75f));
        CHECK(scene->object("morph-target") != nullptr);
        CHECK(scene->object("replacement-target") != nullptr);
        CHECK(scene->object("fade-target") != nullptr);
        CHECK(scene->object("batch-target-a") != nullptr);
        CHECK(scene->object("batch-target-b") != nullptr);
        Surface surface;
        CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 100u);
    }
    delete scene;
}

static uint64_t _surfaceHash(const Surface& surface)
{
    auto hash = uint64_t{1469598103934665603ull};
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            hash ^= surface.data()[static_cast<size_t>(y) * surface.stride() + x];
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

static void _dashTimeline(SwRenderer* renderer)
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {
    width = 320, height = 180, background = "#101820", antialiasing = false,
    camera = {view = "2d", height = 4}
}
local flow = scene:line {
    from = {-2, 0}, to = {2, 0}, stroke = "#4cc9f0", width = 8,
    dash = {20, 12}, dash_offset = 0, id = "flow"
}
scene:play({target = flow, dash_offset = -32}, 1, "linear")
return scene
)lua";
    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, sizeof(source) - 1u, "dash-timeline.lua", &scene,
                            error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "dash-timeline.lua: %s\n", error);
    if (scene) {
        auto flow = scene->object("flow");
        CHECK(flow != nullptr);
        CHECK(flow && _near(flow->style.dashOffset, -32.0f));
        Surface middle;
        Surface end;
        CHECK(renderer->render(scene, 0.5f, middle) == Result::Success);
        auto middleChecksum = _surfaceHash(middle);
        CHECK(renderer->render(scene, 1.0f, end) == Result::Success);
        auto endChecksum = _surfaceHash(end);
        CHECK(middleChecksum != endChecksum);
    }
    delete scene;
}

static void _themes(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local scene = vis.scene {
    width = 320, height = 180, background = "#fafafa",
    theme = {
        preset = "pro_white",
        background = "#101426",
        objects = {"#112233", "#445566"},
        object_width = 3.25,
        gradient = true,
        end_gradient_stop = "#778899",
        text = {
            h1 = {font = "Pretendard", size = 33, color = "#17191d"},
            code = {font = "Pretendard", size = 14, color = "#8a3b12"},
        },
        axis = {x = "#aa3311", y = "#227744", z = "#335599",
            grid = "#d6d8dc", label = "#555b64"},
    },
    camera = {view = "2d", height = 6},
}
local title = scene:text {text = "Theme", role = "h1", point = {0, 2}, id = "title"}
local code = scene:text {text = "f(x) = x^2", role = "code", point = {0, 1.4}, id = "code"}
local axes = scene:space {numbers = true, id = "axes"}
local first = scene:line {from = {-2, 0.5}, to = {2, 0.5}, id = "first"}
local explicit = scene:line {from = {-2, 0}, to = {2, 0}, stroke = "#abcdef", width = 5, id = "explicit"}
local second = scene:circle {center = {-1, -1}, id = "second"}
local wrapped = scene:point {point = {1, -1}, id = "wrapped"}
local custom_gradient = scene:line {from = {-2, -1.5}, to = {0, -1.5}, gradient = "#fedcba", id = "custom-gradient"}
local solid = scene:line {from = {0, -1.5}, to = {2, -1.5}, gradient = false, id = "solid"}
return scene
)lua";

    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                            "theme.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "theme.lua: %s\n", error);
    if (scene) {
        CHECK(_pixel(scene->config().background) == _pixel(Color::hex("#fafafa")));
        CHECK(scene->theme().objectCount == 2u);
        auto title = static_cast<Text*>(scene->object("title"));
        auto code = static_cast<Text*>(scene->object("code"));
        auto axes = static_cast<Space*>(scene->object("axes"));
        auto first = scene->object("first");
        auto explicitLine = scene->object("explicit");
        auto second = scene->object("second");
        auto wrapped = scene->object("wrapped");
        auto customGradient = scene->object("custom-gradient");
        auto solid = scene->object("solid");
        CHECK(title && code && axes && first && explicitLine && second && wrapped
              && customGradient && solid);
        if (title && code && axes && first && explicitLine && second && wrapped
            && customGradient && solid) {
            CHECK(title->role == TextRole::H1 && _near(title->size, 33.0f));
            CHECK(code->role == TextRole::Code && _near(code->size, 14.0f));
            CHECK(std::strcmp(title->font(), "Pretendard") == 0);
            CHECK(_pixel(title->style.fill) == _pixel(Color::hex("#17191d")));
            CHECK(_pixel(code->style.fill) == _pixel(Color::hex("#8a3b12")));
            CHECK(_pixel(axes->axisX) == _pixel(Color::hex("#aa3311")));
            CHECK(_pixel(axes->axisY) == _pixel(Color::hex("#227744")));
            CHECK(_pixel(axes->axisZ) == _pixel(Color::hex("#335599")));
            CHECK(_pixel(first->style.stroke) == _pixel(Color::hex("#112233")));
            CHECK(_near(first->style.width, 3.25f));
            CHECK(_pixel(explicitLine->style.stroke) == _pixel(Color::hex("#abcdef")));
            CHECK(_near(explicitLine->style.width, 5.0f));
            CHECK(_pixel(second->style.stroke) == _pixel(Color::hex("#445566")));
            CHECK(_near(second->style.width, 3.25f));
            CHECK(_pixel(wrapped->style.stroke) == _pixel(Color::hex("#112233")));
            CHECK(_pixel(wrapped->style.fill) == _pixel(Color::hex("#112233")));
            CHECK(first->style.gradient.enabled());
            CHECK(_pixel(first->style.gradientEnd) == _pixel(Color::hex("#778899")));
            CHECK(customGradient->style.gradient.enabled());
            CHECK(_pixel(customGradient->style.gradientEnd)
                  == _pixel(Color::hex("#fedcba")));
            CHECK(!solid->style.gradient.enabled());
        }
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 100u);
    }
    delete scene;

    static constexpr auto blackSource = R"lua(
local scene = vis.scene {theme = "pro_black"}
scene:point {point = {0, 0}, id = "point"}
local legacy = vis.scene {theme = "pro"}
legacy:point {point = {0, 0}, id = "legacy-point"}
scene:viewport(legacy, {x = 0, y = 0, width = 1, height = 1})
return scene
)lua";
    error[0] = '\0';
    scene = nullptr;
    status = Lua::load(blackSource, static_cast<uint32_t>(std::strlen(blackSource)),
                       "theme-pro-black.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "theme-pro-black.lua: %s\n", error);
    if (scene) {
        CHECK(_pixel(scene->config().background) == _pixel(Color::hex("#000000")));
        auto point = scene->object("point");
        CHECK(point != nullptr);
        if (point) {
            CHECK(_pixel(point->style.stroke) == _pixel(Color::hex("#f28e2b")));
            CHECK(_pixel(point->style.fill) == _pixel(Color::hex("#f28e2b")));
        }
        auto legacy = scene->sceneAt(0);
        CHECK(legacy != nullptr);
        if (legacy) {
            CHECK(_pixel(legacy->config().background) == _pixel(Color::hex("#ffffff")));
            auto legacyPoint = legacy->object("legacy-point");
            CHECK(legacyPoint != nullptr);
            if (legacyPoint) {
                CHECK(_pixel(legacyPoint->style.stroke) == _pixel(Color::hex("#b45f06")));
                CHECK(_pixel(legacyPoint->style.fill) == _pixel(Color::hex("#b45f06")));
            }
        }
    }
    delete scene;

    static constexpr auto adaptiveSource = R"lua(
local scene = vis.scene {theme = "adaptive_vscode"}
scene:text {text = "Adaptive", role = "h1", id = "title"}
scene:line {from = {-1, 0}, to = {1, 0}, id = "line"}
return scene
)lua";
    auto adaptive = Theme::preset(ThemePreset::ProBlack);
    adaptive.background = Color::hex("#101820");
    adaptive.h1.color = Color::hex("#e8edf2");
    adaptive.objects[0] = Color::hex("#62b0ff");
    adaptive.objectCount = 1u;
    bool adaptiveUsed = false;
    error[0] = '\0';
    scene = nullptr;
    status = Lua::load(adaptiveSource, static_cast<uint32_t>(std::strlen(adaptiveSource)),
                       "theme-adaptive-vscode.lua", &scene, error, sizeof(error), nullptr,
                       nullptr, &adaptive, &adaptiveUsed);
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    CHECK(adaptiveUsed);
    if (scene) {
        CHECK(_pixel(scene->config().background) == _pixel(Color::hex("#101820")));
        auto title = static_cast<Text*>(scene->object("title"));
        auto line = scene->object("line");
        CHECK(title && line);
        if (title) CHECK(_pixel(title->style.fill) == _pixel(Color::hex("#e8edf2")));
        if (line) CHECK(_pixel(line->style.stroke) == _pixel(Color::hex("#62b0ff")));
    }
    delete scene;

    adaptiveUsed = false;
    scene = nullptr;
    status = Lua::load(adaptiveSource, static_cast<uint32_t>(std::strlen(adaptiveSource)),
                       "theme-adaptive-fallback.lua", &scene, error, sizeof(error), nullptr,
                       nullptr, nullptr, &adaptiveUsed);
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    CHECK(adaptiveUsed);
    if (scene) CHECK(_pixel(scene->config().background) == _pixel(Color::hex("#ffffff")));
    delete scene;

    static constexpr auto namedSource = R"lua(
local named = vis.scene {theme = "3_blue_1_eyes"}
named:point {point = {0, 0}, id = "named-point"}
return named
)lua";
    scene = nullptr;
    status = Lua::load(namedSource, static_cast<uint32_t>(std::strlen(namedSource)),
                       "theme-three-blue-one-eyes.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (scene) {
        CHECK(_pixel(scene->config().background) == _pixel(Color::hex("#0d1117")));
        auto point = scene->object("named-point");
        CHECK(point && _pixel(point->style.fill) == _pixel(Color::hex("#f28e2b")));
    }
    delete scene;

    static constexpr auto implicitSource = "return vis.scene {}";
    scene = nullptr;
    status = Lua::load(implicitSource, static_cast<uint32_t>(std::strlen(implicitSource)),
                       "theme-implicit.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (scene) {
        CHECK(_pixel(scene->config().background) == _pixel(Color::hex("#ffffff")));
        CHECK(_pixel(scene->theme().objects[0]) == _pixel(Color::hex("#b45f06")));
    }
    delete scene;

    static constexpr auto semanticSource = R"lua(
local scene = vis.scene {
    theme = {
        preset = "pro_white",
        colors = {
            foreground = "#111111", muted = "#777777", accent = "#123456",
            secondary = "#654321", success = "#118833", warning = "#aa7700",
            danger = "#bb2233", info = "#2277aa", surface = "#f0f1f2", border = "#ccd0d4",
            result = "#886600", focus = "#336699",
        },
    },
}
local accent = scene:line {from = {-2, 1}, to = {2, 1}, stroke = "accent", gradient = "secondary", id = "accent"}
local animated = scene:line {from = {-2, 1.5}, to = {2, 1.5}, stroke = "foreground", id = "animated"}
scene:point {point = {0, 0}, color = "warning", id = "warning"}
scene:rectangle {center = {0, -1}, size = {2, 0.5}, fill = "surface", stroke = "border", id = "panel"}
scene:circle {center = {-1, -2}, radius = 0.25, stroke = "result", id = "result"}
scene:circle {center = {1, -2}, radius = 0.25, stroke = "focus", id = "focus"}
scene:space {axis_x = "danger", axis_y = "success", axis_z = "info", number_color = "muted", id = "semantic-space"}
scene:stroke(animated, "success", 0.1)
scene:indicate(accent, {duration = 0.1})
return scene
)lua";
    scene = nullptr;
    status = Lua::load(semanticSource, static_cast<uint32_t>(std::strlen(semanticSource)),
                       "theme-semantic.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "theme-semantic.lua: %s\n", error);
    if (scene) {
        CHECK(_pixel(scene->theme().colors.accent) == _pixel(Color::hex("#123456")));
        CHECK(_pixel(scene->theme().colors.danger) == _pixel(Color::hex("#bb2233")));
        CHECK(_pixel(scene->theme().colors.result) == _pixel(Color::hex("#886600")));
        CHECK(_pixel(scene->theme().colors.focus) == _pixel(Color::hex("#336699")));
        auto accent = scene->object("accent");
        auto warning = scene->object("warning");
        auto panel = scene->object("panel");
        auto semanticSpace = static_cast<Space*>(scene->object("semantic-space"));
        CHECK(accent && warning && panel && semanticSpace);
        if (accent) {
            CHECK(_pixel(accent->style.stroke) == _pixel(Color::hex("#123456")));
            CHECK(accent->style.gradient.enabled());
            CHECK(_pixel(accent->style.gradientEnd) == _pixel(Color::hex("#654321")));
        }
        if (warning) {
            CHECK(_pixel(warning->style.stroke) == _pixel(Color::hex("#aa7700")));
            CHECK(_pixel(warning->style.fill) == _pixel(Color::hex("#aa7700")));
        }
        if (panel) {
            CHECK(_pixel(panel->style.stroke) == _pixel(Color::hex("#ccd0d4")));
            CHECK(_pixel(panel->style.fill) == _pixel(Color::hex("#f0f1f2")));
        }
        if (semanticSpace) {
            CHECK(_pixel(semanticSpace->axisX) == _pixel(Color::hex("#bb2233")));
            CHECK(_pixel(semanticSpace->axisY) == _pixel(Color::hex("#118833")));
            CHECK(_pixel(semanticSpace->axisZ) == _pixel(Color::hex("#2277aa")));
            CHECK(_pixel(semanticSpace->numberColor) == _pixel(Color::hex("#777777")));
        }
        CHECK(_near(scene->duration(), 0.2f));
    }
    delete scene;
}

static void _cellVoxelBindings(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local scene = vis.scene {
    width = 320, height = 180, background = "#ffffff",
    camera = {view = "3d", eye = {5, 4, 6}, target = {0, 0, 0},
        projection = "perspective", near = 0.1, far = 100}
}
local space = scene:space {x = {-1, 1, 1}, y = {-1, 1, 1}, z = {-1, 1, 1}}
local calls = 0
local first_time = -1
local last_time = -1
local voxels = space:voxel(function(x, y, z, time)
    calls = calls + 1
    if calls == 1 then first_time = time end
    last_time = time
    if x == 0 and y == 0 and z == 0 then return "#00000000" end
    local radius = 0.8 + 0.35 * time
    if x*x + y*y + z*z > radius*radius then return "#00000000" end
    if z < 0 then return "#2563eb" end
    if z > 0 then return "#dc2626" end
    return "#7c3aed"
end, {mode = "padd", padding = 0.08, duration = 1, fps = 2})
if calls ~= 81 or first_time ~= 0 or last_time ~= 1 then
    error("voxel temporal samples mismatch")
end
local xy_calls = 0
local xy_first_time = -1
local xy_last_time = -1
local cells = space:cell(function(x, y, time)
    xy_calls = xy_calls + 1
    if xy_calls == 1 then xy_first_time = time end
    xy_last_time = time
    local a = 1 + 0.5 * time
    return x*x/(a*a) + y*y <= 1 and "#0891b2" or "#00000000"
end, {mode = "padd", padding = 0.08, duration = 1, fps = 2})
if xy_calls ~= 27 or xy_first_time ~= 0 or xy_last_time ~= 1 then
    error("cell temporal samples mismatch")
end
return scene
)lua";
    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                            "cell-voxel.lua", &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    if (status != Result::Success) std::fprintf(stderr, "cell-voxel.lua: %s\n", error);
    if (scene) {
        CHECK(scene->count() == 7u);
        CHECK(scene->duration() == 2.0f);
        auto space = static_cast<Space*>(scene->objectAt(0));
        CHECK(space && space->type() == Type::Space);
        auto voxels = space ? space->objectAt(0) : nullptr;
        CHECK(voxels && voxels->type() == Type::Group);
        CHECK(voxels && voxels->childCount() == 3u);
        auto cells = space ? space->objectAt(1) : nullptr;
        CHECK(cells && cells->type() == Type::Group);
        CHECK(cells && cells->childCount() == 1u);
        Surface surface;
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 1000u);
        CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        CHECK(_changed(surface, scene->config().background) > 1000u);
    }
    delete scene;
}

static void _gradientError(const char* body, const char* name, const char* expected)
{
    char source[512];
    std::snprintf(source, sizeof(source),
                  "local s=vis.scene{}; s:circle{gradient=%s}; return s", body);
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene,
                            error, sizeof(error));
    CHECK(status == Result::ScriptError);
    CHECK(std::strstr(error, expected) != nullptr);
    if (!std::strstr(error, expected)) std::fprintf(stderr, "  got: %s\n", error);
    delete scene;
}

static void _gradients()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {theme = {colors = {accent = "#112233", warning = "#445566"}}}
scene:circle {center = {0, 0}, radius = 1, fill = "#ffffff", id = "even",
              gradient = {type = "linear", stops = {"accent", "#808080", "warning"},
                          from = {-1, 0}, to = {1, 0}}}
scene:circle {center = {0, 0}, radius = 1, id = "radial",
              gradient = {type = "radial", stops = {{0, "accent"}, {0.25, "#fff"}, {1, "warning"}},
                          center = {0.5, 0}, radius = 2, focal = {0.25, 0}, focal_radius = 0.1}}
scene:circle {center = {0, 0}, radius = 1, id = "conic",
              gradient = {type = "conic", center = {1, 2}, angle = 45}}
scene:circle {center = {0, 0}, radius = 1, id = "legacy", gradient = "warning"}
return scene
)lua";
    char error[256] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "gradients.lua",
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "  %s\n", error);
    if (scene) {
        auto even = scene->object("even");
        auto radial = scene->object("radial");
        auto conic = scene->object("conic");
        auto legacy = scene->object("legacy");
        CHECK(even && radial && conic && legacy);
        if (even && radial && conic && legacy) {
            auto& linear = even->style.gradient;
            CHECK(linear.type == GradientType::Linear && linear.stopCount == 3u && linear.line);
            CHECK(_near(linear.stops[1].offset, 0.5f) && _near(linear.stops[2].offset, 1.0f));
            CHECK(_pixel(linear.stops[0].color) == _pixel(Color::hex("#112233")));
            CHECK(_pixel(linear.stops[2].color) == _pixel(Color::hex("#445566")));
            CHECK(_near(linear.from.x, -1.0f) && _near(linear.to.x, 1.0f));
            auto& r = radial->style.gradient;
            CHECK(r.type == GradientType::Radial && r.stopCount == 3u && r.centered && r.sized
                  && r.focused);
            CHECK(_near(r.stops[1].offset, 0.25f) && _near(r.radius, 2.0f)
                  && _near(r.focalRadius, 0.1f) && _near(r.focal.x, 0.25f));
            auto& c = conic->style.gradient;
            CHECK(c.type == GradientType::Conic && c.stopCount == 0u && c.centered
                  && _near(c.angle, 45.0f) && _near(c.center.y, 2.0f));
            CHECK(legacy->style.gradient.type == GradientType::Linear
                  && legacy->style.gradient.stopCount == 0u);
            CHECK(_pixel(legacy->style.gradientEnd) == _pixel(Color::hex("#445566")));
        }
    }
    delete scene;

    _gradientError("{stops={'#fff','#000'}}", "gradient-type-missing.lua", "gradient.type");
    _gradientError("{type='diamond'}", "gradient-type-unknown.lua", "got 'diamond'");
    _gradientError("{type='linear',colors={}}", "gradient-key.lua", "unknown gradient option 'colors'");
    _gradientError("{type='linear',center={0,0}}", "gradient-linear-center.lua",
                   "'center' is not valid for a linear gradient");
    _gradientError("{type='conic',radius=1}", "gradient-conic-radius.lua",
                   "'radius' is not valid for a conic gradient");
    _gradientError("{type='radial',angle=10}", "gradient-radial-angle.lua",
                   "'angle' is not valid for a radial gradient");
    _gradientError("{type='linear',from={0,0}}", "gradient-linear-half.lua", "both from and to");
    _gradientError("{type='linear',stops={'#fff'}}", "gradient-stops-one.lua", "2 to 8 stops");
    _gradientError("{type='linear',stops={'#000','#111','#222','#333','#444','#555','#666','#777','#888'}}",
                   "gradient-stops-many.lua", "2 to 8 stops");
    _gradientError("{type='linear',stops={{0.5,'#fff'},{0.25,'#000'}}}", "gradient-stops-order.lua",
                   "non-decreasing");
    _gradientError("{type='linear',stops={{0,'#fff'},{1.5,'#000'}}}", "gradient-stops-range.lua",
                   "within 0..1");
    _gradientError("{type='linear',stops={{0,'#fff'},{0/0,'#000'}}}", "gradient-stops-nan.lua",
                   "finite");
    _gradientError("{type='linear',stops={'#fff','nope'}}", "gradient-stops-color.lua",
                   "gradient stop color");
    _gradientError("{type='linear',stops={'#fff',{1}}}", "gradient-stops-pair.lua",
                   "{offset, color}");
    _gradientError("{type='radial',radius=0}", "gradient-radius.lua", "must be positive");
    _gradientError("{type='radial',focal_radius=-1}", "gradient-focal-radius.lua", "cannot be negative");
    _gradientError("{type='linear',stops={a='#fff'}}", "gradient-stops-map.lua", "sequence");
    _error("local s=vis.scene{}; s:text{text='x',gradient={type='linear'}}; return s",
           "text-gradient-table.lua");
}

// A function play blends vertically on matching grids without running callbacks during
// rendering, and keeps the midpoint switch for points and incompatible grids.
static void _functionPlay(SwRenderer* renderer)
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
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "function-play.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "function-play.lua: %s\n", error);
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
static void _animatedPlotGaps(SwRenderer* renderer)
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
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "animated-poles.lua", &scene,
                    error, sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "animated-poles.lua: %s\n", error);
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
static void _replotting(SwRenderer* renderer)
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
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "replot.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "replot.lua: %s\n", error);
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
    scene = nullptr;
    CHECK(Lua::load(room, static_cast<uint32_t>(std::strlen(room)), "room.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "room.lua: %s\n", error);
    delete scene;
    std::snprintf(room, sizeof(room), "%ss:play({target = p, fn = zero, x_range = {0, 1, 1 / 60000}}, 0.1) return s",
                  growing);
    _scriptError(room, "scene point limit exceeded");
}

static void _trimmedSegments(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 200, height = 100, fps = 10,
  camera = {mode = "fixed", view = "2d", height = 10}}
local l = s:line {from = {-4, 0}, to = {4, 0}, buff = {1, 2}, width = 2, id = "line"}
local a = s:arrow {from = {-4, 2}, to = {4, 2}, buff = 1, id = "arrow"}
local v = s:vector {origin = {-4, -2}, value = {8, 0}, buff = {2, 1}, id = "vector"}
s:set {target = v, buff = 1}
local x = s:tracker(0)
s:bind(l, x, function(t) return {to = {4, 6 * t}, buff = {1 + t, 2 - t}} end)
s:track(x, 1, 1, "linear")
return s
)lua";
    Scene* scene = nullptr;
    char error[512] = {};
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "buff.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "buff.lua: %s\n", error);
    if (scene) {
        auto line = static_cast<Line*>(scene->object("line"));
        auto arrow = static_cast<Arrow*>(scene->object("arrow"));
        auto vector = static_cast<Vector*>(scene->object("vector"));
        CHECK(line && arrow && vector);
        if (line && arrow && vector) {
            CHECK(_near(line->buff.x, 2.0f) && _near(line->buff.y, 1.0f));
            CHECK(_near(arrow->buff.x, 1.0f) && _near(vector->buff.y, 1.0f));
            BBox bounds;
            CHECK(renderer->bounds(scene, line, 0.5f, bounds) == Result::Success);
            CHECK(_near(bounds.x, 59.0f + 120.0f / std::sqrt(73.0f)));
        }
    }
    delete scene;
    _scriptError("local s = vis.scene {} s:line {from = {0, 0}, to = {1, 0}, buff = -1} return s",
                 "buff cannot be negative");
    _scriptError("local s = vis.scene {} s:arrow {from = {0, 0}, to = {1, 0}, buff = {0, -1}} return s",
                 "buff cannot be negative");
    _scriptError("local s = vis.scene {} s:vector {value = {1, 0}, buff = {1}} return s",
                 "buff must be a number or {start, end}");
    _scriptError("local s = vis.scene {} s:line {from = {0, 0}, to = {1, 0}, buff = '1'} return s",
                 "buff must be a number or {start, end}");
    _scriptError("local s = vis.scene {} local c = s:circle {radius = 1} s:play({target = c, buff = 1}, 1) return s",
                 "buff needs a line, arrow or vector and non-negative distances");
}

static void _boundGeometry(SwRenderer* renderer)
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
static void _overlappingTracks(SwRenderer* renderer)
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
static void _pendingReads()
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

static void _longTracks(SwRenderer* renderer)
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

static void _springResponses(SwRenderer* renderer)
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

static void _trackerPrecision()
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

static void _trackerTiming()
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

static void _trackers()
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
static void _trackerLines()
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

static void _timeline()
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

static bool _near(const Vec3& value, float x, float y, float z, float tolerance = 1.0e-4f)
{
    return std::fabs(value.x - x) < tolerance && std::fabs(value.y - y) < tolerance
        && std::fabs(value.z - z) < tolerance;
}

// A look whose target names objects aims at the center of their bounds, as focus frames them,
// and keeps the camera's direction, distance and height, also while another camera move runs.
static void _cameraCenters()
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
static void _cameraHome()
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
static float _viewScale(SwRenderer* renderer, Scene* scene, float time, uint32_t index)
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
static void _views(SwRenderer* renderer)
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

static void _motion(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 80, camera = {mode = "fixed", view = "2d", height = 4}}
local solid = {fill = "#ffffff", stroke = "#00000000"}
local function shape(config)
    for key, value in ipairs({"fill", "stroke"}) do config[value] = solid[value] end
    return config
end
local box = s:rectangle(shape {center = {-2, 0}, size = {1, 1}, id = "box"})
local dot = s:rectangle(shape {center = {1, 1}, size = {0.1, 0.1}, id = "dot"})
local ring = s:circle {radius = 1, id = "ring"}
local car = s:rectangle(shape {center = {1, 0}, size = {0.4, 0.2}, id = "car"})
local sun = s:circle {center = {-3, -1}, radius = 0.1, id = "sun"}
local planet = s:circle(shape {center = {-2.5, -1}, radius = 0.05, id = "planet"})
s:parallel(
    function() s:rotate(box, math.pi / 2, {about = "bottom_left", duration = 1, curve = "linear"}) end,
    function() s:play({target = dot, shift = {-2, 0}, arc = math.pi}, 1, "linear") end,
    function() s:move_along(car, ring, {orient = true, duration = 1, curve = "linear"}) end,
    function() s:rotate(planet, 2 * math.pi, {about = sun, duration = 1, curve = "linear"}) end)
s:scale(box, 2, {about = {-3.5, -0.5}})
s:move_along({dot, car}, ring, {from = 1, to = 0.5, lag = 0.5})
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "motion.lua", &scene, error, sizeof(error)) == Result::Success);
    if (!scene) {
        std::fprintf(stderr, "motion.lua: %s\n", error);
        return;
    }
    CHECK(std::fabs(scene->duration() - 3.5f) < 1.0e-4f);
    auto near = [](float value, float expected) { return std::fabs(value - expected) < 0.6f; };
    BBox bounds;
    CHECK(renderer->bounds(scene, scene->object("box"), 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.width, 28.28f) && near(bounds.center().x, 30.0f) && near(bounds.center().y, 35.86f));
    CHECK(renderer->bounds(scene, scene->object("dot"), 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f) && near(bounds.center().y, 0.0f));
    CHECK(renderer->bounds(scene, scene->object("car"), 0.25f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f) && near(bounds.center().y, 20.0f));
    CHECK(near(bounds.width, 4.0f) && near(bounds.height, 8.0f));
    CHECK(renderer->bounds(scene, scene->object("planet"), 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 10.0f) && near(bounds.center().y, 60.0f));
    CHECK(renderer->bounds(scene, scene->object("box"), 2.0f, bounds) == Result::Success);
    CHECK(near(bounds.x, 10.0f) && near(bounds.width, 40.0f));
    CHECK(renderer->bounds(scene, scene->object("dot"), 2.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f) && near(bounds.center().y, 60.0f));
    delete scene;

    // A scale in another track adds to a running shift, about the moving center, and a set
    // shift lands on top of a running turn.
    static constexpr auto layered = R"lua(
local s = vis.scene {width = 160, height = 80, camera = {mode = "fixed", view = "2d", height = 4}}
local dot = s:rectangle {center = {-2, 0}, size = {1, 1}, fill = "#ffffff", stroke = "#00000000", id = "dot"}
s:parallel(
    function() s:shift(dot, {4, 0}, 2, "linear") end,
    function()
        s:wait(0.5)
        s:scale(dot, 2, {duration = 1, curve = "linear"})
    end)
s:rotate(dot, math.pi, {duration = 1})
s:at(2.5, function() s:set {target = dot, shift = {0, 1}} end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(layered, static_cast<uint32_t>(std::strlen(layered)), "layered.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "layered.lua: %s\n", error);
    if (scene) {
        CHECK(std::fabs(scene->duration() - 3.0f) < 1.0e-4f);
        CHECK(renderer->bounds(scene, scene->object("dot"), 1.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 80.0f) && near(bounds.center().y, 40.0f) && near(bounds.width, 30.0f));
        CHECK(renderer->bounds(scene, scene->object("dot"), 2.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.width, 40.0f));
        CHECK(renderer->bounds(scene, scene->object("dot"), 2.75f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.center().y, 20.0f));
        CHECK(renderer->bounds(scene, scene->object("dot"), 3.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.center().y, 20.0f) && near(bounds.width, 40.0f));
    }
    delete scene;

    // A transform that turns adds to a running shift, rigid along its arc, and a set transform
    // turns the bar back on top of the next shift; each lands on its matrix when the shift ends.
    static constexpr auto turned = R"lua(
local s = vis.scene {width = 160, height = 80, camera = {mode = "fixed", view = "2d", height = 4}}
local bar = s:rectangle {center = {-2, 0}, size = {2, 0.5}, fill = "#ffffff", stroke = "#00000000", id = "bar"}
s:parallel(
    function() s:shift(bar, {4, 0}, 2, "linear") end,
    function()
        s:wait(0.5)
        s:play({target = bar, transform = {0, -1, 0, 2, 1, 0, 0, 2, 0, 0, 1, 0, 0, 0, 0, 1}, arc = math.pi / 2}, 1,
               "linear")
    end)
s:shift(bar, {0, -1}, 1, "linear")
s:at(2.5, function() s:set {target = bar, transform = {1, 0, 0, 4, 0, 1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1}} end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(turned, static_cast<uint32_t>(std::strlen(turned)), "turned.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "turned.lua: %s\n", error);
    if (scene) {
        CHECK(renderer->bounds(scene, scene->object("bar"), 1.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 80.0f) && near(bounds.center().y, 40.0f));
        CHECK(near(bounds.width, 35.36f) && near(bounds.height, 35.36f));
        CHECK(renderer->bounds(scene, scene->object("bar"), 1.5f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 100.0f) && near(bounds.width, 10.0f) && near(bounds.height, 40.0f));
        CHECK(renderer->bounds(scene, scene->object("bar"), 2.4f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.center().y, 48.0f) && near(bounds.width, 10.0f));
        CHECK(renderer->bounds(scene, scene->object("bar"), 2.5f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.center().y, 50.0f) && near(bounds.width, 40.0f));
        CHECK(renderer->bounds(scene, scene->object("bar"), 3.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.center().y, 60.0f) && near(bounds.height, 10.0f));
    }
    delete scene;

    // A morph that begins during a shift adds its move on top and ends with the shift, where the
    // square it becomes takes over.
    static constexpr auto morphed = R"lua(
local s = vis.scene {width = 160, height = 80, camera = {mode = "fixed", view = "2d", height = 4}}
local dot = s:circle {center = {-2, 0}, radius = 0.5, fill = "#ffffff", stroke = "#00000000", id = "dot"}
s:parallel(
    function() s:shift(dot, {4, 0}, 2, "linear") end,
    function()
        s:wait(1)
        local box = s:rectangle {center = {2, 1}, size = {1, 1}, fill = "#ffffff", stroke = "#00000000", id = "box"}
        s:morph(dot, box, 1, "linear")
    end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(morphed, static_cast<uint32_t>(std::strlen(morphed)), "morphed.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "morphed.lua: %s\n", error);
    if (scene) {
        CHECK(renderer->bounds(scene, scene->object("dot"), 1.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 80.0f) && near(bounds.center().y, 40.0f));
        CHECK(renderer->bounds(scene, scene->object("dot"), 1.5f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 100.0f) && near(bounds.center().y, 30.0f));
        CHECK(renderer->bounds(scene, scene->object("box"), 2.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.center().y, 20.0f) && near(bounds.width, 20.0f));
    }
    delete scene;

    // A tracker moves a bound shift while another track scales the object.
    static constexpr auto tracked = R"lua(
local s = vis.scene {width = 160, height = 80, camera = {mode = "fixed", view = "2d", height = 4}}
local dot = s:rectangle {center = {-2, 0}, size = {1, 1}, fill = "#ffffff", stroke = "#00000000", id = "dot"}
local x = s:tracker(0)
s:bind(dot, x, function(v) return {shift = {v, 0}} end)
s:parallel(
    function() s:track(x, 4, 2, "linear") end,
    function()
        s:wait(0.5)
        s:scale(dot, 2, {duration = 1, curve = "linear"})
    end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(tracked, static_cast<uint32_t>(std::strlen(tracked)), "tracked.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "tracked.lua: %s\n", error);
    if (scene) {
        CHECK(renderer->bounds(scene, scene->object("dot"), 1.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 80.0f) && near(bounds.center().y, 40.0f) && near(bounds.width, 30.0f));
        CHECK(renderer->bounds(scene, scene->object("dot"), 2.0f, bounds) == Result::Success);
        CHECK(near(bounds.center().x, 120.0f) && near(bounds.width, 40.0f));
    }
    delete scene;

    // A hidden node still anchors its incoming edge on the circle, and a token reads the
    // moving connector's sampled ends instead of a copy made at the call.
    static constexpr auto connected = R"lua(
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local node = s:group {}
node:circle {radius = 1}
node:text {text = "node"}
local sink = s:circle {center = {3, 3}, radius = 0.5}
local edge = s:connector {from = node, to = sink, id = "edge", padding = 0.1}
s:create(edge, 0.5, "linear")
s:fade_in(node, {duration = 0.5})
local token = s:circle {radius = 0.1, id = "token"}
s:parallel(
    function() s:shift(sink, {1, 0}, 1, "linear") end,
    function() s:move_along(token, edge, {from = 1, to = 0, duration = 1, curve = "linear"}) end)
return s
)lua";
    scene = nullptr;
    CHECK(Lua::load(connected, static_cast<uint32_t>(std::strlen(connected)), "connected.lua", &scene,
                    error, sizeof(error)) == Result::Success);
    if (scene) {
        LayoutReport report;
        CHECK(renderer->layout(scene, 0.25f, report) == Result::Success);
        auto found = false;
        for (auto i = 0u; i < report.pathCount(); i++) {
            auto path = report.pathAt(i);
            auto visual = report.visualAt(path->visual);
            if (report.objectAt(visual->object)->object != scene->object("edge")) continue;
            found = true;
            auto x = (path->points[0].x - 100.0f) / 20.0f;
            auto y = (100.0f - path->points[0].y) / 20.0f;
            CHECK(std::fabs(x - 1.1f / std::sqrt(2.0f)) < 0.001f);
            CHECK(std::fabs(y - x) < 0.001f);
        }
        CHECK(found);
        const float times[] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 1.25f};
        for (auto time : times) {
            auto t = time - 1.0f;
            auto dx = 3.0f + t;
            auto length = std::sqrt(dx * dx + 9.0f);
            auto fraction = 1.0f - t;
            auto distance = 1.1f + (length - 1.7f) * fraction;
            CHECK(renderer->bounds(scene, scene->object("token"), time, bounds) == Result::Success);
            CHECK(std::fabs(bounds.center().x - (100 + 20 * dx * distance / length)) < 0.01f);
            CHECK(std::fabs(bounds.center().y - (100 - 60 * distance / length)) < 0.01f);
        }
    } else std::fprintf(stderr, "connected.lua: %s\n", error);
    delete scene;
    static constexpr auto cyclic = "local s=vis.scene{} local a=s:circle{} "
                                  "local b=s:circle{center={3,3}} local k=s:connector{from=a,to=b} "
                                  "s:move_along(a,k) return s";
    scene = nullptr;
    CHECK(Lua::load(cyclic, static_cast<uint32_t>(std::strlen(cyclic)), "cyclic.lua", &scene,
                    error, sizeof(error)) == Result::ScriptError);
    CHECK(std::strstr(error, "an endpoint cannot move along its connector"));
    delete scene;

    _error("local s = vis.scene {} local a = s:rectangle {} local t = s:text {text = 'x'} "
           "s:move_along(a, t) return s", "move-along-text.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:move_along(a, a) return s",
           "move-along-self.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} local c = s:circle {} "
           "s:move_along(a, c, {from = 2}) return s", "move-along-from.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:play({target = a, opacity = 0.5, arc = 1}) return s",
           "arc-without-travel.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:play({target = a, shift = {1, 0}, arc = 7}) return s",
           "arc-range.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:rotate(a, 1, {about = 'middle'}) return s",
           "rotate-about.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:rotate(a, 1, {axis = {0, 0, 0}}) return s",
           "rotate-axis.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:rotate({a, a}, 1) return s",
           "rotate-duplicate.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:scale(a, 'big') return s", "scale-factor.lua");
    _error("local s = vis.scene {} local a = s:rectangle {} s:scale(a, 2, {spin = true}) return s",
           "scale-option.lua");
}

// The layer an object draws at at time, as a layout report gives it, or INT32_MIN without one.
static int32_t _layerAt(SwRenderer* renderer, Scene* scene, const char* id, float time)
{
    LayoutReport report;
    auto object = scene->object(id);
    if (!object || renderer->layout(scene, time, report) != Result::Success) return INT32_MIN;
    for (auto i = 0u; i < report.objectCount(); i++) {
        auto item = report.objectAt(i);
        if (item && item->object == object) return item->layer;
    }
    return INT32_MIN;
}

static void _layering(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 80, camera = {height = 4}}
local low = s:rectangle {center = {-2, 0}, size = {2, 2}, id = "low"}
local card = s:group {id = "card"}
card:rectangle {center = {2, 0}, size = {2, 2}, id = "face", layer = 1}
s:set {target = card, layer = 2}
s:play({target = low, layer = 4}, 1)
s:set {target = card, layer = 0}
s:play({target = low, layer = -1, shift = {0, 0.5}}, 1)
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "layers.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "layers.lua: %s\n", error);
    if (scene) {
        // A higher layer applies as the play begins and a lower one as it ends.
        CHECK(_layerAt(renderer, scene, "low", 0.0f) == 4);
        CHECK(_layerAt(renderer, scene, "low", 1.5f) == 4);
        CHECK(_layerAt(renderer, scene, "low", 2.0f) == -1);
        // set moves a group's starting layer, and the objects below it, before the timeline,
        // and switches it at the cursor afterwards, the objects below moving with it.
        CHECK(_layerAt(renderer, scene, "card", 0.5f) == 2);
        CHECK(_layerAt(renderer, scene, "face", 0.5f) == 3);
        CHECK(_layerAt(renderer, scene, "card", 1.0f) == 0);
        CHECK(_layerAt(renderer, scene, "face", 1.0f) == 1);
        CHECK(scene->object("low") && scene->object("low")->layer == -1);
        CHECK(scene->object("face") && scene->object("face")->layer == 3);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:play({target=c, layer=1.5}, 1); return s",
                 "layer must be an integer");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:set{target=c, layer=math.maxinteger}; return s",
                 "layer is out of range");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:play({target=c, layer=1}, 2); "
                 "s:at(1, function() s:set{target=c, layer=2} end); return s",
                 "already animates its layer until 2.00 s");
    _scriptError("local s=vis.scene{}; local x=s:tracker(0); local c=s:circle{}; "
                 "s:bind(c, x, function(v) return {layer=1} end); return s",
                 "a bound state cannot set 'layer'");
}

// The center of the ink an object paints at time, in pixels, or (-1, -1) when it paints none.
static Vec2 _centerAt(SwRenderer* renderer, Scene* scene, const char* id, float time)
{
    auto object = scene->object(id);
    BBox ink;
    if (!object || renderer->bounds(scene, object, time, ink) != Result::Success) return {-1.0f, -1.0f};
    return ink.center();
}

// The pixel at (x, y) of the frame at time, packed as _pixel packs a color.
static uint32_t _pixelAt(SwRenderer* renderer, Scene* scene, float time, uint32_t x, uint32_t y)
{
    Surface surface;
    if (renderer->render(scene, time, surface) != Result::Success) return 0u;
    return surface.data()[static_cast<size_t>(y) * surface.stride() + x];
}

// A morph of two surfaces calls their color function again at the points between the two: the
// face over (0.1, 1.5) lies near height 0 halfway from the bowl to the saddle, gray, where the
// bowl is orange and the saddle blue, so a blend of the two ends would be neither.
static void _pieceMorphs(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {height = 6}}
local a = s:plot {points = {{-2,0},{-1,0},{0/0,0},{1,0},{2,0}}, id = "source"}
local b = s:path {commands = {{type="move",to={-2,2}},{type="line",to={-1,2}},{type="move",to={1,2}},{type="line",to={2,2}}}, id = "first"}
local c = s:plot {points = {{-2,-2},{-1,-2},{0/0,0},{1,-2},{2,-2}}, id = "second"}
s:replacement_transform({a,a}, {b,c}, 1, "linear", {copy=true, lag=0.25, pairs={{1,2}}})
return s
)lua";
    Scene* scene = nullptr;
    char error[512] = {};
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "piece-morph.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "piece-morph.lua: %s\n", error);
    if (scene) {
        CHECK(_near(scene->duration(), 1.25f));
        BBox bounds;
        CHECK(renderer->bounds(scene, scene->object("source"), 1.25f, bounds) == Result::Success);
        CHECK(renderer->bounds(scene, scene->object("first"), 1.25f, bounds) == Result::Success);
        CHECK(std::fabs(bounds.center().y - 30.0f) < 0.1f);
        LayoutReport report;
        CHECK(renderer->layout(scene, 0.5f, report) == Result::Success);
        CHECK(report.pathCount() == 6u);
        Surface surface;
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        auto hash = _surfaceHash(surface);
        CHECK(renderer->render(scene, 1.25f, surface) == Result::Success);
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success && _surfaceHash(surface) == hash);
    }
    delete scene;
    auto fails = [](const char* options, const char* expected) {
        char script[1024];
        std::snprintf(script, sizeof(script), "local s=vis.scene{} "
            "local a=s:plot{points={{0,0},{1,0},{0/0,0},{2,0},{3,0}}} "
            "local b=s:plot{points={{0,1},{1,1},{0/0,0},{2,1},{3,1}}} "
            "s:morph(a,b,1,'linear',%s) return s", options);
        _scriptError(script, expected);
    };
    fails("{pairs=1}", "morph pairs must be an array of {source_index, target_index}");
    fails("{pairs={{1}}}", "morph pairs must be an array of {source_index, target_index}");
    fails("{pairs={{1.5,1}}}", "morph piece index must be an integer");
    fails("{pairs={{0,1}}}", "morph piece index must be positive and within range");
    fails("{pairs={{3,1}}}", "morph pairs need distinct piece indices within each outline");
    fails("{pairs={{1,1},{1,2}}}", "morph pairs need distinct piece indices within each outline");
    fails("{pairs={{1,1},{2,1}}}", "morph pairs need distinct piece indices within each outline");
}

static void _surfaceMorph(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 200, height = 200, background = "#101010",
    camera = {view = "3d", eye = {0, 0, 10}, target = {0, 0, 0}, up = {0, 1, 0},
        projection = "orthographic", height = 5}}
local function grid(n, f)
    local points = {}
    for j = 0, n - 1 do
        for i = 0, n - 1 do
            local x, y = -2 + 4 * i / (n - 1), -2 + 4 * j / (n - 1)
            points[#points + 1] = {x, y, f(x, y)}
        end
    end
    return points
end
local function height(x, y, z)
    if z > 0.2 then return "#f59e0b" elseif z < -0.2 then return "#3b82f6" end
    return "#e5e7eb"
end
local bowl = s:surface {points = grid(21, function(x, y) return (x * x + y * y) / 4 end),
    size = {21, 21}, shading = false, colors = height}
local saddle = s:surface {points = grid(21, function(x, y) return (x * x - y * y) / 4 end),
    size = {21, 21}, shading = false, colors = height}
s:morph(bowl, saddle, 1, "linear")
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "morph.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "morph.lua: %s\n", error);
    if (scene) {
        CHECK(_pixelAt(renderer, scene, 0.0f, 104u, 40u) == _pixel(Color::hex("#f59e0b")));
        CHECK(_pixelAt(renderer, scene, 0.5f, 104u, 40u) == _pixel(Color::hex("#e5e7eb")));
        CHECK(_pixelAt(renderer, scene, 1.0f, 104u, 40u) == _pixel(Color::hex("#3b82f6")));
    }
    delete scene;
    // The function fails at a point between the two, after the eight calls that made them.
    _scriptError(R"lua(
local s = vis.scene {}
local calls = 0
local function f(x, y, z)
    calls = calls + 1
    if calls > 8 then return nil end
    return "#ff0000"
end
local a = s:surface {points = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}, size = {2, 2}, colors = f}
local b = s:surface {points = {{0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}}, size = {2, 2}, colors = f}
s:morph(a, b)
return s
)lua",
                 "surface colors must return a hex color or Theme color role; at (0.0, 0.0, 0.0) it did not");
    _scriptError(R"lua(
local s = vis.scene {}
local a = s:surface {points = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}, size = {2, 2}}
local b = s:surface {points = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {0, 1, 0}, {1, 1, 0}, {2, 1, 0}},
    size = {3, 2}}
s:morph(a, b)
return s
)lua",
                 "morph needs two surfaces with the same columns and rows of points");
    _scriptError(R"lua(
local s = vis.scene {}
local a = s:surface {points = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}, size = {2, 2}}
local b = s:circle {radius = 1}
s:morph(a, b)
return s
)lua",
                 "morph needs a line, circle, rectangle, polygon, plot, path or curve on both sides, or two surfaces");
}

static void _savingStates(SwRenderer* renderer)
{
    // 20 pixels per unit with the origin at pixel (80, 40): the card at (40, 40).
    static constexpr auto source = R"lua(
local s = vis.scene {width = 160, height = 80, camera = {height = 4}}
local card = s:rectangle {center = {-2, 0}, size = {1, 1}, fill = "#ff0000", id = "card"}
local word = s:text {text = "cat", point = {0, 1.5}, size = 12, id = "word"}
local total = s:number {value = 5, point = {0, -1.5}, size = 12, id = "total"}
local dot = s:circle {center = {2, 0}, radius = 0.5, fill = "#0000ff", id = "dot"}
s:save {card, word, total}
s:save(dot)
s:set {target = word, text = "dog"}
s:play({{target = card, shift = {1, 1}, fill = "#00ff00", size = {2, 1}, corner = 0.25},
        {target = word, text = "horse"}, {target = total, value = 100},
        {target = dot, opacity = 0.25, layer = 3}}, 1, "linear")
s:restore({card, word, total}, 1, "linear")
s:restore(dot)
s:save(card)
s:play({target = card, fill = "#0000ff"}, 1)
s:save(card)
s:play({target = card, shift = {0, 1}, opacity = 0.5}, 1)
s:restore(card)
s:restore(card, 1)
s:save {card, dot}
s:play({{target = card, shift = {0, 1}}, {target = dot, shift = {0, 1}}}, 1)
s:restore({card, dot}, 1, "linear", 0.5)
s:save(card)
s:play({target = card, shift = {2, 0}}, 2, "linear")
s:at(8.5, function() s:restore(card, 1, "linear") end)
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "save.lua", &scene, error, sizeof(error))
          == Result::Success);
    if (!scene) std::fprintf(stderr, "save.lua: %s\n", error);
    if (scene) {
        auto near = [](float lhs, float rhs) { return std::fabs(lhs - rhs) < 1.0f; };
        // A restore brings back a move, a recolor, a shape tween and text and number changes;
        // the text comes back to the string it showed when saved, before set replaced it.
        auto center = _centerAt(renderer, scene, "card", 1.5f);
        CHECK(near(center.x, 50.0f) && near(center.y, 30.0f));
        center = _centerAt(renderer, scene, "card", 2.0f);
        CHECK(near(center.x, 40.0f) && near(center.y, 40.0f));
        CHECK(_pixelAt(renderer, scene, 2.0f, 40u, 40u) == _pixel(Color::hex("#ff0000")));
        CHECK(std::strcmp(_shows(scene, "word", 0.0f), "dog") == 0);
        CHECK(std::strcmp(_shows(scene, "word", 1.0f), "horse") == 0);
        CHECK(std::strcmp(_shows(scene, "word", 2.0f), "cat") == 0);
        CHECK(std::strcmp(_shows(scene, "total", 1.0f), "100") == 0);
        CHECK(std::strcmp(_shows(scene, "total", 2.0f), "5") == 0);
        // Without a duration a restore switches at the cursor, a lower layer included.
        CHECK(_layerAt(renderer, scene, "dot", 1.5f) == 3);
        CHECK(_layerAt(renderer, scene, "dot", 2.0f) == 0);
        // Saves nest, and each restore takes the newest state of its target.
        CHECK(near(_centerAt(renderer, scene, "card", 3.99f).y, 20.0f));
        CHECK(near(_centerAt(renderer, scene, "card", 4.0f).y, 40.0f));
        CHECK(_pixelAt(renderer, scene, 4.0f, 40u, 40u) == _pixel(Color::hex("#0000ff")));
        CHECK(_pixelAt(renderer, scene, 5.0f, 40u, 40u) == _pixel(Color::hex("#ff0000")));
        // A later target begins later by the lag.
        CHECK(near(_centerAt(renderer, scene, "card", 6.5f).y, 30.0f));
        CHECK(near(_centerAt(renderer, scene, "dot", 6.5f).y, 20.0f));
        CHECK(near(_centerAt(renderer, scene, "dot", 7.5f).y, 40.0f));
        // A change of place alone adds to a motion that still runs.
        CHECK(near(_centerAt(renderer, scene, "card", 8.5f).x, 60.0f));
        CHECK(near(_centerAt(renderer, scene, "card", 9.0f).x, 50.0f));
        CHECK(near(_centerAt(renderer, scene, "card", 9.5f).x, 40.0f));
        CHECK(std::fabs(scene->duration() - 9.5f) < 1.0e-4f);
        // The saved values are the objects' latest state again.
        auto card = static_cast<Rectangle*>(scene->object("card"));
        auto word = static_cast<Text*>(scene->object("word"));
        auto total = static_cast<Text*>(scene->object("total"));
        auto dot = scene->object("dot");
        CHECK(card && _near(card->size.x, 1.0f) && _near(card->size.y, 1.0f) && _near(card->corner, 0.0f));
        CHECK(card && _same(card->style.fill, Color::hex("#ff0000")) && _near(card->opacity, 1.0f));
        CHECK(card && std::fabs(card->model.e[3]) < 1.0e-4f && std::fabs(card->model.e[7]) < 1.0e-4f);
        CHECK(word && std::strcmp(word->text(), "cat") == 0);
        CHECK(total && _near(total->value(), 5.0f));
        CHECK(dot && _near(dot->opacity, 1.0f) && dot->layer == 0);
    }
    delete scene;

    _scriptError("local s=vis.scene{}; local c=s:circle{id='c'}; s:restore(c, 1); return s",
                 "could not restore: 'c' has no saved state; save it before restoring");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:save(c); s:restore(c); s:restore(c); return s",
                 "could not restore: the object has no saved state");
    _scriptError("local s=vis.scene{}; local a=s:circle{}; local b=s:circle{}; s:save(a, b); return s",
                 "save takes one handle or a list of handles");
    _scriptError("local s=vis.scene{}; s:save({}); return s", "handle array cannot be empty");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:save({c, 1}); return s",
                 "expected an object handle, got number");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:save({c, c}); return s",
                 "could not save: an object can take part only once in one call");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:save(c); s:restore(c, -1); return s",
                 "duration must be non-negative");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:save(c); s:restore(c, 1, 'linear', -1); return s",
                 "lag cannot be negative");
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:save(c); s:play({target=c, fill='#ff0000'}, 2); "
                 "s:at(1, function() s:restore(c) end); return s",
                 "already animates its fill color until 2.00 s");
    // Each save is charged to the script by the objects it records.
    _scriptError("local s=vis.scene{}; local g=s:group{}; for i=1,1000 do g:point{point={0,0}} end; "
                 "for i=1,200 do s:save(g) end; return s",
                 "scene native memory limit exceeded");
}

// Warp callbacks run during authoring; quadratic maps remain exact between grid samples.
static void _warping(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 480, height = 270, background = "#000000", antialiasing = false,
    camera = {view = "2d", height = 9}}
local plane = s:space {id = "plane", x = {-4, 4, 1}, y = {-3, 3, 1}, stroke = "#ffffff", width = 2}
local calls = 0
s:warp(plane, function(x, y)
    calls = calls + 1
    return x * x - y * y, 2 * x * y
end, 1, "linear")
if calls ~= 8051 then error("the warp called fn " .. calls .. " times") end
local dot = s:point {id = "dot", point = {1, 1}, radius = 6}
local lifted = s:point {id = "lifted", point = {-1, 1}, radius = 6}
local bar = s:line {id = "bar", from = {-1.5, -3.5}, to = {1.5, -3.5}}
s:warp(dot, function(x, y) return {x + 1, y} end, 0.5)
s:warp(lifted, function(x, y, z) return x, y + 1, z + 2 end, 0.5)
-- A step of 0.5 cuts the line 3 long into six pieces.
calls = 0
s:warp(bar, function(x, y) calls = calls + 1; return {x, y + 0.5, 0} end, 0.5, "smooth", {step = 0.5})
if calls ~= 7 then error("the step left " .. calls .. " points") end
return s
)lua";
    static constexpr auto sampled = R"lua(
local s = vis.scene {width = 480, height = 270, background = "#000000", antialiasing = false,
    camera = {view = "2d", height = 9}}
local plane = s:space {id = "plane", x = {-4, 4, 1}, y = {-3, 3, 1}, stroke = "#ffffff", width = 2}
local values = {}
for j = 0, 12 do
    for i = 0, 16 do
        local x, y = -4 + 0.5 * i, -3 + 0.5 * j
        values[#values + 1] = {x * x - y * y, 2 * x * y}
    end
end
-- Points between samples land on their exact images, inside and past the last samples.
local inner = s:point {id = "inner", point = {0.25, 0.75}, radius = 2}
local corner = s:point {id = "corner", point = {-3.75, -2.75}, radius = 2}
s:warp({plane, inner, corner}, {x_range = {-4, 4, 0.5}, y_range = {-3, 3, 0.5}, values = values}, 1, "linear")
-- Two samples a side map in a straight line, here doubling, and a point keeps its z.
local dot = s:point {id = "dot", point = {0.25, 0.5}, radius = 6}
local high = s:point {id = "high", point = {0.5, 1, 2}, radius = 6}
s:warp({dot, high}, {x_range = {0, 1, 1}, y_range = {0, 1, 1}, values = {{0, 0}, {2, 0}, {0, 2}, {2, 2}}}, 0.5)
return s
)lua";
    auto load = [](const char* name, const char* script) {
        char error[256] = {};
        Scene* scene = nullptr;
        CHECK(Lua::load(script, static_cast<uint32_t>(std::strlen(script)), name, &scene, error, sizeof(error))
              == Result::Success);
        if (!scene) std::fprintf(stderr, "%s: %s\n", name, error);
        return scene;
    };
    auto background = _pixel(Color::hex("#000000"));
    Surface surface;
    // Whether any of the nine pixels nearest world point (x, y) shows something, or none of the
    // 25 nearest does.
    auto shows = [&](float x, float y, int reach) {
        auto column = static_cast<int>(std::lround(240.0f + 30.0f * x));
        auto row = static_cast<int>(std::lround(135.0f - 30.0f * y));
        for (auto dy = -reach; dy <= reach; dy++) {
            for (auto dx = -reach; dx <= reach; dx++) {
                if (surface.data()[static_cast<size_t>(row + dy) * surface.stride() + column + dx] != background) {
                    return true;
                }
            }
        }
        return false;
    };
    // The grid leaves where its lines stood for their images over the first second.
    auto bends = [&](Scene* scene) {
        const float images[][2] = {{3.51f, 2.8f}, {1.56f, 3.2f}, {0.64f, -1.2f}, {-0.64f, -1.2f}, {-0.69f, -2.6f},
                                   {3.7975f, -1.8f}};
        const float gone[][2] = {{2.5f, 1.0f}, {-2.5f, -1.0f}, {2.0f, -2.5f}, {-1.5f, 2.0f}, {2.5f, 2.0f}, {-3.5f, 1.0f}};
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        for (auto& point : images) CHECK(!shows(point[0], point[1], 2));
        for (auto& point : gone) CHECK(shows(point[0], point[1], 1));
        // Halfway each point is halfway to its image: (2, 0.7) halfway to (3.51, 2.8).
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(shows(2.755f, 1.75f, 1) && shows(-0.845f, -0.65f, 1) && shows(-1.805f, 1.6f, 1));
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        for (auto& point : images) CHECK(shows(point[0], point[1], 1));
        for (auto& point : gone) CHECK(!shows(point[0], point[1], 2));
    };
    Bounds bounds;
    auto scene = load("warp.lua", source);
    if (scene) {
        bends(scene);
        // A list moves the dot by one, a third number lifts the other off the plane, and the
        // bar ends half a unit up; their bounds read the images.
        auto dot = scene->object("dot");
        auto lifted = scene->object("lifted");
        auto bar = scene->object("bar");
        CHECK(dot && dot->bounds(bounds) && _near(bounds.center(), 2.0f, 1.0f, 0.0f));
        CHECK(lifted && lifted->bounds(bounds) && _near(bounds.center(), -1.0f, 2.0f, 2.0f));
        CHECK(bar && bar->bounds(bounds) && _near(bounds.min, -1.5f, -3.0f, 0.0f) && _near(bounds.max, 1.5f, -3.0f, 0.0f));
        CHECK(_near(scene->duration(), 2.5f) && scene->clipCount() == 4u);
    }
    delete scene;
    scene = load("sampled.lua", sampled);
    if (scene) {
        bends(scene);
        auto inner = scene->object("inner");
        auto corner = scene->object("corner");
        auto dot = scene->object("dot");
        auto high = scene->object("high");
        CHECK(inner && inner->bounds(bounds) && _near(bounds.center(), -0.5f, 0.375f, 0.0f));
        CHECK(corner && corner->bounds(bounds) && _near(bounds.center(), 6.5f, 20.625f, 0.0f, 1.0e-3f));
        CHECK(dot && dot->bounds(bounds) && _near(bounds.center(), 0.5f, 1.0f, 0.0f));
        CHECK(high && high->bounds(bounds) && _near(bounds.center(), 1.0f, 2.0f, 2.0f));
        CHECK(_near(scene->duration(), 1.5f) && scene->clipCount() == 5u);
    }
    delete scene;

    static constexpr auto prefix =
        "local s=vis.scene{}; local function F(x, y) return x, y end; local a=s:point{point={1, 0}}; ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:warp(a)", "warp requires fn, a function of x, y and z that returns the image point, "
                       "or its images sampled as {x_range, y_range, values}");
    fails("s:warp(a, F, 0)", "duration must be positive");
    fails("s:warp(a, F, 1, 'linear', {step = 0})", "warp step must be a positive length");
    fails("s:warp(a, F, 1, 'linear', {steps = 1})", "unknown option 'steps'");
    fails("s:warp({a, a}, F)", "animation targets must be distinct");
    fails("s:warp(a, function() return 'x' end)",
          "warp fn must return x and y, or x, y and z, as numbers or {x, y, z}; at (1.0, 0.0, 0.0) it did not");
    fails("s:warp(a, function() error('no image here') end)", "no image here");
    fails("s:warp(a, function(x, y) s:wait(1); return x, y end)",
          "scene authoring is unavailable inside a sampling callback");
    fails("s:warp(a, function(x, y) return x, 1 / 0 end)",
          "could not warp: the warp function gives no finite image of the point (1, 0, 0)");
    fails("s:warp(s:surface{points={{0,0,0},{1,0,0},{0,1,0},{1,1,0}}, size={2,2}}, F)",
          "could not warp: a warp cannot bend cells or surfaces");
    fails("s:warp(s:line{from={0,0}, to={3,0}}, F, 1, 'linear', {step = 1e-6})",
          "could not warp: the scene warps too many points; raise the step or warp fewer objects");
    fails("local c=s:circle{}; s:warp(c, F); s:play({target=c, radius=2})",
          "keeps the outline its warp gave, so its radius, size and corner no longer change");
    // Images sampled on a grid cover every point they map, two samples or more a side.
    fails("s:warp(a, {y_range={0,1,1}, values={}})", "warp samples require x_range");
    fails("s:warp(a, {x_range={0,2,1}, y_range={0,1,1}, value={}})", "unknown option 'value'");
    fails("s:warp(a, {x_range={0,2,1}, y_range={0,1,2}, values={}})",
          "warp samples need at least two samples along x_range and two along y_range");
    fails("s:warp(a, {x_range={0,2000,1}, y_range={0,2000,1}, values={}})",
          "warp samples hold at most 1048576 images; widen the steps of x_range and y_range");
    fails("s:warp(a, {x_range={0,2,1}, y_range={0,1,1}, values={{0,0}}})",
          "warp values must hold one {x, y} for each of the 6 samples of x_range and y_range");
    fails("s:warp(a, {x_range={0,1,1}, y_range={0,1,1}, values={1,2,3,4}})", "warp values must be {x, y} images");
    fails("s:warp(a, {x_range={0,1,1}, y_range={0,1,1}, values={{0,0},{1,0},{0,1},{1,0/0}}})",
          "vector component must be a finite number");
    fails("s:warp(a, {x_range={0,0.5,0.5}, y_range={0,1,1}, values={{0,0},{1,0},{0,1},{1,1}}})",
          "the warp point (1.0, 0.0, 0.0) lies outside its samples, x from 0.0 to 0.5 and y from 0.0 to 1.0");
}

// A copy of a warped object keeps what the warp left: the twin of a bar bent by a sine shows
// the same curve from the moment it is attached and a later warp of the twin goes on from it,
// a group's copy holds its bent line, and morph and match_transform with copy = true start
// from the bent bar. Nothing is copied while a warp runs below the source.
static void _warpedClones(SwRenderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {mode = "fixed", view = "2d", height = 9}}
local bar = s:rectangle {center = {-4, 0}, size = {4, 0.5}, fill = "#ff0000", stroke = "#00000000", id = "bar"}
local row = s:group {id = "row"}
row:line {from = {1, 2}, to = {5, 2}, id = "row/line"}
s:warp({bar, row}, function(x, y) return x, y + 1.5 * math.sin(x) end, 1, "linear", {step = 0.1})
local twin = s:clone(bar, {id = "twin", shift = {6, 0}})
s:clone(row, {id = "low", shift = {0, -3}})
s:warp(twin, function(x, y) return x, y - 1 end, 1, "linear")
local disc = s:circle {center = {4, -3}, radius = 0.5, id = "disc"}
s:morph(bar, disc, 1, "linear", {copy = true})
local slab = s:rectangle {center = {-4, -3}, size = {2, 1}, id = "slab"}
s:match_transform(bar, slab, 1, "linear", {copy = true, shapes = true})
return s
)lua";
    char error[512] = {};
    Scene* scene = nullptr;
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "warped-clone.lua", &scene, error,
                    sizeof(error)) == Result::Success);
    if (!scene) std::fprintf(stderr, "warped-clone.lua: %s\n", error);
    if (scene) {
        // The pixel bounds of the object named id at time, and whether they are those of
        // reference moved by dx and dy pixels.
        auto drawn = [&](const char* id, float time, BBox& box) {
            auto object = scene->object(id);
            return object && renderer->bounds(scene, object, time, box) == Result::Success;
        };
        auto matches = [&](const char* id, float time, const BBox& reference, float dx, float dy) {
            BBox box;
            if (!drawn(id, time, box)) return false;
            auto fits = std::fabs(box.x - reference.x - dx) < 0.5f && std::fabs(box.y - reference.y - dy) < 0.5f
                        && std::fabs(box.width - reference.width) < 0.5f
                        && std::fabs(box.height - reference.height) < 0.5f;
            if (!fits) {
                std::printf("  %s at %.2f s: %.2f x %.2f px at (%.2f, %.2f)\n", id, static_cast<double>(time),
                            static_cast<double>(box.width), static_cast<double>(box.height),
                            static_cast<double>(box.x), static_cast<double>(box.y));
            }
            return fits;
        };
        // Flat, the bar is 10 pixels high, and bent, 67; its twin, 120 pixels right, is bent
        // from the moment it is attached, and its own warp then carries the curve 20 pixels
        // down.
        BBox flat;
        BBox bent;
        CHECK(drawn("bar", 0.0f, flat) && std::fabs(flat.height - 10.0f) < 0.5f);
        CHECK(drawn("bar", 1.4f, bent) && std::fabs(bent.height - 67.3f) < 0.5f);
        CHECK(matches("twin", 1.0f, bent, 120.0f, 0.0f) && matches("twin", 1.4f, bent, 120.0f, 8.0f));
        CHECK(matches("twin", 2.0f, bent, 120.0f, 20.0f));
        // The group's copy holds the bent line three units, 60 pixels, down.
        BBox line;
        CHECK(drawn("row/line", 1.4f, line) && line.height > 60.0f && matches("low/line", 1.4f, line, 0.0f, 60.0f));
        // The copy the morph plays on starts bent, and so does the copy a match plays on, which
        // fades out in place: a warped rectangle hands the slab no size, so the slab fades in
        // where it stands.
        CHECK(matches("bar:copy", 2.0f, bent, 0.0f, 0.0f) && matches("bar:copy2", 3.5f, bent, 0.0f, 0.0f));
        BBox box;
        CHECK(drawn("slab", 3.5f, box) && std::fabs(box.center().x - 80.0f) < 0.5f
              && std::fabs(box.center().y - 150.0f) < 0.5f);
    }
    delete scene;

    static constexpr auto prefix = "local s=vis.scene{}; local b=s:line{from={-2, 0}, to={2, 0}}; local g=s:group{};"
                                   " g:line{from={-2, 1}, to={2, 1}};"
                                   " s:warp({b, g}, function(x, y) return x, y + math.sin(x) end, 1); ";
    char script[512];
    auto fails = [&](const char* body, const char* expected) {
        std::snprintf(script, sizeof(script), "%s%s return s", prefix, body);
        _scriptError(script, expected);
    };
    fails("s:at(0.5, function() s:clone(b) end)",
          "could not clone line: the object is in the middle of a warp; clone it once the warp ends");
    fails("s:at(0.5, function() s:clone(g) end)", "could not clone group: the object is in the middle of a warp");
    fails("s:at(0.5, function() s:morph(b, s:line{from={0, 2}, to={1, 2}}, 1, 'smooth', {copy=true}) end)",
          "could not play animation: the object is in the middle of a warp");
}

int main()
{
    auto renderer = SwRenderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
#ifdef VIS_TEST_FONT
        CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
#ifdef VIS_TEST_MONO_FONT
        CHECK(renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
#endif
#endif
        _pngCompression(renderer);
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
#ifdef VIS_TEST_FONT
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
#endif
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
a:replacement_transform(source, target)
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

    if (_failures) std::fprintf(stderr, "%d Lua checks failed\n", _failures);
    return _failures ? 1 : 0;
}
