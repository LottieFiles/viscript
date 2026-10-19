// Builds ThorVG paints from a display list for both engines: paths, circles, text with font
// fallback and math glyphs, numbers, pictures and effects, reusing the retained paint tree.
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#include <thorvg.h>

#include "visDefaultFont.h"
#include "visMathGlyphs.h"

#include "vis.h"
#include "visDisplayList.h"
#include "visTvgRenderer.h"
#include "visScene.h"
#include "../../common/visGrow.h"

namespace vis
{

static bool _success(tvg::Result result)
{
    return result == tvg::Result::Success;
}

static void _release(tvg::Paint* paint)
{
    paint->unref();
}

static bool _add(tvg::Scene* scene, tvg::Paint* paint)
{
    if (!paint) return false;
    if (_success(scene->add(paint))) return true;
    _release(paint);
    return false;
}

// Why the most recent paint on this thread could not be built; empty when none failed.
static thread_local char _reason[256];

// Only when no font at all is loaded, not even the default one.
static void _missingFont()
{
    std::snprintf(_reason, sizeof(_reason), "text needs a font, and none is loaded; load a .ttf or .otf file first");
}

// Sets text in the font it names or, when that font is not loaded, in any loaded font, which is
// the default one unless it failed to load: ThorVG's Lottie loader falls back the same way.
static bool _font(tvg::Text* text, const char* font, bool* fallback = nullptr)
{
    if (font && _success(text->font(font))) return true;
    if (fallback) *fallback = true;
    return _success(text->font(nullptr));
}

static void _extent(const DrawCommand& command, Vec2& min, Vec2& max)
{
    if (command.type == CommandType::Circle) {
        min = {command.center.x - command.radius, command.center.y - command.radius};
        max = {command.center.x + command.radius, command.center.y + command.radius};
        return;
    }
    min = max = command.count ? command.points[0] : Vec2{};
    for (auto i = 1u; i < command.count; i++) {
        min.x = std::fmin(min.x, command.points[i].x);
        min.y = std::fmin(min.y, command.points[i].y);
        max.x = std::fmax(max.x, command.points[i].x);
        max.y = std::fmax(max.y, command.points[i].y);
    }
}

static void _linearAxis(const DrawCommand& command, Vec2& from, Vec2& to)
{
    auto& gradient = command.style.gradient;
    if (gradient.line) {
        from = {gradient.from.x, gradient.from.y};
        to = {gradient.to.x, gradient.to.y};
    } else if (command.type == CommandType::Circle) {
        _extent(command, from, to);
    } else if (command.count) {
        if (!command.closed && command.count > 1u) {
            from = command.points[0];
            to = command.points[command.count - 1u];
        }
        if (command.closed || (std::fabs(to.x - from.x) < 1e-6f
                               && std::fabs(to.y - from.y) < 1e-6f)) {
            _extent(command, from, to);
        }
    }
    if (std::fabs(to.x - from.x) < 1e-6f && std::fabs(to.y - from.y) < 1e-6f) {
        to.x = from.x + 1.0f;
    }
}

static uint32_t _stops(const DrawCommand& command, Color paint,
                       tvg::Fill::ColorStop (&stops)[Gradient::StopLimit])
{
    auto& gradient = command.style.gradient;
    if (!gradient.stopCount) {
        // Legacy ramp: paint color to Style::gradientEnd, end alpha follows the paint.
        auto end = command.style.gradientEnd;
        stops[0] = {0.0f, paint.r, paint.g, paint.b, paint.a};
        stops[1] = {1.0f, end.r, end.g, end.b, paint.a};
        return 2u;
    }
    auto count = gradient.stopCount < Gradient::StopLimit ? gradient.stopCount
                                                          : Gradient::StopLimit;
    for (auto i = 0u; i < count; i++) {
        auto& stop = gradient.stops[i];
        auto alpha = (static_cast<uint32_t>(stop.color.a) * paint.a + 127u) / 255u;
        stops[i] = {stop.offset, stop.color.r, stop.color.g, stop.color.b,
                    static_cast<uint8_t>(alpha)};
    }
    return count;
}

// Resolved gradient parameters. Comparing two specs tells the retained tree whether a
// shape's Fill must be regenerated; generating from a spec matches the immediate builder.
struct FillSpec
{
    tvg::Fill::ColorStop stops[Gradient::StopLimit];
    float values[6];
    uint32_t count;
    GradientType type;

    bool operator==(const FillSpec& other) const noexcept
    {
        return std::memcmp(this, &other, sizeof(FillSpec)) == 0;
    }
};

static void _spec(const DrawCommand& command, Color paint, FillSpec& spec)
{
    std::memset(&spec, 0, sizeof(FillSpec));
    auto& gradient = command.style.gradient;
    spec.count = _stops(command, paint, spec.stops);
    Vec2 min;
    Vec2 max;
    _extent(command, min, max);
    auto mid = Vec2{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
    auto center = gradient.centered ? Vec2{gradient.center.x, gradient.center.y}
                  : command.type == CommandType::Circle ? command.center
                                                        : mid;
    switch (gradient.type) {
        case GradientType::Radial: {
            spec.type = GradientType::Radial;
            auto radius = gradient.sized ? gradient.radius
                          : command.type == CommandType::Circle ? command.radius
                                                                : (max - min).length() * 0.5f;
            if (radius < 1e-3f) radius = 1.0f;
            auto focal = gradient.focused ? Vec2{gradient.focal.x, gradient.focal.y} : center;
            spec.values[0] = center.x;
            spec.values[1] = center.y;
            spec.values[2] = radius;
            spec.values[3] = focal.x;
            spec.values[4] = focal.y;
            spec.values[5] = gradient.focalRadius;
            break;
        }
        case GradientType::Conic: {
            spec.type = GradientType::Conic;
            spec.values[0] = center.x;
            spec.values[1] = center.y;
            spec.values[2] = gradient.angle;
            break;
        }
        default: {
            spec.type = GradientType::Linear;
            Vec2 from;
            Vec2 to;
            _linearAxis(command, from, to);
            spec.values[0] = from.x;
            spec.values[1] = from.y;
            spec.values[2] = to.x;
            spec.values[3] = to.y;
            break;
        }
    }
}

static tvg::Fill* _gradient(const FillSpec& spec)
{
    tvg::Fill* fill = nullptr;
    auto result = tvg::Result::Unknown;
    auto& v = spec.values;
    switch (spec.type) {
        case GradientType::Radial: {
            auto radial = tvg::RadialGradient::gen();
            if (!radial) return nullptr;
            result = radial->radial(v[0], v[1], v[2], v[3], v[4], v[5]);
            fill = radial;
            break;
        }
        case GradientType::Conic: {
            auto conic = tvg::ConicGradient::gen();
            if (!conic) return nullptr;
            result = conic->conic(v[0], v[1], v[2]);
            fill = conic;
            break;
        }
        default: {
            auto linear = tvg::LinearGradient::gen();
            if (!linear) return nullptr;
            result = linear->linear(v[0], v[1], v[2], v[3]);
            fill = linear;
            break;
        }
    }
    if (!_success(result) || !_success(fill->colorStops(spec.stops, spec.count))) {
        delete fill;
        return nullptr;
    }
    return fill;
}

static tvg::Fill* _gradient(const DrawCommand& command, Color paint)
{
    FillSpec spec;
    _spec(command, paint, spec);
    return _gradient(spec);
}

static bool _stroke(tvg::Shape* shape, const DrawCommand& command)
{
    if (!command.style.stroke.a || command.style.width <= 0.0f) return true;
    if (!_success(shape->strokeWidth(command.style.width))
        || !_success(shape->strokeCap(command.cap == PathCap::Butt
                                      ? tvg::StrokeCap::Butt
                                      : tvg::StrokeCap::Round))
        || !_success(shape->strokeJoin(tvg::StrokeJoin::Round))
        || (command.style.dashCount
            && !_success(shape->strokeDash(command.style.dash, command.style.dashCount,
                                            command.style.dashOffset)))) {
        return false;
    }
    if (!command.style.gradient.enabled()) {
        auto& color = command.style.stroke;
        return _success(shape->strokeFill(color.r, color.g, color.b, color.a));
    }
    auto gradient = _gradient(command, command.style.stroke);
    if (!gradient) return false;
    if (_success(shape->strokeFill(gradient))) return true;
    delete gradient;
    return false;
}

static bool _fill(tvg::Shape* shape, const DrawCommand& command)
{
    auto& color = command.style.fill;
    if (!command.style.gradient.enabled() || !color.a) {
        return _success(shape->fill(color.r, color.g, color.b, color.a));
    }
    auto gradient = _gradient(command, color);
    if (!gradient) return false;
    if (_success(shape->fill(gradient))) return true;
    delete gradient;
    return false;
}

// Appends the contours of a path command; several contours fill by the even-odd rule.
static bool _contours(tvg::Shape* shape, const DrawCommand& command)
{
    auto contours = command.ends ? command.contours : 1u;
    auto start = 0u;
    for (auto contour = 0u; contour < contours; contour++) {
        auto end = command.ends ? command.ends[contour] : command.count;
        if (!_success(shape->moveTo(command.points[start].x, command.points[start].y))) return false;
        for (auto i = start + 1u; i < end; i++) {
            if (!_success(shape->lineTo(command.points[i].x, command.points[i].y))) return false;
        }
        if (command.closed && !_success(shape->close())) return false;
        start = end;
    }
    return _success(shape->fillRule(command.ends ? tvg::FillRule::EvenOdd : tvg::FillRule::NonZero));
}

static tvg::Shape* _path(const DrawCommand& command)
{
    auto shape = tvg::Shape::gen();
    if (!shape) return nullptr;
    if (!_contours(shape, command)) {
        _release(shape);
        return nullptr;
    }
    if (!_stroke(shape, command) || !_fill(shape, command)) {
        _release(shape);
        return nullptr;
    }
    return shape;
}

static tvg::Shape* _circle(const DrawCommand& command)
{
    auto shape = tvg::Shape::gen();
    if (!shape) return nullptr;
    if (!_success(shape->appendCircle(command.center.x, command.center.y, command.radius, command.radius))) {
        _release(shape);
        return nullptr;
    }
    if (!_stroke(shape, command) || !_fill(shape, command)) {
        _release(shape);
        return nullptr;
    }
    return shape;
}

// Places a text paint by its transform, or at its center when it has none, leaning by its
// slant about the first baseline, which the font sets at the paint's size.
static tvg::Matrix _textMatrix(const DrawCommand& command, tvg::Text* text)
{
    auto& source = command.transform.e;
    tvg::Matrix matrix = {1.0f, 0.0f, command.center.x, 0.0f, 1.0f, command.center.y, 0.0f, 0.0f, 1.0f};
    if (command.transformed) {
        matrix = {source[0], source[1], source[2],
                  source[3], source[4], source[5],
                  source[6], source[7], source[8]};
    }
    tvg::TextMetrics metrics;
    if (command.slant == 0.0f || !_success(text->metrics(metrics))) return matrix;
    auto lines = 1.0f;
    for (auto at = text->text(); at && *at; at++) {
        if (*at == '\n') lines++;
    }
    auto baseline = metrics.ascent - command.align.y * (metrics.advance * lines - metrics.linegap);
    // In the paint's own space, y down: x' = x - slant * (y - baseline), then the placement.
    auto shear = -command.slant;
    return {matrix.e11, matrix.e12 + shear * matrix.e11, matrix.e13 - shear * baseline * matrix.e11,
            matrix.e21, matrix.e22 + shear * matrix.e21, matrix.e23 - shear * baseline * matrix.e21,
            matrix.e31, matrix.e32 + shear * matrix.e31, matrix.e33 - shear * baseline * matrix.e31};
}

static tvg::Matrix _pictureMatrix(const DrawCommand& command)
{
    auto& source = command.picture;
    auto& origin = command.points[0];
    return {(command.points[1].x - origin.x) / static_cast<float>(source.width),
            (command.points[2].x - origin.x) / static_cast<float>(source.height), origin.x,
            (command.points[1].y - origin.y) / static_cast<float>(source.width),
            (command.points[2].y - origin.y) / static_cast<float>(source.height), origin.y,
            0.0f, 0.0f, 1.0f};
}

// Only symbols absent from the selected font use the bundled math outlines.
static const MathGlyph* _mathGlyph(const char* character)
{
    for (auto& glyph : MATH_GLYPHS) {
        if (std::strcmp(glyph.text, character) == 0) return &glyph;
    }
    return nullptr;
}

static uint32_t _character(const char*& at, char (&character)[5])
{
    auto lead = static_cast<uint8_t>(*at);
    auto length = lead < 0x80 ? 1u : lead < 0xe0 ? 2u : lead < 0xf0 ? 3u : 4u;
    auto read = 0u;
    while (read < length && *at) character[read++] = *at++;
    return read;
}

static const MathGlyph* _fallback(tvg::Text* text, const char* character)
{
    auto math = _mathGlyph(character);
    if (!math) return nullptr;
    tvg::GlyphMetrics glyph;
    if (_success(text->metrics(character, glyph)) && glyph.advance > 0.0f) return nullptr;
    return math;
}

static bool _needsMath(tvg::Text* text, const char* value)
{
    for (auto at = value; *at;) {
        char character[5] = {};
        _character(at, character);
        if (_fallback(text, character)) return true;
    }
    return false;
}

static float _digitAdvance(tvg::Text* text)
{
    auto width = 0.0f;
    for (char digit = '0'; digit <= '9'; digit++) {
        char character[] = {digit, 0};
        tvg::GlyphMetrics glyph;
        if (_success(text->metrics(character, glyph))) width = std::fmax(width, glyph.advance);
    }
    return width;
}

static bool _tabular(const char* character)
{
    return (character[0] >= '0' && character[0] <= '9') || character[0] == '-' || character[0] == ' ';
}

static tvg::Paint* _mathText(const DrawCommand& command, tvg::Text* text)
{
    tvg::TextMetrics metrics;
    if (!_success(text->metrics(metrics))) return nullptr;
    auto scale = command.size * (4.0f / 3.0f);
    auto cell = command.tabular ? _digitAdvance(text) : 0.0f;
    auto width = 0.0f; auto pen = 0.0f; auto lines = 1u;
    for (auto at = command.text; *at;) {
        char character[5] = {};
        _character(at, character);
        if (character[0] == '\n') { width = std::fmax(width, pen); pen = 0.0f; lines++; continue; }
        auto math = _fallback(text, character);
        tvg::GlyphMetrics glyph;
        if (math) pen += math->advance * scale;
        else if (_success(text->metrics(character, glyph))) pen += command.tabular && _tabular(character) ? cell : glyph.advance;
    }
    width = std::fmax(width, pen);
    auto height = metrics.advance * lines - metrics.linegap;
    auto left = -command.align.x * width;
    auto baseline = metrics.ascent - command.align.y * height;
    auto scene = tvg::Scene::gen();
    if (!scene) return nullptr;
    pen = 0.0f; auto row = 0u;
    for (auto at = command.text; *at;) {
        char character[5] = {};
        _character(at, character);
        if (character[0] == '\n') { pen = 0.0f; row++; continue; }
        auto math = _fallback(text, character);
        auto y = baseline + row * metrics.advance;
        auto x = left + pen;
        tvg::GlyphMetrics extent;
        auto found = _success(text->metrics(character, extent));
        auto tabular = command.tabular && _tabular(character);
        if (tabular && found) x += (cell - extent.advance) * 0.5f;
        tvg::Paint* paint = nullptr;
        if (math) {
            auto shape = tvg::Shape::gen();
            if (shape && _success(shape->appendPath(math->commands, math->commandCount, math->points, math->pointCount))
                && _success(shape->fill(command.style.fill.r, command.style.fill.g, command.style.fill.b))
                && _success(shape->transform({scale, 0.0f, x, 0.0f, -scale, y, 0.0f, 0.0f, 1.0f}))) paint = shape;
            else if (shape) _release(shape);
            pen += math->advance * scale;
        } else {
            auto glyph = tvg::Text::gen();
            if (glyph && _font(glyph, command.font) && _success(glyph->size(command.size))
                && _success(glyph->text(character)) && _success(glyph->align(0.0f, 0.0f))
                && _success(glyph->fill(command.style.fill.r, command.style.fill.g, command.style.fill.b))
                && _success(glyph->translate(x, y - metrics.ascent))) paint = glyph;
            else if (glyph) _release(glyph);
            if (found) pen += tabular ? cell : extent.advance;
        }
        if (!_add(scene, paint)) { _release(scene); return nullptr; }
    }
    if (!_success(scene->opacity(command.style.fill.a)) || !_success(scene->transform(_textMatrix(command, text)))) {
        _release(scene); return nullptr;
    }
    return scene;
}

static tvg::Paint* _text(const DrawCommand& command)
{
    auto text = tvg::Text::gen();
    if (!text) return nullptr;
    if (!_font(text, command.font)) {
        _missingFont();
        _release(text);
        return nullptr;
    }
    if (!_success(text->size(command.size)) || !_success(text->text(command.text))) {
        _release(text);
        return nullptr;
    }
    if (command.tabular || _needsMath(text, command.text)) {
        // The matrix uses the native line metrics and the same aligned baseline as measurement.
        text->align(command.align.x, command.align.y);
        auto paint = _mathText(command, text);
        _release(text);
        return paint;
    }
    if (!_success(text->align(command.align.x, command.align.y)) || !_success(text->fill(command.style.fill.r, command.style.fill.g, command.style.fill.b)) || !_success(text->opacity(command.style.fill.a))) {
        _release(text);
        return nullptr;
    }
    if (command.transformed || command.slant != 0.0f) {
        if (!_success(text->transform(_textMatrix(command, text)))) {
            _release(text);
            return nullptr;
        }
    } else if (!_success(text->translate(command.center.x, command.center.y))) {
        _release(text);
        return nullptr;
    }
    return text;
}

// Shortest text that reads back as value, written as std::to_chars(value, general) writes it:
// fixed for decimal exponents -4..5 and scientific otherwise, as %g does at its default
// precision. Built on snprintf so the WASM runtime does not link libc++'s ~100 KB of Ryu tables.
static bool _shortest(float value, char* output, size_t capacity)
{
    if (!std::isfinite(value) || value == 0.0f) {
        auto n = std::snprintf(output, capacity, "%g", value);
        return n > 0 && static_cast<size_t>(n) < capacity;
    }
    char sci[32];
    for (auto precision = 0; precision < 9; precision++) {
        std::snprintf(sci, sizeof(sci), "%.*e", precision, value);
        if (std::strtof(sci, nullptr) == value) break;
    }
    // sci is [-]d[.ddd]e±XX: collect sign, digits and the decimal exponent
    auto at = sci;
    auto negative = *at == '-';
    if (negative) at++;
    char digits[16];
    auto count = 0;
    for (; *at != 'e'; at++) {
        if (*at != '.') digits[count++] = *at;
    }
    auto exponent = std::atoi(at + 1);
    while (count > 1 && digits[count - 1] == '0') count--;

    char fixed[64], shortSci[32];
    auto f = 0;
    if (negative) fixed[f++] = '-';
    if (exponent >= 0) {
        for (auto i = 0; i <= exponent || i < count; i++) {
            if (i == exponent + 1) fixed[f++] = '.';
            fixed[f++] = i < count ? digits[i] : '0';
        }
    } else {
        fixed[f++] = '0';
        fixed[f++] = '.';
        for (auto i = 0; i < -exponent - 1; i++) fixed[f++] = '0';
        for (auto i = 0; i < count; i++) fixed[f++] = digits[i];
    }
    fixed[f] = '\0';
    auto s = 0;
    if (negative) shortSci[s++] = '-';
    shortSci[s++] = digits[0];
    if (count > 1) {
        shortSci[s++] = '.';
        for (auto i = 1; i < count; i++) shortSci[s++] = digits[i];
    }
    s += std::snprintf(shortSci + s, sizeof(shortSci) - s, "e%c%02d", exponent < 0 ? '-' : '+',
                       exponent < 0 ? -exponent : exponent);
    auto useFixed = exponent >= -4 && exponent < 6;
    auto pick = useFixed ? fixed : shortSci;
    auto length = static_cast<size_t>(useFixed ? f : s);
    if (length >= capacity) return false;
    std::memcpy(output, pick, length + 1);
    return true;
}

static bool _numberText(const DrawCommand& command, char (&value)[64])
{
    return _shortest(command.number, value, sizeof(value));
}

static tvg::Paint* _number(const DrawCommand& command)
{
    char value[64];
    if (!_numberText(command, value)) return nullptr;
    auto copy = command;
    copy.text = value;
    copy.font = nullptr;
    return _text(copy);
}

static tvg::Picture* _picture(const DrawCommand& command)
{
    auto& source = command.picture;
    auto picture = tvg::Picture::gen();
    if (!picture) return nullptr;
    // Encoded bytes are owned by the Picture object, so ThorVG may reference them without copying.
    auto loaded = source.encoded
        ? _success(picture->load(reinterpret_cast<const char*>(source.encoded), source.encodedSize, source.mime, nullptr, false))
          && _success(picture->size(static_cast<float>(source.width), static_cast<float>(source.height)))
        : _success(picture->load(source.pixels, source.width, source.height, tvg::ColorSpace::ABGR8888S, false));
    if (!loaded || !_success(picture->filter(command.filter == ImageFilter::Nearest ? tvg::FilterMethod::Nearest
                                                                                     : tvg::FilterMethod::Bilinear))) {
        _release(picture);
        return nullptr;
    }
    if (!_success(picture->transform(_pictureMatrix(command))) || !_success(picture->opacity(command.opacity))) {
        _release(picture);
        return nullptr;
    }
    return picture;
}

static tvg::Paint* _paint(const DrawCommand& command)
{
    switch (command.type) {
        case CommandType::Path: return _path(command);
        case CommandType::Circle: return _circle(command);
        case CommandType::Text: return _text(command);
        case CommandType::Number: return _number(command);
        case CommandType::Picture: return _picture(command);
    }
    return nullptr;
}

static bool _visible(const DrawCommand& command)
{
    switch (command.type) {
        case CommandType::Path:
        case CommandType::Circle:
            return command.style.fill.a || (command.style.stroke.a && command.style.width > 0.0f);
        case CommandType::Text:
        case CommandType::Number: return command.style.fill.a;
        case CommandType::Picture: return command.opacity;
    }
    return false;
}

static Color _mix(const Color& from, const Color& to, float progress)
{
    auto channel = [progress](uint8_t first, uint8_t second) {
        auto value = static_cast<float>(first)
                   + (static_cast<float>(second) - first) * progress;
        if (value < 0.0f) value = 0.0f;
        if (value > 255.0f) value = 255.0f;
        return static_cast<uint8_t>(value + 0.5f);
    };
    return {
        channel(from.r, to.r), channel(from.g, to.g),
        channel(from.b, to.b), channel(from.a, to.a)};
}

/************************************************************************/
/* Retained paint tree                                                  */
/************************************************************************/

// Bitwise comparisons: a retained property is reapplied whenever its bits change, so the
// retained paint always carries exactly the values a freshly generated paint would.
template<typename T>
static bool _same(const T& first, const T& second)
{
    return std::memcmp(&first, &second, sizeof(T)) == 0;
}

static bool _sameText(const char* first, const char* second)
{
    if (!first || !second) return first == second;
    return std::strcmp(first, second) == 0;
}

static bool _sameColor(const Color& first, const Color& second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b && first.a == second.a;
}

static char* _dup(const char* text)
{
    if (!text) return nullptr;
    auto size = std::strlen(text) + 1u;
    auto copy = new (std::nothrow) char[size];
    if (copy) std::memcpy(copy, text, size);
    return copy;
}

static uint32_t _hash(const void* first, const void* second, uint32_t extra)
{
    auto value = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(first)) * 0x9E3779B97F4A7C15ull;
    value ^= static_cast<uint64_t>(reinterpret_cast<uintptr_t>(second)) * 0xC2B2AE3D27D4EB4Full
             + (value << 6) + (value >> 2);
    value ^= static_cast<uint64_t>(extra) * 0x165667B19E3779F9ull;
    value ^= value >> 31;
    value *= 0xD6E8FEB86659FD93ull;
    value ^= value >> 32;
    return static_cast<uint32_t>(value);
}

// Stable identity of a draw command across frames: the object that emitted it (and its
// morph/blend counterpart), the command type and its rank among the commands that object
// emitted, counted in emission order so depth sorting does not change identities.
struct SlotKey
{
    const Object* object = nullptr;
    const Object* counterpart = nullptr;
    uint32_t rank = 0;
    CommandType type = CommandType::Path;

    bool operator==(const SlotKey& other) const
    {
        return object == other.object && counterpart == other.counterpart
               && rank == other.rank && type == other.type;
    }

    uint32_t hash() const
    {
        return _hash(object, counterpart, rank * 8u + static_cast<uint32_t>(type));
    }
};

// One retained ThorVG paint plus the command state last applied to it.
struct Slot
{
    SlotKey key;
    tvg::Paint* paint = nullptr;  // holds one reference owned by the slot
    bool used = false;
    // Shape
    Vec2* points = nullptr;
    uint32_t count = 0;
    uint32_t capacity = 0;
    uint32_t* ends = nullptr;
    uint32_t contours = 0;
    float radius = 0.0f;
    bool closed = false;
    bool stroked = false;
    bool fillGradient = false;
    bool strokeGradient = false;
    bool dashed = false;
    float width = 0.0f;
    PathCap cap = PathCap::Round;
    float dash[Style::DashLimit] = {};
    float dashOffset = 0.0f;
    uint8_t dashCount = 0;
    Color fill;
    Color stroke;
    FillSpec fillSpec;
    FillSpec strokeSpec;
    // Shape (circle) and Text (translation)
    Vec2 center;
    // Text and Number
    bool mathText = false;
    char* text = nullptr;
    char* font = nullptr;
    float size = 0.0f;
    float slant = 0.0f;
    Vec2 align;
    bool transformed = false;
    // Text and Picture
    tvg::Matrix matrix = {};
    uint8_t opacity = 255;
    // Picture
    PictureSource source;
    ImageFilter filter = ImageFilter::Bilinear;

    Slot() = default;
    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;

    ~Slot()
    {
        delete[] points;
        delete[] ends;
        delete[] text;
        delete[] font;
    }
};

static bool _stroked(const DrawCommand& command)
{
    return command.style.stroke.a && command.style.width > 0.0f;
}

static bool _fillGradient(const DrawCommand& command)
{
    return command.style.gradient.enabled() && command.style.fill.a;
}

static bool _storePoints(Slot& slot, const DrawCommand& command)
{
    if (command.count > slot.capacity) {
        auto points = new (std::nothrow) Vec2[command.count];
        if (!points) return false;
        delete[] slot.points;
        slot.points = points;
        slot.capacity = command.count;
    }
    if (command.count) std::memcpy(slot.points, command.points, sizeof(Vec2) * command.count);
    slot.count = command.count;
    slot.closed = command.closed;
    delete[] slot.ends;
    slot.ends = nullptr;
    slot.contours = 0;
    if (command.ends) {
        slot.ends = new (std::nothrow) uint32_t[command.contours];
        if (!slot.ends) return false;
        std::memcpy(slot.ends, command.ends, sizeof(uint32_t) * command.contours);
        slot.contours = command.contours;
    }
    return true;
}

static bool _storeText(char*& target, const char* text)
{
    if (_sameText(target, text)) return true;
    auto copy = _dup(text);
    if (text && !copy) return false;
    delete[] target;
    target = copy;
    return true;
}

static bool _geometry(tvg::Shape* shape, const DrawCommand& command)
{
    if (command.type == CommandType::Circle) {
        return _success(shape->appendCircle(command.center.x, command.center.y,
                                            command.radius, command.radius));
    }
    return _contours(shape, command);
}

// Records the state of a paint freshly generated by _paint(command).
static bool _capture(Slot& slot, const DrawCommand& command)
{
    auto& style = command.style;
    switch (command.type) {
        case CommandType::Path:
        case CommandType::Circle: {
            if (command.type == CommandType::Path && !_storePoints(slot, command)) return false;
            slot.center = command.center;
            slot.radius = command.radius;
            slot.stroked = _stroked(command);
            slot.fillGradient = _fillGradient(command);
            slot.strokeGradient = slot.stroked && style.gradient.enabled();
            slot.dashed = slot.stroked && style.dashCount;
            slot.width = style.width;
            slot.cap = command.cap;
            std::memcpy(slot.dash, style.dash, sizeof(slot.dash));
            slot.dashCount = style.dashCount;
            slot.dashOffset = style.dashOffset;
            slot.fill = style.fill;
            slot.stroke = style.stroke;
            if (slot.fillGradient) _spec(command, style.fill, slot.fillSpec);
            if (slot.strokeGradient) _spec(command, style.stroke, slot.strokeSpec);
            return true;
        }
        case CommandType::Text:
        case CommandType::Number: {
            char value[64];
            auto text = command.text;
            auto font = command.font;
            if (command.type == CommandType::Number) {
                if (!_numberText(command, value)) return false;
                text = value;
                font = nullptr;
            }
            if (!_storeText(slot.text, text) || !_storeText(slot.font, font)) return false;
            slot.size = command.size;
            slot.align = command.align;
            slot.fill = style.fill;
            slot.transformed = command.transformed;
            slot.slant = command.slant;
            slot.center = command.center;
            slot.mathText = slot.paint->type() == tvg::Type::Scene;
            if (!slot.mathText && (command.transformed || command.slant != 0.0f)) {
                slot.matrix = _textMatrix(command, static_cast<tvg::Text*>(slot.paint));
            }
            return true;
        }
        case CommandType::Picture: {
            slot.source = command.picture;
            slot.filter = command.filter;
            slot.matrix = _pictureMatrix(command);
            slot.opacity = command.opacity;
            return true;
        }
    }
    return false;
}

// Applies only the properties that changed since the last frame. Returns false with
// replace == true when the paint must be regenerated (structural change).
static bool _updateShape(Slot& slot, const DrawCommand& command, bool& replace)
{
    auto& style = command.style;
    auto stroked = _stroked(command);
    auto fillGradient = _fillGradient(command);
    auto strokeGradient = stroked && style.gradient.enabled();
    auto dashed = stroked && style.dashCount;
    if (stroked != slot.stroked || fillGradient != slot.fillGradient
        || strokeGradient != slot.strokeGradient || dashed != slot.dashed) {
        replace = true;
        return false;
    }
    auto shape = static_cast<tvg::Shape*>(slot.paint);
    // The CPU engine derives the fill coverage (and its antialiasing, which depends on the
    // stroke width and stroke alpha) only when the path is marked dirty. Changes to those
    // inputs therefore re-append the path so the result matches a freshly generated shape.
    auto rebuild = (slot.fill.a == 0) != (style.fill.a == 0)
                   || (stroked && (slot.stroke.a != style.stroke.a || !_same(slot.width, style.width)));
    if (command.type == CommandType::Path) {
        auto same = slot.count == command.count && slot.closed == command.closed
                    && (!command.count
                        || std::memcmp(slot.points, command.points, sizeof(Vec2) * command.count) == 0)
                    && slot.contours == (command.ends ? command.contours : 0u)
                    && (!command.ends
                        || std::memcmp(slot.ends, command.ends, sizeof(uint32_t) * command.contours) == 0);
        if (!same || rebuild) {
            if (!_success(shape->reset()) || !_geometry(shape, command) || !_storePoints(slot, command)) {
                return false;
            }
        }
    } else if (rebuild || !_same(slot.center, command.center) || !_same(slot.radius, command.radius)) {
        if (!_success(shape->reset()) || !_geometry(shape, command)) return false;
        slot.center = command.center;
        slot.radius = command.radius;
    }

    if (stroked) {
        if (!_same(slot.width, style.width)) {
            if (!_success(shape->strokeWidth(style.width))) return false;
            slot.width = style.width;
        }
        if (slot.cap != command.cap) {
            if (!_success(shape->strokeCap(command.cap == PathCap::Butt ? tvg::StrokeCap::Butt
                                                                         : tvg::StrokeCap::Round))) {
                return false;
            }
            slot.cap = command.cap;
        }
        if (dashed && (slot.dashCount != style.dashCount || !_same(slot.dash, style.dash)
                       || !_same(slot.dashOffset, style.dashOffset))) {
            if (!_success(shape->strokeDash(style.dash, style.dashCount, style.dashOffset))) return false;
            std::memcpy(slot.dash, style.dash, sizeof(slot.dash));
            slot.dashCount = style.dashCount;
            slot.dashOffset = style.dashOffset;
        }
        if (strokeGradient) {
            FillSpec spec;
            _spec(command, style.stroke, spec);
            if (!(spec == slot.strokeSpec)) {
                auto gradient = _gradient(spec);
                if (!gradient) return false;
                if (!_success(shape->strokeFill(gradient))) {
                    delete gradient;
                    return false;
                }
                slot.strokeSpec = spec;
            }
        } else if (!_sameColor(slot.stroke, style.stroke)) {
            if (!_success(shape->strokeFill(style.stroke.r, style.stroke.g, style.stroke.b, style.stroke.a))) {
                return false;
            }
        }
        slot.stroke = style.stroke;
    }

    if (fillGradient) {
        FillSpec spec;
        _spec(command, style.fill, spec);
        if (!(spec == slot.fillSpec)) {
            auto gradient = _gradient(spec);
            if (!gradient) return false;
            if (!_success(shape->fill(gradient))) {
                delete gradient;
                return false;
            }
            slot.fillSpec = spec;
        }
    } else if (!_sameColor(slot.fill, style.fill)) {
        if (!_success(shape->fill(style.fill.r, style.fill.g, style.fill.b, style.fill.a))) return false;
    }
    slot.fill = style.fill;
    return true;
}

static bool _updateText(Slot& slot, const DrawCommand& command, bool& replace)
{
    char value[64];
    auto text = command.text;
    auto font = command.font;
    if (command.type == CommandType::Number) {
        if (!_numberText(command, value)) return false;
        text = value;
        font = nullptr;
    }
    if (slot.transformed != command.transformed || !_same(slot.slant, command.slant)
        || !_sameText(slot.font, font)) {
        replace = true;
        return false;
    }
    if (slot.mathText || command.tabular) { replace = true; return false; }
    auto paint = static_cast<tvg::Text*>(slot.paint);
    if (_needsMath(paint, text)) { replace = true; return false; }
    auto& fill = command.style.fill;
    // ThorVG lays glyphs out lazily and only text() reliably invalidates that layout (align()
    // and size() alone do not re-run it), so any layout input change resets the string too.
    auto relayout = false;
    if (!_same(slot.size, command.size)) {
        if (!_success(paint->size(command.size))) return false;
        slot.size = command.size;
        relayout = true;
    }
    if (!_same(slot.align, command.align)) {
        if (!_success(paint->align(command.align.x, command.align.y))) return false;
        slot.align = command.align;
        relayout = true;
    }
    if (relayout || !_sameText(slot.text, text)) {
        if (!_success(paint->text(text)) || !_storeText(slot.text, text)) return false;
    }
    if (slot.fill.r != fill.r || slot.fill.g != fill.g || slot.fill.b != fill.b) {
        if (!_success(paint->fill(fill.r, fill.g, fill.b))) return false;
    }
    if (slot.fill.a != fill.a && !_success(paint->opacity(fill.a))) return false;
    slot.fill = fill;
    if (command.transformed || command.slant != 0.0f) {
        auto matrix = _textMatrix(command, paint);
        if (!_same(slot.matrix, matrix)) {
            if (!_success(paint->transform(matrix))) return false;
            slot.matrix = matrix;
        }
    } else if (!_same(slot.center, command.center)) {
        if (!_success(paint->translate(command.center.x, command.center.y))) return false;
        slot.center = command.center;
    }
    return true;
}

static bool _updatePicture(Slot& slot, const DrawCommand& command, bool& replace)
{
    auto& source = command.picture;
    auto& last = slot.source;
    if (source.encoded != last.encoded || source.encodedSize != last.encodedSize
        || source.mime != last.mime || source.pixels != last.pixels
        || source.width != last.width || source.height != last.height) {
        replace = true;
        return false;
    }
    auto picture = static_cast<tvg::Picture*>(slot.paint);
    if (slot.filter != command.filter) {
        if (!_success(picture->filter(command.filter == ImageFilter::Nearest ? tvg::FilterMethod::Nearest
                                                                              : tvg::FilterMethod::Bilinear))) {
            return false;
        }
        slot.filter = command.filter;
    }
    auto matrix = _pictureMatrix(command);
    if (!_same(slot.matrix, matrix)) {
        if (!_success(picture->transform(matrix))) return false;
        slot.matrix = matrix;
    }
    if (slot.opacity != command.opacity) {
        if (!_success(picture->opacity(command.opacity))) return false;
        slot.opacity = command.opacity;
    }
    return true;
}

static bool _update(Slot& slot, const DrawCommand& command, bool& replace)
{
    replace = false;
    switch (command.type) {
        case CommandType::Path:
        case CommandType::Circle: return _updateShape(slot, command, replace);
        case CommandType::Text:
        case CommandType::Number: return _updateText(slot, command, replace);
        case CommandType::Picture: return _updatePicture(slot, command, replace);
    }
    return false;
}

struct RetainedNode;

// A child scene mounted into a parent node: wrapper(clip) -> child root(transform).
struct RetainedMount
{
    const Scene* first = nullptr;
    const Scene* second = nullptr;
    uint64_t firstSerial = 0;
    uint64_t secondSerial = 0;
    uint8_t kind = 0;
    tvg::Scene* wrapper = nullptr;  // holds one reference owned by the mount
    tvg::Shape* clipper = nullptr;  // owned by wrapper
    RetainedNode* child = nullptr;
    tvg::Matrix matrix = {};
    float rect[4] = {};
    uint8_t opacity = 255;
    bool placed = false;
};

struct RankEntry
{
    const Object* object;
    const Object* counterpart;
    uint32_t count;
};

// A retained tvg::Scene whose children follow the order the latest frame asked for.
struct RetainedContainer
{
    tvg::Scene* scene = nullptr;  // holds one reference owned by the node or the composite
    tvg::Paint** attached = nullptr;  // mirror of scene->paints()
    uint32_t attachedCnt = 0;
    uint32_t attachedCap = 0;
    tvg::Paint** desired = nullptr;
    uint32_t desiredCnt = 0;
    uint32_t desiredCap = 0;

    RetainedContainer() = default;
    RetainedContainer(const RetainedContainer&) = delete;
    RetainedContainer& operator=(const RetainedContainer&) = delete;

    ~RetainedContainer()
    {
        delete[] attached;
        delete[] desired;
    }
};

// The layer of a composited object: its drawings, and those of its descendants, are children
// of one scene that the node composites at once (see DisplayLayer).
struct RetainedComposite
{
    const Object* object = nullptr;
    RetainedContainer content;
    // The scene of the mask layer attached to content.scene, and the one this frame wants.
    tvg::Scene* mask = nullptr;
    tvg::Scene* wanted = nullptr;
    tvg::MaskMethod method = tvg::MaskMethod::None;
    tvg::MaskMethod wantedMethod = tvg::MaskMethod::None;
    // The blend of content.scene: the layer's own, or Composition when the layer holds a
    // blended layer and must draw offscreen.
    tvg::BlendMethod blend = tvg::BlendMethod::Normal;
    // The effects content.scene carries.
    float blur = 0.0f;
    Shadow shadow;
    uint8_t opacity = 255;
    bool placed = false;
    bool used = false;
    bool isolating = false;
    // Already pushed into its parent container this frame.
    bool pushed = false;
};

// The retained tvg::Scene of one DisplayList: [background, command paints and layers...,
// mounts...]. Commands inside a layer are children of that layer's scene instead, and the
// wrappers of the scene's views go among the command paints by their layers.
struct RetainedNode
{
    RetainedContainer content;
    tvg::Shape* background = nullptr;
    float backgroundRect[4] = {0.0f, 0.0f, -1.0f, -1.0f};
    Color backgroundColor = {0, 0, 0, 0};
    bool backgroundFilled = false;

    Slot** slots = nullptr;
    uint32_t slotCnt = 0;
    uint32_t slotCap = 0;
    Slot** table = nullptr;
    uint32_t tableCap = 0;

    RetainedMount** mounts = nullptr;
    uint32_t mountCnt = 0;
    uint32_t mountCap = 0;
    uint32_t mountUsed = 0;

    RetainedComposite** composites = nullptr;
    uint32_t compositeCnt = 0;
    uint32_t compositeCap = 0;
    // The composite of each layer of the current DisplayList.
    RetainedComposite** layers = nullptr;
    uint32_t layerCap = 0;
    RetainedComposite** deadComposites = nullptr;
    uint32_t deadCompositeCnt = 0;
    uint32_t deadCompositeCap = 0;

    tvg::Paint** dead = nullptr;  // paints to detach and release this frame
    uint32_t deadCnt = 0;
    uint32_t deadCap = 0;
    RetainedMount** deadMounts = nullptr;
    uint32_t deadMountCnt = 0;
    uint32_t deadMountCap = 0;

    RankEntry* ranks = nullptr;
    uint32_t rankCap = 0;
    uint32_t* orderIndex = nullptr;
    uint32_t orderCap = 0;
    uint32_t* rankOf = nullptr;
    uint32_t rankOfCap = 0;

    ~RetainedNode();
};

static void _mountFree(RetainedMount* mount);

RetainedNode::~RetainedNode()
{
    if (content.scene) content.scene->remove();
    for (auto i = 0u; i < compositeCnt; i++)
        composites[i]->content.scene->remove();
    for (auto i = 0u; i < compositeCnt; i++) {
        composites[i]->content.scene->unref();
        delete composites[i];
    }
    for (auto i = 0u; i < slotCnt; i++) {
        if (slots[i]->paint) slots[i]->paint->unref();
        delete slots[i];
    }
    if (background) background->unref();
    for (auto i = 0u; i < mountCnt; i++)
        _mountFree(mounts[i]);
    if (content.scene) content.scene->unref();
    delete[] slots;
    delete[] table;
    delete[] mounts;
    delete[] composites;
    delete[] layers;
    delete[] deadComposites;
    delete[] dead;
    delete[] deadMounts;
    delete[] ranks;
    delete[] orderIndex;
    delete[] rankOf;
}

// The mount's wrapper must already be detached from its parent scene.
static void _mountFree(RetainedMount* mount)
{
    if (!mount) return;
    if (mount->wrapper) mount->wrapper->unref();
    delete mount->child;
    delete mount;
}

static RetainedNode* _nodeGen()
{
    auto node = new (std::nothrow) RetainedNode;
    if (!node) return nullptr;
    node->content.scene = tvg::Scene::gen();
    node->background = tvg::Shape::gen();
    if (node->content.scene) node->content.scene->ref();
    if (node->background) node->background->ref();
    if (!node->content.scene || !node->background) {
        delete node;
        return nullptr;
    }
    return node;
}

static bool _push(tvg::Paint**& values, uint32_t& count, uint32_t& capacity, tvg::Paint* paint)
{
    if (!grow(values, count, capacity, 1u, 16u)) return false;
    values[count++] = paint;
    return true;
}

static bool _buildTable(RetainedNode* node)
{
    auto required = 16u;
    while (required < node->slotCnt * 2u) required *= 2u;
    if (required != node->tableCap) {
        auto table = new (std::nothrow) Slot*[required];
        if (!table) return false;
        delete[] node->table;
        node->table = table;
        node->tableCap = required;
    }
    for (auto i = 0u; i < node->tableCap; i++)
        node->table[i] = nullptr;
    auto mask = node->tableCap - 1u;
    for (auto i = 0u; i < node->slotCnt; i++) {
        auto slot = node->slots[i];
        auto index = slot->key.hash() & mask;
        while (node->table[index]) index = (index + 1u) & mask;
        node->table[index] = slot;
    }
    return true;
}

static Slot* _find(RetainedNode* node, const SlotKey& key)
{
    auto mask = node->tableCap - 1u;
    auto index = key.hash() & mask;
    while (auto slot = node->table[index]) {
        if (slot->key == key) return slot;
        index = (index + 1u) & mask;
    }
    return nullptr;
}

// rankOf[i] = number of commands the same (object, counterpart) emitted before command i.
static bool _ranks(RetainedNode* node, const DisplayList& list)
{
    auto count = list.count;
    if (!count) return true;
    if (!grow(node->orderIndex, 0u, node->orderCap, count, 16u)
        || !grow(node->rankOf, 0u, node->rankOfCap, count, 16u)) {
        return false;
    }
    auto required = 16u;
    while (required < count * 2u) required *= 2u;
    if (required > node->rankCap) {
        auto ranks = new (std::nothrow) RankEntry[required];
        if (!ranks) return false;
        delete[] node->ranks;
        node->ranks = ranks;
        node->rankCap = required;
    }
    auto mask = required - 1u;
    for (auto i = 0u; i < required; i++)
        node->ranks[i] = {nullptr, nullptr, 0u};

    // Emission order: commands carry their append index in `order`.
    auto valid = true;
    for (auto i = 0u; i < count; i++)
        node->orderIndex[i] = UINT32_MAX;
    for (auto i = 0u; i < count && valid; i++) {
        auto order = list.commands[i].order;
        if (order >= count || node->orderIndex[order] != UINT32_MAX) valid = false;
        else node->orderIndex[order] = i;
    }
    if (!valid) {
        for (auto i = 0u; i < count; i++)
            node->orderIndex[i] = i;
    }
    for (auto j = 0u; j < count; j++) {
        auto i = node->orderIndex[j];
        auto& command = list.commands[i];
        auto index = _hash(command.object, command.counterpart, 0u) & mask;
        while (true) {
            auto& entry = node->ranks[index];
            if (entry.count && entry.object == command.object && entry.counterpart == command.counterpart) {
                node->rankOf[i] = entry.count++;
                break;
            }
            if (!entry.count) {
                entry = {command.object, command.counterpart, 1u};
                node->rankOf[i] = 0u;
                break;
            }
            index = (index + 1u) & mask;
        }
    }
    return true;
}

static void _begin(RetainedNode* node)
{
    node->content.desiredCnt = 0;
    node->deadCnt = 0;
    node->deadMountCnt = 0;
    node->deadCompositeCnt = 0;
    node->mountUsed = 0;
    for (auto i = 0u; i < node->slotCnt; i++)
        node->slots[i]->used = false;
    for (auto i = 0u; i < node->compositeCnt; i++)
        node->composites[i]->used = false;
}

static Result _detach(RetainedNode* node, tvg::Paint* paint);

static_assert(static_cast<uint8_t>(BlendMode::Multiply) == static_cast<uint8_t>(tvg::BlendMethod::Multiply)
                  && static_cast<uint8_t>(BlendMode::Hue) == static_cast<uint8_t>(tvg::BlendMethod::Hue)
                  && static_cast<uint8_t>(BlendMode::Add) == static_cast<uint8_t>(tvg::BlendMethod::Add),
              "BlendMode follows the order of tvg::BlendMethod");

static tvg::MaskMethod _maskMethod(MaskMode mode)
{
    switch (mode) {
        case MaskMode::InverseAlpha: return tvg::MaskMethod::InvAlpha;
        case MaskMode::Luma: return tvg::MaskMethod::Luma;
        case MaskMode::InverseLuma: return tvg::MaskMethod::InvLuma;
        default: return tvg::MaskMethod::Alpha;
    }
}

// Attaches the scene of each mask layer to the scene of the layer it masks. ThorVG takes a
// mask only while it has no parent, so masks that change come off before any goes on.
static Result _masks(RetainedNode* node, const DisplayList& list)
{
    for (auto i = 0u; i < node->compositeCnt; i++)
        node->composites[i]->wanted = nullptr;
    for (auto i = 0u; i < list.layerCount; i++) {
        auto& layer = list.layers[i];
        if (!layer.mask) continue;
        node->layers[i]->wanted = node->layers[layer.mask - 1u]->content.scene;
        node->layers[i]->wantedMethod = _maskMethod(layer.maskMode);
    }
    for (auto i = 0u; i < node->compositeCnt; i++) {
        auto composite = node->composites[i];
        if (!composite->mask) continue;
        if (composite->mask == composite->wanted && composite->method == composite->wantedMethod) continue;
        if (!_success(composite->content.scene->mask(nullptr, tvg::MaskMethod::None))) return Result::Unknown;
        composite->mask = nullptr;
    }
    for (auto i = 0u; i < node->compositeCnt; i++) {
        auto composite = node->composites[i];
        if (!composite->wanted || composite->mask) continue;
        // A layer that drew in a container last frame becomes a mask.
        if (_detach(node, composite->wanted) != Result::Success) return Result::Unknown;
        if (!_success(composite->content.scene->mask(composite->wanted, composite->wantedMethod))) {
            return Result::Unknown;
        }
        composite->mask = composite->wanted;
        composite->method = composite->wantedMethod;
    }
    return Result::Success;
}

// Replaces the effects of a layer scene, which ThorVG applies in the order they were added.
static bool _effects(tvg::Scene* scene, const DisplayLayer& layer)
{
    // Three box passes, the closest ThorVG comes to a true Gaussian.
    static constexpr int quality = 100;
    if (!_success(scene->add(tvg::SceneEffect::Clear))) return false;
    if (layer.blur > 0.0f
        && !_success(scene->add(tvg::SceneEffect::GaussianBlur, static_cast<double>(layer.blur), 0, 0, quality))) {
        return false;
    }
    auto& shadow = layer.shadow;
    if (!shadow.color.a) return true;
    // ThorVG takes a distance and an angle clockwise from screen up; the offset is +y up.
    constexpr auto DEGREES = 180.0 / 3.14159265358979323846;
    auto distance = std::hypot(static_cast<double>(shadow.offset.x), static_cast<double>(shadow.offset.y));
    auto angle = 90.0 - std::atan2(static_cast<double>(shadow.offset.y), static_cast<double>(shadow.offset.x)) * DEGREES;
    if (angle < 0.0) angle += 360.0;
    return _success(scene->add(tvg::SceneEffect::DropShadow, static_cast<int>(shadow.color.r),
                               static_cast<int>(shadow.color.g), static_cast<int>(shadow.color.b),
                               static_cast<int>(shadow.color.a), angle, distance,
                               static_cast<double>(shadow.blur), quality));
}

// Finds or makes the composite of every layer in list and applies the layer's opacity.
static Result _composites(RetainedNode* node, const DisplayList& list)
{
    if (!list.layerCount) return Result::Success;
    if (!grow(node->layers, 0u, node->layerCap, list.layerCount, 16u)) return Result::OutOfMemory;
    for (auto i = 0u; i < list.layerCount; i++) {
        auto& layer = list.layers[i];
        RetainedComposite* composite = nullptr;
        for (auto j = 0u; j < node->compositeCnt && !composite; j++) {
            if (node->composites[j]->object == layer.object && !node->composites[j]->used) {
                composite = node->composites[j];
            }
        }
        if (!composite) {
            if (!grow(node->composites, node->compositeCnt, node->compositeCap, 1u, 16u)) {
                return Result::OutOfMemory;
            }
            composite = new (std::nothrow) RetainedComposite;
            if (!composite) return Result::OutOfMemory;
            composite->object = layer.object;
            composite->content.scene = tvg::Scene::gen();
            if (!composite->content.scene) {
                delete composite;
                return Result::OutOfMemory;
            }
            composite->content.scene->ref();
            node->composites[node->compositeCnt++] = composite;
        }
        composite->used = true;
        composite->pushed = false;
        composite->content.desiredCnt = 0;
        auto opacity = static_cast<uint8_t>(layer.opacity * 255.0f + 0.5f);
        if (!composite->placed || composite->opacity != opacity) {
            if (!_success(composite->content.scene->opacity(opacity))) return Result::Unknown;
            composite->opacity = opacity;
        }
        auto& shadow = layer.shadow;
        if (composite->blur != layer.blur || !_sameColor(composite->shadow.color, shadow.color)
            || composite->shadow.offset.x != shadow.offset.x || composite->shadow.offset.y != shadow.offset.y
            || composite->shadow.blur != shadow.blur) {
            if (!_effects(composite->content.scene, layer)) return Result::Unknown;
            composite->blur = layer.blur;
            composite->shadow = shadow;
        }
        composite->placed = true;
        composite->isolating = false;
        node->layers[i] = composite;
    }
    // A blended layer combines with what lies beneath it inside its enclosing layer, so that
    // layer draws offscreen; ThorVG otherwise draws an opaque scene straight onto the canvas.
    for (auto i = 0u; i < list.layerCount; i++) {
        auto parent = list.layers[i].parent;
        if (parent && list.layers[i].blend != BlendMode::Normal) node->layers[parent - 1u]->isolating = true;
    }
    for (auto i = 0u; i < list.layerCount; i++) {
        auto composite = node->layers[i];
        auto blend = list.layers[i].blend;
        auto method = blend != BlendMode::Normal ? static_cast<tvg::BlendMethod>(blend)
                      : composite->isolating     ? tvg::BlendMethod::Composition
                                                 : tvg::BlendMethod::Normal;
        if (composite->blend == method) continue;
        if (!_success(composite->content.scene->blend(method))) return Result::Unknown;
        composite->blend = method;
    }
    return _masks(node, list);
}

// Pushes the scene of layer index, and of the layers around it, into their containers the
// first time a command inside it comes up, which is the layer's place in sorted order.
static bool _open(RetainedNode* node, const DisplayList& list, uint32_t index)
{
    auto composite = node->layers[index - 1u];
    if (composite->pushed) return true;
    composite->pushed = true;
    // A mask draws only through the layer it masks.
    if (list.layers[index - 1u].masking) return true;
    auto parent = list.layers[index - 1u].parent;
    if (parent && !_open(node, list, parent)) return false;
    auto& container = parent ? node->layers[parent - 1u]->content : node->content;
    return _push(container.desired, container.desiredCnt, container.desiredCap, composite->content.scene);
}

// A view ready to join the paints of its scene: its wrapper, and the layer it lies above.
struct PlacedView
{
    tvg::Paint* wrapper = nullptr;
    int32_t layer = 0;
};

// The layer by which a command takes its place among the paints of its scene: its own, or that
// of the outermost layer it draws into. A command inside a mask has no place of its own.
static bool _topLayer(const DisplayList& list, const DrawCommand& command, int32_t& layer)
{
    layer = command.layer;
    auto index = command.composite;
    if (!index) return true;
    while (list.layers[index - 1u].parent) index = list.layers[index - 1u].parent;
    if (list.layers[index - 1u].masking) return false;
    layer = list.layers[index - 1u].layer;
    return true;
}

// Fills node with the paints of list: the background, over the frame of config or over area
// when given, then the commands in order, each view of views going in before the first paint
// on a layer above its own, or after all of them. views come sorted by layer.
static Result _local(RetainedNode* node, const Config& config, const DisplayList& list,
                     const PlacedView* views = nullptr, uint32_t viewCount = 0u,
                     const float* area = nullptr)
{
    auto background = node->background;
    float rect[4] = {0.0f, 0.0f, static_cast<float>(config.width), static_cast<float>(config.height)};
    if (area) std::memcpy(rect, area, sizeof(rect));
    if (!_same(node->backgroundRect, rect)) {
        if (!_success(background->reset()) || !_success(background->appendRect(rect[0], rect[1], rect[2], rect[3]))) {
            return Result::Unknown;
        }
        std::memcpy(node->backgroundRect, rect, sizeof(rect));
    }
    auto& color = config.background;
    if (!node->backgroundFilled || !_sameColor(node->backgroundColor, color)) {
        if (!_success(background->fill(color.r, color.g, color.b, color.a))) return Result::Unknown;
        node->backgroundColor = color;
        node->backgroundFilled = true;
    }
    if (!_push(node->content.desired, node->content.desiredCnt, node->content.desiredCap, background)) {
        return Result::OutOfMemory;
    }

    if (!_buildTable(node) || !_ranks(node, list)) return Result::OutOfMemory;
    auto result = _composites(node, list);
    if (result != Result::Success) return result;
    auto shown = 0u;
    for (auto i = 0u; i < list.count; i++) {
        auto& command = list.commands[i];
        SlotKey key = {command.object, command.counterpart, node->rankOf[i], command.type};
        auto slot = _find(node, key);
        if (slot && slot->used) slot = nullptr;
        if (slot) {
            auto replace = false;
            if (!_update(*slot, command, replace)) {
                if (!replace) return Result::Unknown;
                auto paint = _paint(command);
                if (!paint) return Result::NonSupport;
                paint->ref();
                if (!_push(node->dead, node->deadCnt, node->deadCap, slot->paint)) {
                    paint->unref();
                    return Result::OutOfMemory;
                }
                slot->paint = paint;
                if (!_capture(*slot, command)) return Result::OutOfMemory;
            }
        } else {
            if (!grow(node->slots, node->slotCnt, node->slotCap, 1u, 16u)) return Result::OutOfMemory;
            slot = new (std::nothrow) Slot;
            if (!slot) return Result::OutOfMemory;
            slot->key = key;
            slot->paint = _paint(command);
            if (!slot->paint) {
                delete slot;
                return Result::NonSupport;
            }
            slot->paint->ref();
            node->slots[node->slotCnt++] = slot;
            if (!_capture(*slot, command)) return Result::OutOfMemory;
        }
        slot->used = true;
        int32_t top;
        while (shown < viewCount && _topLayer(list, command, top) && views[shown].layer < top) {
            if (!_push(node->content.desired, node->content.desiredCnt, node->content.desiredCap,
                       views[shown++].wrapper)) {
                return Result::OutOfMemory;
            }
        }
        auto container = &node->content;
        if (command.composite) {
            if (!_open(node, list, command.composite)) return Result::OutOfMemory;
            container = &node->layers[command.composite - 1u]->content;
        }
        if (!_push(container->desired, container->desiredCnt, container->desiredCap, slot->paint)) {
            return Result::OutOfMemory;
        }
    }
    while (shown < viewCount) {
        if (!_push(node->content.desired, node->content.desiredCnt, node->content.desiredCap,
                   views[shown++].wrapper)) {
            return Result::OutOfMemory;
        }
    }
    return Result::Success;
}

static RetainedMount* _mount(RetainedNode* node, uint8_t kind, const Scene* first, uint64_t firstSerial,
                             const Scene* second, uint64_t secondSerial)
{
    auto index = node->mountUsed;
    if (index < node->mountCnt) {
        auto mount = node->mounts[index];
        if (mount->kind == kind && mount->first == first && mount->second == second
            && mount->firstSerial == firstSerial && mount->secondSerial == secondSerial) {
            node->mountUsed++;
            return mount;
        }
        // A different scene now occupies this mount: release the old subtree.
        if (!grow(node->deadMounts, node->deadMountCnt, node->deadMountCap, 1u, 16u)) return nullptr;
        if (mount->wrapper && !_push(node->dead, node->deadCnt, node->deadCap, mount->wrapper)) return nullptr;
        node->deadMounts[node->deadMountCnt++] = mount;
        node->mounts[index] = nullptr;
    } else {
        if (!grow(node->mounts, node->mountCnt, node->mountCap, 1u, 16u)) return nullptr;
        node->mounts[node->mountCnt++] = nullptr;
    }
    auto mount = new (std::nothrow) RetainedMount;
    if (!mount) return nullptr;
    mount->kind = kind;
    mount->first = first;
    mount->second = second;
    mount->firstSerial = firstSerial;
    mount->secondSerial = secondSerial;
    mount->child = _nodeGen();
    if (!mount->child) {
        delete mount;
        return nullptr;
    }
    node->mounts[index] = mount;
    node->mountUsed++;
    return mount;
}

static Result _wrap(RetainedMount* mount, const tvg::Matrix& matrix, const float (&rect)[4], uint8_t opacity);

// Places the scene of mount in viewport and clips it there, or where viewport and frame overlap
// when a frame holds a scene that moves through it.
static Result _place(RetainedNode* node, RetainedMount* mount, const Config& childConfig,
                     const Config& config, const Viewport& viewport, uint8_t opacity,
                     const Viewport* frame = nullptr)
{
    auto x = viewport.x * static_cast<float>(config.width);
    auto y = viewport.y * static_cast<float>(config.height);
    auto width = viewport.width * static_cast<float>(config.width);
    auto height = viewport.height * static_cast<float>(config.height);
    tvg::Matrix matrix = {
        width / static_cast<float>(childConfig.width), 0.0f, x,
        0.0f, height / static_cast<float>(childConfig.height), y,
        0.0f, 0.0f, 1.0f};
    float rect[4] = {x, y, width, height};
    if (frame) {
        auto left = std::fmax(x, frame->x * static_cast<float>(config.width));
        auto top = std::fmax(y, frame->y * static_cast<float>(config.height));
        auto right = std::fmin(x + width,
                               (frame->x + frame->width) * static_cast<float>(config.width));
        auto bottom = std::fmin(y + height,
                                (frame->y + frame->height) * static_cast<float>(config.height));
        rect[0] = left;
        rect[1] = top;
        rect[2] = std::fmax(right - left, 0.0f);
        rect[3] = std::fmax(bottom - top, 0.0f);
    }
    auto result = _wrap(mount, matrix, rect, opacity);
    if (result != Result::Success) return result;
    return _push(node->content.desired, node->content.desiredCnt, node->content.desiredCap, mount->wrapper)
         ? Result::Success
         : Result::OutOfMemory;
}

// Transforms the root of the scene of mount by matrix and puts it in its wrapper, clipped to rect
// and as opaque as opacity.
static Result _wrap(RetainedMount* mount, const tvg::Matrix& matrix, const float (&rect)[4], uint8_t opacity)
{
    auto child = mount->child->content.scene;
    if (!mount->placed || !_same(mount->matrix, matrix)) {
        if (!_success(child->transform(matrix))) return Result::Unknown;
        mount->matrix = matrix;
    }
    if (!mount->wrapper) {
        auto wrapper = tvg::Scene::gen();
        auto clipper = tvg::Shape::gen();
        if (!wrapper || !clipper) {
            if (wrapper) _release(wrapper);
            if (clipper) _release(clipper);
            return Result::OutOfMemory;
        }
        wrapper->ref();
        mount->wrapper = wrapper;
        if (!_success(wrapper->add(child))) {
            _release(clipper);
            return Result::Unknown;
        }
        if (!_success(clipper->appendRect(rect[0], rect[1], rect[2], rect[3]))
            || !_success(wrapper->clip(clipper))) {
            if (!clipper->parent() && wrapper->clip() != clipper) _release(clipper);
            return Result::Unknown;
        }
        mount->clipper = clipper;
        std::memcpy(mount->rect, rect, sizeof(rect));
    } else if (!_same(mount->rect, rect)) {
        if (!_success(mount->clipper->reset())
            || !_success(mount->clipper->appendRect(rect[0], rect[1], rect[2], rect[3]))) {
            return Result::Unknown;
        }
        std::memcpy(mount->rect, rect, sizeof(rect));
    }
    if (!mount->placed || mount->opacity != opacity) {
        if (!_success(mount->wrapper->opacity(opacity))) return Result::Unknown;
        mount->opacity = opacity;
    }
    mount->placed = true;
    return Result::Success;
}

static bool _contains(tvg::Paint* const* sorted, uint32_t count, tvg::Paint* paint)
{
    auto low = 0u;
    auto high = count;
    while (low < high) {
        auto mid = (low + high) / 2u;
        if (sorted[mid] == paint) return true;
        if (reinterpret_cast<uintptr_t>(sorted[mid]) < reinterpret_cast<uintptr_t>(paint)) low = mid + 1u;
        else high = mid;
    }
    return false;
}

static void _sortPaints(tvg::Paint** values, uint32_t count)
{
    // Insertion sort is enough: dead lists are short and usually empty.
    for (auto i = 1u; i < count; i++) {
        auto value = values[i];
        auto j = i;
        while (j > 0u && reinterpret_cast<uintptr_t>(values[j - 1u]) > reinterpret_cast<uintptr_t>(value)) {
            values[j] = values[j - 1u];
            j--;
        }
        values[j] = value;
    }
}

// Detaches paint from the container it is attached to, when it moves to another one.
static Result _detach(RetainedNode* node, tvg::Paint* paint)
{
    auto parent = paint->parent();
    if (!parent) return Result::Success;
    auto container = parent == node->content.scene ? &node->content : nullptr;
    for (auto i = 0u; i < node->compositeCnt && !container; i++) {
        if (parent == node->composites[i]->content.scene) container = &node->composites[i]->content;
    }
    if (!container || !_success(container->scene->remove(paint))) return Result::Unknown;
    auto kept = 0u;
    for (auto i = 0u; i < container->attachedCnt; i++) {
        if (container->attached[i] != paint) container->attached[kept++] = container->attached[i];
    }
    container->attachedCnt = kept;
    return Result::Success;
}

// Reorders the container's children to match the desired z-order with minimal edits.
static Result _reconcile(RetainedNode* node, RetainedContainer& container)
{
    auto scene = container.scene;
    auto& count = container.attachedCnt;
    auto attached = container.attached;
    auto desired = container.desired;
    auto total = container.desiredCnt;
    if (count == total) {
        auto same = true;
        for (auto i = 0u; i < total && same; i++)
            same = attached[i] == desired[i];
        if (same) return Result::Success;
    }
    auto prefix = 0u;
    while (prefix < count && prefix < total && attached[prefix] == desired[prefix]) prefix++;
    auto suffix = 0u;
    while (suffix < count - prefix && suffix < total - prefix
           && attached[count - 1u - suffix] == desired[total - 1u - suffix]) {
        suffix++;
    }
    // Paints that leave stay attached until the end, so the mirror may briefly hold both.
    if (!grow(container.attached, count, container.attachedCap, total, 16u)) return Result::OutOfMemory;
    attached = container.attached;

    if (total - prefix - suffix > 64u) {
        // Heavy reorder (e.g. depth sorting): detach the children (they stay alive, held by
        // their slots) and attach them again in order, all in linear time.
        if (!_success(scene->remove())) return Result::Unknown;
        count = 0;
        for (auto i = 0u; i < total; i++) {
            if (_detach(node, desired[i]) != Result::Success) return Result::Unknown;
            if (!_success(scene->add(desired[i]))) return Result::Unknown;
            attached[count++] = desired[i];
        }
        return Result::Success;
    }
    for (auto k = prefix; k < total; k++) {
        auto paint = desired[k];
        if (k < count && attached[k] == paint) continue;
        auto found = false;
        for (auto j = k + 1u; j < count; j++) {
            if (attached[j] != paint) continue;
            if (!_success(scene->remove(paint))) return Result::Unknown;
            for (auto m = j; m + 1u < count; m++)
                attached[m] = attached[m + 1u];
            count--;
            found = true;
            break;
        }
        // A paint attached to another layer moves here.
        if (!found && _detach(node, paint) != Result::Success) return Result::Unknown;
        auto at = k < count ? attached[k] : nullptr;
        if (!_success(scene->add(paint, at))) return Result::Unknown;
        for (auto m = count; m > k; m--)
            attached[m] = attached[m - 1u];
        attached[k] = paint;
        count++;
    }
    // Children no longer asked for here, such as a layer without drawings this frame.
    while (count > total) {
        if (!_success(scene->remove(attached[count - 1u]))) return Result::Unknown;
        count--;
    }
    return Result::Success;
}

// Detaches the paints of the sorted dead list from the container.
static void _prune(RetainedContainer& container, tvg::Paint* const* dead, uint32_t deadCnt)
{
    auto remain = 0u;
    for (auto i = 0u; i < container.attachedCnt; i++) {
        auto paint = container.attached[i];
        if (_contains(dead, deadCnt, paint)) {
            container.scene->remove(paint);
        } else {
            container.attached[remain++] = paint;
        }
    }
    container.attachedCnt = remain;
}

static Result _end(RetainedNode* node)
{
    // Slots that no command claimed this frame.
    auto kept = 0u;
    for (auto i = 0u; i < node->slotCnt; i++) {
        auto slot = node->slots[i];
        if (slot->used) {
            node->slots[kept++] = slot;
            continue;
        }
        if (!_push(node->dead, node->deadCnt, node->deadCap, slot->paint)) return Result::OutOfMemory;
        slot->paint = nullptr;
        delete slot;
    }
    node->slotCnt = kept;
    // Mounts no longer present (e.g. a sequence before it begins).
    for (auto i = node->mountUsed; i < node->mountCnt; i++) {
        auto mount = node->mounts[i];
        if (!grow(node->deadMounts, node->deadMountCnt, node->deadMountCap, 1u, 16u)) return Result::OutOfMemory;
        if (mount->wrapper && !_push(node->dead, node->deadCnt, node->deadCap, mount->wrapper)) {
            return Result::OutOfMemory;
        }
        node->deadMounts[node->deadMountCnt++] = mount;
    }
    node->mountCnt = node->mountUsed;
    // Layers no longer present: their children leave first, so the ones still drawn can join
    // another container, and the layer scene goes with the dead paints.
    kept = 0u;
    for (auto i = 0u; i < node->compositeCnt; i++) {
        auto composite = node->composites[i];
        if (composite->used) {
            node->composites[kept++] = composite;
            continue;
        }
        if (!grow(node->deadComposites, node->deadCompositeCnt, node->deadCompositeCap, 1u, 16u)
            || !_push(node->dead, node->deadCnt, node->deadCap, composite->content.scene)) {
            return Result::OutOfMemory;
        }
        composite->content.scene->remove();
        composite->content.attachedCnt = 0;
        node->deadComposites[node->deadCompositeCnt++] = composite;
    }
    node->compositeCnt = kept;

    if (node->deadCnt) {
        _sortPaints(node->dead, node->deadCnt);
        _prune(node->content, node->dead, node->deadCnt);
        for (auto i = 0u; i < node->compositeCnt; i++)
            _prune(node->composites[i]->content, node->dead, node->deadCnt);
    }
    // Release detached paints first: a dead wrapper must drop its child root before the
    // child node releases that root.
    for (auto i = 0u; i < node->deadCnt; i++)
        node->dead[i]->unref();
    for (auto i = 0u; i < node->deadMountCnt; i++) {
        auto mount = node->deadMounts[i];
        mount->wrapper = nullptr;
        _mountFree(mount);
    }
    for (auto i = 0u; i < node->deadCompositeCnt; i++)
        delete node->deadComposites[i];
    node->deadCnt = 0;
    node->deadMountCnt = 0;
    node->deadCompositeCnt = 0;
    auto result = _reconcile(node, node->content);
    for (auto i = 0u; result == Result::Success && i < node->compositeCnt; i++)
        result = _reconcile(node, node->composites[i]->content);
    return result;
}

// The views a scene holds at most, as Scene::view() allows.
static constexpr uint32_t MAX_SCENE_VIEWS = 64;

struct RendererBuilder
{
    static uint64_t serial(const Scene* scene)
    {
        return scene->pImpl->serial;
    }

    static Result scene(RetainedNode* node, const Scene* scene, float time, uint32_t& commands)
    {
        DisplayList list;
        auto result = scene->pImpl->build(time, list);
        if (result != Result::Success) return result;
        if (list.count > DISPLAY_COMMAND_LIMIT - commands) return Result::InsufficientCondition;
        commands += list.count;
        auto& config = scene->pImpl->cfg;
        _begin(node);
        PlacedView placed[MAX_SCENE_VIEWS];
        auto placedCnt = 0u;
        result = views(node, scene, time, config, 255u, commands, placed, placedCnt);
        if (result == Result::Success) result = _local(node, config, list, placed, placedCnt);
        if (result == Result::Success) result = viewports(node, scene, time, config, 255u, commands);
        if (result == Result::Success) result = sequence(node, scene, time, config, commands);
        if (result == Result::Success) result = _end(node);
        return result;
    }

    // Draws the views of scene at time into their own nodes, in the space of config, and lists
    // the wrappers of those shown in placed, sorted by layer, for _local to push. Every view keeps
    // its mount, shown or not, so the mounts after it keep their places.
    static Result views(RetainedNode* node, const Scene* scene, float time, const Config& config,
                        uint8_t opacity, uint32_t& commands, PlacedView* placed, uint32_t& placedCnt)
    {
        auto impl = scene->pImpl;
        placedCnt = 0u;
        if (impl->viewCnt > MAX_SCENE_VIEWS) return Result::Unknown;
        auto& own = impl->cfg;
        auto spaceX = static_cast<float>(config.width) / static_cast<float>(own.width);
        auto spaceY = static_cast<float>(config.height) / static_cast<float>(own.height);
        for (auto i = 0u; i < impl->viewCnt; i++) {
            auto mount = _mount(node, 5u, scene, impl->serial, nullptr, i);
            if (!mount) return Result::OutOfMemory;
            ViewFrame frame;
            auto result = impl->frame(i, time, frame);
            if (result != Result::Success) return result;
            auto shown = static_cast<uint8_t>(frame.opacity * static_cast<float>(opacity) + 0.5f);
            if (!frame.shown || !shown) continue;
            DisplayList list;
            result = impl->build(time, list, true, &frame);
            if (result != Result::Success) return result;
            if (list.count > DISPLAY_COMMAND_LIMIT - commands) return Result::InsufficientCondition;
            commands += list.count;
            // The rectangle in the view's own pixels, which the background fills.
            float area[4] = {(frame.rect[0] - frame.offset.x) / frame.scale,
                             (frame.rect[1] - frame.offset.y) / frame.scale,
                             frame.rect[2] / frame.scale, frame.rect[3] / frame.scale};
            auto childConfig = own;
            childConfig.background = frame.background;
            _begin(mount->child);
            result = _local(mount->child, childConfig, list, nullptr, 0u, area);
            if (result == Result::Success) result = _end(mount->child);
            if (result != Result::Success) return result;
            tvg::Matrix matrix = {
                spaceX * frame.scale, 0.0f, spaceX * frame.offset.x,
                0.0f, spaceY * frame.scale, spaceY * frame.offset.y,
                0.0f, 0.0f, 1.0f};
            float rect[4] = {spaceX * frame.rect[0], spaceY * frame.rect[1], spaceX * frame.rect[2],
                             spaceY * frame.rect[3]};
            result = _wrap(mount, matrix, rect, shown);
            if (result != Result::Success) return result;
            auto at = placedCnt++;
            while (at > 0u && placed[at - 1u].layer > frame.layer) {
                placed[at] = placed[at - 1u];
                at--;
            }
            placed[at] = {mount->wrapper, frame.layer};
        }
        return Result::Success;
    }

    // Draws the views of scene above everything drawn so far, for a scene whose own paints come
    // from a list prepared for a transition.
    static Result viewsAbove(RetainedNode* node, const Scene* scene, float time, const Config& config,
                             uint8_t opacity, uint32_t& commands)
    {
        PlacedView placed[MAX_SCENE_VIEWS];
        auto placedCnt = 0u;
        auto result = views(node, scene, time, config, opacity, commands, placed, placedCnt);
        for (auto i = 0u; result == Result::Success && i < placedCnt; i++) {
            if (!_push(node->content.desired, node->content.desiredCnt, node->content.desiredCap, placed[i].wrapper)) {
                result = Result::OutOfMemory;
            }
        }
        return result;
    }

    static Result viewports(RetainedNode* node, const Scene* scene, float time,
                            const Config& config, uint8_t opacity, uint32_t& commands)
    {
        for (auto i = 0u; i < scene->pImpl->viewportCnt; i++) {
            auto& entry = scene->pImpl->viewports[i];
            auto mount = _mount(node, 1u, entry.scene, entry.scene->pImpl->serial, nullptr, 0u);
            if (!mount) return Result::OutOfMemory;
            auto result = RendererBuilder::scene(mount->child, entry.scene, time, commands);
            if (result != Result::Success) return result;
            result = _place(node, mount, entry.scene->pImpl->cfg, config, entry.viewport, opacity);
            if (result != Result::Success) return result;
        }
        return Result::Success;
    }

    static Result transition(RetainedNode* node, const Scene* from, const Scene* to,
                             const SceneTransition& transition, float progress,
                             float fromTime, float toTime, Config& config, uint32_t& commands)
    {
        DisplayList first;
        DisplayList second;
        DisplayList blended;
        auto result = from->pImpl->build(fromTime, first, false);
        if (result != Result::Success) return result;
        result = to->pImpl->build(toTime, second, false);
        if (result != Result::Success) return result;
        auto& fromConfig = from->pImpl->cfg;
        auto& toConfig = to->pImpl->cfg;
        result = blended.blend(first, second, transition.matches,
                               from->pImpl->objectCnt, to->pImpl->objectCnt,
                               {static_cast<float>(fromConfig.width), static_cast<float>(fromConfig.height)},
                               {static_cast<float>(toConfig.width), static_cast<float>(toConfig.height)},
                               progress);
        if (result != Result::Success) return result;
        if (blended.count > DISPLAY_COMMAND_LIMIT - commands) return Result::InsufficientCondition;
        commands += blended.count;
        config = fromConfig;
        auto bounded = progress;
        if (bounded < 0.0f) bounded = 0.0f;
        if (bounded > 1.0f) bounded = 1.0f;
        config.background = _mix(fromConfig.background, toConfig.background, bounded);
        _begin(node);
        result = _local(node, config, blended);
        if (result != Result::Success) return result;
        auto fromOpacity = static_cast<uint8_t>((1.0f - bounded) * 255.0f + 0.5f);
        auto toOpacity = static_cast<uint8_t>(bounded * 255.0f + 0.5f);
        result = viewsAbove(node, from, fromTime, config, fromOpacity, commands);
        if (result == Result::Success) result = viewsAbove(node, to, toTime, config, toOpacity, commands);
        if (result == Result::Success) result = viewports(node, from, fromTime, config, fromOpacity, commands);
        if (result == Result::Success) result = viewports(node, to, toTime, config, toOpacity, commands);
        if (result == Result::Success) result = sequence(node, from, fromTime, config, commands, fromOpacity);
        if (result == Result::Success) result = sequence(node, to, toTime, config, commands, toOpacity);
        if (result == Result::Success) result = _end(node);
        return result;
    }

    static Result sequence(RetainedNode* node, const Scene* scene, float time,
                           const Config& config, uint32_t& commands, uint8_t opacity = 255u)
    {
        auto sequence = scene->pImpl->sequence;
        if (!sequence || time < sequence->begin) return Result::Success;
        const Scene* stageScene = nullptr;
        const Scene* nextScene = nullptr;
        const SceneTransition* clip = nullptr;
        auto progress = 0.0f;
        auto index = 0u;
        for (auto i = 0u; i < sequence->count; i++) {
            index = i;
            auto& stage = sequence->stages[i];
            if (time < stage.end || i + 1u >= sequence->count) {
                stageScene = stage.scene;
                break;
            }
            auto& transition = sequence->transitions[i];
            if (time >= transition.end) continue;
            progress = (time - transition.begin) / (transition.end - transition.begin);
            if (progress <= 0.0f) {
                stageScene = stage.scene;
            } else {
                stageScene = stage.scene;
                nextScene = sequence->stages[i + 1u].scene;
                clip = &transition;
            }
            break;
        }
        if (!stageScene) return Result::Unknown;
        auto fromTime = sequence->time(index, time);
        auto toTime = nextScene ? sequence->time(index + 1u, time) : 0.0f;
        if (clip && clip->kind != TransitionKind::Match) {
            return styled(node, *sequence, stageScene, nextScene, *clip,
                          ease(clip->curve, progress), fromTime, toTime, config, commands, opacity);
        }
        Config childConfig;
        RetainedMount* mount = nullptr;
        auto result = Result::Success;
        if (clip) {
            mount = _mount(node, 3u, stageScene, stageScene->pImpl->serial, nextScene,
                           nextScene->pImpl->serial);
            if (!mount) return Result::OutOfMemory;
            result = transition(mount->child, stageScene, nextScene, *clip,
                                ease(clip->curve, progress), fromTime, toTime, childConfig, commands);
        } else {
            mount = _mount(node, 2u, stageScene, stageScene->pImpl->serial, nullptr, 0u);
            if (!mount) return Result::OutOfMemory;
            childConfig = stageScene->pImpl->cfg;
            result = RendererBuilder::scene(mount->child, stageScene, fromTime, commands);
        }
        if (result != Result::Success) return result;
        return _place(node, mount, childConfig, config, sequence->viewport, opacity);
    }

    // A scene of a transition at its local time, drawn from a list prepared in the space of config.
    static Result stage(RetainedNode* node, const Scene* scene, const DisplayList& list,
                        const Config& config, float time, uint32_t& commands)
    {
        if (list.count > DISPLAY_COMMAND_LIMIT - commands) return Result::InsufficientCondition;
        commands += list.count;
        _begin(node);
        auto result = _local(node, config, list);
        if (result == Result::Success) result = viewsAbove(node, scene, time, config, 255u, commands);
        if (result == Result::Success) result = viewports(node, scene, time, config, 255u, commands);
        if (result == Result::Success) result = sequence(node, scene, time, config, commands);
        if (result == Result::Success) result = _end(node);
        return result;
    }

    // A transition of a style other than Match: each scene draws whole, at its local time, where and
    // as opaque as the frame of the style says, the outgoing one first unless it lies above.
    // A style that also matches draws each scene without the objects that pair, and the pairs
    // on their way above both in the viewport of the sequence.
    static Result styled(RetainedNode* node, const SceneSequence& sequence, const Scene* from,
                         const Scene* to, const SceneTransition& transition, float progress,
                         float fromTime, float toTime, const Config& config,
                         uint32_t& commands, uint8_t opacity)
    {
        TransitionFrame frame;
        transition.frame(sequence.viewport, progress, from->pImpl->cfg.background.a,
                         to->pImpl->cfg.background.a, frame);
        DisplayList parts[3];
        Config spaces[3];
        if (transition.match) {
            auto result = Scene::Impl::matched(from, to, transition, progress, fromTime, toTime,
                                               parts, spaces);
            if (result != Result::Success) return result;
        }
        auto stage = [&](const Scene* scene, uint32_t part, const Viewport& viewport, float share) {
            auto mount = _mount(node, 2u, scene, scene->pImpl->serial, nullptr, 0u);
            if (!mount) return Result::OutOfMemory;
            auto time = part ? toTime : fromTime;
            auto result = transition.match
                        ? RendererBuilder::stage(mount->child, scene, parts[part], spaces[part], time, commands)
                        : RendererBuilder::scene(mount->child, scene, time, commands);
            if (result != Result::Success) return result;
            auto shown = static_cast<uint8_t>(share * static_cast<float>(opacity) + 0.5f);
            return _place(node, mount, transition.match ? spaces[part] : scene->pImpl->cfg,
                          config, viewport, shown, &sequence.viewport);
        };
        auto result = frame.outgoingAbove ? stage(to, 1u, frame.incoming, frame.incomingOpacity)
                                          : stage(from, 0u, frame.outgoing, frame.outgoingOpacity);
        if (result != Result::Success) return result;
        result = frame.outgoingAbove ? stage(from, 0u, frame.outgoing, frame.outgoingOpacity)
                                     : stage(to, 1u, frame.incoming, frame.incomingOpacity);
        if (result != Result::Success || !transition.match) return result;
        if (parts[2].count > DISPLAY_COMMAND_LIMIT - commands) return Result::InsufficientCondition;
        commands += parts[2].count;
        auto mount = _mount(node, 4u, from, from->pImpl->serial, to, to->pImpl->serial);
        if (!mount) return Result::OutOfMemory;
        _begin(mount->child);
        result = _local(mount->child, spaces[2], parts[2]);
        if (result == Result::Success) result = _end(mount->child);
        if (result != Result::Success) return result;
        return _place(node, mount, spaces[2], config, sequence.viewport, opacity);
    }
};

struct RetainedTree
{
    tvg::Canvas* canvas = nullptr;
    RetainedNode* root = nullptr;
    const Scene* scene = nullptr;
    uint64_t serial = 0;
    float scale = 1.0f;

    ~RetainedTree()
    {
        reset();
    }

    void reset()
    {
        if (root) {
            if (canvas) canvas->remove(root->content.scene);
            delete root;
            root = nullptr;
        }
        canvas = nullptr;
        scene = nullptr;
        serial = 0;
        scale = 1.0f;
    }

    Result sync(tvg::Canvas* target, const Scene* source, float time, float ratio)
    {
        // Paints belong to one canvas and reference the scene's objects and picture bytes:
        // a different canvas or scene starts a fresh tree.
        if (target != canvas || source != scene || RendererBuilder::serial(source) != serial) reset();
        if (!root) {
            root = _nodeGen();
            if (!root) return Result::OutOfMemory;
            if (!_success(target->add(root->content.scene))) {
                delete root;
                root = nullptr;
                return Result::Unknown;
            }
            canvas = target;
            scene = source;
            serial = RendererBuilder::serial(source);
        }
        if (!_same(scale, ratio)) {
            tvg::Matrix matrix = {
                ratio, 0.0f, 0.0f,
                0.0f, ratio, 0.0f,
                0.0f, 0.0f, 1.0f};
            if (!_success(root->content.scene->transform(matrix))) {
                reset();
                return Result::Unknown;
            }
            scale = ratio;
        }
        auto commands = 0u;
        auto result = RendererBuilder::scene(root, source, time, commands);
        // A partially updated tree is not trusted: rebuild it on the next frame.
        if (result != Result::Success) reset();
        return result;
    }
};

// ThorVG counts its users the same way, unguarded, and drops every font when the last one
// terminates it; the default font goes in again for the next first user. It is registered
// under its name before any other font, so it is also the one any loaded font finds.
static uint32_t _users = 0u;

static bool _init()
{
    if (!_success(tvg::Initializer::init(0))) return false;
    if (_users++ == 0u) {
        tvg::Text::load(DEFAULT_FONT_NAME, reinterpret_cast<const char*>(DEFAULT_FONT), sizeof(DEFAULT_FONT), "ttf", false);
    }
    return true;
}

static void _term()
{
    if (_users > 0u) _users--;
    tvg::Initializer::term();
}

namespace renderer
{

const char* TvgDiagnostic::text() const noexcept
{
    return message[0] ? message : nullptr;
}

TvgDiagnosis::TvgDiagnosis(TvgDiagnostic& diagnostic) noexcept : diagnostic(diagnostic)
{
    diagnostic.message[0] = '\0';
    _reason[0] = '\0';
}

TvgDiagnosis::~TvgDiagnosis()
{
    if (_reason[0]) std::snprintf(diagnostic.message, sizeof(diagnostic.message), "%s", _reason);
}

Result tvgFont(TvgDiagnostic& diagnostic, const char* path) noexcept
{
    diagnostic.message[0] = '\0';
    if (!path) return Result::InvalidArguments;
    if (_success(tvg::Text::load(path))) return Result::Success;
    std::snprintf(diagnostic.message, sizeof(diagnostic.message),
                  "could not load the font file '%s'; it is missing, unreadable or not a .ttf, "
                  ".ttc, .otf or .otc font", path);
    return Result::NonSupport;
}

Result tvgFont(TvgDiagnostic& diagnostic, const char* name, const void* data, uint32_t size,
               const char* mime) noexcept
{
    diagnostic.message[0] = '\0';
    if (!name || !name[0] || !data || !size || !mime || !mime[0]) return Result::InvalidArguments;
    if (_success(tvg::Text::load(name, static_cast<const char*>(data), size, mime, true))) {
        return Result::Success;
    }
    std::snprintf(diagnostic.message, sizeof(diagnostic.message),
                  "could not load the font '%s'; its data must be a TrueType or OpenType font "
                  "with the type ttf or otf", name);
    return Result::NonSupport;
}

bool tvgSuccess(tvg::Result result) noexcept
{
    return _success(result);
}

void tvgRelease(tvg::Paint* paint) noexcept
{
    _release(paint);
}

bool tvgAdd(tvg::Scene* scene, tvg::Paint* paint) noexcept
{
    return _add(scene, paint);
}

tvg::Paint* tvgPaint(const DrawCommand& command) noexcept
{
    return _paint(command);
}

bool tvgVisible(const DrawCommand& command) noexcept
{
    return _visible(command);
}

RetainedTree* tvgRetainedGen() noexcept
{
    return new (std::nothrow) RetainedTree;
}

void tvgRetainedFree(RetainedTree* tree) noexcept
{
    delete tree;
}

void tvgRetainedReset(RetainedTree* tree) noexcept
{
    if (tree) tree->reset();
}

Result tvgRetainedSync(RetainedTree* tree, tvg::Canvas* canvas, const Scene* scene,
                       float time, float scale) noexcept
{
    if (!tree || !canvas) return Result::InvalidArguments;
    return tree->sync(canvas, scene, time, scale);
}

// Bytes in the UTF-8 sequence that lead starts; a stray byte counts as one character.
static uint32_t _sequence(uint8_t lead)
{
    if (lead >= 0xf0u && lead < 0xf8u) return 4u;
    if (lead >= 0xe0u) return lead < 0xf0u ? 3u : 1u;
    if (lead >= 0xc0u) return 2u;
    return 1u;
}

// Whether a character holds one whole UTF-8 sequence of length bytes with no overlong form,
// surrogate, or codepoint past U+10FFFF.
static bool _wellFormed(const char* character, uint32_t length)
{
    auto lead = static_cast<uint8_t>(character[0]);
    if (length != _sequence(lead)) return false;
    if (length == 1u) return lead < 0x80u;
    if (lead < 0xc2u || lead > 0xf4u) return false;
    for (auto i = 1u; i < length; i++) {
        if ((static_cast<uint8_t>(character[i]) & 0xc0u) != 0x80u) return false;
    }
    auto second = static_cast<uint8_t>(character[1]);
    if (lead == 0xe0u) return second >= 0xa0u;
    if (lead == 0xedu) return second < 0xa0u;
    if (lead == 0xf0u) return second >= 0x90u;
    if (lead == 0xf4u) return second < 0x90u;
    return true;
}

// The codepoint of a well-formed UTF-8 sequence of length bytes.
static uint32_t _codepoint(const char* character, uint32_t length)
{
    static constexpr uint8_t LEAD_MASK[] = {0x7fu, 0x1fu, 0x0fu, 0x07u};
    auto code = static_cast<uint32_t>(static_cast<uint8_t>(character[0]) & LEAD_MASK[length - 1u]);
    for (auto i = 1u; i < length; i++) code = (code << 6u) | (static_cast<uint8_t>(character[i]) & 0x3fu);
    return code;
}

// Whether a character is drawn as nothing by design: a format character, a joiner, or a
// variation selector.
static bool _invisible(uint32_t code)
{
    return code == 0x00adu || code == 0x034fu || code == 0x061cu || code == 0x180eu
           || (code >= 0x200bu && code <= 0x200fu) || (code >= 0x202au && code <= 0x202eu)
           || (code >= 0x2060u && code <= 0x206fu) || (code >= 0xfe00u && code <= 0xfe0fu) || code == 0xfeffu
           || (code >= 0xe0000u && code <= 0xe01efu);
}

// Whether the font has no glyph for a character, whose lookup found glyph or not. The font
// loader keeps a codepoint it could not find as an empty glyph, so after the first miss the
// lookup succeeds with no advance, bearing or outline. Control and invisible characters do
// not count.
static bool _missing(bool found, const tvg::GlyphMetrics& glyph, const char* character, uint32_t length)
{
    if (_wellFormed(character, length)) {
        auto code = _codepoint(character, length);
        if (code < 0x20u || (code >= 0x7fu && code <= 0x9fu) || _invisible(code)) return false;
    }
    if (!found) return true;
    return glyph.advance == 0.0f && glyph.bearing == 0.0f && glyph.min.x == 0.0f && glyph.min.y == 0.0f
           && glyph.max.x == 0.0f && glyph.max.y == 0.0f;
}

// Adds up the glyph advances of every line the way the font loader lays them out, gathers the
// box their outlines cover, leaning by slant, and counts the characters the font has no glyph
// for, which the loader skips. A font that is not loaded is measured as the default one that
// draws it.
static Result _measure(tvg::Text* text, const char* font, float size, const char* value, float slant, bool tabular,
                       TextExtent& output)
{
    if (!_font(text, font, &output.fallback)) return Result::InsufficientCondition;
    tvg::TextMetrics metrics;
    if (!_success(text->size(size)) || !_success(text->metrics(metrics))) return Result::Unknown;
    auto cell = tabular ? _digitAdvance(text) : 0.0f;
    auto lines = 1u;
    auto width = 0.0f;
    auto line = 0.0f;
    auto inked = false;
    for (auto at = value; *at;) {
        if (*at == '\n') {
            if (line > width) width = line;
            line = 0.0f;
            lines++;
            at++;
            continue;
        }
        char character[5] = {};
        auto length = _sequence(static_cast<uint8_t>(*at));
        auto read = 0u;
        while (read < length && *at) character[read++] = *at++;
        tvg::GlyphMetrics glyph;
        auto found = _success(text->metrics(character, glyph));
        if (_missing(found, glyph, character, read)) {
            if (auto math = _mathGlyph(character)) {
                auto scale = size * (4.0f / 3.0f);
                glyph.advance = math->advance * scale;
                glyph.bearing = math->min.x * scale;
                glyph.min = {math->min.x * scale, math->min.y * scale};
                glyph.max = {math->max.x * scale, math->max.y * scale};
                found = true;
            }
        }
        auto fixed = tabular && _tabular(character);
        auto offset = fixed && found ? (cell - glyph.advance) * 0.5f : 0.0f;
        // Glyph boxes are y up from the pen on the baseline; each line sits one advance lower.
        if (found && (glyph.max.x > glyph.min.x || glyph.max.y > glyph.min.y)) {
            auto baseline = -metrics.advance * static_cast<float>(lines - 1u);
            auto left = line + offset + glyph.min.x;
            auto right = line + offset + glyph.max.x;
            auto top = baseline + glyph.max.y;
            auto bottom = baseline + glyph.min.y;
            output.left = inked ? std::fmin(output.left, left) : left;
            output.right = inked ? std::fmax(output.right, right) : right;
            output.top = inked ? std::fmax(output.top, top) : top;
            output.bottom = inked ? std::fmin(output.bottom, bottom) : bottom;
            inked = true;
        }
        if (found) line += fixed ? cell : glyph.advance;
        if (_missing(found, glyph, character, read)) {
            if (!output.missing && _wellFormed(character, read)) std::memcpy(output.firstMissing, character, sizeof(character));
            output.missing++;
        }
    }
    if (line > width) width = line;
    output.width = width;
    output.height = metrics.advance * static_cast<float>(lines) - metrics.linegap;
    output.ascent = metrics.ascent;
    output.descent = -metrics.descent;
    output.lines = lines;
    if (!inked || slant == 0.0f) return Result::Success;
    if (tabular) {
        DrawCommand command;
        command.text = value;
        command.font = font;
        command.size = size;
        command.slant = slant;
        command.tabular = true;
        auto paint = _mathText(command, text);
        if (!paint) return Result::OutOfMemory;
        float x, y, w, h;
        auto result = paint->bounds(&x, &y, &w, &h);
        _release(paint);
        if (!_success(result)) return Result::Unknown;
        output.left = x;
        output.right = x + w;
        return Result::Success;
    }
    if (_needsMath(text, value)) {
        auto a = slant * output.top;
        auto b = slant * output.bottom;
        output.left += std::fmin(a, b);
        output.right += std::fmax(a, b);
        return Result::Success;
    }
    // Leaning outlines reach as far as the shear carries their own points, which the glyph boxes
    // cannot tell: the top of a b leans past its bowl. Shear the set text about its first
    // baseline, as the renderer draws it, and read left and right from its outlines; heights stay.
    float x;
    float y;
    float w;
    float h;
    auto baseline = metrics.ascent;
    if (!_success(text->text(value)) || !_success(text->align(0.0f, 0.0f))
        || !_success(text->transform({1.0f, -slant, slant * baseline, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}))
        || !_success(text->bounds(&x, &y, &w, &h)) || !std::isfinite(x) || !std::isfinite(w)) {
        return Result::Unknown;
    }
    output.left = x;
    output.right = x + w;
    return Result::Success;
}

Result measureText(const char* font, float size, const char* value, float slant, bool tabular, TextExtent& output) noexcept
{
    output = {};
    if (!font || !value || !std::isfinite(size) || size <= 0.0f || !std::isfinite(slant)) {
        return Result::InvalidArguments;
    }
    if (!_init()) return Result::Unknown;
    auto text = tvg::Text::gen();
    auto result = text ? _measure(text, font, size, value, slant, tabular, output) : Result::OutOfMemory;
    if (text) _release(text);
    _term();
    if (result != Result::Success) output = {};
    return result;
}

bool tvgInit() noexcept
{
    return _init();
}

void tvgTerm() noexcept
{
    _term();
}

}  // namespace renderer

}  // namespace vis
