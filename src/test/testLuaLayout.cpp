#include "testLuaChecks.h"

// vis.merge and vis.keys stand in for pairs, whose order may change between runs.
void _tableHelpers()
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
void _queries()
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
void _rowQueries()
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

void _arranging()
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

void _advecting()
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

void _surrounding()
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
void _boundsQueries()
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
