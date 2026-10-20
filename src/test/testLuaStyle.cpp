#include "testLuaChecks.h"

// A cross-fade takes its duration and curve as other plays do, a second and smooth when left out.
void _crossFades()
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
    auto scene = _loaded(source, "cross-fades.lua");
    CHECK(scene && scene->duration() == 1.5f);
    delete scene;
}

// An explicit size is kept as given, and an omitted one follows the theme role.
void _textSizes()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
scene:text {text = "sized", size = 28, id = "sized"}
scene:text {text = "body", id = "body"}
scene:text {text = "title", role = "h1", id = "title"}
return scene
)lua";
    auto scene = _loaded(source, "text-size.lua");
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

void _groupStyles()
{
    static constexpr auto source = R"lua(
local scene = vis.scene {}
local g = scene:group {stroke = "#00ff00", fill = "#ff0000", width = 6, id = "g"}
g:circle {radius = 1, center = {-2, 0}, id = "circle"}
g:rectangle {size = {1, 1}, center = {2, 0}, stroke = "#ffff00", id = "own"}
g:text {text = "label", id = "label"}
scene:tween({target = g, fill = "#0000ff"}, 1)
scene:stroke(g, "#ffffff", 0.5)
return scene
)lua";
    auto scene = _loaded(source, "group-style.lua");
    if (scene) {
        auto circle = scene->object("circle");
        auto own = scene->object("own");
        auto label = scene->object("label");
        CHECK(circle && same(circle->style.stroke, Color::hex("#ffffff")));
        CHECK(circle && same(circle->style.fill, Color::hex("#0000ff")) && circle->style.width == 6.0f);
        CHECK(own && same(own->style.stroke, Color::hex("#ffff00")) && same(own->style.fill, Color::hex("#0000ff")));
        CHECK(label && same(label->style.fill, Theme::preset(ThemePreset::ProWhite).text.color));
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
scene:tween({target = row, fill = "#0000ff"}, 1)
scene:set {target = inner, fill = "#ffffff"}
return scene
)lua";
    scene = _loaded(blocks, "group-blocks.lua");
    if (scene) {
        auto role = Theme::preset(ThemePreset::ProWhite).text.color;
        auto text = [scene](const char* tag) { return scene->object(tag); };
        CHECK(text("set") && same(text("set")->style.fill, Color::hex("#ff0000")));
        CHECK(text("innerText") && same(text("innerText")->style.fill, Color::hex("#ffffff")));
        CHECK(text("ownText") && same(text("ownText")->style.fill, Color::hex("#ffff00")));
        // A fill set above the block stays out of it, as it stays out of any text.
        CHECK(text("plainText") && same(text("plainText")->style.fill, role));
        Text* words[2] = {};
        CHECK(_spanTexts(scene, "row", words, 2) == 2u);
        CHECK(words[0] && same(words[0]->style.fill, Color::hex("#0000ff")));
        CHECK(words[1] && same(words[1]->style.fill, Color::hex("#00ffff")));
        // The tween adds a clip for the row and its first word, the set one for the group and its text.
        CHECK(scene->clipCount() == 4u);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; s:group{color='#ff0000'}; return s", "a group draws nothing itself");
    _scriptError("local s=vis.scene{}; s:group{dash={4,2}}; return s", "a group draws nothing itself");
    _scriptError("local s=vis.scene{}; s:group{gradient=true}; return s",
                 "a group draws nothing itself, so gradient is unsupported");
    _scriptError("local s=vis.scene{}; s:space{gradient=true}; return s", "gradient is supported only by vector Shapes");
    _scriptError("local s=vis.scene{}; s:number{value=1, gradient=true}; return s",
                 "gradient is supported only by vector Shapes");
    // Every shape that takes the group's color adds a clip to the script's budget.
    _scriptError("local s=vis.scene{}; local g=s:group{}; for i=1,1000 do g:point{point={0,0}} end;"
                 " for i=1,66 do s:tween({target=g, fill=i%2==0 and '#ff0000' or '#00ff00'}, 0.1) end; return s",
                 "scene animation limit exceeded");
}

// A frame reads each clip for its object and for every object below it, and the scene may make
// it read four million: thousands of points with a clip each load, as clips on a group that
// many objects sit in fail the script, whether the objects or the clips come last.
void _evaluating()
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
    auto scene = _loaded(source, "points.lua");
    CHECK(scene && scene->count() == 2000u && scene->clipCount() == 2000u && scene->sampleWork() == 2000u);
    delete scene;
    // Each move of a group of a thousand points is read for the group and each point.
    _scriptError("local s=vis.scene{}; local g=s:group{}; for i=1,1000 do g:point{point={0,0}} end;"
                 " for i=1,4000 do s:tween({target=g, shift={0.001, 0}}, 0.01) end; return s",
                 "scene evaluation limit exceeded");
    _scriptError("local s=vis.scene{}; local g=s:group{};"
                 " for i=1,4000 do s:tween({target=g, shift={0.001, 0}}, 0.01) end;"
                 " for i=1,1000 do g:point{point={0,0}} end; return s",
                 "scene evaluation limit exceeded");
    // The scenes of viewports add their work to the scene that shows them.
    static constexpr auto nested = R"lua(
local function half()
    local s = vis.scene {}
    local g = s:group {}
    for i = 1, 500 do g:point {point = {0, 0}} end
    for i = 1, 4000 do s:tween({target = g, shift = {0.001, 0}}, 0.01) end
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
void _composition()
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
scene:tween({{target = card2, blur = 5, shadow = {offset = {0, -12}}}, {target = glass, shadow = {blur = 10}}}, 1)
return scene
)lua";
    auto scene = _loaded(source, "composition.lua");
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
        // Objects hold the values at the cursor, after the tween.
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
    _scriptError("local s=vis.scene{}; local c=s:circle{}; s:tween({target=c, blur=-2}); return s",
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

void _arcsAndAngles()
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
    auto scene = _loaded(source, "arcs-and-angles.lua");
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
    _scriptError("local s = vis.scene {} s:angle {label = {text = 'x', size = 'big'}} return s",
                 "diagnostic.lua:1: size must be a number, got string");
    _scriptError("local s = vis.scene {} local a = s:circle {} s:tween {target = a, start = 1} return s", "start needs an arc and finite radians");
}

void _shapeProperties()
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
scene:tween({
    {target = c, radius = 1.5},
    {target = r, size = {3, 2}, corner = 0.5},
    {target = t, size = 40},
    {target = p, radius = 9},
    {target = g, width = 7},
    {target = bar, gradient = {type = "linear", stops = {"#00ff00", "#ffff00"}}},
    {target = ramp, gradient = "#00ff00"},
}, 1)
scene:tween({target = r, size = 2}, 1)
return scene
)lua";
    auto scene = _loaded(source, "shape-properties.lua");
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
        CHECK(bar && same(bar->style.gradient.stops[0].color, Color::hex("#00ff00")));
        CHECK(ramp && same(ramp->style.gradientEnd, Color::hex("#00ff00")));
    }
    delete scene;
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:tween({target=c, size=2}, 1); return s",
                 "could not play animation: size needs a rectangle");
    _scriptError("local s=vis.scene{}; local l=s:line{from={0,0},to={1,0}}; s:tween({target=l, radius=1}, 1);"
                 " return s",
                 "could not play animation: radius needs a circle");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:tween({target=r, corner=0.8}, 1);"
                 " return s",
                 "half the rectangle's shorter side");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}, gradient={type='linear', stops={'#000', '#fff'}}};"
                 " s:tween({target=r, gradient={type='radial', stops={'#000', '#fff'}}}, 1); return s",
                 "keep the current gradient's type");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:tween({target=r, size=true}, 1); return s",
                 "size must be a number for text or {width, height} for a rectangle");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:tween({target=r, size={1}}, 1); return s",
                 "size must contain exactly 2 numbers");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:tween({target=c, radius=0}, 1); return s",
                 "radius must be positive");
    _scriptError("local s=vis.scene{}; local r=s:rectangle{size={1,1}}; s:tween({target=r, corner=-1}, 1); return s",
                 "corner cannot be negative");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:tween({target=c, width=-1}, 1); return s",
                 "width cannot be negative");
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:tween({target=c, gradient=true}, 1); return s",
                 "gradient must be an end-stop color or a gradient table");
}

void _instantSets()
{
    static constexpr auto source = R"lua(
local s = vis.scene {}
local c = s:circle {radius = 0.5, id = "c"}
local r = s:rectangle {size = {1, 1}, id = "r"}
local g = s:group {}
g:line {from = {0, 0}, to = {1, 0}, id = "heir"}
s:set {target = c, opacity = 0.5, radius = 1}
s:tween({target = r, fill = "#ff0000"}, 1)
s:set {{target = r, size = {2, 1}}, {target = g, width = 5}}
return s
)lua";
    auto scene = _loaded(source, "instant-set.lua");
    if (scene) {
        auto circle = static_cast<Circle*>(scene->object("c"));
        auto box = static_cast<Rectangle*>(scene->object("r"));
        auto heir = scene->object("heir");
        CHECK(circle && circle->opacity == 0.5f && circle->radius == 1.0f);
        CHECK(box && box->size.x == 2.0f && box->size.y == 1.0f);
        CHECK(heir && heir->style.width == 5.0f);
        // The tween adds one clip; the later set adds one for the rectangle, the group and its line.
        CHECK(scene->clipCount() == 4u && scene->duration() == 1.0f);
    }
    delete scene;
    _scriptError("local s=vis.scene{}; local c=s:circle{radius=1}; s:set({target=c, opacity=0.5}, 1); return s",
                 "set takes only descriptors; use tween to animate the change");
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

void _themes(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local scene = vis.scene {
    width = 320, height = 180, background = "#fafafa",
    theme = {
        preset = "pro_white",
        background = "#101426",
        objects = {"#112233", "#445566"},
        object_width = 3.25,
        roundness = 0.25,
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

    auto scene = _loaded(source, "theme.lua");
    if (scene) {
        CHECK(pixel(scene->config().background) == pixel(Color::hex("#fafafa")));
        CHECK(scene->theme().objectCount == 2u);
        CHECK(_near(scene->theme().roundness, 0.25f));
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
            CHECK(pixel(title->style.fill) == pixel(Color::hex("#17191d")));
            CHECK(pixel(code->style.fill) == pixel(Color::hex("#8a3b12")));
            CHECK(pixel(axes->axisX) == pixel(Color::hex("#aa3311")));
            CHECK(pixel(axes->axisY) == pixel(Color::hex("#227744")));
            CHECK(pixel(axes->axisZ) == pixel(Color::hex("#335599")));
            CHECK(pixel(first->style.stroke) == pixel(Color::hex("#112233")));
            CHECK(_near(first->style.width, 3.25f));
            CHECK(pixel(explicitLine->style.stroke) == pixel(Color::hex("#abcdef")));
            CHECK(_near(explicitLine->style.width, 5.0f));
            CHECK(pixel(second->style.stroke) == pixel(Color::hex("#445566")));
            CHECK(_near(second->style.width, 3.25f));
            CHECK(pixel(wrapped->style.stroke) == pixel(Color::hex("#112233")));
            CHECK(pixel(wrapped->style.fill) == pixel(Color::hex("#112233")));
            CHECK(first->style.gradient.enabled());
            CHECK(pixel(first->style.gradientEnd) == pixel(Color::hex("#778899")));
            CHECK(customGradient->style.gradient.enabled());
            CHECK(pixel(customGradient->style.gradientEnd)
                  == pixel(Color::hex("#fedcba")));
            CHECK(!solid->style.gradient.enabled());
        }
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, scene->config().background) > 100u);
    }
    delete scene;

    static constexpr auto blackSource = R"lua(
local scene = vis.scene {theme = "pro_black"}
scene:point {point = {0, 0}, id = "point"}
local white = vis.scene {theme = "pro_white"}
white:point {point = {0, 0}, id = "white-point"}
scene:viewport(white, {x = 0, y = 0, width = 1, height = 1})
return scene
)lua";
    scene = _loaded(blackSource, "theme-pro-black.lua");
    if (scene) {
        CHECK(pixel(scene->config().background) == pixel(Color::hex("#000000")));
        auto point = scene->object("point");
        CHECK(point != nullptr);
        if (point) {
            CHECK(pixel(point->style.stroke) == pixel(Color::hex("#f28e2b")));
            CHECK(pixel(point->style.fill) == pixel(Color::hex("#f28e2b")));
        }
        // A child Scene keeps its own Theme.
        auto white = scene->sceneAt(0);
        CHECK(white != nullptr);
        if (white) {
            CHECK(pixel(white->config().background) == pixel(Color::hex("#ffffff")));
            auto whitePoint = white->object("white-point");
            CHECK(whitePoint != nullptr);
            if (whitePoint) {
                CHECK(pixel(whitePoint->style.stroke) == pixel(Color::hex("#b45f06")));
                CHECK(pixel(whitePoint->style.fill) == pixel(Color::hex("#b45f06")));
            }
        }
    }
    delete scene;

    static constexpr auto adaptiveSource = R"lua(
local scene = vis.scene {theme = "adaptive"}
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
    char error[1024] = {};
    scene = nullptr;
    auto status = Lua::load(adaptiveSource, static_cast<uint32_t>(std::strlen(adaptiveSource)),
                            "theme-adaptive-vscode.lua", &scene, error, sizeof(error), nullptr,
                            nullptr, &adaptive, &adaptiveUsed);
    CHECK(status == Result::Success);
    CHECK(scene != nullptr);
    CHECK(adaptiveUsed);
    if (scene) {
        CHECK(pixel(scene->config().background) == pixel(Color::hex("#101820")));
        auto title = static_cast<Text*>(scene->object("title"));
        auto line = scene->object("line");
        CHECK(title && line);
        if (title) CHECK(pixel(title->style.fill) == pixel(Color::hex("#e8edf2")));
        if (line) CHECK(pixel(line->style.stroke) == pixel(Color::hex("#62b0ff")));
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
    if (scene) CHECK(pixel(scene->config().background) == pixel(Color::hex("#ffffff")));
    delete scene;

    // A host theme the scene refuses stops the script with the scene's reason.
    adaptive.objectCount = 0u;
    scene = nullptr;
    status = Lua::load(adaptiveSource, static_cast<uint32_t>(std::strlen(adaptiveSource)),
                       "theme-adaptive-invalid.lua", &scene, error, sizeof(error), nullptr,
                       nullptr, &adaptive, nullptr);
    CHECK(status == Result::ScriptError);
    CHECK(scene == nullptr);
    CHECK(std::strstr(error, "theme-adaptive-invalid.lua:2: could not apply scene theme: the theme "
                             "needs named fonts, finite positive text sizes and object width, a "
                             "roundness from 0 to 0.5 and 1 to 10 object colors"));

    // Leaving out the theme, or a theme table's preset, means adaptive; a fixed preset does not.
    auto host = Theme::preset(ThemePreset::ProBlack);
    host.background = Color::hex("#101820");
    host.objects[0] = Color::hex("#62b0ff");
    host.objectCount = 1u;
    struct DefaultCase
    {
        const char* source;
        bool adaptive;
        const char* background;
        const char* stroke;
    };
    const DefaultCase defaults[] = {
        {"local s = vis.scene {}; s:line {from = {-1, 0}, to = {1, 0}, id = 'line'}; return s",
         true, "#101820", "#62b0ff"},
        {"local s = vis.scene {background = '#203040'}; s:line {from = {-1, 0}, to = {1, 0}, id = 'line'}; return s",
         true, "#203040", "#62b0ff"},
        {"local s = vis.scene {theme = {colors = {accent = '#ff0000'}}}; s:line {from = {-1, 0}, to = {1, 0}, stroke = 'accent', id = 'line'}; return s",
         true, "#101820", "#ff0000"},
        {"local s = vis.scene {theme = 'pro_white'}; s:line {from = {-1, 0}, to = {1, 0}, id = 'line'}; return s",
         false, "#ffffff", "#b45f06"},
    };
    for (auto& example : defaults) {
        adaptiveUsed = !example.adaptive;
        scene = nullptr;
        status = Lua::load(example.source, static_cast<uint32_t>(std::strlen(example.source)),
                           "theme-default.lua", &scene, error, sizeof(error), nullptr, nullptr, &host,
                           &adaptiveUsed);
        CHECK(status == Result::Success);
        CHECK(adaptiveUsed == example.adaptive);
        auto line = scene ? scene->object("line") : nullptr;
        CHECK(line != nullptr);
        if (scene) CHECK(pixel(scene->config().background) == pixel(Color::hex(example.background)));
        if (line) CHECK(pixel(line->style.stroke) == pixel(Color::hex(example.stroke)));
        if (!scene || !line) std::fprintf(stderr, "  in: %s\n  error: %s\n", example.source, error);
        delete scene;
    }

    static constexpr auto namedSource = R"lua(
local named = vis.scene {theme = "pro_black"}
named:point {point = {0, 0}, id = "named-point"}
return named
)lua";
    scene = _loaded(namedSource, "theme-pro-black.lua");
    if (scene) {
        CHECK(pixel(scene->config().background) == pixel(Color::hex("#000000")));
        CHECK(scene->theme().roundness == 0.0f);
        auto point = scene->object("named-point");
        CHECK(point && pixel(point->style.fill) == pixel(Color::hex("#f28e2b")));
    }
    delete scene;
    _scriptError("return vis.scene {theme = '3_blue_1_eyes'}",
                 "theme preset must be 'pro_white', 'pro_black' or 'adaptive'");
    _scriptError("return vis.scene {theme = 'adaptive_vscode'}",
                 "theme preset must be 'pro_white', 'pro_black' or 'adaptive'");

    // Without a host, the adaptive theme a scene takes when it names none is Pro White.
    static constexpr auto implicitSource = "return vis.scene {}";
    scene = _loaded(implicitSource, "theme-implicit.lua");
    if (scene) {
        CHECK(pixel(scene->config().background) == pixel(Color::hex("#ffffff")));
        CHECK(pixel(scene->theme().objects[0]) == pixel(Color::hex("#b45f06")));
        CHECK(scene->theme().roundness == 0.0f);
    }
    delete scene;
    _scriptError("return vis.scene {theme = {preset = 'pro_black', roundness = 0.75}}",
                 "theme roundness must be between 0 and 0.5");
    // A color read before the scene has a theme takes a hex value alone, and the message says so.
    for (auto source : {"return vis.scene {background = 'accent'}", "return vis.scene {theme = {colors = {muted = 'accent'}}}"}) {
        char error[256] = {};
        Scene* failed = nullptr;
        detail::LuaHostContext context;
        _load(source, "theme-hex.lua", failed, error, sizeof(error), context);
        auto end = std::strstr(error, " must be a hex color");
        CHECK(!failed && end && std::strcmp(end, " must be a hex color") == 0);
        if (!end || std::strcmp(end, " must be a hex color") != 0) std::fprintf(stderr, "  got: %s\n", error);
    }

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
    scene = _loaded(semanticSource, "theme-semantic.lua");
    if (scene) {
        CHECK(pixel(scene->theme().colors.accent) == pixel(Color::hex("#123456")));
        CHECK(pixel(scene->theme().colors.danger) == pixel(Color::hex("#bb2233")));
        CHECK(pixel(scene->theme().colors.result) == pixel(Color::hex("#886600")));
        CHECK(pixel(scene->theme().colors.focus) == pixel(Color::hex("#336699")));
        auto accent = scene->object("accent");
        auto warning = scene->object("warning");
        auto panel = scene->object("panel");
        auto semanticSpace = static_cast<Space*>(scene->object("semantic-space"));
        CHECK(accent && warning && panel && semanticSpace);
        if (accent) {
            CHECK(pixel(accent->style.stroke) == pixel(Color::hex("#123456")));
            CHECK(accent->style.gradient.enabled());
            CHECK(pixel(accent->style.gradientEnd) == pixel(Color::hex("#654321")));
        }
        if (warning) {
            CHECK(pixel(warning->style.stroke) == pixel(Color::hex("#aa7700")));
            CHECK(pixel(warning->style.fill) == pixel(Color::hex("#aa7700")));
        }
        if (panel) {
            CHECK(pixel(panel->style.stroke) == pixel(Color::hex("#ccd0d4")));
            CHECK(pixel(panel->style.fill) == pixel(Color::hex("#f0f1f2")));
        }
        if (semanticSpace) {
            CHECK(pixel(semanticSpace->axisX) == pixel(Color::hex("#bb2233")));
            CHECK(pixel(semanticSpace->axisY) == pixel(Color::hex("#118833")));
            CHECK(pixel(semanticSpace->axisZ) == pixel(Color::hex("#2277aa")));
            CHECK(pixel(semanticSpace->numberColor) == pixel(Color::hex("#777777")));
        }
        CHECK(_near(scene->duration(), 0.2f));
    }
    delete scene;
}

void _cellVoxelBindings(Renderer* renderer)
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
    auto scene = _loaded(source, "cell-voxel.lua");
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
        CHECK(changed(surface, scene->config().background) > 1000u);
        CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        CHECK(changed(surface, scene->config().background) > 1000u);
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

void _gradients()
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
    auto scene = _loaded(source, "gradients.lua");
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
            CHECK(pixel(linear.stops[0].color) == pixel(Color::hex("#112233")));
            CHECK(pixel(linear.stops[2].color) == pixel(Color::hex("#445566")));
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
            CHECK(pixel(legacy->style.gradientEnd) == pixel(Color::hex("#445566")));
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
