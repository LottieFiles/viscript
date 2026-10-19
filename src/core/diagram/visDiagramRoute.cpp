// The diagram module's edge routing: orthogonal paths between ports that keep clear of nodes.
#include <cmath>

#include "visDiagram.h"
#include "visDiagramGeometry.h"
#include "../common/visFinite.h"

namespace vis::diagram::detail
{

struct RouteBounds
{
    float minimumX = 0.0f;
    float minimumY = 0.0f;
    float maximumX = 0.0f;
    float maximumY = 0.0f;
    bool valid = false;
};

struct RouteChoice
{
    Vec2 points[Diagram::WaypointLimit + 4u];
    float length = 0.0f;
    uint32_t count = 0u;
    uint32_t bends = 0u;
};

static uint32_t _simplify(Vec2* points, uint32_t count) noexcept
{
    auto output = 1u;
    for (auto i = 1u; i < count; i++) {
        if (_same(points[output - 1u], points[i])) continue;
        points[output++] = points[i];
        while (output >= 3u) {
            auto a = points[output - 3u];
            auto b = points[output - 2u];
            auto c = points[output - 1u];
            auto ab = b - a;
            auto bc = c - b;
            auto collinear = (_near(ab.x, 0.0f) && _near(bc.x, 0.0f)
                              && ab.y * bc.y > 0.0f)
                             || (_near(ab.y, 0.0f) && _near(bc.y, 0.0f)
                                 && ab.x * bc.x > 0.0f);
            if (!collinear) break;
            points[output - 2u] = points[output - 1u];
            output--;
        }
    }
    return output;
}

bool axisAligned(const Vec2* points, uint32_t count) noexcept
{
    if (count < 2u) return false;
    for (auto i = 1u; i < count; i++) {
        auto delta = points[i] - points[i - 1u];
        if (delta.length() <= Epsilon) return false;
        if (!_near(delta.x, 0.0f) && !_near(delta.y, 0.0f)) return false;
        if (i < 2u) continue;
        auto previous = points[i - 1u] - points[i - 2u];
        if (previous.dot(delta) < -Epsilon && _near(previous.x, 0.0f) == _near(delta.x, 0.0f)) {
            return false;
        }
    }
    return true;
}

static uint32_t _initialRoute(const Config& config, const EdgeRecord& edge,
                              const EdgeLayout& layout, Route route, Vec2* output) noexcept
{
    auto count = 0u;
    output[count++] = layout.from;
    for (auto i = 0u; i < edge.waypointCount; i++) output[count++] = edge.waypoints[i];
    if (!edge.waypointCount && route == Route::Orthogonal
        && !_near(layout.from.x, layout.to.x) && !_near(layout.from.y, layout.to.y)) {
        if (config.direction == Direction::LeftToRight) {
            auto middle = (layout.from.x + layout.to.x) * 0.5f;
            output[count++] = {middle, layout.from.y};
            output[count++] = {middle, layout.to.y};
        } else {
            auto middle = (layout.from.y + layout.to.y) * 0.5f;
            output[count++] = {layout.from.x, middle};
            output[count++] = {layout.to.x, middle};
        }
    }
    output[count++] = layout.to;
    return edge.waypointCount ? count : _simplify(output, count);
}

static bool _endpointDirections(const Vec2* points, uint32_t count,
                                const EdgeLayout& layout) noexcept
{
    return _outwardSegment(points[0], points[1], layout.fromPort)
           && _outwardSegment(points[count - 1u], points[count - 2u],
                              layout.toPort);
}

static bool _drawable(const Config& config, const EdgeRecord& edge, Route route,
                      const Vec2* points, uint32_t count) noexcept
{
    if (route != Route::Orthogonal || edge.kind == EdgeKind::Relation
        || _dashed(edge)) {
        return true;
    }
    return (points[count - 1u] - points[count - 2u]).length() > config.arrowLength + Epsilon;
}

static bool _routeClear(const Vec2* points, uint32_t count, const NodeLayout* nodes,
                        uint32_t nodeCount, const EdgeRecord& edge,
                        float margin) noexcept
{
    if (!axisAligned(points, count)) return false;
    for (auto i = 0u; i < nodeCount; i++) {
        if (_routeCrosses(points, count, nodes[i].center, nodes[i].size, margin,
                          i == edge.from.index, i == edge.to.index)) {
            return false;
        }
    }
    return true;
}

static void _include(RouteBounds& bounds, const NodeLayout& node,
                     float padding) noexcept
{
    auto minimumX = node.center.x - node.size.x * 0.5f - padding;
    auto minimumY = node.center.y - node.size.y * 0.5f - padding;
    auto maximumX = node.center.x + node.size.x * 0.5f + padding;
    auto maximumY = node.center.y + node.size.y * 0.5f + padding;
    if (!bounds.valid) {
        bounds = {minimumX, minimumY, maximumX, maximumY, true};
        return;
    }
    bounds.minimumX = std::fmin(bounds.minimumX, minimumX);
    bounds.minimumY = std::fmin(bounds.minimumY, minimumY);
    bounds.maximumX = std::fmax(bounds.maximumX, maximumX);
    bounds.maximumY = std::fmax(bounds.maximumY, maximumY);
}

static RouteBounds _blockingBounds(const Vec2* points, uint32_t count,
                                   const NodeLayout* nodes, uint32_t nodeCount,
                                   const EdgeRecord& edge, float margin) noexcept
{
    RouteBounds bounds;
    for (auto i = 0u; i < nodeCount; i++) {
        if (_routeCrosses(points, count, nodes[i].center, nodes[i].size, margin,
                          i == edge.from.index, i == edge.to.index)) {
            _include(bounds, nodes[i], margin);
        }
    }
    return bounds;
}

static uint32_t _elbow(const EdgeLayout& layout, float stub, bool horizontal,
                       Vec2* output) noexcept
{
    auto from = layout.from + _outward(layout.fromPort) * stub;
    auto to = layout.to + _outward(layout.toPort) * stub;
    auto count = 0u;
    output[count++] = layout.from;
    output[count++] = from;
    output[count++] = horizontal ? Vec2{to.x, from.y} : Vec2{from.x, to.y};
    output[count++] = to;
    output[count++] = layout.to;
    return _simplify(output, count);
}

static uint32_t _corridor(const EdgeLayout& layout, float fromStub, float toStub,
                          float coordinate, bool horizontal,
                          Vec2* output) noexcept
{
    auto from = layout.from + _outward(layout.fromPort) * fromStub;
    auto to = layout.to + _outward(layout.toPort) * toStub;
    auto count = 0u;
    output[count++] = layout.from;
    output[count++] = from;
    output[count++] = horizontal ? Vec2{from.x, coordinate}
                                 : Vec2{coordinate, from.y};
    output[count++] = horizontal ? Vec2{to.x, coordinate}
                                 : Vec2{coordinate, to.y};
    output[count++] = to;
    output[count++] = layout.to;
    return _simplify(output, count);
}

static void _consider(const Config& config, const Vec2* points, uint32_t count,
                      const EdgeLayout& layout, const NodeLayout* nodes,
                      uint32_t nodeCount, const EdgeRecord& edge, float margin,
                      RouteChoice& choice) noexcept
{
    if (!_routeClear(points, count, nodes, nodeCount, edge, margin)
        || !_endpointDirections(points, count, layout)
        || !_drawable(config, edge, Route::Orthogonal, points, count)) {
        return;
    }
    auto length = 0.0f;
    for (auto i = 1u; i < count; i++) length += (points[i] - points[i - 1u]).length();
    auto bends = count > 2u ? count - 2u : 0u;
    if (choice.count && length > choice.length - Epsilon) {
        if (!_near(length, choice.length) || bends >= choice.bends) return;
    }
    choice.length = length;
    choice.count = count;
    choice.bends = bends;
    for (auto i = 0u; i < count; i++) choice.points[i] = points[i];
}

static void _considerBounds(const Config& config, const EdgeLayout& layout,
                            const NodeLayout* nodes, uint32_t nodeCount,
                            const EdgeRecord& edge, const RouteBounds& bounds,
                            float fromStub, float toStub, float margin,
                            RouteChoice& choice) noexcept
{
    if (!bounds.valid) return;
    Vec2 points[Diagram::WaypointLimit + 4u];
    auto gap = config.routeWidth + Epsilon * 2.0f;
    auto consider = [&](bool horizontal) {
        auto maximum = horizontal ? bounds.maximumY : bounds.maximumX;
        auto minimum = horizontal ? bounds.minimumY : bounds.minimumX;
        auto count = _corridor(layout, fromStub, toStub, maximum + gap,
                               horizontal, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
        count = _corridor(layout, fromStub, toStub, minimum - gap,
                          horizontal, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
    };
    auto horizontal = config.direction == Direction::LeftToRight;
    consider(horizontal);
    consider(!horizontal);
}

static float _outerStub(const Vec2& endpoint, Port port,
                        const RouteBounds& bounds, float gap,
                        float minimum) noexcept
{
    auto distance = minimum;
    switch (port) {
        case Port::Left: distance = endpoint.x - bounds.minimumX + gap; break;
        case Port::Right: distance = bounds.maximumX - endpoint.x + gap; break;
        case Port::Top: distance = bounds.maximumY - endpoint.y + gap; break;
        case Port::Bottom: distance = endpoint.y - bounds.minimumY + gap; break;
        case Port::Auto: break;
    }
    return std::fmax(minimum, distance);
}

uint32_t centerline(const Config& config, const EdgeRecord& edge,
                            const EdgeLayout& layout, Route& route,
                            const NodeLayout* nodes, uint32_t nodeCount,
                            Vec2* output) noexcept
{
    auto count = _initialRoute(config, edge, layout, route, output);
    if (edge.waypointCount || edge.route == Route::Straight) {
        return count;
    }

    auto margin = _routeMargin(config);
    if (_routeClear(output, count, nodes, nodeCount, edge, margin)
        && _endpointDirections(output, count, layout)
        && _drawable(config, edge, route, output, count)) {
        return count;
    }
    auto stub = std::fmax(margin, config.arrowLength + config.routeWidth);
    RouteChoice choice;
    Vec2 points[Diagram::WaypointLimit + 4u];
    auto horizontal = config.direction == Direction::LeftToRight;
    auto candidateCount = _elbow(layout, stub, horizontal, points);
    _consider(config, points, candidateCount, layout, nodes, nodeCount, edge,
              margin, choice);
    candidateCount = _elbow(layout, stub, !horizontal, points);
    _consider(config, points, candidateCount, layout, nodes, nodeCount, edge,
              margin, choice);

    auto blocked = _blockingBounds(output, count, nodes, nodeCount, edge, margin);
    _considerBounds(config, layout, nodes, nodeCount, edge, blocked, stub, stub,
                    margin, choice);
    RouteBounds all;
    for (auto i = 0u; i < nodeCount; i++) _include(all, nodes[i], margin);
    _considerBounds(config, layout, nodes, nodeCount, edge, all, stub, stub,
                    margin, choice);
    auto gap = config.routeWidth + Epsilon * 2.0f;
    auto fromOuter = _outerStub(layout.from, layout.fromPort, all, gap, stub);
    auto toOuter = _outerStub(layout.to, layout.toPort, all, gap, stub);
    _considerBounds(config, layout, nodes, nodeCount, edge, all, fromOuter, stub,
                    margin, choice);
    _considerBounds(config, layout, nodes, nodeCount, edge, all, stub, toOuter,
                    margin, choice);
    _considerBounds(config, layout, nodes, nodeCount, edge, all, fromOuter,
                    toOuter, margin, choice);
    if (!choice.count) return count;
    for (auto i = 0u; i < choice.count; i++) output[i] = choice.points[i];
    route = Route::Orthogonal;
    return choice.count;
}

static Vec2 _normal(const Vec2& delta) noexcept
{
    auto length = delta.length();
    return {-delta.y / length, delta.x / length};
}

Path* orthogonalPath(const Config& config, const Vec2* route,
                             uint32_t count) noexcept
{
    if (!axisAligned(route, count)) return nullptr;
    auto last = route[count - 1u] - route[count - 2u];
    auto lastLength = last.length();
    if (lastLength <= config.arrowLength + Epsilon) return nullptr;
    auto direction = last * (1.0f / lastLength);
    auto normal = Vec2{-direction.y, direction.x};
    auto base = route[count - 1u] - direction * config.arrowLength;

    Vec2 shaft[Diagram::WaypointLimit + 4u];
    auto shaftCount = count;
    for (auto i = 0u; i + 1u < count; i++) shaft[i] = route[i];
    shaft[shaftCount - 1u] = base;
    if (!axisAligned(shaft, shaftCount)) return nullptr;

    Vec2 left[Diagram::WaypointLimit + 4u];
    Vec2 right[Diagram::WaypointLimit + 4u];
    auto half = config.routeWidth * 0.5f;
    // Each point moves half the width along its segment's normal. At a right-angle bend the
    // two normals add up to the corner where the offset sides meet.
    for (auto i = 0u; i < shaftCount; i++) {
        auto previous = _normal(i ? shaft[i] - shaft[i - 1u] : shaft[1u] - shaft[0u]);
        auto next = i + 1u < shaftCount ? _normal(shaft[i + 1u] - shaft[i]) : previous;
        auto offset = _same(previous, next) ? previous * half : (previous + next) * half;
        left[i] = shaft[i] + offset;
        right[i] = shaft[i] - offset;
    }

    PathCommand commands[(Diagram::WaypointLimit + 4u) * 2u + 5u];
    auto commandCount = 0u;
    commands[commandCount++] = PathCommand::move({left[0].x, left[0].y, 0.0f});
    for (auto i = 1u; i < shaftCount; i++) {
        commands[commandCount++] = PathCommand::line({left[i].x, left[i].y, 0.0f});
    }
    auto headHalf = config.arrowWidth * 0.5f;
    auto headLeft = base + normal * headHalf;
    auto headRight = base - normal * headHalf;
    commands[commandCount++] = PathCommand::line({headLeft.x, headLeft.y, 0.0f});
    commands[commandCount++] = PathCommand::line(
        {route[count - 1u].x, route[count - 1u].y, 0.0f});
    commands[commandCount++] = PathCommand::line({headRight.x, headRight.y, 0.0f});
    for (auto i = shaftCount; i > 0u; i--) {
        auto point = right[i - 1u];
        commands[commandCount++] = PathCommand::line({point.x, point.y, 0.0f});
    }
    commands[commandCount++] = PathCommand::close();
    return Path::gen(commands, commandCount, 1u);
}

Vec2 labelPoint(const Config& config, const Vec2* points,
                        uint32_t count) noexcept
{
    auto total = 0.0f;
    for (auto i = 1u; i < count; i++) total += (points[i] - points[i - 1u]).length();
    auto target = total * 0.5f;
    auto cursor = 0.0f;
    for (auto i = 1u; i < count; i++) {
        auto segment = points[i] - points[i - 1u];
        auto length = segment.length();
        if (cursor + length >= target && length > Epsilon) {
            auto progress = (target - cursor) / length;
            auto point = points[i - 1u] + segment * progress;
            auto normal = _normal(segment);
            if (normal.y < -Epsilon
                || (_near(normal.y, 0.0f) && normal.x < 0.0f)) {
                normal = normal * -1.0f;
            }
            // A corner that follows the Theme clears the label as the former 0.12 default
            // did, so labels keep their place whatever the Theme rounds.
            auto corner = config.corner >= 0.0f ? config.corner : 0.12f;
            auto gap = std::fmax(config.routeWidth * 2.0f, corner + 0.12f);
            return point + normal * gap;
        }
        cursor += length;
    }
    return points[count - 1u];
}

static Vec2 _port(const NodeLayout& node, Port port, uint32_t ordinal,
                  uint32_t count) noexcept
{
    auto fraction = (static_cast<float>(ordinal) + 1.0f)
                    / (static_cast<float>(count) + 1.0f) - 0.5f;
    switch (port) {
        case Port::Left:
            return {node.center.x - node.size.x * 0.5f,
                    node.center.y + fraction * node.size.y * 0.7f};
        case Port::Right:
            return {node.center.x + node.size.x * 0.5f,
                    node.center.y + fraction * node.size.y * 0.7f};
        case Port::Top:
            return {node.center.x + fraction * node.size.x * 0.7f,
                    node.center.y + node.size.y * 0.5f};
        case Port::Bottom:
            return {node.center.x + fraction * node.size.x * 0.7f,
                    node.center.y - node.size.y * 0.5f};
        case Port::Auto: break;
    }
    return node.center;
}

Result layoutEdges(const Config& config, const EdgeRecord* edges, uint32_t edgeCount,
                   const NodeLayout* layouts, EdgeLayout* edgeLayouts) noexcept
{
    // Edges per node side, indexed by the resolved Port, which is never Auto.
    uint32_t portCount[Diagram::NodeLimit][5]{};
    uint32_t portCursor[Diagram::NodeLimit][5]{};

    auto horizontal = config.direction == Direction::LeftToRight;
    auto resolvePort = [&](const NodeLayout& from, const NodeLayout& to, Port requested, bool destination) {
        if (requested != Port::Auto) return requested;
        auto forward = horizontal ? to.center.x >= from.center.x : to.center.y <= from.center.y;
        if (destination) forward = !forward;
        if (horizontal) return forward ? Port::Right : Port::Left;
        return forward ? Port::Bottom : Port::Top;
    };

    for (auto i = 0u; i < edgeCount; i++) {
        auto& edge = edges[i];
        auto& layout = edgeLayouts[i];
        layout.fromPort = resolvePort(layouts[edge.from.index], layouts[edge.to.index], edge.fromPort, false);
        layout.toPort = resolvePort(layouts[edge.from.index], layouts[edge.to.index], edge.toPort, true);
        portCount[edge.from.index][static_cast<uint8_t>(layout.fromPort)]++;
        portCount[edge.to.index][static_cast<uint8_t>(layout.toPort)]++;
    }
    for (auto i = 0u; i < edgeCount; i++) {
        auto& edge = edges[i];
        auto& layout = edgeLayouts[i];
        auto fromSide = static_cast<uint8_t>(layout.fromPort);
        auto toSide = static_cast<uint8_t>(layout.toPort);
        layout.from = _port(layouts[edge.from.index], layout.fromPort,
                            portCursor[edge.from.index][fromSide]++,
                            portCount[edge.from.index][fromSide]);
        layout.to = _port(layouts[edge.to.index], layout.toPort,
                          portCursor[edge.to.index][toSide]++,
                          portCount[edge.to.index][toSide]);
        if (!_finite(layout.from) || !_finite(layout.to)) return Result::InvalidArguments;
    }

    return Result::Success;
}

}  // namespace vis::diagram::detail
