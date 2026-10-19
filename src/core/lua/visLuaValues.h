// Reads Lua values and table fields for the bindings: numbers and vectors with their checks,
// option lists, colors and points. The core binding and the optional modules share these;
// a module keeps only the readers of shapes it alone accepts.
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

namespace vis
{

static inline float _number(lua_State* lua, int index, const char* name)
{
    auto value = luaL_checknumber(lua, index);
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
    luaL_checktype(lua, index, LUA_TTABLE);
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
    luaL_checktype(lua, index, LUA_TTABLE);
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

static inline Range _range(lua_State* lua, int index, const char* name)
{
    luaL_checktype(lua, index, LUA_TTABLE);
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
    luaL_checktype(lua, index, LUA_TTABLE);
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

static inline void _size(lua_State* lua, int index, uint32_t& columns, uint32_t& rows)
{
    luaL_checktype(lua, index, LUA_TTABLE);
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
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    if (lua_rawlen(lua, index) != count) luaL_error(lua, "%s must contain exactly %u numbers", name, count);
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
    auto value = luaL_checklstring(lua, -1, &length);
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
    auto value = luaL_checklstring(lua, -1, &length);
    if (std::strlen(value) != length) luaL_error(lua, "field '%s' cannot contain zero bytes", name);
    return value;
}

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

static inline bool _themeColorRole(const char* value, size_t size, ThemeColorRole& role)
{
    struct Entry { const char* name; ThemeColorRole role; };
    static constexpr Entry entries[] = {
        {"background", ThemeColorRole::Background},
        {"foreground", ThemeColorRole::Foreground},
        {"muted", ThemeColorRole::Muted},
        {"accent", ThemeColorRole::Accent},
        {"secondary", ThemeColorRole::Secondary},
        {"success", ThemeColorRole::Success},
        {"warning", ThemeColorRole::Warning},
        {"danger", ThemeColorRole::Danger},
        {"info", ThemeColorRole::Info},
        {"surface", ThemeColorRole::Surface},
        {"border", ThemeColorRole::Border},
        {"result", ThemeColorRole::Result},
        {"focus", ThemeColorRole::Focus},
    };
    for (const auto& entry : entries) {
        if (std::strlen(entry.name) == size && std::memcmp(value, entry.name, size) == 0) {
            role = entry.role;
            return true;
        }
    }
    return false;
}

static inline Color _color(lua_State* lua, int index, const char* name, const Theme* theme = nullptr)
{
    size_t size;
    auto value = luaL_checklstring(lua, index, &size);
    if (_hex(value, size)) return Color::hex(value);
    ThemeColorRole role;
    if (theme && std::strlen(value) == size && _themeColorRole(value, size, role)) {
        return theme->color(role);
    }
    luaL_error(lua, "%s must be a hex color or Theme color role", name);
    return {};
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
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    auto size = lua_rawlen(lua, index);
    if (size < minimum) luaL_error(lua, "points must contain at least %u vectors", minimum);
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

}  // namespace vis

#endif
