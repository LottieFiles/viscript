#include <cmath>
#include <cstring>
#include <new>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "vis_diagram.h"
#include "visDiagramLuaExtension.h"

namespace vis::diagram
{

namespace
{

static constexpr const char* DIAGRAM_METATABLE = "vis.diagram.Diagram";
static constexpr const char* NODE_METATABLE = "vis.diagram.Node";
static constexpr const char* EDGE_METATABLE = "vis.diagram.Edge";
static constexpr const char* ZONE_METATABLE = "vis.diagram.Zone";
static constexpr const char* BUILT_METATABLE = "vis.diagram.Built";

struct LuaDiagram
{
    Diagram* diagram = nullptr;
    Scene* scene = nullptr;
    bool built = false;
};

template<typename T>
struct LuaHandle
{
    LuaDiagram* owner = nullptr;
    T value;
};

struct LuaBuilt
{
    Built* built = nullptr;
    LuaDiagram* owner = nullptr;
    Group* root = nullptr;
};

static bool _has(lua_State* lua, int index, const char* name)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    auto found = !lua_isnil(lua, -1);
    lua_pop(lua, 1);
    return found;
}

static void _options(lua_State* lua, int index, const char* kind,
                     const char* const* names)
{
    index = lua_absindex(lua, index);
    lua_pushnil(lua);
    while (lua_next(lua, index)) {
        if (lua_type(lua, -2) != LUA_TSTRING) {
            luaL_error(lua, "%s keys must be strings", kind);
        }
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

static float _number(lua_State* lua, int index, const char* name)
{
    auto value = static_cast<float>(luaL_checknumber(lua, index));
    if (!std::isfinite(value)) luaL_error(lua, "%s must be finite", name);
    return value;
}

static float _fieldNumber(lua_State* lua, int index, const char* name,
                          float fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    auto value = _number(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

static int32_t _fieldInteger(lua_State* lua, int index, const char* name,
                             int32_t fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    auto value = luaL_checkinteger(lua, -1);
    lua_pop(lua, 1);
    if (value < INT32_MIN || value > INT32_MAX) luaL_error(lua, "%s is out of range", name);
    return static_cast<int32_t>(value);
}

static bool _fieldBool(lua_State* lua, int index, const char* name, bool fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    if (!lua_isboolean(lua, -1)) luaL_error(lua, "%s must be boolean", name);
    auto value = lua_toboolean(lua, -1) != 0;
    lua_pop(lua, 1);
    return value;
}

// The text of field name of the table at index, or nullptr when it is nil. The text is left on
// the stack until the caller returns, after the diagram has copied it, since text given as a
// number becomes a string that only the stack holds.
static const char* _fieldString(lua_State* lua, int index, const char* name,
                                bool required = false)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        if (required) luaL_error(lua, "missing field '%s'", name);
        return nullptr;
    }
    size_t size = 0u;
    auto value = luaL_checklstring(lua, -1, &size);
    if (!size || std::memchr(value, '\0', size)) luaL_error(lua, "%s must be non-empty text", name);
    return value;
}

static Vec2 _vec2(lua_State* lua, int index, const char* name)
{
    luaL_checktype(lua, index, LUA_TTABLE);
    index = lua_absindex(lua, index);
    Vec2 value;
    auto keyed = _has(lua, index, "x") || _has(lua, index, "y");
    if (keyed) {
        static const char* const options[] = {"x", "y", nullptr};
        _options(lua, index, name, options);
        if (!_has(lua, index, "x") || !_has(lua, index, "y")) {
            luaL_error(lua, "%s requires x and y", name);
        }
        lua_getfield(lua, index, "x");
        value.x = _number(lua, -1, name);
        lua_pop(lua, 1);
        lua_getfield(lua, index, "y");
        value.y = _number(lua, -1, name);
        lua_pop(lua, 1);
        return value;
    }
    if (lua_rawlen(lua, index) != 2u) luaL_error(lua, "%s requires two numbers", name);
    lua_rawgeti(lua, index, 1);
    value.x = _number(lua, -1, name);
    lua_pop(lua, 1);
    lua_rawgeti(lua, index, 2);
    value.y = _number(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

static Vec2 _fieldVec2(lua_State* lua, int index, const char* name,
                       const Vec2& fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    auto value = _vec2(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

static Direction _direction(lua_State* lua, int index)
{
    auto value = luaL_checkstring(lua, index);
    if (std::strcmp(value, "lr") == 0 || std::strcmp(value, "left_to_right") == 0) {
        return Direction::LeftToRight;
    }
    if (std::strcmp(value, "tb") == 0 || std::strcmp(value, "top_to_bottom") == 0) {
        return Direction::TopToBottom;
    }
    luaL_error(lua, "diagram direction must be 'lr' or 'tb'");
    return Direction::LeftToRight;
}

static Layout _layout(lua_State* lua, int index)
{
    auto value = luaL_checkstring(lua, index);
    if (std::strcmp(value, "ranked") == 0) return Layout::Ranked;
    if (std::strcmp(value, "manual") == 0) return Layout::Manual;
    if (std::strcmp(value, "grid") == 0) return Layout::Grid;
    if (std::strcmp(value, "timeline") == 0) return Layout::Timeline;
    luaL_error(lua, "diagram layout must be ranked, manual, grid or timeline");
    return Layout::Ranked;
}

static NodeKind _nodeKind(lua_State* lua, int index)
{
    auto value = luaL_checkstring(lua, index);
    if (std::strcmp(value, "default") == 0) return NodeKind::Default;
    if (std::strcmp(value, "state") == 0) return NodeKind::State;
    if (std::strcmp(value, "decision") == 0) return NodeKind::Decision;
    if (std::strcmp(value, "terminal") == 0) return NodeKind::Terminal;
    if (std::strcmp(value, "entity") == 0) return NodeKind::Entity;
    if (std::strcmp(value, "cell") == 0) return NodeKind::Cell;
    if (std::strcmp(value, "task") == 0) return NodeKind::Task;
    if (std::strcmp(value, "evidence") == 0) return NodeKind::Evidence;
    luaL_error(lua, "invalid diagram node kind");
    return NodeKind::Default;
}

static EdgeKind _edgeKind(lua_State* lua, int index)
{
    auto value = luaL_checkstring(lua, index);
    if (std::strcmp(value, "directed") == 0) return EdgeKind::Directed;
    if (std::strcmp(value, "relation") == 0) return EdgeKind::Relation;
    if (std::strcmp(value, "return") == 0) return EdgeKind::Return;
    if (std::strcmp(value, "error") == 0) return EdgeKind::Error;
    if (std::strcmp(value, "optional") == 0) return EdgeKind::Optional;
    luaL_error(lua, "invalid diagram edge kind");
    return EdgeKind::Directed;
}

static Port _port(lua_State* lua, int index)
{
    auto value = luaL_checkstring(lua, index);
    if (std::strcmp(value, "auto") == 0) return Port::Auto;
    if (std::strcmp(value, "left") == 0) return Port::Left;
    if (std::strcmp(value, "right") == 0) return Port::Right;
    if (std::strcmp(value, "top") == 0) return Port::Top;
    if (std::strcmp(value, "bottom") == 0) return Port::Bottom;
    luaL_error(lua, "diagram port must be auto, left, right, top or bottom");
    return Port::Auto;
}

static Route _route(lua_State* lua, int index)
{
    auto value = luaL_checkstring(lua, index);
    if (std::strcmp(value, "auto") == 0) return Route::Auto;
    if (std::strcmp(value, "straight") == 0) return Route::Straight;
    if (std::strcmp(value, "orthogonal") == 0) return Route::Orthogonal;
    luaL_error(lua, "diagram route must be auto, straight or orthogonal");
    return Route::Auto;
}

static LuaDiagram* _diagram(lua_State* lua, int index = 1)
{
    auto handle = static_cast<LuaDiagram*>(luaL_checkudata(lua, index, DIAGRAM_METATABLE));
    if (!handle->diagram) luaL_error(lua, "diagram is no longer available");
    if (!detail::luaAuthoring(lua)) {
        luaL_error(lua, "diagram authoring is unavailable after loading");
    }
    return handle;
}

template<typename T>
static LuaHandle<T>* _handle(lua_State* lua, int index, const char* metatable,
                             LuaDiagram* owner)
{
    auto handle = static_cast<LuaHandle<T>*>(luaL_checkudata(lua, index, metatable));
    if (!handle->owner || handle->owner != owner || !static_cast<bool>(handle->value)) {
        luaL_error(lua, "diagram handle belongs to another diagram");
    }
    return handle;
}

static LuaBuilt* _built(lua_State* lua)
{
    auto handle = static_cast<LuaBuilt*>(luaL_checkudata(lua, 1, BUILT_METATABLE));
    if (!handle->built || !handle->owner) luaL_error(lua, "built diagram is no longer available");
    return handle;
}

static int _diagramGc(lua_State* lua)
{
    auto handle = static_cast<LuaDiagram*>(luaL_checkudata(lua, 1, DIAGRAM_METATABLE));
    delete handle->diagram;
    handle->diagram = nullptr;
    handle->scene = nullptr;
    return 0;
}

static int _builtGc(lua_State* lua)
{
    auto handle = static_cast<LuaBuilt*>(luaL_checkudata(lua, 1, BUILT_METATABLE));
    delete handle->built;
    handle->built = nullptr;
    handle->owner = nullptr;
    handle->root = nullptr;
    return 0;
}

static Layers _fieldLayers(lua_State* lua, int index, const Layers& fallback)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, "layers");
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    luaL_checktype(lua, -1, LUA_TTABLE);
    auto layerIndex = lua_absindex(lua, -1);
    static const char* const options[] = {
        "zones", "routes", "nodes", "annotations", nullptr,
    };
    _options(lua, layerIndex, "diagram layers", options);
    auto layers = fallback;
    layers.zones = _fieldInteger(lua, layerIndex, "zones", layers.zones);
    layers.routes = _fieldInteger(lua, layerIndex, "routes", layers.routes);
    layers.nodes = _fieldInteger(lua, layerIndex, "nodes", layers.nodes);
    layers.annotations = _fieldInteger(lua, layerIndex, "annotations", layers.annotations);
    lua_pop(lua, 1);
    return layers;
}

static int _newDiagram(lua_State* lua)
{
    auto scene = detail::luaScene(lua, 1);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "id", "layout", "direction", "origin", "node_size", "rank_gap", "node_gap",
        "time_unit", "zone_padding", "route_width", "arrow_length", "arrow_width",
        "corner", "layers", nullptr,
    };
    _options(lua, 2, "diagram config", options);
    Config config;
    if (_has(lua, 2, "id")) config.id = _fieldString(lua, 2, "id", true);
    if (_has(lua, 2, "layout")) {
        lua_getfield(lua, 2, "layout");
        config.layout = _layout(lua, -1);
        lua_pop(lua, 1);
    }
    if (_has(lua, 2, "direction")) {
        lua_getfield(lua, 2, "direction");
        config.direction = _direction(lua, -1);
        lua_pop(lua, 1);
    }
    config.origin = _fieldVec2(lua, 2, "origin", config.origin);
    config.nodeSize = _fieldVec2(lua, 2, "node_size", config.nodeSize);
    config.rankGap = _fieldNumber(lua, 2, "rank_gap", config.rankGap);
    config.nodeGap = _fieldNumber(lua, 2, "node_gap", config.nodeGap);
    config.timeUnit = _fieldNumber(lua, 2, "time_unit", config.timeUnit);
    config.zonePadding = _fieldNumber(lua, 2, "zone_padding", config.zonePadding);
    config.routeWidth = _fieldNumber(lua, 2, "route_width", config.routeWidth);
    config.arrowLength = _fieldNumber(lua, 2, "arrow_length", config.arrowLength);
    config.arrowWidth = _fieldNumber(lua, 2, "arrow_width", config.arrowWidth);
    // An omitted or negative corner follows the Scene Theme.
    config.corner = _fieldNumber(lua, 2, "corner", config.corner);
    config.layers = _fieldLayers(lua, 2, config.layers);
    auto diagram = Diagram::gen(config);
    if (!diagram) return luaL_error(lua, "could not create diagram: invalid config or out of memory");
    auto handle = new (lua_newuserdatauv(lua, sizeof(LuaDiagram), 1)) LuaDiagram;
    handle->diagram = diagram;
    handle->scene = scene;
    luaL_setmetatable(lua, DIAGRAM_METATABLE);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

template<typename T>
static int _pushHandle(lua_State* lua, LuaDiagram* owner, const T& value,
                       const char* metatable)
{
    auto handle = static_cast<LuaHandle<T>*>(lua_newuserdatauv(lua, sizeof(LuaHandle<T>), 1));
    handle->owner = owner;
    handle->value = value;
    luaL_setmetatable(lua, metatable);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    return 1;
}

static int _addNode(lua_State* lua)
{
    auto owner = _diagram(lua);
    if (owner->built) return luaL_error(lua, "diagram is already built");
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "id", "label", "detail", "kind", "rank", "row", "column", "start", "span",
        "position", "size", nullptr,
    };
    _options(lua, 2, "diagram node", options);
    NodeSpec spec;
    spec.id = _fieldString(lua, 2, "id", true);
    spec.label = _fieldString(lua, 2, "label");
    spec.detail = _fieldString(lua, 2, "detail");
    if (_has(lua, 2, "kind")) {
        lua_getfield(lua, 2, "kind");
        spec.kind = _nodeKind(lua, -1);
        lua_pop(lua, 1);
    }
    spec.rank = _fieldInteger(lua, 2, "rank", spec.rank);
    spec.row = _fieldInteger(lua, 2, "row", spec.row);
    spec.column = _fieldInteger(lua, 2, "column", spec.column);
    spec.start = _fieldNumber(lua, 2, "start", spec.start);
    spec.span = _fieldNumber(lua, 2, "span", spec.span);
    spec.manualPosition = _has(lua, 2, "position");
    spec.position = _fieldVec2(lua, 2, "position", spec.position);
    spec.size = _fieldVec2(lua, 2, "size", spec.size);
    Node node;
    auto result = owner->diagram->add(spec, node);
    if (result != Result::Success) {
        return luaL_error(lua, "could not add diagram node: %s", vis::result(result));
    }
    return _pushHandle(lua, owner, node, NODE_METATABLE);
}

static int _addEdge(lua_State* lua)
{
    auto owner = _diagram(lua);
    if (owner->built) return luaL_error(lua, "diagram is already built");
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "id", "label", "from", "to", "from_port", "to_port", "route", "kind",
        "waypoints", "flow", "flow_offset", nullptr,
    };
    _options(lua, 2, "diagram edge", options);
    EdgeSpec spec;
    spec.id = _fieldString(lua, 2, "id", true);
    spec.label = _fieldString(lua, 2, "label");
    lua_getfield(lua, 2, "from");
    spec.from = _handle<Node>(lua, -1, NODE_METATABLE, owner)->value;
    lua_pop(lua, 1);
    lua_getfield(lua, 2, "to");
    spec.to = _handle<Node>(lua, -1, NODE_METATABLE, owner)->value;
    lua_pop(lua, 1);
    if (_has(lua, 2, "from_port")) {
        lua_getfield(lua, 2, "from_port");
        spec.fromPort = _port(lua, -1);
        lua_pop(lua, 1);
    }
    if (_has(lua, 2, "to_port")) {
        lua_getfield(lua, 2, "to_port");
        spec.toPort = _port(lua, -1);
        lua_pop(lua, 1);
    }
    if (_has(lua, 2, "route")) {
        lua_getfield(lua, 2, "route");
        spec.route = _route(lua, -1);
        lua_pop(lua, 1);
    }
    if (_has(lua, 2, "kind")) {
        lua_getfield(lua, 2, "kind");
        spec.kind = _edgeKind(lua, -1);
        lua_pop(lua, 1);
    }
    spec.flow = _fieldBool(lua, 2, "flow", spec.flow);
    spec.flowOffset = _fieldNumber(lua, 2, "flow_offset", spec.flowOffset);
    if (_has(lua, 2, "waypoints")) {
        lua_getfield(lua, 2, "waypoints");
        luaL_checktype(lua, -1, LUA_TTABLE);
        auto waypointIndex = lua_absindex(lua, -1);
        auto count = lua_rawlen(lua, waypointIndex);
        if (!count || count > Diagram::WaypointLimit) {
            return luaL_error(lua, "diagram waypoints must contain between 1 and %u points",
                              Diagram::WaypointLimit);
        }
        auto points = static_cast<Vec2*>(lua_newuserdatauv(lua, sizeof(Vec2) * count, 0));
        for (auto i = 0u; i < count; i++) {
            lua_rawgeti(lua, waypointIndex, i + 1u);
            points[i] = _vec2(lua, -1, "diagram waypoint");
            lua_pop(lua, 1);
        }
        spec.waypoints = points;
        spec.waypointCount = static_cast<uint32_t>(count);
    }
    Edge edge;
    auto result = owner->diagram->add(spec, edge);
    if (result != Result::Success) {
        return luaL_error(lua, "could not add diagram edge: %s", vis::result(result));
    }
    return _pushHandle(lua, owner, edge, EDGE_METATABLE);
}

static int _addZone(lua_State* lua)
{
    auto owner = _diagram(lua);
    if (owner->built) return luaL_error(lua, "diagram is already built");
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {"id", "label", "members", "full_width", nullptr};
    _options(lua, 2, "diagram zone", options);
    ZoneSpec spec;
    spec.id = _fieldString(lua, 2, "id", true);
    spec.label = _fieldString(lua, 2, "label");
    lua_getfield(lua, 2, "members");
    luaL_checktype(lua, -1, LUA_TTABLE);
    auto memberIndex = lua_absindex(lua, -1);
    auto count = lua_rawlen(lua, memberIndex);
    if (!count || count > Diagram::NodeLimit) {
        return luaL_error(lua, "diagram zone requires between 1 and %u members",
                          Diagram::NodeLimit);
    }
    auto members = static_cast<Node*>(lua_newuserdatauv(lua, sizeof(Node) * count, 0));
    for (auto i = 0u; i < count; i++) {
        lua_rawgeti(lua, memberIndex, i + 1u);
        members[i] = _handle<Node>(lua, -1, NODE_METATABLE, owner)->value;
        lua_pop(lua, 1);
    }
    spec.members = members;
    spec.memberCount = static_cast<uint32_t>(count);
    spec.fullWidth = _fieldBool(lua, 2, "full_width", spec.fullWidth);
    Zone zone;
    auto result = owner->diagram->add(spec, zone);
    if (result != Result::Success) {
        return luaL_error(lua, "could not add diagram zone: %s", vis::result(result));
    }
    return _pushHandle(lua, owner, zone, ZONE_METATABLE);
}

static void _releaseBuilt(void* data) noexcept
{
    static_cast<Built*>(data)->release();
}

static int _build(lua_State* lua)
{
    auto owner = _diagram(lua);
    if (owner->built) return luaL_error(lua, "diagram is already built");
    Built* built = nullptr;
    auto result = owner->diagram->build(owner->scene->theme(), built);
    if (result != Result::Success || !built) {
        delete built;
        return luaL_error(lua, "could not build diagram: %s", vis::result(result));
    }
    auto receipt = built->ir();
    if (!receipt) {
        delete built;
        return luaL_error(lua, "could not build diagram receipt");
    }
    auto receiptBytes = receipt->byteSize();
    auto handle = static_cast<LuaBuilt*>(lua_newuserdatauv(lua, sizeof(LuaBuilt), 2));
    handle->built = built;
    handle->owner = owner;
    handle->root = built->root();
    luaL_setmetatable(lua, BUILT_METATABLE);
    lua_getiuservalue(lua, 1, 1);
    lua_setiuservalue(lua, -2, 1);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 2);
    auto builtIndex = lua_absindex(lua, -1);
    lua_getiuservalue(lua, builtIndex, 1);
    auto sceneIndex = lua_absindex(lua, -1);
    detail::luaAdoptObject(lua, sceneIndex, handle->root, "diagram tree",
                           _releaseBuilt, built, receiptBytes);
    owner->built = true;
    lua_settop(lua, builtIndex);
    return 1;
}

static int _push(lua_State* lua, Object* object, bool optional = false)
{
    if (!object) {
        if (!optional) return luaL_error(lua, "diagram object is unavailable");
        lua_pushnil(lua);
        return 1;
    }
    lua_getiuservalue(lua, 1, 1);
    auto sceneIndex = lua_absindex(lua, -1);
    detail::luaPushObject(lua, sceneIndex, object);
    lua_remove(lua, sceneIndex);
    return 1;
}

static int _root(lua_State* lua) { auto value = _built(lua); return _push(lua, value->root); }
static int _zones(lua_State* lua) { auto value = _built(lua); return _push(lua, value->built->zones()); }
static int _routes(lua_State* lua) { auto value = _built(lua); return _push(lua, value->built->routes()); }
static int _nodes(lua_State* lua) { auto value = _built(lua); return _push(lua, value->built->nodes()); }
static int _annotations(lua_State* lua) { auto value = _built(lua); return _push(lua, value->built->annotations()); }

static int _nodeObject(lua_State* lua, int kind)
{
    auto value = _built(lua);
    auto node = _handle<Node>(lua, 2, NODE_METATABLE, value->owner)->value;
    Object* object = nullptr;
    if (kind == 0) object = value->built->object(node);
    else if (kind == 1) object = value->built->body(node);
    else if (kind == 2) object = value->built->label(node);
    else object = value->built->detail(node);
    return _push(lua, object, kind == 3);
}

static int _node(lua_State* lua) { return _nodeObject(lua, 0); }
static int _nodeBody(lua_State* lua) { return _nodeObject(lua, 1); }
static int _nodeLabel(lua_State* lua) { return _nodeObject(lua, 2); }
static int _nodeDetail(lua_State* lua) { return _nodeObject(lua, 3); }

static int _edgeObject(lua_State* lua, int kind)
{
    auto value = _built(lua);
    auto edge = _handle<Edge>(lua, 2, EDGE_METATABLE, value->owner)->value;
    Object* object = nullptr;
    if (kind == 0) object = value->built->object(edge);
    else object = value->built->label(edge);
    return _push(lua, object, kind != 0);
}

static int _edge(lua_State* lua) { return _edgeObject(lua, 0); }
static int _edgeLabel(lua_State* lua) { return _edgeObject(lua, 1); }

static int _zoneObject(lua_State* lua, int kind)
{
    auto value = _built(lua);
    auto zone = _handle<Zone>(lua, 2, ZONE_METATABLE, value->owner)->value;
    Object* object = nullptr;
    if (kind == 0) object = value->built->object(zone);
    else if (kind == 1) object = value->built->body(zone);
    else object = value->built->label(zone);
    return _push(lua, object);
}

static int _zone(lua_State* lua) { return _zoneObject(lua, 0); }
static int _zoneBody(lua_State* lua) { return _zoneObject(lua, 1); }
static int _zoneLabel(lua_State* lua) { return _zoneObject(lua, 2); }

static void _plainMetatable(lua_State* lua, const char* name)
{
    luaL_newmetatable(lua, name);
    lua_pushstring(lua, name);
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_pop(lua, 1);
}

static int _open(lua_State* lua, void*)
{
    static const luaL_Reg diagramMethods[] = {
        {"node", _addNode}, {"connect", _addEdge}, {"zone", _addZone},
        {"build", _build}, {nullptr, nullptr},
    };
    static const luaL_Reg builtMethods[] = {
        {"root", _root}, {"zones", _zones}, {"routes", _routes},
        {"nodes", _nodes}, {"annotations", _annotations}, {"node", _node},
        {"node_body", _nodeBody}, {"node_label", _nodeLabel},
        {"node_detail", _nodeDetail}, {"edge", _edge},
        {"edge_label", _edgeLabel}, {"zone", _zone}, {"zone_body", _zoneBody},
        {"zone_label", _zoneLabel}, {nullptr, nullptr},
    };
    luaL_newmetatable(lua, DIAGRAM_METATABLE);
    lua_pushcfunction(lua, _diagramGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "vis.diagram.Diagram");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, diagramMethods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);
    _plainMetatable(lua, NODE_METATABLE);
    _plainMetatable(lua, EDGE_METATABLE);
    _plainMetatable(lua, ZONE_METATABLE);

    luaL_newmetatable(lua, BUILT_METATABLE);
    lua_pushcfunction(lua, _builtGc);
    lua_setfield(lua, -2, "__gc");
    lua_pushliteral(lua, "vis.diagram.Built");
    lua_setfield(lua, -2, "__name");
    lua_pushliteral(lua, "protected");
    lua_setfield(lua, -2, "__metatable");
    lua_newtable(lua);
    luaL_setfuncs(lua, builtMethods, 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    lua_getglobal(lua, "vis");
    lua_pushcfunction(lua, _newDiagram);
    lua_setfield(lua, -2, "diagram");
    lua_pop(lua, 1);
    return 0;
}

}  // namespace

Result Lua::load(const char* source, uint32_t size, const char* name, Scene** scene,
                 char* error, uint32_t errorSize, vis::Lua::AssetResolver resolver,
                 void* resolverData, const Theme* adaptiveTheme,
                 bool* adaptiveThemeUsed) noexcept
{
    auto hook = lua_extension::luaHooks();
    detail::LuaHookSpan hooks = {&hook, 1u};
    return detail::luaLoad(source, size, name, scene, error, errorSize, resolver,
                           resolverData, nullptr, 0u, adaptiveTheme,
                           adaptiveThemeUsed, hooks);
}

vis::detail::LuaHooks lua_extension::luaHooks() noexcept
{
    return {_open, nullptr, nullptr};
}

}  // namespace vis::diagram
