#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
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
        if (nodes) {
            for (auto i = 0u; i < nodeCount; i++) {
                delete[] nodes[i].id;
                delete[] nodes[i].label;
                delete[] nodes[i].detail;
            }
        }
        if (edges) {
            for (auto i = 0u; i < edgeCount; i++) {
                delete[] edges[i].id;
                delete[] edges[i].label;
                delete[] edges[i].waypoints;
            }
        }
        if (edgePlacements) {
            for (auto i = 0u; i < edgeCount; i++) delete[] edgePlacements[i].centerline;
        }
        if (zones) {
            for (auto i = 0u; i < zoneCount; i++) {
                delete[] zones[i].id;
                delete[] zones[i].label;
                delete[] zones[i].members;
            }
        }
        delete[] nodes;
        delete[] edges;
        delete[] zones;
        delete[] nodePlacements;
        delete[] edgePlacements;
        delete[] zonePlacements;
        delete[] nodeViews;
        delete[] edgeViews;
        delete[] zoneViews;
        delete[] prefix;
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
        bytes += std::strlen(prefix) + 1u;
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
            auto& source = sourceNodes[i];
            auto& target = nodes[i];
            target.id = _duplicate(source.id);
            target.label = _duplicate(source.label);
            target.detail = _duplicate(source.detail);
            if (!target.id || !target.label || (source.detail && !target.detail)) {
                return Result::OutOfMemory;
            }
            target.rank = source.rank;
            target.position = source.position;
            target.size = source.size;
            target.manualPosition = source.manualPosition;
            target.kind = source.kind;
            target.row = source.row;
            target.column = source.column;
            target.start = source.start;
            target.span = source.span;
            nodePlacements[i] = {layouts[i].center, layouts[i].size, layouts[i].rank};
            bytes += std::strlen(target.id) + std::strlen(target.label) + 2u;
            if (target.detail) bytes += std::strlen(target.detail) + 1u;
        }

        for (auto i = 0u; i < edgeCount; i++) {
            auto& source = sourceEdges[i];
            auto& target = edges[i];
            target.id = _duplicate(source.id);
            target.label = _duplicate(source.label);
            target.from = source.from;
            target.to = source.to;
            target.fromPort = source.fromPort;
            target.toPort = source.toPort;
            target.route = source.route;
            target.waypointCount = source.waypointCount;
            target.flowOffset = source.flowOffset;
            target.flow = source.flow;
            target.kind = source.kind;
            if (source.waypointCount) {
                auto waypoints = new (std::nothrow) Vec2[source.waypointCount];
                if (!waypoints) return Result::OutOfMemory;
                for (auto j = 0u; j < source.waypointCount; j++) {
                    waypoints[j] = source.waypoints[j];
                }
                target.waypoints = waypoints;
            }
            if (!target.id || (source.label && !target.label)) return Result::OutOfMemory;

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
                auto length = _length(_sub(centerline[j], centerline[j - 1u]));
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
            bytes += std::strlen(target.id) + 1u + sizeof(Vec2) * source.waypointCount
                     + sizeof(Vec2) * centerlineCount;
            if (target.label) bytes += std::strlen(target.label) + 1u;
        }

        for (auto i = 0u; i < zoneCount; i++) {
            auto& source = sourceZones[i];
            auto& target = zones[i];
            target.id = _duplicate(source.id);
            target.label = _duplicate(source.label);
            auto members = new (std::nothrow) Node[source.memberCount];
            if (!target.id || !target.label || !members) {
                delete[] members;
                return Result::OutOfMemory;
            }
            for (auto j = 0u; j < source.memberCount; j++) members[j] = source.members[j];
            target.members = members;
            target.memberCount = source.memberCount;
            target.fullWidth = source.fullWidth;
            zonePlacements[i] = {zoneLayouts[i].center, zoneLayouts[i].size};
            bytes += std::strlen(target.id) + std::strlen(target.label) + 2u
                     + sizeof(Node) * target.memberCount;
        }
        return Result::Success;
    }
};

struct ConstraintReport::Impl
{
    ConstraintIssue* issues = nullptr;
    uint32_t count = 0u;
    uint32_t capacity = 0u;

    ~Impl()
    {
        delete[] issues;
    }

    bool add(const ConstraintIssue& issue) noexcept
    {
        if (count == capacity) {
            auto next = capacity ? capacity * 2u : 8u;
            auto grown = new (std::nothrow) ConstraintIssue[next];
            if (!grown) return false;
            for (auto i = 0u; i < count; i++) grown[i] = issues[i];
            delete[] issues;
            issues = grown;
            capacity = next;
        }
        issues[count++] = issue;
        return true;
    }
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
    auto containerMin = _sub(container.center, _mul(container.size, 0.5f));
    auto containerMax = _add(container.center, _mul(container.size, 0.5f));
    auto contentMin = _sub(content.center, _mul(content.size, 0.5f));
    auto contentMax = _add(content.center, _mul(content.size, 0.5f));
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
    auto half = _mul(node.size, 0.5f);
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

}  // namespace

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
        for (auto i = 0u; i < nodeCount; i++) {
            delete[] nodes[i].id;
            delete[] nodes[i].label;
            delete[] nodes[i].detail;
        }
        for (auto i = 0u; i < edgeCount; i++) {
            delete[] edges[i].id;
            delete[] edges[i].label;
            delete[] edges[i].waypoints;
        }
        for (auto i = 0u; i < zoneCount; i++) {
            delete[] zones[i].id;
            delete[] zones[i].label;
            delete[] zones[i].members;
        }
        delete[] nodes;
        delete[] edges;
        delete[] zones;
        delete[] prefix;
    }
};

struct Built::Impl
{
    Group* root = nullptr;
    IR* receipt = nullptr;
    Group* zones = nullptr;
    Group* routes = nullptr;
    Group* nodes = nullptr;
    Group* annotations = nullptr;
    NodeView* nodeViews = nullptr;
    EdgeView* edgeViews = nullptr;
    ZoneView* zoneViews = nullptr;
    uint32_t nodeCount = 0u;
    uint32_t edgeCount = 0u;
    uint32_t zoneCount = 0u;

    ~Impl()
    {
        if (root) {
            if (zones && !zones->parent()) delete zones;
            if (routes && !routes->parent()) delete routes;
            if (nodes && !nodes->parent()) delete nodes;
            if (annotations && !annotations->parent()) delete annotations;
        }
        delete root;
        delete[] nodeViews;
        delete[] edgeViews;
        delete[] zoneViews;
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
    auto id = _duplicate(spec.id);
    auto label = _duplicate(spec.label ? spec.label : spec.id);
    auto detail = spec.detail && spec.detail[0] ? _duplicate(spec.detail) : nullptr;
    if (!id || !label || (spec.detail && spec.detail[0] && !detail)) {
        delete[] id;
        delete[] label;
        delete[] detail;
        return Result::OutOfMemory;
    }
    auto& record = pImpl->nodes[pImpl->nodeCount];
    record.id = id;
    record.label = label;
    record.detail = detail;
    record.kind = spec.kind;
    record.rank = spec.rank;
    record.row = spec.row;
    record.column = spec.column;
    record.start = spec.start;
    record.span = spec.span;
    record.position = spec.position;
    record.size = spec.size;
    record.manualPosition = spec.manualPosition;
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
    auto id = _duplicate(spec.id);
    auto label = spec.label && spec.label[0] ? _duplicate(spec.label) : nullptr;
    auto waypoints = spec.waypointCount
                         ? new (std::nothrow) Vec2[spec.waypointCount]
                         : nullptr;
    if (!id || (spec.label && spec.label[0] && !label)
        || (spec.waypointCount && !waypoints)) {
        delete[] id;
        delete[] label;
        delete[] waypoints;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < spec.waypointCount; i++) waypoints[i] = spec.waypoints[i];
    auto& record = pImpl->edges[pImpl->edgeCount];
    record.id = id;
    record.label = label;
    record.from = spec.from;
    record.to = spec.to;
    record.fromPort = spec.fromPort;
    record.toPort = spec.toPort;
    record.route = spec.route;
    record.kind = spec.kind;
    record.waypoints = waypoints;
    record.waypointCount = spec.waypointCount;
    record.flowOffset = spec.flowOffset;
    record.flow = spec.flow;
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
    auto id = _duplicate(spec.id);
    auto label = _duplicate(spec.label ? spec.label : spec.id);
    auto members = new (std::nothrow) Node[spec.memberCount];
    if (!id || !label || !members) {
        delete[] id;
        delete[] label;
        delete[] members;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < spec.memberCount; i++) members[i] = spec.members[i];
    auto& record = pImpl->zones[pImpl->zoneCount];
    record.id = id;
    record.label = label;
    record.members = members;
    record.memberCount = spec.memberCount;
    record.fullWidth = spec.fullWidth;
    output.index = pImpl->zoneCount++;
    return Result::Success;
}

uint32_t Diagram::nodeCount() const noexcept
{
    return pImpl->nodeCount;
}

uint32_t Diagram::edgeCount() const noexcept
{
    return pImpl->edgeCount;
}

uint32_t Diagram::zoneCount() const noexcept
{
    return pImpl->zoneCount;
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

    auto builtImpl = new (std::nothrow) Built::Impl;
    if (!builtImpl) {
        delete receipt;
        return Result::OutOfMemory;
    }
    builtImpl->root = new (std::nothrow) receipt_detail::DiagramRoot(receipt);
    if (!builtImpl->root) {
        delete receipt;
        delete builtImpl;
        return Result::OutOfMemory;
    }
    builtImpl->receipt = receipt;
    builtImpl->nodeCount = pImpl->nodeCount;
    builtImpl->edgeCount = pImpl->edgeCount;
    builtImpl->zoneCount = pImpl->zoneCount;
    builtImpl->nodeViews = new (std::nothrow) NodeView[pImpl->nodeCount]{};
    if (pImpl->edgeCount) {
        builtImpl->edgeViews = new (std::nothrow) EdgeView[pImpl->edgeCount]{};
    }
    if (pImpl->zoneCount) {
        builtImpl->zoneViews = new (std::nothrow) ZoneView[pImpl->zoneCount]{};
    }
    if (!builtImpl->nodeViews || (pImpl->edgeCount && !builtImpl->edgeViews)
        || (pImpl->zoneCount && !builtImpl->zoneViews)) {
        delete builtImpl;
        return Result::OutOfMemory;
    }

    builtImpl->zones = Group::gen();
    builtImpl->routes = Group::gen();
    builtImpl->nodes = Group::gen();
    builtImpl->annotations = Group::gen();
    result = _tag(builtImpl->root, pImpl->prefix, "root");
    if (result == Result::Success) result = _tag(builtImpl->zones, pImpl->prefix, "zones");
    if (result == Result::Success) result = _tag(builtImpl->routes, pImpl->prefix, "routes");
    if (result == Result::Success) result = _tag(builtImpl->nodes, pImpl->prefix, "nodes");
    if (result == Result::Success) {
        result = _tag(builtImpl->annotations, pImpl->prefix, "annotations");
    }
    if (result != Result::Success) {
        delete builtImpl;
        return result;
    }
    if ((result = builtImpl->root->add(builtImpl->zones)) != Result::Success
        || (result = builtImpl->root->add(builtImpl->routes)) != Result::Success
        || (result = builtImpl->root->add(builtImpl->nodes)) != Result::Success
        || (result = builtImpl->root->add(builtImpl->annotations)) != Result::Success) {
        delete builtImpl;
        return result;
    }

    for (auto i = 0u; i < pImpl->zoneCount; i++) {
        auto& record = pImpl->zones[i];
        auto& layout = zoneLayouts[i];
        auto minimum = _sub(layout.center, _mul(layout.size, 0.5f));
        auto maximum = _add(layout.center, _mul(layout.size, 0.5f));
        auto zone = Group::gen();
        auto body = Rectangle::gen({layout.center.x, layout.center.y, 0.0f}, layout.size,
                                   corner * 1.5f);
        auto label = Text::gen(record.label,
                               {minimum.x + pImpl->config.zonePadding * 0.35f,
                                maximum.y - pImpl->config.zonePadding * 0.55f, 0.0f});
        if (!zone || !body || !label) {
            delete zone;
            delete body;
            delete label;
            delete builtImpl;
            return Result::OutOfMemory;
        }
        if ((result = _tag(zone, pImpl->prefix, "zone", record.id)) != Result::Success
            || (result = _tag(body, pImpl->prefix, "zone", record.id, "body"))
                   != Result::Success
            || (result = _tag(label, pImpl->prefix, "zone", record.id, "label"))
                   != Result::Success) {
            delete zone;
            delete body;
            delete label;
            delete builtImpl;
            return result;
        }
        // A zone only outlines its members.
        body->fill({0, 0, 0, 0});
        body->stroke(theme.colors.border, theme.objectWidth);
        body->layer = pImpl->config.layers.zones;
        label->fill(theme.colors.muted);
        label->role = record.fullWidth ? TextRole::Code : TextRole::H3;
        label->align = {0.0f, 0.5f};
        label->layer = pImpl->config.layers.annotations;
        if ((result = zone->add(body)) != Result::Success
            || (result = builtImpl->zones->add(zone)) != Result::Success
            || (result = builtImpl->annotations->add(label)) != Result::Success) {
            if (!label->parent()) delete label;
            if (!body->parent()) delete body;
            if (!zone->parent()) delete zone;
            delete builtImpl;
            return result;
        }
        builtImpl->zoneViews[i] = {zone, body, label};
        receiptImpl->zoneViews[i] = {zone, body, label};
    }

    for (auto i = 0u; i < pImpl->edgeCount; i++) {
        auto& record = pImpl->edges[i];
        auto& placement = receiptImpl->edgePlacements[i];
        auto route = placement.route;
        auto centerline = placement.centerline;
        auto centerlineCount = placement.centerlineCount;
        auto color = _edgeColor(theme, record);
        auto dashed = _dashed(record);
        auto directed = record.kind != EdgeKind::Relation;
        Object* edgeObject = nullptr;
        if (route == Route::Straight) {
            if (directed) {
                edgeObject = Arrow::gen({edgeLayouts[i].from.x, edgeLayouts[i].from.y, 0.0f},
                                        {edgeLayouts[i].to.x, edgeLayouts[i].to.y, 0.0f});
            } else {
                edgeObject = Line::gen({edgeLayouts[i].from.x, edgeLayouts[i].from.y, 0.0f},
                                       {edgeLayouts[i].to.x, edgeLayouts[i].to.y, 0.0f});
            }
            if (edgeObject) {
                edgeObject->stroke(color, theme.objectWidth);
                edgeObject->layer = pImpl->config.layers.routes;
            }
        } else if (!directed || dashed) {
            Vec3 points[WaypointLimit + 4u];
            for (auto j = 0u; j < centerlineCount; j++) {
                points[j] = {centerline[j].x, centerline[j].y, 0.0f};
            }
            if (directed) {
                edgeObject = vis::DirectedRoute::gen(points, centerlineCount);
            } else {
                edgeObject = Plot::gen(points, centerlineCount);
            }
            if (edgeObject) {
                edgeObject->stroke(color, theme.objectWidth);
                edgeObject->layer = pImpl->config.layers.routes;
            }
        } else {
            auto path = orthogonalPath(pImpl->config, centerline, centerlineCount);
            if (path) {
                path->fill(color);
                path->stroke({0, 0, 0, 0}, 0.0f);
                path->layer = pImpl->config.layers.routes;
            }
            edgeObject = path;
        }
        if (!edgeObject) {
            delete builtImpl;
            return Result::InvalidArguments;
        }
        result = _tag(edgeObject, pImpl->prefix, "edge", record.id);
        if (result != Result::Success) {
            delete edgeObject;
            delete builtImpl;
            return result;
        }
        if ((result = builtImpl->routes->add(edgeObject)) != Result::Success) {
            delete edgeObject;
            delete builtImpl;
            return result;
        }
        builtImpl->edgeViews[i].object = edgeObject;
        receiptImpl->edgeViews[i].object = edgeObject;

        if (dashed) {
            float pattern[] = {5.0f, 4.0f};
            result = edgeObject->dash(pattern, 2u, record.flowOffset);
            if (result != Result::Success) {
                delete builtImpl;
                return result;
            }
        }
        if (record.label) {
            auto point = placement.labelPoint;
            auto label = Text::gen(record.label, {point.x, point.y, 0.0f});
            if (!label) {
                delete builtImpl;
                return Result::OutOfMemory;
            }
            label->fill(color);
            label->role = TextRole::Code;
            label->layer = pImpl->config.layers.annotations;
            result = _tag(label, pImpl->prefix, "edge", record.id, "label");
            if (result != Result::Success) {
                delete label;
                delete builtImpl;
                return result;
            }
            if ((result = builtImpl->annotations->add(label)) != Result::Success) {
                delete label;
                delete builtImpl;
                return result;
            }
            builtImpl->edgeViews[i].label = label;
            receiptImpl->edgeViews[i].label = label;
        }
    }

    for (auto i = 0u; i < pImpl->nodeCount; i++) {
        auto& record = pImpl->nodes[i];
        auto& layout = layouts[i];
        auto node = Group::gen();
        // A Terminal is a pill once corners round, and square like the rest otherwise.
        auto pill = record.kind == NodeKind::Terminal && corner > 0.0f;
        auto body = Rectangle::gen({layout.center.x, layout.center.y, 0.0f}, layout.size,
                                   pill ? std::fmin(layout.size.x, layout.size.y) * 0.5f
                                        : corner);
        auto labelPoint = record.detail
                              ? Vec3{layout.center.x, layout.center.y + layout.size.y * 0.27f,
                                     0.0f}
                              : Vec3{layout.center.x, layout.center.y, 0.0f};
        auto label = Text::gen(record.label, labelPoint);
        auto detail = record.detail
                          ? Text::gen(record.detail,
                                      {layout.center.x,
                                       layout.center.y - layout.size.y * 0.27f, 0.0f})
                          : nullptr;
        if (!node || !body || !label || (record.detail && !detail)) {
            delete node;
            delete body;
            delete label;
            delete detail;
            delete builtImpl;
            return Result::OutOfMemory;
        }
        if ((result = _tag(node, pImpl->prefix, "node", record.id)) != Result::Success
            || (result = _tag(body, pImpl->prefix, "node", record.id, "body"))
                   != Result::Success
            || (result = _tag(label, pImpl->prefix, "node", record.id, "label"))
                   != Result::Success
            || (detail
                && (result = _tag(detail, pImpl->prefix, "node", record.id, "detail"))
                       != Result::Success)) {
            delete node;
            delete body;
            delete label;
            delete detail;
            delete builtImpl;
            return result;
        }
        // Every kind is ink on the background; the author colors the node in focus.
        body->fill(theme.background);
        body->stroke(theme.colors.foreground, theme.objectWidth);
        body->layer = pImpl->config.layers.nodes;
        label->fill(theme.colors.foreground);
        label->role = record.kind == NodeKind::Cell || record.kind == NodeKind::Task
                              || record.kind == NodeKind::Terminal
                              || record.kind == NodeKind::Evidence
                          ? TextRole::Code
                          : TextRole::Text;
        label->layer = pImpl->config.layers.annotations;
        if (detail) {
            detail->fill(theme.colors.muted);
            detail->role = TextRole::Code;
            detail->layer = pImpl->config.layers.annotations;
        }
        if ((result = node->add(body)) != Result::Success
            || (result = node->add(label)) != Result::Success
            || (detail && (result = node->add(detail)) != Result::Success)
            || (result = builtImpl->nodes->add(node)) != Result::Success) {
            if (!body->parent()) delete body;
            if (!label->parent()) delete label;
            if (detail && !detail->parent()) delete detail;
            if (!node->parent()) delete node;
            delete builtImpl;
            return result;
        }
        builtImpl->nodeViews[i] = {node, body, label, detail};
        receiptImpl->nodeViews[i] = {node, body, label, detail};
    }

    auto built = new (std::nothrow) Built(builtImpl);
    if (!built) {
        delete builtImpl;
        return Result::OutOfMemory;
    }
    output = built;
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
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].object
               : nullptr;
}

const Rectangle* IR::body(Node node) const noexcept
{
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].body
               : nullptr;
}

const Text* IR::label(Node node) const noexcept
{
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].label
               : nullptr;
}

const Text* IR::detail(Node node) const noexcept
{
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].detail
               : nullptr;
}

const Object* IR::object(Edge edge) const noexcept
{
    return pImpl && edge && edge.index < pImpl->edgeCount
               ? pImpl->edgeViews[edge.index].object
               : nullptr;
}

const Text* IR::label(Edge edge) const noexcept
{
    return pImpl && edge && edge.index < pImpl->edgeCount
               ? pImpl->edgeViews[edge.index].label
               : nullptr;
}

const Object* IR::object(Zone zone) const noexcept
{
    return pImpl && zone && zone.index < pImpl->zoneCount
               ? pImpl->zoneViews[zone.index].object
               : nullptr;
}

const Rectangle* IR::body(Zone zone) const noexcept
{
    return pImpl && zone && zone.index < pImpl->zoneCount
               ? pImpl->zoneViews[zone.index].body
               : nullptr;
}

const Text* IR::label(Zone zone) const noexcept
{
    return pImpl && zone && zone.index < pImpl->zoneCount
               ? pImpl->zoneViews[zone.index].label
               : nullptr;
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

        if (spec->fromPort != Port::Auto && spec->fromPort != placement->fromPort) {
            ConstraintIssue issue;
            issue.code = ConstraintCode::EdgePortMismatch;
            issue.node = spec->from;
            issue.edge.index = i;
            issue.endpoint = EdgeEndpoint::From;
            issue.port = spec->fromPort;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
        if (spec->toPort != Port::Auto && spec->toPort != placement->toPort) {
            ConstraintIssue issue;
            issue.code = ConstraintCode::EdgePortMismatch;
            issue.node = spec->to;
            issue.edge.index = i;
            issue.endpoint = EdgeEndpoint::To;
            issue.port = spec->toPort;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
        auto fromAttached = _attached(*from, placement->centerline[0],
                                      placement->fromPort);
        if (!fromAttached) {
            ConstraintIssue issue;
            issue.code = ConstraintCode::EdgeEndpointDetached;
            issue.node = spec->from;
            issue.edge.index = i;
            issue.endpoint = EdgeEndpoint::From;
            issue.port = placement->fromPort;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
        auto toAttached = _attached(
            *to, placement->centerline[placement->centerlineCount - 1u],
            placement->toPort);
        if (!toAttached) {
            ConstraintIssue issue;
            issue.code = ConstraintCode::EdgeEndpointDetached;
            issue.node = spec->to;
            issue.edge.index = i;
            issue.endpoint = EdgeEndpoint::To;
            issue.port = placement->toPort;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
        if (fromAttached
            && !_outwardSegment(placement->centerline[0], placement->centerline[1],
                                placement->fromPort)) {
            ConstraintIssue issue;
            issue.code = ConstraintCode::EdgeEndpointDirection;
            issue.node = spec->from;
            issue.edge.index = i;
            issue.endpoint = EdgeEndpoint::From;
            issue.port = placement->fromPort;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
        if (toAttached
            && !_outwardSegment(
                placement->centerline[placement->centerlineCount - 1u],
                placement->centerline[placement->centerlineCount - 2u],
                placement->toPort)) {
            ConstraintIssue issue;
            issue.code = ConstraintCode::EdgeEndpointDirection;
            issue.node = spec->to;
            issue.edge.index = i;
            issue.endpoint = EdgeEndpoint::To;
            issue.port = placement->toPort;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }

        auto margin = _routeMargin(receipt.config());
        for (auto j = 0u; j < receipt.nodeCount(); j++) {
            auto node = receipt.nodePlacementAt(j);
            if (!node) return Result::InvalidArguments;
            auto crosses = false;
            for (auto k = 1u; k < placement->centerlineCount; k++) {
                auto padding = margin;
                if ((j == spec->from.index && k == 1u)
                    || (j == spec->to.index
                        && k + 1u == placement->centerlineCount)) {
                    padding = 0.0f;
                }
                if (!_crosses(placement->centerline[k - 1u], placement->centerline[k],
                              node->center, node->size, padding)) {
                    continue;
                }
                crosses = true;
                break;
            }
            if (!crosses) continue;
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
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].object
               : nullptr;
}

Rectangle* Built::body(Node node) const noexcept
{
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].body
               : nullptr;
}

Text* Built::label(Node node) const noexcept
{
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].label
               : nullptr;
}

Text* Built::detail(Node node) const noexcept
{
    return pImpl && node && node.index < pImpl->nodeCount
               ? pImpl->nodeViews[node.index].detail
               : nullptr;
}

Object* Built::object(Edge edge) const noexcept
{
    return pImpl && edge && edge.index < pImpl->edgeCount
               ? pImpl->edgeViews[edge.index].object
               : nullptr;
}

Text* Built::label(Edge edge) const noexcept
{
    return pImpl && edge && edge.index < pImpl->edgeCount
               ? pImpl->edgeViews[edge.index].label
               : nullptr;
}

Object* Built::object(Zone zone) const noexcept
{
    return pImpl && zone && zone.index < pImpl->zoneCount
               ? pImpl->zoneViews[zone.index].object
               : nullptr;
}

Rectangle* Built::body(Zone zone) const noexcept
{
    return pImpl && zone && zone.index < pImpl->zoneCount
               ? pImpl->zoneViews[zone.index].body
               : nullptr;
}

Text* Built::label(Zone zone) const noexcept
{
    return pImpl && zone && zone.index < pImpl->zoneCount
               ? pImpl->zoneViews[zone.index].label
               : nullptr;
}

const IR* Built::ir() const noexcept
{
    return pImpl ? pImpl->receipt : nullptr;
}

}  // namespace vis::diagram
