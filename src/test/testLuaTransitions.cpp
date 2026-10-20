#include "testLuaChecks.h"

void _viewports(Renderer* renderer)
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
    auto scene = _loaded(source, "viewports.lua");
    if (scene) {
        CHECK(scene->viewportCount() == 64);
        CHECK(scene->count() == 0);
        CHECK(scene->sceneAt(0) && scene->sceneAt(0)->count() == 2);
        CHECK(scene->sceneAt(1) && scene->sceneAt(1)->count() == 2);
        CHECK(std::fabs(scene->duration() - 1.2f) < 1.0e-4f);
        Surface surface;
        CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        if (surface.data()) {
            CHECK(surface.data()[50u * surface.stride() + 20u] == pixel(Color::hex("#d84040")));
            CHECK(surface.data()[50u * surface.stride() + 180u] == pixel(Color::hex("#4080d8")));
        }
    }
    delete scene;

}

void _sceneTransitions(Renderer* renderer)
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
    auto scene = _loaded(source, "scene-transition.lua");
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
        CHECK(changed(surface, scene->config().background) > 100u);
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(changed(surface, scene->config().background) > 100u);
        CHECK(renderer->render(scene, 0.9f, surface) == Result::Success);
        CHECK(changed(surface, scene->config().background) > 100u);
    }
    delete scene;
}

void _transitionStyles(Renderer* renderer)
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
    auto scene = _loaded(source, "transition-styles.lua");
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
    scene = _loaded(slides, "transition-slides.lua");
    if (scene) {
        auto white = pixel(Color::hex("#ffffff"));
        auto black = pixel(Color::hex("#000000"));
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
    scene = _loaded(zooms, "transition-zooms.lua");
    if (scene) {
        auto white = pixel(Color::hex("#ffffff"));
        auto black = pixel(Color::hex("#000000"));
        Surface surface;
        // Before a tenth of the way the incoming scene has not shown; halfway it fills the card.
        CHECK(renderer->render(scene, 0.05f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 60u] == pixel(Color::hex("#0000ff")));
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
    scene = _loaded(zoomOut, "transition-zoom-out.lua");
    if (scene) {
        Surface surface;
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 5u] == pixel(Color::hex("#000000")));
        CHECK(surface.data() && surface.data()[30u * surface.stride() + 60u] == pixel(Color::hex("#ffffff")));
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
                scene = _loaded(source, "transition-exact-area.lua");
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
    scene = _loaded(pairs, "transition-pairs.lua");
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
    fails("p:scene_transition({a,b},{hold={1,'slow'}})", "hold must be a number, got string");
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
          "area_margin must be a number, got boolean");
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
        scene = _loaded(script, "transition-no-options.lua");
        CHECK(scene && scene->duration() == 1.0f);
        delete scene;
    }
}

void _sceneTree(Renderer* renderer)
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

scene:tween({
    {target = box, shift = {0.5, 0}, opacity = 0.6,
     stroke = "#ffd166", fill = "#40d890"},
    {target = mixed, shift = {-0.5, 0}, fill = "#b060e8"}
}, 1.2, "linear", 0.25)
if math.abs(scene:duration() - 1.5) > 0.0001 then error("composite duration mismatch") end
return scene
)lua";

    auto scene = _loaded(source, "scene-tree.lua");
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
    CHECK(changed(surface, scene->config().background) > 500);
    delete scene;
}
