#include <cmath>
#include <cstdio>
#include <cstring>

#include "visLuaHost.h"
#include "vis_motion.h"
#if defined(VIS_LUA_RUNTIME)
#include "vis_lua_runtime.h"
#endif
#include "testLuaCollector.h"

using namespace vis;

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);     \
            failures++;                                                                            \
        }                                                                                          \
    } while (false)

static bool _near(float lhs, float rhs, float epsilon = 1.0e-4f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

static void _standalone()
{
    static constexpr char source[] = R"lua(
if not vis.motion then error("motion module missing") end
local scene = vis.scene {width = 320, height = 180}
local target = scene:rectangle {id = "target", center = {1, 0}, size = {2, 1}}
local motion = vis.motion(scene)
motion:define(target, "idle", {})
motion:define(target, "focused", {
    shift = {2, 1}, rotation = 0.2, scale = {1.5, 1.25},
    opacity = 0.75, progress = 0.5,
})
motion:transition(target, "focused", 1, "linear")
motion:advance(0.25)
local state = motion:sample(target)
if math.abs(state.shift[1] - 0.5) > 0.0001 then error("sample mismatch") end
if math.abs(state.scale[3] - 1) > 0.0001 then error("2D scale changed Z") end
if motion:definition_count() ~= 2 then error("definition count mismatch") end
if motion:event_count() ~= 1 then error("event count mismatch") end
local event = motion:event(1)
if event.object ~= 1 or event.state ~= "focused" then error("event mismatch") end
if event.transaction ~= "1" or event.curve.preset ~= "linear" then
    error("event record mismatch")
end
return scene
)lua";
    Scene* scene = nullptr;
    motion::Controller* controller = nullptr;
    char error[1024] = {};
    auto result = motion::Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                                    "motion.lua", &scene, &controller, error, sizeof(error));
    CHECK(result == Result::Success);
    CHECK(scene && controller && controller->scene() == scene);
    if (result != Result::Success) std::fprintf(stderr, "standalone motion: %s\n", error);
    if (scene && controller) {
        motion::State state;
        auto object = scene->object("target");
        CHECK(object != nullptr);
        CHECK(controller->sample(object, state) == Result::Success);
        CHECK(_near(state.shift.x, 0.5f));
        CHECK(controller->eventCount() == 1u);
    }
    delete scene;
}

static void _failures()
{
    static constexpr char unknownField[] = R"lua(
local scene = vis.scene {}
local target = scene:circle {id = "target"}
local motion = vis.motion(scene)
motion:define(target, "idle", {unknown = true})
return scene
)lua";
    Scene* scene = nullptr;
    motion::Controller* controller = nullptr;
    char error[1024] = {};
    auto result =
        motion::Lua::load(unknownField, static_cast<uint32_t>(std::strlen(unknownField)),
                          "invalid-motion.lua", &scene, &controller, error, sizeof(error));
    CHECK(result == Result::ScriptError);
    CHECK(scene == nullptr && controller == nullptr);
    CHECK(std::strstr(error, "unknown motion state field") != nullptr);

    static constexpr char duplicate[] = R"lua(
local scene = vis.scene {}
vis.motion(scene)
vis.motion(scene)
return scene
)lua";
    result = motion::Lua::load(duplicate, static_cast<uint32_t>(std::strlen(duplicate)),
                               "duplicate-motion.lua", &scene, &controller, error, sizeof(error));
    CHECK(result == Result::ScriptError);
    CHECK(scene == nullptr && controller == nullptr);
    CHECK(std::strstr(error, "only one motion controller") != nullptr);

    static constexpr char aborted[] = R"lua(
local scene = vis.scene {}
vis.motion(scene)
error("abort after motion creation")
return scene
)lua";
    detail::LuaHostContext context;
    result = detail::luaHostLoad(aborted, static_cast<uint32_t>(std::strlen(aborted)),
                                 "aborted-motion.lua", &scene, error, sizeof(error), nullptr,
                                 nullptr, nullptr, nullptr, context);
    CHECK(result == Result::ScriptError);
    CHECK(scene == nullptr);
    CHECK(context.motion.controller == nullptr && context.motion.scene == nullptr);

    struct InvalidCall {const char* source; const char* error;};
    static constexpr InvalidCall batches[] = {
        {"motion:transition({})", "motion targets must contain"},
        {"motion:transition({{object=a, state='idle'}, 1})", "table expected"},
        {"motion:transition({{object=a, state='idle', extra=true}})", "extra"},
        {"motion:transition({{object=a, state='idle'}, {object=a, state='idle'}})", "transition failed"},
        {"motion:transition({{object=a, state='idle'}, {object=b, state='missing'}})", "transition failed"},
        {"motion:transition({{object=a, state='idle'}}, -1)", "duration"},
        {"motion:transition({{object=a, state='idle'}}, 1, 'unknown')", "curve"},
    };
    for (const auto& batch : batches) {
        char source[1024];
        std::snprintf(source, sizeof(source), R"lua(
local scene = vis.scene {}
local a = scene:circle {}
local b = scene:circle {}
local motion = vis.motion(scene)
motion:define(a, 'idle', {})
motion:define(b, 'idle', {})
%s
return scene
)lua", batch.source);
        result = motion::Lua::load(source, static_cast<uint32_t>(std::strlen(source)),
                                  "invalid-batch.lua", &scene, &controller, error, sizeof(error));
        CHECK(result == Result::ScriptError);
        CHECK(scene == nullptr && controller == nullptr);
        CHECK(std::strstr(error, batch.error) != nullptr);
        if (!std::strstr(error, batch.error)) std::fprintf(stderr, "batch: %s\n", error);
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
local motion = vis.motion(scene)
motion:define(a, 101, {opacity = 0.5})
motion:define(b, 202, {opacity = 0.25})
motion:define(c, 303, {opacity = 0.75})
motion:define(d, 404, {opacity = 0.625})
motion:transition({{object = a, state = 101}, {object = b, state = 202}}, {duration = 1, curve = "linear"})
motion:transition({{object = c, state = 303}})
motion:transition(d, 404, {})
if motion:event_count() ~= 4 then error("transition event count mismatch") end
local first, second = motion:event(1), motion:event(2)
local third, fourth = motion:event(3), motion:event(4)
if first.transaction ~= second.transaction or second.transaction == third.transaction
    or third.transaction == fourth.transaction then error("batch transaction mismatch") end
if first.duration ~= 1 or first.curve.preset ~= "linear"
    or math.abs(third.duration - 0.2) > 0.0001 or third.curve.preset ~= "smooth"
    or math.abs(fourth.duration - 0.2) > 0.0001 or fourth.curve.preset ~= "smooth" then
    error("transition defaults mismatch")
end
motion:advance(1)
return scene
)lua";
    motion::lua_extension::LuaContext context;
    detail::LuaHooks hooks[] = {motion::lua_extension::luaHooks(context), collectingHooks()};
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
            motion::State state;
            CHECK(context.controller->sample(scene->object(ids[i]), state) == Result::Success);
            CHECK(_near(state.opacity, opacities[i]));
        }
    }
    delete scene;
}

#if defined(VIS_LUA_RUNTIME)
static void _retainedRuntime()
{
    static constexpr char source[] = R"lua(
if not vis.motion then error("motion module missing") end
if not vis.runtime then error("runtime module missing") end
local scene = vis.scene {width = 320, height = 180}
local target = scene:rectangle {id = "target", size = {1, 1}}
local motion = vis.motion(scene)
motion:define(target, "right", {shift = {2, 0}})
motion:define(target, "up", {shift = {1, 2}})
motion:transition(target, "right", 1, "linear")
vis.runtime(scene, {
    fixed_step = 0.25,
    max_steps = 4,
    update = function(ctx, dt, time, tick)
        motion:advance(dt)
        if tick == 2 then
            local current = motion:sample(target)
            if math.abs(current.shift[1] - 1) > 0.0001 then
                error("capture-current mismatch")
            end
            motion:transition(target, "up", 1, "linear")
        end
    end,
})
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = detail::luaHostLoad(source, static_cast<uint32_t>(std::strlen(source)),
                                      "motion-runtime.lua", &scene, error, sizeof(error), nullptr,
                                      nullptr, nullptr, nullptr, context);
    CHECK(result == Result::Success);
    CHECK(scene && context.motion.controller && context.runtime.runtime);
    if (result != Result::Success) std::fprintf(stderr, "retained motion: %s\n", error);
    if (result == Result::Success) {
        CHECK(context.runtime.runtime->advance(0.5f) == Result::Success);
        motion::State state;
        auto object = scene->object("target");
        CHECK(context.motion.controller->sample(object, state) == Result::Success);
        CHECK(_near(state.shift.x, 1.0f));
        CHECK(_near(state.shift.y, 0.0f));
        CHECK(context.runtime.runtime->advance(0.5f) == Result::Success);
        CHECK(context.motion.controller->sample(object, state) == Result::Success);
        CHECK(_near(state.shift.x, 1.0f));
        CHECK(_near(state.shift.y, 1.0f));
        CHECK(context.motion.controller->eventCount() == 2u);
    }
    delete scene;
}
#endif

int main()
{
    _standalone();
    _failures();
    _numericStates();
#if defined(VIS_LUA_RUNTIME)
    _retainedRuntime();
#endif
    if (failures) std::fprintf(stderr, "%d motion Lua test(s) failed\n", failures);
    return failures ? 1 : 0;
}
