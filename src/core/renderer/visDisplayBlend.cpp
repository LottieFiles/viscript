// DisplayList::blend(): a display list part way between two others, with matched commands
// interpolated and the rest cross-faded.
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

#include "visDisplayColor.h"
#include "visDisplayList.h"
#include "visScene.h"

namespace vis
{

struct CommandSpan
{
    uint32_t begin = UINT32_MAX;
    uint32_t count = 0;
};

static bool _same(const char* first, const char* second)
{
    if (first == second) return true;
    return first && second && std::strcmp(first, second) == 0;
}

static bool _same(const PictureSource& first, const PictureSource& second)
{
    if (first.width != second.width || first.height != second.height) return false;
    if (first.encoded || second.encoded) {
        return first.encoded && second.encoded && first.encodedSize == second.encodedSize
               && _same(first.mime, second.mime)
               && (first.encoded == second.encoded
                   || std::memcmp(first.encoded, second.encoded, first.encodedSize) == 0);
    }
    return first.pixels == second.pixels;
}

static float _scale(const Vec2& from, const Vec2& to)
{
    return (to.x / from.x + to.y / from.y) * 0.5f;
}

static Vec2 _scale(const Vec2& point, const Vec2& from, const Vec2& to)
{
    return {point.x * to.x / from.x, point.y * to.y / from.y};
}

static Mat3 _scale(const Mat3& matrix, const Vec2& from, const Vec2& to)
{
    auto output = matrix;
    auto x = to.x / from.x;
    auto y = to.y / from.y;
    output.e[0] *= x;
    output.e[1] *= x;
    output.e[2] *= x;
    output.e[3] *= y;
    output.e[4] *= y;
    output.e[5] *= y;
    return output;
}

static Mat3 _mix(const Mat3& from, const Mat3& to, float progress)
{
    auto output = Mat3{};
    for (auto i = 0u; i < 9u; i++) output.e[i] = _mix(from.e[i], to.e[i], progress);
    return output;
}

static Style _scale(const Style& style, const Vec2& from, const Vec2& to, float opacity)
{
    auto output = style;
    auto factor = _scale(from, to);
    output.stroke = _opacity(output.stroke, opacity);
    output.fill = _opacity(output.fill, opacity);
    output.width *= factor;
    output.radius *= factor;
    for (auto i = 0u; i < output.dashCount; i++) output.dash[i] *= factor;
    output.dashOffset *= factor;
    auto& gradient = output.gradient;
    auto point = [&](Vec3& value) {
        auto pixel = _scale(Vec2{value.x, value.y}, from, to);
        value = {pixel.x, pixel.y, 0.0f};
    };
    point(gradient.from);
    point(gradient.to);
    point(gradient.center);
    point(gradient.focal);
    gradient.radius *= factor;
    gradient.focalRadius *= factor;
    return output;
}

static Style _mix(const Style& from, const Style& to, float progress)
{
    auto bounded = _clamp(progress);
    auto output = bounded < 0.5f ? from : to;
    output.stroke = _mix(from.stroke, to.stroke, progress);
    output.fill = _mix(from.fill, to.fill, progress);
    output.gradientEnd = _mix(from.gradientEnd, to.gradientEnd, progress);
    output.width = _mix(from.width, to.width, bounded);
    output.radius = _mix(from.radius, to.radius, bounded);
    output.gradient = mix(from.gradient, to.gradient, bounded);
    if (from.dashCount == to.dashCount) {
        output.dashCount = from.dashCount;
        output.dashOffset = _mix(from.dashOffset, to.dashOffset, bounded);
        for (auto i = 0u; i < output.dashCount; i++) {
            output.dash[i] = _mix(from.dash[i], to.dash[i], bounded);
        }
    }
    return output;
}

static Result _copy(DisplayList& list, const DrawCommand& command, const Vec2& from,
                    const Vec2& to, float opacity, LayoutVisualKind kind,
                    const Object* counterpart)
{
    list.object = command.object;
    list.counterpart = counterpart;
    list.visualKind = kind;
    list.visualOpacity = static_cast<uint8_t>(_clamp(opacity) * 255.0f + 0.5f);
    auto style = _scale(command.style, from, to, opacity);
    auto layer = command.layer;
    auto depth = command.depth;
    switch (command.type) {
        case CommandType::Path: {
            auto points = new (std::nothrow) Vec2[command.count];
            if (!points) return Result::OutOfMemory;
            for (auto i = 0u; i < command.count; i++)
                points[i] = _scale(command.points[i], from, to);
            auto result = list.path(points, command.count, command.closed, style, layer, depth,
                                    command.cap, command.ends, command.contours);
            delete[] points;
            return result;
        }
        case CommandType::Circle:
            return list.circle(_scale(command.center, from, to),
                               command.radius * _scale(from, to), style, layer, depth);
        case CommandType::Text: {
            auto transform = _scale(command.transform, from, to);
            return list.text(_scale(command.center, from, to), command.text, command.font,
                             command.transformed ? command.size : command.size * _scale(from, to),
                             command.align, style, layer, depth,
                             command.transformed ? &transform : nullptr, command.buffer != nullptr,
                             command.slant, command.tabular);
        }
        case CommandType::Number:
            return list.number(_scale(command.center, from, to), command.number,
                               command.size * _scale(from, to), command.align, style, layer, depth);
        case CommandType::Picture: {
            Vec2 corners[3];
            for (auto i = 0u; i < 3u; i++)
                corners[i] = _scale(command.points[i], from, to);
            return list.picture(corners, command.picture, command.filter, static_cast<uint8_t>(static_cast<float>(command.opacity)
                              * _clamp(opacity) + 0.5f), layer, depth);
        }
    }
    return Result::NonSupport;
}

static bool _compatible(const DrawCommand& from, const DrawCommand& to)
{
    if (from.type != to.type) return false;
    switch (from.type) {
        case CommandType::Path: return from.closed == to.closed && !from.ends && !to.ends;
        case CommandType::Text:
            return from.transformed == to.transformed && _same(from.text, to.text)
                   && _same(from.font, to.font);
        case CommandType::Picture: return _same(from.picture, to.picture);
        default: return true;
    }
}

static bool _sample(const DrawCommand& command, const Vec2& from, const Vec2& to,
                    Vec2* output, uint32_t count)
{
    auto at = [&](uint32_t index) {
        return _scale(command.points[index % command.count], from, to);
    };
    auto segments = command.closed ? command.count : command.count - 1u;
    auto total = 0.0;
    for (auto i = 0u; i < segments; i++) {
        auto a = at(i);
        auto b = at(i + 1u);
        total += static_cast<double>((b - a).length());
    }
    if (!std::isfinite(total)) return false;
    if (total <= 1.0e-7) {
        auto point = at(0u);
        for (auto i = 0u; i < count; i++) output[i] = point;
        return true;
    }
    auto segment = 0u;
    auto accumulated = 0.0;
    auto a = at(0u);
    auto b = at(1u);
    auto length = static_cast<double>((b - a).length());
    auto denominator = command.closed ? count : count - 1u;
    for (auto i = 0u; i < count; i++) {
        auto distance = total * static_cast<double>(i) / denominator;
        while (segment + 1u < segments && accumulated + length < distance) {
            accumulated += length;
            segment++;
            a = at(segment);
            b = at(segment + 1u);
            length = static_cast<double>((b - a).length());
        }
        auto progress = length > 1.0e-12 ? (distance - accumulated) / length : 0.0;
        output[i] = a + (b - a) * static_cast<float>(progress);
    }
    return true;
}

static uint32_t _target(uint32_t index, uint32_t offset, bool reverse, uint32_t count)
{
    return reverse ? (offset + count - index) % count : (offset + index) % count;
}

static void _align(const Vec2* from, const Vec2* to, uint32_t count,
                   uint32_t& offset, bool& reverse)
{
    auto best = std::numeric_limits<double>::infinity();
    offset = 0u;
    reverse = false;
    for (auto direction = 0u; direction < 2u; direction++) {
        for (auto candidate = 0u; candidate < count; candidate++) {
            auto distance = 0.0;
            for (auto i = 0u; i < count; i++) {
                auto delta = to[_target(i, candidate, direction != 0u, count)] - from[i];
                distance += static_cast<double>(delta.dot(delta));
            }
            if (distance >= best) continue;
            best = distance;
            offset = candidate;
            reverse = direction != 0u;
        }
    }
}

static Result _blendPath(DisplayList& list, const DrawCommand& from,
                         const DrawCommand& to, const Vec2& fromSize,
                         const Vec2& toSize, float progress)
{
    constexpr auto count = 64u;
    Vec2 first[count];
    Vec2 second[count];
    Vec2 points[count];
    if (!_sample(from, fromSize, fromSize, first, count)
        || !_sample(to, toSize, fromSize, second, count)) {
        return Result::NonSupport;
    }
    auto offset = 0u;
    auto reverse = false;
    if (from.closed) _align(first, second, count, offset, reverse);
    for (auto i = 0u; i < count; i++) {
        auto target = from.closed ? _target(i, offset, reverse, count) : i;
        points[i] = first[i] + (second[target] - first[i]) * progress;
    }
    auto firstStyle = _scale(from.style, fromSize, fromSize, 1.0f);
    auto secondStyle = _scale(to.style, toSize, fromSize, 1.0f);
    auto style = _mix(firstStyle, secondStyle, progress);
    auto bounded = _clamp(progress);
    auto layer = bounded < 0.5f ? from.layer : to.layer;
    auto cap = bounded < 0.5f ? from.cap : to.cap;
    return list.path(points, count, from.closed, style, layer,
                     _mix(from.depth, to.depth, progress), cap);
}

static Result _blend(DisplayList& list, const DrawCommand& from, const DrawCommand& to,
                     const Vec2& fromSize, const Vec2& toSize, float progress)
{
    auto bounded = _clamp(progress);
    list.object = from.object;
    list.counterpart = to.object;
    // A matched object keeps the layer it has in the outgoing scene.
    list.composite = from.composite;
    list.visualKind = LayoutVisualKind::Morph;
    list.visualOpacity = 255u;
    if (from.type == CommandType::Path) {
        return _blendPath(list, from, to, fromSize, toSize, progress);
    }
    auto firstStyle = _scale(from.style, fromSize, fromSize, 1.0f);
    auto secondStyle = _scale(to.style, toSize, fromSize, 1.0f);
    auto style = _mix(firstStyle, secondStyle, progress);
    auto first = _scale(from.center, fromSize, fromSize);
    auto second = _scale(to.center, toSize, fromSize);
    auto center = first + (second - first) * progress;
    auto factor = _scale(toSize, fromSize);
    auto layer = bounded < 0.5f ? from.layer : to.layer;
    auto depth = _mix(from.depth, to.depth, progress);
    switch (from.type) {
        case CommandType::Circle:
            return list.circle(center, _mix(from.radius, to.radius * factor, bounded),
                               style, layer, depth);
        case CommandType::Text: {
            auto firstTransform = from.transform;
            auto secondTransform = _scale(to.transform, toSize, fromSize);
            auto transform = _mix(firstTransform, secondTransform, progress);
            return list.text(center, from.text, from.font,
                             _mix(from.size, to.size * (from.transformed ? 1.0f : factor), bounded),
                             from.align + (to.align - from.align) * progress,
                             style, layer, depth, from.transformed ? &transform : nullptr,
                             from.buffer != nullptr, _mix(from.slant, to.slant, progress), from.tabular);
        }
        case CommandType::Number:
            return list.number(center, _mix(from.number, to.number, progress),
                               _mix(from.size, to.size * factor, bounded),
                               from.align + (to.align - from.align) * progress,
                               style, layer, depth);
        case CommandType::Picture: {
            Vec2 corners[3];
            for (auto i = 0u; i < 3u; i++) {
                auto first = _scale(from.points[i], fromSize, fromSize);
                auto second = _scale(to.points[i], toSize, fromSize);
                corners[i] = first + (second - first) * progress;
            }
            return list.picture(corners, from.picture, from.filter,
                                _mix(from.opacity, to.opacity, progress), layer, depth);
        }
        default: return Result::NonSupport;
    }
}

static bool _compatible(const DisplayList& from, const CommandSpan& fromSpan,
                        const DisplayList& to, const CommandSpan& toSpan)
{
    if (!fromSpan.count || fromSpan.count != toSpan.count) return false;
    for (auto i = 0u; i < fromSpan.count; i++) {
        if (!_compatible(from.commands[fromSpan.begin + i], to.commands[toSpan.begin + i])) {
            return false;
        }
    }
    return true;
}

static Result _copy(DisplayList& output, const DisplayList& input, const CommandSpan& span,
                    const Vec2& from, const Vec2& to, float opacity,
                    LayoutVisualKind kind, const Object* counterpart, uint32_t offset = 0u)
{
    for (auto i = 0u; i < span.count; i++) {
        auto& command = input.commands[span.begin + i];
        // Zero denotes the scene root and must survive layer remapping.
        output.composite = command.composite ? command.composite + offset : 0u;
        auto result = _copy(output, command, from, to, opacity, kind, counterpart);
        if (result != Result::Success) return result;
    }
    return Result::Success;
}

Result DisplayList::blend(const DisplayList& from, const DisplayList& to,
                          const uint32_t* matches, uint32_t fromObjects,
                          uint32_t toObjects, const Vec2& fromSize,
                          const Vec2& toSize, float progress, uint8_t parts) noexcept
{
    if ((fromObjects && !matches) || fromSize.x <= 0.0f || fromSize.y <= 0.0f
        || toSize.x <= 0.0f || toSize.y <= 0.0f || !std::isfinite(progress)) {
        return Result::InvalidArguments;
    }
    auto fromSpans = fromObjects ? new (std::nothrow) CommandSpan[fromObjects] : nullptr;
    auto toSpans = toObjects ? new (std::nothrow) CommandSpan[toObjects] : nullptr;
    auto morphed = toObjects ? new (std::nothrow) bool[toObjects] : nullptr;
    auto counterparts = toObjects ? new (std::nothrow) const Object*[toObjects] : nullptr;
    if ((fromObjects && !fromSpans)
        || (toObjects && (!toSpans || !morphed || !counterparts))) {
        delete[] fromSpans;
        delete[] toSpans;
        delete[] morphed;
        delete[] counterparts;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < toObjects; i++) {
        morphed[i] = false;
        counterparts[i] = nullptr;
    }
    auto spans = [](const DisplayList& list, CommandSpan* output, uint32_t size) {
        for (auto i = 0u; i < list.count; i++) {
            auto id = list.commands[i].object ? list.commands[i].object->id() : 0u;
            if (!id || id > size) continue;
            auto& span = output[id - 1u];
            if (!span.count) span.begin = i;
            span.count++;
        }
    };
    spans(from, fromSpans, fromObjects);
    spans(to, toSpans, toObjects);

    auto layers = [this](const DisplayList& input) {
        auto offset = layerCount;
        for (auto i = 0u; i < input.layerCount; i++) {
            auto layer = input.layers[i];
            if (layer.parent) layer.parent += offset;
            if (layer.mask) layer.mask += offset;
            if (!add(layer)) return false;
        }
        return true;
    };
    auto text = [](const DisplayList& list, const CommandSpan& span) {
        if (!span.count) return false;
        for (auto i = 0u; i < span.count; i++) {
            if (list.commands[span.begin + i].type != CommandType::Text) return false;
        }
        return true;
    };
    auto result = Result::Success;
    auto offset = from.layerCount;
    if (!layers(from) || !layers(to)) result = Result::OutOfMemory;
    auto bounded = _clamp(progress);
    for (auto i = 0u; result == Result::Success && i < fromObjects; i++) {
        auto target = matches[i];
        if (target < toObjects && _compatible(from, fromSpans[i], to, toSpans[target])) {
            for (auto j = 0u; (parts & BLEND_MATCHED) && j < fromSpans[i].count; j++) {
                result = _blend(*this, from.commands[fromSpans[i].begin + j],
                                to.commands[toSpans[target].begin + j],
                                fromSize, toSize, progress);
                if (result != Result::Success) break;
            }
            if (result == Result::Success) morphed[target] = true;
        } else if (target < toObjects && text(from, fromSpans[i])
                   && text(to, toSpans[target])) {
            if (!(parts & BLEND_MATCHED)) {
                morphed[target] = true;
                continue;
            }
            auto outgoing = _clamp(1.0f - bounded * 2.0f);
            auto incoming = _clamp(bounded * 2.0f - 1.0f);
            auto fromObject = fromSpans[i].count
                            ? from.commands[fromSpans[i].begin].object : nullptr;
            auto toObject = toSpans[target].count
                          ? to.commands[toSpans[target].begin].object : nullptr;
            result = _copy(*this, from, fromSpans[i], fromSize, fromSize, outgoing,
                           LayoutVisualKind::FadeOut, toObject);
            if (result == Result::Success) {
                result = _copy(*this, to, toSpans[target], toSize, fromSize, incoming,
                               LayoutVisualKind::FadeIn, fromObject, offset);
            }
            if (result == Result::Success) morphed[target] = true;
        } else {
            auto counterpart = target < toObjects && toSpans[target].count
                             ? to.commands[toSpans[target].begin].object : nullptr;
            if (parts & BLEND_FROM) {
                result = _copy(*this, from, fromSpans[i], fromSize, fromSize,
                               1.0f - bounded, LayoutVisualKind::FadeOut, counterpart);
            }
            if (target < toObjects) {
                counterparts[target] = fromSpans[i].count
                                     ? from.commands[fromSpans[i].begin].object : nullptr;
            }
        }
    }
    for (auto i = 0u; result == Result::Success && (parts & BLEND_TO) && i < toObjects; i++) {
        if (morphed[i]) continue;
        result = _copy(*this, to, toSpans[i], toSize, fromSize, bounded,
                       LayoutVisualKind::FadeIn, counterparts[i], offset);
    }
    delete[] fromSpans;
    delete[] toSpans;
    delete[] morphed;
    delete[] counterparts;
    object = nullptr;
    counterpart = nullptr;
    composite = 0u;
    visualKind = LayoutVisualKind::Stable;
    visualOpacity = 255u;
    if (result == Result::Success) result = sort();
    return result;
}

}  // namespace vis
