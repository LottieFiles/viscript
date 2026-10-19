// The diagram module's physical checks: edges that cross or come too near nodes and zones, and
// the clearance they keep, reported as PhysicalConstraintIssues.
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#include "visDiagram.h"
#include "visDiagramGeometry.h"

namespace vis::diagram
{

struct PhysicalConstraintReport::Impl : detail::IssueList<PhysicalConstraintIssue>
{
};

namespace
{

// The audit measures root output pixels, which take a coarser tolerance than the world units
// of detail::Epsilon.
constexpr float PixelEpsilon = 1.0e-4f;

struct PhysicalVisual
{
    const LayoutVisual* visual = nullptr;
    uint32_t index = 0xffffffffu;
};

struct Segment
{
    Vec2 from;
    Vec2 to;
};

static bool _path(const LayoutReport& layout, uint32_t scene, const char* path) noexcept
{
    auto item = layout.sceneAt(scene);
    return item && item->path && path && std::strcmp(item->path, path) == 0;
}

static bool _applies(const LayoutVisual& visual, uint32_t object) noexcept
{
    return visual.object == object
           || (visual.kind == LayoutVisualKind::Morph && visual.counterpart == object);
}

static PhysicalVisual _visual(const LayoutReport& layout, const char* scenePath,
                              const Object* object) noexcept
{
    if (!scenePath || !object) return {};
    for (auto i = 0u; i < layout.objectCount(); i++) {
        auto item = layout.objectAt(i);
        if (!item || item->object != object || !_path(layout, item->scene, scenePath)) continue;
        PhysicalVisual fallback;
        for (auto j = 0u; j < layout.visualCount(); j++) {
            auto visual = layout.visualAt(j);
            if (!visual || !_applies(*visual, i)) continue;
            if (!fallback.visual) fallback = {visual, j};
            if (visual->visible && !visual->occluded && visual->opacity > 0.0f
                && visual->paintBounds.width > 0.0f
                && visual->paintBounds.height > 0.0f) {
                return {visual, j};
            }
        }
        return fallback;
    }
    return {};
}

// The end of a visual's paths in a Lookup.
constexpr uint32_t NoPath = 0xffffffffu;

struct NodeVisuals
{
    PhysicalVisual body;
    PhysicalVisual label;
    PhysicalVisual detail;
};

struct EdgeVisuals
{
    PhysicalVisual route;
    PhysicalVisual label;
};

// What validatePhysical() asks of the layout for every edge, found once, as each search walks
// the whole report: the visual of each part of the receipt, and the paths of each visual in the
// report's order, those of visual v from firstPath[v] through nextPath[] to NoPath.
struct Lookup
{
    const LayoutReport* layout = nullptr;
    std::unique_ptr<NodeVisuals[]> nodes;
    std::unique_ptr<EdgeVisuals[]> edges;
    std::unique_ptr<PhysicalVisual[]> zones;
    std::unique_ptr<uint32_t[]> firstPath;
    std::unique_ptr<uint32_t[]> nextPath;

    // Fills the tables for receipt in layout's scene at scenePath; false when out of memory.
    bool find(const IR& receipt, const LayoutReport& layout, const char* scenePath) noexcept
    {
        this->layout = &layout;
        nodes.reset(new (std::nothrow) NodeVisuals[receipt.nodeCount() + 1u]);
        edges.reset(new (std::nothrow) EdgeVisuals[receipt.edgeCount() + 1u]);
        zones.reset(new (std::nothrow) PhysicalVisual[receipt.zoneCount() + 1u]);
        firstPath.reset(new (std::nothrow) uint32_t[layout.visualCount() + 1u]);
        nextPath.reset(new (std::nothrow) uint32_t[layout.pathCount() + 1u]);
        if (!nodes || !edges || !zones || !firstPath || !nextPath) return false;
        for (auto i = 0u; i < receipt.nodeCount(); i++) {
            auto node = Node{i};
            nodes[i] = {_visual(layout, scenePath, receipt.body(node)),
                        _visual(layout, scenePath, receipt.label(node)),
                        _visual(layout, scenePath, receipt.detail(node))};
        }
        for (auto i = 0u; i < receipt.edgeCount(); i++) {
            edges[i] = {_visual(layout, scenePath, receipt.object(Edge{i})),
                        _visual(layout, scenePath, receipt.label(Edge{i}))};
        }
        for (auto i = 0u; i < receipt.zoneCount(); i++) {
            zones[i] = _visual(layout, scenePath, receipt.label(Zone{i}));
        }
        // Linked from the last path, so that the paths of each visual keep the report's order.
        for (auto i = 0u; i < layout.visualCount(); i++) firstPath[i] = NoPath;
        for (auto i = layout.pathCount(); i-- > 0u;) {
            auto path = layout.pathAt(i);
            if (!path || path->visual >= layout.visualCount()) continue;
            nextPath[i] = firstPath[path->visual];
            firstPath[path->visual] = i;
        }
        return true;
    }
};

static float _cross(const Vec2& first, const Vec2& second,
                    const Vec2& point) noexcept
{
    return (second.x - first.x) * (point.y - first.y)
           - (second.y - first.y) * (point.x - first.x);
}

static bool _between(float value, float first, float second) noexcept
{
    return value >= std::fmin(first, second) - PixelEpsilon
           && value <= std::fmax(first, second) + PixelEpsilon;
}

// Whether the crosses of two points put them on opposite sides of a line, past the tolerance.
static bool _opposite(float first, float second) noexcept
{
    return (first > PixelEpsilon && second < -PixelEpsilon)
           || (first < -PixelEpsilon && second > PixelEpsilon);
}

// Whether point, whose cross against the line through a and b is cross, lies on segment ab.
static bool _touches(float cross, const Vec2& point, const Vec2& a, const Vec2& b) noexcept
{
    return std::fabs(cross) <= PixelEpsilon && _between(point.x, a.x, b.x)
           && _between(point.y, a.y, b.y);
}

static bool _segmentsIntersect(const Vec2& firstA, const Vec2& firstB,
                               const Vec2& secondA, const Vec2& secondB) noexcept
{
    auto firstStart = _cross(firstA, firstB, secondA);
    auto firstEnd = _cross(firstA, firstB, secondB);
    auto secondStart = _cross(secondA, secondB, firstA);
    auto secondEnd = _cross(secondA, secondB, firstB);
    if (_opposite(firstStart, firstEnd) && _opposite(secondStart, secondEnd)) return true;
    return _touches(firstStart, secondA, firstA, firstB)
           || _touches(firstEnd, secondB, firstA, firstB)
           || _touches(secondStart, firstA, secondA, secondB)
           || _touches(secondEnd, firstB, secondA, secondB);
}

static float _pointSegment(const Vec2& point, const Vec2& from,
                           const Vec2& to) noexcept
{
    auto delta = to - from;
    auto length = delta.dot(delta);
    if (length <= PixelEpsilon * PixelEpsilon) return (point - from).length();
    auto progress = (point - from).dot(delta) / length;
    if (progress < 0.0f) progress = 0.0f;
    else if (progress > 1.0f) progress = 1.0f;
    return (point - (from + delta * progress)).length();
}

static float _segmentDistance(const Vec2& firstA, const Vec2& firstB,
                              const Vec2& secondA, const Vec2& secondB) noexcept
{
    if (_segmentsIntersect(firstA, firstB, secondA, secondB)) return 0.0f;
    return std::fmin(
        std::fmin(_pointSegment(firstA, secondA, secondB),
                  _pointSegment(firstB, secondA, secondB)),
        std::fmin(_pointSegment(secondA, firstA, firstB),
                  _pointSegment(secondB, firstA, firstB)));
}

static bool _inside(const Vec2& point, const LayoutPath& path) noexcept
{
    if (!path.closed || !path.filled || !path.points || path.count < 3u) return false;
    auto inside = false;
    for (auto i = 0u, j = path.count - 1u; i < path.count; j = i++) {
        auto& first = path.points[i];
        auto& second = path.points[j];
        if ((first.y > point.y) == (second.y > point.y)) continue;
        auto crossing = (second.x - first.x) * (point.y - first.y)
                      / (second.y - first.y) + first.x;
        if (point.x < crossing) inside = !inside;
    }
    return inside;
}

static uint32_t _segments(const LayoutPath& path) noexcept
{
    if (!path.points || path.count < 2u) return 0u;
    return path.closed ? path.count : path.count - 1u;
}

static bool _inside(const Vec2& point, const BBox& bounds) noexcept
{
    return point.x >= bounds.x && point.x <= bounds.x + bounds.width
           && point.y >= bounds.y && point.y <= bounds.y + bounds.height;
}

// The part of segment inside bounds, as parameters 0 <= entry <= exit <= 1 along it.
static bool _clip(const Segment& segment, const BBox& bounds,
                  float& entry, float& exit) noexcept
{
    entry = 0.0f;
    exit = 1.0f;
    auto delta = segment.to - segment.from;
    return detail::_axisInterval(segment.from.x, delta.x, bounds.x, bounds.x + bounds.width,
                                 entry, exit, PixelEpsilon)
           && detail::_axisInterval(segment.from.y, delta.y, bounds.y,
                                    bounds.y + bounds.height, entry, exit, PixelEpsilon);
}

static uint32_t _subtract(const Segment& segment, const BBox& bounds,
                          Segment* output) noexcept
{
    float entry;
    float exit;
    if (!_clip(segment, bounds, entry, exit)) {
        output[0] = segment;
        return 1u;
    }
    auto delta = segment.to - segment.from;
    auto count = 0u;
    if (entry > PixelEpsilon) output[count++] = {segment.from, segment.from + delta * entry};
    if (exit < 1.0f - PixelEpsilon) output[count++] = {segment.from + delta * exit, segment.to};
    return count;
}

static uint32_t _outside(const Segment& segment, const BBox* excluded,
                         uint32_t excludedCount, Segment* output) noexcept
{
    Segment current[4] = {segment};
    auto currentCount = 1u;
    for (auto i = 0u; i < excludedCount; i++) {
        Segment next[4];
        auto nextCount = 0u;
        for (auto j = 0u; j < currentCount; j++) {
            Segment pieces[2];
            auto count = _subtract(current[j], excluded[i], pieces);
            for (auto k = 0u; k < count; k++) next[nextCount++] = pieces[k];
        }
        for (auto j = 0u; j < nextCount; j++) current[j] = next[j];
        currentCount = nextCount;
        if (!currentCount) break;
    }
    for (auto i = 0u; i < currentCount; i++) output[i] = current[i];
    return currentCount;
}

static bool _excluded(const Vec2& point, const BBox* excluded,
                      uint32_t excludedCount) noexcept
{
    for (auto i = 0u; i < excludedCount; i++) {
        if (_inside(point, excluded[i])) return true;
    }
    return false;
}

static float _distance(const LayoutPath& first, const LayoutPath& second) noexcept
{
    auto firstSegments = _segments(first);
    auto secondSegments = _segments(second);
    if (!firstSegments || !secondSegments) return std::numeric_limits<float>::infinity();
    auto distance = std::numeric_limits<float>::infinity();
    for (auto i = 0u; i < firstSegments; i++) {
        auto firstNext = (i + 1u) % first.count;
        for (auto j = 0u; j < secondSegments; j++) {
            auto secondNext = (j + 1u) % second.count;
            distance = std::fmin(distance,
                                 _segmentDistance(first.points[i], first.points[firstNext],
                                                  second.points[j], second.points[secondNext]));
            if (distance <= PixelEpsilon) break;
        }
        if (distance <= PixelEpsilon) break;
    }
    if (distance > PixelEpsilon
        && ((_inside(first.points[0], second)) || _inside(second.points[0], first))) {
        distance = 0.0f;
    }
    auto firstRadius = first.stroked ? first.strokeWidth * 0.5f : 0.0f;
    auto secondRadius = second.stroked ? second.strokeWidth * 0.5f : 0.0f;
    return distance - firstRadius - secondRadius;
}

static float _distance(const LayoutPath& first, const LayoutPath& second,
                       const BBox* excluded, uint32_t excludedCount) noexcept
{
    if (!excludedCount) return _distance(first, second);
    auto firstSegments = _segments(first);
    auto secondSegments = _segments(second);
    if (!firstSegments || !secondSegments) return std::numeric_limits<float>::infinity();
    auto distance = std::numeric_limits<float>::infinity();
    for (auto i = 0u; i < firstSegments; i++) {
        Segment firstPieces[4];
        auto firstCount = _outside({first.points[i], first.points[(i + 1u) % first.count]},
                                   excluded, excludedCount, firstPieces);
        for (auto j = 0u; j < secondSegments; j++) {
            Segment secondPieces[4];
            auto secondCount = _outside(
                {second.points[j], second.points[(j + 1u) % second.count]},
                excluded, excludedCount, secondPieces);
            for (auto k = 0u; k < firstCount; k++) {
                for (auto l = 0u; l < secondCount; l++) {
                    distance = std::fmin(
                        distance, _segmentDistance(firstPieces[k].from, firstPieces[k].to,
                                                   secondPieces[l].from, secondPieces[l].to));
                }
            }
        }
    }
    if (distance > PixelEpsilon) {
        for (auto i = 0u; i < first.count; i++) {
            if (!_excluded(first.points[i], excluded, excludedCount)
                && _inside(first.points[i], second)) {
                distance = 0.0f;
                break;
            }
        }
    }
    if (distance > PixelEpsilon) {
        for (auto i = 0u; i < second.count; i++) {
            if (!_excluded(second.points[i], excluded, excludedCount)
                && _inside(second.points[i], first)) {
                distance = 0.0f;
                break;
            }
        }
    }
    auto firstRadius = first.stroked ? first.strokeWidth * 0.5f : 0.0f;
    auto secondRadius = second.stroked ? second.strokeWidth * 0.5f : 0.0f;
    return distance - firstRadius - secondRadius;
}

static float _distance(const Lookup& lookup, uint32_t first, uint32_t second,
                       const BBox* excluded = nullptr, uint32_t excludedCount = 0u) noexcept
{
    auto distance = std::numeric_limits<float>::infinity();
    for (auto i = lookup.firstPath[first]; i != NoPath; i = lookup.nextPath[i]) {
        for (auto j = lookup.firstPath[second]; j != NoPath; j = lookup.nextPath[j]) {
            distance = std::fmin(distance, _distance(*lookup.layout->pathAt(i),
                                                     *lookup.layout->pathAt(j),
                                                     excluded, excludedCount));
        }
    }
    return distance;
}

static float _strokeRadius(const Lookup& lookup, uint32_t visual) noexcept
{
    auto radius = 0.0f;
    for (auto i = lookup.firstPath[visual]; i != NoPath; i = lookup.nextPath[i]) {
        auto path = lookup.layout->pathAt(i);
        if (path->stroked) radius = std::fmax(radius, path->strokeWidth * 0.5f);
    }
    return radius;
}

static BBox _expanded(const BBox& bounds, float amount) noexcept
{
    return {bounds.x - amount, bounds.y - amount,
            bounds.width + amount * 2.0f, bounds.height + amount * 2.0f};
}

static uint32_t _sharedEndpointExclusions(const Lookup& lookup,
                                          const EdgeSpec& first,
                                          const EdgeSpec& second,
                                          uint32_t firstVisual,
                                          uint32_t secondVisual,
                                          float routeGap,
                                          BBox* output) noexcept
{
    Node firstNodes[] = {first.from, first.to};
    Node secondNodes[] = {second.from, second.to};
    auto count = 0u;
    auto margin = routeGap + std::fmax(_strokeRadius(lookup, firstVisual),
                                      _strokeRadius(lookup, secondVisual)) + PixelEpsilon;
    for (auto i = 0u; i < 2u; i++) {
        for (auto j = 0u; j < 2u; j++) {
            if (firstNodes[i].index != secondNodes[j].index) continue;
            auto duplicate = count && firstNodes[i].index == firstNodes[0].index
                             && firstNodes[0].index == firstNodes[1].index;
            if (duplicate) continue;
            auto& body = lookup.nodes[firstNodes[i].index].body;
            if (!body.visual || body.visual->paintBounds.width <= 0.0f
                || body.visual->paintBounds.height <= 0.0f) {
                continue;
            }
            // Keep the intentional fan-in/out local to the sampled endpoint node.
            auto junction = std::fmin(body.visual->paintBounds.width,
                                      body.visual->paintBounds.height) * 0.5f;
            output[count++] = _expanded(body.visual->paintBounds, margin + junction);
            break;
        }
    }
    return count;
}

static float _distance(const Lookup& lookup, uint32_t visual, const BBox& bounds) noexcept
{
    Vec2 points[] = {
        {bounds.x, bounds.y},
        {bounds.x + bounds.width, bounds.y},
        {bounds.x + bounds.width, bounds.y + bounds.height},
        {bounds.x, bounds.y + bounds.height},
    };
    LayoutPath box;
    box.points = points;
    box.count = 4u;
    box.closed = true;
    box.filled = true;
    auto distance = std::numeric_limits<float>::infinity();
    for (auto i = lookup.firstPath[visual]; i != NoPath; i = lookup.nextPath[i]) {
        distance = std::fmin(distance, _distance(*lookup.layout->pathAt(i), box));
    }
    return distance;
}

static float _clearance(const BBox& inner, const BBox& outer) noexcept
{
    auto left = inner.x - outer.x;
    auto top = inner.y - outer.y;
    auto right = outer.x + outer.width - inner.x - inner.width;
    auto bottom = outer.y + outer.height - inner.y - inner.height;
    return std::fmin(std::fmin(left, top), std::fmin(right, bottom));
}

// Whether the visual is missing or cannot be seen, so nothing collides with it.
static bool _hidden(const PhysicalVisual& item) noexcept
{
    return !item.visual || !item.visual->visible || item.visual->occluded
           || item.visual->opacity <= 0.0f;
}

static bool _collision(float actual, float required) noexcept
{
    if (!std::isfinite(actual)) return false;
    return actual < required || (required <= PixelEpsilon && actual <= PixelEpsilon);
}

static PhysicalConstraintIssue _issue(PhysicalConstraintCode code, Edge edge,
                                      float actual, float required,
                                      const BBox& route,
                                      const BBox& related = {}) noexcept
{
    PhysicalConstraintIssue issue;
    issue.code = code;
    issue.edge = edge;
    issue.actual = actual;
    issue.required = required;
    issue.routeBounds = route;
    issue.relatedBounds = related;
    return issue;
}

}  // namespace

PhysicalConstraintReport::PhysicalConstraintReport() noexcept
{
    pImpl = new (std::nothrow) Impl;
}

PhysicalConstraintReport::~PhysicalConstraintReport()
{
    delete pImpl;
}

uint32_t PhysicalConstraintReport::count() const noexcept
{
    return pImpl ? pImpl->count : 0u;
}

const PhysicalConstraintIssue* PhysicalConstraintReport::issueAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->count ? pImpl->issues + index : nullptr;
}

Result validatePhysical(const IR& receipt, const LayoutReport& layout,
                        const char* scenePath, float routeGap, float canvasInset,
                        PhysicalConstraintReport& report) noexcept
{
    if (!report.pImpl) return Result::OutOfMemory;
    report.pImpl->count = 0u;
    if (!scenePath || !scenePath[0] || !std::isfinite(routeGap) || routeGap < 0.0f
        || !std::isfinite(canvasInset) || canvasInset < 0.0f) {
        return Result::InvalidArguments;
    }
    Lookup lookup;
    if (!lookup.find(receipt, layout, scenePath)) return Result::OutOfMemory;

    for (auto i = 0u; i < receipt.edgeCount(); i++) {
        auto edge = Edge{i};
        auto spec = receipt.edgeAt(i);
        auto& route = lookup.edges[i].route;
        if (!route.visual || route.visual->occluded || route.visual->opacity <= 0.0f
            || route.visual->paintBounds.width <= 0.0f
            || route.visual->paintBounds.height <= 0.0f) {
            continue;
        }

        auto canvasClearance = _clearance(route.visual->paintBounds,
                                          route.visual->clipBounds);
        if (route.visual->clipped || canvasClearance < canvasInset) {
            auto issue = _issue(PhysicalConstraintCode::RouteCanvasOverflow, edge,
                                canvasClearance, canvasInset,
                                route.visual->paintBounds, route.visual->clipBounds);
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
        if (!route.visual->visible) continue;

        for (auto j = 0u; j < receipt.nodeCount(); j++) {
            if (j == spec->from.index || j == spec->to.index) continue;
            auto node = Node{j};
            auto& body = lookup.nodes[j].body;
            if (_hidden(body)) continue;
            auto actual = _distance(lookup, route.index, body.index);
            if (!_collision(actual, routeGap)) continue;
            auto issue = _issue(PhysicalConstraintCode::RouteNodeCollision, edge,
                                actual, routeGap, route.visual->paintBounds,
                                body.visual->paintBounds);
            issue.node = node;
            issue.relatedKind = PhysicalEntityKind::NodeBody;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }

        // Reports the label of node, relatedEdge or zone, as kind says, when the route comes
        // too near it.
        auto reportLabel = [&](const PhysicalVisual& label, PhysicalEntityKind kind, Node node,
                               Edge relatedEdge = {}, Zone zone = {}) {
            if (_hidden(label)) return true;
            auto actual = _distance(lookup, route.index, label.visual->paintBounds);
            if (!_collision(actual, routeGap)) return true;
            auto issue = _issue(PhysicalConstraintCode::RouteLabelCollision, edge, actual,
                                routeGap, route.visual->paintBounds,
                                label.visual->paintBounds);
            issue.node = node;
            issue.relatedEdge = relatedEdge;
            issue.zone = zone;
            issue.relatedKind = kind;
            return report.pImpl->add(issue);
        };
        for (auto j = 0u; j < receipt.nodeCount(); j++) {
            if (j == spec->from.index || j == spec->to.index) continue;
            auto node = Node{j};
            if (!reportLabel(lookup.nodes[j].label, PhysicalEntityKind::NodeLabel, node)
                || !reportLabel(lookup.nodes[j].detail, PhysicalEntityKind::NodeDetail, node)) {
                return Result::OutOfMemory;
            }
        }
        for (auto j = 0u; j < receipt.edgeCount(); j++) {
            if (j == i) continue;
            auto relatedEdge = Edge{j};
            if (!reportLabel(lookup.edges[j].label, PhysicalEntityKind::EdgeLabel, {},
                             relatedEdge)) {
                return Result::OutOfMemory;
            }
        }
        for (auto j = 0u; j < receipt.zoneCount(); j++) {
            auto zone = Zone{j};
            if (!reportLabel(lookup.zones[j], PhysicalEntityKind::ZoneLabel, {}, {}, zone)) {
                return Result::OutOfMemory;
            }
        }

        for (auto j = i + 1u; j < receipt.edgeCount(); j++) {
            auto relatedEdge = Edge{j};
            auto relatedSpec = receipt.edgeAt(j);
            auto& related = lookup.edges[j].route;
            if (_hidden(related)) continue;
            BBox excluded[2];
            auto excludedCount = _sharedEndpointExclusions(
                lookup, *spec, *relatedSpec, route.index, related.index, routeGap, excluded);
            auto actual = _distance(lookup, route.index, related.index, excluded, excludedCount);
            if (!_collision(actual, routeGap)) continue;
            auto issue = _issue(PhysicalConstraintCode::RouteRouteCollision, edge,
                                actual, routeGap, route.visual->paintBounds,
                                related.visual->paintBounds);
            issue.relatedEdge = relatedEdge;
            issue.relatedKind = PhysicalEntityKind::EdgeRoute;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }
    }
    return Result::Success;
}

}  // namespace vis::diagram
