// The diagram module's node and zone layout: ranks and rows placed apart, with overlaps
// resolved by shifting along the secondary axis.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

#include "visDiagram.h"
#include "visDiagramGeometry.h"
#include "../common/visFinite.h"
#include "../common/visSort.h"

namespace vis::diagram::detail
{

struct ShiftInterval
{
    double minimum = 0.0;
    double maximum = 0.0;
};

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
    // Float centers cannot preserve gaps below their local ULP. Bounds overlap
    // remains invalid regardless of this spacing tolerance.
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
        // Only the outward member of an automatic pair can repair this gap.
        // A pinned pair has one moving member. Skip unrelated rounding events
        // so fine ULPs cannot consume the search budget.
        auto repair = secondFailure != Node::Invalid && direction < 0
                          ? secondFailure
                          : firstFailure;
        auto tie = 0.0;
        auto beyond = 0.0;
        if (_outwardBreakpoints(config, layouts, repair, current, direction, tie,
                                beyond)) {
            // Keep the tie and its successor separate: merging their per-center
            // offsets can skip a valid combination of rounded positions.
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
        auto candidates = new (std::nothrow) double[2u * candidateCapacity];
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

        stableSort(candidates, candidates + candidateCount, candidateCount,
                   sizeof(double), [](const void* a, const void* b, const void*) {
                       return _shiftLess(*static_cast<const double*>(a),
                                         *static_cast<const double*>(b));
                   });
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

Result layoutNodes(const Config& config, const NodeRecord* nodes, uint32_t nodeCount,
                   const EdgeRecord* edges, uint32_t edgeCount, NodeLayout* layouts) noexcept
{
    uint32_t indegree[Diagram::NodeLimit]{};
    bool processed[Diagram::NodeLimit]{};
    float rankSpan[Diagram::NodeLimit]{};
    float rankPosition[Diagram::NodeLimit]{};
    float rowSpan[Diagram::NodeLimit]{};
    float rowPosition[Diagram::NodeLimit]{};

    auto placeRows = [&](uint32_t maximumRow) {
        for (auto row = 0u; row <= maximumRow; row++) {
            if (rowSpan[row] <= 0.0f) rowSpan[row] = config.nodeSize.y;
        }
        rowPosition[0] = config.origin.y;
        for (auto row = 1u; row <= maximumRow; row++) {
            rowPosition[row] = rowPosition[row - 1u] - rowSpan[row - 1u] * 0.5f
                               - config.nodeGap - rowSpan[row] * 0.5f;
        }
    };

    for (auto i = 0u; i < nodeCount; i++) {
        layouts[i].size = nodes[i].size.x > 0.0f ? nodes[i].size : config.nodeSize;
        layouts[i].rank = nodes[i].rank >= 0
                              ? static_cast<uint32_t>(nodes[i].rank)
                              : 0u;
    }
    if (config.layout == Layout::Ranked) {
        for (auto i = 0u; i < edgeCount; i++) indegree[edges[i].to.index]++;

        auto visited = 0u;
        while (visited < nodeCount) {
            auto current = Node::Invalid;
            for (auto i = 0u; i < nodeCount; i++) {
                if (!processed[i] && !indegree[i]) {
                    current = i;
                    break;
                }
            }
            if (current == Node::Invalid) return Result::InvalidArguments;
            processed[current] = true;
            visited++;
            for (auto i = 0u; i < edgeCount; i++) {
                auto& edge = edges[i];
                if (edge.from.index != current) continue;
                auto target = edge.to.index;
                auto candidate = layouts[current].rank + 1u;
                if (candidate >= Diagram::NodeLimit) return Result::InvalidArguments;
                auto authored = nodes[target].rank;
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
        for (auto i = 0u; i < nodeCount; i++) {
            maximumRank = layouts[i].rank > maximumRank ? layouts[i].rank : maximumRank;
            auto primary = config.direction == Direction::LeftToRight
                               ? layouts[i].size.x
                               : layouts[i].size.y;
            if (primary > rankSpan[layouts[i].rank]) rankSpan[layouts[i].rank] = primary;
        }
        for (auto rank = 0u; rank <= maximumRank; rank++) {
            if (rankSpan[rank] <= 0.0f) {
                rankSpan[rank] = config.direction == Direction::LeftToRight
                                     ? config.nodeSize.x
                                     : config.nodeSize.y;
            }
            if (!rank) {
                rankPosition[rank] = config.direction == Direction::LeftToRight
                                         ? config.origin.x
                                         : config.origin.y;
            } else {
                auto distance = rankSpan[rank - 1u] * 0.5f + config.rankGap
                                + rankSpan[rank] * 0.5f;
                rankPosition[rank] = config.direction == Direction::LeftToRight
                                         ? rankPosition[rank - 1u] + distance
                                         : rankPosition[rank - 1u] - distance;
            }
        }

        for (auto rank = 0u; rank <= maximumRank; rank++) {
            auto total = 0.0f;
            auto automaticCount = 0u;
            for (auto i = 0u; i < nodeCount; i++) {
                if (layouts[i].rank != rank || nodes[i].manualPosition) continue;
                total += config.direction == Direction::LeftToRight
                             ? layouts[i].size.y
                             : layouts[i].size.x;
                automaticCount++;
            }
            if (automaticCount > 1u) {
                total += config.nodeGap * (automaticCount - 1u);
            }
            auto cursor = (config.direction == Direction::LeftToRight
                               ? config.origin.y
                               : config.origin.x)
                          + total * 0.5f;
            for (auto i = 0u; i < nodeCount; i++) {
                if (layouts[i].rank != rank) continue;
                if (nodes[i].manualPosition) {
                    layouts[i].center = nodes[i].position;
                    continue;
                }
                auto secondary = config.direction == Direction::LeftToRight
                                     ? layouts[i].size.y
                                     : layouts[i].size.x;
                auto position = cursor - secondary * 0.5f;
                if (config.direction == Direction::LeftToRight) {
                    layouts[i].center = {rankPosition[rank], position};
                } else {
                    layouts[i].center = {position, rankPosition[rank]};
                }
                cursor -= secondary + config.nodeGap;
            }
        }
        auto reflow = _reflowRanked(config, nodes, nodeCount, maximumRank, layouts);
        if (reflow != Result::Success) return reflow;
    } else if (config.layout == Layout::Manual) {
        for (auto i = 0u; i < nodeCount; i++) {
            layouts[i].center = nodes[i].position;
        }
    } else if (config.layout == Layout::Grid) {
        auto maximumColumn = 0u;
        auto maximumRow = 0u;
        for (auto i = 0u; i < nodeCount; i++) {
            auto column = static_cast<uint32_t>(nodes[i].column);
            auto row = static_cast<uint32_t>(nodes[i].row);
            maximumColumn = column > maximumColumn ? column : maximumColumn;
            maximumRow = row > maximumRow ? row : maximumRow;
            if (layouts[i].size.x > rankSpan[column]) rankSpan[column] = layouts[i].size.x;
            if (layouts[i].size.y > rowSpan[row]) rowSpan[row] = layouts[i].size.y;
        }
        for (auto column = 0u; column <= maximumColumn; column++) {
            if (rankSpan[column] <= 0.0f) rankSpan[column] = config.nodeSize.x;
        }
        rankPosition[0] = config.origin.x;
        for (auto column = 1u; column <= maximumColumn; column++) {
            rankPosition[column] = rankPosition[column - 1u]
                                   + rankSpan[column - 1u] * 0.5f
                                   + config.rankGap + rankSpan[column] * 0.5f;
        }
        placeRows(maximumRow);
        for (auto i = 0u; i < nodeCount; i++) {
            layouts[i].center = {
                rankPosition[static_cast<uint32_t>(nodes[i].column)],
                rowPosition[static_cast<uint32_t>(nodes[i].row)],
            };
        }
    } else {
        auto maximumRow = 0u;
        for (auto i = 0u; i < nodeCount; i++) {
            auto row = static_cast<uint32_t>(nodes[i].row);
            maximumRow = row > maximumRow ? row : maximumRow;
            layouts[i].size.x = nodes[i].span * config.timeUnit;
            if (layouts[i].size.y > rowSpan[row]) rowSpan[row] = layouts[i].size.y;
        }
        placeRows(maximumRow);
        for (auto i = 0u; i < nodeCount; i++) {
            auto& record = nodes[i];
            layouts[i].center = {
                config.origin.x + (record.start + record.span * 0.5f) * config.timeUnit,
                rowPosition[static_cast<uint32_t>(record.row)],
            };
        }
    }

    for (auto i = 0u; i < nodeCount; i++) {
        if (!_finite(layouts[i].center) || !_finite(layouts[i].size)
            || layouts[i].size.x <= 0.0f || layouts[i].size.y <= 0.0f
            || layouts[i].rank >= Diagram::NodeLimit) {
            return Result::InvalidArguments;
        }
        auto half = layouts[i].size * 0.5f;
        auto minimum = layouts[i].center - half;
        auto maximum = layouts[i].center + half;
        auto labelOffset = layouts[i].size.y * 0.27f;
        if (!_finite(minimum) || !_finite(maximum)
            || !_finite(layouts[i].center.y + labelOffset)
            || !_finite(layouts[i].center.y - labelOffset)) {
            return Result::InvalidArguments;
        }
    }

    return Result::Success;
}

Result layoutZones(const Config& config, const ZoneRecord* zones, uint32_t zoneCount,
                   const NodeLayout* layouts, uint32_t nodeCount, ZoneLayout* zoneLayouts) noexcept
{
    for (auto i = 0u; i < zoneCount; i++) {
        auto& record = zones[i];
        auto& first = layouts[record.members[0].index];
        auto minimum = first.center - first.size * 0.5f;
        auto maximum = first.center + first.size * 0.5f;
        for (auto j = 1u; j < record.memberCount; j++) {
            auto& member = layouts[record.members[j].index];
            minimum.x = std::fmin(minimum.x, member.center.x - member.size.x * 0.5f);
            minimum.y = std::fmin(minimum.y, member.center.y - member.size.y * 0.5f);
            maximum.x = std::fmax(maximum.x, member.center.x + member.size.x * 0.5f);
            maximum.y = std::fmax(maximum.y, member.center.y + member.size.y * 0.5f);
        }
        if (record.fullWidth) {
            for (auto j = 0u; j < nodeCount; j++) {
                minimum.x = std::fmin(minimum.x,
                                      layouts[j].center.x - layouts[j].size.x * 0.5f);
                maximum.x = std::fmax(maximum.x,
                                      layouts[j].center.x + layouts[j].size.x * 0.5f);
            }
        }
        minimum.x -= config.zonePadding;
        minimum.y -= config.zonePadding;
        maximum.x += config.zonePadding;
        maximum.y += config.zonePadding * 1.45f;
        zoneLayouts[i].center = (minimum + maximum) * 0.5f;
        zoneLayouts[i].size = maximum - minimum;
        auto labelPoint = Vec2{minimum.x + config.zonePadding * 0.35f,
                               maximum.y - config.zonePadding * 0.55f};
        if (!_finite(zoneLayouts[i].center) || !_finite(zoneLayouts[i].size)
            || !_finite(labelPoint) || zoneLayouts[i].size.x <= 0.0f
            || zoneLayouts[i].size.y <= 0.0f) {
            return Result::InvalidArguments;
        }
    }

    return Result::Success;
}

}  // namespace vis::diagram::detail
