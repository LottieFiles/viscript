// DisplayList::split(): the painter's order of a 3D scene sorts every drawing as one piece, so a
// stroke partly behind a surface would draw wholly in front of it or wholly behind it. split()
// tests each stroke against the faces under it every few pixels, as a depth buffer would at that
// resolution, and gives it the depth that draws it after the faces it lies in front of and before
// the faces that hide it, cutting it only where no one depth does.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

#include "vis.h"
#include "visDisplayList.h"
#include "../common/visGrow.h"
#include "../common/visSort.h"

namespace vis
{

namespace
{

// How far apart a stroke is tested against the faces under it, in pixels, and the most tests
// along one stroke, past which they spread further apart.
constexpr float SPACING = 2.0f;
constexpr uint32_t SAMPLES = 2048u;
// The most pieces one stroke is cut into, and the most commands a list gains.
constexpr uint32_t PIECES = 256u;
constexpr uint32_t ADDED = 8192u;
// Halvings that find where a cut falls between two tests.
constexpr uint32_t STEPS = 6u;
// Treats near-coplanar strokes as lying on the face within pixel/relative tolerance,
// preventing faceted surfaces from hiding their own curves.
constexpr float TOLERANCE = 1.0f;
constexpr float SHARE = 1.0f / 32.0f;
// How far an opaque stroke with butt caps runs on past a cut, in pixels, so the two pieces
// overlap instead of leaving a seam where their antialiased ends meet.
constexpr float OVERLAP = 0.5f;
// A piece shorter than this, in pixels, joins the longer piece beside it.
constexpr float SHORTEST = 0.5f;
// The most items the grid that finds faces may hold before its cells grow.
constexpr uint64_t GRID_ITEMS = 1ull << 22;
constexpr float INF = std::numeric_limits<float>::infinity();

// A face of a surface that hides what lies behind it.
struct DepthFace
{
    Vec2 points[4];
    float depths[4];
    Vec2 min;
    Vec2 max;
    float key;
    // The longer diagonal, in pixels.
    float size;
};

// The faces of the surfaces in one layer of one composite, and the grid that finds those near
// a point.
struct DepthGroup
{
    uint32_t composite = 0;
    int32_t layer = 0;
    uint32_t count = 0;
    uint32_t* faces = nullptr;
    Vec2 min;
    Vec2 max;
    // The side of a cell of the grid, which starts at min, in pixels.
    float cell = 1.0f;
    uint32_t columns = 0;
    uint32_t rows = 0;
    uint32_t* starts = nullptr;
    uint32_t* items = nullptr;
    bool indexed = false;
    bool failed = false;

    // The columns x0 to x1 and the rows y0 to y1 of the cells that the box from low to high
    // covers, kept inside the grid.
    void span(const Vec2& low, const Vec2& high, uint32_t& x0, uint32_t& x1, uint32_t& y0, uint32_t& y1) const
    {
        auto at = [this](float value, float origin, uint32_t cells) {
            auto index = std::fmax((value - origin) / cell, 0.0f);
            return std::min(static_cast<uint32_t>(std::fmin(index, 1e9f)), cells - 1u);
        };
        x0 = at(low.x, min.x, columns);
        x1 = at(high.x, min.x, columns);
        y0 = at(low.y, min.y, rows);
        y1 = at(high.y, min.y, rows);
    }
};

// The keys a drawing may take at a point: above lo, the largest key among the faces in front
// of it there, and below hi, the smallest among the faces it lies in front of or on. hidden: a
// face lies in front of it there.
struct DepthRange
{
    float lo = -INF;
    float hi = INF;
    bool hidden = false;
};

// A face near a drawing: its key, whether it lies in front, and how far it reaches into the
// drawing, in pixels.
struct DepthVote
{
    float key;
    float weight;
    bool front;
};

// A stretch of a stroke that draws at one key: from start to end along it, in pixels.
struct DepthRun
{
    float start;
    float end;
    DepthRange range;
    // A lifted stroke's stretch that a face below it hides, which drops to that layer.
    bool lowered;
};

struct DepthCamera
{
    bool perspective = false;
    // The world units a pixel spans at a distance of 1 from a perspective camera, or anywhere
    // before an orthographic one.
    double pixel = 0.0;

    // The distance of a point from the camera, from DrawCommand::depths.
    double distance(float measure) const
    {
        return perspective ? 1.0 / static_cast<double>(measure) : static_cast<double>(measure);
    }
};

// The polyline of a stroke: a closed one runs on to its first point again.
struct DepthLine
{
    Vec2* points = nullptr;
    float* depths = nullptr;
    // How far along the line each point lies, in pixels.
    float* lengths = nullptr;
    uint32_t count = 0;
    float total = 0.0f;

    DepthLine() = default;
    DepthLine(const DepthLine&) = delete;
    DepthLine& operator=(const DepthLine&) = delete;
    ~DepthLine()
    {
        delete[] points;
        delete[] depths;
        delete[] lengths;
    }

    bool make(const DrawCommand& command)
    {
        count = command.closed ? command.count + 1u : command.count;
        points = new (std::nothrow) Vec2[count];
        depths = new (std::nothrow) float[count];
        lengths = new (std::nothrow) float[count];
        if (!points || !depths || !lengths) return false;
        for (auto i = 0u; i < count; i++) {
            points[i] = command.points[i % command.count];
            depths[i] = command.depths[i % command.count];
        }
        auto travelled = 0.0;
        lengths[0] = 0.0f;
        for (auto i = 1u; i < count; i++) {
            travelled += static_cast<double>((points[i] - points[i - 1u]).length());
            lengths[i] = static_cast<float>(travelled);
        }
        total = lengths[count - 1u];
        return std::isfinite(total) && total > SHORTEST;
    }

    // The segment that holds position, and the share of the way along it.
    uint32_t segment(float position, float& share) const
    {
        auto low = 0u;
        auto high = count - 1u;
        while (high - low > 1u) {
            auto middle = (low + high) / 2u;
            if (lengths[middle] <= position) low = middle;
            else high = middle;
        }
        auto length = lengths[low + 1u] - lengths[low];
        share = length > 0.0f ? std::fmin(std::fmax((position - lengths[low]) / length, 0.0f), 1.0f) : 0.0f;
        return low;
    }

    void at(float position, Vec2& point, float& depth) const
    {
        float share;
        auto i = segment(position, share);
        point = points[i] + (points[i + 1u] - points[i]) * share;
        depth = depths[i] + (depths[i + 1u] - depths[i]) * share;
    }

    // Appends the points from position from to position to, both inside the line, and returns
    // how many: from, the points between and to.
    uint32_t copy(float from, float to, Vec2* outPoints, float* outDepths) const
    {
        auto n = 0u;
        at(from, outPoints[n], outDepths[n]);
        n++;
        for (auto i = 0u; i < count; i++) {
            if (lengths[i] <= from || lengths[i] >= to) continue;
            outPoints[n] = points[i];
            outDepths[n] = depths[i];
            n++;
        }
        at(to, outPoints[n], outDepths[n]);
        return n + 1u;
    }
};

// What split() does to one command: nothing, a new key and layer, or a cut into runs.
struct DepthPlan
{
    uint32_t first = 0;
    uint32_t count = 0;
    float key = 0.0f;
    int32_t layer = 0;
    bool moved = false;
    bool lifted = false;
};

struct DepthState
{
    DepthCamera camera;
    DepthFace* faces = nullptr;
    uint32_t faceCount = 0;
    uint32_t* order = nullptr;
    uint32_t* stamps = nullptr;
    uint32_t stamp = 0;
    DepthGroup* groups = nullptr;
    uint32_t groupCount = 0;
    // The faces near the point _range() tests, one for each face at most, and room to sort them.
    DepthVote* votes = nullptr;
    // The plan of each command, the runs of the strokes they cut, and room for the pieces of one.
    DepthPlan* plans = nullptr;
    DepthRun* runs = nullptr;
    uint32_t runCount = 0;
    uint32_t runCapacity = 0;
    DepthRun* pieces = nullptr;

    DepthState() = default;
    DepthState(const DepthState&) = delete;
    DepthState& operator=(const DepthState&) = delete;
    ~DepthState()
    {
        for (auto i = 0u; i < groupCount; i++) {
            delete[] groups[i].starts;
            delete[] groups[i].items;
        }
        delete[] faces;
        delete[] order;
        delete[] stamps;
        delete[] groups;
        delete[] votes;
        delete[] plans;
        delete[] runs;
        delete[] pieces;
    }

    DepthGroup* find(uint32_t composite, int32_t layer)
    {
        for (auto i = 0u; i < groupCount; i++) {
            if (groups[i].composite == composite && groups[i].layer == layer) return groups + i;
        }
        return nullptr;
    }
};

// Whether the command is a face of a surface that hides what lies behind it.
bool _face(const DrawCommand& command)
{
    return command.object && command.object->type() == Type::SurfaceMesh && command.type == CommandType::Path
           && command.closed && command.count == 4u && command.depths && command.style.fill.a;
}

float _cross(const Vec2& a, const Vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

// How far p lies from the segment from a to b, and the share of the way along it of the point of
// the segment nearest p.
float _segmentDistance(const Vec2& p, const Vec2& a, const Vec2& b, float& share)
{
    auto edge = b - a;
    auto length = edge.dot(edge);
    share = length > 0.0f ? std::fmin(std::fmax((p - a).dot(edge) / length, 0.0f), 1.0f) : 0.0f;
    return (p - (a + edge * share)).length();
}

// The depth of a face at p and how far p lies outside it (gap): where the face covers p, from the
// triangle of its two that p lies deepest inside, and elsewhere at the point of its outline
// nearest p, as a face seen at a slant would carry its depth far off within a few pixels past its
// edges. False when p lies farther than radius from the face.
bool _faceAt(const DepthFace& face, const Vec2& p, float radius, float& measure, float& gap)
{
    if (p.x < face.min.x - radius || p.x > face.max.x + radius || p.y < face.min.y - radius
        || p.y > face.max.y + radius) {
        return false;
    }
    gap = 0.0f;
    static constexpr uint8_t triangles[2][3] = {{0u, 1u, 2u}, {0u, 2u, 3u}};
    auto inside = -INF;
    for (auto& triangle : triangles) {
        auto& a = face.points[triangle[0]];
        auto& b = face.points[triangle[1]];
        auto& c = face.points[triangle[2]];
        auto area = _cross(b - a, c - a);
        if (std::fabs(area) < 1e-6f) continue;
        auto first = _cross(b - p, c - p) / area;
        auto second = _cross(c - p, a - p) / area;
        auto third = 1.0f - first - second;
        auto least = std::fmin(first, std::fmin(second, third));
        if (least <= inside) continue;
        inside = least;
        measure = first * face.depths[triangle[0]] + second * face.depths[triangle[1]]
                + third * face.depths[triangle[2]];
    }
    if (inside == -INF) return false;
    if (inside >= -1e-4f) return true;
    gap = INF;
    for (auto i = 0u; i < 4u; i++) {
        auto next = (i + 1u) % 4u;
        float share;
        auto distance = _segmentDistance(p, face.points[i], face.points[next], share);
        if (distance >= gap) continue;
        gap = distance;
        measure = face.depths[i] + (face.depths[next] - face.depths[i]) * share;
    }
    return gap <= radius;
}

// Lays a grid over the faces of group, so the faces near a point take few tests to find.
bool _index(DepthState& state, DepthGroup& group)
{
    if (group.indexed) return true;
    if (group.failed) return false;
    group.failed = true;
    auto width = std::fmax(group.max.x - group.min.x, 1.0f);
    auto height = std::fmax(group.max.y - group.min.y, 1.0f);
    group.cell = std::fmax(4.0f, 2.0f * std::sqrt(width * height / static_cast<float>(group.count)));
    group.cell = std::fmax(group.cell, std::fmax(width, height) / 256.0f);
    if (!std::isfinite(group.cell)) return false;
    uint64_t items = 0;
    for (auto attempt = 0u; attempt < 16u; attempt++) {
        group.columns = static_cast<uint32_t>(width / group.cell) + 1u;
        group.rows = static_cast<uint32_t>(height / group.cell) + 1u;
        items = 0;
        for (auto i = 0u; i < group.count; i++) {
            auto& face = state.faces[group.faces[i]];
            uint32_t x0, x1, y0, y1;
            group.span(face.min, face.max, x0, x1, y0, y1);
            items += static_cast<uint64_t>(x1 - x0 + 1u) * (y1 - y0 + 1u);
        }
        if (items <= GRID_ITEMS) break;
        group.cell *= 2.0f;
    }
    if (items > GRID_ITEMS) return false;
    auto cells = group.columns * group.rows;
    group.starts = new (std::nothrow) uint32_t[cells + 1u];
    group.items = new (std::nothrow) uint32_t[items ? items : 1u];
    if (!group.starts || !group.items) return false;
    for (auto i = 0u; i <= cells; i++) group.starts[i] = 0u;
    auto visit = [&](auto&& each) {
        for (auto i = 0u; i < group.count; i++) {
            auto& face = state.faces[group.faces[i]];
            uint32_t x0, x1, y0, y1;
            group.span(face.min, face.max, x0, x1, y0, y1);
            for (auto y = y0; y <= y1; y++) {
                for (auto x = x0; x <= x1; x++) each(y * group.columns + x, group.faces[i]);
            }
        }
    };
    visit([&](uint32_t cellIndex, uint32_t) { group.starts[cellIndex + 1u]++; });
    for (auto i = 0u; i < cells; i++) group.starts[i + 1u] += group.starts[i];
    auto filled = new (std::nothrow) uint32_t[cells];
    if (!filled) return false;
    for (auto i = 0u; i < cells; i++) filled[i] = group.starts[i];
    visit([&](uint32_t cellIndex, uint32_t face) { group.items[filled[cellIndex]++] = face; });
    delete[] filled;
    group.indexed = true;
    group.failed = false;
    return true;
}

// Whether a key lies strictly between lo and hi.
bool _between(float lo, float hi)
{
    return std::nextafter(lo, INF) < hi;
}

// Finds sorting keys satisfying nearby faces. Treats point/stroke thickness as a
// camera-facing sphere; if no key satisfies every face, minimizes weighted depth error.
DepthRange _range(DepthState& state, const DepthGroup& group, const Vec2& p, float measure, float radius,
                  float body)
{
    DepthRange range;
    auto votes = 0u;
    if (p.x + radius < group.min.x || p.x - radius > group.max.x || p.y + radius < group.min.y
        || p.y - radius > group.max.y) {
        return range;
    }
    auto distance = state.camera.distance(measure);
    if (!std::isfinite(distance) || distance <= 0.0) return range;
    uint32_t x0, x1, y0, y1;
    group.span({p.x - radius, p.y - radius}, {p.x + radius, p.y + radius}, x0, x1, y0, y1);
    auto stamp = ++state.stamp;
    for (auto y = y0; y <= y1; y++) {
        for (auto x = x0; x <= x1; x++) {
            auto cellIndex = y * group.columns + x;
            for (auto item = group.starts[cellIndex]; item < group.starts[cellIndex + 1u]; item++) {
                auto index = group.items[item];
                if (state.stamps[index] == stamp) continue;
                state.stamps[index] = stamp;
                auto& face = state.faces[index];
                float faceMeasure;
                float gap;
                if (!_faceAt(face, p, radius, faceMeasure, gap)) continue;
                auto faceDistance = state.camera.distance(faceMeasure);
                if (!std::isfinite(faceDistance) || faceDistance <= 0.0) continue;
                auto scale = state.camera.pixel * (state.camera.perspective ? faceDistance : 1.0);
                auto bulge = gap < body ? std::sqrt(body * body - gap * gap) : 0.0f;
                auto reach = (TOLERANCE + SHARE * face.size + bulge) * scale;
                auto front = faceDistance < distance - reach;
                if (front) range.lo = std::fmax(range.lo, face.key);
                else range.hi = std::fmin(range.hi, face.key);
                state.votes[votes++] = {face.key, radius - gap + 0.05f, front};
            }
        }
    }
    range.hidden = range.lo > -INF;
    if (_between(range.lo, range.hi)) return range;
    // Below every key, every face in front fails to hide the drawing; each key passed mends a
    // face in front or wrongs a face behind.
    auto vote = state.votes;
    stableSort(vote, vote + state.faceCount, votes, sizeof(DepthVote), [](const void* a, const void* b, const void*) {
        return static_cast<const DepthVote*>(a)->key < static_cast<const DepthVote*>(b)->key;
    });
    auto wrong = 0.0f;
    for (auto i = 0u; i < votes; i++) {
        if (vote[i].front) wrong += vote[i].weight;
    }
    auto least = wrong;
    DepthRange best = {-INF, vote[0].key, true};
    for (auto i = 0u; i < votes; i++) {
        wrong += vote[i].front ? -vote[i].weight : vote[i].weight;
        if (i + 1u < votes && vote[i + 1u].key == vote[i].key) continue;
        if (wrong >= least) continue;
        least = wrong;
        best = {vote[i].key, i + 1u < votes ? vote[i + 1u].key : INF, true};
    }
    return best;
}

// The keys both ranges allow. Where none is, as when a stroke out of pieces takes in the rest of
// its length, the faces that hide it win.
DepthRange _meet(const DepthRange& first, const DepthRange& second)
{
    DepthRange range = {std::fmax(first.lo, second.lo), std::fmin(first.hi, second.hi),
                        first.hidden || second.hidden};
    if (!_between(range.lo, range.hi)) range.hi = INF;
    return range;
}

// Whether a run can take on a point with range: a lifted run that a face hides takes only hidden
// points, one that none hides only points no face hides, and the keys must still agree.
bool _joins(const DepthRun& run, const DepthRange& range, bool lifted)
{
    if (lifted) {
        if (run.lowered != range.hidden) return false;
        if (!run.lowered) return true;
    }
    return _between(std::fmax(run.range.lo, range.lo), std::fmin(run.range.hi, range.hi));
}

// The key nearest to key that lies inside range.
float _pick(float key, const DepthRange& range)
{
    if (range.lo < key && key < range.hi) return key;
    if (key >= range.hi) {
        auto below = std::nextafter(range.hi, -INF);
        return below > range.lo ? below : range.lo + (range.hi - range.lo) * 0.5f;
    }
    auto above = std::nextafter(range.lo, INF);
    return above < range.hi ? above : range.lo + (range.hi - range.lo) * 0.5f;
}

// Where a cut at position along a stroke dashed by style falls. Anywhere in a gap leaves every
// dash whole, so a cut in a gap moves to its middle, and a cut in a dash moves to the middle of
// the gap beside it when that gap begins within reach; otherwise the cut stays and splits the
// dash, whose parts meet as the pieces of a stroke without a dash do, rather than move the edge
// where the stroke passes behind a face by up to half the dash.
float _snap(const Style& style, float position, float reach)
{
    auto count = static_cast<uint32_t>(style.dashCount);
    if (!count) return position;
    // ThorVG repeats an odd pattern twice, so its dashes and gaps take turns.
    auto entries = count % 2u ? count * 2u : count;
    auto period = 0.0f;
    for (auto i = 0u; i < entries; i++) period += style.dash[i % count];
    if (!(period > 1e-6f) || !std::isfinite(period)) return position;
    auto phase = std::fmod(style.dashOffset + position, period);
    if (phase < 0.0f) phase += period;
    auto nearest = INF;
    auto shift = 0.0f;
    auto at = 0.0f;
    for (auto i = 0u; i < entries; i++) {
        auto length = style.dash[i % count];
        if (i % 2u && length > 1e-6f) {
            for (auto wrap = -1; wrap <= 1; wrap++) {
                auto begin = at + static_cast<float>(wrap) * period;
                auto end = begin + length;
                auto distance = phase < begin ? begin - phase : (phase > end ? phase - end : 0.0f);
                if (distance >= nearest) continue;
                nearest = distance;
                shift = begin + length * 0.5f - phase;
            }
        }
        at += length;
    }
    return nearest <= reach ? position + shift : position;
}

// Cuts a stroke into runs that each draw at one key. lifted: the stroke draws above the faces'
// layer, and only its hidden runs drop to it. Returns how many runs runs holds, at most limit.
uint32_t _runs(DepthState& state, const DepthGroup& group, const DrawCommand& command,
               const DepthLine& line, bool lifted, DepthRun* runs, uint32_t limit)
{
    // At least one test, the line being longer than SHORTEST, and at most SAMPLES, bounded
    // before the cast, which a line too long for uint32_t to count its tests would overflow.
    auto steps = static_cast<uint32_t>(std::fmin(std::ceil(line.total / SPACING), static_cast<float>(SAMPLES)));
    auto spacing = line.total / static_cast<float>(steps);
    auto radius = command.style.width * 0.5f + spacing * 0.5f + 0.75f;
    auto sample = [&](float position) {
        Vec2 point;
        float measure;
        line.at(position, point, measure);
        return _range(state, group, point, measure, radius, command.style.width * 0.5f);
    };
    auto first = sample(0.0f);
    DepthRun run = {0.0f, 0.0f, first, first.hidden};
    auto count = 0u;
    auto previous = 0.0f;
    for (auto step = 1u; step <= steps; step++) {
        auto position = step == steps ? line.total : spacing * static_cast<float>(step);
        auto range = sample(position);
        if (_joins(run, range, lifted)) {
            run.range = _meet(run.range, range);
            previous = position;
            continue;
        }
        if (count + 1u >= limit) {
            // Out of pieces: the last one takes the rest.
            run.range = _meet(run.range, range);
            run.lowered = run.lowered || range.hidden;
            previous = position;
            continue;
        }
        // The cut lies between the last point the run took and this one.
        auto low = previous;
        auto high = position;
        auto next = range;
        for (auto halving = 0u; halving < STEPS; halving++) {
            auto middle = (low + high) * 0.5f;
            auto between = sample(middle);
            if (_joins(run, between, lifted)) {
                run.range = _meet(run.range, between);
                low = middle;
            } else {
                next = _meet(next, between);
                high = middle;
            }
        }
        auto cut = (low + high) * 0.5f;
        run.end = cut;
        runs[count++] = run;
        run = {cut, 0.0f, next, next.hidden};
        previous = position;
    }
    run.end = line.total;
    runs[count++] = run;
    return count;
}

// Joins runs too short to draw to the longer run beside them, which keeps its key.
uint32_t _tidy(DepthRun* runs, uint32_t count)
{
    auto kept = 0u;
    for (auto i = 0u; i < count; i++) {
        if (kept && runs[i].end - runs[i].start < SHORTEST) {
            runs[kept - 1u].end = runs[i].end;
            continue;
        }
        if (kept && runs[kept - 1u].end - runs[kept - 1u].start < SHORTEST) {
            runs[i].start = runs[kept - 1u].start;
            runs[kept - 1u] = runs[i];
            continue;
        }
        runs[kept++] = runs[i];
    }
    return kept;
}

// The box a command draws in: a path's points, half its stroke and a pixel around them, or the
// reach of a point or a number around its center.
void _box(const DrawCommand& command, Vec2& min, Vec2& max)
{
    auto reach = command.style.width * 0.5f + 1.0f;
    if (command.type == CommandType::Path) {
        extent(command.points, command.count, min, max);
    } else {
        min = max = command.center;
        reach = command.type == CommandType::Circle ? command.radius + command.style.width * 0.5f : command.size;
    }
    min = {min.x - reach, min.y - reach};
    max = {max.x + reach, max.y + reach};
}

}  // namespace

Result DisplayList::split(const Config& config) noexcept
{
    if (config.cameraView != CameraView::ThreeD || count < 2u) return Result::Success;
    DepthState state;
    for (auto i = 0u; i < count; i++) {
        if (_face(commands[i])) state.faceCount++;
    }
    if (!state.faceCount) return Result::Success;

    auto& camera = config.camera;
    state.camera.perspective = camera.projection == Projection::Perspective;
    state.camera.pixel = state.camera.perspective
                       ? 2.0 * std::tan(static_cast<double>(camera.fov) * 0.5) / config.height
                       : static_cast<double>(camera.orthoHeight) / config.height;
    if (!(camera.far > camera.near) || !std::isfinite(state.camera.pixel)) return Result::Success;

    // The faces, gathered by the layer and the composite they draw in.
    state.faces = new (std::nothrow) DepthFace[state.faceCount];
    state.order = new (std::nothrow) uint32_t[state.faceCount];
    state.stamps = new (std::nothrow) uint32_t[state.faceCount];
    state.groups = new (std::nothrow) DepthGroup[state.faceCount];
    state.votes = new (std::nothrow) DepthVote[2u * state.faceCount];
    if (!state.faces || !state.order || !state.stamps || !state.groups || !state.votes) return Result::OutOfMemory;
    auto faceIndex = 0u;
    for (auto i = 0u; i < count; i++) {
        auto& command = commands[i];
        if (!_face(command)) continue;
        auto& face = state.faces[faceIndex];
        for (auto j = 0u; j < 4u; j++) {
            face.points[j] = command.points[j];
            face.depths[j] = command.depths[j];
        }
        extent(face.points, 4u, face.min, face.max);
        face.key = command.depth;
        face.size = std::fmax((face.points[2] - face.points[0]).length(), (face.points[3] - face.points[1]).length());
        state.stamps[faceIndex] = 0u;
        auto group = state.find(command.composite, command.layer);
        if (!group) {
            group = state.groups + state.groupCount++;
            group->composite = command.composite;
            group->layer = command.layer;
            group->min = face.min;
            group->max = face.max;
        }
        group->min = {std::fmin(group->min.x, face.min.x), std::fmin(group->min.y, face.min.y)};
        group->max = {std::fmax(group->max.x, face.max.x), std::fmax(group->max.y, face.max.y)};
        group->count++;
        faceIndex++;
    }
    // Each group lists its faces in order.
    auto placed = 0u;
    for (auto g = 0u; g < state.groupCount; g++) {
        state.groups[g].faces = state.order + placed;
        placed += state.groups[g].count;
        state.groups[g].count = 0u;
    }
    faceIndex = 0u;
    for (auto i = 0u; i < count; i++) {
        auto& command = commands[i];
        if (!_face(command)) continue;
        auto group = state.find(command.composite, command.layer);
        group->faces[group->count++] = faceIndex++;
    }

    // What becomes of each drawing a surface of its layer overlaps.
    state.plans = new (std::nothrow) DepthPlan[count];
    state.pieces = new (std::nothrow) DepthRun[PIECES];
    if (!state.plans || !state.pieces) return Result::OutOfMemory;
    auto added = 0u;
    auto changed = false;
    for (auto i = 0u; i < count; i++) {
        auto& command = commands[i];
        if (!command.depths || _face(command)
            || (command.object && command.object->type() == Type::SurfaceMesh)) {
            continue;
        }
        if (command.type != CommandType::Path && command.type != CommandType::Circle
            && command.type != CommandType::Number) {
            continue;
        }
        auto group = state.find(command.composite, command.layer);
        auto lifted = false;
        if (!group && command.lift) {
            group = state.find(command.composite,
                               static_cast<int32_t>(static_cast<int64_t>(command.layer) - command.lift));
            lifted = group != nullptr;
        }
        if (!group) continue;
        Vec2 min;
        Vec2 max;
        _box(command, min, max);
        if (max.x < group->min.x || min.x > group->max.x || max.y < group->min.y || min.y > group->max.y) {
            continue;
        }
        if (!_index(state, *group)) continue;
        auto lowered = lifted
                     ? static_cast<int32_t>(static_cast<int64_t>(command.layer) - command.lift)
                     : command.layer;
        auto stroke = command.type == CommandType::Path && !command.style.fill.a;
        if (!stroke) {
            // A point, a number or the head of a route draws whole, at the key its anchor takes.
            Vec2 anchor = command.center;
            auto measure = command.depths[0];
            auto radius = 0.0f;
            if (command.type == CommandType::Path) {
                anchor = {};
                measure = 0.0f;
                for (auto j = 0u; j < command.count; j++) {
                    anchor = anchor + command.points[j] * (1.0f / static_cast<float>(command.count));
                    measure += command.depths[j] / static_cast<float>(command.count);
                }
                for (auto j = 0u; j < command.count; j++) {
                    radius = std::fmax(radius, (command.points[j] - anchor).length());
                }
            } else if (command.type == CommandType::Circle) {
                radius = command.radius + (command.style.stroke.a ? command.style.width * 0.5f : 0.0f);
            } else {
                radius = command.size * 0.5f;
            }
            // A point is round as a ball; a number and the head of a route lie flat.
            auto body = command.type == CommandType::Circle ? radius : 0.0f;
            auto range = _range(state, *group, anchor, measure, radius + 0.75f, body);
            if (lifted && !range.hidden) continue;
            auto key = _pick(command.depth, range);
            if (key == command.depth && lowered == command.layer) continue;
            state.plans[i] = {0u, 0u, key, lowered, true, lifted};
            changed = true;
            continue;
        }
        if (command.style.width <= 0.0f || !command.style.stroke.a || command.ends) continue;
        DepthLine line;
        if (!line.make(command)) {
            if (!line.points || !line.depths || !line.lengths) return Result::OutOfMemory;
            continue;
        }
        auto runs = state.pieces;
        auto made = _runs(state, *group, command, line, lifted, runs, PIECES);
        // A closed stroke joins its last run to its first across the point it starts at, unless
        // a dash, which starts again there, would no longer fit.
        if (command.closed && made > 1u && !command.style.dashCount
            && _joins(runs[made - 1u], runs[0].range, lifted)) {
            runs[made - 1u].range = _meet(runs[made - 1u].range, runs[0].range);
            runs[made - 1u].end = line.total + runs[0].end;
            for (auto j = 1u; j < made; j++) runs[j - 1u] = runs[j];
            made--;
        }
        if (command.style.dashCount) {
            for (auto j = 1u; j < made; j++) {
                auto snapped = _snap(command.style, runs[j].start, command.style.width * 0.5f + 1.0f);
                runs[j].start = snapped;
                runs[j - 1u].end = snapped;
            }
        }
        made = _tidy(runs, made);
        if (made == 1u) {
            auto key = lifted && !runs[0].lowered ? command.depth : _pick(command.depth, runs[0].range);
            auto layer = lifted && !runs[0].lowered ? command.layer : lowered;
            if (key == command.depth && layer == command.layer) continue;
            state.plans[i] = {0u, 0u, key, layer, true, lifted};
            changed = true;
            continue;
        }
        if (added + made - 1u > ADDED || count + added + made - 1u > DISPLAY_COMMAND_LIMIT) continue;
        if (!grow(state.runs, state.runCount, state.runCapacity, made, 64u)) return Result::OutOfMemory;
        std::memcpy(state.runs + state.runCount, runs, sizeof(DepthRun) * made);
        state.plans[i] = {state.runCount, made, command.depth, lowered, true, lifted};
        state.runCount += made;
        added += made - 1u;
        changed = true;
    }
    if (!changed) return Result::Success;

    // The list again, each cut stroke in its pieces where it stood, numbered in order. Output
    // owns what it holds until it hands it over, so a failure frees what was written; commands
    // not yet moved stay with the old array.
    DisplayList output;
    output.commands = new (std::nothrow) DrawCommand[count + added];
    if (!output.commands) return Result::OutOfMemory;
    output.capacity = count + added;
    for (auto i = 0u; i < count; i++) {
        auto& command = commands[i];
        auto& plan = state.plans[i];
        if (!plan.moved || !plan.count) {
            auto& kept = output.commands[output.count];
            kept = command;
            if (plan.moved) {
                kept.depth = plan.key;
                kept.layer = plan.layer;
                kept.piece = 1u;
                kept.whole = command.depth;
            }
            kept.order = output.count++;
            command.points = nullptr;
            command.ends = nullptr;
            command.buffer = nullptr;
            command.depths = nullptr;
            continue;
        }
        DepthLine line;
        if (!line.make(command)) return Result::OutOfMemory;
        // The pieces keep the gradient of the whole stroke, fixed where the renderer would
        // derive it from each piece.
        placeGradient(command, command.style.gradient);
        auto opaque = command.style.stroke.a == 255u;
        auto dashed = command.style.dashCount > 0u;
        for (auto r = 0u; r < plan.count; r++) {
            auto& run = state.runs[plan.first + r];
            auto start = run.start;
            auto end = run.end;
            auto cutStart = command.closed || r > 0u;
            auto cutEnd = command.closed || r + 1u < plan.count;
            auto cap = command.cap;
            if (!dashed && opaque && cap == PathCap::Butt) {
                if (cutStart) start -= OVERLAP;
                if (cutEnd) end += OVERLAP;
            } else if (!dashed && !opaque) {
                // Pieces of a stroke that is not opaque meet with butt caps, so their ends do not
                // overlap and darken where they meet. A round end of the stroke stays round: the
                // first and last pieces keep the cap and stop half a width short of their cut, so
                // the cap ends at the cut.
                auto half = command.style.width * 0.5f;
                auto round = cap == PathCap::Round && !command.closed && end - start > half * 2.0f;
                cap = PathCap::Butt;
                if (round && r == 0u) {
                    end -= half;
                    cap = PathCap::Round;
                } else if (round && r + 1u == plan.count) {
                    start += half;
                    cap = PathCap::Round;
                }
            }
            if (!command.closed) {
                start = std::fmax(start, 0.0f);
                end = std::fmin(end, line.total);
            }
            // A closed stroke's pieces may run past its start into its first lap again.
            auto wraps = end > line.total;
            if (start < 0.0f) {
                start += line.total;
                end += line.total;
                wraps = true;
            }
            auto room = wraps ? line.count * 2u + 4u : line.count + 2u;
            auto piece = output.commands + output.count;
            *piece = command;
            piece->order = output.count++;
            piece->points = new (std::nothrow) Vec2[room];
            piece->depths = new (std::nothrow) float[room];
            piece->ends = nullptr;
            piece->contours = 0u;
            piece->buffer = nullptr;
            if (!piece->points || !piece->depths) return Result::OutOfMemory;
            uint32_t n;
            if (!wraps) {
                n = line.copy(start, end, piece->points, piece->depths);
            } else {
                n = line.copy(start, line.total, piece->points, piece->depths);
                auto rest = line.copy(0.0f, std::fmin(end - line.total, line.total), piece->points + n - 1u,
                                      piece->depths + n - 1u);
                n += rest - 1u;
            }
            piece->count = n;
            piece->closed = false;
            piece->cap = cap;
            piece->style.dashOffset = command.style.dashOffset + start;
            auto down = !plan.lifted || run.lowered;
            piece->layer = down ? plan.layer : command.layer;
            piece->depth = down ? _pick(command.depth, run.range) : command.depth;
            piece->piece = r ? 2u : 1u;
            piece->whole = command.depth;
        }
        delete[] command.points;
        delete[] command.depths;
        command.points = nullptr;
        command.depths = nullptr;
    }
    // Output takes the old array, which holds nothing now, and frees it.
    std::swap(commands, output.commands);
    std::swap(count, output.count);
    std::swap(capacity, output.capacity);
    return Result::Success;
}

}  // namespace vis
