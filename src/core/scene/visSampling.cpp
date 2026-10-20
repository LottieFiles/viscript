// Sampled groups: Riemann sums, vector fields and stream lines built from the callbacks a Range
// steps through.
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <new>

#include "vis.h"
#include "visSceneValidation.h"

namespace vis
{

// The rectangles one Riemann sum may hold.
static constexpr uint32_t RIEMANN_STEP_LIMIT = 4096u;
// The samples of a field along one axis and in all, which bound its arrows and stream lines.
static constexpr uint32_t FIELD_AXIS_LIMIT = 4096u;
static constexpr uint32_t FIELD_SAMPLE_LIMIT = 16384u;
static constexpr uint32_t FIELD_COLOR_LIMIT = 8u;
// The steps a stream line takes each way, and the points all the lines of one call hold.
static constexpr uint32_t STREAM_STEP_LIMIT = 4096u;
static constexpr uint32_t STREAM_POINT_LIMIT = 1048576u;

uint32_t Range::steps() const noexcept
{
    if (!_finite(*this)) return 0u;
    auto steps = std::ceil((static_cast<double>(max) - min) / step - 1.0e-4);
    if (!(steps >= 1.0) || steps > static_cast<double>(UINT32_MAX)) return 0u;
    return static_cast<uint32_t>(steps);
}

// Where step index of steps begins, as Range::stepAt() gives it without counting the steps again.
static float _stepAt(const Range& range, uint32_t index, uint32_t steps)
{
    if (index >= steps) return range.max;
    return static_cast<float>(static_cast<double>(range.min) + static_cast<double>(range.step) * index);
}

float Range::stepAt(uint32_t index) const noexcept
{
    return _stepAt(*this, index, steps());
}

// The rectangles of a Riemann sum over the steps of x, as tall as heights gives them or, when
// heights is nullptr, as fn where sample reads it.
static Result _riemann(const Range& x, uint32_t steps, const float* heights, FunctionSampler fn,
                       void* data, RiemannSample sample, Group*& output) noexcept
{
    auto group = Group::gen();
    if (!group) return Result::OutOfMemory;
    auto bar = [group](float left, float right, float height) {
        auto width = static_cast<float>(static_cast<double>(right) - left);
        // A step too narrow for its ends to differ as floats has nothing to draw.
        if (!(width > 0.0f) || !std::isfinite(width)) return Result::InvalidArguments;
        // A height of zero leaves a flat rectangle, so that every step keeps its place.
        auto size = std::fmax(std::fabs(height), width * 1.0e-6f);
        auto center = static_cast<float>((static_cast<double>(left) + right) * 0.5);
        auto rectangle = Rectangle::gen({center, height * 0.5f, 0.0f}, {width, size});
        if (!rectangle) return Result::OutOfMemory;
        auto result = group->add(rectangle);
        if (result != Result::Success) delete rectangle;
        return result;
    };
    // Where in its step a rectangle reads fn, as a share of the step from its left end.
    auto share = sample == RiemannSample::Right ? 1.0f : sample == RiemannSample::Midpoint ? 0.5f : 0.0f;
    auto result = Result::Success;
    for (auto i = 0u; i < steps && result == Result::Success; i++) {
        auto left = _stepAt(x, i, steps);
        auto right = _stepAt(x, i + 1u, steps);
        auto height = heights ? heights[i] : 0.0f;
        if (!heights && (!fn(left + (right - left) * share, height, data) || !std::isfinite(height))) {
            result = Result::InvalidArguments;
        }
        if (result == Result::Success) result = bar(left, right, height);
    }
    if (result != Result::Success) {
        delete group;
        return result;
    }
    output = group;
    return Result::Success;
}

Result Group::riemann(const Range& x, const float* heights, uint32_t count, Group*& output) noexcept
{
    output = nullptr;
    if (!_finite(x)) return Result::InvalidArguments;
    auto steps = x.steps();
    if (!steps || steps > RIEMANN_STEP_LIMIT) return Result::InsufficientCondition;
    if (!heights || count != steps) return Result::InvalidArguments;
    for (auto i = 0u; i < count; i++) {
        if (!std::isfinite(heights[i])) return Result::InvalidArguments;
    }
    return _riemann(x, steps, heights, nullptr, nullptr, RiemannSample::Left, output);
}

Result Group::riemann(const Range& x, FunctionSampler fn, void* data, RiemannSample sample,
                      Group*& output) noexcept
{
    output = nullptr;
    if (!fn || !_finite(x) || sample > RiemannSample::Midpoint) return Result::InvalidArguments;
    auto steps = x.steps();
    if (!steps || steps > RIEMANN_STEP_LIMIT) return Result::InsufficientCondition;
    return _riemann(x, steps, nullptr, fn, data, sample, output);
}

uint32_t Range::samples() const noexcept
{
    if (!_finite(*this)) return 0u;
    auto intervals = std::floor((static_cast<double>(max) - min) / step + 1.0e-4);
    if (!(intervals >= 0.0) || intervals >= static_cast<double>(UINT32_MAX)) return 0u;
    return static_cast<uint32_t>(intervals) + 1u;
}

float Range::sampleAt(uint32_t index) const noexcept
{
    return static_cast<float>(static_cast<double>(min) + static_cast<double>(step) * index);
}

// Colors spread evenly over a range of magnitudes.
struct ColorMap
{
    const Color* stops = nullptr;
    uint32_t count = 0;
    double low = 0.0;
    double high = 0.0;
};

// Whether colors are none, or 2 to FIELD_COLOR_LIMIT of them, over finite magnitudes.
static bool _validColors(const Color* colors, uint32_t count, float low, float high)
{
    if (!std::isfinite(low) || !std::isfinite(high)) return false;
    if (!colors) return count == 0u;
    return count >= 2u && count <= FIELD_COLOR_LIMIT;
}

// The colors over low to high, or over zero to greatest unless low is below high.
static ColorMap _colorMap(const Color* colors, uint32_t count, float low, float high, double greatest)
{
    if (low < high) return {colors, count, low, high};
    return {colors, count, 0.0, greatest};
}

// The color at magnitude, between the two colors it falls between.
static Color _mapColor(const ColorMap& map, double magnitude)
{
    auto span = map.high - map.low;
    auto at = span > 0.0 ? (magnitude - map.low) / span : 1.0;
    at = std::fmin(std::fmax(at, 0.0), 1.0) * static_cast<double>(map.count - 1u);
    auto i = std::min(static_cast<uint32_t>(at), map.count - 2u);
    auto share = at - static_cast<double>(i);
    auto& from = map.stops[i];
    auto& to = map.stops[i + 1u];
    auto mix = [share](uint8_t a, uint8_t b) {
        return static_cast<uint8_t>(std::lround(static_cast<double>(a) + (static_cast<double>(b) - a) * share));
    };
    return {mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b), mix(from.a, to.a)};
}

// A vector a field gives, which must stay within the range of float.
static bool _finite(double u, double v)
{
    return std::fabs(u) <= FLT_MAX && std::fabs(v) <= FLT_MAX;
}

// The samples of the grid of x and y, which must be within the limits of a field.
static Result _grid(const Range& x, const Range& y, uint32_t& columns, uint32_t& rows)
{
    if (!_finite(x) || !_finite(y)) return Result::InvalidArguments;
    columns = x.samples();
    rows = y.samples();
    if (!columns || !rows || columns > FIELD_AXIS_LIMIT || rows > FIELD_AXIS_LIMIT
        || static_cast<uint64_t>(columns) * rows > FIELD_SAMPLE_LIMIT) {
        return Result::InsufficientCondition;
    }
    return Result::Success;
}

// The arrows of a field over the grid of x and y, for the vectors given or, when vectors is
// nullptr, those fn gives.
static Result _vectorField(const Range& x, const Range& y, const Vec2* vectors, uint32_t count,
                           FieldSampler fn, void* data, const VectorFieldOptions& options,
                           Group*& output, uint32_t* indices) noexcept
{
    output = nullptr;
    uint32_t columns;
    uint32_t rows;
    auto result = _grid(x, y, columns, rows);
    if (result == Result::InvalidArguments) return result;
    if (!std::isfinite(options.length) || options.length < 0.0f || !std::isfinite(options.scale)
        || options.scale < 0.0f || (options.length > 0.0f && options.scale > 0.0f)
        || !std::isfinite(options.tip) || options.tip < 0.0f
        || !_validColors(options.colors, options.colorCount, options.low, options.high)) {
        return Result::InvalidArguments;
    }
    if (result != Result::Success) return result;
    auto samples = columns * rows;
    if (vectors) {
        if (count != samples) return Result::InvalidArguments;
        for (auto k = 0u; k < samples; k++) {
            if (!_finite(vectors[k])) return Result::InvalidArguments;
        }
    } else if (!fn) {
        return Result::InvalidArguments;
    }

    Vec2* sampled = nullptr;
    if (!vectors) {
        sampled = new (std::nothrow) Vec2[samples];
        if (!sampled) return Result::OutOfMemory;
        for (auto k = 0u; k < samples; k++) {
            double u;
            double v;
            if (!fn(x.sampleAt(k % columns), y.sampleAt(k / columns), u, v, data) || !_finite(u, v)) {
                delete[] sampled;
                return Result::InvalidArguments;
            }
            sampled[k] = {static_cast<float>(u), static_cast<float>(v)};
        }
        vectors = sampled;
    }
    auto greatest = 0.0;
    for (auto k = 0u; k < samples; k++)
        greatest = std::fmax(greatest, std::hypot(static_cast<double>(vectors[k].x), vectors[k].y));
    auto map = _colorMap(options.colors, options.colorCount, options.low, options.high, greatest);
    auto scaled = options.scale > 0.0f;
    auto length = options.length > 0.0f ? options.length : 0.8f * std::fmin(x.step, y.step);

    auto group = Group::gen();
    if (!group) result = Result::OutOfMemory;
    // A sample where the field vanishes has no arrow, nor one whose arrow would be a speck.
    auto shortest = 1.0e-3 * (scaled ? options.scale * greatest : length);
    auto arrows = 0u;
    for (auto k = 0u; k < samples && result == Result::Success; k++) {
        auto magnitude = std::hypot(static_cast<double>(vectors[k].x), vectors[k].y);
        auto drawn = scaled ? magnitude * options.scale : length * std::tanh(magnitude / length);
        if (!(magnitude > 0.0) || drawn < shortest) continue;
        auto value = Vec3{static_cast<float>(vectors[k].x / magnitude * drawn),
                          static_cast<float>(vectors[k].y / magnitude * drawn), 0.0f};
        auto origin = Vec3{x.sampleAt(k % columns), y.sampleAt(k / columns), 0.0f};
        if (options.centered) origin = origin - value * 0.5f;
        auto arrow = Vector::gen(value, origin);
        if (!arrow) {
            result = Result::OutOfMemory;
            break;
        }
        arrow->tip = options.tip;
        if (map.stops) arrow->stroke(_mapColor(map, magnitude));
        result = group->add(arrow);
        if (result != Result::Success) {
            delete arrow;
            break;
        }
        if (indices) indices[arrows] = k;
        arrows++;
    }
    delete[] sampled;
    if (result != Result::Success) {
        delete group;
        return result;
    }
    output = group;
    return Result::Success;
}

Result Group::vectorField(const Range& x, const Range& y, FieldSampler fn, void* data, Group*& output,
                          const VectorFieldOptions& options, uint32_t* indices) noexcept
{
    return _vectorField(x, y, nullptr, 0u, fn, data, options, output, indices);
}

Result Group::vectorField(const Range& x, const Range& y, const Vec2* vectors, uint32_t count,
                          Group*& output, const VectorFieldOptions& options, uint32_t* indices) noexcept
{
    if (!vectors) {
        output = nullptr;
        return Result::InvalidArguments;
    }
    return _vectorField(x, y, vectors, count, nullptr, nullptr, options, output, indices);
}

uint32_t StreamLineOptions::steps() const noexcept
{
    if (!std::isfinite(time) || !std::isfinite(dt) || !(time > 0.0f) || !(dt > 0.0f)) return 0u;
    auto steps = std::ceil(static_cast<double>(time) / dt - 1.0e-6);
    if (!(steps <= static_cast<double>(UINT32_MAX))) return 0u;
    return std::max(1u, static_cast<uint32_t>(std::fmax(steps, 0.0)));
}

// Where the lines of one call start, and the field and the box that carry them from there.
struct StreamTrace
{
    const Vec2* seeds = nullptr;
    // The grid that seeds the lines when seeds is nullptr.
    Range x;
    Range y;
    uint32_t columns = 0;
    uint32_t count = 0;
    FieldSampler fn = nullptr;
    void* data = nullptr;
    bool bounded = false;
    double left = 0.0;
    double right = 0.0;
    double bottom = 0.0;
    double top = 0.0;
    // Set once fn returns false, which ends the call.
    bool failed = false;

    Vec2 seed(uint32_t k) const
    {
        if (seeds) return seeds[k];
        return {x.sampleAt(k % columns), y.sampleAt(k / columns)};
    }

    bool inside(double px, double py) const
    {
        return !bounded || (px >= left && px <= right && py >= bottom && py <= top);
    }

    // The field at px, py; false when it is not finite, or when fn fails.
    bool field(double px, double py, double& u, double& v)
    {
        if (!fn(px, py, u, v, data)) {
            failed = true;
            return false;
        }
        return _finite(u, v);
    }
};

// One step of the classic Runge-Kutta method along the field from x, y by h, giving the field
// where the step starts in u and v; false where the field is not finite.
static bool _rungeKutta(StreamTrace& trace, double& x, double& y, double h, double& u, double& v)
{
    double u2, v2, u3, v3, u4, v4;
    if (!trace.field(x, y, u, v)) return false;
    if (!trace.field(x + 0.5 * h * u, y + 0.5 * h * v, u2, v2)) return false;
    if (!trace.field(x + 0.5 * h * u2, y + 0.5 * h * v2, u3, v3)) return false;
    if (!trace.field(x + h * u3, y + h * v3, u4, v4)) return false;
    auto nextX = x + h / 6.0 * (u + 2.0 * u2 + 2.0 * u3 + u4);
    auto nextY = y + h / 6.0 * (v + 2.0 * v2 + 2.0 * v3 + v4);
    if (!_finite(nextX, nextY)) return false;
    x = nextX;
    y = nextY;
    return true;
}

// Traces a stream line from x, y in steps of h, writing the point each step reaches one place
// further from output in direction, until the field vanishes, stops being finite, or carries
// the line out of the box. Returns the points written and adds the speed at each point a step
// leaves from to speed.
static uint32_t _trace(StreamTrace& trace, double x, double y, double h, uint32_t steps, Vec3* output,
                       int direction, double& speed, uint32_t& speeds)
{
    auto written = 0u;
    for (auto step = 0u; step < steps; step++) {
        double u;
        double v;
        auto nextX = x;
        auto nextY = y;
        if (!_rungeKutta(trace, nextX, nextY, h, u, v)) break;
        auto magnitude = std::hypot(u, v);
        if (!(magnitude > 1.0e-9) || !trace.inside(nextX, nextY)) break;
        speed += magnitude;
        speeds++;
        x = nextX;
        y = nextY;
        written++;
        output[direction * static_cast<int>(written)] = {static_cast<float>(x), static_cast<float>(y), 0.0f};
    }
    return written;
}

// A traced line: its plot, the seed it starts from, its length, and the mean speed along it.
struct StreamLine
{
    Plot* plot = nullptr;
    uint32_t seed = 0;
    double length = 0.0;
    double speed = 0.0;
};

static Result _streamLines(StreamTrace& trace, const StreamLineOptions& options, Group*& output,
                           uint32_t* indices) noexcept
{
    auto steps = options.steps();
    if (!steps || steps > STREAM_STEP_LIMIT) return Result::InsufficientCondition;
    // Every line is traced first, into a buffer with its seed in the middle, and kept until the
    // speeds of all of them set the range of the colors.
    auto buffer = new (std::nothrow) Vec3[2u * steps + 1u];
    auto lines = new (std::nothrow) StreamLine[trace.count];
    auto result = buffer && lines ? Result::Success : Result::OutOfMemory;
    auto middle = buffer ? buffer + steps : nullptr;
    auto lineCount = 0u;
    uint64_t total = 0;
    auto longest = 0.0;
    for (auto k = 0u; k < trace.count && result == Result::Success; k++) {
        auto seed = trace.seed(k);
        double x = seed.x;
        double y = seed.y;
        *middle = {seed.x, seed.y, 0.0f};
        auto speed = 0.0;
        auto speeds = 0u;
        auto backward = 0u;
        auto forward = 0u;
        if (options.direction != StreamDirection::Forward) {
            backward = _trace(trace, x, y, -static_cast<double>(options.dt), steps, middle, -1, speed, speeds);
        }
        if (!trace.failed && options.direction != StreamDirection::Backward) {
            forward = _trace(trace, x, y, options.dt, steps, middle, 1, speed, speeds);
        }
        if (trace.failed) {
            result = Result::InvalidArguments;
            break;
        }
        // A seed the field cannot carry anywhere has no line.
        auto points = backward + forward + 1u;
        if (points < 2u) continue;
        total += points;
        if (total > STREAM_POINT_LIMIT) {
            result = Result::InsufficientCondition;
            break;
        }
        // The points run along the field, a backward part toward the seed and on past it.
        auto line = middle - backward;
        auto plot = Plot::gen(line, points);
        if (!plot) {
            result = Result::OutOfMemory;
            break;
        }
        auto length = 0.0;
        for (auto i = 1u; i < points; i++) {
            length += std::hypot(static_cast<double>(line[i].x) - line[i - 1u].x,
                                 static_cast<double>(line[i].y) - line[i - 1u].y);
        }
        lines[lineCount++] = {plot, k, length, speed / speeds};
        longest = std::fmax(longest, length);
    }
    // A line under a thousandth of the longest is left out, as a seed the field cannot carry
    // is: near a point where the field vanishes it would be a speck.
    auto greatest = 0.0;
    for (auto i = 0u; i < lineCount; i++) {
        if (lines[i].length < 1.0e-3 * longest) continue;
        greatest = std::fmax(greatest, lines[i].speed);
    }
    auto map = _colorMap(options.colors, options.colorCount, options.low, options.high, greatest);
    Group* group = nullptr;
    if (result == Result::Success) {
        group = Group::gen();
        if (!group) result = Result::OutOfMemory;
    }
    auto kept = 0u;
    for (auto i = 0u; i < lineCount; i++) {
        auto plot = lines[i].plot;
        if (result != Result::Success || lines[i].length < 1.0e-3 * longest) {
            delete plot;
            continue;
        }
        if (map.stops) plot->stroke(_mapColor(map, lines[i].speed));
        result = group->add(plot);
        if (result != Result::Success) {
            delete plot;
            continue;
        }
        if (indices) indices[kept] = lines[i].seed;
        kept++;
    }
    delete[] lines;
    delete[] buffer;
    if (result != Result::Success) {
        delete group;
        return result;
    }
    output = group;
    return Result::Success;
}

// Whether options trace and color lines as Group::streamLines() takes them.
static bool _validLines(const StreamLineOptions& options)
{
    return options.direction <= StreamDirection::Both && std::isfinite(options.time) && options.time > 0.0f
           && std::isfinite(options.dt) && options.dt > 0.0f
           && _validColors(options.colors, options.colorCount, options.low, options.high);
}

Result Group::streamLines(const Range& x, const Range& y, FieldSampler fn, void* data, Group*& output,
                          const StreamLineOptions& options, uint32_t* indices) noexcept
{
    output = nullptr;
    uint32_t columns;
    uint32_t rows;
    auto result = _grid(x, y, columns, rows);
    if (result == Result::InvalidArguments || !fn || !_validLines(options)) return Result::InvalidArguments;
    if (result != Result::Success) return result;
    StreamTrace trace;
    trace.x = x;
    trace.y = y;
    trace.columns = columns;
    trace.count = columns * rows;
    trace.fn = fn;
    trace.data = data;
    trace.bounded = true;
    trace.left = x.min;
    trace.right = x.max;
    trace.bottom = y.min;
    trace.top = y.max;
    return _streamLines(trace, options, output, indices);
}

Result Group::streamLines(const Vec2* seeds, uint32_t count, const Bounds* region, FieldSampler fn,
                          void* data, Group*& output, const StreamLineOptions& options,
                          uint32_t* indices) noexcept
{
    output = nullptr;
    if (!seeds || !count || !fn || !_validLines(options)) return Result::InvalidArguments;
    for (auto k = 0u; k < count; k++) {
        if (!_finite(seeds[k])) return Result::InvalidArguments;
    }
    if (region && !(std::isfinite(region->min.x) && std::isfinite(region->min.y) && std::isfinite(region->max.x)
                    && std::isfinite(region->max.y) && region->min.x <= region->max.x
                    && region->min.y <= region->max.y)) {
        return Result::InvalidArguments;
    }
    if (count > FIELD_SAMPLE_LIMIT) return Result::InsufficientCondition;
    StreamTrace trace;
    trace.seeds = seeds;
    trace.count = count;
    trace.fn = fn;
    trace.data = data;
    if (region) {
        trace.bounded = true;
        trace.left = region->min.x;
        trace.right = region->max.x;
        trace.bottom = region->min.y;
        trace.top = region->max.y;
    }
    return _streamLines(trace, options, output, indices);
}

}  // namespace vis
