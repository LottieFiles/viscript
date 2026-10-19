// The Lua binding of the tex module: tex formulas, the parts and sides they expose, and the
// pairing of two formulas for a match transform.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "vis_tex.h"
#include "visTex.h"
#include "visTexLuaExtension.h"
#include "visTexScene.h"

namespace vis::tex
{

namespace
{

namespace core = vis::detail;

static constexpr const char* FORMULA_METATABLE = "vis.tex.Formula";
static constexpr const char* FORMULA_REGISTRY = "vis.tex.Formulas";
// The longest id the core gives an object; a piece whose name would be longer stays unnamed.
static constexpr size_t ID_LIMIT = 256u;
// The most pieces, and pairs, the core's match_transform takes.
static constexpr uint32_t GROUP_LIMIT = 1024u;
static constexpr uint32_t NONE = 0xffffffffu;

// Keys no TeX reads as, since TeX holds no control characters: they mark the pieces a pair the
// script gives took, and the runs of a part that a side holds only some pieces of, so that
// nothing marked on one side reads the same as anything on the other.
static char SOURCE_MARK[] = "\x01";
static char TARGET_MARK[] = "\x02";

// The options a formula and its parts style their group with, as the core styles any object;
// color and fill both set the group's fill, which colors the glyphs and shapes below it.
static constexpr const char* GROUP_STYLES[] = {"id",      "opacity", "progress", "layer", "isolate",
                                               "mask",    "blend",   "blur",     "shadow"};
static constexpr const char* FORMULA_OPTIONS[] = {"tex",         "point",   "align",    "size",  "font", "role",
                                                  "orientation", "display", "baseline", "color", "fill"};
static constexpr const char* PART_OPTIONS[] = {"text", "color", "fill"};
// The options of a tex part of spans, {tex = "x^2"}, besides the group options.
static constexpr const char* SPAN_OPTIONS[] = {"tex", "display", "size", "font", "role", "rise", "color", "fill"};
static constexpr const char* SHAPE_STYLES[] = {"stroke", "gradient", "width", "radius", "dash", "dash_offset"};

// A formula a script set: what it built, and the scene and id of its group once attached. Its
// user value is the handle of its scene, which it keeps alive, so that a formula found by the
// address of a group is the one set in that scene.
struct LuaFormula
{
    detail::Built built;
    Scene* scene = nullptr;
    uint32_t id = 0;
};

template<size_t N>
static bool _listed(const char* name, const char* const (&names)[N])
{
    for (auto listed : names) {
        if (std::strcmp(name, listed) == 0) return true;
    }
    return false;
}

static int _formulaGc(lua_State* lua)
{
    static_cast<LuaFormula*>(lua_touserdata(lua, 1))->~LuaFormula();
    return 0;
}

static void _disown(void* owned) noexcept
{
    *static_cast<bool*>(owned) = false;
}

// Pushes table[name] without metamethods, so that no script runs while the module holds the
// strings of the table it reads.
static int _get(lua_State* lua, int index, const char* name)
{
    index = lua_absindex(lua, index);
    lua_pushstring(lua, name);
    return lua_rawget(lua, index);
}

// Stops the script on an option a formula, or one of its parts, does not take.
static void _check(lua_State* lua, int index, bool part)
{
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) == LUA_TNUMBER) {
            if (!part || !lua_isinteger(lua, -2) || lua_tointeger(lua, -2) != 1) {
                luaL_error(lua, part ? "a tex part holds one string, first or as text"
                                     : "tex takes its options by name, such as tex = \"x^2\"");
            }
        } else if (lua_type(lua, -2) != LUA_TSTRING) {
            luaL_error(lua, "option names must be strings");
        } else {
            auto name = lua_tostring(lua, -2);
            if (_listed(name, SHAPE_STYLES)) {
                luaL_error(lua, "tex draws glyphs and filled shapes, so %s is unsupported; color and fill set its color",
                           name);
            }
            if (!_listed(name, GROUP_STYLES) && !(part ? _listed(name, PART_OPTIONS) : _listed(name, FORMULA_OPTIONS))) {
                luaL_error(lua, "unknown option '%s'", name);
            }
        }
        lua_pop(lua, 1);
    }
}

static float _number(lua_State* lua, int index, const char* name)
{
    if (lua_type(lua, index) != LUA_TNUMBER) luaL_error(lua, "%s must be a number", name);
    auto value = lua_tonumber(lua, index);
    if (!std::isfinite(value)) luaL_error(lua, "%s must be finite", name);
    return static_cast<float>(value);
}

// Two or, with depth, three numbers, as a list or as x, y and z.
static Vec3 _vector(lua_State* lua, int index, const char* name, bool depth)
{
    if (!lua_istable(lua, index)) luaL_error(lua, "%s must be a table of numbers", name);
    index = lua_absindex(lua, index);
    float values[3] = {};
    auto keyed = _get(lua, index, "x") != LUA_TNIL;
    lua_pop(lua, 1);
    if (keyed) {
        const char* keys[] = {"x", "y", "z"};
        for (auto i = 0; i < (depth ? 3 : 2); i++) {
            if (_get(lua, index, keys[i]) != LUA_TNIL) values[i] = _number(lua, -1, name);
            else if (i < 2) luaL_error(lua, "%s must contain x and y", name);
            lua_pop(lua, 1);
        }
        return {values[0], values[1], values[2]};
    }
    auto size = lua_rawlen(lua, index);
    if (size != 2u && !(depth && size == 3u)) {
        luaL_error(lua, depth ? "%s must contain 2 or 3 numbers" : "%s must contain 2 numbers", name);
    }
    for (auto i = 0u; i < size; i++) {
        lua_rawgeti(lua, index, static_cast<lua_Integer>(i) + 1);
        values[i] = _number(lua, -1, name);
        lua_pop(lua, 1);
    }
    return {values[0], values[1], values[2]};
}

// Reads the setting options of the table at index, which keeps the font's string.
static Options _options(lua_State* lua, int index)
{
    Options options;
    if (_get(lua, index, "point") != LUA_TNIL) options.point = _vector(lua, -1, "point", true);
    lua_pop(lua, 1);
    if (_get(lua, index, "align") != LUA_TNIL) {
        auto align = _vector(lua, -1, "align", false);
        options.align = {align.x, align.y};
    }
    lua_pop(lua, 1);
    if (_get(lua, index, "size") != LUA_TNIL) {
        options.size = _number(lua, -1, "size");
        if (options.size <= 0.0f) luaL_error(lua, "size must be positive");
    }
    lua_pop(lua, 1);
    if (_get(lua, index, "role") != LUA_TNIL) {
        auto role = lua_type(lua, -1) == LUA_TSTRING ? lua_tostring(lua, -1) : "";
        if (std::strcmp(role, "h1") == 0) options.role = TextRole::H1;
        else if (std::strcmp(role, "h2") == 0) options.role = TextRole::H2;
        else if (std::strcmp(role, "h3") == 0) options.role = TextRole::H3;
        else if (std::strcmp(role, "text") == 0) options.role = TextRole::Text;
        else if (std::strcmp(role, "code") == 0) options.role = TextRole::Code;
        else luaL_error(lua, "text role must be 'h1', 'h2', 'h3', 'text' or 'code'");
    }
    lua_pop(lua, 1);
    if (_get(lua, index, "orientation") != LUA_TNIL) {
        auto orientation = lua_type(lua, -1) == LUA_TSTRING ? lua_tostring(lua, -1) : "";
        if (std::strcmp(orientation, "billboard") == 0) options.orientation = TextOrientation::Billboard;
        else if (std::strcmp(orientation, "plane") == 0) options.orientation = TextOrientation::Plane;
        else luaL_error(lua, "text orientation must be 'billboard' or 'plane'");
    }
    lua_pop(lua, 1);
    if (_get(lua, index, "display") != LUA_TNIL) {
        if (!lua_isboolean(lua, -1)) luaL_error(lua, "display must be a boolean");
        options.display = lua_toboolean(lua, -1);
    }
    lua_pop(lua, 1);
    if (_get(lua, index, "baseline") != LUA_TNIL) {
        if (!lua_isboolean(lua, -1)) luaL_error(lua, "baseline must be a boolean");
        options.baseline = lua_toboolean(lua, -1);
    }
    lua_pop(lua, 1);
    if (_get(lua, index, "font") != LUA_TNIL) {
        if (lua_type(lua, -1) != LUA_TSTRING) luaL_error(lua, "font must be a string");
        size_t size;
        auto font = lua_tolstring(lua, -1, &size);
        if (std::strlen(font) != size) luaL_error(lua, "font cannot contain zero bytes");
        if (size > 4096u) luaL_error(lua, "font name exceeds the 4096-byte limit");
        options.font = font;
    }
    lua_pop(lua, 1);
    return options;
}

// Reads one part of a list, or the whole TeX: a string, or for a part a table holding its
// string first or as text, which keeps it.
static const char* _part(lua_State* lua, int part)
{
    if (lua_istable(lua, part)) {
        _check(lua, part, true);
        lua_rawgeti(lua, part, 1);
        auto text = _get(lua, part, "text") != LUA_TNIL;
        if (text && !lua_isnil(lua, -2)) luaL_error(lua, "a tex part takes its string first or as text, not both");
        part = text ? lua_gettop(lua) : lua_gettop(lua) - 1;
        if (lua_type(lua, part) != LUA_TSTRING) luaL_error(lua, "a tex part needs a string, first or as text");
    } else if (lua_type(lua, part) != LUA_TSTRING) {
        luaL_error(lua, "a tex part must be a string or a table, got %s", luaL_typename(lua, part));
    }
    size_t size;
    auto source = lua_tolstring(lua, part, &size);
    if (std::strlen(source) != size) luaL_error(lua, "tex cannot contain zero bytes");
    return source;
}

// Pushes the style table a group attaches with: the group options of the table at from, or
// none, with color as the fill unless fill is set.
static void _style(lua_State* lua, int from)
{
    lua_createtable(lua, 0, 10);
    if (!from) return;
    for (auto name : GROUP_STYLES) {
        _get(lua, from, name);
        lua_setfield(lua, -2, name);
    }
    if (_get(lua, from, "fill") == LUA_TNIL) {
        lua_pop(lua, 1);
        _get(lua, from, "color");
    }
    lua_setfield(lua, -2, "fill");
}

// Names the object base/ordinal, unless the name would be too long.
static void _name(lua_State* lua, Scene* scene, Object* object, const char* base, uint32_t ordinal)
{
    char id[ID_LIMIT + 16u];
    auto length = std::snprintf(id, sizeof(id), "%s/%u", base, ordinal);
    if (length <= 0 || static_cast<size_t>(length) > ID_LIMIT) return;
    if (scene->object(id)) luaL_error(lua, "duplicate id '%s'", id);
    object->tag(id);
    if (!object->tag()) luaL_error(lua, "out of memory while copying id");
}

static int _attachPart(lua_State* lua)
{
    auto formula = static_cast<LuaFormula*>(lua_touserdata(lua, lua_upvalueindex(1)));
    auto index = static_cast<uint32_t>(lua_tointeger(lua, lua_upvalueindex(2)));
    return core::luaAddObject(lua, formula->built.parts[index], 2, "tex part", _disown, &formula->built.ownsPart[index]);
}

// The formula whose group object is, in the scene of the handle at box, or nullptr.
static LuaFormula* _formula(lua_State* lua, int box, Scene* scene, const Object* object)
{
    lua_getfield(lua, LUA_REGISTRYINDEX, FORMULA_REGISTRY);
    lua_pushlightuserdata(lua, const_cast<Object*>(object));
    lua_rawget(lua, -2);
    auto formula = static_cast<LuaFormula*>(lua_touserdata(lua, -1));
    if (formula) {
        lua_getiuservalue(lua, -1, 1);
        if (!lua_rawequal(lua, -1, box)) formula = nullptr;
        lua_pop(lua, 1);
    }
    lua_pop(lua, 2);
    if (!formula || formula->scene != scene || formula->built.root != object || scene->object(formula->id) != object) {
        return nullptr;
    }
    return formula;
}

// Reads the TeX at list, a string or a list of parts, and sets it with options in the scene of
// the handle at box, drawing nothing yet: pushes the formula, which keeps that handle.
static LuaFormula* _prepare(lua_State* lua, int box, int list, const Options& options, Scene* scene)
{
    auto split = lua_istable(lua, list);
    if (!split && lua_type(lua, list) != LUA_TSTRING) {
        luaL_error(lua, "tex needs its TeX: a string, or a list of parts");
    }
    auto count = split ? lua_rawlen(lua, list) : 1u;
    if (!count) luaL_error(lua, "tex needs at least one part");
    // The parts read with a blank between each, which alone would pass the limit.
    if (count > detail::SourceLimit) luaL_error(lua, "tex: a formula holds at most %u bytes", detail::SourceLimit);
    if (split) {
        lua_pushnil(lua);
        while (lua_next(lua, list)) {
            auto key = lua_isinteger(lua, -2) ? lua_tointeger(lua, -2) : 0;
            if (key < 1 || static_cast<size_t>(key) > count) luaL_error(lua, "tex parts must form a list");
            lua_pop(lua, 1);
        }
    }
    auto sources = static_cast<const char**>(lua_newuserdatauv(lua, sizeof(const char*) * count, 0));
    auto scratch = lua_gettop(lua);
    for (auto i = 0u; i < count; i++) {
        if (split) lua_rawgeti(lua, list, static_cast<lua_Integer>(i) + 1);
        else lua_pushvalue(lua, list);
        // The list, or the options, keep the string while the formula is set.
        sources[i] = _part(lua, lua_gettop(lua));
        lua_settop(lua, scratch);
    }
    auto formula = new (lua_newuserdatauv(lua, sizeof(LuaFormula), 1)) LuaFormula;
    luaL_setmetatable(lua, FORMULA_METATABLE);
    lua_pushvalue(lua, box);
    lua_setiuservalue(lua, -2, 1);
    char error[512];
    if (!detail::prepare(scene, sources, static_cast<uint32_t>(count), split, options, formula->built, error,
                         sizeof(error))) {
        luaL_error(lua, "tex: %s", error);
    }
    return formula;
}

// Attaches the formula drawn in the userdata at store below the handle at index 1: its group
// with the style options of the table at from, its pieces named after its id, and each part of
// the list at list, if the TeX was one, with the options of its own table. Pushes the handle
// of the formula's group.
static int _attach(lua_State* lua, int store, int list, int from, Scene* scene)
{
    auto formula = static_cast<LuaFormula*>(lua_touserdata(lua, store));
    auto& built = formula->built;
    auto split = lua_istable(lua, list);

    // Pieces are named after their group: the formula's, or their part's.
    _get(lua, from, "id");
    auto id = lua_type(lua, -1) == LUA_TSTRING ? lua_tostring(lua, -1) : nullptr;
    if (id && scene->object(id)) return luaL_error(lua, "duplicate id '%s'", id);
    if (!split && id) {
        for (auto i = 0u; i < built.pieceCount; i++) _name(lua, scene, built.pieces[i].object, id, i + 1u);
    }
    _style(lua, from);
    core::luaAddObject(lua, built.root, -1, "tex", _disown, &built.ownsRoot);
    auto root = lua_gettop(lua);
    formula->scene = scene;
    formula->id = built.root->id();
    lua_getfield(lua, LUA_REGISTRYINDEX, FORMULA_REGISTRY);
    lua_pushlightuserdata(lua, built.root);
    lua_pushvalue(lua, store);
    lua_rawset(lua, -3);
    lua_settop(lua, root);

    for (auto i = 0u; split && i < built.partCount; i++) {
        lua_rawgeti(lua, list, static_cast<lua_Integer>(i) + 1);
        _style(lua, lua_istable(lua, -1) ? lua_gettop(lua) : 0);
        auto style = lua_gettop(lua);
        lua_getfield(lua, style, "id");
        if (lua_isnil(lua, -1) && id) {
            lua_pop(lua, 1);
            lua_pushfstring(lua, "%s/%d", id, static_cast<int>(i + 1u));
            if (lua_rawlen(lua, -1) > ID_LIMIT) {
                lua_pop(lua, 1);
                lua_pushnil(lua);
            }
            lua_pushvalue(lua, -1);
            lua_setfield(lua, style, "id");
        }
        if (lua_type(lua, -1) == LUA_TSTRING) {
            auto name = lua_tostring(lua, -1);
            auto ordinal = 0u;
            for (auto k = 0u; k < built.pieceCount; k++) {
                if (built.pieces[k].part == i) _name(lua, scene, built.pieces[k].object, name, ++ordinal);
            }
        }
        lua_pushvalue(lua, store);
        lua_pushinteger(lua, static_cast<lua_Integer>(i));
        lua_pushcclosure(lua, _attachPart, 2);
        lua_pushvalue(lua, root);
        lua_pushvalue(lua, style);
        core::luaCall(lua, 2, 1);
        lua_settop(lua, root);
    }
    return 1;
}

// scene:tex{tex = "a^2+b^2=c^2", point = {0, 0}, role = "h2"}: TeX math set in the scene's
// fonts as a typeset group. With tex as a list of parts, each a string or a table that also
// takes the part's id and style, the group holds one group per part.
static int _tex(lua_State* lua)
{
    luaL_checktype(lua, 2, LUA_TTABLE);
    lua_settop(lua, 2);
    _check(lua, 2, false);
    auto options = _options(lua, 2);
    auto scene = core::luaHandleScene(lua, 1);
    auto box = lua_gettop(lua);
    _get(lua, 2, "tex");
    auto list = lua_gettop(lua);
    auto formula = _prepare(lua, box, list, options, scene);
    auto store = lua_gettop(lua);
    char error[512];
    if (!detail::draw(formula->built, detail::origin(formula->built, options), true, error, sizeof(error))) {
        return luaL_error(lua, "tex: %s", error);
    }
    return _attach(lua, store, list, 2, scene);
}

// place(row, style, x, y, z) of a tex part of spans: draws the formula of upvalue 1 with the
// left end of its baseline at x, y, z and attaches it below the row with style, its parts, if
// the TeX of upvalue 2 is a list, with their own options.
static int _placeTex(lua_State* lua)
{
    lua_settop(lua, 5);
    Vec3 origin = {_number(lua, 3, "x"), _number(lua, 4, "y"), _number(lua, 5, "z")};
    auto scene = core::luaHandleScene(lua, 1);
    lua_pushvalue(lua, lua_upvalueindex(1));
    auto store = lua_gettop(lua);
    lua_pushvalue(lua, lua_upvalueindex(2));
    auto list = lua_gettop(lua);
    auto formula = static_cast<LuaFormula*>(lua_touserdata(lua, store));
    char error[512];
    if (!detail::draw(formula->built, origin, true, error, sizeof(error))) return luaL_error(lua, "tex: %s", error);
    // The row resolved the formula's layer; each part may override it. Set every painted
    // piece before attachment, since ordinary groups do not pass their layer down.
    auto layer = [&](int table, int32_t fallback) {
        if (_get(lua, table, "layer") == LUA_TNIL) {
            lua_pop(lua, 1);
            return fallback;
        }
        if (!lua_isinteger(lua, -1)) luaL_error(lua, "layer must be an integer");
        auto value = lua_tointeger(lua, -1);
        lua_pop(lua, 1);
        if (value < INT32_MIN || value > INT32_MAX) luaL_error(lua, "layer is out of range");
        return static_cast<int32_t>(value);
    };
    auto& built = formula->built;
    built.root->layer = layer(2, built.root->layer);
    for (auto i = 0u; i < built.partCount; i++) {
        lua_rawgeti(lua, list, static_cast<lua_Integer>(i) + 1);
        built.parts[i]->layer = lua_istable(lua, -1) ? layer(lua_gettop(lua), built.root->layer) : built.root->layer;
        lua_pop(lua, 1);
    }
    for (auto i = 0u; i < built.pieceCount; i++) {
        built.pieces[i].object->layer = built.partCount ? built.parts[built.pieces[i].part]->layer : built.root->layer;
    }
    return _attach(lua, store, list, 2, scene);
}

// Stops the script on an option a tex part of spans does not take.
static void _checkSpan(lua_State* lua, int index)
{
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) == LUA_TNUMBER) luaL_error(lua, "a spans part sets text or tex, not both");
        if (lua_type(lua, -2) != LUA_TSTRING) luaL_error(lua, "option names must be strings");
        auto name = lua_tostring(lua, -2);
        if (std::strcmp(name, "text") == 0) luaL_error(lua, "a spans part sets text or tex, not both");
        if (_listed(name, SHAPE_STYLES)) {
            luaL_error(lua, "tex draws glyphs and filled shapes, so %s is unsupported; color and fill set its color",
                       name);
        }
        if (!_listed(name, GROUP_STYLES) && !_listed(name, SPAN_OPTIONS)) luaL_error(lua, "unknown option '%s'", name);
        lua_pop(lua, 1);
    }
}

// The maker of the tex parts of spans, {tex = "x^2"}, which spans calls with its handle, the
// part and its setting: the formula set inline, in text style unless display is set, in the
// size, font and role the setting gives. Its line box is the font's, grown to hold the formula,
// so that a formula no taller than the text keeps the lines as they are; its ink is its box.
static int _spanTex(lua_State* lua)
{
    lua_settop(lua, 3);
    luaL_checktype(lua, 2, LUA_TTABLE);
    luaL_checktype(lua, 3, LUA_TTABLE);
    _checkSpan(lua, 2);
    auto options = _options(lua, 3);
    options.display = false;
    if (_get(lua, 2, "display") != LUA_TNIL) {
        if (!lua_isboolean(lua, -1)) luaL_error(lua, "display must be a boolean");
        options.display = lua_toboolean(lua, -1);
    }
    lua_pop(lua, 1);
    auto scene = core::luaHandleScene(lua, 1);
    auto box = lua_gettop(lua);
    _get(lua, 2, "tex");
    auto list = lua_gettop(lua);
    auto& built = _prepare(lua, box, list, options, scene)->built;
    auto store = lua_gettop(lua);
    lua_pushnumber(lua, built.width);
    lua_pushnumber(lua, built.height > built.ascent ? built.height : built.ascent);
    lua_pushnumber(lua, built.depth > built.descent ? built.depth : built.descent);
    lua_pushnumber(lua, built.height);
    lua_pushnumber(lua, -built.depth);
    lua_pushvalue(lua, store);
    lua_pushvalue(lua, list);
    lua_pushcclosure(lua, _placeTex, 2);
    return 6;
}

// Pushes what a formula or one of its parts holds, as a list for a message.
static void _contents(lua_State* lua, const detail::Built& built, uint32_t part)
{
    luaL_Buffer buffer;
    luaL_buffinit(lua, &buffer);
    auto written = 0u;
    auto listed = 0u;
    auto add = [&](const char* text) {
        if (written > 200u) return;
        if (listed++) luaL_addstring(&buffer, ", ");
        luaL_addchar(&buffer, '\'');
        luaL_addstring(&buffer, text);
        luaL_addchar(&buffer, '\'');
        written += static_cast<uint32_t>(std::strlen(text)) + 4u;
        if (written > 200u) luaL_addstring(&buffer, ", ...");
    };
    if (part == NONE && built.partCount) {
        for (auto i = 0u; i < built.partCount; i++) add(built.sources[i]);
    } else {
        for (auto i = 0u; i < built.pieceCount; i++) {
            if (part == NONE || built.pieces[i].part == part) add(built.pieces[i].key);
        }
    }
    luaL_pushresult(&buffer);
}

// formula:part(tex, n): the nth part of the formula whose TeX reads tex, blanks aside, or else
// its nth glyph or shape whose TeX does, such as "c" or "\frac" for a bar; on a part, the nth
// of its own glyphs and shapes.
static int _partOf(lua_State* lua)
{
    lua_settop(lua, 3);
    auto scene = core::luaHandleScene(lua, 1);
    auto box = lua_gettop(lua);
    auto object = core::luaObject(lua, scene, 1);
    size_t size;
    auto wanted = luaL_checklstring(lua, 2, &size);
    if (std::strlen(wanted) != size) return luaL_error(lua, "part cannot contain zero bytes");
    auto nth = luaL_optinteger(lua, 3, 1);
    if (nth < 1) return luaL_error(lua, "part counts its matches from 1");
    auto part = NONE;
    auto formula = _formula(lua, box, scene, object);
    if (!formula && object->parent()) {
        formula = _formula(lua, box, scene, object->parent());
        for (auto i = 0u; formula && i < formula->built.partCount; i++) {
            if (formula->built.parts[i] == object) part = i;
        }
        if (part == NONE) formula = nullptr;
    }
    if (!formula) return luaL_error(lua, "part looks in a tex formula or in one of its parts");
    auto& built = formula->built;
    auto key = static_cast<char*>(lua_newuserdatauv(lua, size + 1u, 0));
    detail::key(wanted, 0u, static_cast<uint32_t>(size), key);
    lua_Integer seen = 0;
    if (part == NONE) {
        for (auto i = 0u; i < built.partCount; i++) {
            if (std::strcmp(built.sources[i], key) == 0 && ++seen == nth) {
                return core::luaPushObject(lua, box, built.parts[i]);
            }
        }
    }
    if (!seen) {
        for (auto i = 0u; i < built.pieceCount; i++) {
            if (part != NONE && built.pieces[i].part != part) continue;
            if (std::strcmp(built.pieces[i].key, key) == 0 && ++seen == nth) {
                return core::luaPushObject(lua, box, built.pieces[i].object);
            }
        }
    }
    luaL_where(lua, 1);
    if (seen) {
        lua_pushfstring(lua, "tex has only %d of '%s'", static_cast<int>(seen), key);
        lua_concat(lua, 2);
        return lua_error(lua);
    }
    auto parts = part == NONE && built.partCount;
    lua_pushfstring(lua, parts ? "tex has no part '%s'; its parts are " : "tex draws no '%s'; it draws ", key);
    _contents(lua, built, part);
    lua_concat(lua, 3);
    return lua_error(lua);
}

// The pieces formulas drew below the object of one side of a match, in the order of the tree,
// which keeps the pieces of each part together; each part is a run of them, and the size of a
// run is how many pieces its part has in all.
struct LuaSide
{
    uint32_t count = 0;
    detail::Piece* pieces = nullptr;
    uint32_t keyed = 0;
    const char** sources = nullptr;
    uint32_t* sizes = nullptr;
    uint32_t runs = 0;
};

static uint32_t _leafCount(const Object* object)
{
    if (object->type() != Type::Group) return 1u;
    auto count = 0u;
    for (auto i = 0u; i < object->childCount(); i++) count += _leafCount(object->childAt(i));
    return count;
}

static void _gather(Object* object, Object** output, uint32_t& count)
{
    if (object->type() != Type::Group) {
        output[count++] = object;
        return;
    }
    for (auto i = 0u; i < object->childCount(); i++) _gather(object->childAt(i), output, count);
}

// The index of leaf among the pieces of a formula, trying hint first, since the leaves of a
// formula come in the order of its pieces; NONE when it is none of them.
static uint32_t _indexOf(const detail::Built& built, const Object* leaf, uint32_t hint)
{
    if (hint < built.pieceCount && built.pieces[hint].object == leaf) return hint;
    for (auto i = 0u; i < built.pieceCount; i++) {
        if (built.pieces[i].object == leaf) return i;
    }
    return NONE;
}

// How many pieces the part of the piece at index has, which lie together.
static uint32_t _partSize(const detail::Built& built, uint32_t index)
{
    auto part = built.pieces[index].part;
    auto first = index;
    auto last = index + 1u;
    while (first > 0u && built.pieces[first - 1u].part == part) first--;
    while (last < built.pieceCount && built.pieces[last].part == part) last++;
    return last - first;
}

// Gathers the side of object, whose arrays a userdata left on the stack holds; none when it
// has more than limit leaves.
static LuaSide _side(lua_State* lua, int box, Scene* scene, Object* object, uint32_t limit)
{
    LuaSide side;
    side.count = _leafCount(object);
    auto room = side.count && side.count <= limit ? side.count : 1u;
    auto bytes = (sizeof(detail::Piece) + sizeof(Object*) + sizeof(const char*) + sizeof(uint32_t)) * room;
    auto memory = static_cast<char*>(lua_newuserdatauv(lua, bytes, 0));
    side.pieces = new (memory) detail::Piece[room];
    auto leaves = reinterpret_cast<Object**>(memory + sizeof(detail::Piece) * room);
    side.sources = reinterpret_cast<const char**>(memory + (sizeof(detail::Piece) + sizeof(Object*)) * room);
    side.sizes = reinterpret_cast<uint32_t*>(memory + (sizeof(detail::Piece) + sizeof(Object*) + sizeof(const char*)) * room);
    if (side.count > limit) return side;
    auto count = 0u;
    _gather(object, leaves, count);
    // A piece lies right below the group of its formula, or below the group of its part.
    const Object* parent = nullptr;
    LuaFormula* formula = nullptr;
    LuaFormula* runFormula = nullptr;
    auto run = NONE;
    auto next = NONE;
    for (auto i = 0u; i < count; i++) {
        auto leaf = leaves[i];
        if (leaf->parent() != parent) {
            parent = leaf->parent();
            formula = parent ? _formula(lua, box, scene, parent) : nullptr;
            if (!formula && parent && parent->parent()) formula = _formula(lua, box, scene, parent->parent());
        }
        auto index = formula ? _indexOf(formula->built, leaf, next) : NONE;
        if (index == NONE) continue;
        next = index + 1u;
        auto& piece = formula->built.pieces[index];
        if (formula != runFormula || piece.part != run) {
            side.sources[side.runs] = formula->built.sources[piece.part];
            side.sizes[side.runs++] = _partSize(formula->built, index);
            runFormula = formula;
            run = piece.part;
        }
        side.pieces[side.keyed++] = {leaf, piece.key, side.runs - 1u};
    }
    return side;
}

// Marks the runs of side that hold only some pieces of their part, or a piece a given pair
// took, so that no run pairs with another as a whole part.
static void _seal(LuaSide& side, char* mark)
{
    for (auto i = 0u; i < side.keyed;) {
        auto run = side.pieces[i].part;
        auto held = 0u;
        auto whole = true;
        for (; i < side.keyed && side.pieces[i].part == run; i++, held++) {
            if (side.pieces[i].key == mark) whole = false;
        }
        if (!whole || held != side.sizes[run]) side.sources[run] = mark;
    }
}

// Pushes a table that finds the pieces of side by their objects.
static void _index(lua_State* lua, const LuaSide& side)
{
    lua_createtable(lua, 0, static_cast<int>(side.keyed));
    for (auto i = 0u; i < side.keyed; i++) {
        lua_pushlightuserdata(lua, side.pieces[i].object);
        lua_pushinteger(lua, static_cast<lua_Integer>(i));
        lua_rawset(lua, -3);
    }
}

static uint32_t _lookup(lua_State* lua, int index, const Object* object)
{
    lua_pushlightuserdata(lua, const_cast<Object*>(object));
    lua_rawget(lua, index);
    auto found = lua_isinteger(lua, -1) ? static_cast<uint32_t>(lua_tointeger(lua, -1)) : NONE;
    lua_pop(lua, 1);
    return found;
}

// The pieces of a whole side that lie below object and are still free, as a side of its own
// that shares the runs of the whole, with where each piece lies in the whole; on the stack.
static LuaSide _below(lua_State* lua, const LuaSide& whole, int index, Object* object, const uint32_t* partners,
                      const uint32_t* taken, char* mark, uint32_t*& places)
{
    LuaSide side;
    side.count = _leafCount(object);
    auto room = side.count ? side.count : 1u;
    auto runs = whole.runs ? whole.runs : 1u;
    auto bytes = (sizeof(detail::Piece) + sizeof(Object*) + sizeof(uint32_t)) * room
                 + (sizeof(const char*) + sizeof(uint32_t)) * runs;
    auto memory = static_cast<char*>(lua_newuserdatauv(lua, bytes, 0));
    side.pieces = new (memory) detail::Piece[room];
    auto leaves = reinterpret_cast<Object**>(memory + sizeof(detail::Piece) * room);
    side.sources = reinterpret_cast<const char**>(memory + (sizeof(detail::Piece) + sizeof(Object*)) * room);
    side.sizes = reinterpret_cast<uint32_t*>(memory + (sizeof(detail::Piece) + sizeof(Object*)) * room
                                             + sizeof(const char*) * runs);
    places = side.sizes + runs;
    side.runs = whole.runs;
    for (auto r = 0u; r < whole.runs; r++) {
        side.sources[r] = whole.sources[r];
        side.sizes[r] = whole.sizes[r];
    }
    auto count = 0u;
    _gather(object, leaves, count);
    for (auto i = 0u; i < count; i++) {
        auto place = _lookup(lua, index, leaves[i]);
        if (place == NONE || whole.pieces[place].key == mark) continue;
        if (partners ? partners[place] != NONE : taken[place] != NONE) continue;
        places[side.keyed] = place;
        side.pieces[side.keyed++] = whole.pieces[place];
    }
    _seal(side, mark);
    return side;
}

// Pairs the pieces of a pair the script gives with a group on either side: those that read
// the same as match() pairs them, and then the rest in order. False when either holds no free
// piece of the formulas, which leaves the pair to the core.
static bool _given(lua_State* lua, const LuaSide& source, const LuaSide& target, int sourceIndex, int targetIndex,
                   Object* from, Object* to, uint32_t* partners, uint32_t* taken)
{
    auto top = lua_gettop(lua);
    uint32_t* sourcePlaces;
    uint32_t* targetPlaces;
    auto a = _below(lua, source, sourceIndex, from, partners, nullptr, SOURCE_MARK, sourcePlaces);
    auto b = _below(lua, target, targetIndex, to, nullptr, taken, TARGET_MARK, targetPlaces);
    if (!a.keyed || !b.keyed) {
        lua_settop(lua, top);
        return false;
    }
    auto inner = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * (a.keyed + b.keyed), 0));
    auto used = inner + a.keyed;
    for (auto i = 0u; i < a.keyed; i++) inner[i] = NONE;
    for (auto j = 0u; j < b.keyed; j++) used[j] = NONE;
    detail::Side sides[] = {{a.pieces, a.keyed, a.sources, a.runs}, {b.pieces, b.keyed, b.sources, b.runs}};
    if (!detail::match(sides[0], sides[1], inner)) luaL_error(lua, "out of memory");
    for (auto i = 0u; i < a.keyed; i++) {
        if (inner[i] != NONE) used[inner[i]] = i;
    }
    // What the TeX leaves unpaired pairs in order.
    for (auto i = 0u, j = 0u; i < a.keyed; i++) {
        if (inner[i] != NONE) continue;
        while (j < b.keyed && used[j] != NONE) j++;
        if (j == b.keyed) break;
        inner[i] = j;
        used[j++] = i;
    }
    for (auto i = 0u; i < a.keyed; i++) {
        if (inner[i] == NONE) continue;
        partners[sourcePlaces[i]] = targetPlaces[inner[i]];
        taken[targetPlaces[inner[i]]] = sourcePlaces[i];
    }
    lua_settop(lua, top);
    return true;
}

// Pairs the pieces of formulas for the core's match_transform, with arg 6 replaced by options
// that carry the pairs: true when both sides hold pieces of formulas. With strings = false,
// which keeps the core to the pairs it is given, the TeX pairs no more pieces either: only
// the pairs given, a pair with a group pairing the pieces below it.
static bool _texMatch(lua_State* lua, int arguments)
{
    if (arguments < 6) lua_settop(lua, 6);
    auto scene = core::luaHandleScene(lua, 1);
    auto box = lua_gettop(lua);
    auto from = core::luaObject(lua, scene, 2);
    auto to = core::luaObject(lua, scene, 3);
    auto source = _side(lua, box, scene, from, GROUP_LIMIT);
    auto target = _side(lua, box, scene, to, GROUP_LIMIT);
    if (!source.keyed || !target.keyed) return false;
    auto given = 0;
    auto strings = true;
    if (!lua_isnil(lua, 6)) {
        luaL_checktype(lua, 6, LUA_TTABLE);
        // The core says what is wrong with strings that is not a boolean.
        if (_get(lua, 6, "strings") == LUA_TBOOLEAN) strings = lua_toboolean(lua, -1);
        lua_pop(lua, 1);
        if (_get(lua, 6, "pairs") != LUA_TNIL) {
            // The core says what is wrong with pairs it cannot take.
            if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) > GROUP_LIMIT) return false;
            given = lua_gettop(lua);
        }
    }
    auto partners = static_cast<uint32_t*>(lua_newuserdatauv(lua, sizeof(uint32_t) * (source.keyed + target.keyed), 0));
    auto taken = partners + source.keyed;
    for (auto i = 0u; i < source.keyed + target.keyed; i++) partners[i] = NONE;
    // The options as given, but for the pairs.
    lua_createtable(lua, 0, 8);
    auto options = lua_gettop(lua);
    lua_createtable(lua, 0, 0);
    auto pairs = lua_gettop(lua);
    auto count = 0;
    if (!lua_isnil(lua, 6)) {
        lua_pushnil(lua);
        while (lua_next(lua, 6)) {
            if (lua_type(lua, -2) == LUA_TSTRING && std::strcmp(lua_tostring(lua, -2), "pairs") == 0) {
                lua_pop(lua, 1);
                continue;
            }
            lua_pushvalue(lua, -2);
            lua_insert(lua, -2);
            lua_rawset(lua, options);
        }
    }
    if (given) {
        _index(lua, source);
        auto sourceIndex = lua_gettop(lua);
        _index(lua, target);
        auto targetIndex = lua_gettop(lua);
        auto size = static_cast<lua_Integer>(lua_rawlen(lua, given));
        // Pairs of two pieces go to the core as given, and the formulas leave those pieces
        // out; then each pair with a group pairs the pieces below it.
        for (auto pass = 0; pass < 2; pass++) {
            for (lua_Integer i = 1; i <= size; i++) {
                lua_rawgeti(lua, given, i);
                if (!lua_istable(lua, -1) || lua_rawlen(lua, -1) != 2u) {
                    luaL_error(lua, "pairs must be an array of {source_piece, target_piece}");
                }
                lua_rawgeti(lua, -1, 1);
                auto a = core::luaObject(lua, scene, -1);
                lua_rawgeti(lua, -2, 2);
                auto b = core::luaObject(lua, scene, -1);
                lua_pop(lua, 2);
                auto group = a->type() == Type::Group || b->type() == Type::Group;
                if (!group && pass == 0) {
                    auto place = _lookup(lua, sourceIndex, a);
                    if (place != NONE) source.pieces[place].key = SOURCE_MARK;
                    place = _lookup(lua, targetIndex, b);
                    if (place != NONE) target.pieces[place].key = TARGET_MARK;
                    lua_rawseti(lua, pairs, ++count);
                    continue;
                }
                if (group && pass == 1
                    && !_given(lua, source, target, sourceIndex, targetIndex, a, b, partners, taken)) {
                    lua_rawseti(lua, pairs, ++count);
                    continue;
                }
                lua_pop(lua, 1);
            }
        }
    }
    if (strings) {
        _seal(source, SOURCE_MARK);
        _seal(target, TARGET_MARK);
        detail::Side sides[] = {{source.pieces, source.keyed, source.sources, source.runs},
                                {target.pieces, target.keyed, target.sources, target.runs}};
        if (!detail::match(sides[0], sides[1], partners)) luaL_error(lua, "out of memory");
    }
    for (auto i = 0u; i < source.keyed; i++) {
        if (partners[i] == NONE) continue;
        lua_createtable(lua, 2, 0);
        core::luaPushObject(lua, box, source.pieces[i].object);
        lua_rawseti(lua, -2, 1);
        core::luaPushObject(lua, box, target.pieces[partners[i]].object);
        lua_rawseti(lua, -2, 2);
        lua_rawseti(lua, pairs, ++count);
    }
    lua_pushvalue(lua, pairs);
    lua_setfield(lua, options, "pairs");
    lua_pushvalue(lua, options);
    lua_replace(lua, 6);
    return true;
}

// Matches formula pieces by TeX keys unless strings = false requests only explicit pairs.
static int _matchTransform(lua_State* lua)
{
    auto arguments = lua_gettop(lua);
    if (arguments >= 3 && lua_type(lua, 2) == LUA_TUSERDATA && lua_type(lua, 3) == LUA_TUSERDATA
        && _texMatch(lua, arguments)) {
        arguments = 6;
    }
    lua_settop(lua, arguments);
    return core::luaCallWrapped(lua, lua_upvalueindex(1));
}

static int _open(lua_State* lua, void*)
{
    luaL_newmetatable(lua, FORMULA_METATABLE);
    lua_pushcfunction(lua, _formulaGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_pop(lua, 1);
    lua_newtable(lua);
    lua_setfield(lua, LUA_REGISTRYINDEX, FORMULA_REGISTRY);

    lua_pushcfunction(lua, _tex);
    core::luaSetMethod(lua, core::LuaHandle::Scene, "tex");
    lua_pushcfunction(lua, _tex);
    core::luaSetMethod(lua, core::LuaHandle::Object, "tex");
    lua_pushcfunction(lua, _partOf);
    core::luaSetMethod(lua, core::LuaHandle::Group, "part");
    lua_pushcfunction(lua, _spanTex);
    core::luaSetSpanPart(lua, "tex");
    core::luaGetMethod(lua, core::LuaHandle::Scene, "match_transform");
    lua_pushcclosure(lua, _matchTransform, 1);
    core::luaSetMethod(lua, core::LuaHandle::Scene, "match_transform");
    return 0;
}

}  // namespace

Result Lua::load(const char* source, uint32_t size, const char* name, Scene** scene, char* error, uint32_t errorSize,
                 vis::Lua::AssetResolver resolver, void* resolverData, const Theme* adaptiveTheme,
                 bool* adaptiveThemeUsed) noexcept
{
    auto hook = lua_extension::luaHooks();
    core::LuaHookSpan hooks = {&hook, 1u};
    return core::luaLoad(source, size, name, scene, error, errorSize, resolver, resolverData, nullptr, 0u, adaptiveTheme,
                         adaptiveThemeUsed, hooks);
}

vis::detail::LuaHooks lua_extension::luaHooks() noexcept
{
    return {_open, nullptr, nullptr};
}

}  // namespace vis::tex
