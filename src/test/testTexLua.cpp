#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "vis.h"
#include "vis_tex.h"
#include "visLuaHost.h"
#include "testCheck.h"

using namespace vis;

static Scene* _load(const char* source, const char* name)
{
    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = tex::Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error, sizeof(error));
    CHECK(status == Result::Success && scene);
    if (status != Result::Success) {
        std::fprintf(stderr, "%s: %s\n", name, error);
        delete scene;
        return nullptr;
    }
    return scene;
}

static void _error(const char* source, const char* name, const char* expected)
{
    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = tex::Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error, sizeof(error));
    CHECK(status == Result::ScriptError && !scene);
    if (!std::strstr(error, expected)) {
        std::fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", name, expected, error);
        failures++;
    }
    delete scene;
}

static const Text* _text(const Scene* scene, const char* id)
{
    auto object = scene->object(id);
    return object && object->type() == Type::Text ? static_cast<const Text*>(object) : nullptr;
}

// One source: a typeset group of glyphs named after it, set in the role's color, its box
// pinned to the point.
static void _single(Renderer* renderer)
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {}
local eq = scene:tex {tex = "a^2+b^2=c^2", point = {1, 0.5}, role = "h2", id = "eq"}
if #eq:children() ~= 8 then error("one piece per glyph") end
if not rawequal(eq:part("c"), scene:find("eq/7")) or not rawequal(eq:part("2", 3), scene:find("eq/8")) then
    error("part finds a piece by its TeX")
end
local group = scene:group {id = "holder"}
group:tex {tex = "x", id = "inner", point = {-2, 0}}
return scene
)lua";
    auto scene = _load(source, "tex-single.lua");
    if (!scene) return;
    auto root = scene->object("eq");
    CHECK(root && root->type() == Type::Group && static_cast<const Group*>(root)->typeset);
    if (root) CHECK(same(root->style.fill, scene->theme().h2.color));
    auto a = _text(scene, "eq/1");
    auto two = _text(scene, "eq/2");
    CHECK(a && std::strcmp(a->text(), "a") == 0 && two && std::strcmp(two->text(), "2") == 0);
    // Letters are math italic, leaning where the font has none; scripts are smaller.
    if (a && two) {
        CHECK(a->slant > 0.1f && two->slant == 0.0f);
        CHECK(two->size < a->size && a->role == TextRole::H2);
    }
    Bounds bounds;
    CHECK(root && root->bounds(bounds));
    auto center = bounds.center();
    CHECK(std::fabs(center.x - 1.0f) < 0.05f && std::fabs(center.y - 0.5f) < 0.05f);
    CHECK(bounds.size().x > 1.0f && bounds.size().y > 0.2f);
    auto inner = scene->object("inner");
    CHECK(inner && inner->parent() == scene->object("holder") && scene->object("inner/1"));

    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(changed(surface, scene->config().background) > 500u);
    CHECK(Saver::png(surface, "test-tex-lua.png") == Result::Success);
    delete scene;
}

// A list of parts: a group per part, named and styled on its own, whose glyphs part() finds
// in it or in the whole; presets play on the formula and on its parts.
static void _parts()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {}
local eq = scene:tex {
    tex = {"a^2", "+", {"b^2", color = "#ff0000"}, "=", {"c^2", id = "hyp", fill = "#00ff00", opacity = 0.5}},
    id = "eq",
}
local hyp = eq:part("c^2")
if not rawequal(hyp, scene:find("hyp")) or #eq:children() ~= 5 then error("one group per part") end
if not rawequal(hyp:part("2"), scene:find("hyp/2")) or not rawequal(eq:part("2", 3), scene:find("hyp/2")) then
    error("part finds glyphs in a part and in the whole")
end
if not rawequal(eq:part(" b ^ 2 "), scene:find("eq/3")) then error("part reads TeX without blanks") end
scene:write(eq, 1)
scene:play({target = eq:part("+"), fill = "#0000ff"}, 0.5)
scene:indicate(eq:part("="), {color = "#ff00ff", duration = 0.5})
return scene
)lua";
    auto scene = _load(source, "tex-parts.lua");
    if (!scene) return;
    auto root = scene->object("eq");
    CHECK(root && root->childCount() == 5u);
    for (auto i = 0u; root && i < root->childCount(); i++) CHECK(root->childAt(i)->type() == Type::Group);
    CHECK(scene->object("eq/1") && scene->object("eq/2") && scene->object("eq/4"));
    auto b = scene->object("eq/3");
    auto hyp = scene->object("hyp");
    CHECK(b && same(b->style.fill, Color{255, 0, 0, 255}));
    CHECK(hyp && same(hyp->style.fill, Color{0, 255, 0, 255}) && hyp->opacity == 0.5f);
    auto plus = scene->object("eq/2");
    CHECK(plus && same(plus->style.fill, Color{0, 0, 255, 255}));
    auto c = _text(scene, "hyp/1");
    CHECK(c && std::strcmp(c->text(), "c") == 0 && c->parent() == hyp);
    CHECK(_text(scene, "eq/1/1") && _text(scene, "eq/3/2") && !scene->object("eq/1/3"));
    CHECK(std::fabs(scene->duration() - 2.0f) < 1.0e-4f);
    delete scene;
}

// Bars, roots, stretched delimiters and accents are shapes, keyed by the command that draws
// them.
static void _shapes()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {}
local f = scene:tex {tex = "\\frac{1}{\\sqrt{x}} \\left( \\frac{a}{b} \\right) \\hat{y}", id = "f"}
if not rawequal(f:part("\\frac"), scene:find("f/1")) or not rawequal(f:part("\\sqrt", 2), scene:find("f/4")) then
    error("part finds shapes by their commands")
end
return scene
)lua";
    auto scene = _load(source, "tex-shapes.lua");
    if (!scene) return;
    auto bar = scene->object("f/1");
    auto radical = scene->object("f/3");
    auto vinculum = scene->object("f/4");
    auto open = scene->object("f/6");
    auto hat = scene->object("f/11");
    CHECK(bar && bar->type() == Type::Rectangle && vinculum && vinculum->type() == Type::Rectangle);
    CHECK(radical && radical->type() == Type::Path && open && open->type() == Type::Path);
    CHECK(hat && hat->type() == Type::Path);
    // Shapes are filled by the group, without strokes of their own.
    if (bar) CHECK(bar->style.stroke.a == 0u);
    delete scene;
}

static void _errors()
{
    _error("local scene = vis.scene {} scene:tex {tex = 'a+\\\\foo'} return scene", "tex-command.lua",
           "tex: unknown command '\\foo' at 3");
    _error("local scene = vis.scene {} scene:tex {tex = {'a', '\\\\frac{'}} return scene", "tex-part.lua",
           "tex: part 2: ");
    _error("local scene = vis.scene {} scene:tex {tex = 'x', stroke = 'red'} return scene", "tex-stroke.lua",
           "tex draws glyphs and filled shapes, so stroke is unsupported");
    _error("local scene = vis.scene {} scene:tex {tex = 'x', colour = 'red'} return scene", "tex-option.lua",
           "unknown option 'colour'");
    _error("local scene = vis.scene {} scene:tex {} return scene", "tex-missing.lua",
           "tex needs its TeX: a string, or a list of parts");
    _error("local scene = vis.scene {} scene:tex {tex = 'x', size = -1} return scene", "tex-size.lua",
           "size must be positive");
    _error("local scene = vis.scene {} scene:tex {tex = 'x', id = 'x'} scene:tex {tex = 'y', id = 'x'} return scene",
           "tex-duplicate.lua", "duplicate id 'x'");
    _error("local scene = vis.scene {} scene:tex {tex = 'a+b'}:part('c') return scene", "tex-absent.lua",
           "tex draws no 'c'; it draws 'a', '+', 'b'");
    _error("local scene = vis.scene {} scene:tex {tex = {'a', '+b'}}:part('c') return scene", "tex-absent-part.lua",
           "tex has no part 'c'; its parts are 'a', '+b'");
    _error("local scene = vis.scene {} scene:tex {tex = 'x^2+y^2'}:part('2', 3) return scene", "tex-nth.lua",
           "tex has only 2 of '2'");
    _error("local scene = vis.scene {} scene:group {}:part('x') return scene", "tex-not-formula.lua",
           "part looks in a tex formula or in one of its parts");

    // The core's match_transform runs below the module's, and a part attaches below its formula
    // through a call of the module's own; their errors still name the script's line, and the
    // method in a bad argument, as the core's method alone does.
    _error(R"lua(
local scene = vis.scene {}
local a = scene:tex {tex = "a+b"}
scene:wait(1)
local b = scene:tex {tex = "b+a", point = {0, -1}}
scene:wait(1)
scene:match_transform(a, b, 1)
return scene
)lua",
           "tex-match-late.lua", "tex-match-late.lua:7: could not match transform: add the object to match into");
    _error("local scene = vis.scene {}\nlocal a = scene:circle {}\nscene:match_transform(a, a, 'long')\nreturn scene",
           "tex-match-duration.lua",
           "tex-match-duration.lua:3: bad argument #3 to 'match_transform' (number expected, got string)");
    _error("local scene = vis.scene {}\nscene:circle {id = 'taken'}\nscene:tex {tex = {'a', {'b', id = 'taken'}}}\n"
           "return scene",
           "tex-part-taken.lua", "tex-part-taken.lua:3: duplicate id 'taken'");
}

// Empty groups set through the binding as through the core, alone, together, as parts and
// inline: an empty fraction draws no bar and an empty root keeps its sign.
static void _empties()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {}
local sources = {"\\frac{}{}", "{}", "\\sqrt{}", "\\hat{}", "x + {} + \\frac{}{} + \\sqrt{} + \\hat{}"}
for i, tex in ipairs(sources) do scene:tex {tex = tex, id = "empty" .. i, point = {0, i}} end
if #scene:find("empty1"):children() ~= 0 or #scene:find("empty3"):children() ~= 1 then
    error("an empty fraction draws no bar and an empty root its sign")
end
scene:tex {tex = {"\\frac{}{}", "\\sqrt{}"}, point = {0, -1}}
scene:spans {"a ", {tex = "\\frac{}{} + \\sqrt{}"}, " b", point = {0, -2}}
return scene
)lua";
    auto scene = _load(source, "tex-empty.lua");
    delete scene;
}

// More parts than the bytes a formula holds stop the script with the limit before any part is
// read.
static void _partLimit()
{
    _error("local scene = vis.scene {}\nlocal parts = {}\nfor i = 1, 16385 do parts[i] = 'x' end\n"
           "scene:tex {tex = parts}\nreturn scene",
           "tex-part-limit.lua", "tex-part-limit.lua:4: tex: a formula holds at most 16384 bytes");
}

// The bounds of a piece when the scene shows time, in pixels.
static Vec2 _center(Renderer* renderer, const Scene* scene, const char* id, float time)
{
    BBox box;
    auto object = scene->object(id);
    CHECK(object && renderer->bounds(scene, object, time, box) == Result::Success);
    return box.center();
}

static bool _at(Vec2 lhs, Vec2 rhs)
{
    return std::fabs(lhs.x - rhs.x) < 1.0f && std::fabs(lhs.y - rhs.y) < 1.0f;
}

// match_transform pairs the pieces of formulas by their TeX: a part that reads the same as a
// whole, then glyphs in the order both keep, then wherever they stand; a pair of groups pairs
// the pieces below them.
static void _matching(Renderer* renderer)
{
    static constexpr char whole[] = R"lua(
local scene = vis.scene {}
local a = scene:tex {tex = "a^2+b^2=c^2", id = "a"}
scene:wait(0.5)
local b = scene:tex {tex = "a^2=c^2-b^2", id = "b", point = {0, -1}}
scene:match_transform(a, b, 1, "smooth", {arc = 0.5})
return scene
)lua";
    auto scene = _load(whole, "tex-match.lua");
    if (scene) {
        // The 2 of c^2 starts where the 2 of c^2 stood, not where the first free 2 did.
        CHECK(_at(_center(renderer, scene, "b/5", 0.5f), _center(renderer, scene, "a/8", 0.4f)));
        CHECK(!_at(_center(renderer, scene, "b/5", 0.5f), _center(renderer, scene, "a/5", 0.4f)));
        CHECK(_at(_center(renderer, scene, "b/7", 0.5f), _center(renderer, scene, "a/4", 0.4f)));
        CHECK(std::fabs(scene->duration() - 1.5f) < 1.0e-4f);
    }
    delete scene;

    static constexpr char parts[] = R"lua(
local scene = vis.scene {}
local a = scene:tex {tex = {"a^2", "+", "b^2", "=", "c^2"}, id = "a"}
local x = scene:tex {tex = {"x^2", "+", "y"}, id = "x", point = {0, 1}}
scene:wait(0.5)
local b = scene:tex {tex = {"c^2", "=", "a^2", "+", "b^2"}, id = "b", point = {0, -1}}
scene:match_transform(a, b, 1)
local y = scene:tex {tex = {"y", "+", "z^2"}, id = "y", point = {0, 2}}
scene:match_transform(x, y, 1, "smooth", {pairs = {{x:part("x^2"), y:part("z^2")}}})
return scene
)lua";
    scene = _load(parts, "tex-match-parts.lua");
    if (scene) {
        CHECK(_at(_center(renderer, scene, "b/1/2", 0.5f), _center(renderer, scene, "a/5/2", 0.4f)));
        CHECK(_at(_center(renderer, scene, "b/3/2", 0.5f), _center(renderer, scene, "a/1/2", 0.4f)));
        // A pair of parts: the 2s pair by their TeX, the letters in order.
        CHECK(_at(_center(renderer, scene, "y/3/2", 1.5f), _center(renderer, scene, "x/1/2", 1.4f)));
        CHECK(!_at(_center(renderer, scene, "y/3/1", 2.0f), _center(renderer, scene, "y/3/1", 2.6f)));
        CHECK(_at(_center(renderer, scene, "y/1/1", 1.5f), _center(renderer, scene, "x/3/1", 1.4f)));
    }
    delete scene;
}

// With strings = false a match of formulas pairs only the pieces given, as it pairs texts: the
// copy of b^2 - 4ac flies into the discriminant, while the rest of the target fades in where it
// stands and the rest of the copy fades out where it stands. A pair of parts still pairs the
// pieces below them.
static void _choosing(Renderer* renderer)
{
    static constexpr char given[] = R"lua(
local s = vis.scene {width = 800, height = 400, camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 400}}
local eq = s:tex {tex = [[x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}]], point = {0, 60}, size = 40, id = "eq"}
s:wait(0.5)
local delta = s:tex {tex = [[\Delta = b^2 - 4ac]], point = {0, -120}, size = 40, id = "delta"}
local pairs = {}
for i = 9, 14 do pairs[#pairs + 1] = {s:find("eq/" .. i), s:find("delta/" .. (i - 6))} end
s:match_transform(eq, delta, 1, "smooth", {copy = true, strings = strings, pairs = pairs})
return s
)lua";
    char source[1024];
    std::snprintf(source, sizeof(source), "local strings = %s%s", "false", given);
    auto scene = _load(source, "tex-match-given.lua");
    if (scene) {
        // b and c of the discriminant start where those of the formula stand, which stay.
        CHECK(_at(_center(renderer, scene, "delta/3", 0.5f), _center(renderer, scene, "eq/9", 0.4f)));
        CHECK(_at(_center(renderer, scene, "delta/8", 0.5f), _center(renderer, scene, "eq/14", 0.4f)));
        CHECK(_at(_center(renderer, scene, "eq/9", 1.0f), _center(renderer, scene, "eq/9", 0.4f)));
        // The delta and the equals sign, which the TeX would pair, stay where they arrive.
        CHECK(_at(_center(renderer, scene, "delta/1", 1.0f), _center(renderer, scene, "delta/1", 1.5f)));
        CHECK(_at(_center(renderer, scene, "delta/2", 1.0f), _center(renderer, scene, "delta/2", 1.5f)));
        CHECK(std::fabs(scene->duration() - 1.5f) < 1.0e-4f);
    }
    delete scene;
    std::snprintf(source, sizeof(source), "local strings = %s%s", "true", given);
    scene = _load(source, "tex-match-strings.lua");
    if (scene) CHECK(_at(_center(renderer, scene, "delta/2", 0.5f), _center(renderer, scene, "eq/2", 0.4f)));
    delete scene;

    static constexpr char parts[] = R"lua(
local scene = vis.scene {}
local x = scene:tex {tex = {"x^2", "+", "y"}, id = "x", point = {0, 1}}
scene:wait(0.5)
local y = scene:tex {tex = {"y", "+", "z^2"}, id = "y", point = {0, 2}}
scene:match_transform(x, y, 1, "smooth", {strings = false, pairs = {{x:part("x^2"), y:part("z^2")}}})
return scene
)lua";
    scene = _load(parts, "tex-match-given-parts.lua");
    if (scene) {
        CHECK(_at(_center(renderer, scene, "y/3/2", 0.5f), _center(renderer, scene, "x/1/2", 0.4f)));
        CHECK(!_at(_center(renderer, scene, "y/3/1", 1.0f), _center(renderer, scene, "y/3/1", 1.5f)));
        CHECK(_at(_center(renderer, scene, "y/1/1", 1.0f), _center(renderer, scene, "y/1/1", 1.5f)));
        CHECK(_at(_center(renderer, scene, "y/2/1", 1.0f), _center(renderer, scene, "y/2/1", 1.5f)));
    }
    delete scene;
}

// A rectangle around glyphs sits its padding off their ink on every side, and bounds report
// the ink, leaning letters included, as painted.
static void _framing(Renderer* renderer)
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 800, height = 400, camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 400}}
local eq = s:tex {tex = [[x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}]], point = {0, 60}, size = 40, id = "eq"}
s:rectangle {id = "box", around = {eq:part("2", 2), eq:part("a", 2)}, padding = 6, fill = "#00000000"}
local b = s:bounds(eq:part("b"))
s:rectangle {id = "under", center = {(b.left + b.right) / 2, b.bottom - 4}, size = {b.width, 2}}
return s
)lua";
    auto scene = _load(source, "tex-framing.lua");
    if (!scene) return;
    BBox two;
    BBox a;
    BBox b;
    CHECK(renderer->bounds(scene, scene->object("eq/15"), 0.0f, two) == Result::Success);
    CHECK(renderer->bounds(scene, scene->object("eq/16"), 0.0f, a) == Result::Success);
    CHECK(renderer->bounds(scene, scene->object("eq/5"), 0.0f, b) == Result::Success);
    auto box = static_cast<const Rectangle*>(scene->object("box"));
    auto under = static_cast<const Rectangle*>(scene->object("under"));
    auto glyphs = _text(scene, "eq/15") && _text(scene, "eq/16") && _text(scene, "eq/5");
    CHECK(box && under && glyphs);
    if (glyphs) {
        CHECK(std::strcmp(_text(scene, "eq/15")->text(), "2") == 0 && std::strcmp(_text(scene, "eq/16")->text(), "a") == 0);
        CHECK(std::strcmp(_text(scene, "eq/5")->text(), "b") == 0 && _text(scene, "eq/5")->slant != 0.0f);
    }
    if (box && under) {
        // One world unit is one pixel, from the middle of the frame, y down.
        auto left = box->center.x - 0.5f * box->size.x + 400.0f;
        auto right = box->center.x + 0.5f * box->size.x + 400.0f;
        auto top = 200.0f - box->center.y - 0.5f * box->size.y;
        auto bottom = 200.0f - box->center.y + 0.5f * box->size.y;
        CHECK(std::fabs(two.x - left - 6.0f) < 0.75f && std::fabs(right - a.x - a.width - 6.0f) < 0.75f);
        CHECK(std::fabs(two.y - top - 6.0f) < 0.75f && std::fabs(bottom - a.y - a.height - 6.0f) < 0.75f);
        CHECK(std::fabs(under->center.x + 400.0f - b.center().x) < 0.75f && std::fabs(under->size.x - b.width) < 0.75f);
    }
    delete scene;
}

// A brace under a term of a formula reaches from the ink of its first glyph to the ink of its
// last, offset below the lowest, and its label's ink stands gap below its tip.
static void _bracing(Renderer* renderer)
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 800, height = 400, camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 400}}
s:tex {tex = [[x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}]], point = {0, 60}, size = 40, id = "eq"}
local term = {}
for i = 9, 14 do term[#term + 1] = s:find("eq/" .. i) end
s:brace {around = term, offset = 8, gap = 3, label = "discriminant", id = "brace"}
return s
)lua";
    auto scene = _load(source, "tex-brace.lua");
    if (!scene) return;
    // b, 2, minus, 4, a and c of b^2 - 4ac, the b and a leaning.
    BBox ink{};
    auto first = true;
    for (auto i = 9; i <= 14; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "eq/%d", i);
        BBox glyph;
        CHECK(_text(scene, id) && renderer->bounds(scene, scene->object(id), 0.0f, glyph) == Result::Success);
        if (first) ink = glyph;
        auto right = std::max(ink.x + ink.width, glyph.x + glyph.width);
        auto bottom = std::max(ink.y + ink.height, glyph.y + glyph.height);
        ink.x = std::min(ink.x, glyph.x);
        ink.y = std::min(ink.y, glyph.y);
        ink.width = right - ink.x;
        ink.height = bottom - ink.y;
        first = false;
    }
    BBox brace;
    BBox label;
    CHECK(renderer->bounds(scene, scene->object("brace"), 0.0f, brace) == Result::Success);
    CHECK(renderer->bounds(scene, scene->object("brace/label"), 0.0f, label) == Result::Success);
    CHECK(std::fabs(brace.x - ink.x) < 0.75f && std::fabs(brace.x + brace.width - ink.x - ink.width) < 0.75f);
    CHECK(std::fabs(brace.y - ink.y - ink.height - 8.0f) < 0.75f);
    CHECK(std::fabs(label.y - brace.y - brace.height - 3.0f) < 0.75f);
    CHECK(std::fabs(label.x + 0.5f * label.width - brace.x - 0.5f * brace.width) < 0.75f);
    delete scene;
}

// Where the baseline of a glyph on a formula's baseline lies: the glyph pins its box by the
// align that pins the line box of its font, so its point lies the font's ascent less align
// lines above the baseline.
static float _glyphBaseline(const Scene* scene, const char* id)
{
    auto glyph = _text(scene, id);
    TextExtent extent;
    CHECK(glyph && const_cast<Scene*>(scene)->measure(glyph, extent) == Result::Success);
    if (!glyph) return 0.0f;
    return glyph->point.y - extent.ascent + glyph->align.y * extent.height;
}

static float _baseline(const Scene* scene, const char* id)
{
    auto text = _text(scene, id);
    CHECK(text != nullptr);
    return text ? text->point.y : 0.0f;
}

static bool _near(float lhs, float rhs, float tolerance = 0.01f)
{
    return std::fabs(lhs - rhs) < tolerance;
}

// spans{..., {tex = "..."}, ...}: a formula set inline in text style, in the row's size and
// color, as one box on the baseline that lines never break inside.
static void _inline(Renderer* renderer)
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
local row = s:spans {"if ", {tex = "a^2+b^2=c^2", id = "eq"}, ", then", split = "words", size = 30, id = "row"}
local eq = s:find("eq")
if not rawequal(row:children()[2], eq) or #row:children() ~= 4 then error("a formula is a child of its row") end
if not rawequal(eq:part("c"), s:find("eq/7")) then error("part finds a glyph of an inline formula") end
s:spans {"so ", {tex = {"x", {"+", color = "#ff0000", id = "plus"}, "y"}}, split = "words", size = 30, id = "list",
         point = {0, 100}, color = "#336699"}
if not rawequal(s:find("list/2"):part("+"), s:find("plus")) then error("part finds a part of an inline formula") end
s:spans {"x\n", {tex = "x", id = "alone"}, "\ny", size = 30, id = "lines", point = {-400, 0}}
s:spans {"a b ", {tex = "x_1+x_2+x_3+x_4+x_5", id = "long"}, " c", split = "words", wrap = 60, size = 30,
         id = "wide", point = {400, 0}}
s:spans {{tex = "\\sum_{i=1}^n i", id = "inline"}, {tex = "\\sum_{i=1}^n i", id = "display", display = true},
         size = 30, point = {0, -200}}
s:spans {"x", {tex = "n", id = "raised", rise = 0.5}, size = 30, id = "rise", point = {0, -300}}
s:fill(eq, "#ffffff", 0.5)
local target = s:tex {tex = "c^2=a^2+b^2", point = {0, 250}, id = "target"}
s:match_transform(eq, target, 1)
return s
)lua";
    auto scene = _load(source, "tex-inline.lua");
    if (!scene) return;
    auto eq = scene->object("eq");
    CHECK(eq && eq->type() == Type::Group && eq->parent() == scene->object("row"));
    // The formula takes the row's color, and its group's fill colors it as fill plays it.
    CHECK(eq && static_cast<const Group*>(eq)->typeset && same(eq->style.fill, Color{255, 255, 255, 255}));
    auto sum = scene->object("list/2");
    CHECK(sum && same(sum->style.fill, Color{0x33, 0x66, 0x99, 255}));
    // Inline formulas take text style in the row's size, as scene:tex does with display = false.
    auto a = _text(scene, "eq/1");
    CHECK(a && a->size == 30.0f);
    // Every glyph on the formula's baseline sits on the row's.
    auto baseline = _baseline(scene, "row/1");
    for (auto id : {"eq/1", "eq/3", "eq/4", "eq/6", "eq/7"}) CHECK(_near(_glyphBaseline(scene, id), baseline));
    CHECK(_near(_baseline(scene, "row/3"), baseline));
    // The formula starts a blank after the word before it, and the comma holds on to its end.
    auto blank = Text::gen(" ");
    TextExtent space;
    if (blank) blank->size = 30.0f;
    CHECK(blank && scene->measure(blank, space) == Result::Success);
    delete blank;
    auto word = _text(scene, "row/1");
    if (word && a) CHECK(_near(a->point.x, word->point.x + word->box.x + space.width, 0.5f));
    auto two = _text(scene, "eq/8");
    auto comma = _text(scene, "row/3");
    // TeX leaves a script space after the superscript inside the formula's box. A glyph's box
    // frames its ink, so the advance of the 2 comes from measuring it.
    TextExtent advance;
    CHECK(two && scene->measure(two, advance) == Result::Success);
    if (two && comma) {
        CHECK(comma->point.x > two->point.x + advance.width && comma->point.x < two->point.x + advance.width + 3.0f);
    }
    auto plus = scene->object("plus");
    CHECK(plus && plus->parent() == scene->object("list/2") && same(plus->style.fill, Color{255, 0, 0, 255}));

    // A formula alone on a line keeps the line as tall as a line of text.
    auto first = _baseline(scene, "lines/1");
    auto second = _glyphBaseline(scene, "alone/1");
    auto third = _baseline(scene, "lines/3");
    CHECK(_near(first - second, space.ascent + space.descent) && _near(second - third, space.ascent + space.descent));

    // Lines break around a formula wider than the wrap width, which takes a line of its own.
    auto words = _baseline(scene, "wide/1");
    CHECK(_near(_baseline(scene, "wide/2"), words));
    auto formula = _glyphBaseline(scene, "long/1");
    CHECK(words - formula > 1.0f && formula - _baseline(scene, "wide/4") > 1.0f);

    // Display style sets the sum larger, with its limits above and below.
    Bounds small;
    Bounds large;
    CHECK(scene->object("inline")->bounds(small) && scene->object("display")->bounds(large));
    CHECK(large.size().y > small.size().y + 10.0f);
    // rise raises a formula as it raises a text.
    CHECK(_near(_glyphBaseline(scene, "raised/1") - _baseline(scene, "rise/1"), 0.5f * (space.ascent + space.descent)));
    // match_transform pairs its glyphs by their TeX: the c of the target starts where the c stood.
    CHECK(_at(_center(renderer, scene, "target/1", 0.5f), _center(renderer, scene, "eq/7", 0.4f)));
    CHECK(std::fabs(scene->duration() - 1.5f) < 1.0e-4f);
    delete scene;

    _error("local s = vis.scene {} s:spans {{tex = 'x', text = 'y'}} return s", "tex-span-text.lua",
           "a spans part sets text or tex, not both");
    _error("local s = vis.scene {} s:spans {{'y', tex = 'x'}} return s", "tex-span-first.lua",
           "a spans part sets text or tex, not both");
    _error("local s = vis.scene {} s:spans {{tex = 'x', stroke = 'red'}} return s", "tex-span-stroke.lua",
           "tex draws glyphs and filled shapes, so stroke is unsupported");
    _error("local s = vis.scene {} s:spans {{tex = 'x', colour = 'red'}} return s", "tex-span-option.lua",
           "unknown option 'colour'");
    _error("local s = vis.scene {} s:spans {{tex = 'x', display = 1}} return s", "tex-span-display.lua",
           "display must be a boolean");
    _error("local s = vis.scene {} s:spans {{tex = 'a+\\\\foo'}} return s", "tex-span-command.lua",
           "tex: unknown command '\\foo' at 3");
    _error("local s = vis.scene {} s:spans {{tex = 3}} return s", "tex-span-missing.lua",
           "tex needs its TeX: a string, or a list of parts");
    _error("local s = vis.scene {} s:spans {{tex = 'x', size = -1}} return s", "tex-span-size.lua",
           "size must be positive");
    _error("local s = vis.scene {} s:spans {'a', {tex = 'x'}, id = 'r'} s:spans {{tex = 'y', id = 'r/2'}} return s",
           "tex-span-duplicate.lua", "duplicate id 'r/2'");
}

// spans and matrix call the tex part's maker and place from C, and its errors still name the
// line of the script.
static void _spanLines()
{
    _error("local s = vis.scene {}\ns:spans {'a ', {tex = 'a+\\\\foo'}}\nreturn s", "tex-span-line.lua",
           "tex-span-line.lua:2: tex: unknown command '\\foo' at 3");
    _error("local s = vis.scene {}\ns:circle {id = 'taken'}\ns:spans {'a ', {tex = {'x', {'y', id = 'taken'}}}}\n"
           "return s",
           "tex-span-place.lua", "tex-span-place.lua:3: duplicate id 'taken'");
    _error("local s = vis.scene {}\ns:matrix {rows = {{'\\\\frac{'}}}\nreturn s", "tex-matrix-line.lua",
           "tex-matrix-line.lua:2: tex: missing '}'");
}

// An inline formula applies its resolved row or part layer to every painted piece.
static void _inlineLayers(Renderer* renderer)
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 640, height = 360, background = "#000000",
    camera = {mode = "fixed", view = "2d", height = 360}}
local row = s:spans {"area ", {tex = "\\left(\\frac{1}{\\sqrt{x}}\\right)", id = "eq"},
    id = "row", layer = 10, size = 36, fill = "#ffffff"}
s:spans {{tex = {"x", {"+", layer = 0, id = "zero"}, {"\\frac{a}{b}", layer = -4, id = "low"}, "y"},
    layer = 3, id = "override"}, layer = 10, point = {0, 100}, size = 24, id = "overrides"}
s:spans {{tex = {"x", "\\sqrt{y}"}, id = "parts"}, layer = 10, point = {0, -100}, size = 24}
local group = s:group {id = "ordinary", layer = 10}
group:circle {id = "child", center = {250, 0}, radius = 5}
local highlight = s:rectangle {around = row, padding = 5, fill = "#ffeb3b", stroke = "#00000000",
    layer = 0, opacity = 0, id = "highlight"}
s:wait(1)
s:set {target = highlight, opacity = 1}
s:wait(1)
return s
)lua";
    auto scene = _load(source, "tex-inline-layers.lua");
    if (!scene) return;
    auto layers = [&](const char* id, int32_t layer) {
        auto root = scene->object(id);
        CHECK(root && root->childCount() > 0u && root->layer == layer);
        if (!root) return;
        for (auto i = 0u; i < root->childCount(); i++) CHECK(root->childAt(i)->layer == layer);
    };
    layers("eq", 10);
    layers("parts", 10);
    layers("parts/1", 10);
    layers("parts/2", 10);
    layers("override/1", 3);
    layers("zero", 0);
    layers("low", -4);
    layers("override/4", 3);
    CHECK(scene->object("override") && scene->object("override")->layer == 3);
    CHECK(scene->object("ordinary") && scene->object("ordinary")->layer == 10);
    CHECK(scene->object("child") && scene->object("child")->layer == 0);

    // Check the actual draw layers for glyphs, the fraction bar, root and delimiters.
    LayoutReport report;
    CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
    auto eq = scene->object("eq");
    auto glyphs = 0u;
    auto bars = 0u;
    auto paths = 0u;
    for (auto i = 0u; eq && i < eq->childCount(); i++) {
        auto piece = eq->childAt(i);
        if (piece->type() == Type::Text) glyphs++;
        else if (piece->type() == Type::Rectangle) bars++;
        else if (piece->type() == Type::Path) paths++;
        auto drawn = false;
        for (auto j = 0u; j < report.objectCount(); j++) {
            auto item = report.objectAt(j);
            if (item->object != piece) continue;
            CHECK(item->layer == 10);
            drawn = true;
        }
        CHECK(drawn);
    }
    CHECK(glyphs >= 2u && bars >= 1u && paths >= 3u);

    // Adding a lower-layer highlight must preserve all opaque white formula ink.
    Surface clear;
    Surface highlighted;
    CHECK(renderer->render(scene, 0.0f, clear) == Result::Success);
    CHECK(renderer->render(scene, 1.0f, highlighted) == Result::Success);
    auto white = 0u;
    auto lost = 0u;
    for (auto y = 0u; y < clear.height(); y++) {
        for (auto x = 0u; x < clear.width(); x++) {
            auto offset = static_cast<size_t>(y) * clear.stride() + x;
            if (clear.data()[offset] != 0xffffffffu) continue;
            white++;
            if (highlighted.data()[offset] != 0xffffffffu) lost++;
        }
    }
    CHECK(white > 100u && lost == 0u);
    delete scene;

    _error("local s = vis.scene {} s:spans {{tex = 'x', layer = 1.5}} return s", "tex-inline-layer.lua",
           "layer must be an integer");
    _error("local s = vis.scene {} s:spans {{tex = 'x'}, layer = 1 << 40} return s", "tex-inline-layer-range.lua",
           "layer is out of range");
    _error("local s = vis.scene {} s:spans {{tex = {{'x', layer = 1.5}}}} return s", "tex-inline-part-layer.lua",
           "layer must be an integer");
    _error("local s = vis.scene {} s:spans {{tex = {{'x', layer = 1 << 40}}}} return s",
           "tex-inline-part-layer-range.lua", "layer is out of range");
}

// The ink of a text, or of a glyph of a formula, as heights of the scene.
static float _inkTop(const Scene* scene, const char* id, float baseline)
{
    TextExtent extent;
    auto text = _text(scene, id);
    CHECK(text && const_cast<Scene*>(scene)->measure(text, extent) == Result::Success);
    return baseline + extent.top;
}

static float _inkBottom(const Scene* scene, const char* id, float baseline)
{
    TextExtent extent;
    auto text = _text(scene, id);
    CHECK(text && const_cast<Scene*>(scene)->measure(text, extent) == Result::Success);
    return baseline + extent.bottom;
}

// With leading, inline formulas that fit leave the baselines evenly apart, and a formula
// that would reach into the ink of the line beside it moves its line just far enough to
// keep a tenth of the line height clear.
static void _leading()
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
s:spans {"x\n", {tex = "a^2+b^2=c^2", id = "fits"}, " here\nx", leading = 1.5, size = 30, id = "even"}
s:spans {"gg\n", {tex = "\\frac{a}{b}", display = true, id = "frac"}, "\nHH\nx", leading = 1, size = 30,
         point = {300, 0}, id = "tall"}
return s
)lua";
    auto scene = _load(source, "tex-leading.lua");
    if (!scene) return;
    auto blank = Text::gen(" ");
    TextExtent space;
    if (blank) blank->size = 30.0f;
    CHECK(blank && scene->measure(blank, space) == Result::Success);
    delete blank;
    auto lineHeight = space.ascent + space.descent;
    // The formula sits on its line's baseline, and the lines stand 1.5 line heights apart.
    auto line = _baseline(scene, "even/3");
    CHECK(_near(_glyphBaseline(scene, "fits/1"), line));
    CHECK(_near(_baseline(scene, "even/1") - line, 1.5f * lineHeight));
    CHECK(_near(line - _baseline(scene, "even/4"), 1.5f * lineHeight));
    // The display fraction keeps a tenth of the line height from the descenders above and the
    // capitals below, and the line after them keeps the pitch.
    auto clear = 0.1f * lineHeight;
    auto top = _inkTop(scene, "frac/2", _glyphBaseline(scene, "frac/2"));
    auto bottom = _inkBottom(scene, "frac/3", _glyphBaseline(scene, "frac/3"));
    CHECK(_near(_inkBottom(scene, "tall/1", _baseline(scene, "tall/1")) - top, clear));
    CHECK(_near(bottom - _inkTop(scene, "tall/3", _baseline(scene, "tall/3")), clear));
    CHECK(_baseline(scene, "tall/1") - _baseline(scene, "tall/3") > 2.0f * lineHeight);
    CHECK(_near(_baseline(scene, "tall/3") - _baseline(scene, "tall/4"), lineHeight));
    delete scene;
}

// matrix{rows = ...}: with the tex module a string entry is an inline formula in the matrix's
// size and color, on its row's baseline, while numbers stay texts that count and blanks stay
// texts.
static void _matrixEntries()
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
local m = s:matrix {rows = {{1, "x^2", "\\frac{1}{2}"}, {"", "y_i", 3}}, size = 30, color = "#336699", id = "m"}
if not rawequal(m:entry(1, 2), s:find("m/1/2")) then error("a formula entry keeps its name") end
if not rawequal(m:entry(1, 2):part("x"), s:find("m/1/2/1")) then error("part finds a glyph of an entry") end
s:play({target = m:entry(1, 1), value = 7}, 1)
return s
)lua";
    auto scene = _load(source, "tex-matrix.lua");
    if (!scene) return;
    auto x = scene->object("m/1/2");
    CHECK(x && x->type() == Type::Group && static_cast<const Group*>(x)->typeset && x->parent() == scene->object("m/1"));
    CHECK(x && same(x->style.fill, Color{0x33, 0x66, 0x99, 255}));
    auto glyph = _text(scene, "m/1/2/1");
    CHECK(glyph && glyph->size == 30.0f);
    // A number counts as before, and a blank entry is a text.
    auto one = _text(scene, "m/1/1");
    CHECK(one && one->counting());
    CHECK(_text(scene, "m/2/1") != nullptr);
    // Formulas center their TeX boxes in their column, which a script's space after it leaves a
    // little off their ink, and the second row lies below the first.
    Bounds square;
    Bounds sub;
    CHECK(x && x->bounds(square) && scene->object("m/2/2")->bounds(sub));
    CHECK(_near(0.5f * (square.min.x + square.max.x), 0.5f * (sub.min.x + sub.max.x), 2.0f));
    CHECK(sub.max.y < square.min.y);
    delete scene;
}

// baseline = true pins a formula's baseline at its point, where a text pinned the same way
// sits, and align.x still places it along the line.
static void _baselinePinned()
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 1280, height = 720, camera = {mode = "fixed", view = "2d", height = 720}}
s:text {text = "area", point = {-200, 40}, size = 30, baseline = true, align = {1, 0.5}, id = "word"}
s:tex {tex = "\\frac{x^2}{2}", point = {-200, 40}, size = 30, baseline = true, align = {0, 0}, id = "frac"}
s:tex {tex = "x", point = {300, -40}, size = 30, baseline = true, align = {1, 1}, id = "right"}
s:tex {tex = "x", point = {300, 100}, size = 30, align = {1, 1}, id = "boxed"}
s:spans {"so ", {tex = "y_1", id = "y"}, point = {0, -200}, size = 30, baseline = true, id = "row"}
return s
)lua";
    auto scene = _load(source, "tex-baseline.lua");
    if (!scene) return;
    // The fraction's baseline is its math axis less the axis height: its bar lies above the
    // word's baseline, and its numerator and denominator on either side of it.
    Bounds bar;
    CHECK(scene->object("frac/1") && scene->object("frac/1")->bounds(bar));
    CHECK(bar.center().y > 40.0f && bar.center().y < 60.0f);
    CHECK(_glyphBaseline(scene, "frac/2") > bar.center().y && _glyphBaseline(scene, "frac/4") < 40.0f);
    CHECK(_near(_glyphBaseline(scene, "right/1"), -40.0f));
    // align.x places the formula along its baseline as it places its box.
    auto x = _text(scene, "right/1");
    auto boxed = _text(scene, "boxed/1");
    CHECK(x && boxed && x->point.x == boxed->point.x);
    CHECK(_near(_glyphBaseline(scene, "y/1"), -200.0f) && _near(_baseline(scene, "row/1"), -200.0f));
    delete scene;

    _error("local s = vis.scene {} s:tex {tex = 'x', baseline = 1} return s", "tex-baseline-type.lua",
           "baseline must be a boolean");
    _error("local s = vis.scene {} s:spans {{tex = 'x', baseline = true}} return s", "tex-baseline-part.lua",
           "unknown option 'baseline'");
}

// A formula is a handle like any other, so a space places it beside a tick as a label by its
// bounds, below the space or anywhere in the scene, without knowing it is a formula.
static void _axisLabels()
{
    static constexpr char source[] = R"lua(
local s = vis.scene {width = 600, height = 300, camera = {mode = "fixed", view = "2d", height = 6}}
local world = s:space {x = {-4, 4, 1}, y = {-2, 2, 1}, ticks = {x = math.pi / 2, y = 1}}
local half = world:tex {tex = "\\frac{\\pi}{2}", id = "half"}
local minus = s:tex {tex = "-\\pi", id = "minus"}
local made = world:labels {x = {{math.pi / 2, half}, {-math.pi, minus}}, y = {{1, s:tex {tex = "1", id = "one"}}}}
if #made ~= 3 or not rawequal(made[1], half) or not rawequal(made[2], minus) then
    error("labels return the handles given")
end
return s
)lua";
    auto scene = _load(source, "tex-axis.lua");
    if (!scene) return;
    // 50 pixels to a unit: the side of a label that faces its axis lies 4 + 4 pixels, 0.16, from it.
    auto bounds = [&](const char* id, Bounds& box) {
        auto object = scene->object(id);
        return object && object->bounds(box);
    };
    Bounds half;
    Bounds minus;
    Bounds one;
    CHECK(bounds("half", half) && std::fabs(half.center().x - 1.5707963f) < 1.0e-4f
          && std::fabs(half.max.y + 0.16f) < 1.0e-4f && half.size().y > 0.3f);
    CHECK(bounds("minus", minus) && std::fabs(minus.center().x + 3.1415927f) < 1.0e-4f
          && std::fabs(minus.max.y + 0.16f) < 1.0e-4f);
    CHECK(bounds("one", one) && std::fabs(one.max.x + 0.16f) < 1.0e-4f && std::fabs(one.center().y - 1.0f) < 1.0e-4f);
    auto formula = scene->object("half");
    CHECK(formula && formula->parent() && formula->parent()->type() == Type::Space);
    delete scene;
}

// The host that runs every module enabled sets formulas too.
static void _host()
{
    static constexpr char source[] = "local scene = vis.scene {} scene:tex {tex = 'x', id = 'x'} return scene";
    char error[1024] = {};
    Scene* scene = nullptr;
    detail::LuaHostContext context;
    auto status = detail::luaHostLoad(source, sizeof(source) - 1u, "tex-host.lua", &scene, error, sizeof(error), nullptr,
                                      nullptr, nullptr, nullptr, context);
    CHECK(status == Result::Success && scene && scene->object("x/1"));
    if (status != Result::Success) std::fprintf(stderr, "tex-host.lua: %s\n", error);
    delete scene;
}

int main()
{
    auto renderer = Renderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
        CHECK(renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
        _single(renderer);
        _parts();
        _shapes();
        _errors();
        _empties();
        _partLimit();
        _matching(renderer);
        _choosing(renderer);
        _framing(renderer);
        _bracing(renderer);
        _inline(renderer);
        _spanLines();
        _matrixEntries();
        _inlineLayers(renderer);
        _leading();
        _baselinePinned();
        _axisLabels();
        _host();
    }
    delete renderer;
    return report();
}
