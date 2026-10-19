#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

#include "vis.h"
#include "../common/visFinite.h"
#include "../common/visString.h"
#include "../common/visConstants.h"
#include "visRendererBackend.h"
#include "visScene.h"

namespace vis
{

static constexpr uint32_t MAX_OBJECT_DEPTH = 64;

const char* type(Type type) noexcept
{
    switch (type) {
        case Type::Space: return "space";
        case Type::Point: return "point";
        case Type::Line: return "line";
        case Type::Arrow: return "arrow";
        case Type::Vector: return "vector";
        case Type::Circle: return "circle";
        case Type::Arc: return "arc";
        case Type::Angle: return "angle";
        case Type::Polygon: return "polygon";
        case Type::Plot: return "plot";
        case Type::Route: return "route";
        case Type::Text: return "text";
        case Type::Picture: return "picture";
        case Type::Cell: return "cell";
        case Type::Group: return "group";
        case Type::Rectangle: return "rectangle";
        case Type::Connector: return "connector";
        case Type::Path: return "path";
        case Type::Curve: return "curve";
        case Type::SurfaceMesh: return "surface";
    }
    return "unknown";
}

Object::Object(Type type) noexcept : objType(type)
{
}

Object::~Object()
{
    if (objMask) objMask->objMasked = nullptr;
    if (objMasked) objMasked->objMask = nullptr;
    for (auto i = 0u; i < childCnt; i++)
        delete children[i];
    delete[] children;
    delete[] objTag;
}

Type Object::type() const noexcept
{
    return objType;
}

uint32_t Object::id() const noexcept
{
    return objId;
}

Object* Object::parent() const noexcept
{
    return objParent;
}

Space* Object::space() const noexcept
{
    for (auto object = objParent; object; object = object->objParent) {
        if (object->type() == Type::Space) return static_cast<Space*>(object);
    }
    return nullptr;
}

bool Object::owned() const noexcept
{
    return objOwner;
}

bool Object::grow() noexcept
{
    if (childCnt < childCap) return true;
    auto capacity = childCap ? childCap * 2u : 8u;
    if (capacity < childCap) return false;
    auto grown = new (std::nothrow) Object*[capacity];
    if (!grown) return false;
    for (auto i = 0u; i < childCnt; i++)
        grown[i] = children[i];
    delete[] children;
    children = grown;
    childCap = capacity;
    return true;
}

static bool _height(const Object* object, uint32_t depth, uint32_t& height)
{
    if (!object || depth > MAX_OBJECT_DEPTH) return false;
    if (depth > height) height = depth;
    for (auto i = 0u; i < object->childCount(); i++) {
        if (!_height(object->childAt(i), depth + 1u, height)) return false;
    }
    return true;
}

static constexpr auto CHILD_OWNED = "the child already belongs to a scene or a parent";
static constexpr auto CHILD_INVALID = "the child has invalid geometry or style values";
static constexpr auto CHILD_CYCLE = "an object cannot contain itself or one of its ancestors";
static constexpr auto CHILD_DEPTH = "objects nest at most 64 levels deep";

Result Object::add(Object* object) noexcept
{
    if (!object || object->objOwner || object->objParent) return fail(Result::InvalidArguments, CHILD_OWNED);
    if (object == this) return fail(Result::InvalidArguments, CHILD_CYCLE);
    if (!valid(object)) return fail(Result::InvalidArguments, CHILD_INVALID);
    auto ancestors = 0u;
    for (auto parent = this; parent; parent = parent->objParent) {
        if (parent == object) return fail(Result::InvalidArguments, CHILD_CYCLE);
        if (ancestors++ >= MAX_OBJECT_DEPTH) return fail(Result::InsufficientCondition, CHILD_DEPTH);
    }
    auto height = 0u;
    if (!_height(object, 0u, height) || ancestors + height > MAX_OBJECT_DEPTH) {
        return fail(Result::InsufficientCondition, CHILD_DEPTH);
    }
    if (!grow()) return fail(Result::OutOfMemory, "out of memory");
    if (objOwner) {
        auto result = objOwner->attach(object, this);
        if (result != Result::Success) return result;
    } else {
        object->objParent = this;
    }
    children[childCnt++] = object;
    return Result::Success;
}

uint32_t Object::childCount() const noexcept
{
    return childCnt;
}

Object* Object::childAt(uint32_t index) const noexcept
{
    if (index >= childCnt) return nullptr;
    return children[index];
}

Object& Object::tag(const char* value) noexcept
{
    if (objOwner) return *this;
    auto copy = _duplicate(value);
    if (value && !copy) return *this;
    delete[] objTag;
    objTag = copy;
    return *this;
}

const char* Object::tag() const noexcept
{
    return objTag;
}

Object& Object::stroke(Color color) noexcept
{
    if (owned()) return *this;
    style.stroke = color;
    objColorAuthored = true;
    objStrokeAuthored = true;
    return *this;
}

Object& Object::stroke(Color color, float width) noexcept
{
    stroke(color);
    return strokeWidth(width);
}

Object& Object::strokeWidth(float width) noexcept
{
    if (owned()) return *this;
    style.width = width < 0.0f ? 0.0f : width;
    objWidthAuthored = true;
    return *this;
}

static bool _same(const Color& a, const Color& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool _same(const Vec3& a, const Vec3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool Gradient::valid() const noexcept
{
    if (static_cast<uint8_t>(type) > static_cast<uint8_t>(GradientType::Conic)) return false;
    if (stopCount > StopLimit || stopCount == 1u) return false;
    auto previous = 0.0f;
    for (auto i = 0u; i < stopCount; i++) {
        auto offset = stops[i].offset;
        if (!std::isfinite(offset) || offset < 0.0f || offset > 1.0f || offset < previous) {
            return false;
        }
        previous = offset;
    }
    if (!_finite(from) || !_finite(to) || !_finite(center) || !_finite(focal)
        || !std::isfinite(radius) || !std::isfinite(focalRadius) || !std::isfinite(angle)) {
        return false;
    }
    if (sized && radius <= 0.0f) return false;
    if (focalRadius < 0.0f) return false;
    if (line && type != GradientType::Linear) return false;
    if (centered && type != GradientType::Radial && type != GradientType::Conic) return false;
    if ((sized || focused || focalRadius > 0.0f) && type != GradientType::Radial) return false;
    if (angle != 0.0f && type != GradientType::Conic) return false;
    return true;
}

bool Gradient::operator==(const Gradient& other) const noexcept
{
    if (type != other.type || stopCount != other.stopCount || line != other.line
        || centered != other.centered || sized != other.sized || focused != other.focused
        || radius != other.radius || focalRadius != other.focalRadius || angle != other.angle
        || !_same(from, other.from) || !_same(to, other.to) || !_same(center, other.center)
        || !_same(focal, other.focal)) {
        return false;
    }
    for (auto i = 0u; i < stopCount && i < StopLimit; i++) {
        if (stops[i].offset != other.stops[i].offset
            || !_same(stops[i].color, other.stops[i].color)) {
            return false;
        }
    }
    return true;
}

static float _mixValue(float from, float to, float progress)
{
    return static_cast<float>(static_cast<double>(from)
                              + (static_cast<double>(to) - from) * progress);
}

static uint8_t _mixByte(uint8_t from, uint8_t to, float progress)
{
    auto value = static_cast<float>(from) + (static_cast<float>(to) - from) * progress;
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return 255;
    return static_cast<uint8_t>(value + 0.5f);
}

Gradient mix(const Gradient& from, const Gradient& to, float progress) noexcept
{
    auto bounded = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    if (from.type != to.type || from.stopCount != to.stopCount || from.line != to.line
        || from.centered != to.centered || from.sized != to.sized
        || from.focused != to.focused) {
        return bounded < 0.5f ? from : to;
    }
    auto output = from;
    for (auto i = 0u; i < output.stopCount && i < Gradient::StopLimit; i++) {
        auto& a = from.stops[i];
        auto& b = to.stops[i];
        output.stops[i].offset = _mixValue(a.offset, b.offset, bounded);
        output.stops[i].color = {_mixByte(a.color.r, b.color.r, bounded),
                                 _mixByte(a.color.g, b.color.g, bounded),
                                 _mixByte(a.color.b, b.color.b, bounded),
                                 _mixByte(a.color.a, b.color.a, bounded)};
    }
    auto point = [bounded](const Vec3& a, const Vec3& b) {
        return Vec3{_mixValue(a.x, b.x, bounded), _mixValue(a.y, b.y, bounded),
                    _mixValue(a.z, b.z, bounded)};
    };
    output.from = point(from.from, to.from);
    output.to = point(from.to, to.to);
    output.center = point(from.center, to.center);
    output.focal = point(from.focal, to.focal);
    output.radius = _mixValue(from.radius, to.radius, bounded);
    output.focalRadius = _mixValue(from.focalRadius, to.focalRadius, bounded);
    output.angle = _mixValue(from.angle, to.angle, bounded);
    return output;
}

Object& Object::gradient(bool enabled) noexcept
{
    if (owned()) return *this;
    style.gradient = {};
    if (enabled) style.gradient.type = GradientType::Linear;
    objGradientAuthored = true;
    return *this;
}

Object& Object::gradient(Color end) noexcept
{
    if (owned()) return *this;
    style.gradient = {};
    style.gradient.type = GradientType::Linear;
    style.gradientEnd = end;
    objGradientAuthored = true;
    objGradientEndAuthored = true;
    return *this;
}

Result Object::gradient(const Gradient& value) noexcept
{
    if (owned()) return Result::InsufficientCondition;
    if (!value.valid()) return Result::InvalidArguments;
    style.gradient = value;
    if (!value.enabled()) style.gradient = {};
    objGradientAuthored = true;
    // Explicit stops own every color, so the Theme end stop must not override them.
    if (value.stopCount) objGradientEndAuthored = true;
    return Result::Success;
}

Result Object::dash(const float* pattern, uint32_t count, float offset) noexcept
{
    if (owned()) return Result::InsufficientCondition;
    if ((pattern && !count) || (!pattern && count) || count > Style::DashLimit
        || !std::isfinite(offset)) {
        return Result::InvalidArguments;
    }
    auto visible = false;
    for (auto i = 0u; i < count; i++) {
        if (!std::isfinite(pattern[i]) || pattern[i] < 0.0f) return Result::InvalidArguments;
        if (pattern[i] > 0.0f) visible = true;
    }
    if (count && !visible) return Result::InvalidArguments;
    for (auto i = 0u; i < Style::DashLimit; i++) {
        style.dash[i] = i < count ? pattern[i] : 0.0f;
    }
    style.dashCount = static_cast<uint8_t>(count);
    style.dashOffset = count ? offset : 0.0f;
    return Result::Success;
}

Object& Object::fill(Color color) noexcept
{
    if (owned()) return *this;
    style.fill = color;
    objColorAuthored = true;
    objFillAuthored = true;
    return *this;
}

Object& Object::shift(const Vec3& by) noexcept
{
    if (owned()) return *this;
    model = Mat4::translate(by) * model;
    return *this;
}

Object& Object::scale(const Vec3& by) noexcept
{
    if (owned()) return *this;
    model = Mat4::scale(by) * model;
    return *this;
}

Object& Object::rotateX(float radians) noexcept
{
    if (owned()) return *this;
    model = Mat4::rotateX(radians) * model;
    return *this;
}

Object& Object::rotateY(float radians) noexcept
{
    if (owned()) return *this;
    model = Mat4::rotateY(radians) * model;
    return *this;
}

Object& Object::rotateZ(float radians) noexcept
{
    if (owned()) return *this;
    model = Mat4::rotateZ(radians) * model;
    return *this;
}

Object& Object::transform(const Mat4& by) noexcept
{
    if (owned()) return *this;
    model = by * model;
    return *this;
}

static constexpr auto COMPOSITE_MOUNTED =
    "the scene is shown inside another scene and can no longer change";

Result Object::isolate(bool enabled) noexcept
{
    if (objOwner && objOwner->pImpl && objOwner->pImpl->owner) {
        return fail(Result::InsufficientCondition, COMPOSITE_MOUNTED);
    }
    objIsolated = enabled;
    return Result::Success;
}

bool Object::isolated() const noexcept
{
    return objIsolated;
}

static constexpr auto MASK_MODE = "unknown mask mode";
static constexpr auto MASK_SCENE = "the mask target must belong to the same scene as the object";
static constexpr auto MASK_SELF = "an object cannot be masked by itself or by an object that contains it";
static constexpr auto MASK_TAKEN = "the target already masks another object";
static constexpr auto MASK_NESTED = "a mask target and the objects below it cannot be masked";

Result Object::mask(Object* target, MaskMode mode) noexcept
{
    if (objOwner && objOwner->pImpl && objOwner->pImpl->owner) {
        return fail(Result::InsufficientCondition, COMPOSITE_MOUNTED);
    }
    if (mode > MaskMode::InverseLuma) return fail(Result::InvalidArguments, MASK_MODE);
    if (target) {
        if (!objOwner || target->objOwner != objOwner) return fail(Result::InvalidArguments, MASK_SCENE);
        // A mask that drew the object, or another mask, could end up drawing itself.
        for (auto object = this; object; object = object->objParent) {
            if (object == target) return fail(Result::InvalidArguments, MASK_SELF);
            if (object->objMasked) return fail(Result::InvalidArguments, MASK_NESTED);
        }
        if (target->objMasked && target->objMasked != this) return fail(Result::InvalidArguments, MASK_TAKEN);
        auto masked = [](auto& self, const Object* object) -> bool {
            if (object->objMask) return true;
            for (auto i = 0u; i < object->childCnt; i++) {
                if (self(self, object->children[i])) return true;
            }
            return false;
        };
        if (masked(masked, target)) return fail(Result::InvalidArguments, MASK_NESTED);
    }
    if (objMask) objMask->objMasked = nullptr;
    objMask = target;
    objMaskMode = mode;
    if (target) target->objMasked = this;
    return Result::Success;
}

Object* Object::mask() const noexcept
{
    return objMask;
}

MaskMode Object::maskMode() const noexcept
{
    return objMaskMode;
}

static constexpr auto BLEND_MODE = "unknown blend mode";

Result Object::blend(BlendMode mode) noexcept
{
    if (objOwner && objOwner->pImpl && objOwner->pImpl->owner) {
        return fail(Result::InsufficientCondition, COMPOSITE_MOUNTED);
    }
    if (mode > BlendMode::Add) return fail(Result::InvalidArguments, BLEND_MODE);
    objBlend = mode;
    return Result::Success;
}

BlendMode Object::blend() const noexcept
{
    return objBlend;
}

bool Scene::Impl::composited(const Object* object) noexcept
{
    return object
           && (object->objIsolated || object->objMask || object->objMasked || object->objBlend != BlendMode::Normal
               || object->objEffects || object->blur > 0.0f || object->shadow.color.a > 0);
}

Line::Line(const Vec3& from, const Vec3& to) noexcept : Object(Type::Line), from(from), to(to)
{
}

Line* Line::gen(const Vec3& from, const Vec3& to) noexcept
{
    return new (std::nothrow) Line(from, to);
}

Arrow::Arrow(const Vec3& from, const Vec3& to) noexcept : Object(Type::Arrow), from(from), to(to)
{
}

Arrow* Arrow::gen(const Vec3& from, const Vec3& to) noexcept
{
    return new (std::nothrow) Arrow(from, to);
}

Point::Point(const Vec3& point) noexcept : Object(Type::Point), point(point)
{
    style.fill = style.stroke;
}

Point* Point::gen(const Vec3& point) noexcept
{
    return new (std::nothrow) Point(point);
}

Text::Text(const char* text, const Vec3& point) noexcept : Object(Type::Text), point(point), objText(_duplicate(text))
{
    style.fill = style.stroke;
    style.stroke.a = 0;
}

// Deletes the strings a text keeps for its text changes, its own string among them.
static void _forget(char**& texts, uint32_t& count)
{
    for (auto i = 0u; i < count; i++)
        delete[] texts[i];
    delete[] texts;
    texts = nullptr;
    count = 0;
}

Text::~Text()
{
    if (objTextCnt) _forget(objTexts, objTextCnt);
    else delete[] objText;
    delete[] objFont;
    delete[] objPrefix;
    delete[] objSuffix;
}

Text* Text::gen(const char* text, const Vec3& point) noexcept
{
    if (!text) return nullptr;
    auto object = new (std::nothrow) Text(text, point);
    if (!object || object->objText) return object;
    delete object;
    return nullptr;
}

const char* Text::text() const noexcept
{
    return objText;
}

Result Text::measure(TextExtent& output) const noexcept
{
    return measure(objText, output);
}

Result Text::measure(const char* value, TextExtent& output) const noexcept
{
    output = {};
    if (!value || !objFont || !std::isfinite(size) || size <= 0.0f) return Result::InvalidArguments;
    return renderer::measureText(objFont, size, value, slant, tabular, output);
}

uint32_t Text::texts(const char** output, uint32_t capacity) const noexcept
{
    if (!objTextCnt) {
        if (output && capacity) output[0] = objText;
        return 1u;
    }
    for (auto i = 0u; output && i < objTextCnt && i < capacity; i++)
        output[i] = objTexts[i];
    return objTextCnt;
}

Result Text::text(const char* value) noexcept
{
    if (!value) return Result::InvalidArguments;
    auto copy = _duplicate(value);
    if (!copy) return Result::OutOfMemory;
    auto previous = objText;
    auto counted = objCounting;
    objText = copy;
    objCounting = false;
    if (objOwner) {
        auto result = objOwner->update(this);
        if (result != Result::Success) {
            delete[] objText;
            objText = previous;
            objCounting = counted;
            return result;
        }
    }
    // A new string also ends the text changes, whose strings hold the previous one.
    if (objTextCnt) _forget(objTexts, objTextCnt);
    else delete[] previous;
    // A plain string ends counting.
    delete[] objPrefix;
    delete[] objSuffix;
    objPrefix = nullptr;
    objSuffix = nullptr;
    objCapacity = 0;
    objValue = 0.0f;
    objFormat = {};
    return Result::Success;
}

// Room for the sign, digits, separators and point of any finite float.
static constexpr uint32_t NUMBER_ROOM = 64;

Result Text::count(float value, const NumberFormat& format, const char* prefix, const char* suffix) noexcept
{
    if (!std::isfinite(value) || format.decimals > 6u
        || (format.separator && (format.separator < ' ' || format.separator > '~'))) {
        return Result::InvalidArguments;
    }
    auto prefixLength = prefix ? std::strlen(prefix) : 0u;
    auto suffixLength = suffix ? std::strlen(suffix) : 0u;
    if (prefixLength + suffixLength > UINT32_MAX - NUMBER_ROOM - 1u) return Result::InvalidArguments;
    auto capacity = static_cast<uint32_t>(prefixLength + suffixLength + NUMBER_ROOM + 1u);
    auto text = new (std::nothrow) char[capacity];
    auto prefixCopy = prefixLength ? _duplicate(prefix) : nullptr;
    auto suffixCopy = suffixLength ? _duplicate(suffix) : nullptr;
    if (!text || (prefixLength && !prefixCopy) || (suffixLength && !suffixCopy)) {
        delete[] text;
        delete[] prefixCopy;
        delete[] suffixCopy;
        return Result::OutOfMemory;
    }
    auto previousText = objText;
    auto previousPrefix = objPrefix;
    auto previousSuffix = objSuffix;
    auto previousCapacity = objCapacity;
    auto previousValue = objValue;
    auto previousFormat = objFormat;
    auto previousCounting = objCounting;
    objText = text;
    objPrefix = prefixCopy;
    objSuffix = suffixCopy;
    objCapacity = capacity;
    objValue = value;
    objFormat = format;
    objCounting = true;
    write(value, objText, objCapacity);
    if (objOwner) {
        auto result = objOwner->update(this);
        if (result != Result::Success) {
            delete[] objText;
            delete[] objPrefix;
            delete[] objSuffix;
            objText = previousText;
            objPrefix = previousPrefix;
            objSuffix = previousSuffix;
            objCapacity = previousCapacity;
            objValue = previousValue;
            objFormat = previousFormat;
            objCounting = previousCounting;
            return result;
        }
    }
    if (objTextCnt) _forget(objTexts, objTextCnt);
    else delete[] previousText;
    delete[] previousPrefix;
    delete[] previousSuffix;
    return Result::Success;
}

bool Text::counting() const noexcept
{
    return objCounting;
}

float Text::value() const noexcept
{
    return objCounting ? objValue : 0.0f;
}

NumberFormat Text::format() const noexcept
{
    return objFormat;
}

uint32_t Text::write(float value, char* output, uint32_t capacity) const noexcept
{
    if (!objCounting || !output || !std::isfinite(value)) return 0u;
    char digits[NUMBER_ROOM];
    // snprintf rather than std::to_chars: same digits, without libc++'s float tables in the WASM runtime
    auto written = std::snprintf(digits, sizeof(digits), "%.*f", static_cast<int>(objFormat.decimals),
                                 static_cast<double>(std::fabs(value)));
    if (written < 0 || static_cast<size_t>(written) >= sizeof(digits)) return 0u;
    auto length = static_cast<uint32_t>(written);
    // Digits before the point, and whether the value rounds to zero.
    auto whole = length;
    auto zero = true;
    for (auto i = 0u; i < length; i++) {
        if (digits[i] == '.') whole = i;
        else if (digits[i] != '0') zero = false;
    }
    auto sign = !zero && (value < 0.0f || objFormat.sign);
    auto separators = objFormat.separator && whole > 3u ? (whole - 1u) / 3u : 0u;
    auto prefixLength = objPrefix ? std::strlen(objPrefix) : 0u;
    auto suffixLength = objSuffix ? std::strlen(objSuffix) : 0u;
    auto total = prefixLength + (sign ? 1u : 0u) + length + separators + suffixLength;
    if (total >= capacity) return 0u;
    auto at = output;
    if (prefixLength) std::memcpy(at, objPrefix, prefixLength);
    at += prefixLength;
    if (sign) *at++ = value < 0.0f ? '-' : '+';
    for (auto i = 0u; i < whole; i++) {
        if (separators && i > 0u && (whole - i) % 3u == 0u) *at++ = objFormat.separator;
        *at++ = digits[i];
    }
    std::memcpy(at, digits + whole, length - whole);
    at += length - whole;
    if (suffixLength) std::memcpy(at, objSuffix, suffixLength);
    at[suffixLength] = '\0';
    return static_cast<uint32_t>(total);
}

const char* Text::font() const noexcept
{
    return objFont;
}

Result Text::font(const char* value) noexcept
{
    if (owned()) return Result::InsufficientCondition;
    auto copy = _duplicate(value);
    if (value && !copy) return Result::OutOfMemory;
    delete[] objFont;
    objFont = copy;
    return Result::Success;
}

Result Object::label(Text* text) noexcept
{
    return add(text);
}

Vector::Vector(const Vec3& value, const Vec3& origin) noexcept : Object(Type::Vector), value(value), origin(origin)
{
}

Vector* Vector::gen(const Vec3& value, const Vec3& origin) noexcept
{
    return new (std::nothrow) Vector(value, origin);
}

Group::Group() noexcept : Object(Type::Group)
{
    style.stroke.a = 0;
    style.fill.a = 0;
    style.width = 0.0f;
}

Group* Group::gen() noexcept
{
    auto group = new (std::nothrow) Group;
    if (group) group->objPlain = true;
    return group;
}

Space::Space(const Range& x, const Range& y, const Range& z) noexcept : Object(Type::Space), x(x), y(y), z(z)
{
    style.stroke = {55, 65, 81, 180};
    style.width = 1.0f;
    layer = -10;
}

Space::~Space()
{
}

Space* Space::gen(const Range& x, const Range& y, const Range& z) noexcept
{
    if (x.step <= 0.0f || y.step <= 0.0f || z.step <= 0.0f) return nullptr;
    if (x.min > x.max || y.min > y.max || z.min > z.max) return nullptr;
    return new (std::nothrow) Space(x, y, z);
}

Result Space::add(Object* object) noexcept
{
    if (objSampling) return Result::InsufficientCondition;
    return Object::add(object);
}

uint32_t Space::count() const noexcept
{
    return childCount();
}

Object* Space::objectAt(uint32_t index) const noexcept
{
    return childAt(index);
}

static Result _samples(const Range& range, float& first, uint32_t& count) noexcept
{
    if (!std::isfinite(range.min) || !std::isfinite(range.max)
        || !std::isfinite(range.step) || range.min > range.max || range.step <= 0.0f) {
        return Result::InvalidArguments;
    }
    auto step = static_cast<double>(range.step);
    auto start = std::ceil(static_cast<double>(range.min) / step) * step;
    auto tolerance = step * 0.001;
    if (!std::isfinite(start) || start > static_cast<double>(range.max) + tolerance) {
        return Result::InsufficientCondition;
    }
    auto samples = std::floor((static_cast<double>(range.max) + tolerance - start) / step) + 1.0;
    if (!std::isfinite(samples) || samples < 1.0 || samples > 4096.0) {
        return Result::InsufficientCondition;
    }
    first = static_cast<float>(start);
    count = static_cast<uint32_t>(samples);
    return Result::Success;
}

struct CellSample
{
    CellSampler callback = nullptr;
    void* data = nullptr;
};

static bool _sampleCell(float x, float y, float, float time,
                        Color& color, void* data) noexcept
{
    auto context = static_cast<CellSample*>(data);
    return context->callback(x, y, time, color, context->data);
}

Result Space::cell(CellSampler callback, Group*& cells, void* data,
                   CellMode mode, float padding, const SampleTime& time) noexcept
{
    cells = nullptr;
    if (!callback) return Result::InvalidArguments;
    auto context = CellSample{callback, data};
    return sample(_sampleCell, cells, &context, mode, padding, time, false);
}

Result Space::voxel(VoxelSampler callback, Group*& voxels, void* data,
                    CellMode mode, float padding, const SampleTime& time) noexcept
{
    return sample(callback, voxels, data, mode, padding, time, true);
}

static Result _timeFrames(const SampleTime& time, uint64_t volume,
                          uint32_t& frames) noexcept
{
    static constexpr uint64_t MaxColors = 262144u;
    if (!std::isfinite(time.duration) || time.duration < 0.0f || !time.fps) {
        return Result::InvalidArguments;
    }
    if (time.duration == 0.0f) {
        frames = 1u;
    } else {
        auto intervals = std::ceil(static_cast<double>(time.duration) * time.fps);
        if (!std::isfinite(intervals) || intervals < 1.0 || intervals > 4095.0) {
            return Result::InsufficientCondition;
        }
        frames = static_cast<uint32_t>(intervals) + 1u;
    }
    if (volume > MaxColors / frames) return Result::InsufficientCondition;
    return Result::Success;
}

Result Space::sample(VoxelSampler callback, Group*& output, void* data,
                     CellMode mode, float padding, const SampleTime& time,
                     bool spatial) noexcept
{
    output = nullptr;
    if (objSampling) return Result::InsufficientCondition;
    if (!callback || (mode != CellMode::Full && mode != CellMode::Padd)
        || !std::isfinite(padding) || padding < 0.0f || padding >= 0.5f) {
        return Result::InvalidArguments;
    }
    float starts[3];
    uint32_t counts[3];
    auto result = _samples(x, starts[0], counts[0]);
    if (result != Result::Success) return result;
    result = _samples(y, starts[1], counts[1]);
    if (result != Result::Success) return result;
    if (spatial) {
        result = _samples(z, starts[2], counts[2]);
        if (result != Result::Success) return result;
    } else {
        starts[2] = 0.0f;
        counts[2] = 1u;
    }
    auto slice = static_cast<uint64_t>(counts[0]) * counts[1];
    auto volume = slice * counts[2];
    if (slice > 16384u || volume > 65536u) return Result::InsufficientCondition;
    uint32_t frames;
    result = _timeFrames(time, volume, frames);
    if (result != Result::Success) return result;

    auto group = Group::gen();
    if (!group) return Result::OutOfMemory;
    for (auto iz = 0u; iz < counts[2]; iz++) {
        auto cell = Cell::gen({}, counts[0], counts[1], {}, mode, padding);
        if (!cell) {
            delete group;
            return Result::OutOfMemory;
        }
        result = cell->frames(frames, time.duration);
        if (result != Result::Success) {
            delete cell;
            delete group;
            return result;
        }
        auto zStep = spatial ? z.step : 1.0f;
        auto zValue = starts[2] + static_cast<float>(iz) * zStep;
        cell->model = Mat4::translate({
            starts[0] - x.step * 0.5f,
            starts[1] - y.step * 0.5f,
            zValue - zStep * 0.5f}) * Mat4::scale({x.step, y.step, zStep});
        result = group->add(cell);
        if (result != Result::Success) {
            delete cell;
            delete group;
            return result;
        }
    }

    objSampling = true;
    for (auto frame = 0u; frame < frames; frame++) {
        auto sampleTime = frames > 1u
                        ? time.duration * static_cast<float>(frame) / static_cast<float>(frames - 1u)
                        : 0.0f;
        for (auto iz = 0u; iz < counts[2]; iz++) {
            auto cell = static_cast<Cell*>(group->childAt(iz));
            auto zStep = spatial ? z.step : 1.0f;
            auto zValue = starts[2] + static_cast<float>(iz) * zStep;
            for (auto iy = 0u; iy < counts[1]; iy++) {
                auto yValue = starts[1] + static_cast<float>(iy) * y.step;
                for (auto ix = 0u; ix < counts[0]; ix++) {
                    auto xValue = starts[0] + static_cast<float>(ix) * x.step;
                    Color color;
                    if (!callback(xValue, yValue, zValue, sampleTime, color, data)) {
                        objSampling = false;
                        delete group;
                        return Result::InvalidArguments;
                    }
                    auto index = static_cast<size_t>(frame) * slice
                               + static_cast<size_t>(iy) * counts[0] + ix;
                    cell->objColors[index] = color;
                }
            }
        }
    }
    objSampling = false;
    result = add(group);
    if (result != Result::Success) {
        delete group;
        return result;
    }
    output = group;
    return Result::Success;
}

static Mat4 _world(const Space* space)
{
    auto matrix = space->model;
    for (auto parent = space->parent(); parent; parent = parent->parent())
        matrix = parent->model * matrix;
    return matrix;
}

Vec3 Space::c2w(const Vec3& coordinate) const noexcept
{
    return _world(this) * coordinate;
}

bool Space::w2c(const Vec3& world, Vec3& coordinate) const noexcept
{
    auto matrix = _world(this);
    auto a = static_cast<double>(matrix.e[0]);
    auto b = static_cast<double>(matrix.e[1]);
    auto c = static_cast<double>(matrix.e[2]);
    auto d = static_cast<double>(matrix.e[4]);
    auto e = static_cast<double>(matrix.e[5]);
    auto f = static_cast<double>(matrix.e[6]);
    auto g = static_cast<double>(matrix.e[8]);
    auto h = static_cast<double>(matrix.e[9]);
    auto i = static_cast<double>(matrix.e[10]);
    auto det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    auto scale = std::fmax(std::fmax(std::fabs(a), std::fabs(b)), std::fabs(c));
    scale = std::fmax(scale, std::fmax(std::fmax(std::fabs(d), std::fabs(e)), std::fabs(f)));
    scale = std::fmax(scale, std::fmax(std::fmax(std::fabs(g), std::fabs(h)), std::fabs(i)));
    if (!std::isfinite(det) || scale == 0.0 || std::fabs(det) <= std::numeric_limits<double>::epsilon() * scale * scale * scale) {
        return false;
    }
    auto x = static_cast<double>(world.x) - matrix.e[3];
    auto y = static_cast<double>(world.y) - matrix.e[7];
    auto z = static_cast<double>(world.z) - matrix.e[11];
    auto inv = 1.0 / det;
    auto result = Vec3{
        static_cast<float>(((e * i - f * h) * x + (c * h - b * i) * y + (b * f - c * e) * z) * inv),
        static_cast<float>(((f * g - d * i) * x + (a * i - c * g) * y + (c * d - a * f) * z) * inv),
        static_cast<float>(((d * h - e * g) * x + (b * g - a * h) * y + (a * e - b * d) * z) * inv)};
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) return false;
    coordinate = result;
    return true;
}

Circle::Circle(const Vec3& center, float radius) noexcept : Object(Type::Circle), center(center), radius(radius)
{
}

Circle* Circle::gen(const Vec3& center, float radius) noexcept
{
    if (radius <= 0.0f) return nullptr;
    return new (std::nothrow) Circle(center, radius);
}

Arc::Arc(const Vec3& center, float radius, float start, float angle) noexcept
    : Object(Type::Arc), center(center), radius(radius), start(start), angle(angle)
{
    style.fill.a = 0;
}

Arc* Arc::gen(const Vec3& center, float radius, float start, float angle) noexcept
{
    auto object = new (std::nothrow) Arc(center, radius, start, angle);
    if (object && !valid(object)) { delete object; return nullptr; }
    return object;
}

Angle::Angle(const Vec3& vertex, const Vec3& from, const Vec3& to, float radius) noexcept
    : Object(Type::Angle), vertex(vertex), from(from), to(to), radius(radius), labelDistance(radius * 1.7f)
{
    style.fill.a = 0;
}

Angle* Angle::gen(const Vec3& vertex, const Vec3& from, const Vec3& to, float radius) noexcept
{
    auto object = new (std::nothrow) Angle(vertex, from, to, radius);
    if (object && !valid(object)) { delete object; return nullptr; }
    return object;
}

Vec3 Angle::labelPoint() const noexcept
{
    return angleLabelPoint(this);
}

Polygon::Polygon(const Vec3* points, uint32_t count) noexcept : Object(Type::Polygon), objCount(count)
{
    objPoints = new (std::nothrow) Vec3[count];
    if (objPoints) std::memcpy(objPoints, points, sizeof(Vec3) * count);
}

Polygon::~Polygon()
{
    delete[] objPoints;
}

Polygon* Polygon::gen(const Vec3* points, uint32_t count) noexcept
{
    if (!points || count < 3) return nullptr;
    auto object = new (std::nothrow) Polygon(points, count);
    if (!object || object->objPoints) return object;
    delete object;
    return nullptr;
}

const Vec3* Polygon::points() const noexcept
{
    return objPoints;
}

uint32_t Polygon::count() const noexcept
{
    return objCount;
}

Plot::Plot(const Vec3* points, uint32_t count) noexcept : Object(Type::Plot), objCount(count)
{
    // An empty plot, as a geometry clip may leave one, has no pieces either.
    if (!count) {
        objContours = 0;
        return;
    }
    objPoints = new (std::nothrow) Vec3[count];
    if (!objPoints) return;
    std::memcpy(objPoints, points, sizeof(Vec3) * count);
    objRoom = count;
}

Plot::~Plot()
{
    delete[] objPoints;
    delete[] objEnds;
    delete[] objSamplePoints;
}

Plot* Plot::gen(const Vec3* points, uint32_t count) noexcept
{
    if (!points || count < 2) return nullptr;
    auto object = new (std::nothrow) Plot(points, count);
    if (!object || object->objPoints) return object;
    delete object;
    return nullptr;
}

Plot* Plot::gen(const Vec3* points, uint32_t count, const uint32_t* ends, uint32_t pieces,
                const PlotSamples* samples) noexcept
{
    if (samples && !samples->valid()) return nullptr;
    if (!pieces || (pieces > 1u && !ends)) return nullptr;
    if (ends) {
        // Every piece holds two points or more, and the last ends at count.
        auto start = 0u;
        for (auto i = 0u; i < pieces; i++) {
            if (ends[i] < start || ends[i] - start < 2u || ends[i] > count) return nullptr;
            start = ends[i];
        }
        if (start != count) return nullptr;
    }
    auto object = gen(points, count);
    if (object && samples && samples->count) {
        object->objSamplePoints = new (std::nothrow) Vec3[samples->count];
        if (!object->objSamplePoints) {
            delete object;
            return nullptr;
        }
        std::memcpy(object->objSamplePoints, samples->points, sizeof(Vec3) * samples->count);
        object->objSamples = *samples;
        object->objSamples.points = object->objSamplePoints;
        object->objSampleRoom = samples->count;
    }
    if (!object || pieces == 1u) return object;
    object->objEnds = new (std::nothrow) uint32_t[pieces];
    if (!object->objEnds) {
        delete object;
        return nullptr;
    }
    std::memcpy(object->objEnds, ends, sizeof(uint32_t) * pieces);
    object->objContours = pieces;
    object->objEndRoom = pieces;
    return object;
}

const Vec3* Plot::points() const noexcept
{
    return objPoints;
}

PlotSamples Plot::samples() const noexcept
{
    return objSamples;
}

uint32_t Plot::count() const noexcept
{
    return objCount;
}

uint32_t Plot::contours() const noexcept
{
    return objContours;
}

uint32_t Plot::contourEnd(uint32_t index) const noexcept
{
    if (!objEnds || index + 1u >= objContours) return objCount;
    return objEnds[index];
}

DirectedRoute::DirectedRoute(const Vec3* points, uint32_t count) noexcept : Object(Type::Route), objCount(count)
{
    objPoints = new (std::nothrow) Vec3[count];
    if (objPoints) std::memcpy(objPoints, points, sizeof(Vec3) * count);
}

DirectedRoute::~DirectedRoute()
{
    delete[] objPoints;
}

DirectedRoute* DirectedRoute::gen(const Vec3* points, uint32_t count) noexcept
{
    if (!points || count < 2) return nullptr;
    auto object = new (std::nothrow) DirectedRoute(points, count);
    if (!object || object->objPoints) return object;
    delete object;
    return nullptr;
}

const Vec3* DirectedRoute::points() const noexcept
{
    return objPoints;
}

uint32_t DirectedRoute::count() const noexcept
{
    return objCount;
}

static constexpr uint32_t PATH_SAMPLE_LIMIT = 1024u;

PathCommand PathCommand::move(const Vec3& to) noexcept
{
    return {PathVerb::Move, to, {}, {}};
}

PathCommand PathCommand::line(const Vec3& to) noexcept
{
    return {PathVerb::Line, to, {}, {}};
}

PathCommand PathCommand::quadratic(const Vec3& control, const Vec3& to) noexcept
{
    return {PathVerb::Quadratic, to, control, {}};
}

PathCommand PathCommand::cubic(const Vec3& control1, const Vec3& control2,
                               const Vec3& to) noexcept
{
    return {PathVerb::Cubic, to, control1, control2};
}

PathCommand PathCommand::close() noexcept
{
    return {PathVerb::Close, {}, {}, {}};
}

static Vec3 _quadratic(const Vec3& from, const Vec3& control, const Vec3& to, float t)
{
    auto inverse = 1.0f - t;
    return from * (inverse * inverse) + control * (2.0f * inverse * t) + to * (t * t);
}

static Vec3 _cubic(const Vec3& from, const Vec3& control1, const Vec3& control2,
                   const Vec3& to, float t)
{
    auto inverse = 1.0f - t;
    auto inverse2 = inverse * inverse;
    auto t2 = t * t;
    return from * (inverse2 * inverse) + control1 * (3.0f * inverse2 * t)
         + control2 * (3.0f * inverse * t2) + to * (t2 * t);
}

Path::Path(Vec3* points, uint32_t count, bool closed, uint32_t* ends, uint32_t contours) noexcept :
    Object(Type::Path), objPoints(points), objEnds(ends), objCount(count), objContours(contours),
    objClosed(closed)
{
}

Path::~Path()
{
    delete[] objPoints;
    delete[] objEnds;
}

Path* Path::gen(const PathCommand* commands, uint32_t count, uint32_t samples) noexcept
{
    if (!commands || count < 2u || !samples || samples > PATH_SAMPLE_LIMIT
        || commands[0].verb != PathVerb::Move) {
        return nullptr;
    }
    // Every contour needs two points, three when it is closed, and either all close or none.
    auto points = uint64_t{0};
    auto contourPoints = uint64_t{0};
    auto contours = 0u;
    auto closedContours = 0u;
    auto drawing = false;
    for (auto i = 0u; i < count; i++) {
        switch (commands[i].verb) {
            case PathVerb::Move:
                if (drawing && contourPoints < 2u) return nullptr;
                if (contours == UINT32_MAX) return nullptr;
                contours++;
                contourPoints = 1u;
                drawing = true;
                points++;
                break;
            case PathVerb::Line:
                if (!drawing) return nullptr;
                contourPoints++;
                points++;
                break;
            case PathVerb::Quadratic:
            case PathVerb::Cubic:
                if (!drawing) return nullptr;
                contourPoints += samples;
                points += samples;
                break;
            case PathVerb::Close:
                if (!drawing || contourPoints < 3u) return nullptr;
                closedContours++;
                drawing = false;
                break;
            default: return nullptr;
        }
        if (points > UINT32_MAX) return nullptr;
    }
    if (drawing && contourPoints < 2u) return nullptr;
    if (closedContours && closedContours != contours) return nullptr;
    auto closed = closedContours != 0u;
    auto output = new (std::nothrow) Vec3[static_cast<uint32_t>(points)];
    auto ends = contours > 1u ? new (std::nothrow) uint32_t[contours] : nullptr;
    if (!output || (contours > 1u && !ends)) {
        delete[] output;
        delete[] ends;
        return nullptr;
    }
    auto cursor = 0u;
    auto contour = 0u;
    for (auto i = 0u; i < count; i++) {
        auto& command = commands[i];
        switch (command.verb) {
            case PathVerb::Move:
                if (ends && cursor) ends[contour++] = cursor;
                output[cursor++] = command.to;
                break;
            case PathVerb::Line: output[cursor++] = command.to; break;
            case PathVerb::Quadratic: {
                auto from = output[cursor - 1u];
                for (auto sample = 1u; sample <= samples; sample++) {
                    auto t = static_cast<float>(sample) / static_cast<float>(samples);
                    output[cursor++] = _quadratic(from, command.control1, command.to, t);
                }
                break;
            }
            case PathVerb::Cubic: {
                auto from = output[cursor - 1u];
                for (auto sample = 1u; sample <= samples; sample++) {
                    auto t = static_cast<float>(sample) / static_cast<float>(samples);
                    output[cursor++] = _cubic(from, command.control1, command.control2, command.to, t);
                }
                break;
            }
            case PathVerb::Close: break;
        }
    }
    if (ends) ends[contour] = cursor;
    auto object = new (std::nothrow) Path(output, cursor, closed, ends, contours);
    if (object) return object;
    delete[] output;
    delete[] ends;
    return nullptr;
}

const Vec3* Path::points() const noexcept
{
    return objPoints;
}

uint32_t Path::count() const noexcept
{
    return objCount;
}

bool Path::closed() const noexcept
{
    return objClosed;
}

uint32_t Path::contours() const noexcept
{
    return objContours;
}

uint32_t Path::contourEnd(uint32_t index) const noexcept
{
    if (!objEnds || index + 1u >= objContours) return objCount;
    return objEnds[index];
}

Curve::Curve(Vec3* points, uint32_t count) noexcept :
    Object(Type::Curve), objPoints(points), objCount(count)
{
}

Curve::~Curve()
{
    delete[] objPoints;
}

Curve* Curve::gen(const Vec3& from, const Vec3& control1, const Vec3& control2,
                  const Vec3& to, uint32_t samples) noexcept
{
    if (!samples || samples > PATH_SAMPLE_LIMIT) return nullptr;
    auto points = new (std::nothrow) Vec3[samples + 1u];
    if (!points) return nullptr;
    points[0] = from;
    for (auto sample = 1u; sample <= samples; sample++) {
        auto t = static_cast<float>(sample) / static_cast<float>(samples);
        points[sample] = _cubic(from, control1, control2, to, t);
    }
    auto object = new (std::nothrow) Curve(points, samples + 1u);
    if (!object) {
        delete[] points;
        return nullptr;
    }
    object->objControls[0] = from;
    object->objControls[1] = control1;
    object->objControls[2] = control2;
    object->objControls[3] = to;
    return object;
}

const Vec3* Curve::points() const noexcept
{
    return objPoints;
}

uint32_t Curve::count() const noexcept
{
    return objCount;
}

const Vec3* Curve::controls() const noexcept
{
    return objControls;
}

Vec3 curvePoint(const Vec3* controls, uint32_t index, uint32_t count) noexcept
{
    // The same arithmetic as gen(), so a curve drawn from its controls matches its points.
    if (!index || count < 2u) return controls[0];
    auto t = static_cast<float>(index) / static_cast<float>(count - 1u);
    return _cubic(controls[0], controls[1], controls[2], controls[3], t);
}

SurfaceMesh::SurfaceMesh(const Vec3* points, uint32_t columns, uint32_t rows) noexcept :
    Object(Type::SurfaceMesh), objColumns(columns), objRows(rows)
{
    if (rows <= std::numeric_limits<size_t>::max() / columns) {
        auto count = static_cast<size_t>(columns) * rows;
        objPoints = new (std::nothrow) Vec3[count];
        if (objPoints) std::memcpy(objPoints, points, count * sizeof(Vec3));
    }
    style.fill = {91, 141, 239, 255};
    style.stroke = {35, 55, 84, 255};
    style.width = 1.0f;
}

SurfaceMesh::~SurfaceMesh()
{
    delete[] objPoints;
    delete[] objColors;
}

Result SurfaceMesh::colors(const Color* colors, uint32_t count) noexcept
{
    if (!colors) {
        delete[] objColors;
        objColors = nullptr;
        return Result::Success;
    }
    if (count != objColumns * objRows) return Result::InvalidArguments;
    auto copy = new (std::nothrow) Color[count];
    if (!copy) return Result::OutOfMemory;
    std::memcpy(copy, colors, count * sizeof(Color));
    delete[] objColors;
    objColors = copy;
    return Result::Success;
}

const Color* SurfaceMesh::colors() const noexcept
{
    return objColors;
}

// Listed colormaps sampled at nine evenly spaced stops.
static constexpr uint8_t COLORMAP_STOPS[5][9][3] = {
    {{68, 1, 84}, {71, 45, 123}, {59, 82, 139}, {44, 114, 142}, {33, 144, 140},
     {40, 174, 128}, {93, 201, 99}, {171, 220, 50}, {253, 231, 37}},
    {{13, 8, 135}, {76, 2, 161}, {126, 3, 168}, {169, 35, 150}, {203, 71, 120},
     {230, 108, 92}, {248, 148, 65}, {253, 195, 40}, {240, 249, 33}},
    {{0, 0, 4}, {33, 12, 74}, {87, 16, 109}, {137, 34, 106}, {187, 55, 85},
     {227, 89, 50}, {249, 141, 10}, {249, 202, 50}, {252, 255, 164}},
    {{0, 0, 4}, {29, 17, 70}, {80, 18, 124}, {130, 38, 129}, {182, 55, 121},
     {230, 81, 99}, {251, 135, 97}, {254, 195, 135}, {252, 253, 191}},
    {{0, 34, 78}, {26, 56, 111}, {67, 78, 108}, {97, 101, 111}, {124, 123, 120},
     {155, 147, 118}, {187, 173, 108}, {221, 201, 88}, {254, 232, 56}}};

Color colormapColor(Colormap map, float share) noexcept
{
    if (map == Colormap::None || static_cast<uint8_t>(map) > 5u) return {};
    auto& stops = COLORMAP_STOPS[static_cast<uint8_t>(map) - 1u];
    auto t = std::isfinite(share) ? std::fmin(std::fmax(share, 0.0f), 1.0f) * 8.0f : 0.0f;
    auto index = std::min(static_cast<uint32_t>(t), 7u);
    auto u = t - static_cast<float>(index);
    Color color;
    uint8_t* channels[3] = {&color.r, &color.g, &color.b};
    for (auto i = 0u; i < 3u; i++) {
        auto from = static_cast<float>(stops[index][i]);
        auto to = static_cast<float>(stops[index + 1u][i]);
        *channels[i] = static_cast<uint8_t>(from + (to - from) * u + 0.5f);
    }
    return color;
}

float colormapShare(const SurfaceColormap& colormap, const Vec3& point) noexcept
{
    auto value = point.dot(colormap.axis);
    return (value - colormap.min) / (colormap.max - colormap.min);
}

SurfaceMesh* SurfaceMesh::gen(const Vec3* points, uint32_t columns,
                              uint32_t rows) noexcept
{
    if (!points || columns < 2u || rows < 2u || rows > 65536u / columns) return nullptr;
    auto object = new (std::nothrow) SurfaceMesh(points, columns, rows);
    if (!object || object->objPoints) return object;
    delete object;
    return nullptr;
}

const Vec3* SurfaceMesh::points() const noexcept
{
    return objPoints;
}

uint32_t SurfaceMesh::columns() const noexcept
{
    return objColumns;
}

uint32_t SurfaceMesh::rows() const noexcept
{
    return objRows;
}

Picture::Picture(const Vec3& center, float width) noexcept :
    Object(Type::Picture), center(center), width(width)
{
}

Picture::~Picture()
{
    delete[] objPixels;
    delete[] objEncoded;
    delete[] objMime;
}

Picture* Picture::gen(const Asset* asset, const Vec3& center, float width) noexcept
{
    if (!asset || !asset->width() || !asset->height() || !std::isfinite(width) || width <= 0.0f) return nullptr;
    auto object = new (std::nothrow) Picture(center, width);
    if (!object) return nullptr;
    object->objWidth = asset->width();
    object->objHeight = asset->height();
    if (asset->encoded() && asset->encodedSize() && asset->mime()) {
        object->objEncodedSize = asset->encodedSize();
        object->objEncoded = new (std::nothrow) uint8_t[object->objEncodedSize];
        object->objMime = _duplicate(asset->mime());
        if (object->objEncoded && object->objMime) {
            std::memcpy(object->objEncoded, asset->encoded(), object->objEncodedSize);
            return object;
        }
    } else if (asset->data() && object->objHeight <= std::numeric_limits<size_t>::max() / object->objWidth) {
        auto count = static_cast<size_t>(object->objWidth) * object->objHeight;
        object->objPixels = new (std::nothrow) uint32_t[count];
        if (object->objPixels) {
            std::memcpy(object->objPixels, asset->data(), count * sizeof(uint32_t));
            return object;
        }
    }
    delete object;
    return nullptr;
}

uint32_t Picture::pixelWidth() const noexcept
{
    return objWidth;
}

uint32_t Picture::pixelHeight() const noexcept
{
    return objHeight;
}

const uint32_t* Picture::pixels() const noexcept
{
    return objPixels;
}

const uint8_t* Picture::encoded() const noexcept
{
    return objEncoded;
}

uint32_t Picture::encodedSize() const noexcept
{
    return objEncodedSize;
}

const char* Picture::mime() const noexcept
{
    return objMime;
}

Cell::Cell(const Vec3& origin, uint32_t columns, uint32_t rows, Color color,
           CellMode mode, float padding) noexcept :
    Object(Type::Cell), origin(origin), mode(mode), padding(padding), objColumns(columns), objRows(rows)
{
    if (rows <= std::numeric_limits<size_t>::max() / columns) {
        auto count = static_cast<size_t>(columns) * rows;
        objColors = new (std::nothrow) Color[count];
        if (objColors) {
            for (auto i = 0u; i < count; i++)
                objColors[i] = color;
        }
    }
}

Cell::~Cell()
{
    delete[] objColors;
}

Cell* Cell::gen(const Vec3& origin, uint32_t columns, uint32_t rows, Color color,
                CellMode mode, float padding) noexcept
{
    if (!columns || !rows || rows > 16384u / columns || (mode != CellMode::Full && mode != CellMode::Padd) || !std::isfinite(padding) || padding < 0.0f || padding >= 0.5f) {
        return nullptr;
    }
    auto object = new (std::nothrow) Cell(origin, columns, rows, color, mode, padding);
    if (!object || object->objColors) return object;
    delete object;
    return nullptr;
}

Result Cell::frames(uint32_t count, float duration) noexcept
{
    if (!count || !std::isfinite(duration) || duration < 0.0f
        || (count == 1u) != (duration == 0.0f)) {
        return Result::InvalidArguments;
    }
    if (count == 1u) return Result::Success;
    auto size = static_cast<size_t>(objColumns) * objRows;
    if (size > std::numeric_limits<size_t>::max() / count) return Result::OutOfMemory;
    auto colors = new (std::nothrow) Color[size * count];
    if (!colors) return Result::OutOfMemory;
    delete[] objColors;
    objColors = colors;
    objFrames = count;
    objFrameDuration = duration;
    return Result::Success;
}

const Color* sampleColors(const Cell* cell, uint32_t frame) noexcept
{
    if (!cell || frame >= cell->objFrames) return nullptr;
    auto size = static_cast<size_t>(cell->objColumns) * cell->objRows;
    return cell->objColors + size * frame;
}

uint32_t sampleFrames(const Cell* cell) noexcept
{
    return cell ? cell->objFrames : 0u;
}

float sampleDuration(const Object* object) noexcept
{
    if (!object || object->type() != Type::Cell) return 0.0f;
    return static_cast<const Cell*>(object)->objFrameDuration;
}

uint32_t Cell::columns() const noexcept
{
    return objColumns;
}

uint32_t Cell::rows() const noexcept
{
    return objRows;
}

const Color* Cell::colors() const noexcept
{
    return objColors;
}

Result Cell::fill(const CellRegion& region, Color color) noexcept
{
    if (owned()) return Result::InsufficientCondition;
    if (!region.width || !region.height || region.x >= objColumns || region.y >= objRows || region.width > objColumns - region.x || region.height > objRows - region.y) {
        return Result::InvalidArguments;
    }
    for (auto y = region.y; y < region.y + region.height; y++) {
        for (auto x = region.x; x < region.x + region.width; x++)
            objColors[static_cast<size_t>(y) * objColumns + x] = color;
    }
    return Result::Success;
}

Result Cell::texture(const Asset* asset, const CellRegion& source,
                     const CellRegion& destination) noexcept
{
    if (owned()) return Result::InsufficientCondition;
    if (!asset || !asset->data() || !asset->width() || !asset->height()) return Result::InvalidArguments;
    auto src = source;
    auto dst = destination;
    if (!src.width) src.width = asset->width() - src.x;
    if (!src.height) src.height = asset->height() - src.y;
    if (!dst.width) dst.width = objColumns - dst.x;
    if (!dst.height) dst.height = objRows - dst.y;
    if (src.x >= asset->width() || src.y >= asset->height() || src.width > asset->width() - src.x || src.height > asset->height() - src.y || dst.x >= objColumns || dst.y >= objRows || dst.width > objColumns - dst.x || dst.height > objRows - dst.y || !src.width || !src.height || !dst.width || !dst.height) {
        return Result::InvalidArguments;
    }
    for (auto y = 0u; y < dst.height; y++) {
        auto sourceY = src.y + static_cast<uint32_t>((static_cast<uint64_t>(y) * src.height) / dst.height);
        auto targetY = dst.y + dst.height - y - 1u;
        for (auto x = 0u; x < dst.width; x++) {
            auto sourceX = src.x + static_cast<uint32_t>((static_cast<uint64_t>(x) * src.width) / dst.width);
            auto pixel = asset->data()[static_cast<size_t>(sourceY) * asset->width() + sourceX];
            objColors[static_cast<size_t>(targetY) * objColumns + dst.x + x] = {
                static_cast<uint8_t>(pixel), static_cast<uint8_t>(pixel >> 8u),
                static_cast<uint8_t>(pixel >> 16u), static_cast<uint8_t>(pixel >> 24u)};
        }
    }
    return Result::Success;
}

Rectangle::Rectangle(const Vec3& center, const Vec2& size, float corner) noexcept :
    Object(Type::Rectangle), center(center), size(size), corner(corner)
{
}

Rectangle* Rectangle::gen(const Vec3& center, const Vec2& size, float corner) noexcept
{
    if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0.0f || size.y <= 0.0f
        || !std::isfinite(corner) || corner < 0.0f || corner > std::fmin(size.x, size.y) * 0.5f) {
        return nullptr;
    }
    return new (std::nothrow) Rectangle(center, size, corner);
}

Connector::Connector(Object* from, Object* to) noexcept :
    Object(Type::Connector), source(from), destination(to)
{
    layer = -1;
}

Connector* Connector::gen(Object* from, Object* to) noexcept
{
    if (!from || !to || from == to) return nullptr;
    return new (std::nothrow) Connector(from, to);
}

Object* Connector::from() const noexcept
{
    return source;
}

Object* Connector::to() const noexcept
{
    return destination;
}

Vec3 Bounds::center() const noexcept
{
    return (min + max) * 0.5f;
}

Vec3 Bounds::size() const noexcept
{
    return max - min;
}

Vec2 BBox::center() const noexcept
{
    return {x + width * 0.5f, y + height * 0.5f};
}

Vec2 BBox::size() const noexcept
{
    return {width, height};
}

bool BBox::intersects(const BBox& other, float padding) const noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)
        || !std::isfinite(other.x) || !std::isfinite(other.y) || !std::isfinite(other.width)
        || !std::isfinite(other.height) || !std::isfinite(padding) || width <= 0.0f
        || height <= 0.0f || other.width <= 0.0f || other.height <= 0.0f || padding < 0.0f) {
        return false;
    }
    return x < other.x + other.width + padding && other.x < x + width + padding
        && y < other.y + other.height + padding && other.y < y + height + padding;
}

static void _include(Bounds& bounds, bool& defined, const Vec3& point)
{
    if (!defined) {
        bounds.min = point;
        bounds.max = point;
        defined = true;
        return;
    }
    bounds.min.x = std::fmin(bounds.min.x, point.x);
    bounds.min.y = std::fmin(bounds.min.y, point.y);
    bounds.min.z = std::fmin(bounds.min.z, point.z);
    bounds.max.x = std::fmax(bounds.max.x, point.x);
    bounds.max.y = std::fmax(bounds.max.y, point.y);
    bounds.max.z = std::fmax(bounds.max.z, point.z);
}

const WarpShape* warped(const Object* object) noexcept
{
    return object ? object->objWarp : nullptr;
}

const WarpShape* bornWarp(const Object* object) noexcept
{
    return object ? object->objBornWarp : nullptr;
}

bool anchor(const Object* object, Vec3& point) noexcept
{
    switch (object->type()) {
        case Type::Point: point = static_cast<const Point*>(object)->point; return true;
        case Type::Text: point = static_cast<const Text*>(object)->point; return true;
        case Type::Picture: point = static_cast<const Picture*>(object)->center; return true;
        default: return false;
    }
}

bool segment(const Object* object, const VisualState* state, Vec3* ends) noexcept
{
    Vec2 trimDistance;
    switch (object->type()) {
        case Type::Line: {
            auto line = static_cast<const Line*>(object);
            ends[0] = line->from;
            ends[1] = line->to;
            trimDistance = line->trimDistance;
            break;
        }
        case Type::Arrow: {
            auto arrow = static_cast<const Arrow*>(object);
            ends[0] = arrow->from;
            ends[1] = arrow->to;
            trimDistance = arrow->trimDistance;
            break;
        }
        case Type::Vector: {
            auto vector = static_cast<const Vector*>(object);
            ends[0] = vector->origin;
            ends[1] = vector->origin + vector->value;
            trimDistance = vector->trimDistance;
            break;
        }
        default: return false;
    }
    if (state) {
        trimDistance = state->trimDistance;
        if (state->geometry) {
            Vec3 points[2];
            shapeAt(*state, 2u, points);
            ends[0] = object->type() == Type::Vector ? points[1] : points[0];
            ends[1] = object->type() == Type::Vector ? points[1] + points[0] : points[1];
        }
    }
    auto delta = ends[1] - ends[0];
    auto length = delta.length();
    if (static_cast<double>(trimDistance.x) + trimDistance.y >= length) return false;
    if (trimDistance.x == 0.0f && trimDistance.y == 0.0f) return true;
    ends[0] = ends[0] + delta * (trimDistance.x / length);
    ends[1] = ends[1] - delta * (trimDistance.y / length);
    return true;
}

static bool _ownBounds(const Object* object, Bounds& bounds);

bool localBounds(const Object* object, Bounds& bounds) noexcept
{
    return localBounds(object, warped(object), 1.0f, bounds);
}

bool localBounds(const Object* object, const WarpShape* warp, float progress, Bounds& bounds) noexcept
{
    if (!warp) return _ownBounds(object, bounds);
    Vec3 point;
    if (anchor(object, point)) {
        if (!warp->count || !_ownBounds(object, bounds)) return false;
        auto moved = warp->at(0u, progress) - point;
        bounds.min = bounds.min + moved;
        bounds.max = bounds.max + moved;
        return true;
    }
    auto defined = false;
    auto begin = 0u;
    for (auto run = 0u; run < warp->runCount; run++) {
        auto marks = spaceMark(warp->runs[run].part);
        for (auto i = begin; i < warp->runs[run].end && !marks; i++)
            _include(bounds, defined, warp->at(i, progress));
        begin = warp->runs[run].end;
    }
    return defined;
}

static constexpr double ARC_TURN = 6.28318530717958647692;

ArcGeometry arcGeometry(const Object* object, const VisualState* state) noexcept
{
    ArcGeometry arc;
    if (object->type() == Type::Arc) {
        auto value = static_cast<const Arc*>(object);
        arc = {value->center, value->radius, value->start, value->angle, false, value->sector};
        if (state) {
            arc.radius = state->size.x;
            arc.start = state->arcStart;
            arc.angle = state->arcAngle;
            if (state->geometry) shapeAt(*state, 1u, &arc.center);
        }
        arc.start = static_cast<float>(std::remainder(static_cast<double>(arc.start), ARC_TURN));
    } else {
        auto value = static_cast<const Angle*>(object);
        Vec3 points[] = {value->vertex, value->from, value->to};
        if (state && state->geometry) shapeAt(*state, 3u, points);
        arc.center = points[0];
        arc.radius = state ? state->size.x : value->radius;
        auto a = points[1] - points[0];
        auto b = points[2] - points[0];
        if ((a.x == 0.0f && a.y == 0.0f) || (b.x == 0.0f && b.y == 0.0f)) { arc.radius = 0.0f; return arc; }
        auto first = std::atan2(static_cast<double>(a.y), a.x);
        auto second = std::atan2(static_cast<double>(b.y), b.x);
        auto sweep = std::fmod(second - first + 2.0 * ARC_TURN, ARC_TURN);
        if (sweep > ARC_TURN * 0.5) { first = second; sweep = ARC_TURN - sweep; }
        if (value->other) { first += sweep; sweep = ARC_TURN - sweep; }
        arc.start = static_cast<float>(first);
        arc.angle = static_cast<float>(sweep);
        arc.square = value->right == RightAngle::Always
                     || (value->right == RightAngle::Auto && std::fabs(sweep - ARC_TURN * 0.25) <= 1.0e-5);
    }
    return arc;
}

Vec3 angleLabelPoint(const Angle* angle, const VisualState* state) noexcept
{
    auto arc = arcGeometry(angle, state);
    if (arc.radius <= 0.0f) return arc.center;
    auto mid = arc.start + arc.angle * 0.5f;
    return arc.center + Vec3{std::cos(mid) * angle->labelDistance,
                             std::sin(mid) * angle->labelDistance, 0.0f};
}

uint32_t arcOutline(const ArcGeometry& arc, Vec3* points, uint32_t capacity) noexcept
{
    if (arc.angle == 0.0f || arc.radius <= 0.0f) return 0u;
    auto at = [&arc](float direction) {
        return arc.center + Vec3{std::cos(direction) * arc.radius, std::sin(direction) * arc.radius, 0.0f};
    };
    if (arc.square) {
        Vec3 corners[] = {at(arc.start), at(arc.start) + at(arc.start + arc.angle) - arc.center,
                          at(arc.start + arc.angle)};
        for (auto i = 0u; i < 3u && i < capacity; i++) points[i] = corners[i];
        return 3u;
    }
    auto segments = static_cast<uint32_t>(std::ceil(std::fabs(arc.angle) * 512.0 / ARC_TURN));
    segments = std::max(1u, std::min(512u, segments));
    auto count = segments + 1u + (arc.sector ? 1u : 0u);
    auto offset = arc.sector ? 1u : 0u;
    if (offset && capacity) points[0] = arc.center;
    for (auto i = 0u; i <= segments && i + offset < capacity; i++)
        points[i + offset] = at(arc.start + arc.angle * static_cast<float>(i) / segments);
    return count;
}

bool arcBounds(const ArcGeometry& arc, Bounds& bounds) noexcept
{
    auto defined = false;
    auto at = [&arc](float direction) {
        return arc.center + Vec3{std::cos(direction) * arc.radius, std::sin(direction) * arc.radius, 0.0f};
    };
    if (arc.square) {
        Vec3 points[3];
        auto count = arcOutline(arc, points, 3u);
        for (auto i = 0u; i < count; i++) _include(bounds, defined, points[i]);
        return defined;
    }
    _include(bounds, defined, at(arc.start));
    _include(bounds, defined, at(arc.start + arc.angle));
    if (arc.sector) _include(bounds, defined, arc.center);
    for (auto i = 0u; i < 4u; i++) {
        auto direction = static_cast<float>(i) * ARC_TURN * 0.25f;
        auto distance = std::fmod((arc.angle < 0.0f ? arc.start - direction : direction - arc.start), ARC_TURN);
        if (distance < 0.0f) distance += ARC_TURN;
        if (distance <= std::fabs(arc.angle)) _include(bounds, defined, at(direction));
    }
    return defined;
}

static bool _ownBounds(const Object* object, Bounds& bounds)
{
    auto defined = false;
    switch (object->type()) {
        case Type::Group:
        case Type::Connector: return false;
        case Type::Point: _include(bounds, defined, static_cast<const Point*>(object)->point); break;
        case Type::Line:
        case Type::Arrow:
        case Type::Vector: {
            Vec3 ends[2];
            if (!segment(object, nullptr, ends)) return false;
            _include(bounds, defined, ends[0]);
            _include(bounds, defined, ends[1]);
            break;
        }
        case Type::Arc:
        case Type::Angle: return arcBounds(arcGeometry(object), bounds);
        case Type::Circle: {
            auto circle = static_cast<const Circle*>(object);
            auto radius = Vec3{circle->radius, circle->radius, 0.0f};
            _include(bounds, defined, circle->center - radius);
            _include(bounds, defined, circle->center + radius);
            break;
        }
        case Type::Rectangle: {
            auto rectangle = static_cast<const Rectangle*>(object);
            auto half = Vec3{rectangle->size.x * 0.5f, rectangle->size.y * 0.5f, 0.0f};
            _include(bounds, defined, rectangle->center - half);
            _include(bounds, defined, rectangle->center + half);
            break;
        }
        case Type::Polygon: {
            auto polygon = static_cast<const Polygon*>(object);
            for (auto i = 0u; i < polygon->count(); i++)
                _include(bounds, defined, polygon->points()[i]);
            break;
        }
        case Type::Plot: {
            auto plot = static_cast<const Plot*>(object);
            for (auto i = 0u; i < plot->count(); i++)
                _include(bounds, defined, plot->points()[i]);
            break;
        }
        case Type::Route: {
            auto route = static_cast<const DirectedRoute*>(object);
            for (auto i = 0u; i < route->count(); i++)
                _include(bounds, defined, route->points()[i]);
            break;
        }
        case Type::Path: {
            auto path = static_cast<const Path*>(object);
            for (auto i = 0u; i < path->count(); i++)
                _include(bounds, defined, path->points()[i]);
            break;
        }
        case Type::Curve: {
            auto curve = static_cast<const Curve*>(object);
            for (auto i = 0u; i < curve->count(); i++)
                _include(bounds, defined, curve->points()[i]);
            break;
        }
        case Type::SurfaceMesh: {
            auto surface = static_cast<const SurfaceMesh*>(object);
            auto count = surface->columns() * surface->rows();
            for (auto i = 0u; i < count; i++)
                _include(bounds, defined, surface->points()[i]);
            break;
        }
        case Type::Text: {
            auto text = static_cast<const Text*>(object);
            if (text->box.x <= 0.0f && text->box.y <= 0.0f) {
                _include(bounds, defined, text->point);
                break;
            }
            auto left = text->point.x - text->align.x * text->box.x + text->boxOffset.x;
            auto top = text->point.y + text->align.y * text->box.y + text->boxOffset.y;
            _include(bounds, defined, {left, top - text->box.y, text->point.z});
            _include(bounds, defined, {left + text->box.x, top, text->point.z});
            break;
        }
        case Type::Space: {
            auto space = static_cast<const Space*>(object);
            _include(bounds, defined, {space->x.min, space->y.min, space->z.min});
            _include(bounds, defined, {space->x.max, space->y.max, space->z.max});
            break;
        }
        case Type::Picture: {
            auto image = static_cast<const Picture*>(object);
            auto height = image->width * static_cast<float>(image->pixelHeight()) / image->pixelWidth();
            auto half = Vec3{image->width * 0.5f, height * 0.5f, 0.0f};
            _include(bounds, defined, image->center - half);
            _include(bounds, defined, image->center + half);
            break;
        }
        case Type::Cell: {
            auto cell = static_cast<const Cell*>(object);
            _include(bounds, defined, cell->origin);
            _include(bounds, defined,
                     cell->origin + Vec3{static_cast<float>(cell->columns()), static_cast<float>(cell->rows()), cell->depth});
            break;
        }
    }
    return defined;
}

static void _transform(const Bounds& source, const Mat4& matrix, Bounds& output, bool& defined)
{
    for (auto x = 0u; x < 2u; x++) {
        for (auto y = 0u; y < 2u; y++) {
            for (auto z = 0u; z < 2u; z++) {
                _include(output, defined, matrix * Vec3{
                    x ? source.max.x : source.min.x,
                    y ? source.max.y : source.min.y,
                    z ? source.max.z : source.min.z});
            }
        }
    }
}

static bool _bounds(const Object* object, const Mat4& parent, Bounds& output, bool& defined)
{
    if (!valid(object)) return false;
    auto model = parent * object->model;
    Bounds local;
    if (localBounds(object, local)) _transform(local, model, output, defined);
    for (auto i = 0u; i < object->childCount(); i++) {
        auto childModel = model;
        if (object->type() == Type::Angle) childModel = model * Mat4::translate(static_cast<const Angle*>(object)->labelPoint());
        if (!_bounds(object->childAt(i), childModel, output, defined)) return false;
    }
    return true;
}

bool localFamilyBounds(const Object* object, Bounds& output) noexcept
{
    if (!valid(object)) return false;
    auto defined = false;
    Bounds local;
    if (localBounds(object, local)) {
        _transform(local, Mat4::identity(), output, defined);
    }
    auto childModel = object->type() == Type::Angle
                          ? Mat4::translate(static_cast<const Angle*>(object)->labelPoint()) : Mat4::identity();
    for (auto i = 0u; i < object->childCount(); i++) {
        if (!_bounds(object->childAt(i), childModel, output, defined)) {
            return false;
        }
    }
    return defined;
}

static uint32_t _outline(const Vec3* source, uint32_t count, Vec3* points, uint32_t capacity)
{
    for (auto i = 0u; i < count && i < capacity; i++)
        points[i] = source[i];
    return count;
}

uint32_t outline(const Object* object, Vec3* points, uint32_t capacity, bool& closed) noexcept
{
    closed = false;
    if (!object) return 0u;
    if (!points) capacity = 0u;
    // A warp leaves the images of its points; a space, a path of several contours, a plot of
    // several pieces and the objects it moves by one point still have no one outline.
    if (auto warp = warped(object)) {
        Vec3 point;
        if (object->type() == Type::Space || anchor(object, point) || warp->runCount != 1u) return 0u;
        closed = warp->closed;
        return _outline(warp->to, warp->count, points, capacity);
    }
    switch (object->type()) {
        case Type::Line:
        case Type::Arrow:
        case Type::Vector: {
            Vec3 ends[2];
            return segment(object, nullptr, ends) ? _outline(ends, 2u, points, capacity) : 0u;
        }
        case Type::Arc:
        case Type::Angle: {
            auto arc = arcGeometry(object);
            closed = arc.sector;
            return arcOutline(arc, points, capacity);
        }
        case Type::Circle: {
            // Same tessellation as the renderer: 64 points counterclockwise from +x.
            auto circle = static_cast<const Circle*>(object);
            constexpr auto count = 64u;
            for (auto i = 0u; i < count && i < capacity; i++) {
                auto angle = TWO_PI * static_cast<float>(i) / static_cast<float>(count);
                points[i] = circle->center + Vec3{std::cos(angle) * circle->radius,
                                                  std::sin(angle) * circle->radius, 0.0f};
            }
            closed = true;
            return count;
        }
        case Type::Rectangle: {
            auto rectangle = static_cast<const Rectangle*>(object);
            auto half = Vec2{rectangle->size.x * 0.5f, rectangle->size.y * 0.5f};
            closed = true;
            if (rectangle->corner <= 0.0f) {
                Vec3 corners[] = {
                    rectangle->center + Vec3{-half.x, -half.y, 0.0f},
                    rectangle->center + Vec3{half.x, -half.y, 0.0f},
                    rectangle->center + Vec3{half.x, half.y, 0.0f},
                    rectangle->center + Vec3{-half.x, half.y, 0.0f}};
                return _outline(corners, 4u, points, capacity);
            }
            constexpr auto segments = 8u;
            auto radius = rectangle->corner;
            Vec2 centers[4] = {
                {half.x - radius, half.y - radius},
                {-half.x + radius, half.y - radius},
                {-half.x + radius, -half.y + radius},
                {half.x - radius, -half.y + radius}};
            auto count = 0u;
            for (auto corner = 0u; corner < 4u; corner++) {
                for (auto j = 0u; j < segments; j++, count++) {
                    if (count >= capacity) continue;
                    auto angle = TWO_PI * (static_cast<float>(corner) * 0.25f
                                       + static_cast<float>(j) / (segments * 4.0f));
                    points[count] = rectangle->center + Vec3{
                        centers[corner].x + std::cos(angle) * radius,
                        centers[corner].y + std::sin(angle) * radius, 0.0f};
                }
            }
            return count;
        }
        case Type::Polygon: {
            auto polygon = static_cast<const Polygon*>(object);
            closed = true;
            return _outline(polygon->points(), polygon->count(), points, capacity);
        }
        case Type::Plot: {
            // A plot of several pieces has no one outline to follow or morph either.
            auto plot = static_cast<const Plot*>(object);
            if (plot->contours() > 1u) return 0u;
            return _outline(plot->points(), plot->count(), points, capacity);
        }
        case Type::Route: {
            auto route = static_cast<const DirectedRoute*>(object);
            return _outline(route->points(), route->count(), points, capacity);
        }
        case Type::Path: {
            // A path of several contours has no one outline to follow or morph.
            auto path = static_cast<const Path*>(object);
            closed = path->closed();
            if (path->contours() > 1u) return 0u;
            return _outline(path->points(), path->count(), points, capacity);
        }
        case Type::Curve: {
            auto curve = static_cast<const Curve*>(object);
            return _outline(curve->points(), curve->count(), points, capacity);
        }
        default: return 0u;
    }
}

bool geometric(const Object* object) noexcept
{
    if (!object) return false;
    switch (object->type()) {
        case Type::Arc:
        case Type::Angle:
        case Type::Line:
        case Type::Arrow:
        case Type::Vector:
        case Type::Curve:
        case Type::Polygon:
        case Type::Plot:
        case Type::Route: return true;
        case Type::Path: return static_cast<const Path*>(object)->contours() == 1u;
        default: return false;
    }
}

uint32_t shapeCount(const Object* object) noexcept
{
    if (!object) return 0u;
    switch (object->type()) {
        case Type::Line:
        case Type::Arrow:
        case Type::Vector: return 2u;
        case Type::Arc: return 1u;
        case Type::Angle: return 3u;
        case Type::Curve: return 4u;
        case Type::Polygon: return static_cast<const Polygon*>(object)->count();
        case Type::Plot: return static_cast<const Plot*>(object)->count();
        case Type::Route: return static_cast<const DirectedRoute*>(object)->count();
        case Type::Path: {
            auto path = static_cast<const Path*>(object);
            return path->contours() > 1u ? 0u : path->count();
        }
        default: return 0u;
    }
}

void shapeOf(const Object* object, Vec3* points) noexcept
{
    switch (object->type()) {
        case Type::Arc: points[0] = static_cast<const Arc*>(object)->center; break;
        case Type::Angle: {
            auto angle = static_cast<const Angle*>(object);
            points[0] = angle->vertex; points[1] = angle->from; points[2] = angle->to;
            break;
        }
        case Type::Line:
            points[0] = static_cast<const Line*>(object)->from;
            points[1] = static_cast<const Line*>(object)->to;
            break;
        case Type::Arrow:
            points[0] = static_cast<const Arrow*>(object)->from;
            points[1] = static_cast<const Arrow*>(object)->to;
            break;
        case Type::Vector:
            points[0] = static_cast<const Vector*>(object)->value;
            points[1] = static_cast<const Vector*>(object)->origin;
            break;
        case Type::Curve: std::memcpy(points, static_cast<const Curve*>(object)->controls(), sizeof(Vec3) * 4u); break;
        case Type::Polygon:
        case Type::Plot:
        case Type::Route:
        case Type::Path: {
            const Vec3* own = nullptr;
            if (object->type() == Type::Polygon) own = static_cast<const Polygon*>(object)->points();
            else if (object->type() == Type::Plot) own = static_cast<const Plot*>(object)->points();
            else if (object->type() == Type::Route) own = static_cast<const DirectedRoute*>(object)->points();
            else own = static_cast<const Path*>(object)->points();
            if (auto count = shapeCount(object)) std::memcpy(points, own, sizeof(Vec3) * count);
            break;
        }
        default: break;
    }
}

bool PlotSamples::valid() const noexcept
{
    auto& samples = *this;
    if (!samples.count) return true;
    if (!samples.points || samples.count < 2u || samples.count > 1000000u) return false;
    if (samples.band && (!std::isfinite(samples.yRange.x) || !std::isfinite(samples.yRange.y)
                         || samples.yRange.x >= samples.yRange.y)) return false;
    auto previous = -std::numeric_limits<float>::infinity();
    for (auto i = 0u; i < samples.count; i++) {
        auto& point = samples.points[i];
        if (!std::isfinite(point.x)) continue;
        if (point.x <= previous || !std::isfinite(point.z)) return false;
        previous = point.x;
    }
    return true;
}

bool sameGrid(const GeometryShape& from, const GeometryShape& to) noexcept
{
    auto& a = from.samples;
    auto& b = to.samples;
    if (!a.count || a.count != b.count || a.band != b.band) return false;
    if (a.band && (a.yRange.x != b.yRange.x || a.yRange.y != b.yRange.y)) return false;
    for (auto i = 0u; i < a.count; i++) {
        if (!std::isfinite(a.points[i].x) && !std::isfinite(b.points[i].x)) continue;
        if (a.points[i].x != b.points[i].x || a.points[i].z != b.points[i].z) return false;
    }
    return true;
}

static Vec3 _plotSamplePoint(const VisualState& state, uint32_t i)
{
    auto a = state.geometryFrom->samples.points[i];
    auto b = state.geometry->samples.points[i];
    auto t = state.geometryProgress;
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    a.y = static_cast<float>((1.0 - t) * a.y + static_cast<double>(t) * b.y);
    return a;
}

void plotSamplesAt(const VisualState& state, Vec3* points) noexcept
{
    for (auto i = 0u; i < state.geometry->samples.count; i++) points[i] = _plotSamplePoint(state, i);
}

GeometryShape plotAt(const VisualState& state, Vec3* points, uint32_t* ends) noexcept
{
    auto shape = *state.geometry;
    if (state.geometryFrom) {
        auto from = state.geometryFrom->contour.level;
        shape.contour.level = from + (shape.contour.level - from) * state.geometryProgress;
    }
    if (state.geometryFrom && sameGrid(*state.geometryFrom, shape) && state.geometryProgress <= 0.0f) {
        shape = *state.geometryFrom;
        if (points && shape.count) std::memcpy(points, shape.points, sizeof(Vec3) * shape.count);
        if (ends) {
            for (auto i = 0u; i < shape.pieces; i++) ends[i] = shape.pieces > 1u ? shape.ends[i] : shape.count;
        }
        return shape;
    }
    if (!state.geometryFrom || !sameGrid(*state.geometryFrom, shape)
        || state.geometryProgress <= 0.0f || state.geometryProgress >= 1.0f) {
        if (points) {
            auto from = state.geometryFrom ? state.geometryFrom->points : nullptr;
            for (auto i = 0u; i < shape.count; i++) {
                auto to = shape.points[i];
                points[i] = !from || state.geometryProgress == 1.0f ? to
                    : from[i] + (to - from[i]) * state.geometryProgress;
            }
        }
        if (ends) {
            for (auto i = 0u; i < shape.pieces; i++) ends[i] = shape.pieces > 1u ? shape.ends[i] : shape.count;
        }
        return shape;
    }
    GeometryShape output;
    output.points = points;
    output.ends = ends;
    output.pieces = 0;
    auto start = 0u;
    Vec3 last;
    auto end = [&]() {
        if (output.count - start < 2u) output.count = start;
        else {
            if (ends) ends[output.pieces] = output.count;
            output.pieces++;
        }
        start = output.count;
    };
    auto put = [&](const Vec3& point) {
        if (output.count > start && point.x == last.x && point.y == last.y && point.z == last.z) return;
        if (points) points[output.count] = point;
        last = point;
        output.count++;
    };
    auto level = [](const Vec3& a, const Vec3& b, double y) {
        auto t = (y - a.y) / (static_cast<double>(b.y) - a.y);
        return Vec3{static_cast<float>(a.x + (static_cast<double>(b.x) - a.x) * t),
                    static_cast<float>(y), static_cast<float>(a.z + (static_cast<double>(b.z) - a.z) * t)};
    };
    auto lo = shape.samples.band ? static_cast<double>(shape.samples.yRange.x) : -std::numeric_limits<double>::infinity();
    auto hi = shape.samples.band ? static_cast<double>(shape.samples.yRange.y) : std::numeric_limits<double>::infinity();
    Vec3 previous;
    auto running = false;
    for (auto i = 0u; i < shape.samples.count; i++) {
        auto point = _plotSamplePoint(state, i);
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            end();
            running = false;
            continue;
        }
        if (running) {
            auto a = previous, b = point;
            if ((a.y < lo && b.y < lo) || (a.y > hi && b.y > hi)) end();
            else {
                if (a.y < lo) a = level(previous, point, lo);
                else if (a.y > hi) a = level(previous, point, hi);
                if (b.y < lo) b = level(previous, point, lo);
                else if (b.y > hi) b = level(previous, point, hi);
                put(a);
                put(b);
                if (point.y < lo || point.y > hi) end();
            }
        }
        previous = point;
        running = true;
    }
    end();
    return output;
}

uint32_t drawnCount(const Object* object, const VisualState& state) noexcept
{
    if (object->type() == Type::Line || object->type() == Type::Arrow || object->type() == Type::Vector) {
        Vec3 ends[2];
        return segment(object, &state, ends) ? 2u : 0u;
    }
    if (object->type() == Type::Arc || object->type() == Type::Angle) return 0u;
    if (!state.geometry || !geometric(object)) return 0u;
    switch (object->type()) {
        case Type::Line:
        case Type::Arrow:
        case Type::Vector: return 2u;
        case Type::Curve: return static_cast<const Curve*>(object)->count();
        case Type::Plot: return plotAt(state).count;
        default: return state.geometry->count;
    }
}

// Point i of the geometry of state on its way; the ends of the way are exact.
static Vec3 _shapePoint(const VisualState& state, uint32_t i)
{
    auto to = state.geometry->points;
    auto from = state.geometryFrom ? state.geometryFrom->points : nullptr;
    auto progress = state.geometryProgress;
    if (!from || from == to || progress == 1.0f) return to[i];
    if (progress == 0.0f) return from[i];
    return from[i] + (to[i] - from[i]) * progress;
}

void shapeAt(const VisualState& state, uint32_t count, Vec3* points) noexcept
{
    for (auto i = 0u; i < count; i++) points[i] = _shapePoint(state, i);
}

void drawn(const Object* object, const VisualState& state, Vec3* points) noexcept
{
    auto at = [&state](uint32_t i) { return _shapePoint(state, i); };
    switch (object->type()) {
        case Type::Line:
        case Type::Arrow:
        case Type::Vector:
            segment(object, &state, points);
            break;
        case Type::Curve: {
            // A Bezier curve is linear in its controls, so its points travel as straight as theirs.
            Vec3 controls[] = {at(0u), at(1u), at(2u), at(3u)};
            auto count = static_cast<const Curve*>(object)->count();
            for (auto i = 0u; i < count; i++) points[i] = curvePoint(controls, i, count);
            break;
        }
        case Type::Plot: plotAt(state, points); break;
        default: {
            auto count = state.geometry->count;
            for (auto i = 0u; i < count; i++) points[i] = at(i);
            break;
        }
    }
}

bool Object::bounds(Bounds& output) const noexcept
{
    auto defined = false;
    return _bounds(this, Mat4::identity(), output, defined) && defined;
}

static constexpr auto LAYOUT_POINT = "the point must be finite";
static constexpr auto LAYOUT_BOUNDS = "the object has no bounds yet; give it geometry or children first";
static constexpr auto LAYOUT_REFERENCE = "the reference must be a different object with the same parent";
static constexpr auto LAYOUT_REFERENCE_BOUNDS = "the reference object has no bounds yet";
static constexpr auto LAYOUT_DIRECTION = "the direction must be a finite, non-zero vector";
static constexpr auto LAYOUT_GAP = "the gap must be zero or positive";
static constexpr auto LAYOUT_EMPTY = "the group has no children with bounds to arrange";
static constexpr auto LAYOUT_COLUMNS = "the grid needs at least one column";
static constexpr auto LAYOUT_MEMORY = "out of memory";

Result Object::fail(Result result, const char* reason) const noexcept
{
    if (objOwner && objOwner->pImpl) objOwner->pImpl->diagnostic = reason;
    return result;
}

Result Object::layout(const Mat4& value) noexcept
{
    auto previous = model;
    model = value;
    if (!objOwner) return Result::Success;
    auto result = objOwner->update(this);
    if (result == Result::Success) return result;
    model = previous;
    return result;
}

Result Object::moveTo(const Vec3& point) noexcept
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
        return fail(Result::InvalidArguments, LAYOUT_POINT);
    }
    Bounds box;
    if (!bounds(box)) return fail(Result::InsufficientCondition, LAYOUT_BOUNDS);
    return layout(Mat4::translate(point - box.center()) * model);
}

static float _support(const Bounds& bounds, const Vec3& direction)
{
    auto half = bounds.size() * 0.5f;
    return std::fabs(direction.x) * half.x + std::fabs(direction.y) * half.y + std::fabs(direction.z) * half.z;
}

Result Object::nextTo(const Object* target, const Vec3& direction, float gap) noexcept
{
    if (!target || target == this || target->objParent != objParent || target->objOwner != objOwner) {
        return fail(Result::InvalidArguments, LAYOUT_REFERENCE);
    }
    if (!std::isfinite(gap) || gap < 0.0f) return fail(Result::InvalidArguments, LAYOUT_GAP);
    auto length = direction.length();
    if (!std::isfinite(length) || length <= 1.0e-7f) return fail(Result::InvalidArguments, LAYOUT_DIRECTION);
    Bounds own;
    Bounds other;
    if (!bounds(own)) return fail(Result::InsufficientCondition, LAYOUT_BOUNDS);
    if (!target->bounds(other)) return fail(Result::InsufficientCondition, LAYOUT_REFERENCE_BOUNDS);
    auto axis = direction / length;
    auto point = other.center() + axis * (_support(other, axis) + gap + _support(own, axis));
    return layout(Mat4::translate(point - own.center()) * model);
}

Result Object::alignTo(const Object* target, const Vec3& direction) noexcept
{
    if (!target || target == this || target->objParent != objParent || target->objOwner != objOwner) {
        return fail(Result::InvalidArguments, LAYOUT_REFERENCE);
    }
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z)
        || (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f)) {
        return fail(Result::InvalidArguments, LAYOUT_DIRECTION);
    }
    Bounds own;
    Bounds other;
    if (!bounds(own)) return fail(Result::InsufficientCondition, LAYOUT_BOUNDS);
    if (!target->bounds(other)) return fail(Result::InsufficientCondition, LAYOUT_REFERENCE_BOUNDS);
    Vec3 shift;
    if (direction.x > 0.0f) shift.x = other.max.x - own.max.x;
    else if (direction.x < 0.0f) shift.x = other.min.x - own.min.x;
    if (direction.y > 0.0f) shift.y = other.max.y - own.max.y;
    else if (direction.y < 0.0f) shift.y = other.min.y - own.min.y;
    if (direction.z > 0.0f) shift.z = other.max.z - own.max.z;
    else if (direction.z < 0.0f) shift.z = other.min.z - own.min.z;
    return layout(Mat4::translate(shift) * model);
}

Result Group::arrange(const Vec3& direction, float gap) noexcept
{
    if (!std::isfinite(gap) || gap < 0.0f) return fail(Result::InvalidArguments, LAYOUT_GAP);
    auto length = direction.length();
    if (!std::isfinite(length) || length <= 1.0e-7f) return fail(Result::InvalidArguments, LAYOUT_DIRECTION);
    if (objOwner && objOwner->update(this) != Result::Success) return Result::InsufficientCondition;
    auto items = new (std::nothrow) Object*[childCnt];
    auto boxes = new (std::nothrow) Bounds[childCnt];
    auto positions = new (std::nothrow) float[childCnt];
    if ((!items || !boxes || !positions) && childCnt) {
        delete[] items;
        delete[] boxes;
        delete[] positions;
        return fail(Result::OutOfMemory, LAYOUT_MEMORY);
    }
    auto count = 0u;
    for (auto i = 0u; i < childCnt; i++) {
        Bounds box;
        if (!children[i]->bounds(box)) continue;
        items[count] = children[i];
        boxes[count++] = box;
    }
    if (!count) {
        delete[] items;
        delete[] boxes;
        delete[] positions;
        return fail(Result::InsufficientCondition, LAYOUT_EMPTY);
    }
    auto axis = direction / length;
    auto cursor = 0.0f;
    for (auto i = 0u; i < count; i++) {
        auto radius = _support(boxes[i], axis);
        positions[i] = cursor + radius;
        cursor += radius * 2.0f + gap;
    }
    auto total = cursor - gap;
    auto result = Result::Success;
    for (auto i = 0u; i < count; i++) {
        result = items[i]->moveTo(axis * (positions[i] - total * 0.5f));
        if (result != Result::Success) break;
    }
    delete[] items;
    delete[] boxes;
    delete[] positions;
    return result;
}

Result Group::arrangeGrid(uint32_t columns, float columnGap, float rowGap) noexcept
{
    if (!columns) return fail(Result::InvalidArguments, LAYOUT_COLUMNS);
    if (!std::isfinite(columnGap) || !std::isfinite(rowGap) || columnGap < 0.0f || rowGap < 0.0f) {
        return fail(Result::InvalidArguments, LAYOUT_GAP);
    }
    if (objOwner && objOwner->update(this) != Result::Success) return Result::InsufficientCondition;
    auto items = new (std::nothrow) Object*[childCnt];
    auto boxes = new (std::nothrow) Bounds[childCnt];
    if ((!items || !boxes) && childCnt) {
        delete[] items;
        delete[] boxes;
        return fail(Result::OutOfMemory, LAYOUT_MEMORY);
    }
    auto count = 0u;
    for (auto i = 0u; i < childCnt; i++) {
        Bounds box;
        if (!children[i]->bounds(box)) continue;
        items[count] = children[i];
        boxes[count++] = box;
    }
    if (!count) {
        delete[] items;
        delete[] boxes;
        return fail(Result::InsufficientCondition, LAYOUT_EMPTY);
    }
    if (columns > count) columns = count;
    auto rows = (count + columns - 1u) / columns;
    auto widths = new (std::nothrow) float[columns]();
    auto heights = new (std::nothrow) float[rows]();
    if (!widths || !heights) {
        delete[] items;
        delete[] boxes;
        delete[] widths;
        delete[] heights;
        return Result::OutOfMemory;
    }
    for (auto i = 0u; i < count; i++) {
        auto size = boxes[i].size();
        auto column = i % columns;
        auto row = i / columns;
        widths[column] = std::fmax(widths[column], size.x);
        heights[row] = std::fmax(heights[row], size.y);
    }
    auto width = columnGap * (columns - 1u);
    auto height = rowGap * (rows - 1u);
    for (auto i = 0u; i < columns; i++) width += widths[i];
    for (auto i = 0u; i < rows; i++) height += heights[i];
    auto result = Result::Success;
    auto y = height * 0.5f;
    for (auto row = 0u; row < rows && result == Result::Success; row++) {
        auto x = -width * 0.5f;
        for (auto column = 0u; column < columns; column++) {
            auto index = row * columns + column;
            if (index < count) {
                auto point = Vec3{x + widths[column] * 0.5f, y - heights[row] * 0.5f, 0.0f};
                result = items[index]->moveTo(point);
                if (result != Result::Success) break;
            }
            x += widths[column] + columnGap;
        }
        y -= heights[row] + rowGap;
    }
    delete[] items;
    delete[] boxes;
    delete[] widths;
    delete[] heights;
    return result;
}

}  // namespace vis
