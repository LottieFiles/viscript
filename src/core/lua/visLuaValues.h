// Reads Lua values and table fields for the bindings: numbers and vectors with their checks,
// option lists, colors, points, regions and objects. The core binding and the optional modules
// share these; a module keeps only the readers of shapes it alone accepts.
#ifndef _VIS_LUA_VALUES_H_
#define _VIS_LUA_VALUES_H_

#include <cfloat>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <new>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "vis.h"
#include "visLuaBudget.h"
#include "visLuaExtension.h"

namespace vis
{

// Stops the script because the value at index is not of type expected. An argument is named by
// its number, as luaL_checktype names it; a value pushed from a field or a list is named by name,
// since its place on the stack means nothing to the script.
static inline void _typeError(lua_State* lua, int index, const char* expected, const char* name)
{
    if (index > 0) luaL_typeerror(lua, index, expected);
    luaL_error(lua, "%s must be a %s, got %s", name, expected, luaL_typename(lua, index));
}

// luaL_checknumber, luaL_checklstring and luaL_checktype for a table, naming the value as
// _typeError does.
static inline lua_Number _checkNumber(lua_State* lua, int index, const char* name)
{
    int number = 0;
    auto value = lua_tonumberx(lua, index, &number);
    if (!number) _typeError(lua, index, "number", name);
    return value;
}

static inline const char* _checkString(lua_State* lua, int index, const char* name, size_t* size = nullptr)
{
    auto value = lua_tolstring(lua, index, size);
    if (!value) _typeError(lua, index, "string", name);
    return value;
}

static inline void _checkTable(lua_State* lua, int index, const char* name)
{
    if (!lua_istable(lua, index)) _typeError(lua, index, "table", name);
}

static inline float _number(lua_State* lua, int index, const char* name)
{
    auto value = _checkNumber(lua, index, name);
    if (!std::isfinite(value) || value < -FLT_MAX || value > FLT_MAX) {
        luaL_error(lua, "%s must be a finite number", name);
    }
    return static_cast<float>(value);
}

template<auto Read, typename Value>
static inline Value _field(lua_State* lua, int index, const char* name, const Value& fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    auto value = lua_isnil(lua, -1) ? fallback : Read(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

static inline float _fieldNumber(lua_State* lua, int index, const char* name, float fallback)
{
    return _field<_number>(lua, index, name, fallback);
}

static inline bool _fieldBool(lua_State* lua, int index, const char* name, bool fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    if (!lua_isboolean(lua, -1)) luaL_error(lua, "%s must be a boolean", name);
    auto value = static_cast<bool>(lua_toboolean(lua, -1));
    lua_pop(lua, 1);
    return value;
}

static inline lua_Integer _integer(lua_State* lua, int index, const char* name)
{
    if (!lua_isinteger(lua, index)) luaL_error(lua, "%s must be an integer", name);
    return lua_tointeger(lua, index);
}

static inline lua_Integer _fieldInteger(lua_State* lua, int index, const char* name, lua_Integer fallback)
{
    return _field<_integer>(lua, index, name, fallback);
}

static inline bool _has(lua_State* lua, int index, const char* name)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    auto found = !lua_isnil(lua, -1);
    lua_pop(lua, 1);
    return found;
}

static inline float _component(lua_State* lua, int index, const char* key, int item)
{
    index = lua_absindex(lua, index);
    if (key) lua_getfield(lua, index, key);
    else lua_rawgeti(lua, index, item);
    auto value = _number(lua, -1, key ? key : "vector component");
    lua_pop(lua, 1);
    return value;
}

static inline Vec2 _vec2(lua_State* lua, int index, const char* name)
{
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    auto keyed = _has(lua, index, "x") || _has(lua, index, "y") || _has(lua, index, "z");
    if (keyed) {
        if (!_has(lua, index, "x") || !_has(lua, index, "y") || _has(lua, index, "z")) {
            luaL_error(lua, "%s must be a 2D vector with x and y", name);
        }
        return {_component(lua, index, "x", 0), _component(lua, index, "y", 0)};
    }
    if (lua_rawlen(lua, index) != 2) luaL_error(lua, "%s must contain exactly 2 numbers", name);
    return {_component(lua, index, nullptr, 1), _component(lua, index, nullptr, 2)};
}

static inline Vec3 _world(lua_State* lua, int index, const char* name)
{
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    auto keyed = _has(lua, index, "x") || _has(lua, index, "y") || _has(lua, index, "z");
    if (keyed) {
        if (!_has(lua, index, "x") || !_has(lua, index, "y")) {
            luaL_error(lua, "%s must contain x and y, with optional z", name);
        }
        auto z = _has(lua, index, "z") ? _component(lua, index, "z", 0) : 0.0f;
        return {_component(lua, index, "x", 0), _component(lua, index, "y", 0), z};
    }
    auto size = lua_rawlen(lua, index);
    if (size != 2 && size != 3) luaL_error(lua, "%s must contain 2 or 3 numbers", name);
    return {_component(lua, index, nullptr, 1), _component(lua, index, nullptr, 2),
            size == 3 ? _component(lua, index, nullptr, 3) : 0.0f};
}

static inline Vec2 _fieldVec2(lua_State* lua, int index, const char* name, const Vec2& fallback)
{
    return _field<_vec2>(lua, index, name, fallback);
}

static inline Vec3 _fieldWorld(lua_State* lua, int index, const char* name)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) luaL_error(lua, "missing field '%s'", name);
    auto value = _world(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

static inline Vec3 _fieldWorld(lua_State* lua, int index, const char* name, const Vec3& fallback)
{
    return _field<_world>(lua, index, name, fallback);
}

// A vector as the modules take it: x, y and z keys, or two or three numbers with z for a missing
// third.
static inline Vec3 _vec3(lua_State* lua, int index, const char* name, float z = 0.0f)
{
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    auto keyed = _has(lua, index, "x") || _has(lua, index, "y") || _has(lua, index, "z");
    if (keyed && (!_has(lua, index, "x") || !_has(lua, index, "y") || !_has(lua, index, "z"))) {
        luaL_error(lua, "%s must contain x, y and z", name);
    }
    auto count = keyed ? 3u : lua_rawlen(lua, index);
    if (count != 2u && count != 3u) luaL_error(lua, "%s must contain two or three numbers", name);
    Vec3 value = {0.0f, 0.0f, z};
    const char* keys[] = {"x", "y", "z"};
    float* components[] = {&value.x, &value.y, &value.z};
    for (auto i = 0u; i < count; i++) {
        if (keyed) lua_getfield(lua, index, keys[i]);
        else lua_rawgeti(lua, index, i + 1);
        *components[i] = _number(lua, -1, name);
        lua_pop(lua, 1);
    }
    return value;
}

// The vector field name read as _vec3 reads it, or fallback without it; two numbers keep the z of
// fallback.
static inline Vec3 _fieldVec3(lua_State* lua, int index, const char* name, const Vec3& fallback = {})
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    auto value = lua_isnil(lua, -1) ? fallback : _vec3(lua, -1, name, fallback.z);
    lua_pop(lua, 1);
    return value;
}

// Reads the origin, shift, rotation, scale, opacity and progress of a states or runtime overlay
// table into state, which keeps what the table leaves out: an origin sets originEnabled, and a
// number as rotation turns about z.
template<typename State>
static inline void _overlay(lua_State* lua, int index, State& state)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "origin");
    if (!lua_isnil(lua, -1)) {
        state.origin = _vec3(lua, -1, "origin");
        state.originEnabled = true;
    }
    lua_pop(lua, 1);
    state.shift = _fieldVec3(lua, index, "shift", state.shift);
    state.scale = _fieldVec3(lua, index, "scale", state.scale);
    lua_getfield(lua, index, "rotation");
    if (!lua_isnil(lua, -1)) {
        if (lua_isnumber(lua, -1)) state.rotation.z = _number(lua, -1, "rotation");
        else state.rotation = _vec3(lua, -1, "rotation");
    }
    lua_pop(lua, 1);
    state.opacity = _fieldNumber(lua, index, "opacity", state.opacity);
    state.progress = _fieldNumber(lua, index, "progress", state.progress);
}

static inline Range _range(lua_State* lua, int index, const char* name)
{
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    Range range;
    auto keyed = _has(lua, index, "min") || _has(lua, index, "max") || _has(lua, index, "step");
    if (keyed) {
        if (!_has(lua, index, "min") || !_has(lua, index, "max") || !_has(lua, index, "step")) {
            luaL_error(lua, "%s must have min, max and step", name);
        }
        range.min = _component(lua, index, "min", 0);
        range.max = _component(lua, index, "max", 0);
        range.step = _component(lua, index, "step", 0);
    } else {
        if (lua_rawlen(lua, index) != 3) luaL_error(lua, "%s must contain min, max and step", name);
        range.min = _component(lua, index, nullptr, 1);
        range.max = _component(lua, index, nullptr, 2);
        range.step = _component(lua, index, nullptr, 3);
    }
    if (range.min > range.max || range.step <= 0.0f) luaL_error(lua, "%s is invalid", name);
    return range;
}

static inline Range _fieldRange(lua_State* lua, int index, const char* name, const Range& fallback)
{
    return _field<_range>(lua, index, name, fallback);
}

static inline CellRegion _region(lua_State* lua, int index, const char* name)
{
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    auto keyed = _has(lua, index, "x") || _has(lua, index, "y") || _has(lua, index, "width") || _has(lua, index, "height");
    lua_Integer values[4];
    if (keyed) {
        const char* fields[] = {"x", "y", "width", "height"};
        for (auto i = 0u; i < 4u; i++) {
            if (!_has(lua, index, fields[i])) luaL_error(lua, "%s requires x, y, width and height", name);
            values[i] = _fieldInteger(lua, index, fields[i], 0);
        }
    } else {
        if (lua_rawlen(lua, index) != 4u) luaL_error(lua, "%s must contain x, y, width and height", name);
        for (auto i = 0u; i < 4u; i++) {
            lua_rawgeti(lua, index, i + 1u);
            values[i] = _integer(lua, -1, name);
            lua_pop(lua, 1);
        }
    }
    for (auto value : values) {
        if (value < 0 || value > UINT32_MAX) luaL_error(lua, "%s values must be unsigned 32-bit integers", name);
    }
    return {static_cast<uint32_t>(values[0]), static_cast<uint32_t>(values[1]),
            static_cast<uint32_t>(values[2]), static_cast<uint32_t>(values[3])};
}

static inline CellRegion _fieldRegion(lua_State* lua, int index, const char* name)
{
    return _field<_region>(lua, index, name, CellRegion{});
}

// The region field in which a UI control or an input follow takes the pointer: x, y, width and
// height keys or four numbers, of a positive size.
static inline BBox _fieldPointerRegion(lua_State* lua, int index)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "region");
    if (lua_isnil(lua, -1)) luaL_error(lua, "missing field 'region'");
    _checkTable(lua, -1, "region");
    auto region = lua_absindex(lua, -1);
    auto keyed = _has(lua, region, "x") || _has(lua, region, "y") || _has(lua, region, "width") || _has(lua, region, "height");
    if (!keyed && lua_rawlen(lua, region) != 4u) luaL_error(lua, "region must contain x, y, width and height");
    BBox bounds;
    const char* keys[] = {"x", "y", "width", "height"};
    float* values[] = {&bounds.x, &bounds.y, &bounds.width, &bounds.height};
    for (auto i = 0u; i < 4u; i++) {
        if (keyed) {
            lua_getfield(lua, region, keys[i]);
            if (lua_isnil(lua, -1)) luaL_error(lua, "region requires %s", keys[i]);
        } else {
            lua_rawgeti(lua, region, i + 1u);
        }
        *values[i] = _number(lua, -1, keyed ? keys[i] : "region component");
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) luaL_error(lua, "region width and height must be positive");
    return bounds;
}

static inline void _size(lua_State* lua, int index, uint32_t& columns, uint32_t& rows)
{
    _checkTable(lua, index, "size");
    if (lua_rawlen(lua, index) != 2u) luaL_error(lua, "size must contain columns and rows");
    lua_rawgeti(lua, index, 1);
    auto width = _integer(lua, -1, "columns");
    lua_pop(lua, 1);
    lua_rawgeti(lua, index, 2);
    auto height = _integer(lua, -1, "rows");
    lua_pop(lua, 1);
    if (width <= 0 || width > UINT32_MAX || height <= 0 || height > UINT32_MAX || height > 16384 / width) {
        luaL_error(lua, "size must contain 1 to 16384 cells");
    }
    columns = static_cast<uint32_t>(width);
    rows = static_cast<uint32_t>(height);
}

static inline void _matrix(lua_State* lua, int index, float* values, uint32_t count, const char* name)
{
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    if (lua_rawlen(lua, index) != count) {
        luaL_error(lua, "%s must contain exactly %d numbers", name, static_cast<int>(count));
    }
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, index, i + 1);
        values[i] = _number(lua, -1, name);
        lua_pop(lua, 1);
    }
}

static inline const char* _fieldString(lua_State* lua, int index, const char* name, size_t* size = nullptr)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) luaL_error(lua, "missing field '%s'", name);
    size_t length;
    auto value = _checkString(lua, -1, name, &length);
    if (std::strlen(value) != length) luaL_error(lua, "field '%s' cannot contain zero bytes", name);
    if (size) *size = length;
    return value;
}

static inline const char* _fieldOptionalString(lua_State* lua, int index, const char* name, const char* fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        lua_pushstring(lua, fallback);
        return lua_tostring(lua, -1);
    }
    size_t length;
    auto value = _checkString(lua, -1, name, &length);
    if (std::strlen(value) != length) luaL_error(lua, "field '%s' cannot contain zero bytes", name);
    return value;
}

// The object of field name, a handle of scene.
static inline Object* _fieldObject(lua_State* lua, int index, const char* name, Scene* scene)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) luaL_error(lua, "missing field '%s'", name);
    auto object = detail::luaObject(lua, scene, -1);
    lua_pop(lua, 1);
    return object;
}

// A name a string option takes and the value it stands for.
template<typename Value>
struct LuaName
{
    const char* name;
    Value value;
};

// The entry of names that the size bytes at value spell, or nullptr.
template<typename Value, size_t N>
static inline const LuaName<Value>* _findName(const char* value, size_t size, const LuaName<Value> (&names)[N])
{
    for (auto& name : names) {
        if (std::strlen(name.name) == size && std::memcmp(name.name, value, size) == 0) return &name;
    }
    return nullptr;
}

// The value of the string at index among names; any other string stops the script with
// "what must be 'a', 'b' or 'c'", listing the names in order.
template<typename Value, size_t N>
static inline Value _enum(lua_State* lua, int index, const char* what, const LuaName<Value> (&names)[N])
{
    size_t size;
    auto value = _checkString(lua, index, what, &size);
    if (auto found = _findName(value, size, names)) return found->value;
    luaL_where(lua, 1);
    lua_pushfstring(lua, "%s must be '%s'", what, names[0].name);
    for (auto i = size_t{1}; i < N; i++) {
        lua_pushfstring(lua, i + 1u < N ? ", '%s'" : " or '%s'", names[i].name);
        lua_concat(lua, 2);
    }
    lua_concat(lua, 2);
    lua_error(lua);
    return names[0].value;
}

// The value of the string field name of the table at index among names, or fallback without
// the field; what names the option in the message, the field itself unless given.
template<typename Value, size_t N>
static inline Value _fieldEnum(lua_State* lua, int index, const char* name, Value fallback,
                               const LuaName<Value> (&names)[N], const char* what = nullptr)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (!lua_isnil(lua, -1)) fallback = _enum(lua, -1, what ? what : name, names);
    lua_pop(lua, 1);
    return fallback;
}

// The views a look table, the camera of vis.scene and a UI panel's camera_view name.
static constexpr LuaName<CameraView> CAMERA_VIEWS[] = {{"2d", CameraView::TwoD}, {"3d", CameraView::ThreeD}};
// The roles a text, a part of a spans row and a tex formula take, and the orientations of a text
// and a formula.
static constexpr LuaName<TextRole> TEXT_ROLES[] = {{"h1", TextRole::H1}, {"h2", TextRole::H2}, {"h3", TextRole::H3},
                                                   {"text", TextRole::Text}, {"code", TextRole::Code}};
static constexpr LuaName<TextOrientation> TEXT_ORIENTATIONS[] = {
    {"billboard", TextOrientation::Billboard}, {"plane", TextOrientation::Plane}};

static inline bool _option(const char* name, std::initializer_list<const char*> fields, bool style)
{
    if (style) {
        static constexpr const char* shared[] = {
            "id", "color", "stroke", "fill", "gradient", "width", "radius", "dash",
            "dash_offset", "opacity", "progress", "layer", "isolate", "mask", "blend", "blur", "shadow"};
        for (auto field : shared) {
            if (std::strcmp(name, field) == 0) return true;
        }
    }
    for (auto field : fields) {
        if (std::strcmp(name, field) == 0) return true;
    }
    return false;
}

static inline void _options(lua_State* lua, int index, std::initializer_list<const char*> fields,
                     bool style = true)
{
    index = lua_absindex(lua, index);
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "option names must be strings");
        auto name = lua_tostring(lua, -2);
        if (!_option(name, fields, style)) luaL_error(lua, "unknown option '%s'", name);
        lua_pop(lua, 1);
    }
}

// Raises an error for a key of the table at index that is not in names, a nullptr-terminated
// list; kind names the table in the message, as "unknown diagram node field 'x'".
static inline void _options(lua_State* lua, int index, const char* kind, const char* const* names)
{
    index = lua_absindex(lua, index);
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "%s keys must be strings", kind);
        auto key = lua_tostring(lua, -2);
        auto known = false;
        for (auto cursor = names; *cursor; cursor++) {
            if (std::strcmp(key, *cursor) == 0) {
                known = true;
                break;
            }
        }
        if (!known) luaL_error(lua, "unknown %s field '%s'", kind, key);
        lua_pop(lua, 1);
    }
}

static inline bool _hex(const char* value, size_t size)
{
    if (size && *value == '#') {
        value++;
        size--;
    }
    if (size != 3 && size != 4 && size != 6 && size != 8) return false;
    for (auto i = 0u; i < size; i++) {
        auto c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

// The roles of a theme that a color may name in place of a hex value.
static constexpr LuaName<ThemeColorRole> THEME_COLOR_ROLES[] = {
    {"background", ThemeColorRole::Background}, {"foreground", ThemeColorRole::Foreground},
    {"muted", ThemeColorRole::Muted}, {"accent", ThemeColorRole::Accent}, {"secondary", ThemeColorRole::Secondary},
    {"success", ThemeColorRole::Success}, {"warning", ThemeColorRole::Warning}, {"danger", ThemeColorRole::Danger},
    {"info", ThemeColorRole::Info}, {"surface", ThemeColorRole::Surface}, {"border", ThemeColorRole::Border},
    {"result", ThemeColorRole::Result}, {"focus", ThemeColorRole::Focus}};

// Reads the size bytes at value, a hex color or, with theme, the name of one of its roles, into
// color; false for any other string.
static inline bool _colorName(const char* value, size_t size, const Theme* theme, Color& color)
{
    if (_hex(value, size)) {
        color = Color::hex(value);
        return true;
    }
    auto role = theme ? _findName(value, size, THEME_COLOR_ROLES) : nullptr;
    if (role) color = theme->color(role->value);
    return role != nullptr;
}

// The color the string at index names; without a theme to take roles from, only a hex color.
static inline Color _color(lua_State* lua, int index, const char* name, const Theme* theme = nullptr)
{
    size_t size;
    auto value = _checkString(lua, index, name, &size);
    Color color;
    if (!_colorName(value, size, theme, color)) {
        luaL_error(lua, theme ? "%s must be a hex color or Theme color role" : "%s must be a hex color", name);
    }
    return color;
}

static inline bool _fieldColor(lua_State* lua, int index, const char* name, Color& color,
                        const Theme* theme = nullptr)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return false;
    }
    color = _color(lua, -1, name, theme);
    lua_pop(lua, 1);
    return true;
}

static inline Vec3* _points(lua_State* lua, int index, uint32_t minimum, uint32_t& count)
{
    _checkTable(lua, index, "points");
    index = lua_absindex(lua, index);
    auto size = lua_rawlen(lua, index);
    if (size < minimum) luaL_error(lua, "points must contain at least %d vectors", static_cast<int>(minimum));
    if (size > UINT32_MAX || size > LUA_MEMORY_LIMIT / sizeof(Vec3)) luaL_error(lua, "points array is too large");
    count = static_cast<uint32_t>(size);
    auto points = static_cast<Vec3*>(lua_newuserdatauv(lua, sizeof(Vec3) * size, 0));
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, index, i + 1);
        auto point = _world(lua, -1, "point");
        new (points + i) Vec3(point);
        lua_pop(lua, 1);
    }
    return points;
}

// Ends a method of a module's handle: stops the script with the message of a result other than
// Success, or returns the handle at 1.
static inline int _result(lua_State* lua, Result result)
{
    if (result != Result::Success) luaL_error(lua, "%s", vis::result(result));
    lua_settop(lua, 1);
    return 1;
}

}  // namespace vis

#endif
