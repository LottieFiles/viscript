#include "testLuaChecks.h"

void _coding()
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
    _scriptLoads("local s=vis.scene{}; s:code{text='a', font='Missing'}; return s");
}

// A block pins its first baseline and keeps a world-unit pitch, including blank lines.
void _codeBaselines()
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

void _highlighting()
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
void _searching()
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
void _editing()
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
}, {duration = 2, curve = "linear", fade = 0.5})
if v3 ~= s:find("sum.3") then error("an edit counts on from the id it edits") end
local v4 = s:edit(v3, {{remove = {4, 4}}, {remove = " * i"}, {replace = "sum", with = "total", match = "last"}},
                  0.5, "smooth", {id = "final"})
if v4 ~= s:find("final") then error("an edit takes a given id") end
-- A block without an id at the root makes one without an id there too.
local bare = s:code {text = "x = 1", point = {0, -2}}
local edited = s:edit(bare, {replace = "1", with = "2"}, {duration = 0.5})
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
