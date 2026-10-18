#include "testLuaChecks.h"

// A text measures in its loaded font, and the box it reports holds the glyphs the renderer
// paints for it.
void _measuring(SwRenderer* renderer)
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
        // A font that is not loaded measures as the default font that draws it.
        if (detached) {
            CHECK(detached->font("Missing") == Result::Success);
            CHECK(scene->measure(detached, world) == Result::Success && world.fallback && world.width > 0.0f);
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
        CHECK(loose->measure(extent) == Result::Success && extent.fallback);
        CHECK(loose->font("Pretendard") == Result::Success);
        CHECK(loose->measure(extent) == Result::Success && extent.width > 0.0f && !extent.fallback);
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
    _scriptLoads("local s=vis.scene{}; s:text{text='a', font='Missing'}:measure(); return s");
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

void _scalingTexts(SwRenderer* renderer)
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
void _slanting(SwRenderer* renderer)
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

void _spanning()
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
    _scriptLoads("local s=vis.scene{}; s:spans{'a', font='Missing'}; return s");
    // A formula part needs the tex module, which this script runs without, before any font.
    _scriptError("local s=vis.scene{}; s:spans{'a ', {tex='x^2'}, font='Missing'}; return s",
                 "a tex part needs the tex module: build viscript with -Dmodules=tex");
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
void _spanLeading()
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
void _baselines()
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

void _counting(SwRenderer* renderer)
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

void _textChanges()
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

// A text without a box counts with its glyphs as the scene sets them, so the frame of a
// circumscribe hugs the same box a measured twin of the text takes.
void _framingTexts(SwRenderer* renderer)
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

// A brace lies along the side of what it spans that faces its direction, offset off it, its ends
// where that side ends and its tip depth out; it is set with its text, so its sizes follow the
// text's size, and a label stands gap off its tip, a string as the ink of a text, a handle by
// its bounds.
void _bracing(SwRenderer* renderer)
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
if not sides(s:bounds(s:brace {around = dot, direction = "right", offset = 2, depth = 6}), 32, 38, 20, 80) then
    error("right")
end
if not sides(s:bounds(s:brace {around = dot, direction = "up", offset = 0, depth = 3}), -30, 30, 80, 83) then
    error("up")
end
if not sides(s:bounds(s:brace {around = dot, direction = {-2, 0}, offset = 1, depth = 4}), -35, -31, 20, 80) then
    error("left")
end
-- A label string stands gap off the tip by its ink, its middle on the tip.
local over, label = s:brace {around = dot, direction = "up", label = "radius", size = 20, id = "over"}
local l, o = s:bounds(label), s:bounds(over)
if not (near(l.bottom, o.top + 4) and near(l.left + l.right, 0)) then error("label") end
if s:find("over/label") ~= label then error("label id") end
-- A label handle moves there by its bounds, and comes back as the second value.
local note = s:text {text = "r", point = {100, 100}, measure = true, size = 20}
local _, moved = s:brace {around = dot, direction = "left", label = note, gap = 3, depth = 6, offset = 2}
if moved ~= note then error("label handle") end
local n = s:bounds(note)
if not (near(n.right, -41) and near(n.bottom + n.top, 100)) then error("label place") end
-- Between two points, below a segment drawn left to right unless a direction says otherwise.
if not sides(s:bounds(s:brace {from = {-100, -100}, to = {100, -100}, offset = 0, depth = 10}), -100, 100, -110, -100) then
    error("segment")
end
if not sides(s:bounds(s:brace {from = {-100, -100}, to = {100, -100}, direction = {1, 1}, offset = 2, depth = 10}),
             -100, 100, -98, -88) then
    error("segment side")
end
-- A slanted brace meets the corner of a square: its ends lie on the line through the corner.
local b = s:bounds(s:brace {around = s:rectangle {center = {0, -50}, size = {20, 20}}, direction = {1, 1}, offset = 0, depth = 4})
if not (near(b.left, 0) and near(b.bottom, -50)) then error("slanted") end
-- In a group, in the group's units.
local holder = s:group {matrix = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}
local inner = holder:rectangle {center = {0, -100}, size = {10, 4}}
if not sides(s:bounds(holder:brace {around = inner, offset = 1, depth = 2}), -10, 10, -210, -206) then error("group") end
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
    fails("s:brace{around=c, offset=-1}", "brace offset and gap must be zero or more");
    fails("s:brace{around=c, buff=1}", "unknown option 'buff'");
    fails("s:brace{around=c, gap=-1}", "brace offset and gap must be zero or more");
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
void _matrices()
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
void _tables()
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
