#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

#include "vis_diagram.h"

namespace vis::diagram
{

namespace
{

constexpr uint32_t LabelLimit = 1023u;
constexpr float Epsilon = 1.0e-5f;

struct NodeRecord
{
    char* id = nullptr;
    char* label = nullptr;
    char* detail = nullptr;
    NodeKind kind = NodeKind::Default;
    int32_t rank = -1;
    int32_t row = -1;
    int32_t column = -1;
    float start = 0.0f;
    float span = 0.0f;
    Vec2 position;
    Vec2 size;
    bool manualPosition = false;
};

struct EdgeRecord
{
    char* id = nullptr;
    char* label = nullptr;
    Node from;
    Node to;
    Port fromPort = Port::Auto;
    Port toPort = Port::Auto;
    Route route = Route::Auto;
    EdgeKind kind = EdgeKind::Directed;
    Vec2* waypoints = nullptr;
    uint32_t waypointCount = 0;
    float flowOffset = 0.0f;
    bool flow = false;
};

struct ZoneRecord
{
    char* id = nullptr;
    char* label = nullptr;
    Node* members = nullptr;
    uint32_t memberCount = 0;
    bool fullWidth = false;
};

struct NodeLayout
{
    Vec2 center;
    Vec2 size;
    uint32_t rank = 0;
};

struct EdgeLayout
{
    Port fromPort = Port::Auto;
    Port toPort = Port::Auto;
    Vec2 from;
    Vec2 to;
};

struct ZoneLayout
{
    Vec2 center;
    Vec2 size;
};

struct ShiftInterval
{
    double minimum = 0.0;
    double maximum = 0.0;
};

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

static bool _finite(float value) noexcept
{
    return std::isfinite(value);
}

static bool _finite(const Vec2& value) noexcept
{
    return _finite(value.x) && _finite(value.y);
}

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

static char* _copy(const char* value) noexcept
{
    if (!value) return nullptr;
    auto length = std::strlen(value);
    auto output = new (std::nothrow) char[length + 1u];
    if (!output) return nullptr;
    std::memcpy(output, value, length + 1u);
    return output;
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
           && _finite(config.corner) && config.corner >= 0.0f;
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

static uint32_t _portIndex(Port port) noexcept
{
    switch (port) {
        case Port::Left: return 0u;
        case Port::Right: return 1u;
        case Port::Top: return 2u;
        case Port::Bottom: return 3u;
        case Port::Auto: break;
    }
    return 0u;
}

static Vec2 _sub(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

static Vec2 _add(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

static Vec2 _mul(const Vec2& value, float scalar) noexcept
{
    return {value.x * scalar, value.y * scalar};
}

static float _length(const Vec2& value) noexcept
{
    return std::hypot(value.x, value.y);
}

static bool _near(float lhs, float rhs) noexcept
{
    return std::fabs(lhs - rhs) <= Epsilon;
}

static bool _same(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return _near(lhs.x, rhs.x) && _near(lhs.y, rhs.y);
}

static bool _axisOverlap(float firstCenter, float firstSize, float secondCenter,
                         float secondSize) noexcept
{
    auto firstMin = firstCenter - firstSize * 0.5f;
    auto firstMax = firstCenter + firstSize * 0.5f;
    auto secondMin = secondCenter - secondSize * 0.5f;
    auto secondMax = secondCenter + secondSize * 0.5f;
    return std::fmin(firstMax, secondMax) - std::fmax(firstMin, secondMin)
           > Epsilon;
}

static bool _boundsOverlap(const Vec2& firstCenter, const Vec2& firstSize,
                           const Vec2& secondCenter,
                           const Vec2& secondSize) noexcept
{
    return _axisOverlap(firstCenter.x, firstSize.x, secondCenter.x, secondSize.x)
           && _axisOverlap(firstCenter.y, firstSize.y, secondCenter.y,
                           secondSize.y);
}

static bool _axisInterval(float origin, float delta, float minimum, float maximum,
                          float& first, float& last) noexcept
{
    if (_near(delta, 0.0f)) return origin >= minimum && origin <= maximum;
    auto begin = (minimum - origin) / delta;
    auto end = (maximum - origin) / delta;
    if (begin > end) {
        auto swap = begin;
        begin = end;
        end = swap;
    }
    first = std::fmax(first, begin);
    last = std::fmin(last, end);
    return first <= last;
}

static bool _crosses(const Vec2& from, const Vec2& to, const Vec2& center,
                     const Vec2& size, float padding = 0.0f) noexcept
{
    auto half = _add(_mul(size, 0.5f), {padding, padding});
    auto minimum = Vec2{center.x - half.x + Epsilon,
                        center.y - half.y + Epsilon};
    auto maximum = Vec2{center.x + half.x - Epsilon,
                        center.y + half.y - Epsilon};
    if (minimum.x > maximum.x || minimum.y > maximum.y) return false;
    auto delta = _sub(to, from);
    auto first = 0.0f;
    auto last = 1.0f;
    return _axisInterval(from.x, delta.x, minimum.x, maximum.x, first, last)
           && _axisInterval(from.y, delta.y, minimum.y, maximum.y, first, last);
}

static bool _primaryOverlap(Direction direction, const NodeLayout& first,
                            const NodeLayout& second) noexcept
{
    return direction == Direction::LeftToRight
               ? _axisOverlap(first.center.x, first.size.x, second.center.x,
                              second.size.x)
               : _axisOverlap(first.center.y, first.size.y, second.center.y,
                              second.size.y);
}

static double _secondaryMinimum(Direction direction, const NodeLayout& node) noexcept
{
    return direction == Direction::LeftToRight
               ? static_cast<double>(node.center.y) - node.size.y * 0.5
               : static_cast<double>(node.center.x) - node.size.x * 0.5;
}

static double _secondaryMaximum(Direction direction, const NodeLayout& node) noexcept
{
    return direction == Direction::LeftToRight
               ? static_cast<double>(node.center.y) + node.size.y * 0.5
               : static_cast<double>(node.center.x) + node.size.x * 0.5;
}

static bool _layoutOverlap(const NodeLayout& first, const NodeLayout& second) noexcept
{
    return _boundsOverlap(first.center, first.size, second.center, second.size);
}

static double _floatResolution(float value) noexcept
{
    auto upper = std::nextafter(value, std::numeric_limits<float>::max());
    auto lower = std::nextafter(value, std::numeric_limits<float>::lowest());
    return std::fmax(std::fabs(static_cast<double>(upper) - value),
                     std::fabs(static_cast<double>(value) - lower));
}

static bool _secondarySeparated(Direction direction, const NodeLayout& first,
                                const NodeLayout& second, float gap) noexcept
{
    auto firstCenter = direction == Direction::LeftToRight ? first.center.y
                                                            : first.center.x;
    auto secondCenter = direction == Direction::LeftToRight ? second.center.y
                                                             : second.center.x;
    // A stored float center cannot promise spacing below its local ULP. Canonical
    // overlap is checked separately and is never excused by this tolerance.
    auto tolerance = static_cast<double>(Epsilon) + _floatResolution(firstCenter)
                     + _floatResolution(secondCenter);
    return _secondaryMaximum(direction, first) + static_cast<double>(gap)
               <= _secondaryMinimum(direction, second) + tolerance
           || _secondaryMaximum(direction, second) + static_cast<double>(gap)
                  <= _secondaryMinimum(direction, first) + tolerance;
}

static bool _rankSeparated(const Config& config, const NodeRecord* nodes,
                           uint32_t count, uint32_t rank,
                           const NodeLayout* layouts,
                           uint32_t* firstFailure = nullptr,
                           uint32_t* secondFailure = nullptr) noexcept
{
    if (firstFailure) *firstFailure = Node::Invalid;
    if (secondFailure) *secondFailure = Node::Invalid;
    auto previous = Node::Invalid;
    for (auto i = 0u; i < count; i++) {
        if (layouts[i].rank != rank || nodes[i].manualPosition) continue;
        if (previous != Node::Invalid) {
            if (_layoutOverlap(layouts[previous], layouts[i])
                || !_secondarySeparated(config.direction, layouts[previous],
                                        layouts[i], config.nodeGap)) {
                if (firstFailure) *firstFailure = previous;
                if (secondFailure) *secondFailure = i;
                return false;
            }
        }
        previous = i;
        for (auto j = 0u; j < count; j++) {
            if (!nodes[j].manualPosition
                || !_primaryOverlap(config.direction, layouts[i], layouts[j])) {
                continue;
            }
            if (_layoutOverlap(layouts[i], layouts[j])
                || !_secondarySeparated(config.direction, layouts[i], layouts[j],
                                        config.nodeGap)) {
                if (firstFailure) *firstFailure = i;
                return false;
            }
        }
    }
    return true;
}

static bool _translateRank(const Config& config, const NodeRecord* nodes,
                           uint32_t count, uint32_t rank, const NodeLayout* layouts,
                           double shift, NodeLayout* translated,
                           uint32_t* firstFailure = nullptr,
                           uint32_t* secondFailure = nullptr) noexcept
{
    if (firstFailure) *firstFailure = Node::Invalid;
    if (secondFailure) *secondFailure = Node::Invalid;
    std::memcpy(translated, layouts, sizeof(NodeLayout) * count);
    for (auto i = 0u; i < count; i++) {
        if (layouts[i].rank != rank || nodes[i].manualPosition) continue;
        auto current = config.direction == Direction::LeftToRight
                           ? static_cast<double>(layouts[i].center.y)
                           : static_cast<double>(layouts[i].center.x);
        auto moved = current + shift;
        if (!std::isfinite(moved)
            || std::fabs(moved) > std::numeric_limits<float>::max()) {
            return false;
        }
        auto rounded = static_cast<float>(moved);
        if (!std::isfinite(rounded)) return false;
        if (config.direction == Direction::LeftToRight) {
            translated[i].center.y = rounded;
        } else {
            translated[i].center.x = rounded;
        }
    }
    return _rankSeparated(config, nodes, count, rank, translated, firstFailure,
                          secondFailure);
}

static bool _outwardBreakpoints(const Config& config, const NodeLayout* layouts,
                                uint32_t index, double shift, int direction,
                                double& tie, double& beyond) noexcept
{
    auto limit = direction > 0 ? std::numeric_limits<float>::max()
                               : std::numeric_limits<float>::lowest();
    auto current = config.direction == Direction::LeftToRight
                       ? static_cast<double>(layouts[index].center.y)
                       : static_cast<double>(layouts[index].center.x);
    auto moved = current + shift;
    if (!std::isfinite(moved)
        || std::fabs(moved) > std::numeric_limits<float>::max()) {
        return false;
    }
    auto rounded = static_cast<float>(moved);
    auto next = std::nextafter(rounded, limit);
    if (!std::isfinite(next) || next == rounded) return false;
    auto midpoint = (static_cast<double>(rounded) + static_cast<double>(next))
                    * 0.5;
    tie = midpoint - current;
    // Advance in moved-value space. Advancing the shift itself can be absorbed
    // when a large center is added back to it.
    auto movedBeyond = std::nextafter(midpoint, static_cast<double>(next));
    beyond = movedBeyond - current;
    auto doubleLimit = direction > 0 ? std::numeric_limits<double>::max()
                                     : std::numeric_limits<double>::lowest();
    for (auto attempt = 0u; attempt < 4u; attempt++) {
        auto outward = direction > 0 ? beyond > shift && beyond > tie
                                     : beyond < shift && beyond < tie;
        auto advanced = static_cast<float>(current + beyond);
        auto changed = std::isfinite(advanced)
                       && (direction > 0 ? advanced > rounded
                                         : advanced < rounded);
        if (std::isfinite(tie) && std::isfinite(beyond) && outward
            && changed) {
            return true;
        }
        auto candidate = std::nextafter(beyond, doubleLimit);
        if (!std::isfinite(candidate) || candidate == beyond) return false;
        beyond = candidate;
    }
    return false;
}

static bool _shiftLess(double first, double second) noexcept
{
    auto firstMagnitude = std::fabs(first);
    auto secondMagnitude = std::fabs(second);
    return firstMagnitude == secondMagnitude ? first > second
                                             : firstMagnitude < secondMagnitude;
}

static bool _separationAnchorShift(const Config& config,
                                   const NodeLayout& first,
                                   const NodeLayout& second, int direction,
                                   double& shift) noexcept
{
    auto firstCenter = config.direction == Direction::LeftToRight
                           ? static_cast<double>(first.center.y)
                           : static_cast<double>(first.center.x);
    auto secondCenter = config.direction == Direction::LeftToRight
                            ? static_cast<double>(second.center.y)
                            : static_cast<double>(second.center.x);
    if (!(firstCenter > secondCenter)) return false;

    auto firstSize = config.direction == Direction::LeftToRight
                         ? static_cast<double>(first.size.y)
                         : static_cast<double>(first.size.x);
    auto secondSize = config.direction == Direction::LeftToRight
                          ? static_cast<double>(second.size.y)
                          : static_cast<double>(second.size.x);
    auto required = firstSize * 0.5 + secondSize * 0.5
                    + static_cast<double>(config.nodeGap);
    if (!std::isfinite(required) || required <= 0.0) return false;

    int requiredExponent = 0;
    std::frexp(required, &requiredExponent);
    auto spacingExponent = requiredExponent - 1;
    auto spacing = std::ldexp(1.0, spacingExponent);
    if (spacing < required) {
        spacing *= 2.0;
        spacingExponent++;
    }
    auto binExponent = spacingExponent + 23;
    if (binExponent < -126 || binExponent > 127) return false;

    auto bin = std::ldexp(1.0, binExponent);
    auto anchor = bin * 1.5;
    auto boundary = direction > 0 ? anchor + spacing * 0.5
                                  : -anchor + spacing * 0.5;
    auto midpoint = (firstCenter + secondCenter) * 0.5;
    shift = boundary - midpoint;
    return std::isfinite(shift);
}

static bool _searchOutward(const Config& config, const NodeRecord* nodes,
                           uint32_t count, uint32_t rank,
                           const NodeLayout* layouts, double start, int direction,
                           uint32_t limit, double& shift,
                           NodeLayout* scratch) noexcept
{
    auto current = start;
    auto doubleLimit = direction > 0 ? std::numeric_limits<double>::max()
                                     : std::numeric_limits<double>::lowest();
    for (auto step = 0u; step < limit; step++) {
        auto firstFailure = Node::Invalid;
        auto secondFailure = Node::Invalid;
        if (_translateRank(config, nodes, count, rank, layouts, current, scratch,
                           &firstFailure, &secondFailure)) {
            shift = current;
            return true;
        }
        if (firstFailure == Node::Invalid) return false;

        auto next = doubleLimit;
        auto found = false;
        // For an automatic pair, only its direction-outward member can improve
        // the failed distance; for an auto-pin pair, the automatic member is the
        // only moving one. Until that participant changes rounded position the
        // failure cannot improve, so unrelated fine ULPs cannot exhaust the
        // bounded search before a relevant state transition.
        auto repair = secondFailure != Node::Invalid && direction < 0
                          ? secondFailure
                          : firstFailure;
        auto tie = 0.0;
        auto beyond = 0.0;
        if (_outwardBreakpoints(config, layouts, repair, current, direction, tie,
                                beyond)) {
            // Keep the tie and verified successor as independent events. Their
            // shift-space distance differs per center, so folding them together
            // can skip a narrow valid combination of rounded node positions.
            const double events[] = {tie, beyond};
            for (auto event : events) {
                auto ahead = direction > 0 ? event > current : event < current;
                if (ahead
                    && (!found || (direction > 0 ? event < next : event > next))) {
                    next = event;
                    found = true;
                }
            }
        }
        if (!found) return false;
        current = next;
    }
    return false;
}

static Result _reflowRanked(const Config& config, const NodeRecord* nodes,
                            uint32_t count, uint32_t maximumRank,
                            NodeLayout* layouts) noexcept
{
    ShiftInterval intervals[Diagram::NodeLimit];
    NodeLayout scratch[Diagram::NodeLimit];
    for (auto rank = 0u; rank <= maximumRank; rank++) {
        auto first = Node::Invalid;
        auto automaticCount = 0u;
        for (auto i = 0u; i < count; i++) {
            if (layouts[i].rank == rank && !nodes[i].manualPosition) {
                if (first == Node::Invalid) first = i;
                automaticCount++;
            }
        }
        if (first == Node::Invalid) continue;

        auto minimumX = static_cast<double>(layouts[first].center.x)
                        - layouts[first].size.x * 0.5;
        auto minimumY = static_cast<double>(layouts[first].center.y)
                        - layouts[first].size.y * 0.5;
        auto maximumX = static_cast<double>(layouts[first].center.x)
                        + layouts[first].size.x * 0.5;
        auto maximumY = static_cast<double>(layouts[first].center.y)
                        + layouts[first].size.y * 0.5;
        for (auto i = first + 1u; i < count; i++) {
            if (layouts[i].rank != rank || nodes[i].manualPosition) continue;
            minimumX = std::fmin(minimumX, static_cast<double>(layouts[i].center.x)
                                               - layouts[i].size.x * 0.5);
            minimumY = std::fmin(minimumY, static_cast<double>(layouts[i].center.y)
                                               - layouts[i].size.y * 0.5);
            maximumX = std::fmax(maximumX, static_cast<double>(layouts[i].center.x)
                                               + layouts[i].size.x * 0.5);
            maximumY = std::fmax(maximumY, static_cast<double>(layouts[i].center.y)
                                               + layouts[i].size.y * 0.5);
        }
        auto centerX = (minimumX + maximumX) * 0.5;
        auto centerY = (minimumY + maximumY) * 0.5;
        auto sizeX = maximumX - minimumX;
        auto sizeY = maximumY - minimumY;
        auto floatMaximum = static_cast<double>(std::numeric_limits<float>::max());
        if (!std::isfinite(centerX) || !std::isfinite(centerY)
            || !std::isfinite(sizeX) || !std::isfinite(sizeY)
            || std::fabs(centerX) > floatMaximum || std::fabs(centerY) > floatMaximum
            || sizeX > floatMaximum || sizeY > floatMaximum) {
            return Result::InvalidArguments;
        }
        auto blockMinimum = config.direction == Direction::LeftToRight ? minimumY
                                                                       : minimumX;
        auto blockMaximum = config.direction == Direction::LeftToRight ? maximumY
                                                                       : maximumX;

        auto intervalCount = 0u;
        for (auto i = 0u; i < count; i++) {
            if (!nodes[i].manualPosition) continue;
            auto primaryOverlap = false;
            for (auto j = 0u; j < count; j++) {
                if (layouts[j].rank == rank && !nodes[j].manualPosition
                    && _primaryOverlap(config.direction, layouts[j], layouts[i])) {
                    primaryOverlap = true;
                    break;
                }
            }
            if (!primaryOverlap) continue;
            auto low = _secondaryMinimum(config.direction, layouts[i])
                       - static_cast<double>(config.nodeGap)
                       - blockMaximum;
            auto high = _secondaryMaximum(config.direction, layouts[i])
                        + static_cast<double>(config.nodeGap)
                        - blockMinimum;
            if (!std::isfinite(low) || !std::isfinite(high) || low >= high) {
                return Result::InvalidArguments;
            }
            intervals[intervalCount++] = {low, high};
        }
        if (_translateRank(config, nodes, count, rank, layouts, 0.0, scratch)) {
            continue;
        }
        if (!intervalCount) return Result::InvalidArguments;

        // Merge duplicate/overlapping pin spans before expanding them into float
        // rounding states. This keeps the NodeLimit case interactive.
        for (auto i = 1u; i < intervalCount; i++) {
            auto value = intervals[i];
            auto j = i;
            while (j && intervals[j - 1u].minimum > value.minimum) {
                intervals[j] = intervals[j - 1u];
                j--;
            }
            intervals[j] = value;
        }
        auto mergedCount = 0u;
        for (auto i = 0u; i < intervalCount; i++) {
            if (mergedCount
                && intervals[i].minimum <= intervals[mergedCount - 1u].maximum) {
                intervals[mergedCount - 1u].maximum = std::fmax(
                    intervals[mergedCount - 1u].maximum, intervals[i].maximum);
            } else {
                intervals[mergedCount++] = intervals[i];
            }
        }
        intervalCount = mergedCount;

        auto separationCandidates = automaticCount > 1u
                                        ? static_cast<size_t>(automaticCount - 1u)
                                              * 6u
                                        : 0u;
        auto candidateCapacity = static_cast<size_t>(intervalCount)
                                     * (2u + 4u * automaticCount)
                                 + separationCandidates + 1u;
        auto candidates = new (std::nothrow) double[candidateCapacity];
        if (!candidates) return Result::OutOfMemory;
        auto candidateCount = 0u;
        auto blockCenter = (blockMinimum + blockMaximum) * 0.5;
        candidates[candidateCount++] = -blockCenter;
        auto previous = Node::Invalid;
        for (auto i = 0u; i < count; i++) {
            if (layouts[i].rank != rank || nodes[i].manualPosition) continue;
            if (previous != Node::Invalid) {
                for (auto direction : {-1, 1}) {
                    auto candidate = 0.0;
                    if (!_separationAnchorShift(config, layouts[previous],
                                                layouts[i], direction,
                                                candidate)) {
                        continue;
                    }
                    candidates[candidateCount++] = candidate;
                    candidates[candidateCount++] = std::nextafter(
                        candidate, std::numeric_limits<double>::lowest());
                    candidates[candidateCount++] = std::nextafter(
                        candidate, std::numeric_limits<double>::max());
                }
            }
            previous = i;
        }
        for (auto i = 0u; i < intervalCount; i++) {
            auto lower = intervals[i].minimum - Epsilon;
            auto upper = intervals[i].maximum + Epsilon;
            candidates[candidateCount++] = lower;
            candidates[candidateCount++] = upper;

            for (auto j = 0u; j < count; j++) {
                if (layouts[j].rank != rank || nodes[j].manualPosition) continue;
                auto tie = 0.0;
                auto beyond = 0.0;
                if (_outwardBreakpoints(config, layouts, j, lower, -1, tie,
                                        beyond)) {
                    candidates[candidateCount++] = tie;
                    candidates[candidateCount++] = beyond;
                }
                if (_outwardBreakpoints(config, layouts, j, upper, 1, tie,
                                        beyond)) {
                    candidates[candidateCount++] = tie;
                    candidates[candidateCount++] = beyond;
                }
            }
        }

        std::sort(candidates, candidates + candidateCount, _shiftLess);
        auto placed = false;
        for (auto i = 0u; i < candidateCount; i++) {
            if (i && candidates[i] == candidates[i - 1u]) continue;
            if (!_translateRank(config, nodes, count, rank, layouts, candidates[i],
                                scratch)) {
                continue;
            }
            std::memcpy(layouts, scratch, sizeof(NodeLayout) * count);
            placed = true;
            break;
        }
        delete[] candidates;
        if (!placed) {
            // Rare exponent-boundary cases can require several shared-shift state
            // changes. Search them chronologically with a fixed total budget.
            auto steps = std::max(8u, (Diagram::NodeLimit * 8u)
                                          / (intervalCount * 2u));
            auto found = false;
            auto best = 0.0;
            for (auto i = 0u; i < intervalCount; i++) {
                auto candidate = 0.0;
                if (_searchOutward(config, nodes, count, rank, layouts,
                                   intervals[i].minimum - Epsilon, -1, steps,
                                   candidate, scratch)
                    && (!found || _shiftLess(candidate, best))) {
                    found = true;
                    best = candidate;
                }
                if (_searchOutward(config, nodes, count, rank, layouts,
                                   intervals[i].maximum + Epsilon, 1, steps,
                                   candidate, scratch)
                    && (!found || _shiftLess(candidate, best))) {
                    found = true;
                    best = candidate;
                }
            }
            if (!found
                || !_translateRank(config, nodes, count, rank, layouts, best,
                                   scratch)) {
                return Result::InvalidArguments;
            }
            std::memcpy(layouts, scratch, sizeof(NodeLayout) * count);
        }
    }
    for (auto i = 0u; i < count; i++) {
        for (auto j = i + 1u; j < count; j++) {
            if (nodes[i].manualPosition && nodes[j].manualPosition) continue;
            if (_layoutOverlap(layouts[i], layouts[j])) {
                return Result::InvalidArguments;
            }
        }
    }
    return Result::Success;
}

static Port _resolveFrom(const Config& config, const NodeLayout& from,
                         const NodeLayout& to, Port requested) noexcept
{
    if (requested != Port::Auto) return requested;
    if (config.direction == Direction::LeftToRight) {
        return to.center.x >= from.center.x ? Port::Right : Port::Left;
    }
    return to.center.y <= from.center.y ? Port::Bottom : Port::Top;
}

static Port _resolveTo(const Config& config, const NodeLayout& from,
                       const NodeLayout& to, Port requested) noexcept
{
    if (requested != Port::Auto) return requested;
    if (config.direction == Direction::LeftToRight) {
        return to.center.x >= from.center.x ? Port::Left : Port::Right;
    }
    return to.center.y <= from.center.y ? Port::Top : Port::Bottom;
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

static Result _tag(Object* object, const char* prefix, const char* kind,
                   const char* id = nullptr, const char* suffix = nullptr) noexcept
{
    if (!object || !prefix || !kind) return Result::InvalidArguments;
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

static Result _attach(Object* parent, Object* child) noexcept
{
    return parent && child ? parent->add(child) : Result::OutOfMemory;
}

static Color _alpha(Color color, uint8_t alpha) noexcept
{
    color.a = static_cast<uint8_t>((static_cast<uint16_t>(color.a) * alpha) / 255u);
    return color;
}

static Color _nodeStroke(const Theme& theme, NodeKind kind) noexcept
{
    switch (kind) {
        case NodeKind::State: return theme.colors.accent;
        case NodeKind::Decision: return theme.colors.warning;
        case NodeKind::Terminal: return theme.colors.result;
        case NodeKind::Entity: return theme.colors.info;
        case NodeKind::Task: return theme.colors.accent;
        case NodeKind::Evidence: return theme.colors.danger;
        case NodeKind::Cell:
        case NodeKind::Default: return theme.colors.border;
    }
    return theme.colors.border;
}

static Color _nodeFill(const Theme& theme, NodeKind kind) noexcept
{
    switch (kind) {
        case NodeKind::Decision: return _alpha(theme.colors.warning, 44u);
        case NodeKind::Terminal: return _alpha(theme.colors.result, 48u);
        case NodeKind::Task: return _alpha(theme.colors.accent, 56u);
        case NodeKind::Evidence: return _alpha(theme.colors.danger, 44u);
        case NodeKind::State:
        case NodeKind::Entity:
        case NodeKind::Cell:
        case NodeKind::Default: return theme.colors.surface;
    }
    return theme.colors.surface;
}

static Color _edgeColor(const Theme& theme, const EdgeRecord& edge) noexcept
{
    switch (edge.kind) {
        case EdgeKind::Relation: return theme.colors.border;
        case EdgeKind::Return: return theme.colors.muted;
        case EdgeKind::Error: return theme.colors.danger;
        case EdgeKind::Optional: return theme.colors.secondary;
        case EdgeKind::Directed: return edge.flow ? theme.colors.accent : theme.colors.muted;
    }
    return theme.colors.muted;
}

static bool _dashed(const EdgeRecord& edge) noexcept
{
    return edge.flow || edge.kind == EdgeKind::Return || edge.kind == EdgeKind::Error
           || edge.kind == EdgeKind::Optional;
}

static uint32_t _simplify(Vec2* points, uint32_t count) noexcept
{
    if (!points || count < 2u) return 0u;
    auto output = 1u;
    for (auto i = 1u; i < count; i++) {
        if (_same(points[output - 1u], points[i])) continue;
        points[output++] = points[i];
        while (output >= 3u) {
            auto a = points[output - 3u];
            auto b = points[output - 2u];
            auto c = points[output - 1u];
            auto ab = _sub(b, a);
            auto bc = _sub(c, b);
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

static bool _axisAligned(const Vec2* points, uint32_t count) noexcept
{
    if (!points || count < 2u) return false;
    for (auto i = 1u; i < count; i++) {
        auto delta = _sub(points[i], points[i - 1u]);
        if (_length(delta) <= Epsilon) return false;
        if (!_near(delta.x, 0.0f) && !_near(delta.y, 0.0f)) return false;
        if (i < 2u) continue;
        auto previous = _sub(points[i - 1u], points[i - 2u]);
        auto dot = previous.x * delta.x + previous.y * delta.y;
        if (dot < -Epsilon && (_near(previous.x, 0.0f) == _near(delta.x, 0.0f))) {
            return false;
        }
    }
    return true;
}

static uint32_t _legacyCenterline(const Config& config, const EdgeRecord& edge,
                                  const EdgeLayout& layout, Route route,
                                  Vec2* output) noexcept
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

static Vec2 _outward(Port port) noexcept
{
    switch (port) {
        case Port::Left: return {-1.0f, 0.0f};
        case Port::Right: return {1.0f, 0.0f};
        case Port::Top: return {0.0f, 1.0f};
        case Port::Bottom: return {0.0f, -1.0f};
        case Port::Auto: break;
    }
    return {};
}

static bool _outwardSegment(const Vec2& endpoint, const Vec2& adjacent,
                            Port port) noexcept
{
    auto direction = _sub(adjacent, endpoint);
    auto outward = _outward(port);
    return direction.x * outward.x + direction.y * outward.y > Epsilon;
}

static bool _endpointDirections(const Vec2* points, uint32_t count,
                                const EdgeLayout& layout) noexcept
{
    return points && count >= 2u
           && _outwardSegment(points[0], points[1], layout.fromPort)
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
    return points && count >= 2u
           && _length(_sub(points[count - 1u], points[count - 2u]))
                  > config.arrowLength + Epsilon;
}

static float _routeMargin(const Config& config) noexcept
{
    return std::fmax(config.routeWidth, config.arrowWidth) * 0.5f
           + config.routeWidth;
}

static bool _routeClear(const Vec2* points, uint32_t count, const NodeLayout* nodes,
                        uint32_t nodeCount, const EdgeRecord& edge,
                        float margin) noexcept
{
    if (!_axisAligned(points, count)) return false;
    for (auto i = 0u; i < nodeCount; i++) {
        for (auto j = 1u; j < count; j++) {
            auto padding = margin;
            if ((i == edge.from.index && j == 1u)
                || (i == edge.to.index && j + 1u == count)) {
                padding = 0.0f;
            }
            if (_crosses(points[j - 1u], points[j], nodes[i].center,
                         nodes[i].size, padding)) {
                return false;
            }
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
        auto blocked = false;
        for (auto j = 1u; j < count; j++) {
            auto padding = margin;
            if ((i == edge.from.index && j == 1u)
                || (i == edge.to.index && j + 1u == count)) {
                padding = 0.0f;
            }
            if (!_crosses(points[j - 1u], points[j], nodes[i].center,
                          nodes[i].size, padding)) {
                continue;
            }
            blocked = true;
            break;
        }
        if (blocked) _include(bounds, nodes[i], margin);
    }
    return bounds;
}

static uint32_t _elbow(const EdgeLayout& layout, float stub, bool horizontal,
                       Vec2* output) noexcept
{
    auto from = _add(layout.from, _mul(_outward(layout.fromPort), stub));
    auto to = _add(layout.to, _mul(_outward(layout.toPort), stub));
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
    auto from = _add(layout.from, _mul(_outward(layout.fromPort), fromStub));
    auto to = _add(layout.to, _mul(_outward(layout.toPort), toStub));
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
    for (auto i = 1u; i < count; i++) {
        length += _length(_sub(points[i], points[i - 1u]));
    }
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
    if (config.direction == Direction::LeftToRight) {
        auto count = _corridor(layout, fromStub, toStub, bounds.maximumY + gap,
                               true, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
        count = _corridor(layout, fromStub, toStub, bounds.minimumY - gap,
                          true, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
        count = _corridor(layout, fromStub, toStub, bounds.maximumX + gap,
                          false, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
        count = _corridor(layout, fromStub, toStub, bounds.minimumX - gap,
                          false, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
    } else {
        auto count = _corridor(layout, fromStub, toStub, bounds.maximumX + gap,
                               false, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
        count = _corridor(layout, fromStub, toStub, bounds.minimumX - gap,
                          false, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
        count = _corridor(layout, fromStub, toStub, bounds.maximumY + gap,
                          true, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
        count = _corridor(layout, fromStub, toStub, bounds.minimumY - gap,
                          true, points);
        _consider(config, points, count, layout, nodes, nodeCount, edge, margin,
                  choice);
    }
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

static uint32_t _centerline(const Config& config, const EdgeRecord& edge,
                            const EdgeLayout& layout, Route& route,
                            const NodeLayout* nodes, uint32_t nodeCount,
                            Vec2* output) noexcept
{
    auto count = _legacyCenterline(config, edge, layout, route, output);
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

static Path* _orthogonalPath(const Config& config, const Vec2* route,
                             uint32_t count) noexcept
{
    if (!_axisAligned(route, count)) return nullptr;
    auto last = _sub(route[count - 1u], route[count - 2u]);
    auto lastLength = _length(last);
    if (lastLength <= config.arrowLength + Epsilon) return nullptr;
    auto direction = _mul(last, 1.0f / lastLength);
    auto normal = Vec2{-direction.y, direction.x};
    auto base = _sub(route[count - 1u], _mul(direction, config.arrowLength));

    Vec2 shaft[Diagram::WaypointLimit + 4u];
    auto shaftCount = count;
    for (auto i = 0u; i + 1u < count; i++) shaft[i] = route[i];
    shaft[shaftCount - 1u] = base;
    if (!_axisAligned(shaft, shaftCount)) return nullptr;

    Vec2 left[Diagram::WaypointLimit + 4u];
    Vec2 right[Diagram::WaypointLimit + 4u];
    auto half = config.routeWidth * 0.5f;
    for (auto i = 0u; i < shaftCount; i++) {
        Vec2 offset;
        if (i == 0u) {
            auto delta = _sub(shaft[1u], shaft[0u]);
            auto length = _length(delta);
            offset = _mul(Vec2{-delta.y / length, delta.x / length}, half);
        } else if (i + 1u == shaftCount) {
            auto delta = _sub(shaft[i], shaft[i - 1u]);
            auto length = _length(delta);
            offset = _mul(Vec2{-delta.y / length, delta.x / length}, half);
        } else {
            auto previous = _sub(shaft[i], shaft[i - 1u]);
            auto next = _sub(shaft[i + 1u], shaft[i]);
            auto previousLength = _length(previous);
            auto nextLength = _length(next);
            auto previousNormal = Vec2{-previous.y / previousLength,
                                       previous.x / previousLength};
            auto nextNormal = Vec2{-next.y / nextLength, next.x / nextLength};
            if (_near(previousNormal.x, nextNormal.x)
                && _near(previousNormal.y, nextNormal.y)) {
                offset = _mul(previousNormal, half);
            } else {
                offset = _mul(_add(previousNormal, nextNormal), half);
            }
        }
        left[i] = _add(shaft[i], offset);
        right[i] = _sub(shaft[i], offset);
    }

    PathCommand commands[(Diagram::WaypointLimit + 4u) * 2u + 5u];
    auto commandCount = 0u;
    commands[commandCount++] = PathCommand::move({left[0].x, left[0].y, 0.0f});
    for (auto i = 1u; i < shaftCount; i++) {
        commands[commandCount++] = PathCommand::line({left[i].x, left[i].y, 0.0f});
    }
    auto headHalf = config.arrowWidth * 0.5f;
    auto headLeft = _add(base, _mul(normal, headHalf));
    auto headRight = _sub(base, _mul(normal, headHalf));
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

static Vec2 _labelPoint(const Config& config, const Vec2* points,
                        uint32_t count) noexcept
{
    auto total = 0.0f;
    for (auto i = 1u; i < count; i++) total += _length(_sub(points[i], points[i - 1u]));
    auto target = total * 0.5f;
    auto cursor = 0.0f;
    for (auto i = 1u; i < count; i++) {
        auto segment = _sub(points[i], points[i - 1u]);
        auto length = _length(segment);
        if (cursor + length >= target && length > Epsilon) {
            auto progress = (target - cursor) / length;
            auto point = _add(points[i - 1u], _mul(segment, progress));
            auto normal = Vec2{-segment.y / length, segment.x / length};
            if (normal.y < -Epsilon
                || (_near(normal.y, 0.0f) && normal.x < 0.0f)) {
                normal = _mul(normal, -1.0f);
            }
            auto gap = std::fmax(config.routeWidth * 2.0f,
                                 config.corner + 0.12f);
            return _add(point, _mul(normal, gap));
        }
        cursor += length;
    }
    return points[count - 1u];
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
        prefix = _copy(sourceConfig.id);
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
            target.id = _copy(source.id);
            target.label = _copy(source.label);
            target.detail = source.detail ? _copy(source.detail) : nullptr;
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
            if (!_finite(nodePlacements[i].center) || !_finite(nodePlacements[i].size)
                || nodePlacements[i].size.x <= 0.0f || nodePlacements[i].size.y <= 0.0f
                || nodePlacements[i].rank >= Diagram::NodeLimit) {
                return Result::InvalidArguments;
            }
            bytes += std::strlen(target.id) + std::strlen(target.label) + 2u;
            if (target.detail) bytes += std::strlen(target.detail) + 1u;
        }

        for (auto i = 0u; i < edgeCount; i++) {
            auto& source = sourceEdges[i];
            auto& target = edges[i];
            target.id = _copy(source.id);
            target.label = source.label ? _copy(source.label) : nullptr;
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
            auto centerlineCount = _centerline(sourceConfig, source, edgeLayouts[i], route,
                                               layouts, nodeCount, centerline);
            if (centerlineCount < 2u
                || (route == Route::Orthogonal
                    && !_axisAligned(centerline, centerlineCount))) {
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
                placement.labelPoint = _labelPoint(sourceConfig, centerline, centerlineCount);
                if (!_finite(placement.labelPoint)) return Result::InvalidArguments;
            }
            bytes += std::strlen(target.id) + 1u + sizeof(Vec2) * source.waypointCount
                     + sizeof(Vec2) * centerlineCount;
            if (target.label) bytes += std::strlen(target.label) + 1u;
        }

        for (auto i = 0u; i < zoneCount; i++) {
            auto& source = sourceZones[i];
            auto& target = zones[i];
            target.id = _copy(source.id);
            target.label = _copy(source.label);
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
            if (!_finite(zonePlacements[i].center) || !_finite(zonePlacements[i].size)
                || zonePlacements[i].size.x <= 0.0f || zonePlacements[i].size.y <= 0.0f) {
                return Result::InvalidArguments;
            }
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
    impl->prefix = _copy(config.id);
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
    if (!pImpl || pImpl->nodeCount >= NodeLimit || !_validId(spec.id)
        || !_validText(spec.label) || !_validText(spec.detail) || !_valid(spec.kind)
        || spec.rank < -1 || spec.row < -1 || spec.column < -1
        || spec.rank >= static_cast<int32_t>(NodeLimit) || !_finite(spec.position)
        || spec.row >= static_cast<int32_t>(NodeLimit)
        || spec.column >= static_cast<int32_t>(NodeLimit) || !_finite(spec.start)
        || !_finite(spec.span) || !_finite(spec.size)
        || ((spec.size.x != 0.0f || spec.size.y != 0.0f)
            && (spec.size.x <= 0.0f || spec.size.y <= 0.0f))) {
        return pImpl && pImpl->nodeCount >= NodeLimit ? Result::InsufficientCondition
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
    auto id = _copy(spec.id);
    auto label = _copy(spec.label ? spec.label : spec.id);
    auto detail = spec.detail && spec.detail[0] ? _copy(spec.detail) : nullptr;
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
    if (!pImpl || pImpl->edgeCount >= EdgeLimit || !_validId(spec.id)
        || !_validText(spec.label) || !spec.from || !spec.to
        || spec.from.index >= pImpl->nodeCount || spec.to.index >= pImpl->nodeCount
        || spec.from.index == spec.to.index || !_valid(spec.fromPort) || !_valid(spec.toPort)
        || !_valid(spec.route) || !_valid(spec.kind)
        || (spec.waypointCount && !spec.waypoints)
        || (!spec.waypointCount && spec.waypoints) || spec.waypointCount > WaypointLimit
        || (spec.route == Route::Straight && spec.waypointCount)
        || !_finite(spec.flowOffset)) {
        return pImpl && pImpl->edgeCount >= EdgeLimit ? Result::InsufficientCondition
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
    auto id = _copy(spec.id);
    auto label = spec.label && spec.label[0] ? _copy(spec.label) : nullptr;
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
    if (!pImpl || pImpl->zoneCount >= ZoneLimit || !_validId(spec.id)
        || !_validText(spec.label) || !spec.members || !spec.memberCount
        || spec.memberCount > pImpl->nodeCount) {
        return pImpl && pImpl->zoneCount >= ZoneLimit ? Result::InsufficientCondition
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
    auto id = _copy(spec.id);
    auto label = _copy(spec.label ? spec.label : spec.id);
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
    return pImpl ? pImpl->nodeCount : 0u;
}

uint32_t Diagram::edgeCount() const noexcept
{
    return pImpl ? pImpl->edgeCount : 0u;
}

uint32_t Diagram::zoneCount() const noexcept
{
    return pImpl ? pImpl->zoneCount : 0u;
}

Result Diagram::build(const Theme& theme, Built*& output) const noexcept
{
    if (!pImpl || output || !pImpl->nodeCount) return Result::InvalidArguments;

    NodeLayout layouts[NodeLimit]{};
    EdgeLayout edgeLayouts[EdgeLimit]{};
    ZoneLayout zoneLayouts[ZoneLimit]{};
    uint32_t indegree[NodeLimit]{};
    bool processed[NodeLimit]{};
    float rankSpan[NodeLimit]{};
    float rankPosition[NodeLimit]{};
    float rowSpan[NodeLimit]{};
    float rowPosition[NodeLimit]{};
    uint32_t portCount[NodeLimit][4]{};
    uint32_t portCursor[NodeLimit][4]{};

    for (auto i = 0u; i < pImpl->nodeCount; i++) {
        layouts[i].size = (pImpl->nodes[i].size.x > 0.0f) ? pImpl->nodes[i].size
                                                         : pImpl->config.nodeSize;
        layouts[i].rank = pImpl->nodes[i].rank >= 0
                              ? static_cast<uint32_t>(pImpl->nodes[i].rank)
                              : 0u;
    }
    if (pImpl->config.layout == Layout::Ranked) {
        for (auto i = 0u; i < pImpl->edgeCount; i++) indegree[pImpl->edges[i].to.index]++;

        auto visited = 0u;
        while (visited < pImpl->nodeCount) {
            auto current = Node::Invalid;
            for (auto i = 0u; i < pImpl->nodeCount; i++) {
                if (!processed[i] && !indegree[i]) {
                    current = i;
                    break;
                }
            }
            if (current == Node::Invalid) return Result::InvalidArguments;
            processed[current] = true;
            visited++;
            for (auto i = 0u; i < pImpl->edgeCount; i++) {
                auto& edge = pImpl->edges[i];
                if (edge.from.index != current) continue;
                auto target = edge.to.index;
                auto candidate = layouts[current].rank + 1u;
                if (candidate >= NodeLimit) return Result::InvalidArguments;
                auto authored = pImpl->nodes[target].rank;
                if (authored >= 0) {
                    if (static_cast<uint32_t>(authored) < candidate) {
                        return Result::InvalidArguments;
                    }
                    layouts[target].rank = static_cast<uint32_t>(authored);
                } else if (layouts[target].rank < candidate) {
                    layouts[target].rank = candidate;
                }
                indegree[target]--;
            }
        }

        auto maximumRank = 0u;
        for (auto i = 0u; i < pImpl->nodeCount; i++) {
            maximumRank = layouts[i].rank > maximumRank ? layouts[i].rank : maximumRank;
            auto primary = pImpl->config.direction == Direction::LeftToRight
                               ? layouts[i].size.x
                               : layouts[i].size.y;
            if (primary > rankSpan[layouts[i].rank]) rankSpan[layouts[i].rank] = primary;
        }
        for (auto rank = 0u; rank <= maximumRank; rank++) {
            if (rankSpan[rank] <= 0.0f) {
                rankSpan[rank] = pImpl->config.direction == Direction::LeftToRight
                                     ? pImpl->config.nodeSize.x
                                     : pImpl->config.nodeSize.y;
            }
            if (!rank) {
                rankPosition[rank] = pImpl->config.direction == Direction::LeftToRight
                                         ? pImpl->config.origin.x
                                         : pImpl->config.origin.y;
            } else {
                auto distance = rankSpan[rank - 1u] * 0.5f + pImpl->config.rankGap
                                + rankSpan[rank] * 0.5f;
                rankPosition[rank] = pImpl->config.direction == Direction::LeftToRight
                                         ? rankPosition[rank - 1u] + distance
                                         : rankPosition[rank - 1u] - distance;
            }
        }

        for (auto rank = 0u; rank <= maximumRank; rank++) {
            auto total = 0.0f;
            for (auto i = 0u; i < pImpl->nodeCount; i++) {
                if (layouts[i].rank != rank || pImpl->nodes[i].manualPosition) continue;
                total += pImpl->config.direction == Direction::LeftToRight
                             ? layouts[i].size.y
                             : layouts[i].size.x;
            }
            auto automaticCount = 0u;
            for (auto i = 0u; i < pImpl->nodeCount; i++) {
                if (layouts[i].rank == rank && !pImpl->nodes[i].manualPosition) {
                    automaticCount++;
                }
            }
            if (automaticCount > 1u) {
                total += pImpl->config.nodeGap * (automaticCount - 1u);
            }
            auto cursor = (pImpl->config.direction == Direction::LeftToRight
                               ? pImpl->config.origin.y
                               : pImpl->config.origin.x)
                          + total * 0.5f;
            for (auto i = 0u; i < pImpl->nodeCount; i++) {
                if (layouts[i].rank != rank) continue;
                if (pImpl->nodes[i].manualPosition) {
                    layouts[i].center = pImpl->nodes[i].position;
                    continue;
                }
                auto secondary = pImpl->config.direction == Direction::LeftToRight
                                     ? layouts[i].size.y
                                     : layouts[i].size.x;
                auto position = cursor - secondary * 0.5f;
                if (pImpl->config.direction == Direction::LeftToRight) {
                    layouts[i].center = {rankPosition[rank], position};
                } else {
                    layouts[i].center = {position, rankPosition[rank]};
                }
                cursor -= secondary + pImpl->config.nodeGap;
            }
        }
        auto reflow = _reflowRanked(pImpl->config, pImpl->nodes, pImpl->nodeCount,
                                    maximumRank, layouts);
        if (reflow != Result::Success) return reflow;
    } else if (pImpl->config.layout == Layout::Manual) {
        for (auto i = 0u; i < pImpl->nodeCount; i++) {
            layouts[i].center = pImpl->nodes[i].position;
        }
    } else if (pImpl->config.layout == Layout::Grid) {
        auto maximumColumn = 0u;
        auto maximumRow = 0u;
        for (auto i = 0u; i < pImpl->nodeCount; i++) {
            auto column = static_cast<uint32_t>(pImpl->nodes[i].column);
            auto row = static_cast<uint32_t>(pImpl->nodes[i].row);
            maximumColumn = column > maximumColumn ? column : maximumColumn;
            maximumRow = row > maximumRow ? row : maximumRow;
            if (layouts[i].size.x > rankSpan[column]) rankSpan[column] = layouts[i].size.x;
            if (layouts[i].size.y > rowSpan[row]) rowSpan[row] = layouts[i].size.y;
        }
        for (auto column = 0u; column <= maximumColumn; column++) {
            if (rankSpan[column] <= 0.0f) rankSpan[column] = pImpl->config.nodeSize.x;
        }
        for (auto row = 0u; row <= maximumRow; row++) {
            if (rowSpan[row] <= 0.0f) rowSpan[row] = pImpl->config.nodeSize.y;
        }
        rankPosition[0] = pImpl->config.origin.x;
        rowPosition[0] = pImpl->config.origin.y;
        for (auto column = 1u; column <= maximumColumn; column++) {
            rankPosition[column] = rankPosition[column - 1u]
                                   + rankSpan[column - 1u] * 0.5f
                                   + pImpl->config.rankGap + rankSpan[column] * 0.5f;
        }
        for (auto row = 1u; row <= maximumRow; row++) {
            rowPosition[row] = rowPosition[row - 1u] - rowSpan[row - 1u] * 0.5f
                               - pImpl->config.nodeGap - rowSpan[row] * 0.5f;
        }
        for (auto i = 0u; i < pImpl->nodeCount; i++) {
            layouts[i].center = {
                rankPosition[static_cast<uint32_t>(pImpl->nodes[i].column)],
                rowPosition[static_cast<uint32_t>(pImpl->nodes[i].row)],
            };
        }
    } else {
        auto maximumRow = 0u;
        for (auto i = 0u; i < pImpl->nodeCount; i++) {
            auto row = static_cast<uint32_t>(pImpl->nodes[i].row);
            maximumRow = row > maximumRow ? row : maximumRow;
            layouts[i].size.x = pImpl->nodes[i].span * pImpl->config.timeUnit;
            if (layouts[i].size.y > rowSpan[row]) rowSpan[row] = layouts[i].size.y;
        }
        for (auto row = 0u; row <= maximumRow; row++) {
            if (rowSpan[row] <= 0.0f) rowSpan[row] = pImpl->config.nodeSize.y;
        }
        rowPosition[0] = pImpl->config.origin.y;
        for (auto row = 1u; row <= maximumRow; row++) {
            rowPosition[row] = rowPosition[row - 1u] - rowSpan[row - 1u] * 0.5f
                               - pImpl->config.nodeGap - rowSpan[row] * 0.5f;
        }
        for (auto i = 0u; i < pImpl->nodeCount; i++) {
            auto& record = pImpl->nodes[i];
            layouts[i].center = {
                pImpl->config.origin.x + (record.start + record.span * 0.5f)
                                                   * pImpl->config.timeUnit,
                rowPosition[static_cast<uint32_t>(record.row)],
            };
        }
    }

    for (auto i = 0u; i < pImpl->nodeCount; i++) {
        if (!_finite(layouts[i].center) || !_finite(layouts[i].size)
            || layouts[i].size.x <= 0.0f || layouts[i].size.y <= 0.0f
            || layouts[i].rank >= NodeLimit) {
            return Result::InvalidArguments;
        }
        auto half = _mul(layouts[i].size, 0.5f);
        auto minimum = _sub(layouts[i].center, half);
        auto maximum = _add(layouts[i].center, half);
        auto labelOffset = layouts[i].size.y * 0.27f;
        if (!_finite(minimum) || !_finite(maximum)
            || !_finite(layouts[i].center.y + labelOffset)
            || !_finite(layouts[i].center.y - labelOffset)) {
            return Result::InvalidArguments;
        }
    }

    for (auto i = 0u; i < pImpl->edgeCount; i++) {
        auto& edge = pImpl->edges[i];
        auto& layout = edgeLayouts[i];
        layout.fromPort = _resolveFrom(pImpl->config, layouts[edge.from.index],
                                       layouts[edge.to.index], edge.fromPort);
        layout.toPort = _resolveTo(pImpl->config, layouts[edge.from.index],
                                   layouts[edge.to.index], edge.toPort);
        portCount[edge.from.index][_portIndex(layout.fromPort)]++;
        portCount[edge.to.index][_portIndex(layout.toPort)]++;
    }
    for (auto i = 0u; i < pImpl->edgeCount; i++) {
        auto& edge = pImpl->edges[i];
        auto& layout = edgeLayouts[i];
        auto fromSide = _portIndex(layout.fromPort);
        auto toSide = _portIndex(layout.toPort);
        layout.from = _port(layouts[edge.from.index], layout.fromPort,
                            portCursor[edge.from.index][fromSide]++,
                            portCount[edge.from.index][fromSide]);
        layout.to = _port(layouts[edge.to.index], layout.toPort,
                          portCursor[edge.to.index][toSide]++,
                          portCount[edge.to.index][toSide]);
        if (!_finite(layout.from) || !_finite(layout.to)) return Result::InvalidArguments;
    }

    for (auto i = 0u; i < pImpl->zoneCount; i++) {
        auto& record = pImpl->zones[i];
        auto& first = layouts[record.members[0].index];
        auto minimum = _sub(first.center, _mul(first.size, 0.5f));
        auto maximum = _add(first.center, _mul(first.size, 0.5f));
        for (auto j = 1u; j < record.memberCount; j++) {
            auto& member = layouts[record.members[j].index];
            minimum.x = std::fmin(minimum.x, member.center.x - member.size.x * 0.5f);
            minimum.y = std::fmin(minimum.y, member.center.y - member.size.y * 0.5f);
            maximum.x = std::fmax(maximum.x, member.center.x + member.size.x * 0.5f);
            maximum.y = std::fmax(maximum.y, member.center.y + member.size.y * 0.5f);
        }
        if (record.fullWidth) {
            for (auto j = 0u; j < pImpl->nodeCount; j++) {
                minimum.x = std::fmin(minimum.x,
                                      layouts[j].center.x - layouts[j].size.x * 0.5f);
                maximum.x = std::fmax(maximum.x,
                                      layouts[j].center.x + layouts[j].size.x * 0.5f);
            }
        }
        minimum.x -= pImpl->config.zonePadding;
        minimum.y -= pImpl->config.zonePadding;
        maximum.x += pImpl->config.zonePadding;
        maximum.y += pImpl->config.zonePadding * 1.45f;
        zoneLayouts[i].center = _mul(_add(minimum, maximum), 0.5f);
        zoneLayouts[i].size = _sub(maximum, minimum);
        auto labelPoint = Vec2{minimum.x + pImpl->config.zonePadding * 0.35f,
                               maximum.y - pImpl->config.zonePadding * 0.55f};
        if (!_finite(zoneLayouts[i].center) || !_finite(zoneLayouts[i].size)
            || !_finite(labelPoint) || zoneLayouts[i].size.x <= 0.0f
            || zoneLayouts[i].size.y <= 0.0f) {
            return Result::InvalidArguments;
        }
    }

    auto receiptImpl = new (std::nothrow) IR::Impl;
    if (!receiptImpl) return Result::OutOfMemory;
    auto receipt = new (std::nothrow) IR(receiptImpl);
    if (!receipt) {
        delete receiptImpl;
        return Result::OutOfMemory;
    }
    auto result = receiptImpl->copy(pImpl->config, pImpl->nodes, pImpl->nodeCount,
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
                                   pImpl->config.corner * 1.5f);
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
        body->fill(_alpha(theme.colors.surface, 150u));
        body->stroke(theme.colors.border, theme.objectWidth);
        body->layer = pImpl->config.layers.zones;
        label->fill(theme.colors.muted);
        label->role = record.fullWidth ? TextRole::Code : TextRole::H3;
        label->align = {0.0f, 0.5f};
        label->layer = pImpl->config.layers.annotations;
        if ((result = _attach(zone, body)) != Result::Success
            || (result = _attach(builtImpl->zones, zone)) != Result::Success
            || (result = _attach(builtImpl->annotations, label)) != Result::Success) {
            if (label && !label->parent()) delete label;
            if (body && !body->parent()) delete body;
            if (zone && !zone->parent()) delete zone;
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
            auto path = _orthogonalPath(pImpl->config, centerline, centerlineCount);
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
        if ((result = _attach(builtImpl->routes, edgeObject)) != Result::Success) {
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
            if ((result = _attach(builtImpl->annotations, label)) != Result::Success) {
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
        auto corner = record.kind == NodeKind::Terminal
                          ? std::fmin(layout.size.x, layout.size.y) * 0.5f
                          : pImpl->config.corner;
        auto body = Rectangle::gen({layout.center.x, layout.center.y, 0.0f}, layout.size,
                                   corner);
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
        body->fill(_nodeFill(theme, record.kind));
        body->stroke(_nodeStroke(theme, record.kind), theme.objectWidth);
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
        if ((result = _attach(node, body)) != Result::Success
            || (result = _attach(node, label)) != Result::Success
            || (detail && (result = _attach(node, detail)) != Result::Success)
            || (result = _attach(builtImpl->nodes, node)) != Result::Success) {
            if (body && !body->parent()) delete body;
            if (label && !label->parent()) delete label;
            if (detail && !detail->parent()) delete detail;
            if (node && !node->parent()) delete node;
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
