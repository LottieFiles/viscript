#include "testLuaChecks.h"

void _emphasizing(Renderer* renderer)
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
    auto scene = _loaded(source, "wiggle.lua");
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
    fails("s:wiggle(r, {easing = 'smooth'})", "unknown option 'easing'");
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
    scene = _loaded(framing, "circumscribe.lua");
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
    scene = _loaded(passing, "passing.lua");
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
    scene = _loaded(bursting, "flash.lua");
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
    scene = _loaded(rippling, "ripple.lua");
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

void _animationMethods(Renderer* renderer)
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
scene:indicate(indicated, {color = "#ffd166", scale = 1.25, duration = 0.2, curve = "smooth"})

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
scene:morph(replacement_source, replacement_target, 0.2, "ease_in_out")
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

    auto scene = _loaded(source, "animation-methods.lua");
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
        CHECK(changed(surface, scene->config().background) > 100u);
    }
    delete scene;
}

void _dashTimeline(Renderer* renderer)
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
    auto scene = _loaded(source, "dash-timeline.lua");
    if (scene) {
        auto flow = scene->object("flow");
        CHECK(flow != nullptr);
        CHECK(flow && _near(flow->style.dashOffset, -32.0f));
        Surface middle;
        Surface end;
        CHECK(renderer->render(scene, 0.5f, middle) == Result::Success);
        auto middleChecksum = checksum(middle);
        CHECK(renderer->render(scene, 1.0f, end) == Result::Success);
        auto endChecksum = checksum(end);
        CHECK(middleChecksum != endChecksum);
    }
    delete scene;
}

void _motion(Renderer* renderer)
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
    auto scene = _loaded(source, "motion.lua");
    if (!scene) return;
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
    scene = _loaded(layered, "layered.lua");
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
    scene = _loaded(turned, "turned.lua");
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
    scene = _loaded(morphed, "morphed.lua");
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
    scene = _loaded(tracked, "tracked.lua");
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
    scene = _loaded(connected, "connected.lua");
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
    }
    delete scene;
    _scriptError("local s=vis.scene{} local a=s:circle{} local b=s:circle{center={3,3}} "
                 "local k=s:connector{from=a,to=b} s:move_along(a,k) return s",
                 "an endpoint cannot move along its connector");

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
static int32_t _layerAt(Renderer* renderer, Scene* scene, const char* id, float time)
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

void _layering(Renderer* renderer)
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
    auto scene = _loaded(source, "layers.lua");
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
static Vec2 _centerAt(Renderer* renderer, Scene* scene, const char* id, float time)
{
    auto object = scene->object(id);
    BBox ink;
    if (!object || renderer->bounds(scene, object, time, ink) != Result::Success) return {-1.0f, -1.0f};
    return ink.center();
}

// The pixel at (x, y) of the frame at time, packed as _pixel packs a color.
static uint32_t _pixelAt(Renderer* renderer, Scene* scene, float time, uint32_t x, uint32_t y)
{
    Surface surface;
    if (renderer->render(scene, time, surface) != Result::Success) return 0u;
    return surface.data()[static_cast<size_t>(y) * surface.stride() + x];
}

// A morph of two surfaces calls their color function again at the points between the two: the
// face over (0.1, 1.5) lies near height 0 halfway from the bowl to the saddle, gray, where the
// bowl is orange and the saddle blue, so a blend of the two ends would be neither.
void _pieceMorphs(Renderer* renderer)
{
    static constexpr auto source = R"lua(
local s = vis.scene {width = 320, height = 180, camera = {height = 6}}
local a = s:plot {points = {{-2,0},{-1,0},{0/0,0},{1,0},{2,0}}, id = "source"}
local b = s:path {commands = {{type="move",to={-2,2}},{type="line",to={-1,2}},{type="move",to={1,2}},{type="line",to={2,2}}}, id = "first"}
local c = s:plot {points = {{-2,-2},{-1,-2},{0/0,0},{1,-2},{2,-2}}, id = "second"}
s:morph({a,a}, {b,c}, 1, "linear", {copy=true, lag=0.25, pairs={{1,2}}})
return s
)lua";
    auto scene = _loaded(source, "piece-morph.lua");
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
        auto hash = checksum(surface);
        CHECK(renderer->render(scene, 1.25f, surface) == Result::Success);
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success && checksum(surface) == hash);
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

void _surfaceMorph(Renderer* renderer)
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
    auto scene = _loaded(source, "morph.lua");
    if (scene) {
        CHECK(_pixelAt(renderer, scene, 0.0f, 104u, 40u) == pixel(Color::hex("#f59e0b")));
        CHECK(_pixelAt(renderer, scene, 0.5f, 104u, 40u) == pixel(Color::hex("#e5e7eb")));
        CHECK(_pixelAt(renderer, scene, 1.0f, 104u, 40u) == pixel(Color::hex("#3b82f6")));
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

void _savingStates(Renderer* renderer)
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
    auto scene = _loaded(source, "save.lua");
    if (scene) {
        auto near = [](float lhs, float rhs) { return std::fabs(lhs - rhs) < 1.0f; };
        // A restore brings back a move, a recolor, a shape tween and text and number changes;
        // the text comes back to the string it showed when saved, before set replaced it.
        auto center = _centerAt(renderer, scene, "card", 1.5f);
        CHECK(near(center.x, 50.0f) && near(center.y, 30.0f));
        center = _centerAt(renderer, scene, "card", 2.0f);
        CHECK(near(center.x, 40.0f) && near(center.y, 40.0f));
        CHECK(_pixelAt(renderer, scene, 2.0f, 40u, 40u) == pixel(Color::hex("#ff0000")));
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
        CHECK(_pixelAt(renderer, scene, 4.0f, 40u, 40u) == pixel(Color::hex("#0000ff")));
        CHECK(_pixelAt(renderer, scene, 5.0f, 40u, 40u) == pixel(Color::hex("#ff0000")));
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
        CHECK(card && same(card->style.fill, Color::hex("#ff0000")) && _near(card->opacity, 1.0f));
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
