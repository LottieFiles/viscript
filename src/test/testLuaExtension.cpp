#include <cstdio>
#include <cstring>
#include <new>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "visLuaExtension.h"
#include "testLuaCollector.h"

using namespace vis;

static int failures = 0;

static void check(bool condition, const char* expression, int line)
{
    if (condition) return;
    std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, line, expression);
    failures++;
}

#define CHECK(condition) check((condition), #condition, __LINE__)

struct HookContext
{
    uint32_t openOrder = 0;
    uint32_t loadedOrder = 0;
    bool failLoaded = false;
};

static int addTree(lua_State* lua)
{
    detail::luaScene(lua, 1);
    auto root = Group::gen();
    auto body = Rectangle::gen({0.0f, 0.0f, 0.0f}, {3.0f, 1.5f});
    auto label = Text::gen("extension tree");
    if (!root || !body || !label) {
        delete root;
        delete body;
        delete label;
        return luaL_error(lua, "could not allocate extension tree");
    }
    root->tag("extension-root");
    body->tag("extension-body");
    label->tag("extension-label");
    if (root->add(body) != Result::Success) {
        delete root;
        delete body;
        delete label;
        return luaL_error(lua, "could not attach extension body");
    }
    if (root->add(label) != Result::Success) {
        delete root;
        delete label;
        return luaL_error(lua, "could not attach extension label");
    }
    return detail::luaAdoptObject(lua, 1, root, "extension tree");
}

static int firstChild(lua_State* lua)
{
    auto scene = detail::luaScene(lua, 1);
    auto root = detail::luaObject(lua, scene, 2);
    if (!root->childCount()) return luaL_error(lua, "extension tree is empty");
    return detail::luaPushObject(lua, 1, root->childAt(0));
}

static int addOversizedTree(lua_State* lua)
{
    detail::luaScene(lua, 1);
    auto root = Group::gen();
    if (!root) return luaL_error(lua, "could not allocate oversized tree");
    for (auto i = 0u; i < 4096u; i++) {
        auto child = Point::gen({static_cast<float>(i), 0.0f, 0.0f});
        if (!child || root->add(child) != Result::Success) {
            delete child;
            delete root;
            return luaL_error(lua, "could not assemble oversized tree");
        }
    }
    return detail::luaAdoptObject(lua, 1, root, "oversized tree");
}

// group:test_box{...}: a group holding a rectangle, styled and attached by the core.
static int addBox(lua_State* lua)
{
    luaL_checktype(lua, 2, LUA_TTABLE);
    auto root = Group::gen();
    auto body = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    if (!root || !body || root->add(body) != Result::Success) {
        delete root;
        delete body;
        return luaL_error(lua, "could not allocate box");
    }
    return detail::luaAddObject(lua, root, 2, "box");
}

// What test_parts still holds; a script that fails frees it with the builder.
struct Parts
{
    Object* objects[3] = {};
    bool owned[3] = {};
    ~Parts()
    {
        for (auto i = 0u; i < 3u; i++) {
            if (owned[i]) delete objects[i];
        }
    }
};

static int partsGc(lua_State* lua)
{
    static_cast<Parts*>(lua_touserdata(lua, 1))->~Parts();
    return 0;
}

static void disown(void* owned) noexcept
{
    *static_cast<bool*>(owned) = false;
}

static int attachPart(lua_State* lua)
{
    auto parts = static_cast<Parts*>(lua_touserdata(lua, lua_upvalueindex(1)));
    auto index = static_cast<uint32_t>(lua_tointeger(lua, lua_upvalueindex(2)));
    return detail::luaAddObject(lua, parts->objects[index], 2, "part", disown, &parts->owned[index]);
}

// group:test_parts{id = ..., parts = {{...}, {...}}}: a group whose two parts attach one by
// one below its handle, each styled by its own table, as a module that sets formulas does.
static int addParts(lua_State* lua)
{
    luaL_checktype(lua, 2, LUA_TTABLE);
    auto parts = new (lua_newuserdatauv(lua, sizeof(Parts), 0)) Parts;
    auto store = lua_gettop(lua);
    lua_createtable(lua, 0, 1);
    lua_pushcfunction(lua, partsGc);
    lua_setfield(lua, -2, "__gc");
    lua_setmetatable(lua, store);
    parts->objects[0] = Group::gen();
    parts->objects[1] = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    parts->objects[2] = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    for (auto i = 0u; i < 3u; i++) parts->owned[i] = parts->objects[i] != nullptr;
    detail::luaAddObject(lua, parts->objects[0], 2, "parts", disown, &parts->owned[0]);
    auto root = lua_gettop(lua);
    lua_getfield(lua, 2, "parts");
    auto styles = lua_gettop(lua);
    for (auto i = 1; i <= 2; i++) {
        lua_pushvalue(lua, store);
        lua_pushinteger(lua, i);
        lua_pushcclosure(lua, attachPart, 2);
        lua_pushvalue(lua, root);
        lua_rawgeti(lua, styles, i);
        detail::luaCall(lua, 2, 1);
        lua_pop(lua, 1);
    }
    lua_pushvalue(lua, root);
    return 1;
}

static int sceneOf(lua_State* lua)
{
    return detail::luaHandleScene(lua, 1) ? 1 : 0;
}

static int texStandIn(lua_State* lua)
{
    lua_pushliteral(lua, "stand-in");
    return 1;
}

// Counts the waits of a script, then waits as the core does.
static int countedWait(lua_State* lua)
{
    lua_pushinteger(lua, lua_tointeger(lua, lua_upvalueindex(2)) + 1);
    lua_replace(lua, lua_upvalueindex(2));
    return detail::luaCallWrapped(lua, lua_upvalueindex(1));
}

static int waits(lua_State* lua)
{
    detail::luaGetMethod(lua, detail::LuaHandle::Scene, "wait");
    lua_getupvalue(lua, -1, 2);
    return 1;
}

// place(row, style, x, y, z) of a spans box: a rectangle over its ink, from 4 below the
// baseline to its top above it, attached below the row with the part's style; with skip,
// nothing.
static int placeBox(lua_State* lua)
{
    if (lua_toboolean(lua, lua_upvalueindex(2))) return 0;
    auto width = static_cast<float>(lua_tonumber(lua, lua_upvalueindex(1)));
    auto top = static_cast<float>(lua_tonumber(lua, lua_upvalueindex(3)));
    auto x = static_cast<float>(luaL_checknumber(lua, 3));
    auto y = static_cast<float>(luaL_checknumber(lua, 4));
    auto z = static_cast<float>(luaL_checknumber(lua, 5));
    auto box = Rectangle::gen({x + 0.5f * width, y + 0.5f * (top - 4.0f), z}, {width, top + 4.0f});
    return detail::luaAddObject(lua, box, 2, "box");
}

// The maker of the spans parts {box = width}: a box whose ink reaches from 4 below the
// baseline to 18 above it, or to tall, and whose line box rises 2 above the ink and falls 5
// below the baseline. The setting it gets is kept as vis.test_setting; with bad it measures
// nothing.
static int makeBox(lua_State* lua)
{
    detail::luaHandleScene(lua, 1);
    luaL_checktype(lua, 2, LUA_TTABLE);
    luaL_checktype(lua, 3, LUA_TTABLE);
    lua_getglobal(lua, "vis");
    lua_pushvalue(lua, 3);
    lua_setfield(lua, -2, "test_setting");
    lua_getfield(lua, 2, "box");
    auto width = luaL_checknumber(lua, -1);
    if (width <= 0.0) return luaL_error(lua, "box width must be positive");
    lua_getfield(lua, 2, "bad");
    if (lua_toboolean(lua, -1)) return 0;
    lua_getfield(lua, 2, "skip");
    auto skip = lua_toboolean(lua, -1);
    lua_getfield(lua, 2, "tall");
    auto top = lua_isnil(lua, -1) ? 18.0 : luaL_checknumber(lua, -1);
    lua_pushnumber(lua, width);
    lua_pushnumber(lua, top + 2.0);
    lua_pushnumber(lua, 5.0);
    lua_pushnumber(lua, top);
    lua_pushnumber(lua, -4.0);
    lua_pushnumber(lua, width);
    lua_pushboolean(lua, skip);
    lua_pushnumber(lua, top);
    lua_pushcclosure(lua, placeBox, 3);
    return 6;
}

static int openMethods(lua_State* lua, void*)
{
    lua_pushcfunction(lua, makeBox);
    detail::luaSetSpanPart(lua, "box");
    lua_pushcfunction(lua, makeBox);
    detail::luaSetSpanPart(lua, "ball");
    lua_pushcfunction(lua, addBox);
    detail::luaSetMethod(lua, detail::LuaHandle::Object, "test_box");
    lua_pushcfunction(lua, addBox);
    detail::luaSetMethod(lua, detail::LuaHandle::Scene, "test_box");
    lua_pushcfunction(lua, addParts);
    detail::luaSetMethod(lua, detail::LuaHandle::Object, "test_parts");
    lua_pushcfunction(lua, sceneOf);
    detail::luaSetMethod(lua, detail::LuaHandle::Object, "test_scene");
    lua_pushcfunction(lua, sceneOf);
    detail::luaSetMethod(lua, detail::LuaHandle::Scene, "test_scene");
    // Objects take this tex; the scene keeps the core's stand-in, which names the module.
    lua_pushcfunction(lua, texStandIn);
    detail::luaSetMethod(lua, detail::LuaHandle::Object, "tex");
    detail::luaGetMethod(lua, detail::LuaHandle::Scene, "wait");
    lua_pushinteger(lua, 0);
    lua_pushcclosure(lua, countedWait, 2);
    detail::luaSetMethod(lua, detail::LuaHandle::Scene, "wait");
    lua_getglobal(lua, "vis");
    lua_pushcfunction(lua, waits);
    lua_setfield(lua, -2, "test_waits");
    lua_pop(lua, 1);
    return 0;
}

static int openFirst(lua_State*, void* data)
{
    auto context = static_cast<HookContext*>(data);
    if (context->openOrder != 0u) return 0;
    context->openOrder = 1u;
    return 0;
}

static int openSecond(lua_State* lua, void* data)
{
    auto context = static_cast<HookContext*>(data);
    if (context->openOrder != 1u) return luaL_error(lua, "extension hook order mismatch");
    context->openOrder = 2u;
    lua_getglobal(lua, "vis");
    lua_pushcfunction(lua, addTree);
    lua_setfield(lua, -2, "test_tree");
    lua_pushcfunction(lua, firstChild);
    lua_setfield(lua, -2, "test_child");
    lua_pushcfunction(lua, addOversizedTree);
    lua_setfield(lua, -2, "test_oversized_tree");
    lua_pop(lua, 1);
    return 0;
}

static Result loadedFirst(lua_State*, Scene*, void* data) noexcept
{
    auto context = static_cast<HookContext*>(data);
    if (context->openOrder != 2u || context->loadedOrder != 0u) return Result::Unknown;
    context->loadedOrder = 1u;
    return Result::Success;
}

static Result loadedSecond(lua_State*, Scene*, void* data) noexcept
{
    auto context = static_cast<HookContext*>(data);
    if (context->loadedOrder != 1u) return Result::Unknown;
    context->loadedOrder = 2u;
    return context->failLoaded ? Result::InvalidArguments : Result::Success;
}

static detail::LuaHookSpan hooks(HookContext& context, detail::LuaHooks (&storage)[2])
{
    storage[0] = {openFirst, loadedFirst, &context};
    storage[1] = {openSecond, loadedSecond, &context};
    return {storage, 2u};
}

#ifdef VIS_EXPECT_CPU
static Scene* loadSpans(const char* source, const char* name, char* error, uint32_t errorSize)
{
    static const detail::LuaHooks boxHooks[] = {{openMethods, nullptr, nullptr}};
    Scene* scene = nullptr;
    auto status = detail::luaLoad(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error, errorSize,
                                  nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {boxHooks, 1u});
    if (status != Result::Success) return nullptr;
    return scene;
}

static bool near(float a, float b)
{
    return a - b < 1e-3f && b - a < 1e-3f;
}

static float baselineOf(Scene* scene, const char* id)
{
    auto object = scene->object(id);
    if (!object) return -1e9f;
    if (object->type() == Type::Text) return static_cast<Text*>(object)->point.y;
    // A box's rectangle covers its ink, from 4 below its baseline.
    auto box = static_cast<Rectangle*>(object);
    return box->center.y - 0.5f * box->size.y + 4.0f;
}

// A module's maker gives spans boxes, which sit on the baseline and break no line inside.
static void spanBoxes()
{
    char error[512] = {};
    static constexpr char placed[] = R"lua(
local scene = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
local row = scene:spans {
    "Ab ", {box = 40, id = "wide", color = "#ff0000"}, ", cd", {box = 10},
    id = "row", point = {0, 0}, align = {0, 0}, size = 30, color = "#00ff00",
}
local setting = vis.test_setting
if setting.size ~= 30 or setting.font or setting.role or setting.orientation ~= "billboard" then
    error("a box takes the row's setting")
end
if #row:children() ~= 4 or not rawequal(row:children()[2], scene:find("wide")) then
    error("a box is a child of the row in its place")
end
scene:spans {"a", {box = 5, size = 12, role = "h2", font = "Pretendard"}, orientation = "plane", size = 0.5}
setting = vis.test_setting
if setting.size ~= 12 or setting.role ~= "h2" or setting.font ~= "Pretendard" or setting.orientation ~= "plane" then
    error("a box takes its own size, font and role")
end
scene:spans {"a\nb", {box = 10}, id = "tall", size = 10}
return scene
)lua";
    auto scene = loadSpans(placed, "spans-boxes.lua", error, sizeof(error));
    CHECK(scene != nullptr);
    if (!scene) std::fprintf(stderr, "spans-boxes.lua: %s\n", error);
    if (scene) {
        auto first = static_cast<Text*>(scene->object("row/1"));
        auto wide = scene->object("wide");
        auto after = static_cast<Text*>(scene->object("row/3"));
        auto unnamed = scene->object("row/4");
        CHECK(first && wide && after && unnamed);
        if (first && wide && after && unnamed) {
            auto box = static_cast<Rectangle*>(wide);
            // The box starts where the text before it ends, on its baseline, and the next text
            // starts where it ends.
            CHECK(near(box->center.x, first->point.x + first->box.x + 20.0f));
            CHECK(near(box->center.y, first->point.y + 7.0f));
            CHECK(near(box->size.x, 40.0f) && near(box->size.y, 22.0f));
            CHECK(near(after->point.x, first->point.x + first->box.x + 40.0f));
            CHECK(near(after->point.y, first->point.y));
            CHECK(wide->parent() == scene->object("row") && unnamed->parent() == wide->parent());
            // A box takes its own color, or else the row's.
            CHECK(wide->style.stroke.r == 255 && wide->style.stroke.g == 0);
            CHECK(unnamed->style.stroke.r == 0 && unnamed->style.stroke.g == 255);
        }
        // Lines stack by a box's line box as by a text's: it rises 20 above the baseline.
        auto small = Text::gen(" ");
        TextExtent blank;
        if (small) small->size = 10.0f;
        CHECK(small && scene->measure(small, blank) == Result::Success && blank.ascent < 20.0f);
        delete small;
        CHECK(near(baselineOf(scene, "tall/1") - baselineOf(scene, "tall/2"), blank.descent + 20.0f));
        CHECK(near(baselineOf(scene, "tall/3"), baselineOf(scene, "tall/2")));
    }
    delete scene;

    // A box holds on to the texts beside it unless a blank or a line feed lies between them,
    // and a run wider than the wrap width takes a line of its own.
    static constexpr char wrapped[] = R"lua(
local scene = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
scene:spans {"aaa", {box = 40}, ",", id = "glued", wrap = 1}
scene:spans {"one two ", {box = 60}, ", three", id = "words", split = "words", wrap = 1}
scene:spans {"x is ", {box = 30}, " here", id = "whole", wrap = 1}
scene:spans {"a b ", {box = 500}, " c", id = "wide", split = "words", wrap = 100}
scene:spans {"ab", "cd", id = "texts", wrap = 1}
return scene
)lua";
    scene = loadSpans(wrapped, "spans-box-wrap.lua", error, sizeof(error));
    CHECK(scene != nullptr);
    if (!scene) std::fprintf(stderr, "spans-box-wrap.lua: %s\n", error);
    if (scene) {
        auto glued = baselineOf(scene, "glued/1");
        CHECK(near(baselineOf(scene, "glued/2"), glued) && near(baselineOf(scene, "glued/3"), glued));
        // one | two | box and comma | three
        auto two = baselineOf(scene, "words/2");
        auto box = baselineOf(scene, "words/3");
        CHECK(baselineOf(scene, "words/1") > two && two > box);
        CHECK(near(baselineOf(scene, "words/4"), box));
        CHECK(box > baselineOf(scene, "words/5"));
        // Blanks inside the strings let the line break on either side of the box.
        CHECK(baselineOf(scene, "whole/1") > baselineOf(scene, "whole/2"));
        CHECK(baselineOf(scene, "whole/2") > baselineOf(scene, "whole/3"));
        auto line = baselineOf(scene, "wide/1");
        CHECK(near(baselineOf(scene, "wide/2"), line));
        CHECK(line > baselineOf(scene, "wide/3") && baselineOf(scene, "wide/3") > baselineOf(scene, "wide/4"));
        // Texts alone break between any two of them, as before.
        CHECK(baselineOf(scene, "texts/1") > baselineOf(scene, "texts/2"));
    }
    delete scene;

    // With leading, a box moves its line down only as far as keeps its ink a tenth of the line
    // height clear of the ink of the lines beside it, and the other lines keep the pitch.
    static constexpr char led[] = R"lua(
local scene = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
scene:spans {"a\n", {box = 10, tall = 60}, "\nb\nc", id = "lead", size = 10, leading = 1.5}
scene:spans {"g\n", {box = 10, tall = 20}, id = "under", size = 10, leading = 1}
scene:spans {{box = 10}, "\nH", id = "over", size = 10, leading = 0.5}
scene:spans {"a\n", {box = 10}, "\nb", id = "short", size = 10, leading = 1.5}
return scene
)lua";
    scene = loadSpans(led, "spans-box-leading.lua", error, sizeof(error));
    CHECK(scene != nullptr);
    if (!scene) std::fprintf(stderr, "spans-box-leading.lua: %s\n", error);
    if (scene) {
        TextExtent blank;
        TextExtent ink[4];
        const char* texts[] = {" ", "a", "g", "H"};
        for (auto i = 0u; i < 4u; i++) {
            auto text = Text::gen(texts[i]);
            if (text) text->size = 10.0f;
            CHECK(text && scene->measure(text, i ? ink[i] : blank) == Result::Success);
            delete text;
        }
        auto lineHeight = blank.ascent + blank.descent;
        auto clear = 0.1f * lineHeight;
        // The box rises 60 above its baseline, far past the pitch below a.
        CHECK(near(baselineOf(scene, "lead/1") - baselineOf(scene, "lead/2"), 60.0f - ink[1].bottom + clear));
        CHECK(near(baselineOf(scene, "lead/2") - baselineOf(scene, "lead/3"), 1.5f * lineHeight));
        CHECK(near(baselineOf(scene, "lead/3") - baselineOf(scene, "lead/4"), 1.5f * lineHeight));
        // Below a descender, and above a capital where the box reaches 4 below its baseline.
        CHECK(near(baselineOf(scene, "under/1") - baselineOf(scene, "under/2"), 20.0f - ink[2].bottom + clear));
        CHECK(near(baselineOf(scene, "over/1") - baselineOf(scene, "over/2"), ink[3].top + 4.0f + clear));
        // A box that fits in the pitch leaves it as it is, though its line box is taller.
        CHECK(20.0f > blank.ascent);
        CHECK(near(baselineOf(scene, "short/1") - baselineOf(scene, "short/2"), 1.5f * lineHeight));
        CHECK(near(baselineOf(scene, "short/2") - baselineOf(scene, "short/3"), 1.5f * lineHeight));
    }
    delete scene;

    struct Failure
    {
        const char* source;
        const char* expected;
    };
    static constexpr Failure failures[] = {
        {"local s = vis.scene {} s:spans {{box = -1}} return s", "box width must be positive"},
        {"local s = vis.scene {} s:spans {{box = 1, ball = 1}} return s",
         "a spans part sets one box, so it cannot hold both box and ball"},
        {"local s = vis.scene {} s:spans {{box = 1, bad = true}} return s", "the box part of spans measured no box"},
        {"local s = vis.scene {} s:spans {{box = 1, skip = true}} return s",
         "a spans box must attach one object below its row"},
    };
    for (auto& failure : failures) {
        scene = loadSpans(failure.source, "spans-box-error.lua", error, sizeof(error));
        CHECK(!scene && std::strstr(error, failure.expected));
        if (scene || !std::strstr(error, failure.expected)) std::fprintf(stderr, "spans box error: %s\n", error);
        delete scene;
    }
}
#endif

int main()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 320, height = 180}
local root = vis.test_tree(scene)
local child = vis.test_child(scene, root)
scene:fade_in(root, {duration = 0.2})
scene:indicate(child, {duration = 0.2})
return scene
)lua";
    HookContext context;
    detail::LuaHooks storage[2];
    Scene* scene = nullptr;
    char error[512] = {};
    auto status = detail::luaLoad(source, sizeof(source) - 1u, "extension.lua", &scene,
                                  error, sizeof(error), nullptr, nullptr, nullptr, 0u,
                                  nullptr, nullptr, hooks(context, storage));
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    CHECK(context.openOrder == 2u);
    CHECK(context.loadedOrder == 2u);
    CHECK(scene && scene->object("extension-root"));
    CHECK(scene && scene->object("extension-body"));
    delete scene;

    context = {};
    context.failLoaded = true;
    scene = nullptr;
    status = detail::luaLoad(source, sizeof(source) - 1u, "extension-fail.lua", &scene,
                             error, sizeof(error), nullptr, nullptr, nullptr, 0u,
                             nullptr, nullptr, hooks(context, storage));
    CHECK(status == Result::InvalidArguments);
    CHECK(scene == nullptr);
    CHECK(context.loadedOrder == 2u);

    static constexpr char oversized[] = R"lua(
local scene = vis.scene {width = 320, height = 180}
vis.test_oversized_tree(scene)
return scene
)lua";
    context = {};
    status = detail::luaLoad(oversized, sizeof(oversized) - 1u, "extension-limit.lua",
                             &scene, error, sizeof(error), nullptr, nullptr, nullptr, 0u,
                             nullptr, nullptr, hooks(context, storage));
    CHECK(status == Result::ScriptError);
    CHECK(scene == nullptr);
    CHECK(std::strstr(error, "scene object limit exceeded") != nullptr);

    // A module adds methods to the handles and attaches what it builds below any of them.
    static constexpr char methods[] = R"lua(
local scene = vis.scene {width = 320, height = 180}
local holder = scene:group {id = "holder"}
local box = holder:test_box {id = "box", opacity = 0.5, layer = 2}
if #box:children() ~= 1 then error("the box keeps its rectangle") end
if not rawequal(scene:find("box"), box) then error("the box is found by its id") end
scene:test_box {id = "top"}
if holder:tex {} ~= "stand-in" then error("a module method replaces the core's") end
scene:wait(0.5)
scene:wait(0.5)
if vis.test_waits() ~= 2 then error("a wrapped method still runs") end
return scene
)lua";
    detail::LuaHooks methodHooks[] = {{openMethods, nullptr, nullptr}};
    scene = nullptr;
    status = detail::luaLoad(methods, sizeof(methods) - 1u, "extension-methods.lua", &scene, error,
                             sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {methodHooks, 1u});
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "extension-methods.lua: %s\n", error);
    if (scene) {
        auto box = scene->object("box");
        CHECK(box && box->parent() == scene->object("holder") && box->childCount() == 1u);
        CHECK(box && box->opacity == 0.5f && box->layer == 2);
        CHECK(scene->object("top") && !scene->object("top")->parent());
        CHECK(scene->duration() == 1.0f);
    }
    delete scene;

    // The style options are checked as for any factory, and the tree goes with the error.
    static constexpr char duplicate[] = R"lua(
local scene = vis.scene {}
scene:test_box {id = "box"}
scene:test_box {id = "box"}
return scene
)lua";
    scene = nullptr;
    status = detail::luaLoad(duplicate, sizeof(duplicate) - 1u, "extension-duplicate.lua", &scene, error,
                             sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {methodHooks, 1u});
    CHECK(status == Result::ScriptError && !scene);
    CHECK(std::strstr(error, "duplicate id 'box'") != nullptr);

    // A module finds the scene of any handle, and attaches parts below the handle of a whole
    // it attached, each with its own options; whatever fails goes with its handle.
    static constexpr char parts[] = R"lua(
local scene = vis.scene {}
local holder = scene:group {id = "holder"}
if not rawequal(holder:test_scene(), scene) or not rawequal(scene:test_scene(), scene) then
    error("a handle leads to its scene")
end
local whole = holder:test_parts {id = "whole", parts = {{id = "first"}, {id = "second", opacity = 0.5}}}
if #whole:children() ~= 2 or not rawequal(scene:find("second"), whole:children()[2]) then
    error("the parts attach below the whole")
end
if not rawequal(whole:test_scene(), scene) or not rawequal(scene:find("second"):test_scene(), scene) then
    error("a handle made below another leads to its scene")
end
return scene
)lua";
    scene = nullptr;
    status = detail::luaLoad(parts, sizeof(parts) - 1u, "extension-parts.lua", &scene, error, sizeof(error), nullptr,
                             nullptr, nullptr, 0u, nullptr, nullptr, {methodHooks, 1u});
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "extension-parts.lua: %s\n", error);
    if (scene) {
        auto whole = scene->object("whole");
        CHECK(whole && whole->parent() == scene->object("holder") && whole->childCount() == 2u);
        CHECK(scene->object("first") && scene->object("first")->parent() == whole);
        CHECK(scene->object("second") && scene->object("second")->opacity == 0.5f);
    }
    delete scene;
    static constexpr char partsDuplicate[] = R"lua(
local scene = vis.scene {}
scene:group {}:test_parts {parts = {{id = "same"}, {id = "same"}}}
return scene
)lua";
    scene = nullptr;
    status = detail::luaLoad(partsDuplicate, sizeof(partsDuplicate) - 1u, "extension-parts-duplicate.lua", &scene,
                             error, sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {methodHooks, 1u});
    CHECK(status == Result::ScriptError && !scene);
    // The part attaches through a call of the module's, and its error names the script's line.
    CHECK(std::strstr(error, "extension-parts-duplicate.lua:3: duplicate id 'same'") != nullptr);

    // A wrapped method fails as the core's does: at the script's line, naming the method.
    static constexpr char wrappedWait[] = "local scene = vis.scene {}\nscene:wait('long')\nreturn scene";
    scene = nullptr;
    status = detail::luaLoad(wrappedWait, sizeof(wrappedWait) - 1u, "extension-wait.lua", &scene, error, sizeof(error),
                             nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {methodHooks, 1u});
    CHECK(status == Result::ScriptError && !scene);
    CHECK(std::strstr(error, "extension-wait.lua:2: bad argument #1 to 'wait' (number expected, got string)")
          != nullptr);

    // Without the module, its factory names the build option that adds it.
    static constexpr char absent[] = "local scene = vis.scene {} scene:tex {tex = 'x'} return scene";
    status = detail::luaLoad(absent, sizeof(absent) - 1u, "extension-absent.lua", &scene, error, sizeof(error),
                             nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {methodHooks, 1u});
    CHECK(status == Result::ScriptError && !scene);
    CHECK(std::strstr(error, "tex needs the tex module: build viscript with -Dmodules=tex") != nullptr);
    static constexpr char absentGroup[] = "local scene = vis.scene {} scene:group {}:tex {} return scene";
    status = detail::luaLoad(absentGroup, sizeof(absentGroup) - 1u, "extension-absent-group.lua", &scene, error,
                             sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {});
    CHECK(status == Result::ScriptError && !scene);
    CHECK(std::strstr(error, "-Dmodules=tex") != nullptr);

    // A surface copies its points, and the colors a function gives them, out of userdata the
    // binding keeps on the stack until then. With the collector running at every chance, one
    // popped before the copy is freed and scribbled first, as the points were before a401f71:
    // the first surface pushes its default mode and adds its handle after the pop, and the
    // second calls back into Lua for each point.
    static constexpr char surfaces[] = R"lua(
local scene = vis.scene {width = 320, height = 180}
scene:surface {id = "sheet", size = {3, 2},
               points = {{0, 0, 0}, {1, 0, 0.5}, {2, 0, 1}, {0, 1, 1.5}, {1, 1, 2}, {2, 1, 2.5}}}
scene:surface {id = "tinted", size = {2, 2}, mode = "solid_mesh",
               points = {{0, 0, -1}, {1, 0, -0.5}, {0, 1, 0.5}, {1, 1, 1}},
               colors = function(x, y, z) if z > 0 then return "#ff0000" end return "#0000ff" end}
return scene
)lua";
    detail::LuaHooks collecting[] = {collectingHooks()};
    scene = nullptr;
    status = detail::luaLoad(surfaces, sizeof(surfaces) - 1u, "extension-surfaces.lua", &scene, error,
                             sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {collecting, 1u});
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "extension-surfaces.lua: %s\n", error);
    if (scene) {
        auto surface = [&](const char* id) {
            auto object = scene->object(id);
            return object && object->type() == Type::SurfaceMesh ? static_cast<SurfaceMesh*>(object) : nullptr;
        };
        auto sheet = surface("sheet");
        CHECK(sheet && sheet->columns() == 3u && sheet->rows() == 2u);
        for (auto i = 0u; sheet && i < 6u; i++) {
            auto& point = sheet->points()[i];
            CHECK(point.x == float(i % 3u) && point.y == float(i / 3u) && point.z == 0.5f * float(i));
        }
        auto tinted = surface("tinted");
        CHECK(tinted && tinted->points()[0].z == -1.0f && tinted->points()[3].z == 1.0f);
        CHECK(tinted && tinted->colors());
        for (auto i = 0u; tinted && tinted->colors() && i < 4u; i++) {
            auto& color = tinted->colors()[i];
            CHECK(color.r == (i < 2u ? 0u : 255u) && color.g == 0u && color.b == (i < 2u ? 255u : 0u));
        }
    }
    delete scene;

    // An id given as a number is the string Lua makes of it, which only the stack holds; it
    // stays there until the object, or the scene making a copy, copies it.
    static constexpr char numericIds[] = R"lua(
local scene = vis.scene {}
local dot = scene:circle {id = 7, radius = 1}
scene:clone(dot, {id = 4242, shift = {2, 0}})
return scene
)lua";
    scene = nullptr;
    status = detail::luaLoad(numericIds, sizeof(numericIds) - 1u, "extension-numeric-ids.lua", &scene, error,
                             sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {collecting, 1u});
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "extension-numeric-ids.lua: %s\n", error);
    if (scene) {
        auto dot = scene->object("7");
        auto copy = scene->object("4242");
        CHECK(dot && dot->type() == Type::Circle);
        CHECK(copy && copy != dot && copy->type() == Type::Circle);
    }
    delete scene;

    // A theme keeps the names of its fonts until the scene copies them, after the scene's handle
    // is allocated, and only a string the options hold lasts that long, so a number is refused.
    static constexpr char themeFont[] = R"lua(
return vis.scene {theme = {text = {h1 = {font = "Serif Display", size = 40}}}}
)lua";
    scene = nullptr;
    status = detail::luaLoad(themeFont, sizeof(themeFont) - 1u, "extension-theme-font.lua", &scene, error,
                             sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {collecting, 1u});
    CHECK(status == Result::Success);
    if (status != Result::Success) std::fprintf(stderr, "extension-theme-font.lua: %s\n", error);
    CHECK(scene && std::strcmp(scene->theme().h1.font, "Serif Display") == 0 && scene->theme().h1.size == 40.0f);
    delete scene;
    static constexpr char numericFont[] = "return vis.scene {theme = {text = {h1 = {font = 12345}}}}";
    scene = nullptr;
    status = detail::luaLoad(numericFont, sizeof(numericFont) - 1u, "extension-numeric-font.lua", &scene, error,
                             sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {collecting, 1u});
    CHECK(status == Result::ScriptError && !scene);
    CHECK(std::strstr(error, "theme font must be a string") != nullptr);
#ifdef VIS_EXPECT_CPU
    // Spans measure their texts in the fonts renderers load.
    auto renderer = SwRenderer::gen();
    CHECK(renderer && renderer->font(VIS_TEST_FONT) == Result::Success);
#ifdef VIS_TEST_MONO_FONT
    CHECK(renderer && renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
#endif
    spanBoxes();
    delete renderer;
#endif
    return failures ? 1 : 0;
}
