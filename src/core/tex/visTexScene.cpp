// Builds a laid-out formula into scene objects: glyph outlines, radicals, delimiters and
// accents drawn as shapes, measured in the scene's fonts, with the keys that pair two formulas.
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "visTex.h"
#include "visTexScene.h"
#include "../common/visSort.h"

namespace vis::tex
{

namespace detail
{

// The size of the plane text glyphs are measured at, whose metrics divided by it are ems.
static constexpr float ProbeSize = 100.0f;
// How far letters lean where the font has no math italic, about twelve degrees.
static constexpr float Slant = 0.2f;
static constexpr uint32_t PathSamples = 16u;
static constexpr uint32_t None32 = 0xffffffffu;
// The most cells the longest common subsequence of two formulas' pieces may take.
static constexpr uint64_t MatchLimit = 1u << 22;

static bool _fail(char* error, uint32_t errorSize, const char* format, ...)
{
    if (error && errorSize) {
        va_list args;
        va_start(args, format);
        std::vsnprintf(error, errorSize, format, args);
        va_end(args);
    }
    return false;
}

static const TextTheme& _typography(const Theme& theme, TextRole role)
{
    switch (role) {
        case TextRole::H1: return theme.h1;
        case TextRole::H2: return theme.h2;
        case TextRole::H3: return theme.h3;
        case TextRole::Code: return theme.code;
        default: return theme.text;
    }
}

// Measures glyphs as the scene sets them, in the formula's font as plane text of a known size.
struct SceneMetrics final : Metrics
{
    Scene* scene = nullptr;
    Text* probe = nullptr;
    char* buffer = nullptr;
    uint32_t capacity = 0;
    // Why the scene could not measure, when it could not.
    Result result = Result::Success;

    SceneMetrics() = default;
    SceneMetrics(const SceneMetrics&) = delete;
    SceneMetrics& operator=(const SceneMetrics&) = delete;

    ~SceneMetrics() override
    {
        delete probe;
        delete[] buffer;
    }

    // The text with its letters and digits in the alphabet of variant.
    const char* spell(const char* text, uint32_t length, Variant variant) noexcept
    {
        auto need = length * 4u + 1u;
        if (need > capacity) {
            auto grown = new (std::nothrow) char[need];
            if (!grown) return nullptr;
            delete[] buffer;
            buffer = grown;
            capacity = need;
        }
        auto written = 0u;
        for (auto at = 0u; at < length;) {
            uint32_t codepoint;
            at += decode(text + at, length - at, codepoint);
            written += encode(alphabet(codepoint, variant), buffer + written);
        }
        buffer[written] = '\0';
        return buffer;
    }

    bool measure(const char* text, TextExtent& extent) noexcept
    {
        if (probe->text(text) != Result::Success) {
            result = Result::OutOfMemory;
            return false;
        }
        auto measured = scene->measure(probe, extent);
        if (measured != Result::Success) {
            if (result == Result::Success) result = measured;
            return false;
        }
        return true;
    }

    bool glyph(const char* text, uint32_t length, Variant variant, Glyph& output) noexcept override
    {
        output = {};
        TextExtent extent;
        if (variant != Variant::Upright) {
            auto spelled = spell(text, length, variant);
            if (!spelled || !measure(spelled, extent)) return false;
            // Italic and bold letters the font lacks are set in its own, italic ones leaning;
            // other alphabets stay missing, so the audit names them.
            if (!extent.missing || (variant != Variant::Italic && variant != Variant::Bold)) {
                write(extent, output);
                return true;
            }
            output.plain = true;
            output.slant = variant == Variant::Italic ? Slant : 0.0f;
        }
        auto spelled = spell(text, length, Variant::Upright);
        if (!spelled || !measure(spelled, extent)) return false;
        write(extent, output);
        return output.slant == 0.0f || lean(spelled, output);
    }

    // Reads how far the outlines of text reach once they lean by the glyph's slant, as its text
    // draws them; the box the layout sets by stays upright.
    bool lean(const char* text, Glyph& output) noexcept
    {
        TextExtent extent;
        probe->slant = output.slant;
        auto measured = measure(text, extent);
        probe->slant = 0.0f;
        if (!measured) return false;
        output.inkLeft = extent.left / ProbeSize;
        output.inkRight = extent.right / ProbeSize;
        return true;
    }

    static void write(const TextExtent& extent, Glyph& output)
    {
        output.advance = extent.width / ProbeSize;
        output.left = extent.left / ProbeSize;
        output.right = extent.right / ProbeSize;
        output.top = extent.top / ProbeSize;
        output.bottom = extent.bottom / ProbeSize;
        output.inkLeft = output.left;
        output.inkRight = output.right;
        output.missing = extent.missing > 0u;
    }
};

struct Point2
{
    float x = 0.0f;
    float y = 0.0f;
};

static Point2 _add(Point2 a, Point2 b, float scale = 1.0f)
{
    return {a.x + b.x * scale, a.y + b.y * scale};
}

static Point2 _unit(Point2 from, Point2 to)
{
    auto x = to.x - from.x;
    auto y = to.y - from.y;
    auto length = std::sqrt(x * x + y * y);
    if (length <= 0.0f) return {1.0f, 0.0f};
    return {x / length, y / length};
}

// The normal to the left of a direction.
static Point2 _left(Point2 d)
{
    return {-d.y, d.x};
}

// Where the line through p along d meets the line through q along e, or q when they do not.
static Point2 _meet(Point2 p, Point2 d, Point2 q, Point2 e)
{
    auto cross = d.x * e.y - d.y * e.x;
    if (std::fabs(cross) < 1e-6f) return q;
    auto s = ((q.x - p.x) * e.y - (q.y - p.y) * e.x) / cross;
    return {p.x + s * d.x, p.y + s * d.y};
}

// Where the line through p along d crosses height y.
static Point2 _atHeight(Point2 p, Point2 d, float y)
{
    if (std::fabs(d.y) < 1e-6f) return p;
    return {p.x + (y - p.y) * d.x / d.y, y};
}

static float _distance(Point2 a, Point2 b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// A closed outline in the ems of the layout, placed in the world by the formula's frame and
// mirrored about axis for the right-hand delimiters.
struct Outline
{
    static constexpr uint32_t Capacity = 64u;
    PathCommand commands[Capacity];
    uint32_t count = 0;
    bool full = false;
    float left = 0.0f;
    float baseline = 0.0f;
    float z = 0.0f;
    float em = 1.0f;
    bool mirror = false;
    float axis = 0.0f;

    Vec3 at(float x, float y) const
    {
        if (mirror) x = 2.0f * axis - x;
        return {left + x * em, baseline + y * em, z};
    }

    void add(const PathCommand& command)
    {
        if (count == Capacity) full = true;
        else commands[count++] = command;
    }

    void move(Point2 p) { add(PathCommand::move(at(p.x, p.y))); }
    void line(Point2 p) { add(PathCommand::line(at(p.x, p.y))); }
    void cubic(Point2 a, Point2 b, Point2 p) { add(PathCommand::cubic(at(a.x, a.y), at(b.x, b.y), at(p.x, p.y))); }
    void close() { add(PathCommand::close()); }

    void polygon(const Point2* points, uint32_t count)
    {
        move(points[0]);
        for (auto i = 1u; i < count; i++) line(points[i]);
        close();
    }

    void rectangle(float x, float y, float width, float height)
    {
        Point2 points[] = {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
        polygon(points, 4u);
    }

    void circle(Point2 center, float radius)
    {
        auto k = 0.5523f * radius;
        auto x = center.x;
        auto y = center.y;
        move({x + radius, y});
        cubic({x + radius, y + k}, {x + k, y + radius}, {x, y + radius});
        cubic({x - k, y + radius}, {x - radius, y + k}, {x - radius, y});
        cubic({x - radius, y - k}, {x - k, y - radius}, {x, y - radius});
        cubic({x + k, y - radius}, {x + radius, y - k}, {x + radius, y});
        close();
    }

    // Two strokes of half width r meeting at the corner b, as a hat or an angle bracket draws
    // them: butt ends, and a mitered corner, beveled where the miter would reach past 3 r.
    void chevron(Point2 a, Point2 b, Point2 c, float r)
    {
        auto d1 = _unit(a, b);
        auto d2 = _unit(b, c);
        auto n1 = _left(d1);
        auto n2 = _left(d2);
        Point2 points[8];
        auto n = 0u;
        auto corner = [&](float side) {
            auto miter = _meet(_add(a, n1, side * r), d1, _add(b, n2, side * r), d2);
            if (_distance(miter, b) <= 3.0f * r) {
                points[n++] = miter;
            } else {
                points[n++] = _add(b, n1, side * r);
                points[n++] = _add(b, n2, side * r);
            }
        };
        points[n++] = _add(a, n1, r);
        corner(1.0f);
        points[n++] = _add(c, n2, r);
        points[n++] = _add(c, n2, -r);
        auto inner = n;
        corner(-1.0f);
        // The far side runs back from c to a.
        if (n - inner == 2u) {
            auto swap = points[inner];
            points[inner] = points[inner + 1u];
            points[inner + 1u] = swap;
        }
        points[n++] = _add(a, n1, -r);
        polygon(points, n);
    }

    Path* path() const
    {
        if (full || !count) return nullptr;
        return Path::gen(commands, count, PathSamples);
    }
};

// The radical sign as Computer Modern draws it: a short tick, a heavy stroke down to the foot,
// and a thin one up to the top right, cut level to run on into the bar.
static void _radical(Outline& outline, const Item& item)
{
    auto s = item.scale;
    auto t = item.thickness;
    auto x0 = item.x;
    auto y0 = item.y;
    auto w = item.width;
    auto h = item.height;
    auto u = _min(h, 1.2f * s);
    Point2 start = {x0 + 0.02f * s, y0 + 0.36f * u};
    Point2 joint = {x0 + 0.13f * s, y0 + 0.44f * u};
    Point2 foot = {x0 + 0.3f * s + 0.25f * _max(w - 0.62f * s, 0.0f), y0};
    auto r1 = 0.6f * t;
    auto r2 = _min(1.35f * t, 0.05f * s);
    auto r3 = 0.5f * t;
    // The thin stroke ends on the bar's middle, moved so that its lower edge meets the bar's
    // end where the bar's lower edge does.
    Point2 top = {x0 + w, y0 + h - 0.5f * t};
    for (auto i = 0; i < 2; i++) {
        auto d = _unit(foot, top);
        top.x = x0 + w + 0.5f * t * d.x / d.y - r3 / d.y;
    }
    auto d1 = _unit(start, joint);
    auto d2 = _unit(joint, foot);
    auto d3 = _unit(foot, top);
    auto n1 = _left(d1);
    auto n2 = _left(d2);
    auto n3 = _left(d3);
    Point2 points[12];
    auto n = 0u;
    points[n++] = _add(start, n1, r1);
    points[n++] = _meet(_add(start, n1, r1), d1, _add(joint, n2, r2), d2);
    points[n++] = _meet(_add(joint, n2, r2), d2, _add(foot, n3, r3), d3);
    auto upper = _atHeight(_add(foot, n3, r3), d3, y0 + h);
    if (upper.x > x0 + w) upper.x = x0 + w;
    points[n++] = upper;
    points[n++] = {x0 + w, y0 + h};
    points[n++] = {x0 + w, y0 + h - t};
    // The foot's outer corner stays on the bottom of the box, cut level.
    auto corner = _meet(_add(joint, n2, -r2), d2, _add(foot, n3, -r3), d3);
    if (corner.y < y0) {
        points[n++] = _atHeight(_add(foot, n3, -r3), d3, y0);
        points[n++] = _atHeight(_add(joint, n2, -r2), d2, y0);
    } else {
        points[n++] = corner;
    }
    points[n++] = _meet(_add(start, n1, -r1), d1, _add(joint, n2, -r2), d2);
    points[n++] = _add(start, n1, -r1);
    outline.polygon(points, n);
}

// A left parenthesis: a crescent, thick in the middle and thin at its ends, straight between
// its bends once it is tall.
static void _paren(Outline& outline, float x0, float y0, float w, float h, float s, float t)
{
    auto end = x0 + w - 0.12f * w;
    auto middle = x0 + 0.14f * w;
    auto thin = 0.35f * t;
    auto bend = _min(0.5f * h, 1.1f * s);
    auto top = y0 + h;
    auto inner = middle + t;
    auto tip = end - thin;
    outline.move({end, top});
    outline.cubic({end - 0.45f * (end - middle), top - 0.3f * bend}, {middle, top - 0.6f * bend}, {middle, top - bend});
    if (h > 2.0f * bend) outline.line({middle, y0 + bend});
    outline.cubic({middle, y0 + 0.6f * bend}, {end - 0.45f * (end - middle), y0 + 0.3f * bend}, {end, y0});
    outline.line({tip, y0});
    outline.cubic({tip - 0.45f * (tip - inner), y0 + 0.3f * bend}, {inner, y0 + 0.6f * bend}, {inner, y0 + bend});
    if (h > 2.0f * bend) outline.line({inner, top - bend});
    outline.cubic({inner, top - 0.6f * bend}, {tip - 0.45f * (tip - inner), top - 0.3f * bend}, {tip, top});
    outline.close();
}

// A left brace: two hooks into a stem on each side of a point at the middle.
static void _brace(Outline& outline, float x0, float y0, float w, float h, float s, float t)
{
    auto tip = x0 + w - 0.12f * w;
    auto point = x0 + 0.1f * w;
    auto stem = x0 + 0.5f * w;
    auto half = 0.5f * t;
    auto thin = 0.4f * t;
    auto sharp = 0.9f * t;
    auto middle = y0 + 0.5f * h;
    auto r = _min(0.25f * h, 0.28f * s);
    auto top = y0 + h;
    auto outer = stem - half;
    auto inner = stem + half;
    auto reach = point + 0.45f * (outer - point);
    auto back = point + sharp + 0.5f * (inner - point - sharp);
    outline.move({tip, top});
    outline.cubic({stem + 0.2f * (tip - stem), top}, {outer, top - 0.45f * r}, {outer, top - r});
    outline.line({outer, middle + r});
    outline.cubic({outer, middle + 0.5f * r}, {reach, middle}, {point, middle});
    outline.cubic({reach, middle}, {outer, middle - 0.5f * r}, {outer, middle - r});
    outline.line({outer, y0 + r});
    outline.cubic({outer, y0 + 0.45f * r}, {stem + 0.2f * (tip - stem), y0}, {tip, y0});
    outline.line({tip, y0 + thin});
    outline.cubic({stem + 0.25f * (tip - stem), y0 + thin}, {inner, y0 + 0.5f * r}, {inner, y0 + r});
    outline.line({inner, middle - r});
    outline.cubic({inner, middle - 0.45f * r}, {back, middle}, {point + sharp, middle});
    outline.cubic({back, middle}, {inner, middle + 0.45f * r}, {inner, middle + r});
    outline.line({inner, top - r});
    outline.cubic({inner, top - 0.5f * r}, {stem + 0.25f * (tip - stem), top - thin}, {tip, top - thin});
    outline.close();
}

// A delimiter too tall for the font's glyph, drawn in its box as the left one of its pair and
// mirrored for the right one.
static void _delimiter(Outline& outline, const Item& item)
{
    auto delim = static_cast<Delim>(item.shape);
    auto x0 = item.x;
    auto y0 = item.y;
    auto w = item.width;
    auto h = item.height;
    auto s = item.scale;
    auto t = item.thickness;
    auto stem = x0 + 0.15f * w;
    auto arm = x0 + w - 0.12f * w;
    auto bar = 0.8f * t;
    switch (delim) {
        case Delim::RightParen:
        case Delim::RightBracket:
        case Delim::RightBrace:
        case Delim::RightAngle:
        case Delim::RightFloor:
        case Delim::RightCeil:
        case Delim::Backslash:
            outline.mirror = true;
            outline.axis = x0 + 0.5f * w;
            break;
        default: break;
    }
    switch (delim) {
        case Delim::LeftParen:
        case Delim::RightParen: _paren(outline, x0, y0, w, h, s, t); break;
        case Delim::LeftBrace:
        case Delim::RightBrace: _brace(outline, x0, y0, w, h, s, t); break;
        case Delim::LeftBracket:
        case Delim::RightBracket: {
            Point2 points[] = {{stem, y0}, {arm, y0}, {arm, y0 + bar}, {stem + t, y0 + bar},
                               {stem + t, y0 + h - bar}, {arm, y0 + h - bar}, {arm, y0 + h}, {stem, y0 + h}};
            outline.polygon(points, 8u);
            break;
        }
        case Delim::LeftFloor:
        case Delim::RightFloor: {
            Point2 points[] = {{stem, y0}, {arm, y0}, {arm, y0 + bar}, {stem + t, y0 + bar}, {stem + t, y0 + h},
                               {stem, y0 + h}};
            outline.polygon(points, 6u);
            break;
        }
        case Delim::LeftCeil:
        case Delim::RightCeil: {
            Point2 points[] = {{stem, y0}, {stem + t, y0}, {stem + t, y0 + h - bar}, {arm, y0 + h - bar},
                               {arm, y0 + h}, {stem, y0 + h}};
            outline.polygon(points, 6u);
            break;
        }
        case Delim::LeftAngle:
        case Delim::RightAngle:
            outline.chevron({arm, y0 + h}, {x0 + 0.18f * w, y0 + 0.5f * h}, {arm, y0}, 0.45f * t);
            break;
        case Delim::Vert: outline.rectangle(x0 + 0.5f * (w - t), y0, t, h); break;
        case Delim::DoubleVert:
            outline.rectangle(x0 + 0.3f * w - 0.5f * t, y0, t, h);
            outline.rectangle(x0 + 0.7f * w - 0.5f * t, y0, t, h);
            break;
        case Delim::Slash:
        case Delim::Backslash: {
            auto lean = 1.2f * t;
            Point2 points[] = {{x0 + 0.1f * w, y0}, {x0 + 0.1f * w + lean, y0}, {x0 + 0.9f * w, y0 + h},
                               {x0 + 0.9f * w - lean, y0 + h}};
            outline.polygon(points, 4u);
            break;
        }
        default: break;
    }
}

// The wave of a tilde as a cubic in a unit box, which may bulge past it.
static constexpr Point2 TILDE[] = {{0.0f, 0.15f}, {0.38f, 1.35f}, {0.62f, -0.35f}, {1.0f, 0.85f}};

static float _cubic(float a, float b, float c, float d, float t)
{
    auto u = 1.0f - t;
    return u * u * u * a + 3.0f * u * u * t * b + 3.0f * u * t * t * c + t * t * t * d;
}

static void _tilde(Outline& outline, float x0, float y0, float w, float h, float t)
{
    auto low = 1.0f;
    auto high = 0.0f;
    for (auto i = 0; i <= 32; i++) {
        auto y = _cubic(TILDE[0].y, TILDE[1].y, TILDE[2].y, TILDE[3].y, static_cast<float>(i) / 32.0f);
        low = _min(low, y);
        high = _max(high, y);
    }
    auto r = 0.5f * t;
    auto span = _max(h - t, 0.0f);
    auto at = [&](Point2 p, float shift) {
        return Point2{x0 + p.x * w, y0 + r + (p.y - low) / (high - low) * span + shift};
    };
    outline.move(at(TILDE[0], r));
    outline.cubic(at(TILDE[1], r), at(TILDE[2], r), at(TILDE[3], r));
    outline.line(at(TILDE[3], -r));
    outline.cubic(at(TILDE[2], -r), at(TILDE[1], -r), at(TILDE[0], -r));
    outline.close();
}

static void _accent(Outline& outline, const Item& item)
{
    auto x0 = item.x;
    auto y0 = item.y;
    auto w = item.width;
    auto h = item.height;
    auto t = item.thickness;
    switch (static_cast<Accent>(item.shape)) {
        case Accent::Hat:
        case Accent::WideHat: outline.chevron({x0, y0}, {x0 + 0.5f * w, y0 + h}, {x0 + w, y0}, 0.55f * t); break;
        case Accent::Check: outline.chevron({x0, y0 + h}, {x0 + 0.5f * w, y0}, {x0 + w, y0 + h}, 0.55f * t); break;
        case Accent::Tilde:
        case Accent::WideTilde: _tilde(outline, x0, y0, w, h, 1.1f * t); break;
        case Accent::Vec: {
            auto head = _min(0.4f * w, 0.18f * item.scale);
            auto middle = y0 + 0.5f * h;
            auto r = 0.5f * t;
            Point2 points[] = {{x0, middle - r}, {x0 + w - 0.8f * head, middle - r}, {x0 + w - head, y0},
                               {x0 + w, middle}, {x0 + w - head, y0 + h}, {x0 + w - 0.8f * head, middle + r},
                               {x0, middle + r}};
            outline.polygon(points, 7u);
            break;
        }
        case Accent::Dot: {
            auto radius = 0.5f * _min(w, h);
            outline.circle({x0 + 0.5f * w, y0 + 0.5f * h}, radius);
            break;
        }
        case Accent::Ddot: {
            auto radius = 0.5f * _min(0.12f * item.scale, h);
            outline.circle({x0 + radius, y0 + 0.5f * h}, radius);
            outline.circle({x0 + w - radius, y0 + 0.5f * h}, radius);
            break;
        }
        default: outline.rectangle(x0, y0, w, h); break;
    }
}

// Whether the item at index first of the items data starts before the one at second in the TeX;
// sorted stably, the items of one token keep the order of the layout.
static bool _startsBefore(const void* first, const void* second, const void* data)
{
    auto& a = static_cast<const Item*>(data)[*static_cast<const uint32_t*>(first)];
    auto& b = static_cast<const Item*>(data)[*static_cast<const uint32_t*>(second)];
    return a.begin < b.begin;
}

// The frame a formula is set in: its em in world units, the font's ascent and descent in
// ems, and the font size of its glyphs in points, as Text::size takes it.
struct Frame
{
    float em = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float size = 0.0f;
};

static bool _frame(Scene* scene, SceneMetrics& metrics, const Options& options, Frame& frame, char* error,
                   uint32_t errorSize)
{
    auto measureFailed = [&](Result result) {
        auto reason = scene->diagnostic();
        if (result == Result::OutOfMemory) return _fail(error, errorSize, "out of memory");
        return _fail(error, errorSize, "could not measure its font: %s", reason ? reason : "the font cannot set it");
    };
    TextExtent plane;
    if (!metrics.measure("x", plane)) return measureFailed(metrics.result);
    auto& typography = _typography(scene->theme(), options.role);
    auto pixels = options.size > 0.0f ? options.size : typography.size;
    if (options.orientation == TextOrientation::Plane && options.size > 0.0f) {
        frame.em = options.size;
        frame.size = options.size;
    } else {
        // Billboard pixels cover as many world units as the scene converts them to as authored.
        auto board = Text::gen("x");
        if (!board) return _fail(error, errorSize, "out of memory");
        board->orientation = TextOrientation::Billboard;
        board->role = options.role;
        board->size = pixels;
        if (options.font && board->font(options.font) != Result::Success) {
            delete board;
            return _fail(error, errorSize, "out of memory");
        }
        TextExtent extent;
        auto result = scene->measure(board, extent);
        delete board;
        if (result != Result::Success) return measureFailed(result);
        if (!(plane.height > 0.0f) || !(extent.height > 0.0f)) return _fail(error, errorSize, "its font has no height");
        frame.em = ProbeSize * extent.height / plane.height;
        frame.size = options.orientation == TextOrientation::Plane ? frame.em : pixels;
    }
    frame.ascent = plane.ascent / ProbeSize;
    frame.descent = plane.descent / ProbeSize;
    if (!std::isfinite(frame.em) || frame.em <= 0.0f) return _fail(error, errorSize, "its font has no size");
    return true;
}

// The object an item draws, placed by the formula's frame from the left end of its baseline.
static Object* _object(const Item& item, const Options& options, const Frame& frame, float left, float baseline,
                       SceneMetrics& metrics)
{
    auto em = frame.em;
    auto z = options.point.z;
    if (item.kind == ItemKind::Text) {
        auto spelled = metrics.spell(item.text, item.length, item.variant);
        if (!spelled) return nullptr;
        // The text pins the font's line box to its point by the align that would pin the
        // glyph's advance by its outline's height and depth on the same baseline: the box the
        // layout set the glyph in, which a glyph without ink keeps.
        auto ascent = frame.ascent * item.scale * em;
        auto line = (frame.ascent + frame.descent) * item.scale * em;
        auto height = item.height * em;
        auto ink = (item.height + item.depth) * em;
        auto align = line > 0.0f ? ascent / line : 0.0f;
        if (std::fabs(line - ink) > 1e-4f * line) align = (ascent - height) / (line - ink);
        auto x = left + item.x * em;
        auto y = baseline + item.y * em;
        auto text = Text::gen(spelled, {x, y + ascent - align * line, z});
        if (!text) return nullptr;
        text->orientation = options.orientation;
        text->role = options.role;
        text->size = frame.size * item.scale;
        text->slant = item.slant;
        text->align = {0.0f, align};
        text->box = {item.width * em, ink};
        // Any other glyph's box is the one its outlines cover as they are drawn, leaning with
        // it, moved off where align pins it, so that bounds, frames and braces fit its ink.
        auto wide = (item.inkRight - item.inkLeft) * em;
        auto tall = (item.inkTop - item.inkBottom) * em;
        if (wide >= 0.0f && tall >= 0.0f && (wide > 0.0f || tall > 0.0f)) {
            text->box = {wide, tall};
            text->boxOffset = {item.inkLeft * em, item.inkTop * em - ascent + align * (line - tall)};
        }
        if (options.font && text->font(options.font) != Result::Success) {
            delete text;
            return nullptr;
        }
        return text;
    }
    Object* shape = nullptr;
    if (item.kind == ItemKind::Rule || (item.kind == ItemKind::Accent && static_cast<Accent>(item.shape) == Accent::Bar)) {
        shape = Rectangle::gen({left + (item.x + 0.5f * item.width) * em, baseline + (item.y + 0.5f * item.height) * em, z},
                               {item.width * em, item.height * em});
    } else {
        auto outline = new (std::nothrow) Outline;
        if (!outline) return nullptr;
        outline->left = left;
        outline->baseline = baseline;
        outline->z = z;
        outline->em = em;
        if (item.kind == ItemKind::Radical) _radical(*outline, item);
        else if (item.kind == ItemKind::Delimiter) _delimiter(*outline, item);
        else _accent(*outline, item);
        shape = outline->path();
        delete outline;
    }
    if (!shape) return nullptr;
    // Shapes are filled by the group above them, as the glyphs are, and draw no outline.
    shape->stroke({0, 0, 0, 0});
    shape->gradient(false);
    return shape;
}

// What prepare() keeps for draw(): the parts' TeX joined with a blank between each and where
// each starts in it, its parse and layout in ems, the metrics that spell its glyphs, its frame
// and the root's fill, and the options with a copy of their font of their own.
struct Laid
{
    char* joined = nullptr;
    uint32_t* starts = nullptr;
    uint32_t count = 0;
    bool split = false;
    char* font = nullptr;
    Options options;
    Parse parsed;
    Layout laid;
    SceneMetrics metrics;
    Frame frame;
    Color fill;

    Laid() = default;
    Laid(const Laid&) = delete;
    Laid& operator=(const Laid&) = delete;

    ~Laid()
    {
        delete[] joined;
        delete[] starts;
        delete[] font;
    }
};

Built::~Built()
{
    delete laid;
    if (ownsRoot) delete root;
    for (auto i = 0u; i < partCount; i++) {
        if (ownsPart && ownsPart[i]) delete parts[i];
    }
    for (auto i = 0u; i < pieceCount; i++) delete[] pieces[i].key;
    for (auto i = 0u; i < sourceCount; i++) delete[] sources[i];
    delete[] parts;
    delete[] ownsPart;
    delete[] pieces;
    delete[] sources;
}

void key(const char* source, uint32_t begin, uint32_t end, char* output) noexcept
{
    auto written = 0u;
    // The last thing written is a control word, and blanks followed it.
    auto word = false;
    auto blank = false;
    for (auto i = begin; i < end; i++) {
        auto c = source[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (word) blank = true;
            continue;
        }
        auto letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (blank && letter) output[written++] = ' ';
        blank = false;
        word = false;
        output[written++] = c;
        if (c != '\\' || i + 1u >= end) continue;
        auto next = source[i + 1u];
        if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
            while (i + 1u < end && ((source[i + 1u] >= 'a' && source[i + 1u] <= 'z')
                                    || (source[i + 1u] >= 'A' && source[i + 1u] <= 'Z'))) {
                output[written++] = source[++i];
            }
            word = true;
        } else {
            // A control symbol, such as \, or the control space, keeps its character.
            output[written++] = source[++i];
        }
    }
    output[written] = '\0';
}

bool prepare(Scene* scene, const char* const* sources, uint32_t count, bool split, const Options& options,
             Built& output, char* error, uint32_t errorSize) noexcept
{
    if (error && errorSize) error[0] = '\0';
    if (!scene || !sources || !count || (!split && count != 1u) || output.laid || output.root) {
        return _fail(error, errorSize, "a formula needs a scene and its TeX");
    }
    if (!std::isfinite(options.size) || options.size < 0.0f || !std::isfinite(options.point.x)
        || !std::isfinite(options.point.y) || !std::isfinite(options.point.z) || !std::isfinite(options.align.x)
        || !std::isfinite(options.align.y)) {
        return _fail(error, errorSize, "the size, point and align must be finite and the size not negative");
    }

    // The parts read as one formula, a blank between each.
    uint64_t total = 0u;
    for (auto i = 0u; i < count; i++) {
        if (!sources[i]) return _fail(error, errorSize, "part %u is missing", i + 1u);
        total += std::strlen(sources[i]) + (i ? 1u : 0u);
    }
    if (total > SourceLimit) return _fail(error, errorSize, "a formula holds at most %u bytes", SourceLimit);
    auto size = static_cast<uint32_t>(total);
    auto laid = new (std::nothrow) Laid;
    if (!laid) return _fail(error, errorSize, "out of memory");
    output.laid = laid;
    laid->count = count;
    laid->split = split;
    laid->options = options;
    laid->options.font = nullptr;
    if (options.font) {
        auto length = std::strlen(options.font);
        laid->font = new (std::nothrow) char[length + 1u];
        if (!laid->font) return _fail(error, errorSize, "out of memory");
        std::memcpy(laid->font, options.font, length + 1u);
        laid->options.font = laid->font;
    }
    laid->joined = new (std::nothrow) char[size + 1u];
    laid->starts = new (std::nothrow) uint32_t[count + 1u];
    if (!laid->joined || !laid->starts) return _fail(error, errorSize, "out of memory");
    auto joined = laid->joined;
    auto starts = laid->starts;
    auto at = 0u;
    for (auto i = 0u; i < count; i++) {
        if (i) joined[at++] = ' ';
        starts[i] = at;
        auto length = static_cast<uint32_t>(std::strlen(sources[i]));
        std::memcpy(joined + at, sources[i], length);
        at += length;
    }
    starts[count] = at + 1u;
    joined[size] = '\0';

    if (!detail::parse(joined, size, laid->parsed, error, errorSize)) {
        // Name the part that is wrong on its own, where one is.
        for (auto i = 0u; split && count > 1u && i < count; i++) {
            Parse alone;
            char reason[256];
            if (detail::parse(sources[i], static_cast<uint32_t>(std::strlen(sources[i])), alone, reason, sizeof(reason))) {
                continue;
            }
            return _fail(error, errorSize, "part %u: %s", i + 1u, reason);
        }
        return false;
    }

    auto& metrics = laid->metrics;
    metrics.scene = scene;
    metrics.probe = Text::gen("x");
    if (!metrics.probe) return _fail(error, errorSize, "out of memory");
    metrics.probe->orientation = TextOrientation::Plane;
    metrics.probe->role = options.role;
    metrics.probe->size = ProbeSize;
    if (options.font && metrics.probe->font(options.font) != Result::Success) {
        return _fail(error, errorSize, "out of memory");
    }
    auto& frame = laid->frame;
    if (!_frame(scene, metrics, options, frame, error, errorSize)) return false;
    if (!detail::layout(laid->parsed, options.display, metrics, laid->laid, error, errorSize)) {
        if (metrics.result != Result::Success) {
            auto reason = scene->diagnostic();
            return _fail(error, errorSize, "could not measure its font: %s", reason ? reason : "out of memory");
        }
        return false;
    }
    laid->fill = _typography(scene->theme(), options.role).color;

    // The sources of the parts without blanks.
    output.sourceCount = count;
    output.sources = new (std::nothrow) char*[count]{};
    if (!output.sources) return _fail(error, errorSize, "out of memory");
    for (auto i = 0u; i < count; i++) {
        auto length = static_cast<uint32_t>(std::strlen(sources[i]));
        output.sources[i] = new (std::nothrow) char[length + 1u];
        if (!output.sources[i]) return _fail(error, errorSize, "out of memory");
        key(sources[i], 0u, length, output.sources[i]);
    }

    output.width = laid->laid.width * frame.em;
    output.height = laid->laid.height * frame.em;
    output.depth = laid->laid.depth * frame.em;
    output.ascent = frame.ascent * frame.em;
    output.descent = frame.descent * frame.em;
    return true;
}

bool draw(Built& output, const Vec3& origin, bool detached, char* error, uint32_t errorSize) noexcept
{
    if (error && errorSize) error[0] = '\0';
    auto laid = output.laid;
    if (!laid || output.root) return _fail(error, errorSize, "a formula is drawn once, once it is prepared");
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)) {
        return _fail(error, errorSize, "the point must be finite");
    }
    auto& items = laid->laid;
    auto count = laid->count;
    auto split = laid->split;
    std::unique_ptr<uint32_t[]> order(new (std::nothrow) uint32_t[items.count ? 2u * items.count : 1u]);

    // The pieces in the order of the TeX.
    output.pieces = new (std::nothrow) Piece[items.count ? items.count : 1u];
    if (!output.pieces || !order) return _fail(error, errorSize, "out of memory");
    for (auto i = 0u; i < items.count; i++) order[i] = i;
    stableSort(order.get(), order.get() + items.count, items.count, sizeof(uint32_t), _startsBefore, items.items);

    output.root = Group::gen();
    if (!output.root) return _fail(error, errorSize, "out of memory");
    output.ownsRoot = true;
    output.root->typeset = true;
    output.root->fill(laid->fill);
    if (split) {
        output.parts = new (std::nothrow) Group*[count]{};
        output.ownsPart = new (std::nothrow) bool[count]{};
        if (!output.parts || !output.ownsPart) return _fail(error, errorSize, "out of memory");
        for (auto i = 0u; i < count; i++) {
            output.parts[i] = Group::gen();
            if (!output.parts[i]) return _fail(error, errorSize, "out of memory");
            output.partCount = i + 1u;
            output.ownsPart[i] = true;
            if (detached) continue;
            if (output.root->add(output.parts[i]) != Result::Success) return _fail(error, errorSize, "out of memory");
            output.ownsPart[i] = false;
        }
    }

    // Each item from the left end of the baseline, in the depth of origin.
    auto options = laid->options;
    options.point = origin;
    auto part = 0u;
    for (auto i = 0u; i < items.count; i++) {
        auto& item = items.items[order[i]];
        while (part + 1u < count && item.begin >= laid->starts[part + 1u]) part++;
        auto length = item.end > item.begin ? item.end - item.begin : 0u;
        auto& piece = output.pieces[output.pieceCount];
        piece.key = new (std::nothrow) char[length + 1u];
        if (!piece.key) return _fail(error, errorSize, "out of memory");
        key(laid->joined, item.begin, item.begin + length, piece.key);
        piece.part = split ? part : 0u;
        output.pieceCount++;
        auto object = _object(item, options, laid->frame, origin.x, origin.y, laid->metrics);
        if (!object) return _fail(error, errorSize, "out of memory");
        auto parent = split ? static_cast<Object*>(output.parts[part]) : output.root;
        if (parent->add(object) != Result::Success) {
            delete object;
            return _fail(error, errorSize, "could not place '%s'", piece.key);
        }
        piece.object = object;
    }
    delete output.laid;
    output.laid = nullptr;
    return true;
}

Vec3 origin(const Built& output, const Options& options) noexcept
{
    // The box, width by height and depth, pinned to the point by align, or by its baseline.
    auto left = options.point.x - options.align.x * output.width;
    if (options.baseline) return {left, options.point.y, options.point.z};
    auto baseline = options.point.y + options.align.y * (output.height + output.depth) - output.height;
    return {left, baseline, options.point.z};
}

// The pieces of each part, which follow one another in the order of the TeX.
static void _ranges(const Side& side, uint32_t* first, uint32_t* last)
{
    for (auto i = 0u; i < side.sourceCount; i++) first[i] = last[i] = 0u;
    for (auto i = side.count; i > 0u; i--) first[side.pieces[i - 1u].part] = i - 1u;
    for (auto i = 0u; i < side.count; i++) last[side.pieces[i].part] = i + 1u;
}

bool match(const Side& source, const Side& target, uint32_t* partners) noexcept
{
    auto n = source.count;
    auto m = target.count;
    std::unique_ptr<uint32_t[]> taken(new (std::nothrow) uint32_t[m ? m : 1u]);
    std::unique_ptr<uint32_t[]> ranges(new (std::nothrow) uint32_t[2u * (source.sourceCount + target.sourceCount) + 4u]);
    std::unique_ptr<bool[]> used(new (std::nothrow) bool[source.sourceCount ? source.sourceCount : 1u]{});
    if (!taken || !ranges || !used) return false;
    for (auto j = 0u; j < m; j++) taken[j] = None32;
    for (auto i = 0u; i < n; i++) {
        if (partners[i] != None32) taken[partners[i]] = i;
    }
    auto pair = [&](uint32_t i, uint32_t j) {
        partners[i] = j;
        taken[j] = i;
    };
    auto same = [&](uint32_t i, uint32_t j) { return std::strcmp(source.pieces[i].key, target.pieces[j].key) == 0; };

    // Parts that read the same pair piece by piece.
    auto sourceFirst = ranges.get();
    auto sourceLast = sourceFirst + source.sourceCount;
    auto targetFirst = sourceLast + source.sourceCount;
    auto targetLast = targetFirst + target.sourceCount;
    _ranges(source, sourceFirst, sourceLast);
    _ranges(target, targetFirst, targetLast);
    for (auto q = 0u; q < target.sourceCount; q++) {
        auto size = targetLast[q] - targetFirst[q];
        auto free = size > 0u;
        for (auto j = targetFirst[q]; free && j < targetLast[q]; j++) free = taken[j] == None32;
        for (auto p = 0u; free && p < source.sourceCount; p++) {
            if (used[p] || sourceLast[p] - sourceFirst[p] != size || std::strcmp(source.sources[p], target.sources[q]))
                continue;
            auto open = true;
            for (auto i = sourceFirst[p]; open && i < sourceLast[p]; i++) open = partners[i] == None32;
            if (!open) continue;
            for (auto k = 0u; k < size; k++) pair(sourceFirst[p] + k, targetFirst[q] + k);
            used[p] = true;
            break;
        }
    }

    // The pieces left pair by their TeX in order, as the longest common subsequence keeps it.
    std::unique_ptr<uint32_t[]> a(new (std::nothrow) uint32_t[n ? n : 1u]);
    std::unique_ptr<uint32_t[]> b(new (std::nothrow) uint32_t[m ? m : 1u]);
    if (!a || !b) return false;
    auto as = 0u;
    auto bs = 0u;
    for (auto i = 0u; i < n; i++) {
        if (partners[i] == None32) a[as++] = i;
    }
    for (auto j = 0u; j < m; j++) {
        if (taken[j] == None32) b[bs++] = j;
    }
    if (as && bs && static_cast<uint64_t>(as + 1u) * (bs + 1u) <= MatchLimit) {
        auto columns = bs + 1u;
        std::unique_ptr<uint16_t[]> table(new (std::nothrow) uint16_t[static_cast<size_t>(as + 1u) * columns]);
        if (!table) return false;
        for (auto i = as + 1u; i > 0u; i--) {
            for (auto j = bs + 1u; j > 0u; j--) {
                auto& cell = table[static_cast<size_t>(i - 1u) * columns + (j - 1u)];
                if (i - 1u == as || j - 1u == bs) cell = 0u;
                else if (same(a[i - 1u], b[j - 1u])) cell = static_cast<uint16_t>(table[static_cast<size_t>(i) * columns + j] + 1u);
                else {
                    auto down = table[static_cast<size_t>(i) * columns + (j - 1u)];
                    auto right = table[static_cast<size_t>(i - 1u) * columns + j];
                    cell = down > right ? down : right;
                }
            }
        }
        for (auto i = 0u, j = 0u; i < as && j < bs;) {
            if (same(a[i], b[j])) {
                pair(a[i++], b[j++]);
            } else if (table[static_cast<size_t>(i + 1u) * columns + j] >= table[static_cast<size_t>(i) * columns + j + 1u]) {
                i++;
            } else {
                j++;
            }
        }
    }

    // Then each piece of target takes the first piece of source left that reads the same.
    for (auto j = 0u; j < m; j++) {
        if (taken[j] != None32) continue;
        for (auto i = 0u; i < n; i++) {
            if (partners[i] != None32 || !same(i, j)) continue;
            pair(i, j);
            break;
        }
    }
    return true;
}

}  // namespace detail

Formula::~Formula()
{
    delete built;
}

// Sets count sources as one formula in the fonts and theme of scene, as parts when split, into a
// new build, and draws it where the point and align of options pin its box. False with why in
// error when it cannot.
static bool _build(detail::Built*& built, Scene* scene, const char* const* sources, uint32_t count, bool split,
                   const Options& options, char* error, uint32_t errorSize) noexcept
{
    built = new (std::nothrow) detail::Built;
    if (!built) return detail::_fail(error, errorSize, "out of memory");
    return detail::prepare(scene, sources, count, split, options, *built, error, errorSize)
        && detail::draw(*built, detail::origin(*built, options), false, error, errorSize);
}

Formula* Formula::gen(Scene* scene, const char* const* parts, uint32_t count, const Options& options, char* error,
                      uint32_t errorSize) noexcept
{
    auto formula = new (std::nothrow) Formula;
    if (formula && _build(formula->built, scene, parts, count, true, options, error, errorSize)) return formula;
    if (!formula) detail::_fail(error, errorSize, "out of memory");
    delete formula;
    return nullptr;
}

Formula* Formula::gen(Scene* scene, const char* source, const Options& options, char* error,
                      uint32_t errorSize) noexcept
{
    auto formula = new (std::nothrow) Formula;
    if (formula && _build(formula->built, scene, &source, 1u, false, options, error, errorSize)) return formula;
    if (!formula) detail::_fail(error, errorSize, "out of memory");
    delete formula;
    return nullptr;
}

Group* Formula::root() const noexcept
{
    return built->root;
}

Group* Formula::take() noexcept
{
    built->ownsRoot = false;
    return built->root;
}

uint32_t Formula::parts() const noexcept
{
    return built->partCount;
}

Group* Formula::part(uint32_t index) const noexcept
{
    return index < built->partCount ? built->parts[index] : nullptr;
}

const char* Formula::source(uint32_t index) const noexcept
{
    return index < built->sourceCount ? built->sources[index] : nullptr;
}

uint32_t Formula::pieces() const noexcept
{
    return built->pieceCount;
}

Object* Formula::piece(uint32_t index) const noexcept
{
    return index < built->pieceCount ? built->pieces[index].object : nullptr;
}

const char* Formula::key(uint32_t index) const noexcept
{
    return index < built->pieceCount ? built->pieces[index].key : nullptr;
}

uint32_t Formula::partOf(uint32_t index) const noexcept
{
    return index < built->pieceCount ? built->pieces[index].part : 0u;
}

uint32_t pair(const Formula& source, const Formula& target, Object** pairs, uint32_t capacity) noexcept
{
    auto& from = *source.built;
    auto& to = *target.built;
    std::unique_ptr<uint32_t[]> partners(new (std::nothrow) uint32_t[from.pieceCount ? from.pieceCount : 1u]);
    if (!partners) return 0u;
    for (auto i = 0u; i < from.pieceCount; i++) partners[i] = detail::None32;
    detail::Side sides[] = {{from.pieces, from.pieceCount, from.sources, from.sourceCount},
                            {to.pieces, to.pieceCount, to.sources, to.sourceCount}};
    if (!detail::match(sides[0], sides[1], partners.get())) return 0u;
    auto count = 0u;
    for (auto i = 0u; i < from.pieceCount && count < capacity; i++) {
        if (partners[i] == detail::None32) continue;
        if (pairs) {
            pairs[2u * count] = from.pieces[i].object;
            pairs[2u * count + 1u] = to.pieces[partners[i]].object;
        }
        count++;
    }
    return count;
}

}  // namespace vis::tex
