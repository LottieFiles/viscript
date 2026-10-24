// The Lua binding of the UI module: panels with buttons, sliders, toggles, sample areas and
// camera controls.
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "vis_ui.h"
#include "../lua/visLuaExtension.h"
#include "../lua/visLuaValues.h"
#include "visUiLuaExtension.h"

namespace vis::ui
{

static constexpr const char* PANEL_METATABLE = "vis.ui.Panel";

struct LuaPanel
{
    Panel* panel = nullptr;
};

static Panel* _panel(lua_State* lua)
{
    if (!detail::luaAuthoring(lua)) {
        luaL_error(lua, "UI authoring is unavailable after loading");
    }
    return static_cast<LuaPanel*>(luaL_checkudata(lua, 1, PANEL_METATABLE))->panel;
}

static Vec2 _vec2(lua_State* lua, int index, const char* name)
{
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    Vec2 value;
    if (_has(lua, index, "x") || _has(lua, index, "y")) {
        if (!_has(lua, index, "x") || !_has(lua, index, "y")) {
            luaL_error(lua, "%s must contain x and y", name);
        }
        lua_getfield(lua, index, "x");
        value.x = _number(lua, -1, name);
        lua_pop(lua, 1);
        lua_getfield(lua, index, "y");
        value.y = _number(lua, -1, name);
        lua_pop(lua, 1);
        return value;
    }
    if (lua_rawlen(lua, index) != 2u) luaL_error(lua, "%s must contain two numbers", name);
    lua_rawgeti(lua, index, 1);
    value.x = _number(lua, -1, name);
    lua_pop(lua, 1);
    lua_rawgeti(lua, index, 2);
    value.y = _number(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

// A vector field that gives all of x, y and z, as keys or three numbers, read as _fieldVec3
// reads it.
static Vec3 _fieldXyz(lua_State* lua, int index, const char* name, const Vec3& fallback = {})
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    auto listed = lua_istable(lua, -1) && !_has(lua, -1, "x") && !_has(lua, -1, "y")
                  && !_has(lua, -1, "z");
    if (listed && lua_rawlen(lua, -1) != 3u) {
        luaL_error(lua, "%s must contain three numbers", name);
    }
    lua_pop(lua, 1);
    return _fieldVec3(lua, index, name, fallback);
}

static Object* _fieldOptionalObject(lua_State* lua, int index, const char* name,
                                    Scene* scene)
{
    return _has(lua, index, name) ? _fieldObject(lua, index, name, scene) : nullptr;
}

static UITransform _transform(lua_State* lua, int index, const char* name)
{
    UITransform value;
    if (lua_isnil(lua, index)) return value;
    _checkTable(lua, index, name);
    index = lua_absindex(lua, index);
    static const char* const options[] = {
        "shift", "scale", "rotation", "opacity", "progress", nullptr,
    };
    _options(lua, index, "UI config", options);
    value.shift = _fieldXyz(lua, index, "shift", value.shift);
    value.scale = _fieldXyz(lua, index, "scale", value.scale);
    value.rotation = _fieldXyz(lua, index, "rotation", value.rotation);
    value.opacity = _fieldNumber(lua, index, "opacity", value.opacity);
    value.progress = _fieldNumber(lua, index, "progress", value.progress);
    if (value.opacity < 0.0f || value.opacity > 1.0f
        || value.progress < 0.0f || value.progress > 1.0f) {
        luaL_error(lua, "%s opacity and progress must be between zero and one", name);
    }
    return value;
}

static UITransform _fieldTransform(lua_State* lua, int index, const char* name)
{
    index = lua_absindex(lua, index);
    lua_getfield(lua, index, name);
    auto value = _transform(lua, -1, name);
    lua_pop(lua, 1);
    return value;
}

// The names of a slider's axis and of camera actions shared by buttons and direct commands.
static constexpr LuaName<SliderAxis> SLIDER_AXES[] = {
    {"horizontal", SliderAxis::Horizontal}, {"vertical", SliderAxis::Vertical}};
static constexpr LuaName<CameraAction> CAMERA_ACTIONS[] = {
    {"pan", CameraAction::Pan}, {"orbit", CameraAction::Orbit},
    {"zoom", CameraAction::Zoom}, {"reset", CameraAction::Reset},
    {"view2d", CameraAction::View2D}, {"view3d", CameraAction::View3D}};

// Adds a control just made to the panel; one that is not added is freed before the error.
template<typename Control>
static Control* _add(lua_State* lua, Panel* panel, Control* control, const char* name)
{
    if (!control) luaL_error(lua, "could not allocate %s", name);
    auto result = panel->add(control);
    if (result != Result::Success) {
        delete control;
        luaL_error(lua, "%s", vis::result(result));
    }
    return control;
}

static int _button(lua_State* lua)
{
    auto panel = _panel(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "visual", "hover_visual", "pressed_visual", "region", "camera", "target",
        "delta", "transform", "duration", nullptr,
    };
    _options(lua, 2, "UI config", options);
    auto scene = panel->scene();
    auto visual = _fieldObject(lua, 2, "visual", scene);
    auto hover = _fieldOptionalObject(lua, 2, "hover_visual", scene);
    auto pressed = _fieldOptionalObject(lua, 2, "pressed_visual", scene);
    auto region = _fieldPointerRegion(lua, 2);
    auto hasCamera = _has(lua, 2, "camera");
    auto hasTarget = _has(lua, 2, "target");
    if (hasCamera == hasTarget) {
        return luaL_error(lua, "button requires exactly one of camera or target");
    }
    auto action = CameraAction::Reset;
    Object* target = nullptr;
    UITransform transform;
    auto duration = _fieldNumber(lua, 2, "duration", 0.0f);
    if (duration < 0.0f) {
        return luaL_error(lua, "button duration must be a finite non-negative number");
    }
    if (hasCamera) {
        action = _fieldEnum(lua, 2, "camera", action, CAMERA_ACTIONS, "camera action");
        if (_has(lua, 2, "transform")) {
            return luaL_error(lua, "camera button cannot contain transform");
        }
        if (_has(lua, 2, "duration")) {
            return luaL_error(lua, "camera button cannot contain duration");
        }
    } else {
        if (_has(lua, 2, "delta")) {
            return luaL_error(lua, "target button cannot contain delta");
        }
        target = _fieldObject(lua, 2, "target", scene);
        transform = _fieldTransform(lua, 2, "transform");
    }
    auto delta = _field<_vec2>(lua, 2, "delta", Vec2{});
    auto control = _add(lua, panel, Button::gen(visual, region), "button");
    auto result = hasCamera ? panel->bind(control, action, delta)
                            : panel->bind(control, target, transform, duration);
    if (result == Result::Success && (hover || pressed)) {
        result = panel->states(control, hover, pressed);
    }
    return _result(lua, result);
}

static int _slider(lua_State* lua)
{
    auto panel = _panel(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "visual", "target", "region", "axis", "value", "from", "to", "bindings",
        nullptr,
    };
    _options(lua, 2, "UI config", options);
    auto scene = panel->scene();
    auto visual = _fieldObject(lua, 2, "visual", scene);
    auto region = _fieldPointerRegion(lua, 2);
    auto axis = _fieldEnum(lua, 2, "axis", SliderAxis::Horizontal, SLIDER_AXES, "slider axis");
    auto initial = _fieldNumber(lua, 2, "value", 0.0f);
    if (initial < 0.0f || initial > 1.0f) {
        return luaL_error(lua, "slider value must be between zero and one");
    }

    struct SliderBinding
    {
        Object* target = nullptr;
        UITransform from;
        UITransform to;
    };
    SliderBinding bindings[Panel::BindingLimit];
    uint32_t bindingCount = 1u;
    if (_has(lua, 2, "bindings")) {
        if (_has(lua, 2, "target") || _has(lua, 2, "from") || _has(lua, 2, "to")) {
            return luaL_error(
                lua, "slider target/from/to cannot be combined with bindings");
        }
        lua_getfield(lua, 2, "bindings");
        _checkTable(lua, -1, "slider bindings");
        auto list = lua_absindex(lua, -1);
        auto count = lua_rawlen(lua, list);
        if (!count || count > Panel::BindingLimit) {
            return luaL_error(lua, "slider bindings must contain between 1 and %d items",
                              static_cast<int>(Panel::BindingLimit));
        }
        bindingCount = static_cast<uint32_t>(count);
        static const char* const bindingOptions[] = {
            "target", "from", "to", nullptr,
        };
        for (auto i = 0u; i < bindingCount; i++) {
            lua_rawgeti(lua, list, i + 1u);
            _checkTable(lua, -1, "a slider binding");
            auto binding = lua_absindex(lua, -1);
            _options(lua, binding, "UI config", bindingOptions);
            bindings[i].target = _fieldObject(lua, binding, "target", scene);
            bindings[i].from = _fieldTransform(lua, binding, "from");
            bindings[i].to = _fieldTransform(lua, binding, "to");
            lua_pop(lua, 1);
        }
        lua_pop(lua, 1);
    } else {
        bindings[0].target = _fieldObject(lua, 2, "target", scene);
        bindings[0].from = _fieldTransform(lua, 2, "from");
        bindings[0].to = _fieldTransform(lua, 2, "to");
    }
    auto control = _add(lua, panel, Slider::gen(visual, region, axis, initial), "slider");
    auto result = Result::Success;
    for (auto i = 0u; i < bindingCount; i++) {
        result = panel->bind(control, bindings[i].target, bindings[i].from,
                             bindings[i].to);
        if (result != Result::Success) break;
    }
    return _result(lua, result);
}

static int _toggleButton(lua_State* lua)
{
    auto panel = _panel(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "visual", "hover_visual", "pressed_visual", "region", "target",
        "off", "on", "value", "duration", nullptr,
    };
    _options(lua, 2, "UI config", options);
    if (!_has(lua, 2, "off") || !_has(lua, 2, "on")) {
        return luaL_error(lua, "toggle_button requires off and on transforms");
    }
    auto scene = panel->scene();
    auto visual = _fieldObject(lua, 2, "visual", scene);
    auto hover = _fieldOptionalObject(lua, 2, "hover_visual", scene);
    auto pressed = _fieldOptionalObject(lua, 2, "pressed_visual", scene);
    auto target = _fieldObject(lua, 2, "target", scene);
    auto region = _fieldPointerRegion(lua, 2);
    auto off = _fieldTransform(lua, 2, "off");
    auto on = _fieldTransform(lua, 2, "on");
    auto value = _fieldBool(lua, 2, "value", false);
    auto duration = _fieldNumber(lua, 2, "duration", 0.0f);
    if (duration < 0.0f) {
        return luaL_error(lua,
                          "toggle_button duration must be a finite non-negative number");
    }
    auto control = _add(lua, panel, Button::gen(visual, region), "toggle_button");
    auto result = panel->toggle(control, target, off, on, value, duration);
    if (result == Result::Success && (hover || pressed)) {
        result = panel->states(control, hover, pressed);
    }
    return _result(lua, result);
}

static int _sampleArea(lua_State* lua)
{
    auto panel = _panel(lua);
    luaL_checktype(lua, 2, LUA_TTABLE);
    static const char* const options[] = {
        "region", "targets", "marker", "marker_origin", "marker_x", "marker_y",
        "swatch", nullptr,
    };
    _options(lua, 2, "UI config", options);
    auto scene = panel->scene();
    auto region = _fieldPointerRegion(lua, 2);
    lua_getfield(lua, 2, "targets");
    _checkTable(lua, -1, "sample_area targets");
    auto targetCount = lua_rawlen(lua, -1);
    if (!targetCount || targetCount > SampleArea::TargetLimit) {
        return luaL_error(lua, "sample_area targets must contain between 1 and %d objects",
                          static_cast<int>(SampleArea::TargetLimit));
    }
    const Object* targets[SampleArea::TargetLimit];
    for (auto i = 0u; i < targetCount; i++) {
        lua_rawgeti(lua, -1, i + 1u);
        targets[i] = detail::luaObject(lua, scene, -1);
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);

    SampleBinding binding;
    binding.marker = _fieldOptionalObject(lua, 2, "marker", scene);
    binding.swatch = _fieldOptionalObject(lua, 2, "swatch", scene);
    if (!binding.marker
        && (_has(lua, 2, "marker_origin") || _has(lua, 2, "marker_x")
            || _has(lua, 2, "marker_y"))) {
        return luaL_error(lua, "sample_area marker transforms require marker");
    }
    binding.markerOrigin = _fieldXyz(lua, 2, "marker_origin");
    binding.markerX = _fieldXyz(lua, 2, "marker_x");
    binding.markerY = _fieldXyz(lua, 2, "marker_y");

    auto count = static_cast<uint32_t>(targetCount);
    auto control = _add(lua, panel, SampleArea::gen(targets, count, region), "sample_area");
    return _result(lua, panel->bind(control, binding));
}

static int _camera(lua_State* lua)
{
    auto panel = _panel(lua);
    auto action = _enum(lua, 2, "camera action", CAMERA_ACTIONS);
    Vec2 delta;
    if (action == CameraAction::Pan || action == CameraAction::Orbit) {
        delta = _vec2(lua, 3, "camera delta");
    } else if (action == CameraAction::Zoom) {
        delta.y = _number(lua, 3, "camera zoom");
    }
    return _result(lua, panel->camera(action, delta));
}

static int _newPanel(lua_State* lua)
{
    if (!detail::luaAuthoring(lua)) {
        return luaL_error(lua, "UI authoring is unavailable after loading");
    }
    auto context = static_cast<lua_extension::LuaContext*>(
        lua_touserdata(lua, lua_upvalueindex(1)));
    if (context->panel) return luaL_error(lua, "a scene can have only one UI panel");
    auto scene = detail::luaScene(lua, 1);
    auto panel = Panel::gen(scene);
    if (!panel) return luaL_error(lua, "could not create UI panel");
    context->panel = panel;
    context->scene = scene;
    auto handle = static_cast<LuaPanel*>(lua_newuserdatauv(lua, sizeof(LuaPanel), 1));
    handle->panel = panel;
    luaL_setmetatable(lua, PANEL_METATABLE);
    lua_pushvalue(lua, 1);
    lua_setiuservalue(lua, -2, 1);
    // Keep the Panel and its owning Scene alive until the loader validates the
    // returned root. Script code may otherwise abandon both and force collection.
    lua_pushvalue(lua, -1);
    luaL_ref(lua, LUA_REGISTRYINDEX);
    return 1;
}

static int _open(lua_State* lua, void* data)
{
    static const luaL_Reg methods[] = {
        {"button", _button},
        {"toggle_button", _toggleButton},
        {"slider", _slider},
        {"sample_area", _sampleArea},
        {"camera", _camera},
        {nullptr, nullptr},
    };
    detail::luaMetatable(lua, PANEL_METATABLE, nullptr, {methods});

    lua_getglobal(lua, "vis");
    lua_newtable(lua);
    lua_pushlightuserdata(lua, data);
    lua_pushcclosure(lua, _newPanel, 1);
    lua_setfield(lua, -2, "panel");
    lua_setfield(lua, -2, "ui");
    lua_pop(lua, 1);
    return 0;
}

vis::detail::LuaHooks lua_extension::luaHooks(lua_extension::LuaContext& context) noexcept
{
    return {_open, detail::luaLoadedScene<LuaContext, &LuaContext::panel>, &context};
}

}  // namespace vis::ui
