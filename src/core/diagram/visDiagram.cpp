// The diagram module: nodes, edges and zones laid out by rank, grid, timeline or by hand, and
// built into scene objects with a constraint report.
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>

#include "visDiagram.h"
#include "visDiagramGeometry.h"
#include "../common/visFinite.h"
#include "../common/visString.h"

namespace vis::diagram
{

using namespace detail;

namespace
{

constexpr uint32_t LabelLimit = 1023u;

struct NodeView
{
    Object* object = nullptr;
    Rectangle* body = nullptr;
    Text* label = nullptr;
    Text* detail = nullptr;
};

struct EdgeView
{
    Object* object = nullptr;
    Text* label = nullptr;
};

struct ZoneView
{
    Object* object = nullptr;
    Rectangle* body = nullptr;
    Text* label = nullptr;
};

static uint32_t _length(const char* value, uint32_t limit) noexcept
{
    if (!value) return 0u;
    auto length = 0u;
    while (length <= limit && value[length]) length++;
    return length;
}

static bool _validId(const char* value) noexcept
{
    auto length = _length(value, Diagram::IdLimit);
    if (!length || length > Diagram::IdLimit) return false;
    for (auto i = 0u; i < length; i++) {
        auto c = value[i];
        if (c == ':' || static_cast<unsigned char>(c) <= 0x20u) return false;
    }
    return true;
}

static bool _validText(const char* value, bool required = false) noexcept
{
    if (!value) return !required;
    auto length = _length(value, LabelLimit);
    return length <= LabelLimit && (!required || length > 0u);
}

static bool _equal(const char* lhs, const char* rhs) noexcept
{
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

// The bytes a copied string takes, terminator included.
static size_t _bytes(const char* text) noexcept
{
    return text ? std::strlen(text) + 1u : 0u;
}

// Frees the strings and arrays that a copied spec owns.
static void _release(NodeSpec& spec) noexcept
{
    delete[] spec.id;
    delete[] spec.label;
    delete[] spec.detail;
}

static void _release(EdgeSpec& spec) noexcept
{
    delete[] spec.id;
    delete[] spec.label;
    delete[] spec.waypoints;
}

static void _release(ZoneSpec& spec) noexcept
{
    delete[] spec.id;
    delete[] spec.label;
    delete[] spec.members;
}

template<typename Spec>
static void _release(Spec* specs, uint32_t count) noexcept
{
    for (auto i = 0u; specs && i < count; i++) _release(specs[i]);
    delete[] specs;
}

// Copies spec into target with strings and arrays of its own: a missing label takes the id,
// and an empty detail or edge label is dropped. Out of memory leaves target empty.
static bool _copy(const NodeSpec& spec, NodeSpec& target) noexcept
{
    target = spec;
    target.id = _duplicate(spec.id);
    target.label = _duplicate(spec.label ? spec.label : spec.id);
    target.detail = spec.detail && spec.detail[0] ? _duplicate(spec.detail) : nullptr;
    if (target.id && target.label && (target.detail || !spec.detail || !spec.detail[0])) {
        return true;
    }
    _release(target);
    target = {};
    return false;
}

static bool _copy(const EdgeSpec& spec, EdgeSpec& target) noexcept
{
    target = spec;
    target.id = _duplicate(spec.id);
    target.label = spec.label && spec.label[0] ? _duplicate(spec.label) : nullptr;
    auto waypoints = spec.waypointCount ? new (std::nothrow) Vec2[spec.waypointCount] : nullptr;
    for (auto i = 0u; waypoints && i < spec.waypointCount; i++) waypoints[i] = spec.waypoints[i];
    target.waypoints = waypoints;
    if (target.id && (target.label || !spec.label || !spec.label[0])
        && (waypoints || !spec.waypointCount)) {
        return true;
    }
    _release(target);
    target = {};
    return false;
}

static bool _copy(const ZoneSpec& spec, ZoneSpec& target) noexcept
{
    target = spec;
    target.id = _duplicate(spec.id);
    target.label = _duplicate(spec.label ? spec.label : spec.id);
    auto members = new (std::nothrow) Node[spec.memberCount];
    for (auto i = 0u; members && i < spec.memberCount; i++) members[i] = spec.members[i];
    target.members = members;
    if (target.id && target.label && members) return true;
    _release(target);
    target = {};
    return false;
}

static bool _valid(const Config& config) noexcept
{
    auto layout = static_cast<uint8_t>(config.layout);
    auto direction = static_cast<uint8_t>(config.direction);
    return _validId(config.id) && layout <= static_cast<uint8_t>(Layout::Timeline)
           && direction <= static_cast<uint8_t>(Direction::TopToBottom)
           && _finite(config.origin) && _finite(config.nodeSize)
           && config.nodeSize.x > 0.0f && config.nodeSize.y > 0.0f
           && _finite(config.rankGap) && config.rankGap > 0.0f
           && _finite(config.nodeGap) && config.nodeGap >= 0.0f
           && _finite(config.timeUnit) && config.timeUnit > 0.0f
           && _finite(config.zonePadding) && config.zonePadding > 0.0f
           && _finite(config.routeWidth) && config.routeWidth > 0.0f
           && _finite(config.arrowLength) && config.arrowLength > 0.0f
           && _finite(config.arrowWidth) && config.arrowWidth > config.routeWidth
           && _finite(config.corner);
}

static bool _valid(Port port) noexcept
{
    return static_cast<uint8_t>(port) <= static_cast<uint8_t>(Port::Bottom);
}

static bool _valid(Route route) noexcept
{
    return static_cast<uint8_t>(route) <= static_cast<uint8_t>(Route::Orthogonal);
}

static bool _valid(NodeKind kind) noexcept
{
    return static_cast<uint8_t>(kind) <= static_cast<uint8_t>(NodeKind::Evidence);
}

static bool _valid(EdgeKind kind) noexcept
{
    return static_cast<uint8_t>(kind) <= static_cast<uint8_t>(EdgeKind::Optional);
}

static Result _tag(Object* object, const char* prefix, const char* kind,
                   const char* id = nullptr, const char* suffix = nullptr) noexcept
{
    if (!object) return Result::InvalidArguments;
    char value[256];
    auto count = id ? std::snprintf(value, sizeof(value), "%s:%s:%s%s%s", prefix, kind, id,
                                    suffix ? ":" : "", suffix ? suffix : "")
                    : std::snprintf(value, sizeof(value), "%s:%s%s%s", prefix, kind,
                                    suffix ? ":" : "", suffix ? suffix : "");
    if (count <= 0 || static_cast<size_t>(count) >= sizeof(value)) {
        return Result::InvalidArguments;
    }
    object->tag(value);
    return object->tag() && std::strcmp(object->tag(), value) == 0
               ? Result::Success
               : Result::OutOfMemory;
}

// Routes are quiet structure; only errors and flowing directed edges take a hue.
static Color _edgeColor(const Theme& theme, const EdgeRecord& edge) noexcept
{
    switch (edge.kind) {
        case EdgeKind::Relation: return theme.colors.border;
        case EdgeKind::Return: return theme.colors.muted;
        case EdgeKind::Error: return theme.colors.danger;
        case EdgeKind::Optional: return theme.colors.muted;
        case EdgeKind::Directed: return edge.flow ? theme.colors.accent : theme.colors.muted;
    }
    return theme.colors.muted;
}

}  // namespace

struct IR::Impl
{
    Config config;
    char* prefix = nullptr;
    NodeSpec* nodes = nullptr;
    EdgeSpec* edges = nullptr;
    ZoneSpec* zones = nullptr;
    NodePlacement* nodePlacements = nullptr;
    EdgePlacement* edgePlacements = nullptr;
    ZonePlacement* zonePlacements = nullptr;
    NodeView* nodeViews = nullptr;
    EdgeView* edgeViews = nullptr;
    ZoneView* zoneViews = nullptr;
    size_t bytes = sizeof(IR) + sizeof(Impl);
    uint32_t nodeCount = 0u;
    uint32_t edgeCount = 0u;
    uint32_t zoneCount = 0u;

    ~Impl()
    {
        _release(nodes, nodeCount);
        _release(edges, edgeCount);
        _release(zones, zoneCount);
        for (auto i = 0u; edgePlacements && i < edgeCount; i++) {
            delete[] edgePlacements[i].centerline;
        }
        delete[] nodePlacements;
        delete[] edgePlacements;
        delete[] zonePlacements;
        delete[] nodeViews;
        delete[] edgeViews;
        delete[] zoneViews;
        delete[] prefix;
    }

    // The objects built for a node, edge or zone; all null when the index is out of range.
    NodeView view(Node node) const noexcept
    {
        return node.index < nodeCount ? nodeViews[node.index] : NodeView{};
    }

    EdgeView view(Edge edge) const noexcept
    {
        return edge.index < edgeCount ? edgeViews[edge.index] : EdgeView{};
    }

    ZoneView view(Zone zone) const noexcept
    {
        return zone.index < zoneCount ? zoneViews[zone.index] : ZoneView{};
    }

    Result copy(const Config& sourceConfig, const NodeRecord* sourceNodes,
                uint32_t sourceNodeCount, const EdgeRecord* sourceEdges,
                uint32_t sourceEdgeCount, const ZoneRecord* sourceZones,
                uint32_t sourceZoneCount, const NodeLayout* layouts,
                const EdgeLayout* edgeLayouts, const ZoneLayout* zoneLayouts) noexcept
    {
        prefix = _duplicate(sourceConfig.id);
        if (!prefix) return Result::OutOfMemory;
        config = sourceConfig;
        config.id = prefix;
        bytes += _bytes(prefix);
        nodeCount = sourceNodeCount;
        edgeCount = sourceEdgeCount;
        zoneCount = sourceZoneCount;
        nodes = new (std::nothrow) NodeSpec[nodeCount]{};
        nodePlacements = new (std::nothrow) NodePlacement[nodeCount]{};
        nodeViews = new (std::nothrow) NodeView[nodeCount]{};
        if (edgeCount) {
            edges = new (std::nothrow) EdgeSpec[edgeCount]{};
            edgePlacements = new (std::nothrow) EdgePlacement[edgeCount]{};
            edgeViews = new (std::nothrow) EdgeView[edgeCount]{};
        }
        if (zoneCount) {
            zones = new (std::nothrow) ZoneSpec[zoneCount]{};
            zonePlacements = new (std::nothrow) ZonePlacement[zoneCount]{};
            zoneViews = new (std::nothrow) ZoneView[zoneCount]{};
        }
        if (!nodes || !nodePlacements || !nodeViews
            || (edgeCount && (!edges || !edgePlacements || !edgeViews))
            || (zoneCount && (!zones || !zonePlacements || !zoneViews))) {
            return Result::OutOfMemory;
        }
        bytes += sizeof(NodeSpec) * nodeCount + sizeof(NodePlacement) * nodeCount
                 + sizeof(NodeView) * nodeCount + sizeof(EdgeSpec) * edgeCount
                 + sizeof(EdgePlacement) * edgeCount + sizeof(EdgeView) * edgeCount
                 + sizeof(ZoneSpec) * zoneCount + sizeof(ZonePlacement) * zoneCount
                 + sizeof(ZoneView) * zoneCount;

        for (auto i = 0u; i < nodeCount; i++) {
            auto& target = nodes[i];
            if (!_copy(sourceNodes[i], target)) return Result::OutOfMemory;
            nodePlacements[i] = {layouts[i].center, layouts[i].size, layouts[i].rank};
            bytes += _bytes(target.id) + _bytes(target.label) + _bytes(target.detail);
        }

        for (auto i = 0u; i < edgeCount; i++) {
            auto& source = sourceEdges[i];
            auto& target = edges[i];
            if (!_copy(source, target)) return Result::OutOfMemory;

            auto route = source.route;
            if (route == Route::Auto) {
                route = source.waypointCount
                            || (!_near(edgeLayouts[i].from.x, edgeLayouts[i].to.x)
                                && !_near(edgeLayouts[i].from.y, edgeLayouts[i].to.y))
                            ? Route::Orthogonal
                            : Route::Straight;
            }
            Vec2 centerline[Diagram::WaypointLimit + 4u];
            auto centerlineCount = detail::centerline(sourceConfig, source, edgeLayouts[i], route,
                                               layouts, nodeCount, centerline);
            if (centerlineCount < 2u
                || (route == Route::Orthogonal
                    && !axisAligned(centerline, centerlineCount))) {
                return Result::InvalidArguments;
            }
            for (auto j = 0u; j < centerlineCount; j++) {
                if (!_finite(centerline[j])) return Result::InvalidArguments;
                if (!j) continue;
                auto length = (centerline[j] - centerline[j - 1u]).length();
                if (!_finite(length) || length <= Epsilon) {
                    return Result::InvalidArguments;
                }
            }
            auto receiptCenterline = new (std::nothrow) Vec2[centerlineCount];
            if (!receiptCenterline) return Result::OutOfMemory;
            for (auto j = 0u; j < centerlineCount; j++) {
                receiptCenterline[j] = centerline[j];
            }
            auto& placement = edgePlacements[i];
            placement.centerline = receiptCenterline;
            placement.centerlineCount = centerlineCount;
            placement.fromPort = edgeLayouts[i].fromPort;
            placement.toPort = edgeLayouts[i].toPort;
            placement.route = route;
            placement.hasLabelPoint = source.label != nullptr;
            if (placement.hasLabelPoint) {
                placement.labelPoint = labelPoint(sourceConfig, centerline, centerlineCount);
                if (!_finite(placement.labelPoint)) return Result::InvalidArguments;
            }
            bytes += _bytes(target.id) + _bytes(target.label)
                     + sizeof(Vec2) * (target.waypointCount + centerlineCount);
        }

        for (auto i = 0u; i < zoneCount; i++) {
            auto& target = zones[i];
            if (!_copy(sourceZones[i], target)) return Result::OutOfMemory;
            zonePlacements[i] = {zoneLayouts[i].center, zoneLayouts[i].size};
            bytes += _bytes(target.id) + _bytes(target.label)
                     + sizeof(Node) * target.memberCount;
        }
        return Result::Success;
    }
};

struct ConstraintReport::Impl : IssueList<ConstraintIssue>
{
};

namespace receipt_detail
{

static DiagramRoot* registry = nullptr;
static std::atomic_flag registryLock = ATOMIC_FLAG_INIT;

struct RegistryGuard
{
    RegistryGuard() noexcept
    {
        while (registryLock.test_and_set(std::memory_order_acquire)) {}
    }

    ~RegistryGuard()
    {
        registryLock.clear(std::memory_order_release);
    }
};

struct DiagramRoot final : Group
{
    IR* receipt = nullptr;
    DiagramRoot* next = nullptr;

    explicit DiagramRoot(IR* receipt) noexcept : receipt(receipt)
    {
        RegistryGuard guard;
        next = registry;
        registry = this;
    }

    ~DiagramRoot() override
    {
        {
            RegistryGuard guard;
            auto link = &registry;
            while (*link && *link != this) link = &((*link)->next);
            if (*link) *link = next;
        }
        delete receipt;
    }
};

}  // namespace receipt_detail

namespace
{

static bool _contains(const NodePlacement& container, const NodePlacement& content) noexcept
{
    auto containerMin = container.center - container.size * 0.5f;
    auto containerMax = container.center + container.size * 0.5f;
    auto contentMin = content.center - content.size * 0.5f;
    auto contentMax = content.center + content.size * 0.5f;
    return contentMin.x >= containerMin.x - Epsilon
           && contentMin.y >= containerMin.y - Epsilon
           && contentMax.x <= containerMax.x + Epsilon
           && contentMax.y <= containerMax.y + Epsilon;
}

static bool _contains(const ZonePlacement& container, const NodePlacement& content) noexcept
{
    return _contains({container.center, container.size, 0u}, content);
}

static bool _overlap(const NodePlacement& first, const NodePlacement& second) noexcept
{
    return _boundsOverlap(first.center, first.size, second.center, second.size);
}

static bool _attached(const NodePlacement& node, const Vec2& point, Port port) noexcept
{
    auto half = node.size * 0.5f;
    switch (port) {
        case Port::Left:
            return _near(point.x, node.center.x - half.x)
                   && point.y >= node.center.y - half.y - Epsilon
                   && point.y <= node.center.y + half.y + Epsilon;
        case Port::Right:
            return _near(point.x, node.center.x + half.x)
                   && point.y >= node.center.y - half.y - Epsilon
                   && point.y <= node.center.y + half.y + Epsilon;
        case Port::Top:
            return _near(point.y, node.center.y + half.y)
                   && point.x >= node.center.x - half.x - Epsilon
                   && point.x <= node.center.x + half.x + Epsilon;
        case Port::Bottom:
            return _near(point.y, node.center.y - half.y)
                   && point.x >= node.center.x - half.x - Epsilon
                   && point.x <= node.center.x + half.x + Epsilon;
        case Port::Auto: break;
    }
    return false;
}

// Deletes the objects a failed step made but did not attach, children before their group.
static void _discard(std::initializer_list<Object*> objects) noexcept
{
    for (auto object : objects) {
        if (object && !object->parent()) delete object;
    }
}

// The object that draws an edge along its centerline: an Arrow or a Line when straight, a
// DirectedRoute or a Plot when bent and dashed or undirected, all stroked, and otherwise a
// filled Path that ends in its arrowhead. Null when the edge cannot be drawn.
static Object* _route(const Config& config, const Theme& theme, Color color, bool directed,
                      bool dashed, const EdgePlacement& placement) noexcept
{
    auto line = placement.centerline;
    auto count = placement.centerlineCount;
    Object* object = nullptr;
    if (placement.route == Route::Straight) {
        Vec3 from = {line[0].x, line[0].y, 0.0f};
        Vec3 to = {line[1].x, line[1].y, 0.0f};
        if (directed) object = Arrow::gen(from, to);
        else object = Line::gen(from, to);
    } else if (directed && !dashed) {
        auto path = orthogonalPath(config, line, count);
        if (path) {
            path->fill(color);
            path->stroke({0, 0, 0, 0}, 0.0f);
            path->layer = config.layers.routes;
        }
        return path;
    } else {
        Vec3 points[Diagram::WaypointLimit + 4u];
        for (auto i = 0u; i < count; i++) points[i] = {line[i].x, line[i].y, 0.0f};
        if (directed) object = vis::DirectedRoute::gen(points, count);
        else object = Plot::gen(points, count);
    }
    if (object) {
        object->stroke(color, theme.objectWidth);
        object->layer = config.layers.routes;
    }
    return object;
}

}  // namespace

struct Built::Impl
{
    Group* root = nullptr;
    IR* receipt = nullptr;
    Group* zones = nullptr;
    Group* routes = nullptr;
    Group* nodes = nullptr;
    Group* annotations = nullptr;

    ~Impl()
    {
        if (root) {
            if (zones && !zones->parent()) delete zones;
            if (routes && !routes->parent()) delete routes;
            if (nodes && !nodes->parent()) delete nodes;
            if (annotations && !annotations->parent()) delete annotations;
        }
        delete root;
    }
};

struct Diagram::Impl
{
    Config config;
    char* prefix = nullptr;
    NodeRecord* nodes = nullptr;
    EdgeRecord* edges = nullptr;
    ZoneRecord* zones = nullptr;
    uint32_t nodeCount = 0u;
    uint32_t edgeCount = 0u;
    uint32_t zoneCount = 0u;

    ~Impl()
    {
        _release(nodes, nodeCount);
        _release(edges, edgeCount);
        _release(zones, zoneCount);
        delete[] prefix;
    }

    // build() fills the tree one layer at a time. Each step reads its geometry from the
    // receipt, records what it made in the receipt's views and, when it fails, deletes only
    // what it has not attached; Built::Impl frees the rest.

    // Creates, tags and attaches the four layer groups below the tagged root.
    Result buildGroups(Built::Impl& built) const noexcept
    {
        Group** groups[] = {&built.zones, &built.routes, &built.nodes, &built.annotations};
        const char* names[] = {"zones", "routes", "nodes", "annotations"};
        for (auto group : groups) *group = Group::gen();
        auto result = _tag(built.root, prefix, "root");
        for (auto i = 0u; i < 4u && result == Result::Success; i++) {
            result = _tag(*groups[i], prefix, names[i]);
        }
        for (auto i = 0u; i < 4u && result == Result::Success; i++) {
            result = built.root->add(*groups[i]);
        }
        return result;
    }

    Result buildZones(Built::Impl& built, IR::Impl& receipt, const Theme& theme,
                      float corner) const noexcept
    {
        for (auto i = 0u; i < zoneCount; i++) {
            auto& record = zones[i];
            auto& placement = receipt.zonePlacements[i];
            auto minimum = placement.center - placement.size * 0.5f;
            auto maximum = placement.center + placement.size * 0.5f;
            auto zone = Group::gen();
            auto body = Rectangle::gen({placement.center.x, placement.center.y, 0.0f},
                                       placement.size, corner * 1.5f);
            auto label = Text::gen(record.label, {minimum.x + config.zonePadding * 0.35f,
                                                  maximum.y - config.zonePadding * 0.55f, 0.0f});
            auto result = zone && body && label ? Result::Success : Result::OutOfMemory;
            if (result == Result::Success) result = _tag(zone, prefix, "zone", record.id);
            if (result == Result::Success) result = _tag(body, prefix, "zone", record.id, "body");
            if (result == Result::Success) {
                result = _tag(label, prefix, "zone", record.id, "label");
            }
            if (result == Result::Success) {
                // A zone only outlines its members.
                body->fill({0, 0, 0, 0});
                body->stroke(theme.colors.border, theme.objectWidth);
                body->layer = config.layers.zones;
                label->fill(theme.colors.muted);
                label->role = record.fullWidth ? TextRole::Code : TextRole::H3;
                label->align = {0.0f, 0.5f};
                label->layer = config.layers.annotations;
                result = zone->add(body);
            }
            if (result == Result::Success) result = built.zones->add(zone);
            if (result == Result::Success) result = built.annotations->add(label);
            if (result != Result::Success) {
                _discard({label, body, zone});
                return result;
            }
            receipt.zoneViews[i] = {zone, body, label};
        }
        return Result::Success;
    }

    Result buildEdges(Built::Impl& built, IR::Impl& receipt, const Theme& theme) const noexcept
    {
        float pattern[] = {5.0f, 4.0f};
        for (auto i = 0u; i < edgeCount; i++) {
            auto& record = edges[i];
            auto& placement = receipt.edgePlacements[i];
            auto color = _edgeColor(theme, record);
            auto dashed = _dashed(record);
            auto object = _route(config, theme, color, record.kind != EdgeKind::Relation, dashed,
                                 placement);
            if (!object) return Result::InvalidArguments;
            auto result = _tag(object, prefix, "edge", record.id);
            if (result == Result::Success) result = built.routes->add(object);
            if (result != Result::Success) {
                delete object;
                return result;
            }
            receipt.edgeViews[i].object = object;
            if (dashed) result = object->dash(pattern, 2u, record.flowOffset);
            if (result != Result::Success) return result;
            if (!record.label) continue;

            auto point = placement.labelPoint;
            auto label = Text::gen(record.label, {point.x, point.y, 0.0f});
            if (!label) return Result::OutOfMemory;
            label->fill(color);
            label->role = TextRole::Code;
            label->layer = config.layers.annotations;
            result = _tag(label, prefix, "edge", record.id, "label");
            if (result == Result::Success) result = built.annotations->add(label);
            if (result != Result::Success) {
                delete label;
                return result;
            }
            receipt.edgeViews[i].label = label;
        }
        return Result::Success;
    }

    Result buildNodes(Built::Impl& built, IR::Impl& receipt, const Theme& theme,
                      float corner) const noexcept
    {
        for (auto i = 0u; i < nodeCount; i++) {
            auto& record = nodes[i];
            auto center = receipt.nodePlacements[i].center;
            auto size = receipt.nodePlacements[i].size;
            auto node = Group::gen();
            // A Terminal is a pill once corners round, and square like the rest otherwise.
            auto pill = record.kind == NodeKind::Terminal && corner > 0.0f;
            auto body = Rectangle::gen({center.x, center.y, 0.0f}, size,
                                       pill ? std::fmin(size.x, size.y) * 0.5f : corner);
            auto labelPoint = record.detail ? Vec3{center.x, center.y + size.y * 0.27f, 0.0f}
                                            : Vec3{center.x, center.y, 0.0f};
            auto label = Text::gen(record.label, labelPoint);
            auto detail = record.detail
                              ? Text::gen(record.detail,
                                          {center.x, center.y - size.y * 0.27f, 0.0f})
                              : nullptr;
            auto made = node && body && label && (detail || !record.detail);
            auto result = made ? Result::Success : Result::OutOfMemory;
            if (result == Result::Success) result = _tag(node, prefix, "node", record.id);
            if (result == Result::Success) result = _tag(body, prefix, "node", record.id, "body");
            if (result == Result::Success) {
                result = _tag(label, prefix, "node", record.id, "label");
            }
            if (result == Result::Success && detail) {
                result = _tag(detail, prefix, "node", record.id, "detail");
            }
            if (result == Result::Success) {
                // Every kind is ink on the background; the author colors the node in focus.
                body->fill(theme.background);
                body->stroke(theme.colors.foreground, theme.objectWidth);
                body->layer = config.layers.nodes;
                label->fill(theme.colors.foreground);
                label->role = record.kind == NodeKind::Cell || record.kind == NodeKind::Task
                                      || record.kind == NodeKind::Terminal
                                      || record.kind == NodeKind::Evidence
                                  ? TextRole::Code
                                  : TextRole::Text;
                label->layer = config.layers.annotations;
                if (detail) {
                    detail->fill(theme.colors.muted);
                    detail->role = TextRole::Code;
                    detail->layer = config.layers.annotations;
                }
                result = node->add(body);
            }
            if (result == Result::Success) result = node->add(label);
            if (result == Result::Success && detail) result = node->add(detail);
            if (result == Result::Success) result = built.nodes->add(node);
            if (result != Result::Success) {
                _discard({body, label, detail, node});
                return result;
            }
            receipt.nodeViews[i] = {node, body, label, detail};
        }
        return Result::Success;
    }
};

Diagram::Diagram(Impl* impl) noexcept : pImpl(impl)
{
}

Diagram::~Diagram()
{
    delete pImpl;
}

Diagram* Diagram::gen(const Config& config) noexcept
{
    if (!_valid(config)) return nullptr;
    auto impl = new (std::nothrow) Impl;
    if (!impl) return nullptr;
    impl->prefix = _duplicate(config.id);
    impl->nodes = new (std::nothrow) NodeRecord[NodeLimit]{};
    impl->edges = new (std::nothrow) EdgeRecord[EdgeLimit]{};
    impl->zones = new (std::nothrow) ZoneRecord[ZoneLimit]{};
    if (!impl->prefix || !impl->nodes || !impl->edges || !impl->zones) {
        delete impl;
        return nullptr;
    }
    impl->config = config;
    impl->config.id = impl->prefix;
    auto diagram = new (std::nothrow) Diagram(impl);
    if (!diagram) delete impl;
    return diagram;
}

Result Diagram::add(const NodeSpec& spec, Node& output) noexcept
{
    if (pImpl->nodeCount >= NodeLimit || !_validId(spec.id)
        || !_validText(spec.label) || !_validText(spec.detail) || !_valid(spec.kind)
        || spec.rank < -1 || spec.row < -1 || spec.column < -1
        || spec.rank >= static_cast<int32_t>(NodeLimit) || !_finite(spec.position)
        || spec.row >= static_cast<int32_t>(NodeLimit)
        || spec.column >= static_cast<int32_t>(NodeLimit) || !_finite(spec.start)
        || !_finite(spec.span) || !_finite(spec.size)
        || ((spec.size.x != 0.0f || spec.size.y != 0.0f)
            && (spec.size.x <= 0.0f || spec.size.y <= 0.0f))) {
        return pImpl->nodeCount >= NodeLimit ? Result::InsufficientCondition
                                                      : Result::InvalidArguments;
    }
    switch (pImpl->config.layout) {
        case Layout::Ranked: {
            if (spec.row != -1 || spec.column != -1 || spec.start != 0.0f
                || spec.span != 0.0f) return Result::InvalidArguments;
            break;
        }
        case Layout::Manual: {
            if (!spec.manualPosition || spec.rank != -1 || spec.row != -1
                || spec.column != -1 || spec.start != 0.0f || spec.span != 0.0f) {
                return Result::InvalidArguments;
            }
            break;
        }
        case Layout::Grid: {
            if (spec.manualPosition || spec.rank != -1 || spec.row < 0 || spec.column < 0
                || spec.start != 0.0f || spec.span != 0.0f) {
                return Result::InvalidArguments;
            }
            break;
        }
        case Layout::Timeline: {
            if (spec.manualPosition || spec.rank != -1 || spec.row < 0
                || spec.column != -1 || spec.span <= 0.0f) {
                return Result::InvalidArguments;
            }
            break;
        }
    }
    for (auto i = 0u; i < pImpl->nodeCount; i++) {
        if (_equal(pImpl->nodes[i].id, spec.id)) return Result::InvalidArguments;
        if (pImpl->config.layout == Layout::Grid && pImpl->nodes[i].row == spec.row
            && pImpl->nodes[i].column == spec.column) return Result::InvalidArguments;
    }
    if (!_copy(spec, pImpl->nodes[pImpl->nodeCount])) return Result::OutOfMemory;
    output.index = pImpl->nodeCount++;
    return Result::Success;
}

Result Diagram::add(const EdgeSpec& spec, Edge& output) noexcept
{
    if (pImpl->edgeCount >= EdgeLimit || !_validId(spec.id)
        || !_validText(spec.label) || !spec.from || !spec.to
        || spec.from.index >= pImpl->nodeCount || spec.to.index >= pImpl->nodeCount
        || spec.from.index == spec.to.index || !_valid(spec.fromPort) || !_valid(spec.toPort)
        || !_valid(spec.route) || !_valid(spec.kind)
        || (spec.waypointCount && !spec.waypoints)
        || (!spec.waypointCount && spec.waypoints) || spec.waypointCount > WaypointLimit
        || (spec.route == Route::Straight && spec.waypointCount)
        || !_finite(spec.flowOffset)) {
        return pImpl->edgeCount >= EdgeLimit ? Result::InsufficientCondition
                                                      : Result::InvalidArguments;
    }
    for (auto i = 0u; i < pImpl->edgeCount; i++) {
        if (_equal(pImpl->edges[i].id, spec.id)) return Result::InvalidArguments;
    }
    for (auto i = 0u; i < spec.waypointCount; i++) {
        if (!_finite(spec.waypoints[i])
            || (i && _same(spec.waypoints[i - 1u], spec.waypoints[i]))) {
            return Result::InvalidArguments;
        }
    }
    if (!_copy(spec, pImpl->edges[pImpl->edgeCount])) return Result::OutOfMemory;
    output.index = pImpl->edgeCount++;
    return Result::Success;
}

Result Diagram::add(const ZoneSpec& spec, Zone& output) noexcept
{
    if (pImpl->zoneCount >= ZoneLimit || !_validId(spec.id)
        || !_validText(spec.label) || !spec.members || !spec.memberCount
        || spec.memberCount > pImpl->nodeCount) {
        return pImpl->zoneCount >= ZoneLimit ? Result::InsufficientCondition
                                                      : Result::InvalidArguments;
    }
    for (auto i = 0u; i < pImpl->zoneCount; i++) {
        if (_equal(pImpl->zones[i].id, spec.id)) return Result::InvalidArguments;
    }
    for (auto i = 0u; i < spec.memberCount; i++) {
        if (!spec.members[i] || spec.members[i].index >= pImpl->nodeCount) {
            return Result::InvalidArguments;
        }
        for (auto j = 0u; j < i; j++) {
            if (spec.members[i].index == spec.members[j].index) {
                return Result::InvalidArguments;
            }
        }
    }
    if (!_copy(spec, pImpl->zones[pImpl->zoneCount])) return Result::OutOfMemory;
    output.index = pImpl->zoneCount++;
    return Result::Success;
}

Result Diagram::build(const Theme& theme, Built*& output) const noexcept
{
    if (output || !pImpl->nodeCount) return Result::InvalidArguments;
    // A negative corner follows the Theme: its roundness of the node size's shorter side.
    auto& nodeSize = pImpl->config.nodeSize;
    auto corner = pImpl->config.corner >= 0.0f
                      ? pImpl->config.corner
                      : theme.roundness * std::fmin(nodeSize.x, nodeSize.y);
    if (!_finite(corner) || corner < 0.0f) return Result::InvalidArguments;

    NodeLayout layouts[NodeLimit]{};
    EdgeLayout edgeLayouts[EdgeLimit]{};
    ZoneLayout zoneLayouts[ZoneLimit]{};

    auto result = layoutNodes(pImpl->config, pImpl->nodes, pImpl->nodeCount,
                              pImpl->edges, pImpl->edgeCount, layouts);
    if (result != Result::Success) return result;
    result = layoutEdges(pImpl->config, pImpl->edges, pImpl->edgeCount, layouts, edgeLayouts);
    if (result != Result::Success) return result;
    result = layoutZones(pImpl->config, pImpl->zones, pImpl->zoneCount, layouts,
                          pImpl->nodeCount, zoneLayouts);
    if (result != Result::Success) return result;

    auto receiptImpl = new (std::nothrow) IR::Impl;
    if (!receiptImpl) return Result::OutOfMemory;
    auto receipt = new (std::nothrow) IR(receiptImpl);
    if (!receipt) {
        delete receiptImpl;
        return Result::OutOfMemory;
    }
    result = receiptImpl->copy(pImpl->config, pImpl->nodes, pImpl->nodeCount,
                               pImpl->edges, pImpl->edgeCount, pImpl->zones,
                               pImpl->zoneCount, layouts, edgeLayouts, zoneLayouts);
    if (result != Result::Success) {
        delete receipt;
        return result;
    }

    // Once the root exists it owns the receipt, and the guard owns the root until Built does.
    std::unique_ptr<Built::Impl> built(new (std::nothrow) Built::Impl);
    if (built) built->root = new (std::nothrow) receipt_detail::DiagramRoot(receipt);
    if (!built || !built->root) {
        delete receipt;
        return Result::OutOfMemory;
    }
    built->receipt = receipt;
    if ((result = pImpl->buildGroups(*built)) != Result::Success
        || (result = pImpl->buildZones(*built, *receiptImpl, theme, corner)) != Result::Success
        || (result = pImpl->buildEdges(*built, *receiptImpl, theme)) != Result::Success
        || (result = pImpl->buildNodes(*built, *receiptImpl, theme, corner)) != Result::Success) {
        return result;
    }
    output = new (std::nothrow) Built(built.get());
    if (!output) return Result::OutOfMemory;
    built.release();
    return Result::Success;
}

IR::IR(Impl* impl) noexcept : pImpl(impl)
{
}

IR::~IR()
{
    delete pImpl;
}

const Config& IR::config() const noexcept
{
    static const Config empty;
    return pImpl ? pImpl->config : empty;
}

uint32_t IR::nodeCount() const noexcept
{
    return pImpl ? pImpl->nodeCount : 0u;
}

const NodeSpec* IR::nodeAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->nodeCount ? pImpl->nodes + index : nullptr;
}

const NodePlacement* IR::nodePlacementAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->nodeCount ? pImpl->nodePlacements + index : nullptr;
}

uint32_t IR::edgeCount() const noexcept
{
    return pImpl ? pImpl->edgeCount : 0u;
}

const EdgeSpec* IR::edgeAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->edgeCount ? pImpl->edges + index : nullptr;
}

const EdgePlacement* IR::edgePlacementAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->edgeCount ? pImpl->edgePlacements + index : nullptr;
}

uint32_t IR::zoneCount() const noexcept
{
    return pImpl ? pImpl->zoneCount : 0u;
}

const ZoneSpec* IR::zoneAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->zoneCount ? pImpl->zones + index : nullptr;
}

const ZonePlacement* IR::zonePlacementAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->zoneCount ? pImpl->zonePlacements + index : nullptr;
}

const Object* IR::object(Node node) const noexcept
{
    return pImpl ? pImpl->view(node).object : nullptr;
}

const Rectangle* IR::body(Node node) const noexcept
{
    return pImpl ? pImpl->view(node).body : nullptr;
}

const Text* IR::label(Node node) const noexcept
{
    return pImpl ? pImpl->view(node).label : nullptr;
}

const Text* IR::detail(Node node) const noexcept
{
    return pImpl ? pImpl->view(node).detail : nullptr;
}

const Object* IR::object(Edge edge) const noexcept
{
    return pImpl ? pImpl->view(edge).object : nullptr;
}

const Text* IR::label(Edge edge) const noexcept
{
    return pImpl ? pImpl->view(edge).label : nullptr;
}

const Object* IR::object(Zone zone) const noexcept
{
    return pImpl ? pImpl->view(zone).object : nullptr;
}

const Rectangle* IR::body(Zone zone) const noexcept
{
    return pImpl ? pImpl->view(zone).body : nullptr;
}

const Text* IR::label(Zone zone) const noexcept
{
    return pImpl ? pImpl->view(zone).label : nullptr;
}

size_t IR::byteSize() const noexcept
{
    return pImpl ? pImpl->bytes + sizeof(receipt_detail::DiagramRoot) - sizeof(Group) : 0u;
}

const IR* ir(const Object* root) noexcept
{
    receipt_detail::RegistryGuard guard;
    for (auto current = receipt_detail::registry; current; current = current->next) {
        if (static_cast<const Object*>(current) == root) return current->receipt;
    }
    return nullptr;
}

ConstraintReport::ConstraintReport() noexcept
{
    pImpl = new (std::nothrow) Impl;
}

ConstraintReport::~ConstraintReport()
{
    delete pImpl;
}

uint32_t ConstraintReport::count() const noexcept
{
    return pImpl ? pImpl->count : 0u;
}

const ConstraintIssue* ConstraintReport::issueAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->count ? pImpl->issues + index : nullptr;
}

Result validate(const IR& receipt, ConstraintReport& report) noexcept
{
    if (!report.pImpl) return Result::OutOfMemory;
    report.pImpl->count = 0u;

    for (auto i = 0u; i < receipt.nodeCount(); i++) {
        auto first = receipt.nodePlacementAt(i);
        if (!first) return Result::InvalidArguments;
        for (auto j = i + 1u; j < receipt.nodeCount(); j++) {
            auto second = receipt.nodePlacementAt(j);
            if (!second) return Result::InvalidArguments;
            if (!_overlap(*first, *second)) continue;
            ConstraintIssue issue;
            issue.code = ConstraintCode::NodeOverlap;
            issue.node.index = i;
            issue.relatedNode.index = j;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
    }

    for (auto i = 0u; i < receipt.edgeCount(); i++) {
        auto spec = receipt.edgeAt(i);
        auto placement = receipt.edgePlacementAt(i);
        if (!spec || !placement || placement->centerlineCount < 2u
            || !placement->centerline || spec->from.index >= receipt.nodeCount()
            || spec->to.index >= receipt.nodeCount()) {
            return Result::InvalidArguments;
        }
        auto from = receipt.nodePlacementAt(spec->from.index);
        auto to = receipt.nodePlacementAt(spec->to.index);
        if (!from || !to) return Result::InvalidArguments;

        // Reports a check that failed at the edge's source, or at its target when atTarget.
        auto endpoint = [&](ConstraintCode code, bool atTarget, Port port) {
            ConstraintIssue issue;
            issue.code = code;
            issue.node = atTarget ? spec->to : spec->from;
            issue.edge.index = i;
            issue.endpoint = atTarget ? EdgeEndpoint::To : EdgeEndpoint::From;
            issue.port = port;
            return report.pImpl->add(issue);
        };
        auto line = placement->centerline;
        auto last = placement->centerlineCount - 1u;
        auto fromAttached = _attached(*from, line[0], placement->fromPort);
        auto toAttached = _attached(*to, line[last], placement->toPort);
        // Both ends answer one check before the next check runs.
        if ((spec->fromPort != Port::Auto && spec->fromPort != placement->fromPort
             && !endpoint(ConstraintCode::EdgePortMismatch, false, spec->fromPort))
            || (spec->toPort != Port::Auto && spec->toPort != placement->toPort
                && !endpoint(ConstraintCode::EdgePortMismatch, true, spec->toPort))
            || (!fromAttached
                && !endpoint(ConstraintCode::EdgeEndpointDetached, false, placement->fromPort))
            || (!toAttached
                && !endpoint(ConstraintCode::EdgeEndpointDetached, true, placement->toPort))
            || (fromAttached && !_outwardSegment(line[0], line[1], placement->fromPort)
                && !endpoint(ConstraintCode::EdgeEndpointDirection, false, placement->fromPort))
            || (toAttached && !_outwardSegment(line[last], line[last - 1u], placement->toPort)
                && !endpoint(ConstraintCode::EdgeEndpointDirection, true, placement->toPort))) {
            return Result::OutOfMemory;
        }

        auto margin = _routeMargin(receipt.config());
        for (auto j = 0u; j < receipt.nodeCount(); j++) {
            auto node = receipt.nodePlacementAt(j);
            if (!node) return Result::InvalidArguments;
            if (!_routeCrosses(line, placement->centerlineCount, node->center, node->size, margin,
                               j == spec->from.index, j == spec->to.index)) {
                continue;
            }
            ConstraintIssue issue;
            issue.code = ConstraintCode::RouteCrossesNode;
            issue.node.index = j;
            issue.edge.index = i;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
    }

    auto fullMinimum = 0.0f;
    auto fullMaximum = 0.0f;
    if (receipt.nodeCount()) {
        auto first = receipt.nodePlacementAt(0u);
        if (!first) return Result::InvalidArguments;
        fullMinimum = first->center.x - first->size.x * 0.5f;
        fullMaximum = first->center.x + first->size.x * 0.5f;
        for (auto i = 1u; i < receipt.nodeCount(); i++) {
            auto node = receipt.nodePlacementAt(i);
            if (!node) return Result::InvalidArguments;
            fullMinimum = std::fmin(fullMinimum, node->center.x - node->size.x * 0.5f);
            fullMaximum = std::fmax(fullMaximum, node->center.x + node->size.x * 0.5f);
        }
    }
    for (auto i = 0u; i < receipt.zoneCount(); i++) {
        auto spec = receipt.zoneAt(i);
        auto placement = receipt.zonePlacementAt(i);
        if (!spec || !placement || !spec->members || !spec->memberCount) {
            return Result::InvalidArguments;
        }
        for (auto j = 0u; j < spec->memberCount; j++) {
            auto member = spec->members[j];
            auto node = receipt.nodePlacementAt(member.index);
            if (!node) return Result::InvalidArguments;
            if (_contains(*placement, *node)) continue;
            ConstraintIssue issue;
            issue.code = ConstraintCode::ZoneExcludesMember;
            issue.node = member;
            issue.zone.index = i;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
        if (!spec->fullWidth) continue;
        auto minimum = placement->center.x - placement->size.x * 0.5f;
        auto maximum = placement->center.x + placement->size.x * 0.5f;
        if (minimum <= fullMinimum + Epsilon && maximum >= fullMaximum - Epsilon) continue;
        ConstraintIssue issue;
        issue.code = ConstraintCode::FullWidthZoneMismatch;
        issue.zone.index = i;
        if (!report.pImpl->add(issue)) return Result::OutOfMemory;
    }
    return Result::Success;
}

Built::Built(Impl* impl) noexcept : pImpl(impl)
{
}

Built::~Built()
{
    delete pImpl;
}

Group* Built::root() const noexcept
{
    return pImpl ? pImpl->root : nullptr;
}

Group* Built::release() noexcept
{
    if (!pImpl) return nullptr;
    auto root = pImpl->root;
    pImpl->root = nullptr;
    return root;
}

Group* Built::zones() const noexcept
{
    return pImpl ? pImpl->zones : nullptr;
}

Group* Built::routes() const noexcept
{
    return pImpl ? pImpl->routes : nullptr;
}

Group* Built::nodes() const noexcept
{
    return pImpl ? pImpl->nodes : nullptr;
}

Group* Built::annotations() const noexcept
{
    return pImpl ? pImpl->annotations : nullptr;
}

Object* Built::object(Node node) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(node).object : nullptr;
}

Rectangle* Built::body(Node node) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(node).body : nullptr;
}

Text* Built::label(Node node) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(node).label : nullptr;
}

Text* Built::detail(Node node) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(node).detail : nullptr;
}

Object* Built::object(Edge edge) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(edge).object : nullptr;
}

Text* Built::label(Edge edge) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(edge).label : nullptr;
}

Object* Built::object(Zone zone) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(zone).object : nullptr;
}

Rectangle* Built::body(Zone zone) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(zone).body : nullptr;
}

Text* Built::label(Zone zone) const noexcept
{
    return pImpl ? pImpl->receipt->pImpl->view(zone).label : nullptr;
}

const IR* Built::ir() const noexcept
{
    return pImpl ? pImpl->receipt : nullptr;
}

}  // namespace vis::diagram
