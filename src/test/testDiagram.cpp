#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <type_traits>

#include "vis_diagram.h"
#include "testCheck.h"

using namespace vis;
using namespace vis::diagram;

static_assert(!std::is_destructible_v<IR>);

static const ConstraintIssue* issue(const ConstraintReport& report, ConstraintCode code)
{
    for (auto i = 0u; i < report.count(); i++) {
        auto candidate = report.issueAt(i);
        if (candidate && candidate->code == code) return candidate;
    }
    return nullptr;
}

static const PhysicalConstraintIssue* issue(const PhysicalConstraintReport& report,
                                            PhysicalConstraintCode code)
{
    for (auto i = 0u; i < report.count(); i++) {
        auto candidate = report.issueAt(i);
        if (candidate && candidate->code == code) return candidate;
    }
    return nullptr;
}

static const PhysicalConstraintIssue* routeIssue(const PhysicalConstraintReport& report,
                                                 Edge first, Edge second)
{
    for (auto i = 0u; i < report.count(); i++) {
        auto candidate = report.issueAt(i);
        if (!candidate || candidate->code != PhysicalConstraintCode::RouteRouteCollision) {
            continue;
        }
        if ((candidate->edge.index == first.index
             && candidate->relatedEdge.index == second.index)
            || (candidate->edge.index == second.index
                && candidate->relatedEdge.index == first.index)) {
            return candidate;
        }
    }
    return nullptr;
}

static NodeSpec nodeSpec(const char* id, const char* label, const char* detail = nullptr)
{
    NodeSpec spec;
    spec.id = id;
    spec.label = label;
    spec.detail = detail;
    return spec;
}

static EdgeSpec edgeSpec(const char* id, Node from, Node to)
{
    EdgeSpec spec;
    spec.id = id;
    spec.from = from;
    spec.to = to;
    return spec;
}

static ZoneSpec zoneSpec(const char* id, const char* label, const Node* members,
                         uint32_t count)
{
    ZoneSpec spec;
    spec.id = id;
    spec.label = label;
    spec.members = members;
    spec.memberCount = count;
    return spec;
}

// Every preset keeps boxes square; this Theme rounds them by a tenth of the shorter node side.
static Theme rounded()
{
    auto theme = Theme::preset(ThemePreset::ProWhite);
    theme.roundness = 0.1f;
    return theme;
}

static void aggregateCompatibility()
{
    CHECK(std::strcmp(IR::LayoutAlgorithm, "vis.diagram.layout/v2") == 0);

    vis::diagram::Config config{
        "legacy", Direction::TopToBottom, {}, {2.0f, 1.0f}, 1.0f, 0.5f,
        0.4f, 0.08f, 0.32f, 0.28f, 0.12f, {},
    };
    CHECK(config.layout == Layout::Ranked && near(config.timeUnit, 1.0f));

    NodeSpec node{"node", "Node", nullptr, -1, {}, {}, false};
    CHECK(node.kind == NodeKind::Default && node.row == -1 && node.column == -1);

    Node from{0u};
    Node to{1u};
    EdgeSpec edge{
        "edge", nullptr, from, to, Port::Auto, Port::Auto, Route::Auto,
        nullptr, 0u, 0.0f, false,
    };
    CHECK(edge.kind == EdgeKind::Directed);

    Node members[] = {from};
    ZoneSpec zone{"zone", nullptr, members, 1u};
    CHECK(!zone.fullWidth);
}

static void ownershipAndBands()
{
    vis::diagram::Config config;
    config.id = "pipeline";
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    Node input;
    Node output;
    Edge transfer;
    Zone service;
    CHECK(diagram->add(nodeSpec("input", "Input", "request"), input) == Result::Success);
    NodeSpec outputSpec;
    outputSpec.id = "output";
    outputSpec.label = "Output";
    outputSpec.detail = "response";
    CHECK(diagram->add(outputSpec, output) == Result::Success);

    EdgeSpec edge;
    edge.id = "transfer";
    edge.label = "dispatch";
    edge.from = input;
    edge.to = output;
    edge.route = Route::Straight;
    edge.flow = true;
    edge.flowOffset = 2.0f;
    CHECK(diagram->add(edge, transfer) == Result::Success);
    Node members[] = {input, output};
    CHECK(diagram->add(zoneSpec("service", "Service", members, 2u), service)
          == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->root() && built->zones() && built->routes() && built->nodes()
          && built->annotations());
    if (!built) {
        delete diagram;
        return;
    }
    CHECK(std::strcmp(built->root()->tag(), "pipeline:root") == 0);
    CHECK(std::strcmp(built->object(input)->tag(), "pipeline:node:input") == 0);
    CHECK(built->body(input) && built->label(input) && built->detail(input));
    CHECK(built->body(input)->center.x < built->body(output)->center.x);
    CHECK(built->object(transfer) && built->object(transfer)->type() == Type::Arrow);
    CHECK(built->object(transfer)->childCount() == 0u);
    CHECK(built->object(transfer)->style.dashCount == 2u);
    CHECK(near(built->object(transfer)->style.dashOffset, 2.0f));
    CHECK(built->label(transfer));
    CHECK(built->object(service) && built->body(service) && built->label(service));
    CHECK(built->body(service)->size.x > built->body(output)->center.x
                                            - built->body(input)->center.x);

    auto root = built->release();
    CHECK(root && !built->root());
    CHECK(built->object(input) && built->object(transfer) && built->object(service));
    delete built;
    CHECK(std::strcmp(root->tag(), "pipeline:root") == 0);
    delete root;
    delete diagram;
}

static void receiptLifetimeAndSemantics()
{
    char configId[] = "receipt";
    vis::diagram::Config config;
    config.id = configId;
    config.layout = Layout::Manual;
    config.nodeSize = {2.0f, 1.0f};
    config.corner = 0.2f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;
    configId[0] = 'x';

    char sourceId[] = "source";
    char sourceDetail[] = "entry";
    NodeSpec sourceSpec;
    sourceSpec.id = sourceId;
    sourceSpec.detail = sourceDetail;
    sourceSpec.manualPosition = true;
    sourceSpec.position = {0.0f, 0.0f};
    sourceSpec.kind = NodeKind::State;
    Node source;
    CHECK(diagram->add(sourceSpec, source) == Result::Success);
    sourceId[0] = 'x';
    sourceDetail[0] = 'x';

    NodeSpec targetSpec;
    targetSpec.id = "target";
    targetSpec.label = "Target";
    targetSpec.manualPosition = true;
    targetSpec.position = {6.0f, 2.0f};
    targetSpec.kind = NodeKind::Terminal;
    Node target;
    CHECK(diagram->add(targetSpec, target) == Result::Success);

    Vec2 waypoints[] = {{3.0f, 0.0f}, {3.0f, 2.0f}};
    EdgeSpec edgeSpec;
    edgeSpec.id = "transfer";
    edgeSpec.label = "payload";
    edgeSpec.from = source;
    edgeSpec.to = target;
    edgeSpec.fromPort = Port::Right;
    edgeSpec.toPort = Port::Left;
    edgeSpec.route = Route::Orthogonal;
    edgeSpec.waypoints = waypoints;
    edgeSpec.waypointCount = 2u;
    edgeSpec.flowOffset = 1.5f;
    edgeSpec.flow = true;
    edgeSpec.kind = EdgeKind::Optional;
    Edge transfer;
    CHECK(diagram->add(edgeSpec, transfer) == Result::Success);
    waypoints[0] = {99.0f, 99.0f};

    Node members[] = {source, target};
    auto zoneSpec = ::zoneSpec("scope", nullptr, members, 2u);
    zoneSpec.fullWidth = true;
    Zone scope;
    CHECK(diagram->add(zoneSpec, scope) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir());
    if (!built) {
        delete diagram;
        return;
    }
    auto receipt = built->ir();
    auto sourceBody = built->body(source);
    auto root = built->release();
    CHECK(root && receipt && ir(root) == receipt && ir(built->nodes()) == nullptr);
    delete built;
    delete diagram;
    if (!receipt || !root) {
        delete root;
        return;
    }

    CHECK(std::strcmp(IR::Kind, "vis.diagram.compile-receipt") == 0);
    CHECK(IR::Version == 1u);
    CHECK(std::strcmp(IR::LayoutAlgorithm, "vis.diagram.layout/v2") == 0);
    CHECK(std::strcmp(receipt->config().id, "receipt") == 0);
    CHECK(receipt->config().layout == Layout::Manual);
    CHECK(receipt->nodeCount() == 2u && receipt->edgeCount() == 1u
          && receipt->zoneCount() == 1u);
    auto storedSource = receipt->nodeAt(source.index);
    auto sourcePlacement = receipt->nodePlacementAt(source.index);
    CHECK(storedSource && std::strcmp(storedSource->id, "source") == 0);
    CHECK(storedSource && std::strcmp(storedSource->label, "source") == 0);
    CHECK(storedSource && std::strcmp(storedSource->detail, "entry") == 0);
    CHECK(storedSource && storedSource->kind == NodeKind::State);
    CHECK(sourcePlacement && near(sourcePlacement->center.x, 0.0f)
          && near(sourcePlacement->center.y, 0.0f));
    CHECK(sourcePlacement && near(sourcePlacement->size.x, 2.0f)
          && near(sourcePlacement->size.y, 1.0f));

    auto storedEdge = receipt->edgeAt(transfer.index);
    auto edgePlacement = receipt->edgePlacementAt(transfer.index);
    CHECK(storedEdge && storedEdge->from.index == source.index
          && storedEdge->to.index == target.index);
    CHECK(storedEdge && storedEdge->fromPort == Port::Right
          && storedEdge->toPort == Port::Left && storedEdge->route == Route::Orthogonal);
    CHECK(storedEdge && storedEdge->kind == EdgeKind::Optional && storedEdge->flow
          && near(storedEdge->flowOffset, 1.5f));
    CHECK(storedEdge && storedEdge->waypointCount == 2u
          && near(storedEdge->waypoints[0].x, 3.0f)
          && near(storedEdge->waypoints[1].y, 2.0f));
    CHECK(edgePlacement && edgePlacement->fromPort == Port::Right
          && edgePlacement->toPort == Port::Left
          && edgePlacement->route == Route::Orthogonal);
    CHECK(edgePlacement && edgePlacement->centerlineCount == 4u
          && near(edgePlacement->centerline[0].x, 1.0f)
          && near(edgePlacement->centerline[3].x, 5.0f));
    CHECK(edgePlacement && edgePlacement->hasLabelPoint
          && std::isfinite(edgePlacement->labelPoint.x)
          && std::isfinite(edgePlacement->labelPoint.y));

    auto storedZone = receipt->zoneAt(scope.index);
    auto zonePlacement = receipt->zonePlacementAt(scope.index);
    CHECK(storedZone && storedZone->memberCount == 2u && storedZone->fullWidth
          && storedZone->members[0].index == source.index
          && storedZone->members[1].index == target.index);
    CHECK(zonePlacement && near(zonePlacement->center.x, 3.0f));
    CHECK(receipt->body(source) == sourceBody && receipt->object(transfer)
          && receipt->body(scope) && receipt->label(scope));
    CHECK(receipt->nodeAt(2u) == nullptr && receipt->edgeAt(1u) == nullptr
          && receipt->zoneAt(1u) == nullptr && receipt->byteSize() > sizeof(IR));

    ConstraintReport report;
    CHECK(validate(*receipt, report) == Result::Success && report.count() == 0u);
    auto wrapper = Group::gen();
    auto scene = Scene::gen();
    CHECK(wrapper && scene);
    if (wrapper && scene) {
        auto attach = wrapper->add(root);
        CHECK(attach == Result::Success);
        if (attach == Result::Success) {
            root = nullptr;
            attach = scene->add(wrapper);
            CHECK(attach == Result::Success);
            if (attach == Result::Success) {
                wrapper = nullptr;
                auto found = false;
                for (auto i = 0u; i < scene->count(); i++) {
                    if (ir(scene->objectAt(i)) == receipt) found = true;
                }
                CHECK(found);
            }
        }
    }
    delete root;
    delete wrapper;
    delete scene;
}

static void rankedLayouts()
{
    vis::diagram::Config config;
    config.direction = Direction::TopToBottom;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    Node start;
    Node first;
    Node second;
    Node final;
    CHECK(diagram->add(nodeSpec("start", "Start"), start) == Result::Success);
    CHECK(diagram->add(nodeSpec("first", "First"), first) == Result::Success);
    CHECK(diagram->add(nodeSpec("second", "Second"), second) == Result::Success);
    NodeSpec finalSpec;
    finalSpec.id = "final";
    finalSpec.label = "Final";
    finalSpec.rank = 4;
    CHECK(diagram->add(finalSpec, final) == Result::Success);

    Edge firstRoute;
    Edge secondRoute;
    Edge finalRoute;
    auto firstEdge = edgeSpec("a", start, first);
    firstEdge.route = Route::Straight;
    auto secondEdge = edgeSpec("b", start, second);
    secondEdge.route = Route::Straight;
    auto finalEdge = edgeSpec("c", first, final);
    finalEdge.route = Route::Straight;
    CHECK(diagram->add(firstEdge, firstRoute) == Result::Success);
    CHECK(diagram->add(secondEdge, secondRoute) == Result::Success);
    CHECK(diagram->add(finalEdge, finalRoute) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built);
    if (built) {
        CHECK(built->body(start)->center.y > built->body(first)->center.y);
        CHECK(built->body(first)->center.y > built->body(final)->center.y);
        CHECK(built->body(first)->center.x > built->body(second)->center.x);
        auto firstArrow = static_cast<Arrow*>(built->object(firstRoute));
        auto secondArrow = static_cast<Arrow*>(built->object(secondRoute));
        CHECK(firstArrow && secondArrow && firstArrow->from.x < secondArrow->from.x);
    }
    delete built;
    delete diagram;
}

static float primary(Direction direction, const Vec2& point)
{
    return direction == Direction::LeftToRight ? point.x : point.y;
}

static float secondary(Direction direction, const Vec2& point)
{
    return direction == Direction::LeftToRight ? point.y : point.x;
}

static void rankedPinnedReflow(Direction direction)
{
    vis::diagram::Config config;
    config.direction = direction;
    config.nodeSize = {2.0f, 1.0f};
    config.nodeGap = 0.5f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    Node first;
    Node second;
    Node pinned;
    CHECK(diagram->add(nodeSpec("first", "First"), first) == Result::Success);
    CHECK(diagram->add(nodeSpec("second", "Second"), second) == Result::Success);

    auto firstInitial = direction == Direction::LeftToRight
                            ? Vec2{0.0f, 0.75f}
                            : Vec2{1.25f, 0.0f};
    auto secondInitial = direction == Direction::LeftToRight
                             ? Vec2{0.0f, -0.75f}
                             : Vec2{-1.25f, 0.0f};
    auto pinnedSpec = nodeSpec("pinned", "Pinned");
    pinnedSpec.rank = 0;
    pinnedSpec.manualPosition = true;
    pinnedSpec.position = firstInitial;
    CHECK(diagram->add(pinnedSpec, pinned) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        auto firstPlacement = built->ir()->nodePlacementAt(first.index);
        auto secondPlacement = built->ir()->nodePlacementAt(second.index);
        auto pinnedPlacement = built->ir()->nodePlacementAt(pinned.index);
        CHECK(firstPlacement && secondPlacement && pinnedPlacement);
        if (firstPlacement && secondPlacement && pinnedPlacement) {
            auto firstShift = secondary(direction, firstPlacement->center)
                              - secondary(direction, firstInitial);
            auto secondShift = secondary(direction, secondPlacement->center)
                               - secondary(direction, secondInitial);
            CHECK(std::fabs(firstShift) > 1.0e-4f);
            CHECK(near(firstShift, secondShift));
            CHECK(near(primary(direction, firstPlacement->center), 0.0f));
            CHECK(near(primary(direction, secondPlacement->center), 0.0f));
            CHECK(near(secondary(direction, firstPlacement->center)
                           - secondary(direction, secondPlacement->center),
                       secondary(direction, firstInitial)
                           - secondary(direction, secondInitial)));
            CHECK(firstPlacement->rank == 0u && secondPlacement->rank == 0u);
            CHECK(near(pinnedPlacement->center.x, pinnedSpec.position.x));
            CHECK(near(pinnedPlacement->center.y, pinnedSpec.position.y));
        }
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedPinnedOverlapRemainsDiagnosed()
{
    vis::diagram::Config config;
    config.nodeSize = {2.0f, 1.0f};
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto spec = nodeSpec("first-pin", "First pin");
    spec.manualPosition = true;
    spec.position = {1.0f, -2.0f};
    Node first;
    CHECK(diagram->add(spec, first) == Result::Success);
    spec.id = "second-pin";
    spec.label = "Second pin";
    Node second;
    CHECK(diagram->add(spec, second) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        auto firstPlacement = built->ir()->nodePlacementAt(first.index);
        auto secondPlacement = built->ir()->nodePlacementAt(second.index);
        CHECK(firstPlacement && near(firstPlacement->center.x, 1.0f)
              && near(firstPlacement->center.y, -2.0f));
        CHECK(secondPlacement && near(secondPlacement->center.x, 1.0f)
              && near(secondPlacement->center.y, -2.0f));
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 1u);
        auto overlap = issue(report, ConstraintCode::NodeOverlap);
        CHECK(overlap && overlap->node.index == first.index
              && overlap->relatedNode.index == second.index);
    }
    delete built;
    delete diagram;
}

static void rankedExtremeFiniteReflow()
{
    vis::diagram::Config config;
    config.origin = {2.0e38f, 0.0f};
    config.nodeSize = {1.0e32f, 1.0f};
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    Node automatic;
    CHECK(diagram->add(nodeSpec("automatic", "Automatic"), automatic) == Result::Success);
    auto pinSpec = nodeSpec("pin", "Pin");
    pinSpec.manualPosition = true;
    pinSpec.position = config.origin;
    Node pin;
    CHECK(diagram->add(pinSpec, pin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        auto automaticPlacement = built->ir()->nodePlacementAt(automatic.index);
        auto pinPlacement = built->ir()->nodePlacementAt(pin.index);
        CHECK(automaticPlacement && std::isfinite(automaticPlacement->center.x)
              && std::isfinite(automaticPlacement->center.y));
        CHECK(automaticPlacement && automaticPlacement->center.y > 1.0f);
        CHECK(pinPlacement && pinPlacement->center.x == config.origin.x
              && pinPlacement->center.y == config.origin.y);
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedFloatRoundingReflow()
{
    vis::diagram::Config config;
    config.origin = {0.0f, -0.5980275273f};
    config.nodeGap = 0.0f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto automaticSpec = nodeSpec("automatic", "Automatic");
    automaticSpec.size = {2.0f, 415.3920593f};
    Node automatic;
    CHECK(diagram->add(automaticSpec, automatic) == Result::Success);
    auto pinSpec = nodeSpec("pin", "Pin");
    pinSpec.size = {2.0f, 698.2384644f};
    pinSpec.manualPosition = true;
    pinSpec.position = config.origin;
    Node pin;
    CHECK(diagram->add(pinSpec, pin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedRoundingBreakpointPreservesRigidBlock()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 55970.6875f};
    config.nodeGap = 0.0000139205722f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto firstSpec = nodeSpec("first", "First");
    firstSpec.size = {2.0f, 59812.171875f};
    Node first;
    CHECK(diagram->add(firstSpec, first) == Result::Success);
    auto secondSpec = nodeSpec("second", "Second");
    secondSpec.size = {2.0f, 396.7978515625f};
    Node second;
    CHECK(diagram->add(secondSpec, second) == Result::Success);
    auto pinSpec = nodeSpec("pin", "Pin");
    pinSpec.size = {2.0f, 19565.693359375f};
    pinSpec.manualPosition = true;
    pinSpec.position = {0.0f, 60323.2109375f};
    Node pin;
    CHECK(diagram->add(pinSpec, pin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedInitialFloatOverlapFailsClosed()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 1781186.375f};
    config.nodeGap = 0.0022109157871454954f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto firstSpec = nodeSpec("first", "First");
    firstSpec.size = {2.0f, 163150.390625f};
    Node first;
    CHECK(diagram->add(firstSpec, first) == Result::Success);
    auto secondSpec = nodeSpec("second", "Second");
    secondSpec.size = {2.0f, 130404.140625f};
    Node second;
    CHECK(diagram->add(secondSpec, second) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::InvalidArguments);
    CHECK(!built);
    delete diagram;
}

static void rankedCanonicalFloatOverlapFailsClosed()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 172.120361328125f};
    config.nodeGap = 2.6449449705978623e-6f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto firstSpec = nodeSpec("first", "First");
    firstSpec.size = {2.0f, 4.769577980041504f};
    Node first;
    CHECK(diagram->add(firstSpec, first) == Result::Success);
    auto secondSpec = nodeSpec("second", "Second");
    secondSpec.size = {2.0f, 10.07372760772705f};
    Node second;
    CHECK(diagram->add(secondSpec, second) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::InvalidArguments);
    CHECK(!built);
    delete diagram;
}

static void rankedCanonicalPrimaryOverlapReflows()
{
    vis::diagram::Config config;
    config.origin = {177.1572265625f, 0.0f};
    config.nodeGap = 0.5f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto automaticSpec = nodeSpec("automatic", "Automatic");
    automaticSpec.size = {4.769577980041504f, 2.0f};
    Node automatic;
    CHECK(diagram->add(automaticSpec, automatic) == Result::Success);
    auto pinSpec = nodeSpec("pin", "Pin");
    pinSpec.size = {10.07372760772705f, 2.0f};
    pinSpec.manualPosition = true;
    pinSpec.position = {169.73558044433594f, 0.0f};
    Node pin;
    CHECK(diagram->add(pinSpec, pin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        auto automaticPlacement = built->ir()->nodePlacementAt(automatic.index);
        auto pinPlacement = built->ir()->nodePlacementAt(pin.index);
        CHECK(automaticPlacement && std::fabs(automaticPlacement->center.y) > 2.0f);
        CHECK(pinPlacement && pinPlacement->center.x == pinSpec.position.x
              && pinPlacement->center.y == pinSpec.position.y);
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedCrossRankFloatOverlapFailsClosed()
{
    vis::diagram::Config config;
    config.origin = {580.4063110351562f, 0.0f};
    config.rankGap = 5.746298370468139e-7f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto firstSpec = nodeSpec("first", "First");
    firstSpec.rank = 0;
    firstSpec.size = {28.678424835205078f, 2.0f};
    Node first;
    CHECK(diagram->add(firstSpec, first) == Result::Success);
    auto secondSpec = nodeSpec("second", "Second");
    secondSpec.rank = 1;
    secondSpec.size = {17.232755661010742f, 2.0f};
    Node second;
    CHECK(diagram->add(secondSpec, second) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::InvalidArguments);
    CHECK(!built);
    delete diagram;
}

static void rankedChainedFloatIntervals()
{
    vis::diagram::Config config;
    config.nodeGap = 0.0f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto automaticSpec = nodeSpec("automatic", "Automatic");
    automaticSpec.size = {2.0f, 2.0f};
    Node automatic;
    CHECK(diagram->add(automaticSpec, automatic) == Result::Success);

    auto firstPinSpec = nodeSpec("first-pin", "First pin");
    firstPinSpec.size = {2.0f, 300000000.0f};
    firstPinSpec.manualPosition = true;
    firstPinSpec.position = {0.0f, -50000000.0f};
    Node firstPin;
    CHECK(diagram->add(firstPinSpec, firstPin) == Result::Success);

    auto secondPinSpec = nodeSpec("second-pin", "Second pin");
    secondPinSpec.size = {2.0f, 10.0f};
    secondPinSpec.manualPosition = true;
    secondPinSpec.position = {0.0f, 100000008.0f};
    Node secondPin;
    CHECK(diagram->add(secondPinSpec, secondPin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        auto placement = built->ir()->nodePlacementAt(automatic.index);
        CHECK(placement);
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedPerNodeRoundingBreakpointReflows()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 1987793.125f};
    config.nodeGap = 24.287349700927734f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const float automaticHeights[] = {
        15280.44921875f, 0.33478137850761414f, 2.9087910652160645f,
    };
    Node automatic[3];
    const char* automaticIds[] = {"first", "second", "third"};
    for (auto i = 0u; i < 3u; i++) {
        auto spec = nodeSpec(automaticIds[i], automaticIds[i]);
        spec.size = {2.0f, automaticHeights[i]};
        CHECK(diagram->add(spec, automatic[i]) == Result::Success);
    }

    const float pinCenters[] = {1988158.125f, 1994312.625f, 1973046.875f};
    const float pinHeights[] = {304.39862060546875f, 14384.75f,
                                263.41107177734375f};
    Node pins[3];
    const char* pinIds[] = {"first-pin", "second-pin", "third-pin"};
    for (auto i = 0u; i < 3u; i++) {
        auto spec = nodeSpec(pinIds[i], pinIds[i]);
        spec.size = {2.0f, pinHeights[i]};
        spec.manualPosition = true;
        spec.position = {0.0f, pinCenters[i]};
        CHECK(diagram->add(spec, pins[i]) == Result::Success);
    }

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 1u);
        auto overlap = issue(report, ConstraintCode::NodeOverlap);
        CHECK(overlap && overlap->node.index == pins[0].index
              && overlap->relatedNode.index == pins[1].index);
    }
    delete built;
    delete diagram;
}

static void rankedPairwiseRoundingBreakpointReflows()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 20876.78125f};
    config.nodeGap = 8.576795806902737e-8f;
    config.corner = 0.0f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const float automaticHeights[] = {
        6485.15283203125f, 0.0016429313691332936f,
        0.057669591158628464f, 630.694580078125f,
    };
    Node automatic[4];
    const char* automaticIds[] = {"first", "second", "third", "fourth"};
    for (auto i = 0u; i < 4u; i++) {
        auto spec = nodeSpec(automaticIds[i], automaticIds[i]);
        spec.size = {2.0f, automaticHeights[i]};
        CHECK(diagram->add(spec, automatic[i]) == Result::Success);
    }

    const float pinCenters[] = {-7209.19775390625f, 38638.4765625f,
                                16400.78125f, 21712.98828125f};
    const float pinHeights[] = {0.20358110964298248f, 4.4920806884765625f,
                                39.12228012084961f, 0.8112930655479431f};
    Node pins[4];
    const char* pinIds[] = {"first-pin", "second-pin", "third-pin", "fourth-pin"};
    for (auto i = 0u; i < 4u; i++) {
        auto spec = nodeSpec(pinIds[i], pinIds[i]);
        spec.size = {2.0f, pinHeights[i]};
        spec.manualPosition = true;
        spec.position = {0.0f, pinCenters[i]};
        CHECK(diagram->add(spec, pins[i]) == Result::Success);
    }

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        for (auto i = 0u; i < report.count(); i++) {
            auto problem = report.issueAt(i);
            CHECK(problem && problem->code == ConstraintCode::NodeOverlap
                  && problem->node.index >= pins[0].index
                  && problem->relatedNode.index >= pins[0].index);
        }
    }
    delete built;
    delete diagram;
}

static void rankedQuantizedGapReflows()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 17351.486328125f};
    config.nodeGap = 0.3126089572906494f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const float automaticHeights[] = {5586.845703125f, 3419.520751953125f};
    for (auto i = 0u; i < 2u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "automatic-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, automaticHeights[i]};
        Node node;
        CHECK(diagram->add(spec, node) == Result::Success);
    }
    auto pinSpec = nodeSpec("pin", "Pin");
    pinSpec.size = {2.0f, 12282.9482421875f};
    pinSpec.manualPosition = true;
    pinSpec.position = {0.0f, 12566.0634765625f};
    Node pin;
    CHECK(diagram->add(pinSpec, pin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedMultiBreakpointReflows()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 4925.5205078125f};
    config.nodeGap = 0.00012463693565223366f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const float automaticHeights[] = {
        2301.75048828125f, 912.7445068359375f, 524.9935302734375f,
        460.0561828613281f, 913.0161743164062f,
    };
    for (auto i = 0u; i < 5u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "automatic-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, automaticHeights[i]};
        Node node;
        CHECK(diagram->add(spec, node) == Result::Success);
    }

    const float pinCenters[] = {6743.5048828125f, 3051.29443359375f,
                                -929.702880859375f};
    const float pinHeights[] = {387.9822998046875f, 1440.7725830078125f,
                                1604.490966796875f};
    for (auto i = 0u; i < 3u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "pin-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, pinHeights[i]};
        spec.manualPosition = true;
        spec.position = {0.0f, pinCenters[i]};
        Node pin;
        CHECK(diagram->add(spec, pin) == Result::Success);
    }

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedAbsorbedBreakpointReflows()
{
    vis::diagram::Config config;
    config.origin = {0.0f, -0.11610884219408035f};
    config.nodeGap = 0.018206780776381493f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const float automaticHeights[] = {
        24578.8359375f, 1292935.125f, 8654328.0f, 13739791.0f,
    };
    for (auto i = 0u; i < 4u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "automatic-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, automaticHeights[i]};
        Node node;
        CHECK(diagram->add(spec, node) == Result::Success);
    }
    auto pinSpec = nodeSpec("pin", "Pin");
    pinSpec.size = {2.0f, 30268.845703125f};
    pinSpec.manualPosition = true;
    pinSpec.position = {0.0f, 2852602.25f};
    Node pin;
    CHECK(diagram->add(pinSpec, pin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedInterleavedBreakpointReflows()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 0.0013273073127493262f};
    config.nodeGap = 0.00041391915874555707f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const float automaticHeights[] = {
        74549.1328125f, 438100.0f, 7463.2060546875f,
        31572056.0f, 101594.2109375f,
    };
    for (auto i = 0u; i < 5u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "automatic-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, automaticHeights[i]};
        Node node;
        CHECK(diagram->add(spec, node) == Result::Success);
    }

    const float pinCenters[] = {
        -5214715.0f, 704612.1875f, -15785696.0f, -4118152.0f,
    };
    const float pinHeights[] = {
        14177.732421875f, 9818.568359375f, 5356.8310546875f,
        2340.986572265625f,
    };
    for (auto i = 0u; i < 4u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "pin-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, pinHeights[i]};
        spec.manualPosition = true;
        spec.position = {0.0f, pinCenters[i]};
        Node pin;
        CHECK(diagram->add(spec, pin) == Result::Success);
    }

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedUnrelatedFineUlpEventsDoNotExhaustReflow()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 0.0013273073127493262f};
    config.nodeGap = 0.00041391915874555707f;
    config.corner = 0.0f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const float automaticHeights[] = {
        74549.1328125f, 438100.0f, 7463.2060546875f,
        31572056.0f, 101594.2109375f,
    };
    for (auto i = 0u; i < 5u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "automatic-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, automaticHeights[i]};
        Node node;
        CHECK(diagram->add(spec, node) == Result::Success);
    }

    const float pinCenters[] = {
        -5214715.0f, -55706.390625f, -15785696.0f, -4118152.0f,
    };
    const float pinHeights[] = {
        14177.732421875f, 9818.568359375f, 5356.8310546875f,
        2340.986572265625f,
    };
    for (auto i = 0u; i < 4u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "pin-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, pinHeights[i]};
        spec.manualPosition = true;
        spec.position = {0.0f, pinCenters[i]};
        Node pin;
        CHECK(diagram->add(spec, pin) == Result::Success);
    }

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedRepairFineUlpEventsDoNotExhaustReflow()
{
    vis::diagram::Config config;
    config.origin = {0.0f, 1024.0f};
    config.nodeGap = 0.0f;
    config.corner = 0.0f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto height = 129.0f / 1048576.0f;
    for (auto i = 0u; i < 2u; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "automatic-%u", i);
        auto spec = nodeSpec(id, id);
        spec.size = {2.0f, height};
        Node node;
        CHECK(diagram->add(spec, node) == Result::Success);
    }
    auto pinSpec = nodeSpec("pin", "Pin");
    pinSpec.size = {2.0f, 1.0f};
    pinSpec.manualPosition = true;
    pinSpec.position = {0.0f, 0.0f};
    Node pin;
    CHECK(diagram->add(pinSpec, pin) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void rankedMaximumPinReflow()
{
    vis::diagram::Config config;
    config.nodeGap = 0.1f;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    for (auto i = 0u; i < 128u; i++) {
        char id[24];
        std::snprintf(id, sizeof(id), "automatic-%u", i);
        Node node;
        CHECK(diagram->add(nodeSpec(id, nullptr), node) == Result::Success);
    }
    for (auto i = 0u; i < 128u; i++) {
        char id[24];
        std::snprintf(id, sizeof(id), "pin-%u", i);
        auto spec = nodeSpec(id, nullptr);
        spec.size = {2.0f, 500.0f};
        spec.manualPosition = true;
        Node pin;
        CHECK(diagram->add(spec, pin) == Result::Success);
    }

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::Success);
    CHECK(built && built->ir() && built->ir()->nodeCount() == Diagram::NodeLimit);
    delete built;
    delete diagram;
}

static void automaticObstacleRouting(Direction direction, Route authoredRoute)
{
    Vec2 expected[Diagram::WaypointLimit + 8u]{};
    uint32_t expectedCount = 0u;
    for (auto pass = 0u; pass < 2u; pass++) {
        vis::diagram::Config config;
        config.layout = Layout::Manual;
        config.direction = direction;
        config.nodeSize = {2.0f, 2.0f};
        config.nodeGap = 0.5f;
        auto diagram = Diagram::gen(config);
        CHECK(diagram);
        if (!diagram) return;

        const Vec2 positions[] = {
            direction == Direction::LeftToRight ? Vec2{-4.0f, 0.0f}
                                                : Vec2{0.0f, 4.0f},
            direction == Direction::LeftToRight ? Vec2{4.0f, 0.0f}
                                                : Vec2{0.0f, -4.0f},
            {0.0f, 0.0f},
        };
        const char* ids[] = {"source", "target", "blocker"};
        Node nodes[3];
        for (auto i = 0u; i < 3u; i++) {
            auto spec = nodeSpec(ids[i], ids[i]);
            spec.manualPosition = true;
            spec.position = positions[i];
            CHECK(diagram->add(spec, nodes[i]) == Result::Success);
        }
        auto spec = edgeSpec("automatic", nodes[0], nodes[1]);
        spec.route = authoredRoute;
        Edge automatic;
        CHECK(diagram->add(spec, automatic) == Result::Success);

        Built* built = nullptr;
        CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
        CHECK(built && built->ir());
        if (built && built->ir()) {
            auto authored = built->ir()->edgeAt(automatic.index);
            auto placement = built->ir()->edgePlacementAt(automatic.index);
            CHECK(authored && authored->route == authoredRoute);
            CHECK(placement && placement->route == Route::Orthogonal);
            auto expectedFrom = direction == Direction::LeftToRight ? Port::Right
                                                                     : Port::Bottom;
            auto expectedTo = direction == Direction::LeftToRight ? Port::Left
                                                                   : Port::Top;
            CHECK(placement && placement->fromPort == expectedFrom
                  && placement->toPort == expectedTo);
            CHECK(placement && placement->centerline && placement->centerlineCount >= 4u);
            if (placement && placement->centerline) {
                auto capacity = static_cast<uint32_t>(sizeof(expected) / sizeof(expected[0]));
                CHECK(placement->centerlineCount <= capacity);
                auto secondarySize = direction == Direction::LeftToRight
                                         ? config.nodeSize.y
                                         : config.nodeSize.x;
                auto clearance = secondarySize * 0.5f
                                 + std::fmax(config.routeWidth, config.arrowWidth) * 0.5f
                                 + config.routeWidth;
                auto detours = false;
                for (auto i = 0u; i < placement->centerlineCount; i++) {
                    if (std::fabs(secondary(direction, placement->centerline[i]))
                        > clearance) {
                        detours = true;
                    }
                    if (i) {
                        auto dx = placement->centerline[i].x
                                  - placement->centerline[i - 1u].x;
                        auto dy = placement->centerline[i].y
                                  - placement->centerline[i - 1u].y;
                        CHECK(near(dx, 0.0f) || near(dy, 0.0f));
                    }
                }
                CHECK(detours);
                auto last = placement->centerlineCount - 1u;
                auto lastLength = std::hypot(
                    placement->centerline[last].x - placement->centerline[last - 1u].x,
                    placement->centerline[last].y - placement->centerline[last - 1u].y);
                CHECK(lastLength > config.arrowLength + 1.0e-5f);
                if (placement->centerlineCount <= capacity) {
                    if (!pass) {
                        expectedCount = placement->centerlineCount;
                        for (auto i = 0u; i < expectedCount; i++) {
                            expected[i] = placement->centerline[i];
                        }
                    } else {
                        CHECK(placement->centerlineCount == expectedCount);
                        if (placement->centerlineCount == expectedCount) {
                            for (auto i = 0u; i < expectedCount; i++) {
                                CHECK(placement->centerline[i].x == expected[i].x);
                                CHECK(placement->centerline[i].y == expected[i].y);
                            }
                        }
                    }
                }
            }
            ConstraintReport report;
            CHECK(validate(*built->ir(), report) == Result::Success);
            CHECK(report.count() == 0u);
        }
        delete built;
        delete diagram;
    }
}

static void automaticTightRouting(Direction direction, float primaryDistance)
{
    vis::diagram::Config config;
    config.layout = Layout::Manual;
    config.direction = direction;
    config.nodeSize = {2.0f, 2.0f};
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto sourceSpec = nodeSpec("source", "Source");
    sourceSpec.manualPosition = true;
    Node source;
    CHECK(diagram->add(sourceSpec, source) == Result::Success);
    auto targetSpec = nodeSpec("target", "Target");
    targetSpec.manualPosition = true;
    targetSpec.position = direction == Direction::LeftToRight
                              ? Vec2{primaryDistance, 4.0f}
                              : Vec2{4.0f, -primaryDistance};
    Node target;
    CHECK(diagram->add(targetSpec, target) == Result::Success);
    Edge route;
    CHECK(diagram->add(edgeSpec("tight", source, target), route) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir());
    if (built && built->ir()) {
        auto placement = built->ir()->edgePlacementAt(route.index);
        CHECK(placement && placement->route == Route::Orthogonal
              && placement->centerlineCount >= 4u);
        if (placement && placement->centerlineCount >= 2u) {
            auto last = placement->centerlineCount - 1u;
            auto firstFlow = primary(direction, placement->centerline[1])
                             - primary(direction, placement->centerline[0]);
            auto lastFlow = primary(direction, placement->centerline[last])
                            - primary(direction, placement->centerline[last - 1u]);
            auto expectedSign = direction == Direction::LeftToRight ? 1.0f : -1.0f;
            CHECK(firstFlow * expectedSign > 0.0f);
            CHECK(lastFlow * expectedSign > 0.0f);
            auto lastLength = std::hypot(
                placement->centerline[last].x - placement->centerline[last - 1u].x,
                placement->centerline[last].y - placement->centerline[last - 1u].y);
            CHECK(lastLength > config.arrowLength + 1.0e-5f);
        }
        ConstraintReport report;
        CHECK(validate(*built->ir(), report) == Result::Success);
        CHECK(report.count() == 0u);
    }
    delete built;
    delete diagram;
}

static void manualRoute()
{
    vis::diagram::Config config;
    config.id = "manual";
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    Node source;
    Node target;
    NodeSpec sourceSpec;
    sourceSpec.id = "source";
    sourceSpec.label = "Source";
    sourceSpec.manualPosition = true;
    sourceSpec.position = {0.0f, 0.0f};
    NodeSpec targetSpec;
    targetSpec.id = "target";
    targetSpec.label = "Target";
    targetSpec.manualPosition = true;
    targetSpec.position = {6.0f, 2.0f};
    CHECK(diagram->add(sourceSpec, source) == Result::Success);
    CHECK(diagram->add(targetSpec, target) == Result::Success);

    Vec2 waypoints[] = {{2.4f, 0.0f}, {3.0f, 0.0f}, {3.0f, 2.0f}};
    EdgeSpec edgeSpec;
    edgeSpec.id = "route";
    edgeSpec.from = source;
    edgeSpec.to = target;
    edgeSpec.fromPort = Port::Right;
    edgeSpec.toPort = Port::Left;
    edgeSpec.route = Route::Orthogonal;
    edgeSpec.waypoints = waypoints;
    edgeSpec.waypointCount = 3u;
    edgeSpec.flow = true;
    Edge route;
    CHECK(diagram->add(edgeSpec, route) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProBlack), built) == Result::Success);
    CHECK(built);
    if (built) {
        CHECK(near(built->body(source)->center.x, 0.0f));
        CHECK(near(built->body(source)->center.y, 0.0f));
        CHECK(near(built->body(target)->center.x, 6.0f));
        CHECK(near(built->body(target)->center.y, 2.0f));
        CHECK(built->object(route) && built->object(route)->type() == Type::Route);
        auto path = static_cast<vis::DirectedRoute*>(built->object(route));
        CHECK(path->count() >= 3u && path->childCount() == 0u);
        CHECK(path->style.dashCount == 2u);
        auto placement = built->ir()->edgePlacementAt(route.index);
        CHECK(placement && placement->centerlineCount == 5u);
        CHECK(placement && near(placement->centerline[1].x, 2.4f)
              && near(placement->centerline[1].y, 0.0f));
        CHECK(placement && near(placement->centerline[2].x, 3.0f)
              && near(placement->centerline[2].y, 0.0f));
    }
    delete built;
    delete diagram;
}

static void edgeLabelsClearRoutes()
{
    vis::diagram::Config config;
    config.id = "labels";
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    Node source;
    Node target;
    CHECK(diagram->add(nodeSpec("source", "Source"), source) == Result::Success);
    CHECK(diagram->add(nodeSpec("target", "Target"), target) == Result::Success);
    auto spec = edgeSpec("route", source, target);
    spec.label = "payload";
    spec.route = Route::Straight;
    Edge route;
    CHECK(diagram->add(spec, route) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->label(route));
    if (built && built->label(route)) {
        auto arrow = static_cast<Arrow*>(built->object(route));
        auto midpointY = (arrow->from.y + arrow->to.y) * 0.5f;
        CHECK(built->label(route)->point.y > midpointY);
    }
    delete built;
    delete diagram;
}

static void gridAndTimelineLayouts()
{
    vis::diagram::Config gridConfig;
    gridConfig.layout = Layout::Grid;
    gridConfig.origin = {-4.0f, 3.0f};
    gridConfig.nodeSize = {2.0f, 1.0f};
    gridConfig.rankGap = 0.5f;
    gridConfig.nodeGap = 0.75f;
    auto diagram = Diagram::gen(gridConfig);
    CHECK(diagram);
    if (!diagram) return;

    Node first;
    Node second;
    NodeSpec firstSpec;
    firstSpec.id = "first";
    firstSpec.row = 0;
    firstSpec.column = 0;
    NodeSpec secondSpec;
    secondSpec.id = "second";
    secondSpec.row = 1;
    secondSpec.column = 1;
    CHECK(diagram->add(firstSpec, first) == Result::Success);
    CHECK(diagram->add(secondSpec, second) == Result::Success);
    Node firstLaneMembers[] = {first};
    auto firstLaneSpec = zoneSpec("first-lane", "Lane 1", firstLaneMembers, 1u);
    firstLaneSpec.fullWidth = true;
    Zone firstLane;
    CHECK(diagram->add(firstLaneSpec, firstLane) == Result::Success);
    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && near(built->body(first)->center.x, -4.0f));
    CHECK(built && near(built->body(first)->center.y, 3.0f));
    CHECK(built && near(built->body(second)->center.x, -1.5f));
    CHECK(built && near(built->body(second)->center.y, 1.25f));
    CHECK(built && built->body(firstLane)->size.x > 4.5f);
    delete built;
    delete diagram;

    vis::diagram::Config timelineConfig;
    timelineConfig.layout = Layout::Timeline;
    timelineConfig.origin = {-5.0f, 2.0f};
    timelineConfig.nodeSize = {1.0f, 0.8f};
    timelineConfig.nodeGap = 0.4f;
    timelineConfig.timeUnit = 1.5f;
    diagram = Diagram::gen(timelineConfig);
    CHECK(diagram);
    if (!diagram) return;
    Node task;
    NodeSpec taskSpec;
    taskSpec.id = "task";
    taskSpec.kind = NodeKind::Task;
    taskSpec.row = 1;
    taskSpec.start = 2.0f;
    taskSpec.span = 3.0f;
    CHECK(diagram->add(taskSpec, task) == Result::Success);
    built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && near(built->body(task)->size.x, 4.5f));
    CHECK(built && near(built->body(task)->center.x, 0.25f));
    CHECK(built && near(built->body(task)->center.y, 0.8f));
    delete built;
    delete diagram;
}

static void manualCyclesAndKinds()
{
    vis::diagram::Config config;
    config.layout = Layout::Manual;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    Node first;
    Node second;
    Node third;
    NodeSpec spec;
    spec.id = "first";
    spec.manualPosition = true;
    spec.position = {-4.0f, 0.0f};
    spec.kind = NodeKind::State;
    CHECK(diagram->add(spec, first) == Result::Success);
    spec.id = "second";
    spec.position = {0.0f, 0.0f};
    spec.kind = NodeKind::Decision;
    CHECK(diagram->add(spec, second) == Result::Success);
    spec.id = "third";
    spec.position = {4.0f, 0.0f};
    spec.kind = NodeKind::Terminal;
    CHECK(diagram->add(spec, third) == Result::Success);

    Edge firstEdge;
    Edge secondEdge;
    Edge relation;
    auto edge = edgeSpec("first-second", first, second);
    edge.route = Route::Straight;
    CHECK(diagram->add(edge, firstEdge) == Result::Success);
    edge = edgeSpec("second-third", second, third);
    edge.route = Route::Straight;
    edge.kind = EdgeKind::Return;
    CHECK(diagram->add(edge, secondEdge) == Result::Success);
    edge = edgeSpec("third-first", third, first);
    edge.route = Route::Straight;
    edge.kind = EdgeKind::Relation;
    CHECK(diagram->add(edge, relation) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(rounded(), built) == Result::Success);
    CHECK(built && built->object(firstEdge)->type() == Type::Arrow);
    CHECK(built && built->object(secondEdge)->style.dashCount == 2u);
    CHECK(built && built->object(relation)->type() == Type::Line);
    CHECK(built && near(built->body(third)->corner, 0.6f));
    delete built;
    delete diagram;
}

// A corner left negative follows the Theme's roundness of the shorter node side, which is 0 in
// every preset and gives a roundness of 0.1 the former 0.12 at the default node size. Zones round
// 1.5 times as much, and a Terminal is a pill only once corners round. An explicit corner wins.
static void themeCorners()
{
    CHECK(vis::diagram::Config{}.corner < 0.0f);
    auto round = Theme::preset(ThemePreset::ProWhite);
    round.roundness = 0.5f;
    struct Case
    {
        Theme theme;
        Vec2 nodeSize;
        float corner;
        float expected;
    };
    const Vec2 standard = vis::diagram::Config{}.nodeSize;
    const Case cases[] = {
        {Theme::preset(ThemePreset::ProWhite), standard, -1.0f, 0.0f},
        {Theme::preset(ThemePreset::ProBlack), standard, -1.0f, 0.0f},
        {Theme::preset(ThemePreset::AdaptiveVscode), standard, -1.0f, 0.0f},
        {rounded(), standard, -1.0f, 0.12f},
        {rounded(), {2.0f, 3.0f}, -1.0f, 0.2f},
        {round, {2.0f, 1.0f}, -0.5f, 0.5f},
        {Theme::preset(ThemePreset::ProWhite), standard, 0.2f, 0.2f},
        {rounded(), standard, 0.2f, 0.2f},
        {rounded(), standard, 0.0f, 0.0f},
    };
    for (const auto& item : cases) {
        vis::diagram::Config config;
        config.layout = Layout::Manual;
        config.nodeSize = item.nodeSize;
        config.corner = item.corner;
        auto diagram = Diagram::gen(config);
        CHECK(diagram);
        if (!diagram) continue;
        auto plainSpec = nodeSpec("node", "Node");
        plainSpec.manualPosition = true;
        auto terminalSpec = nodeSpec("terminal", "Done");
        terminalSpec.manualPosition = true;
        terminalSpec.position = {6.0f, 0.0f};
        terminalSpec.kind = NodeKind::Terminal;
        Node node;
        Node terminal;
        CHECK(diagram->add(plainSpec, node) == Result::Success);
        CHECK(diagram->add(terminalSpec, terminal) == Result::Success);
        Node members[] = {node, terminal};
        Zone zone;
        CHECK(diagram->add(zoneSpec("scope", nullptr, members, 2u), zone) == Result::Success);

        Built* built = nullptr;
        CHECK(diagram->build(item.theme, built) == Result::Success);
        CHECK(built && built->body(node) && built->body(terminal) && built->body(zone));
        if (built && built->body(node) && built->body(terminal) && built->body(zone)) {
            auto pill = std::fmin(item.nodeSize.x, item.nodeSize.y) * 0.5f;
            CHECK(near(built->body(node)->corner, item.expected));
            CHECK(near(built->body(terminal)->corner, item.expected > 0.0f ? pill : 0.0f));
            CHECK(near(built->body(zone)->corner, item.expected * 1.5f));
            // The receipt keeps the corner as authored.
            CHECK(built->ir()->config().corner == item.corner);
        }
        delete built;

        // A Theme whose roundness is not a number builds nothing.
        auto invalid = item.theme;
        invalid.roundness = std::numeric_limits<float>::quiet_NaN();
        built = nullptr;
        auto result = diagram->build(invalid, built);
        CHECK(item.corner >= 0.0f ? result == Result::Success : result == Result::InvalidArguments);
        CHECK(item.corner >= 0.0f ? built != nullptr : built == nullptr);
        delete built;
        delete diagram;
    }

    vis::diagram::Config infinite;
    infinite.corner = -std::numeric_limits<float>::infinity();
    CHECK(Diagram::gen(infinite) == nullptr);
}

// Structure is ink: every node kind is the Theme's foreground on its background and zones only
// outline. Routes stay gray but for an error and a flowing directed edge, and labels match.
static void restrainedColors()
{
    vis::diagram::Config config;
    config.layout = Layout::Manual;
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const NodeKind kinds[] = {
        NodeKind::Default, NodeKind::State, NodeKind::Decision, NodeKind::Terminal,
        NodeKind::Entity, NodeKind::Cell, NodeKind::Task, NodeKind::Evidence,
    };
    const char* ids[] = {
        "default", "state", "decision", "terminal", "entity", "cell", "task", "evidence",
    };
    Node nodes[8];
    for (auto i = 0u; i < 8u; i++) {
        auto spec = nodeSpec(ids[i], ids[i]);
        spec.manualPosition = true;
        spec.position = {static_cast<float>(i) * 5.0f, 0.0f};
        spec.kind = kinds[i];
        CHECK(diagram->add(spec, nodes[i]) == Result::Success);
    }
    struct RouteCase
    {
        const char* id;
        EdgeKind kind;
        bool flow;
    };
    const RouteCase routes[] = {
        {"directed", EdgeKind::Directed, false},
        {"flowing", EdgeKind::Directed, true},
        {"relation", EdgeKind::Relation, false},
        {"return", EdgeKind::Return, false},
        {"error", EdgeKind::Error, false},
        {"optional", EdgeKind::Optional, false},
        {"flowing-return", EdgeKind::Return, true},
    };
    Edge edges[7];
    for (auto i = 0u; i < 7u; i++) {
        auto spec = edgeSpec(routes[i].id, nodes[i], nodes[i + 1u]);
        spec.label = routes[i].id;
        spec.route = Route::Straight;
        spec.kind = routes[i].kind;
        spec.flow = routes[i].flow;
        CHECK(diagram->add(spec, edges[i]) == Result::Success);
    }
    Node members[] = {nodes[0], nodes[1]};
    Zone zone;
    CHECK(diagram->add(zoneSpec("scope", "Scope", members, 2u), zone) == Result::Success);

    for (auto preset : {ThemePreset::ProWhite, ThemePreset::ProBlack}) {
        auto theme = Theme::preset(preset);
        Built* built = nullptr;
        CHECK(diagram->build(theme, built) == Result::Success);
        if (!built) continue;
        for (auto i = 0u; i < 8u; i++) {
            auto body = built->body(nodes[i]);
            CHECK(body && same(body->style.fill, theme.background));
            CHECK(body && same(body->style.stroke, theme.colors.foreground));
            CHECK(body && near(body->style.width, theme.objectWidth));
            CHECK(built->label(nodes[i])
                  && same(built->label(nodes[i])->style.fill, theme.colors.foreground));
        }
        auto zoneBody = built->body(zone);
        CHECK(zoneBody && zoneBody->style.fill.a == 0u);
        CHECK(zoneBody && same(zoneBody->style.stroke, theme.colors.border));
        CHECK(zoneBody && near(zoneBody->style.width, theme.objectWidth));
        CHECK(built->label(zone) && same(built->label(zone)->style.fill, theme.colors.muted));

        const Color expected[] = {
            theme.colors.muted, theme.colors.accent, theme.colors.border, theme.colors.muted,
            theme.colors.danger, theme.colors.muted, theme.colors.muted,
        };
        for (auto i = 0u; i < 7u; i++) {
            auto route = built->object(edges[i]);
            CHECK(route && same(route->style.stroke, expected[i]));
            CHECK(route && near(route->style.width, theme.objectWidth));
            CHECK(built->label(edges[i])
                  && same(built->label(edges[i])->style.fill, expected[i]));
        }
        delete built;
    }
    delete diagram;
}

// The label of a route keeps the gap of the former 0.12 corner while the corner follows the
// Theme, whatever it rounds; an explicit corner sets the gap as before.
static void labelGapIgnoresTheme()
{
    struct Case
    {
        Theme theme;
        float corner;
        float gap;
    };
    const Case cases[] = {
        {Theme::preset(ThemePreset::ProWhite), -1.0f, 0.24f},
        {rounded(), -1.0f, 0.24f},
        {Theme::preset(ThemePreset::ProWhite), 0.3f, 0.42f},
        {rounded(), 0.0f, 0.16f},
    };
    for (const auto& item : cases) {
        vis::diagram::Config config;
        config.layout = Layout::Manual;
        config.corner = item.corner;
        auto diagram = Diagram::gen(config);
        CHECK(diagram);
        if (!diagram) continue;
        auto sourceSpec = nodeSpec("source", "Source");
        sourceSpec.manualPosition = true;
        auto targetSpec = nodeSpec("target", "Target");
        targetSpec.manualPosition = true;
        targetSpec.position = {6.0f, 0.0f};
        Node source;
        Node target;
        CHECK(diagram->add(sourceSpec, source) == Result::Success);
        CHECK(diagram->add(targetSpec, target) == Result::Success);
        auto spec = edgeSpec("route", source, target);
        spec.label = "payload";
        spec.route = Route::Straight;
        Edge route;
        CHECK(diagram->add(spec, route) == Result::Success);
        Built* built = nullptr;
        CHECK(diagram->build(item.theme, built) == Result::Success);
        CHECK(built && built->label(route));
        if (built && built->label(route)) CHECK(near(built->label(route)->point.y, item.gap));
        delete built;
        delete diagram;
    }
}

static void constraintDiagnostics()
{
    vis::diagram::Config config;
    config.layout = Layout::Manual;
    config.nodeSize = {1.0f, 1.0f};
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    const Vec2 positions[] = {
        {-4.0f, 0.0f}, {4.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 3.0f}, {0.5f, 3.0f},
    };
    const char* ids[] = {"source", "target", "blocker", "first", "second"};
    Node nodes[5];
    for (auto i = 0u; i < 5u; i++) {
        NodeSpec spec;
        spec.id = ids[i];
        spec.manualPosition = true;
        spec.position = positions[i];
        CHECK(diagram->add(spec, nodes[i]) == Result::Success);
    }
    auto spec = edgeSpec("crossing", nodes[0], nodes[1]);
    spec.route = Route::Straight;
    Edge crossing;
    CHECK(diagram->add(spec, crossing) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    ConstraintReport report;
    CHECK(built && validate(*built->ir(), report) == Result::Success);
    CHECK(report.count() == 2u);
    CHECK(built && built->ir()->edgePlacementAt(crossing.index)->route == Route::Straight);
    auto overlap = issue(report, ConstraintCode::NodeOverlap);
    auto route = issue(report, ConstraintCode::RouteCrossesNode);
    CHECK(overlap && overlap->node.index == nodes[3].index
          && overlap->relatedNode.index == nodes[4].index);
    CHECK(route && route->edge.index == crossing.index
          && route->node.index == nodes[2].index);
    CHECK(issue(report, ConstraintCode::EdgeEndpointDetached) == nullptr);
    CHECK(issue(report, ConstraintCode::EdgePortMismatch) == nullptr);
    CHECK(issue(report, ConstraintCode::ZoneExcludesMember) == nullptr);
    CHECK(issue(report, ConstraintCode::FullWidthZoneMismatch) == nullptr);
    delete built;
    delete diagram;
}

static void endpointRouteDiagnostics()
{
    vis::diagram::Config config;
    config.layout = Layout::Manual;
    config.nodeSize = {2.0f, 2.0f};
    auto diagram = Diagram::gen(config);
    CHECK(diagram);
    if (!diagram) return;

    auto sourceSpec = nodeSpec("source", "Source");
    sourceSpec.manualPosition = true;
    sourceSpec.position = {0.0f, 0.0f};
    Node source;
    CHECK(diagram->add(sourceSpec, source) == Result::Success);
    auto targetSpec = nodeSpec("target", "Target");
    targetSpec.manualPosition = true;
    targetSpec.position = {4.0f, 4.0f};
    Node target;
    CHECK(diagram->add(targetSpec, target) == Result::Success);

    Vec2 waypoints[] = {{0.0f, 0.0f}, {0.0f, 4.0f}};
    auto spec = edgeSpec("foldback", source, target);
    spec.fromPort = Port::Right;
    spec.toPort = Port::Left;
    spec.route = Route::Orthogonal;
    spec.waypoints = waypoints;
    spec.waypointCount = 2u;
    Edge foldback;
    CHECK(diagram->add(spec, foldback) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    ConstraintReport report;
    CHECK(built && validate(*built->ir(), report) == Result::Success);
    CHECK(report.count() == 2u);
    auto route = issue(report, ConstraintCode::RouteCrossesNode);
    auto direction = issue(report, ConstraintCode::EdgeEndpointDirection);
    CHECK(route && route->edge.index == foldback.index
          && route->node.index == source.index);
    CHECK(direction && direction->edge.index == foldback.index
          && direction->node.index == source.index
          && direction->endpoint == EdgeEndpoint::From
          && direction->port == Port::Right);
    delete built;
    delete diagram;
}

static void rankBoundariesAndFiniteGeometry()
{
    auto diagram = Diagram::gen();
    CHECK(diagram);
    if (!diagram) return;
    Node high;
    Node next;
    NodeSpec highSpec;
    highSpec.id = "rank-254";
    highSpec.rank = 254;
    CHECK(diagram->add(highSpec, high) == Result::Success);
    CHECK(diagram->add(nodeSpec("rank-255", nullptr), next) == Result::Success);
    Edge edge;
    CHECK(diagram->add(edgeSpec("last-rank", high, next), edge) == Result::Success);
    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir()->nodePlacementAt(high.index)->rank == 254u
          && built->ir()->nodePlacementAt(next.index)->rank == 255u);
    CHECK(built && built->ir()->edgeAt(edge.index)->fromPort == Port::Auto
          && built->ir()->edgeAt(edge.index)->toPort == Port::Auto
          && built->ir()->edgeAt(edge.index)->route == Route::Auto);
    CHECK(built && built->ir()->edgePlacementAt(edge.index)->fromPort == Port::Right
          && built->ir()->edgePlacementAt(edge.index)->toPort == Port::Left
          && built->ir()->edgePlacementAt(edge.index)->route == Route::Straight);
    delete built;
    delete diagram;

    diagram = Diagram::gen();
    CHECK(diagram);
    if (!diagram) return;
    highSpec.id = "rank-255-source";
    highSpec.rank = 255;
    CHECK(diagram->add(highSpec, high) == Result::Success);
    CHECK(diagram->add(nodeSpec("overflow", nullptr), next) == Result::Success);
    CHECK(diagram->add(edgeSpec("overflow-edge", high, next), edge) == Result::Success);
    built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::InvalidArguments);
    CHECK(!built);
    delete diagram;

    vis::diagram::Config timeline;
    timeline.layout = Layout::Timeline;
    timeline.timeUnit = std::numeric_limits<float>::max();
    diagram = Diagram::gen(timeline);
    CHECK(diagram);
    if (!diagram) return;
    NodeSpec taskSpec;
    taskSpec.id = "non-finite-derived";
    taskSpec.row = 0;
    taskSpec.start = std::numeric_limits<float>::max();
    taskSpec.span = std::numeric_limits<float>::max();
    Node task;
    CHECK(diagram->add(taskSpec, task) == Result::Success);
    built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::InvalidArguments);
    CHECK(!built);
    delete diagram;
}

static void invalidGraphs()
{
    vis::diagram::Config invalidConfig;
    invalidConfig.id = "bad:id";
    CHECK(Diagram::gen(invalidConfig) == nullptr);

    vis::diagram::Config manualConfig;
    manualConfig.layout = Layout::Manual;
    auto manualDiagram = Diagram::gen(manualConfig);
    CHECK(manualDiagram);
    if (manualDiagram) {
        NodeSpec missingPosition;
        missingPosition.id = "missing-position";
        Node node;
        CHECK(manualDiagram->add(missingPosition, node) == Result::InvalidArguments);
    }
    delete manualDiagram;

    vis::diagram::Config gridConfig;
    gridConfig.layout = Layout::Grid;
    auto gridDiagram = Diagram::gen(gridConfig);
    CHECK(gridDiagram);
    if (gridDiagram) {
        NodeSpec cell;
        cell.id = "first-cell";
        cell.row = 0;
        cell.column = 0;
        Node firstCell;
        CHECK(gridDiagram->add(cell, firstCell) == Result::Success);
        cell.id = "duplicate-cell";
        Node duplicateCell;
        CHECK(gridDiagram->add(cell, duplicateCell) == Result::InvalidArguments);
    }
    delete gridDiagram;

    vis::diagram::Config timelineConfig;
    timelineConfig.layout = Layout::Timeline;
    auto timelineDiagram = Diagram::gen(timelineConfig);
    CHECK(timelineDiagram);
    if (timelineDiagram) {
        NodeSpec zeroSpan;
        zeroSpan.id = "zero-span";
        zeroSpan.row = 0;
        Node task;
        CHECK(timelineDiagram->add(zeroSpan, task) == Result::InvalidArguments);
    }
    delete timelineDiagram;

    auto diagram = Diagram::gen();
    CHECK(diagram);
    if (!diagram) return;
    Node first;
    Node second;
    CHECK(diagram->add(nodeSpec("same", "First"), first) == Result::Success);
    Node unchanged;
    CHECK(diagram->add(nodeSpec("same", "Duplicate"), unchanged)
          == Result::InvalidArguments);
    CHECK(!unchanged);
    // The duplicate took no place, so the next node is the second.
    CHECK(diagram->add(nodeSpec("second", "Second"), second) == Result::Success && second.index == 1u);
    Node duplicated[] = {first, first};
    Zone zone;
    CHECK(diagram->add(zoneSpec("duplicate-zone", nullptr, duplicated, 2u), zone)
          == Result::InvalidArguments);

    Edge forward;
    Edge backward;
    CHECK(diagram->add(edgeSpec("forward", first, second), forward) == Result::Success);
    CHECK(diagram->add(edgeSpec("backward", second, first), backward) == Result::Success);
    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::InvalidArguments);
    CHECK(!built);
    delete diagram;

    diagram = Diagram::gen();
    CHECK(diagram);
    if (!diagram) return;
    Node source;
    Node target;
    CHECK(diagram->add(nodeSpec("source", "Source"), source) == Result::Success);
    CHECK(diagram->add(nodeSpec("target", "Target"), target) == Result::Success);
    Vec2 diagonal[] = {{2.0f, 1.0f}};
    EdgeSpec invalidRoute;
    invalidRoute.id = "diagonal";
    invalidRoute.from = source;
    invalidRoute.to = target;
    invalidRoute.route = Route::Orthogonal;
    invalidRoute.waypoints = diagonal;
    invalidRoute.waypointCount = 1u;
    Edge invalid;
    CHECK(diagram->add(invalidRoute, invalid) == Result::Success);
    built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built)
          == Result::InvalidArguments);
    CHECK(!built);
    delete diagram;
}

static void sampledPhysicalConstraints()
{
    vis::Config childConfig;
    childConfig.width = 400u;
    childConfig.height = 450u;
    childConfig.camera.orthoHeight = 9.0f;
    auto child = Scene::gen(childConfig);

    vis::Config rootConfig;
    rootConfig.width = 800u;
    rootConfig.height = 450u;
    auto root = Scene::gen(rootConfig);

    vis::diagram::Config config;
    config.id = "sampled";
    config.layout = Layout::Manual;
    config.nodeSize = {1.0f, 1.0f};
    auto diagram = Diagram::gen(config);
    CHECK(child && root && diagram);
    if (!child || !root || !diagram) {
        delete child;
        delete root;
        delete diagram;
        return;
    }

    auto sourceSpec = nodeSpec("source", "S");
    sourceSpec.manualPosition = true;
    sourceSpec.position = {-3.0f, 0.0f};
    auto targetSpec = nodeSpec("target", "T");
    targetSpec.manualPosition = true;
    targetSpec.position = {3.0f, 0.0f};
    auto blockerSpec = nodeSpec("blocker", "B");
    blockerSpec.manualPosition = true;
    blockerSpec.position = {0.0f, 2.0f};
    Node source;
    Node target;
    Node blocker;
    CHECK(diagram->add(sourceSpec, source) == Result::Success);
    CHECK(diagram->add(targetSpec, target) == Result::Success);
    CHECK(diagram->add(blockerSpec, blocker) == Result::Success);
    auto routeSpec = edgeSpec("route", source, target);
    routeSpec.label = "owned route label";
    routeSpec.route = Route::Straight;
    Edge route;
    CHECK(diagram->add(routeSpec, route) == Result::Success);

    Built* built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && built->ir());
    if (!built) {
        delete root;
        delete child;
        delete diagram;
        return;
    }
    auto receipt = built->ir();
    auto blockerObject = built->object(blocker);
    auto diagramRoot = built->release();
    delete built;
    CHECK(child->add(diagramRoot) == Result::Success);
    CHECK(child->play(Animation::shift(blockerObject, {0.0f, -2.0f, 0.0f}),
                      1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(child->play(Animation::shift(diagramRoot, {10.0f, 0.0f, 0.0f}),
                      1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(root->viewport(child, {0.5f, 0.0f, 0.5f, 1.0f}) == Result::Success);

    auto renderer = Renderer::gen();
    CHECK(renderer && renderer->font(VIS_TEST_FONT) == Result::Success);
    CHECK(renderer && renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
    if (renderer) {
        LayoutReport layout;
        PhysicalConstraintReport physical;
        CHECK(renderer->layout(root, 0.0f, layout) == Result::Success);
        CHECK(layout.pathCount() > 0u && layout.pathAt(layout.pathCount()) == nullptr);
        auto routeObject = UINT32_MAX;
        for (auto i = 0u; i < layout.objectCount(); i++) {
            auto object = layout.objectAt(i);
            if (object && object->object == receipt->object(route)) routeObject = i;
        }
        auto routeVisual = UINT32_MAX;
        for (auto i = 0u; i < layout.visualCount(); i++) {
            auto visual = layout.visualAt(i);
            if (visual && visual->object == routeObject) routeVisual = i;
        }
        auto shaft = false;
        auto marker = false;
        for (auto i = 0u; i < layout.pathCount(); i++) {
            auto path = layout.pathAt(i);
            if (!path || path->visual != routeVisual) continue;
            shaft |= path->stroked && !path->closed && path->count >= 2u;
            marker |= path->filled && path->closed && path->count == 3u;
        }
        CHECK(routeObject != UINT32_MAX && routeVisual != UINT32_MAX && shaft && marker);
        CHECK(validatePhysical(*receipt, layout, "root/viewport:0", 2.0f, 4.0f,
                               physical) == Result::Success);
        CHECK(issue(physical, PhysicalConstraintCode::RouteNodeCollision) == nullptr);
        CHECK(issue(physical, PhysicalConstraintCode::RouteLabelCollision) == nullptr);

        CHECK(renderer->layout(root, 1.0f, layout) == Result::Success);
        CHECK(validatePhysical(*receipt, layout, "root/viewport:0", 2.0f, 4.0f,
                               physical) == Result::Success);
        auto nodeCollision = issue(physical, PhysicalConstraintCode::RouteNodeCollision);
        CHECK(nodeCollision && nodeCollision->edge.index == route.index
              && nodeCollision->node.index == blocker.index
              && nodeCollision->relatedKind == PhysicalEntityKind::NodeBody
              && nodeCollision->actual < nodeCollision->required);
        auto labelCollision = issue(physical, PhysicalConstraintCode::RouteLabelCollision);
        CHECK(labelCollision && labelCollision->edge.index == route.index
              && labelCollision->node.index == blocker.index
              && labelCollision->relatedKind == PhysicalEntityKind::NodeLabel
              && labelCollision->actual < labelCollision->required);

        CHECK(renderer->layout(root, 2.0f, layout) == Result::Success);
        CHECK(validatePhysical(*receipt, layout, "root/viewport:0", 2.0f, 4.0f,
                               physical) == Result::Success);
        auto canvas = issue(physical, PhysicalConstraintCode::RouteCanvasOverflow);
        CHECK(canvas && canvas->edge.index == route.index
              && canvas->actual < canvas->required
              && canvas->routeBounds.x > canvas->relatedBounds.x
                                       + canvas->relatedBounds.width);
        CHECK(physical.issueAt(physical.count()) == nullptr);
        CHECK(validatePhysical(*receipt, layout, nullptr, 2.0f, 4.0f, physical)
              == Result::InvalidArguments);
        CHECK(validatePhysical(*receipt, layout, "root/viewport:0", -1.0f, 4.0f,
                               physical) == Result::InvalidArguments);
    }
    delete renderer;
    delete root;
    delete diagram;

    vis::Config sceneConfig;
    sceneConfig.width = 800u;
    sceneConfig.height = 450u;
    sceneConfig.camera.orthoHeight = 9.0f;
    auto scene = Scene::gen(sceneConfig);
    config.id = "crossed-routes";
    diagram = Diagram::gen(config);
    CHECK(scene && diagram);
    if (!scene || !diagram) {
        delete scene;
        delete diagram;
        return;
    }
    Vec2 positions[] = {{-3.0f, 0.0f}, {3.0f, 0.0f},
                        {0.0f, 3.0f}, {0.0f, -3.0f}};
    const char* ids[] = {"left", "right", "top", "bottom"};
    Node nodes[4];
    for (auto i = 0u; i < 4u; i++) {
        auto spec = nodeSpec(ids[i], ids[i]);
        spec.manualPosition = true;
        spec.position = positions[i];
        CHECK(diagram->add(spec, nodes[i]) == Result::Success);
    }
    Edge horizontal;
    Edge vertical;
    Edge sharedTerminal;
    Edge duplicate;
    auto horizontalSpec = edgeSpec("horizontal", nodes[0], nodes[1]);
    horizontalSpec.route = Route::Straight;
    auto verticalSpec = edgeSpec("vertical", nodes[2], nodes[3]);
    verticalSpec.route = Route::Straight;
    verticalSpec.fromPort = Port::Bottom;
    verticalSpec.toPort = Port::Top;
    CHECK(diagram->add(horizontalSpec, horizontal) == Result::Success);
    CHECK(diagram->add(verticalSpec, vertical) == Result::Success);
    auto sharedTerminalSpec = edgeSpec("shared-terminal", nodes[0], nodes[2]);
    sharedTerminalSpec.route = Route::Straight;
    sharedTerminalSpec.toPort = Port::Bottom;
    CHECK(diagram->add(sharedTerminalSpec, sharedTerminal) == Result::Success);
    auto duplicateSpec = edgeSpec("duplicate", nodes[0], nodes[1]);
    Vec2 duplicateWaypoints[] = {{-1.5f, 0.175f}, {-1.5f, -1.0f},
                                 {1.5f, -1.0f}, {1.5f, 0.1166667f}};
    duplicateSpec.route = Route::Orthogonal;
    duplicateSpec.fromPort = Port::Right;
    duplicateSpec.toPort = Port::Left;
    duplicateSpec.waypoints = duplicateWaypoints;
    duplicateSpec.waypointCount = 4u;
    duplicateSpec.flow = true;
    CHECK(diagram->add(duplicateSpec, duplicate) == Result::Success);
    built = nullptr;
    CHECK(diagram->build(Theme::preset(ThemePreset::ProWhite), built) == Result::Success);
    CHECK(built && scene->add(built->release()) == Result::Success);
    if (built) {
        renderer = Renderer::gen();
        CHECK(renderer && renderer->font(VIS_TEST_FONT) == Result::Success);
        CHECK(renderer && renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
        if (renderer) {
            LayoutReport layout;
            PhysicalConstraintReport physical;
            CHECK(renderer->layout(scene, 0.0f, layout) == Result::Success);
            CHECK(validatePhysical(*built->ir(), layout, "root", 2.0f, 4.0f,
                                   physical) == Result::Success);
            auto collision = issue(physical, PhysicalConstraintCode::RouteRouteCollision);
            CHECK(collision && collision->edge.index == horizontal.index
                  && collision->relatedEdge.index == vertical.index
                  && collision->relatedKind == PhysicalEntityKind::EdgeRoute);
            CHECK(routeIssue(physical, horizontal, sharedTerminal) == nullptr);
            CHECK(routeIssue(physical, vertical, sharedTerminal) == nullptr);
            CHECK(routeIssue(physical, duplicate, sharedTerminal) == nullptr);
            CHECK(routeIssue(physical, horizontal, duplicate) != nullptr);
        }
        delete renderer;
        delete built;
    }
    delete scene;
    delete diagram;
}

int main()
{
    aggregateCompatibility();
    ownershipAndBands();
    receiptLifetimeAndSemantics();
    rankedLayouts();
    rankedPinnedReflow(Direction::LeftToRight);
    rankedPinnedReflow(Direction::TopToBottom);
    rankedPinnedOverlapRemainsDiagnosed();
    rankedExtremeFiniteReflow();
    rankedFloatRoundingReflow();
    rankedRoundingBreakpointPreservesRigidBlock();
    rankedInitialFloatOverlapFailsClosed();
    rankedCanonicalFloatOverlapFailsClosed();
    rankedCanonicalPrimaryOverlapReflows();
    rankedCrossRankFloatOverlapFailsClosed();
    rankedChainedFloatIntervals();
    rankedPerNodeRoundingBreakpointReflows();
    rankedPairwiseRoundingBreakpointReflows();
    rankedQuantizedGapReflows();
    rankedMultiBreakpointReflows();
    rankedAbsorbedBreakpointReflows();
    rankedInterleavedBreakpointReflows();
    rankedUnrelatedFineUlpEventsDoNotExhaustReflow();
    rankedRepairFineUlpEventsDoNotExhaustReflow();
    rankedMaximumPinReflow();
    automaticObstacleRouting(Direction::LeftToRight, Route::Auto);
    automaticObstacleRouting(Direction::TopToBottom, Route::Auto);
    automaticObstacleRouting(Direction::LeftToRight, Route::Orthogonal);
    automaticTightRouting(Direction::LeftToRight, 2.0f);
    automaticTightRouting(Direction::TopToBottom, 2.0f);
    automaticTightRouting(Direction::LeftToRight, 2.5f);
    automaticTightRouting(Direction::TopToBottom, 2.5f);
    manualRoute();
    edgeLabelsClearRoutes();
    gridAndTimelineLayouts();
    manualCyclesAndKinds();
    themeCorners();
    restrainedColors();
    labelGapIgnoresTheme();
    constraintDiagnostics();
    endpointRouteDiagnostics();
    rankBoundariesAndFiniteGeometry();
    invalidGraphs();
    sampledPhysicalConstraints();
    return report();
}
