// Reads styles from Lua tables for the binding: gradients, blur, shadow, blend mode and text
// options.
#ifndef _VIS_LUA_STYLE_H_
#define _VIS_LUA_STYLE_H_

#include "visLuaValues.h"

namespace vis
{

static bool _gradientShape(Type type)
{
    return type != Type::Space && type != Type::Text && type != Type::Picture
           && type != Type::Cell && type != Type::Group;
}

static void _gradientStops(lua_State* lua, int index, Gradient& gradient, const Theme* theme)
{
    lua_getfield(lua, index, "stops");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return;
    }
    if (!lua_istable(lua, -1)) luaL_error(lua, "gradient.stops must be an array of colors or {offset, color} pairs");
    auto stops = lua_absindex(lua, -1);
    auto count = lua_rawlen(lua, stops);
    lua_pushnil(lua);
    while (lua_next(lua, stops)) {
        if (!lua_isinteger(lua, -2) || lua_tointeger(lua, -2) < 1
            || static_cast<size_t>(lua_tointeger(lua, -2)) > count) {
            luaL_error(lua, "gradient.stops must be a sequence");
        }
        lua_pop(lua, 1);
    }
    if (count < 2 || count > Gradient::StopLimit) {
        luaL_error(lua, "gradient.stops must contain 2 to %d stops", static_cast<int>(Gradient::StopLimit));
    }
    auto previous = 0.0f;
    for (auto i = 1u; i <= count; i++) {
        auto& stop = gradient.stops[i - 1u];
        lua_rawgeti(lua, stops, static_cast<lua_Integer>(i));
        if (lua_type(lua, -1) == LUA_TSTRING) {
            stop.offset = static_cast<float>(i - 1u) / static_cast<float>(count - 1u);
            stop.color = _color(lua, -1, "gradient stop color", theme);
        } else if (lua_istable(lua, -1)) {
            auto pair = lua_absindex(lua, -1);
            if (lua_rawlen(lua, pair) != 2) {
                luaL_error(lua, "gradient stop %d must be a color or {offset, color}", static_cast<int>(i));
            }
            lua_rawgeti(lua, pair, 1);
            stop.offset = _number(lua, -1, "gradient stop offset");
            lua_pop(lua, 1);
            lua_rawgeti(lua, pair, 2);
            stop.color = _color(lua, -1, "gradient stop color", theme);
            lua_pop(lua, 1);
            if (stop.offset < 0.0f || stop.offset > 1.0f) {
                luaL_error(lua, "gradient stop %d offset must be within 0..1", static_cast<int>(i));
            }
            if (stop.offset < previous) {
                luaL_error(lua, "gradient stop offsets must be non-decreasing");
            }
        } else {
            luaL_error(lua, "gradient stop %d must be a color or {offset, color}", static_cast<int>(i));
        }
        previous = stop.offset;
        lua_pop(lua, 1);
    }
    gradient.stopCount = static_cast<uint8_t>(count);
    lua_pop(lua, 1);
}

static Gradient _gradient(lua_State* lua, int index, const Theme* theme)
{
    static constexpr LuaName<GradientType> TYPES[] = {
        {"linear", GradientType::Linear}, {"radial", GradientType::Radial}, {"conic", GradientType::Conic}};
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "type");
    if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "gradient.type must be \"linear\", \"radial\" or \"conic\"");
    size_t size;
    auto name = lua_tolstring(lua, -1, &size);
    auto type = _findName(name, size, TYPES);
    if (!type) luaL_error(lua, "gradient.type must be \"linear\", \"radial\" or \"conic\", got '%s'", name);
    lua_pop(lua, 1);
    Gradient gradient;
    gradient.type = type ? type->value : GradientType::None;
    const char* allowed[6] = {"type", "stops", nullptr, nullptr, nullptr, nullptr};
    switch (gradient.type) {
        case GradientType::Linear: allowed[2] = "from"; allowed[3] = "to"; break;
        case GradientType::Radial:
            allowed[2] = "center"; allowed[3] = "radius";
            allowed[4] = "focal"; allowed[5] = "focal_radius";
            break;
        default: allowed[2] = "center"; allowed[3] = "angle"; break;
    }
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "gradient option names must be strings");
        auto name = lua_tostring(lua, -2);
        auto known = false;
        for (auto field : allowed) {
            if (field && std::strcmp(name, field) == 0) known = true;
        }
        if (!known) {
            static constexpr const char* geometry[] = {"from", "to", "center", "radius",
                                                       "focal", "focal_radius", "angle"};
            for (auto field : geometry) {
                if (std::strcmp(name, field) == 0) {
                    luaL_error(lua, "gradient option '%s' is not valid for a %s gradient", name,
                               gradient.type == GradientType::Linear ? "linear"
                               : gradient.type == GradientType::Radial ? "radial" : "conic");
                }
            }
            luaL_error(lua, "unknown gradient option '%s'", name);
        }
        lua_pop(lua, 1);
    }
    _gradientStops(lua, index, gradient, theme);
    if (gradient.type == GradientType::Linear) {
        auto from = _has(lua, index, "from");
        if (from != _has(lua, index, "to")) luaL_error(lua, "linear gradient needs both from and to");
        if (from) {
            gradient.from = _fieldWorld(lua, index, "from");
            gradient.to = _fieldWorld(lua, index, "to");
            gradient.line = true;
        }
    } else {
        if (_has(lua, index, "center")) {
            gradient.center = _fieldWorld(lua, index, "center");
            gradient.centered = true;
        }
    }
    if (gradient.type == GradientType::Radial) {
        if (_has(lua, index, "radius")) {
            gradient.radius = _fieldNumber(lua, index, "radius", 0.0f);
            if (gradient.radius <= 0.0f) luaL_error(lua, "gradient.radius must be positive");
            gradient.sized = true;
        }
        if (_has(lua, index, "focal")) {
            gradient.focal = _fieldWorld(lua, index, "focal");
            gradient.focused = true;
        }
        gradient.focalRadius = _fieldNumber(lua, index, "focal_radius", 0.0f);
        if (gradient.focalRadius < 0.0f) luaL_error(lua, "gradient.focal_radius cannot be negative");
    }
    if (gradient.type == GradientType::Conic) {
        gradient.angle = _fieldNumber(lua, index, "angle", 0.0f);
    }
    return gradient;
}

static float _blur(lua_State* lua, int index, const char* name)
{
    auto blur = _number(lua, index, name);
    if (blur < 0.0f || blur > 256.0f) luaL_error(lua, "%s must be between 0 and 256 pixels", name);
    return blur;
}

// Reads {color = ..., offset = {x, y}, blur = sigma} at index; omitted fields keep base.
static Shadow _shadow(lua_State* lua, int index, const Theme* theme, Shadow base)
{
    index = lua_absindex(lua, index);
    if (!lua_istable(lua, index)) luaL_error(lua, "shadow must be a table with color, offset and blur");
    _options(lua, index, {"color", "offset", "blur"}, false);
    Color color;
    if (_fieldColor(lua, index, "color", color, theme)) base.color = color;
    lua_getfield(lua, index, "offset");
    if (!lua_isnil(lua, -1)) base.offset = _vec2(lua, -1, "shadow offset");
    lua_pop(lua, 1);
    lua_getfield(lua, index, "blur");
    if (!lua_isnil(lua, -1)) base.blur = _blur(lua, -1, "shadow blur");
    lua_pop(lua, 1);
    return base;
}

static BlendMode _blendMode(lua_State* lua, int index)
{
    static constexpr LuaName<BlendMode> MODES[] = {
        {"normal", BlendMode::Normal}, {"multiply", BlendMode::Multiply}, {"screen", BlendMode::Screen},
        {"overlay", BlendMode::Overlay}, {"darken", BlendMode::Darken}, {"lighten", BlendMode::Lighten},
        {"color_dodge", BlendMode::ColorDodge}, {"color_burn", BlendMode::ColorBurn},
        {"hard_light", BlendMode::HardLight}, {"soft_light", BlendMode::SoftLight},
        {"difference", BlendMode::Difference}, {"exclusion", BlendMode::Exclusion}, {"hue", BlendMode::Hue},
        {"saturation", BlendMode::Saturation}, {"color", BlendMode::Color}, {"luminosity", BlendMode::Luminosity},
        {"add", BlendMode::Add}};
    static_assert(sizeof(MODES) / sizeof(MODES[0]) == static_cast<size_t>(BlendMode::Add) + 1u,
                  "a name for every blend mode");
    if (lua_type(lua, index) != LUA_TSTRING) luaL_error(lua, "blend must be a string");
    return _enum(lua, index, "blend", MODES);
}

static TextOrientation _textOrientation(lua_State* lua, int index)
{
    return _fieldEnum(lua, index, "orientation", TextOrientation::Billboard, TEXT_ORIENTATIONS, "text orientation");
}

static void _textFont(lua_State* lua, int index, Text* object)
{
    size_t size;
    auto font = luaL_checklstring(lua, index, &size);
    if (std::strlen(font) != size) luaL_error(lua, "font cannot contain zero bytes");
    if (size > LUA_PATH_LIMIT) luaL_error(lua, "font name exceeds the 4096-byte limit");
    if (object->font(font) != Result::Success) luaL_error(lua, "out of memory while copying font");
}

static void _text(lua_State* lua, int index, Text* object)
{
    if (!object) return;
    lua_getfield(lua, index, "size");
    auto sized = !lua_isnil(lua, -1);
    lua_pop(lua, 1);
    object->size = _fieldNumber(lua, index, "size", object->size);
    object->align = _fieldVec2(lua, index, "align", object->align);
    object->role = _fieldEnum(lua, index, "role", TextRole::Text, TEXT_ROLES, "text role");
    object->orientation = _textOrientation(lua, index);
    if (sized && object->size <= 0.0f) luaL_error(lua, "size must be positive");
    object->tabular = _fieldBool(lua, index, "tabular", object->tabular);
    object->slant = _fieldNumber(lua, index, "slant", object->slant);
    if (std::fabs(object->slant) > 1.0f) luaL_error(lua, "slant must be from -1 to 1");
    lua_getfield(lua, index, "font");
    if (!lua_isnil(lua, -1)) _textFont(lua, -1, object);
    lua_pop(lua, 1);
}

}  // namespace vis

#endif
