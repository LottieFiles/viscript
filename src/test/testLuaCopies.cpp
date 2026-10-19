#include "testLuaChecks.h"

static bool _tagged(const Object* object, const char* tag)
{
    return object && object->tag() && std::strcmp(object->tag(), tag) == 0;
}

void _cloning(Renderer* renderer)
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
    auto scene = _loaded(source, "clone.lua");
    if (scene) {
        // The overrides apply to the copy, and a group passes its stroke to the copies that
        // take theirs from it.
        auto pair = scene->object("pair");
        auto red = Color::hex("#ff0000");
        CHECK(pair && pair->childCount() == 2u && _near(pair->model.e[7], -2.0f) && _near(pair->opacity, 0.5f));
        CHECK(pair && same(pair->childAt(0)->style.stroke, red));
        CHECK(pair && same(pair->childAt(1)->style.stroke, Color::hex("#0000ff")));
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

// morph and match_transform with copy = true transform a copy of the
// source and leave the source in place.
void _copyTransforms(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180}
local square = s:polygon {points = {{-4, 2}, {-2, 2}, {-2, 4}, {-4, 4}}, id = "square"}
local disc = s:circle {center = {3, 3}, radius = 1, id = "disc"}
s:morph(square, disc, 1, "linear", {copy = true})
local left = s:circle {center = {-3, -3}, radius = 0.5, id = "left"}
local right = s:circle {center = {3, -3}, radius = 0.5, id = "right"}
s:morph({square, disc}, {left, right}, 1, "linear", {lag = 0.5, copy = true})
local from = s:group {id = "from"}
from:circle {center = {-3, -1}, radius = 0.5, id = "from/dot"}
s:wait(1)
local to = s:group {id = "to"}
to:circle {center = {3, -1}, radius = 0.5, id = "to/dot"}
s:match_transform(from, to, 1, "linear", {copy = true, shapes = true})
return s
)lua";
    auto scene = _loaded(source, "copy.lua");
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
                 " s:morph(a, b, 1, 'smooth', {lag=-1}); return s",
                 "lag cannot be negative");
    _scriptError("local s=vis.scene{}; local a=s:circle{radius=1}; local b=s:circle{radius=2}; s:wait(1);"
                 " s:morph(a, b, 1, 'smooth', {copy=true}); return s",
                 "could not play animation: add the morph target right before the morph");
    _scriptError("local s=vis.scene{}; local a=s:group{}; local b=s:group{};"
                 " s:match_transform(a, b, 1, 'smooth', {copy='yes'}); return s",
                 "copy must be a boolean");
}

// Measured billboard text preserves its ink bounds across scaling, cloning and matching.
void _matchingBillboard(Renderer* renderer)
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
        auto scene = _loaded(source, "match-billboard.lua");
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

void _matchingPath(Renderer* renderer)
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
    auto scene = _loaded(source, "match-path.lua");
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

void _matching(Renderer* renderer)
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
    auto scene = _loaded(source, "match.lua");
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
    scene = _loaded(twins, "twins.lua");
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
        scene = _loaded(edited, "edited.lua");
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
        scene = _loaded(split, "split.lua");
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

// Warp callbacks run during authoring; quadratic maps remain exact between grid samples.
void _warping(Renderer* renderer)
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
    auto background = pixel(Color::hex("#000000"));
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
    auto scene = _loaded(source, "warp.lua");
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
    scene = _loaded(sampled, "sampled.lua");
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
void _warpedClones(Renderer* renderer)
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
    auto scene = _loaded(source, "warped-clone.lua");
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
