#include <cmath>
#include <cstdio>
#include <cstring>

#include "vis.h"
#if defined(VIS_DIAGRAM)
#include "vis_diagram.h"
#endif
#include "visLuaHost.h"
#if defined(VIS_UI)
#include "vis_ui.h"
#endif
#include "testLuaCollector.h"
#include "testCheck.h"

using namespace vis;

static Result load(const char* source, Scene*& scene, char* error,
                   uint32_t errorSize, detail::LuaHostContext& context)
{
    return detail::luaHostLoad(source, static_cast<uint32_t>(std::strlen(source)),
                               "authoring-modules.lua", &scene, error, errorSize,
                               nullptr, nullptr, nullptr, nullptr, context);
}

#if defined(VIS_DIAGRAM)
static void combinedModules()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {
    width = 800, height = 450, loop = true,
    camera = {view = "2d", height = 10}
}

local diagram = vis.diagram(scene, {
    id = "pipeline", direction = "lr", origin = {0, 2.2},
    node_size = {2.4, 0.9}, rank_gap = 1.2, node_gap = 0.4,
})
local parser = diagram:node {id = "parser", label = "Parser", detail = "source to AST"}
local lower = diagram:node {id = "lower", label = "Lower", detail = "AST to IR"}
local edge = diagram:connect {
    id = "parser-lower", from = parser, to = lower, label = "AST",
    route = "orthogonal", flow = true, flow_offset = 0,
}
diagram:zone {id = "frontend", label = "Frontend", members = {parser, lower}}
local built_diagram = diagram:build()
scene:fade_in(built_diagram:nodes(), {duration = 0.2})
scene:create(built_diagram:routes(), 0.2, "linear")
scene:play({target = built_diagram:edge(edge), dash_offset = -0.36}, 0.2, "linear")
scene:indicate(built_diagram:node(lower), {duration = 0.2, scale = 1.05})

if vis.ui then
    local panel = vis.ui.panel(scene)
    panel:camera_reset()
end
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, error, sizeof(error), context);
    CHECK(result == Result::Success);
    CHECK(scene != nullptr);
    if (result != Result::Success) std::fprintf(stderr, "combined modules: %s\n", error);
    if (scene) {
        CHECK(scene->object("pipeline:root") != nullptr);
        CHECK(scene->object("pipeline:node:parser") != nullptr);
        CHECK(scene->object("pipeline:edge:parser-lower") != nullptr);
        CHECK(scene->object("pipeline:edge:parser-lower:flow") == nullptr);
        CHECK(scene->duration() > 0.7f);
#if defined(VIS_UI)
        CHECK(context.ui.panel != nullptr);
        CHECK(context.ui.panel && context.ui.panel->scene() == scene);
#endif
    }
    delete scene;
}
#endif

#if defined(VIS_DIAGRAM)
static void diagramScriptFailures()
{
    static constexpr char mutation[] = R"lua(
local scene = vis.scene {}
local d = vis.diagram(scene, {id = "once"})
d:node {id = "a"}
d:build()
d:node {id = "b"}
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(mutation, scene, error, sizeof(error), context);
    CHECK(result == Result::ScriptError && scene == nullptr);
    CHECK(std::strstr(error, "already built") != nullptr);

    static constexpr char foreign[] = R"lua(
local scene = vis.scene {}
local left = vis.diagram(scene, {id = "left"})
local right = vis.diagram(scene, {id = "right"})
local a = left:node {id = "a"}
local b = right:node {id = "b"}
left:connect {id = "bad", from = a, to = b}
return scene
)lua";
    context = {};
    result = load(foreign, scene, error, sizeof(error), context);
    CHECK(result == Result::ScriptError && scene == nullptr);
    CHECK(std::strstr(error, "another diagram") != nullptr);
}
#endif

#if defined(VIS_DIAGRAM)
static void standaloneDiagramLoader()
{
    static constexpr char diagramSource[] = R"lua(
local scene = vis.scene {}
local diagram = vis.diagram(scene, {
    id = "standalone-diagram", direction = "lr", origin = {0, 0},
    node_size = {2, 1}, rank_gap = 1, zone_padding = 0.5,
})
local entry = diagram:node {
    id = "entry", label = "Entry", detail = "semantic source", kind = "state",
}
local exit = diagram:node {id = "exit", label = "Exit", kind = "terminal"}
diagram:connect {
    id = "entry-exit", label = "flow", from = entry, to = exit,
    route = "orthogonal", kind = "error", flow = true,
}
diagram:zone {
    id = "scope", label = "Scope", members = {entry, exit}, full_width = true,
}
diagram:build()
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    auto result = diagram::Lua::load(
        diagramSource, static_cast<uint32_t>(std::strlen(diagramSource)),
        "standalone-diagram.lua", &scene, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    if (result != Result::Success) std::fprintf(stderr, "standalone diagram: %s\n", error);
    auto root = scene ? scene->object("standalone-diagram:root") : nullptr;
    auto receipt = diagram::ir(root);
    CHECK(root != nullptr);
    CHECK(receipt != nullptr);
    if (receipt) {
        CHECK(receipt->config().id
              && std::strcmp(receipt->config().id, "standalone-diagram") == 0);
        CHECK(receipt->config().layout == diagram::Layout::Ranked);
        CHECK(receipt->nodeCount() == 2u);
        CHECK(receipt->edgeCount() == 1u);
        CHECK(receipt->zoneCount() == 1u);
        CHECK(receipt->byteSize() > 0u);

        auto entry = receipt->nodeAt(0u);
        auto entryPlacement = receipt->nodePlacementAt(0u);
        auto exit = receipt->nodeAt(1u);
        auto exitPlacement = receipt->nodePlacementAt(1u);
        CHECK(entry && entry->id && std::strcmp(entry->id, "entry") == 0);
        CHECK(entry && entry->detail
              && std::strcmp(entry->detail, "semantic source") == 0);
        CHECK(entry && entry->kind == diagram::NodeKind::State);
        CHECK(entryPlacement && entryPlacement->rank == 0u);
        CHECK(entryPlacement && entryPlacement->center.x == 0.0f
              && entryPlacement->center.y == 0.0f);
        CHECK(entryPlacement && entryPlacement->size.x == 2.0f
              && entryPlacement->size.y == 1.0f);
        CHECK(exit && exit->id && std::strcmp(exit->id, "exit") == 0);
        CHECK(exit && exit->kind == diagram::NodeKind::Terminal);
        CHECK(exitPlacement && exitPlacement->rank == 1u);
        CHECK(exitPlacement && exitPlacement->center.x == 3.0f
              && exitPlacement->center.y == 0.0f);

        auto edge = receipt->edgeAt(0u);
        auto edgePlacement = receipt->edgePlacementAt(0u);
        CHECK(edge && edge->id && std::strcmp(edge->id, "entry-exit") == 0);
        CHECK(edge && edge->from.index == 0u && edge->to.index == 1u);
        CHECK(edge && edge->fromPort == diagram::Port::Auto
              && edge->toPort == diagram::Port::Auto);
        CHECK(edge && edge->route == diagram::Route::Orthogonal);
        CHECK(edge && edge->kind == diagram::EdgeKind::Error && edge->flow);
        CHECK(edgePlacement && edgePlacement->fromPort == diagram::Port::Right);
        CHECK(edgePlacement && edgePlacement->toPort == diagram::Port::Left);
        CHECK(edgePlacement && edgePlacement->route == diagram::Route::Orthogonal);
        CHECK(edgePlacement && edgePlacement->centerlineCount >= 2u
              && edgePlacement->centerline != nullptr);
        CHECK(edgePlacement && edgePlacement->hasLabelPoint);

        auto zone = receipt->zoneAt(0u);
        auto zonePlacement = receipt->zonePlacementAt(0u);
        CHECK(zone && zone->id && std::strcmp(zone->id, "scope") == 0);
        CHECK(zone && zone->memberCount == 2u && zone->members
              && zone->members[0].index == 0u
              && zone->members[1].index == 1u && zone->fullWidth);
        CHECK(zonePlacement && zonePlacement->size.x > 0.0f
              && zonePlacement->size.y > 0.0f);

        CHECK(receipt->object(diagram::Node{0u})
              == scene->object("standalone-diagram:node:entry"));
        CHECK(receipt->body(diagram::Node{0u})
              == scene->object("standalone-diagram:node:entry:body"));
        CHECK(receipt->label(diagram::Node{0u})
              == scene->object("standalone-diagram:node:entry:label"));
        CHECK(receipt->detail(diagram::Node{0u})
              == scene->object("standalone-diagram:node:entry:detail"));
        CHECK(receipt->object(diagram::Edge{0u})
              == scene->object("standalone-diagram:edge:entry-exit"));
        CHECK(receipt->label(diagram::Edge{0u})
              == scene->object("standalone-diagram:edge:entry-exit:label"));
        CHECK(receipt->object(diagram::Zone{0u})
              == scene->object("standalone-diagram:zone:scope"));
        CHECK(receipt->body(diagram::Zone{0u})
              == scene->object("standalone-diagram:zone:scope:body"));
        CHECK(receipt->label(diagram::Zone{0u})
              == scene->object("standalone-diagram:zone:scope:label"));

        diagram::ConstraintReport constraints;
        CHECK(diagram::validate(*receipt, constraints) == Result::Success);
        CHECK(constraints.count() == 0u);
    }
    delete scene;
}
#endif

#if defined(VIS_DIAGRAM)
// Ids, labels and details given as numbers are strings Lua makes of them, which only the stack
// holds. They stay there until the diagram copies them, which the collecting hook would
// otherwise free and scribble over first: the next number turned into text, the waypoints and
// the members all give the collector its chance.
static void numericDiagramText()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {}
local diagram = vis.diagram(scene, {id = 2024, direction = "lr", node_size = {2, 1}, rank_gap = 1})
local a = diagram:node {id = 1, label = 10, detail = 100}
local b = diagram:node {id = 2, label = 20}
diagram:connect {id = 12, label = 1.5, from = a, to = b, route = "orthogonal", waypoints = {{1.5, 0}}}
diagram:zone {id = 3, label = 30, members = {a, b}}
diagram:build()
return scene
)lua";
    detail::LuaHooks hooks[] = {diagram::lua_extension::luaHooks(), collectingHooks()};
    Scene* scene = nullptr;
    char error[1024] = {};
    auto result = detail::luaLoad(source, sizeof(source) - 1u, "numeric-diagram-text.lua", &scene, error,
                                  sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr, {hooks, 2u});
    CHECK(result == Result::Success && scene);
    if (result != Result::Success) std::fprintf(stderr, "numeric diagram text: %s\n", error);
    auto receipt = scene ? diagram::ir(scene->object("2024:root")) : nullptr;
    CHECK(receipt != nullptr);
    if (receipt) {
        auto same = [](const char* text, const char* expected) {
            return text && std::strcmp(text, expected) == 0;
        };
        CHECK(same(receipt->config().id, "2024"));
        auto first = receipt->nodeAt(0u);
        auto second = receipt->nodeAt(1u);
        CHECK(first && same(first->id, "1") && same(first->label, "10") && same(first->detail, "100"));
        CHECK(second && same(second->id, "2") && same(second->label, "20"));
        auto edge = receipt->edgeAt(0u);
        CHECK(edge && same(edge->id, "12") && same(edge->label, "1.5") && edge->waypointCount == 1u);
        auto zone = receipt->zoneAt(0u);
        CHECK(zone && same(zone->id, "3") && same(zone->label, "30") && zone->memberCount == 2u);
    }
    delete scene;
}
#endif

#if defined(VIS_DIAGRAM)
// An omitted or negative corner takes the Scene Theme's roundness of the shorter node side; an
// explicit corner wins. Zones round 1.5 times as much, and a terminal is a pill once corners round.
static void diagramThemeCorners()
{
    struct Case
    {
        const char* scene;
        const char* corner;
        float expected;
    };
    const Case cases[] = {
        {"", "", 0.0f},
        {"theme = 'pro_black'", "", 0.0f},
        {"theme = {roundness = 0.1}", "", 0.1f},
        {"theme = {preset = 'pro_black', roundness = 0.25}", "", 0.25f},
        {"theme = {roundness = 0.1}", "corner = -1", 0.1f},
        {"theme = {roundness = 0.1}", "corner = 0", 0.0f},
        {"theme = 'pro_white'", "corner = 0.2", 0.2f},
    };
    for (const auto& item : cases) {
        char source[512];
        std::snprintf(source, sizeof(source), R"lua(
local scene = vis.scene {%s}
local diagram = vis.diagram(scene, {id = "corners", node_size = {2, 1}, %s})
local a = diagram:node {id = "a"}
local b = diagram:node {id = "b", kind = "terminal"}
diagram:connect {id = "ab", from = a, to = b}
diagram:zone {id = "scope", members = {a, b}}
diagram:build()
return scene
)lua", item.scene, item.corner);
        Scene* scene = nullptr;
        char error[1024] = {};
        auto result = diagram::Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                                         "diagram-theme-corners.lua", &scene, error, sizeof(error));
        CHECK(result == Result::Success && scene);
        if (result != Result::Success) std::fprintf(stderr, "diagram theme corners: %s\n", error);
        auto corner = [scene](const char* id) {
            auto object = scene ? scene->object(id) : nullptr;
            return object && object->type() == Type::Rectangle
                       ? static_cast<const Rectangle*>(object)->corner
                       : -1.0f;
        };
        auto near = [](float lhs, float rhs) { return std::fabs(lhs - rhs) < 1.0e-4f; };
        CHECK(near(corner("corners:node:a:body"), item.expected));
        CHECK(near(corner("corners:node:b:body"), item.expected > 0.0f ? 0.5f : 0.0f));
        CHECK(near(corner("corners:zone:scope:body"), item.expected * 1.5f));
        delete scene;
    }
}
#endif

int main()
{
#if defined(VIS_DIAGRAM)
    combinedModules();
    diagramScriptFailures();
    standaloneDiagramLoader();
    numericDiagramText();
    diagramThemeCorners();
#endif
    return report();
}
