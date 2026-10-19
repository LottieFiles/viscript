// The diagram module's physical checks: edges that cross or come too near nodes and zones, and
// the clearance they keep, reported as PhysicalConstraintIssues.
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

#include "vis_diagram.h"

namespace vis::diagram
{

struct PhysicalConstraintReport::Impl
{
    PhysicalConstraintIssue* issues = nullptr;
    uint32_t count = 0u;
    uint32_t capacity = 0u;

    ~Impl()
    {
        delete[] issues;
    }

    bool add(const PhysicalConstraintIssue& issue) noexcept
    {
        if (count == capacity) {
            auto next = capacity ? capacity * 2u : 8u;
            if (next < capacity) return false;
            auto grown = new (std::nothrow) PhysicalConstraintIssue[next];
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

namespace
{

constexpr float Epsilon = 1.0e-4f;

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

static float _cross(const Vec2& first, const Vec2& second,
                    const Vec2& point) noexcept
{
    return (second.x - first.x) * (point.y - first.y)
           - (second.y - first.y) * (point.x - first.x);
}

static bool _between(float value, float first, float second) noexcept
{
    return value >= std::fmin(first, second) - Epsilon
           && value <= std::fmax(first, second) + Epsilon;
}

static bool _segmentsIntersect(const Vec2& firstA, const Vec2& firstB,
                               const Vec2& secondA, const Vec2& secondB) noexcept
{
    auto firstStart = _cross(firstA, firstB, secondA);
    auto firstEnd = _cross(firstA, firstB, secondB);
    auto secondStart = _cross(secondA, secondB, firstA);
    auto secondEnd = _cross(secondA, secondB, firstB);
    if (((firstStart > Epsilon && firstEnd < -Epsilon)
         || (firstStart < -Epsilon && firstEnd > Epsilon))
        && ((secondStart > Epsilon && secondEnd < -Epsilon)
            || (secondStart < -Epsilon && secondEnd > Epsilon))) {
        return true;
    }
    if (std::fabs(firstStart) <= Epsilon && _between(secondA.x, firstA.x, firstB.x)
        && _between(secondA.y, firstA.y, firstB.y)) {
        return true;
    }
    if (std::fabs(firstEnd) <= Epsilon && _between(secondB.x, firstA.x, firstB.x)
        && _between(secondB.y, firstA.y, firstB.y)) {
        return true;
    }
    if (std::fabs(secondStart) <= Epsilon && _between(firstA.x, secondA.x, secondB.x)
        && _between(firstA.y, secondA.y, secondB.y)) {
        return true;
    }
    return std::fabs(secondEnd) <= Epsilon
           && _between(firstB.x, secondA.x, secondB.x)
           && _between(firstB.y, secondA.y, secondB.y);
}

static float _pointSegment(const Vec2& point, const Vec2& from,
                           const Vec2& to) noexcept
{
    auto delta = to - from;
    auto length = delta.dot(delta);
    if (length <= Epsilon * Epsilon) return (point - from).length();
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

static bool _clip(const Segment& segment, const BBox& bounds,
                  float& entry, float& exit) noexcept
{
    entry = 0.0f;
    exit = 1.0f;
    auto delta = segment.to - segment.from;
    auto axis = [&](float start, float change, float minimum, float maximum) {
        if (std::fabs(change) <= Epsilon) return start >= minimum && start <= maximum;
        auto first = (minimum - start) / change;
        auto second = (maximum - start) / change;
        if (first > second) {
            auto swap = first;
            first = second;
            second = swap;
        }
        entry = std::fmax(entry, first);
        exit = std::fmin(exit, second);
        return entry <= exit;
    };
    return axis(segment.from.x, delta.x, bounds.x, bounds.x + bounds.width)
           && axis(segment.from.y, delta.y, bounds.y, bounds.y + bounds.height)
           && exit >= 0.0f && entry <= 1.0f;
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
    entry = std::fmax(0.0f, entry);
    exit = std::fmin(1.0f, exit);
    auto delta = segment.to - segment.from;
    auto count = 0u;
    if (entry > Epsilon) output[count++] = {segment.from, segment.from + delta * entry};
    if (exit < 1.0f - Epsilon) output[count++] = {segment.from + delta * exit, segment.to};
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
            if (distance <= Epsilon) break;
        }
        if (distance <= Epsilon) break;
    }
    if (distance > Epsilon
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
    if (distance > Epsilon) {
        for (auto i = 0u; i < first.count; i++) {
            if (!_excluded(first.points[i], excluded, excludedCount)
                && _inside(first.points[i], second)) {
                distance = 0.0f;
                break;
            }
        }
    }
    if (distance > Epsilon) {
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

static float _distance(const LayoutReport& layout, uint32_t first,
                       uint32_t second) noexcept
{
    auto distance = std::numeric_limits<float>::infinity();
    for (auto i = 0u; i < layout.pathCount(); i++) {
        auto firstPath = layout.pathAt(i);
        if (!firstPath || firstPath->visual != first) continue;
        for (auto j = 0u; j < layout.pathCount(); j++) {
            auto secondPath = layout.pathAt(j);
            if (!secondPath || secondPath->visual != second) continue;
            distance = std::fmin(distance, _distance(*firstPath, *secondPath));
        }
    }
    return distance;
}

static float _distance(const LayoutReport& layout, uint32_t first, uint32_t second,
                       const BBox* excluded, uint32_t excludedCount) noexcept
{
    auto distance = std::numeric_limits<float>::infinity();
    for (auto i = 0u; i < layout.pathCount(); i++) {
        auto firstPath = layout.pathAt(i);
        if (!firstPath || firstPath->visual != first) continue;
        for (auto j = 0u; j < layout.pathCount(); j++) {
            auto secondPath = layout.pathAt(j);
            if (!secondPath || secondPath->visual != second) continue;
            distance = std::fmin(distance,
                                 _distance(*firstPath, *secondPath,
                                           excluded, excludedCount));
        }
    }
    return distance;
}

static float _strokeRadius(const LayoutReport& layout, uint32_t visual) noexcept
{
    auto radius = 0.0f;
    for (auto i = 0u; i < layout.pathCount(); i++) {
        auto path = layout.pathAt(i);
        if (path && path->visual == visual && path->stroked) {
            radius = std::fmax(radius, path->strokeWidth * 0.5f);
        }
    }
    return radius;
}

static BBox _expanded(const BBox& bounds, float amount) noexcept
{
    return {bounds.x - amount, bounds.y - amount,
            bounds.width + amount * 2.0f, bounds.height + amount * 2.0f};
}

static uint32_t _sharedEndpointExclusions(const IR& receipt,
                                          const LayoutReport& layout,
                                          const char* scenePath,
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
    auto margin = routeGap + std::fmax(_strokeRadius(layout, firstVisual),
                                      _strokeRadius(layout, secondVisual)) + Epsilon;
    for (auto i = 0u; i < 2u; i++) {
        for (auto j = 0u; j < 2u; j++) {
            if (firstNodes[i].index != secondNodes[j].index) continue;
            auto duplicate = count && firstNodes[i].index == firstNodes[0].index
                             && firstNodes[0].index == firstNodes[1].index;
            if (duplicate) continue;
            auto body = _visual(layout, scenePath, receipt.body(firstNodes[i]));
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

static float _distance(const LayoutReport& layout, uint32_t visual,
                       const BBox& bounds) noexcept
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
    for (auto i = 0u; i < layout.pathCount(); i++) {
        auto path = layout.pathAt(i);
        if (path && path->visual == visual) {
            distance = std::fmin(distance, _distance(*path, box));
        }
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

static bool _collision(float actual, float required) noexcept
{
    if (!std::isfinite(actual)) return false;
    return actual < required || (required <= Epsilon && actual <= Epsilon);
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

    for (auto i = 0u; i < receipt.edgeCount(); i++) {
        auto edge = Edge{i};
        auto spec = receipt.edgeAt(i);
        if (!spec || spec->from.index >= receipt.nodeCount()
            || spec->to.index >= receipt.nodeCount()) {
            return Result::InvalidArguments;
        }
        auto route = _visual(layout, scenePath, receipt.object(edge));
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
            auto body = _visual(layout, scenePath, receipt.body(node));
            if (!body.visual || !body.visual->visible || body.visual->occluded
                || body.visual->opacity <= 0.0f) {
                continue;
            }
            auto actual = _distance(layout, route.index, body.index);
            if (!_collision(actual, routeGap)) continue;
            auto issue = _issue(PhysicalConstraintCode::RouteNodeCollision, edge,
                                actual, routeGap, route.visual->paintBounds,
                                body.visual->paintBounds);
            issue.node = node;
            issue.relatedKind = PhysicalEntityKind::NodeBody;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }

        for (auto j = 0u; j < receipt.nodeCount(); j++) {
            if (j == spec->from.index || j == spec->to.index) continue;
            auto node = Node{j};
            const Object* labels[] = {receipt.label(node), receipt.detail(node)};
            PhysicalEntityKind kinds[] = {PhysicalEntityKind::NodeLabel,
                                          PhysicalEntityKind::NodeDetail};
            for (auto k = 0u; k < 2u; k++) {
                auto label = _visual(layout, scenePath, labels[k]);
                if (!label.visual || !label.visual->visible || label.visual->occluded
                    || label.visual->opacity <= 0.0f) {
                    continue;
                }
                auto actual = _distance(layout, route.index,
                                        label.visual->paintBounds);
                if (!_collision(actual, routeGap)) continue;
                auto issue = _issue(PhysicalConstraintCode::RouteLabelCollision, edge,
                                    actual, routeGap, route.visual->paintBounds,
                                    label.visual->paintBounds);
                issue.node = node;
                issue.relatedKind = kinds[k];
                if (!report.pImpl->add(issue)) return Result::OutOfMemory;
            }
        }

        for (auto j = 0u; j < receipt.edgeCount(); j++) {
            if (j == i) continue;
            auto relatedEdge = Edge{j};
            auto label = _visual(layout, scenePath, receipt.label(relatedEdge));
            if (!label.visual || !label.visual->visible || label.visual->occluded
                || label.visual->opacity <= 0.0f) {
                continue;
            }
            auto actual = _distance(layout, route.index, label.visual->paintBounds);
            if (!_collision(actual, routeGap)) continue;
            auto issue = _issue(PhysicalConstraintCode::RouteLabelCollision, edge,
                                actual, routeGap, route.visual->paintBounds,
                                label.visual->paintBounds);
            issue.relatedEdge = relatedEdge;
            issue.relatedKind = PhysicalEntityKind::EdgeLabel;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }

        for (auto j = 0u; j < receipt.zoneCount(); j++) {
            auto zone = Zone{j};
            auto label = _visual(layout, scenePath, receipt.label(zone));
            if (!label.visual || !label.visual->visible || label.visual->occluded
                || label.visual->opacity <= 0.0f) {
                continue;
            }
            auto actual = _distance(layout, route.index, label.visual->paintBounds);
            if (!_collision(actual, routeGap)) continue;
            auto issue = _issue(PhysicalConstraintCode::RouteLabelCollision, edge,
                                actual, routeGap, route.visual->paintBounds,
                                label.visual->paintBounds);
            issue.zone = zone;
            issue.relatedKind = PhysicalEntityKind::ZoneLabel;
            if (!report.pImpl->add(issue)) return Result::OutOfMemory;
        }

        for (auto j = i + 1u; j < receipt.edgeCount(); j++) {
            auto relatedEdge = Edge{j};
            auto relatedSpec = receipt.edgeAt(j);
            if (!relatedSpec || relatedSpec->from.index >= receipt.nodeCount()
                || relatedSpec->to.index >= receipt.nodeCount()) {
                return Result::InvalidArguments;
            }
            auto related = _visual(layout, scenePath, receipt.object(relatedEdge));
            if (!related.visual || !related.visual->visible || related.visual->occluded
                || related.visual->opacity <= 0.0f) {
                continue;
            }
            BBox excluded[2];
            auto excludedCount = _sharedEndpointExclusions(
                receipt, layout, scenePath, *spec, *relatedSpec,
                route.index, related.index, routeGap, excluded);
            auto actual = _distance(layout, route.index, related.index,
                                    excluded, excludedCount);
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
