#include <cmath>
#include <cstdio>

#include "vis_states.h"
#if defined(VIS_LUA_RUNTIME)
#include "vis_lua_runtime.h"
#endif
#include "testLuaCollector.h"
#include "testLuaLoad.h"

using namespace vis;

static void _controller()
{
    static constexpr char source[] = R"lua(
if not vis.states then error("states module missing") end
local scene = vis.scene {width = 320, height = 180}
local target = scene:rectangle {id = "target", center = {1, 0}, size = {2, 1}}
local states = vis.states(scene)
states:define(target, "idle", {})
states:define(target, "focused", {
    shift = {2, 1}, rotation = 0.2, scale = {1.5, 1.25},
    opacity = 0.75, progress = 0.5,
})
states:transition(target, "focused", {duration = 1, curve = "linear"})
states:advance(0.25)
local state = states:sample(target)
if math.abs(state.shift[1] - 0.5) > 0.0001 then error("sample mismatch") end
if math.abs(state.scale[3] - 1) > 0.0001 then error("2D scale changed Z") end
if states:definition_count() ~= 2 then error("definition count mismatch") end
if states:event_count() ~= 1 then error("event count mismatch") end
local event = states:event(1)
if event.object ~= 1 or event.state ~= "focused" then error("event mismatch") end
if event.transaction ~= "1" or event.curve.preset ~= "linear" then
    error("event record mismatch")
end
return scene
)lua";
    detail::LuaHostContext context;
    auto scene = _loaded(source, "states.lua", context);
    auto controller = scene ? context.states.controller : nullptr;
    CHECK(controller && controller->scene() == scene);
    if (scene && controller) {
        states::State state;
        auto object = scene->object("target");
        CHECK(object != nullptr);
        CHECK(controller->sample(object, state) == Result::Success);
        CHECK(near(state.shift.x, 0.5f));
        CHECK(controller->eventCount() == 1u);
    }
    delete scene;
}

static void _failures()
{
    static constexpr char unknownField[] = R"lua(
local scene = vis.scene {}
local target = scene:circle {id = "target"}
local states = vis.states(scene)
states:define(target, "idle", {unknown = true})
return scene
)lua";
    _scriptError(unknownField, "invalid-states.lua", "unknown state field");

    static constexpr char duplicate[] = R"lua(
local scene = vis.scene {}
vis.states(scene)
vis.states(scene)
return scene
)lua";
    _scriptError(duplicate, "duplicate-states.lua", "only one states controller");

    static constexpr char aborted[] = R"lua(
local scene = vis.scene {}
vis.states(scene)
error("abort after states creation")
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    CHECK(_load(aborted, "aborted-states.lua", scene, error, sizeof(error), context) == Result::ScriptError);
    CHECK(scene == nullptr);
    CHECK(context.states.controller == nullptr && context.states.scene == nullptr);

    struct InvalidCall {const char* source; const char* error;};
    static constexpr InvalidCall batches[] = {
        {"states:transition({})", "transition targets must contain"},
        {"states:transition({{object=a, state='idle'}, 1})", "transition target must be a table, got number"},
        {"states:transition({{object=a, state=true}})", "transition target state must be a string, got boolean"},
        {"states:transition({{object=a, state='idle', extra=true}})", "extra"},
        {"states:transition({{object=a, state='idle'}, {object=a, state='idle'}})", "transition failed"},
        {"states:transition({{object=a, state='idle'}, {object=b, state='missing'}})", "transition failed"},
        {"states:transition({{object=a, state='idle'}}, {duration=-1})", "duration"},
        {"states:transition({{object=a, state='idle'}}, {duration = 1, curve = 'unknown'})", "curve"},
    };
    for (const auto& batch : batches) {
        char source[1024];
        std::snprintf(source, sizeof(source), R"lua(
local scene = vis.scene {}
local a = scene:circle {}
local b = scene:circle {}
local states = vis.states(scene)
states:define(a, 'idle', {})
states:define(b, 'idle', {})
%s
return scene
)lua", batch.source);
        _scriptError(source, "invalid-batch.lua", batch.error);
    }
}

// States named by numbers are strings Lua makes of them, which only the stack holds; the
// collecting hook would free and scribble over each one as the next is turned into text if
// transition did not keep them until the controller has read them. The last call leaves out
// its duration and curve, which must read as nil past what the call keeps on the stack.
static void _numericStates()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 320, height = 180}
local a = scene:circle {id = "a", radius = 1}
local b = scene:circle {id = "b", radius = 0.5}
local c = scene:circle {id = "c", radius = 0.25}
local d = scene:circle {id = "d", radius = 0.125}
local states = vis.states(scene)
states:define(a, 101, {opacity = 0.5})
states:define(b, 202, {opacity = 0.25})
states:define(c, 303, {opacity = 0.75})
states:define(d, 404, {opacity = 0.625})
states:transition({{object = a, state = 101}, {object = b, state = 202}}, {duration = 1, curve = "linear"})
states:transition({{object = c, state = 303}})
states:transition(d, 404, {})
if states:event_count() ~= 4 then error("transition event count mismatch") end
local first, second = states:event(1), states:event(2)
local third, fourth = states:event(3), states:event(4)
if first.transaction ~= second.transaction or second.transaction == third.transaction
    or third.transaction == fourth.transaction then error("batch transaction mismatch") end
if first.duration ~= 1 or first.curve.preset ~= "linear"
    or math.abs(third.duration - 0.2) > 0.0001 or third.curve.preset ~= "smooth"
    or math.abs(fourth.duration - 0.2) > 0.0001 or fourth.curve.preset ~= "smooth" then
    error("transition defaults mismatch")
end
states:advance(1)
return scene
)lua";
    states::lua_extension::LuaContext context;
    detail::LuaHooks hooks[] = {states::lua_extension::luaHooks(context), collectingHooks()};
    Scene* scene = nullptr;
    char error[1024] = {};
    auto result = detail::luaLoad(source, sizeof(source) - 1u, "numeric-states.lua", &scene, error,
                                  sizeof(error), nullptr, nullptr, nullptr, 0u, nullptr, nullptr,
                                  {hooks, 2u});
    CHECK(result == Result::Success);
    CHECK(scene && context.controller);
    if (result != Result::Success) std::fprintf(stderr, "numeric states: %s\n", error);
    if (scene && context.controller) {
        const char* ids[] = {"a", "b", "c", "d"};
        float opacities[] = {0.5f, 0.25f, 0.75f, 0.625f};
        for (auto i = 0u; i < 4u; i++) {
            states::State state;
            CHECK(context.controller->sample(scene->object(ids[i]), state) == Result::Success);
            CHECK(near(state.opacity, opacities[i]));
        }
    }
    delete scene;
}

#if defined(VIS_LUA_RUNTIME)
static void _retainedRuntime()
{
    static constexpr char source[] = R"lua(
if not vis.states then error("states module missing") end
if not vis.runtime then error("runtime module missing") end
local scene = vis.scene {width = 320, height = 180}
local target = scene:rectangle {id = "target", size = {1, 1}}
local states = vis.states(scene)
states:define(target, "right", {shift = {2, 0}})
states:define(target, "up", {shift = {1, 2}})
states:transition(target, "right", {duration = 1, curve = "linear"})
vis.runtime(scene, {
    fixed_step = 0.25,
    max_steps = 4,
    update = function(ctx, dt, time, tick)
        states:advance(dt)
        if tick == 2 then
            local current = states:sample(target)
            if math.abs(current.shift[1] - 1) > 0.0001 then
                error("capture-current mismatch")
            end
            states:transition(target, "up", {duration = 1, curve = "linear"})
        end
    end,
})
return scene
)lua";
    detail::LuaHostContext context;
    auto scene = _loaded(source, "states-runtime.lua", context);
    CHECK(context.states.controller && context.runtime.runtime);
    if (scene && context.states.controller && context.runtime.runtime) {
        CHECK(context.runtime.runtime->advance(0.5f) == Result::Success);
        states::State state;
        auto object = scene->object("target");
        CHECK(context.states.controller->sample(object, state) == Result::Success);
        CHECK(near(state.shift.x, 1.0f));
        CHECK(near(state.shift.y, 0.0f));
        CHECK(context.runtime.runtime->advance(0.5f) == Result::Success);
        CHECK(context.states.controller->sample(object, state) == Result::Success);
        CHECK(near(state.shift.x, 1.0f));
        CHECK(near(state.shift.y, 1.0f));
        CHECK(context.states.controller->eventCount() == 2u);
    }
    delete scene;
}
#endif

int main()
{
    _controller();
    _failures();
    _numericStates();
#if defined(VIS_LUA_RUNTIME)
    _retainedRuntime();
#endif
    return report();
}
