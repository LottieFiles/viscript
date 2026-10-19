#include <cstring>

#include "vis.h"
#include "vis_lua_runtime.h"
#include "visLuaHost.h"
#include "testCheck.h"
#if defined(VIS_INPUT)
#include "vis_input.h"
#endif

using namespace vis;

static Result load(const char* source, Scene*& scene,
                   detail::LuaHostContext& context, char* error,
                   uint32_t errorSize)
{
    return detail::luaHostLoad(
        source, static_cast<uint32_t>(std::strlen(source)), "runtime.lua",
        &scene, error, errorSize, nullptr, nullptr, nullptr, nullptr, context);
}

static void fixedStep()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {
    width = 320, height = 180,
    camera = {view = "2d", height = 18},
}
local box = scene:rectangle {
    center = {0, 0}, size = {2, 2}, fill = "accent", id = "box",
}
vis.runtime(scene, {
    fixed_step = 0.1,
    max_steps = 3,
    update = function(ctx, dt, time, tick)
        ctx:update(box, {
            shift = {tick, time, 0},
            rotation = time,
            opacity = 0.8,
            progress = 0.9,
            fill = "result",
        })
    end,
})
scene:wait(1)
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success);
    CHECK(scene != nullptr);
    auto runtime = context.runtime.runtime;
    CHECK(runtime != nullptr);
    CHECK(runtime && near(runtime->fixedStep(), 0.1));
    CHECK(runtime && runtime->advance(0.05f) == Result::Success);
    CHECK(runtime && runtime->steps() == 0u && runtime->tick() == 0u);

    auto renderer = Renderer::gen();
    CHECK(renderer != nullptr);
    auto box = scene ? scene->object("box") : nullptr;
    BBox before;
    BBox after;
    CHECK(renderer && box
          && renderer->bounds(scene, box, 0.0f, before) == Result::Success);
    CHECK(runtime && runtime->advance(0.06f) == Result::Success);
    CHECK(runtime && runtime->steps() == 1u && runtime->tick() == 1u);
    CHECK(runtime && near(runtime->time(), 0.1));
    CHECK(runtime && runtime->interpolation() > 0.09f
          && runtime->interpolation() < 0.11f);
    CHECK(renderer && box
          && renderer->bounds(scene, box, 0.0f, after) == Result::Success);
    CHECK(after.x > before.x && after.y < before.y);

    CHECK(runtime && runtime->advance(1.0f) == Result::Success);
    CHECK(runtime && runtime->steps() == 3u && runtime->tick() == 4u);
    CHECK(runtime && runtime->dropped() > 0.7);
    CHECK(runtime && !runtime->failed() && runtime->error()[0] == '\0');

    lua_runtime::ObjectState invalid;
    invalid.opacity = -1.0f;
    CHECK(runtime && runtime->update(box, invalid) == Result::InvalidArguments);
    CHECK(runtime && runtime->clear(box) == Result::Success);
    CHECK(runtime && runtime->clear(box) == Result::InsufficientCondition);
    delete renderer;
    delete scene;
}

static void objectLimit()
{
    CHECK(lua_runtime::Runtime::ObjectLimit == 512u);
    auto scene = Scene::gen();
    auto runtime = lua_runtime::Runtime::gen(scene);
    CHECK(scene && runtime);
    if (!scene || !runtime) {
        delete scene;
        return;
    }

    Object* objects[lua_runtime::Runtime::ObjectLimit + 1u] = {};
    for (auto i = 0u; i <= lua_runtime::Runtime::ObjectLimit; i++) {
        objects[i] = Rectangle::gen({}, {1.0f, 1.0f});
        CHECK(objects[i] && scene->add(objects[i]) == Result::Success);
    }

    lua_runtime::ObjectState state;
    for (auto i = 0u; i < lua_runtime::Runtime::ObjectLimit; i++) {
        CHECK(runtime->update(objects[i], state) == Result::Success);
    }
    CHECK(runtime->update(objects[lua_runtime::Runtime::ObjectLimit], state)
          == Result::InsufficientCondition);
    CHECK(runtime->clear(objects[0]) == Result::Success);
    CHECK(runtime->update(objects[lua_runtime::Runtime::ObjectLimit], state)
          == Result::Success);
    delete scene;
}

static void soundEvents()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 64, height = 36}
vis.runtime(scene, {
    fixed_step = 0.1,
    max_steps = 3,
    update = function(ctx, dt, time, tick)
        if tick == 1 then
            assert(ctx:sound("laser.wav", {gain = 0.65, rate = 1.25}))
        elseif tick == 2 then
            assert(ctx:sound("level.wav", {bus = "ui", gain = 0.8}))
        elseif tick == 3 then
            ctx:sound("rolled-back.wav")
            error("sound rollback")
        end
    end,
})
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    auto runtime = context.runtime.runtime;
    CHECK(runtime && runtime->soundCount() == 0u);
    CHECK(runtime && runtime->advance(0.2f) == Result::Success);
    CHECK(runtime && runtime->soundCount() == 2u);
    lua_runtime::SoundEvent event;
    CHECK(runtime && runtime->soundAt(0u, event));
    CHECK(std::strcmp(event.asset, "laser.wav") == 0);
    CHECK(event.bus == lua_runtime::SoundBus::Effect);
    CHECK(near(event.gain, 0.65) && near(event.rate, 1.25) && !event.loop);
    CHECK(runtime && runtime->soundAt(1u, event));
    CHECK(std::strcmp(event.asset, "level.wav") == 0);
    CHECK(event.bus == lua_runtime::SoundBus::Ui);
    CHECK(runtime && runtime->advance(0.1f) == Result::ScriptError);
    CHECK(runtime && runtime->soundCount() == 0u);
    CHECK(runtime && !runtime->soundAt(0u, event));
    delete scene;

    static constexpr char limitSource[] = R"lua(
local scene = vis.scene {width = 64, height = 36}
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx, dt, time, tick)
        if tick ~= 1 then return end
        for i = 1, 64 do assert(ctx:sound("voice.wav")) end
        assert(not ctx:sound("overflow.wav"))
    end,
})
return scene
)lua";
    scene = nullptr;
    context = {};
    result = load(limitSource, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    runtime = context.runtime.runtime;
    CHECK(runtime && runtime->advance(0.1f) == Result::Success);
    CHECK(runtime && runtime->soundCount() == lua_runtime::Runtime::SoundEventLimit);
    CHECK(runtime && runtime->soundAt(63u, event));
    CHECK(std::strcmp(event.asset, "voice.wav") == 0);
    CHECK(runtime && !runtime->soundAt(64u, event));
    delete scene;
}

// A sound asset name out of range stops the step with the limit, at the line of the call.
static void soundAsset()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 64, height = 36}
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx, dt, time, tick)
        ctx:sound("")
    end,
})
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    auto runtime = context.runtime.runtime;
    CHECK(runtime && runtime->advance(0.1f) == Result::ScriptError);
    CHECK(runtime && std::strstr(runtime->error(),
                                 "runtime.lua:6: sound asset must contain 1 to 255 bytes"));
    delete scene;
}

static void groupFill()
{
    Config config;
    config.width = 100;
    config.height = 100;
    config.camera.orthoHeight = 2.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto group = Group::gen();
    auto child = Rectangle::gen({}, {1.0f, 1.0f});
    auto runtime = lua_runtime::Runtime::gen(scene);
    CHECK(scene && group && child && runtime);
    if (!scene || !group || !child || !runtime) {
        delete scene;
        delete group;
        delete child;
        return;
    }
    child->fill(Color::hex("#4cc9f0"));
    CHECK(group->add(child) == Result::Success);
    CHECK(scene->add(group) == Result::Success);
    lua_runtime::ObjectState state;
    state.fill = Color::hex("#ff6b6b");
    state.fillEnabled = true;
    CHECK(runtime->update(group, state) == Result::Success);
    auto renderer = Renderer::gen();
    const Object* candidates[] = {group};
    PixelSample sample;
    CHECK(renderer && renderer->sample(scene, 0.0f, {50.0f, 50.0f}, candidates, 1u, sample)
          == Result::Success);
    CHECK(sample.object == group && sample.color.r == 0xff && sample.color.g == 0x6b
          && sample.color.b == 0x6b && sample.color.a == 0xff);
    state.fill = Color::hex("#30a46c");
    CHECK(runtime->update(child, state) == Result::Success);
    CHECK(renderer && renderer->sample(scene, 0.0f, {50.0f, 50.0f}, candidates, 1u, sample)
          == Result::Success);
    CHECK(sample.object == group && sample.color.r == 0x30 && sample.color.g == 0xa4
          && sample.color.b == 0x6c && sample.color.a == 0xff);
    delete renderer;
    delete scene;
}

static void callbackFailure()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 320, height = 180, camera = {view = "2d", height = 18}}
local box = scene:rectangle {center = {0, 0}, size = {2, 2}, id = "box"}
vis.runtime(scene, {
    fixed_step = 0.1,
    max_steps = 4,
    update = function(ctx, dt, time, tick)
        ctx:update(box, {shift = {tick == 2 and 50 or tick, 0, 0}})
        if tick == 2 then error("intentional fixed-step failure") end
    end,
})
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    auto runtime = context.runtime.runtime;
    auto renderer = Renderer::gen();
    auto box = scene ? scene->object("box") : nullptr;
    BBox bounds;
    CHECK(runtime && runtime->advance(0.2f) == Result::ScriptError);
    CHECK(runtime && runtime->failed() && runtime->tick() == 1u);
    CHECK(runtime && std::strstr(runtime->error(), "intentional fixed-step failure"));
    CHECK(renderer && box
          && renderer->bounds(scene, box, 0.0f, bounds) == Result::Success);
    CHECK(bounds.x < 200.0f);
    CHECK(runtime && runtime->advance(0.1f) == Result::ScriptError);
    delete renderer;
    delete scene;
}

static void optionalRuntime()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 320, height = 180}
scene:circle {radius = 1}
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    CHECK(context.runtime.runtime == nullptr);
    delete scene;
}

static void publicLoader()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 64, height = 36}
local subject = scene:circle {center = {0, 0}, radius = 1, id = "subject"}
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx, dt, time, tick)
        ctx:update(subject, {shift = {tick, 0, 0}})
    end,
})
return scene
)lua";
    Scene* scene = nullptr;
    lua_runtime::Runtime* runtime = nullptr;
    char error[1024] = {};
    CHECK(lua_runtime::Lua::load(
              source, static_cast<uint32_t>(std::strlen(source)),
              "public-runtime.lua", &scene, nullptr, error, sizeof(error))
          == Result::InvalidArguments);
    auto result = lua_runtime::Lua::load(
        source, static_cast<uint32_t>(std::strlen(source)), "public-runtime.lua",
        &scene, &runtime, error, sizeof(error));
    CHECK(result == Result::Success && scene && runtime);
    CHECK(runtime && runtime->scene() == scene);
    CHECK(runtime && runtime->advance(0.1f) == Result::Success);
    delete scene;
}

static void authoringLock()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 320, height = 180}
local subject = scene:rectangle {center = {0, 0}, size = {2, 2}, id = "subject"}
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx, dt, time, tick)
        ctx:update(subject, {shift = {tick == 2 and 50 or tick, 0, 0}})
        if tick == 2 then scene:circle {center = {0, 0}, radius = 1} end
    end,
})
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    auto runtime = context.runtime.runtime;
    auto renderer = Renderer::gen();
    auto subject = scene ? scene->object("subject") : nullptr;
    BBox committed;
    BBox rolledBack;
    CHECK(runtime && runtime->advance(0.1f) == Result::Success);
    CHECK(renderer && subject
          && renderer->bounds(scene, subject, 0.0f, committed) == Result::Success);
    CHECK(runtime && runtime->advance(0.1f) == Result::ScriptError);
    CHECK(runtime && std::strstr(runtime->error(),
                                 "scene authoring is unavailable after loading"));
    CHECK(renderer && subject
          && renderer->bounds(scene, subject, 0.0f, rolledBack) == Result::Success);
    CHECK(near(committed.x, rolledBack.x) && near(committed.y, rolledBack.y));
    delete renderer;
    delete scene;
}

#if defined(VIS_INPUT)
static input::InputEvent key(input::InputKind kind, input::Key value, float time)
{
    input::InputEvent event;
    event.kind = kind;
    event.key = value;
    event.time = time;
    return event;
}

static void inputActions()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 320, height = 180, camera = {view = "2d", height = 18}}
local box = scene:rectangle {center = {0, 0}, size = {2, 2}, id = "box"}
local input = vis.input.controller(scene)
local x, presses = 0, 0
local runtime = vis.runtime(scene, {
    fixed_step = 0.1,
    max_steps = 4,
    update = function(ctx, dt)
        local move = ctx:action("Move")
        local key = ctx:key("D")
        if move.pressed and key.pressed then presses = presses + 1 end
        x = x + move.value.x * dt
        ctx:update(box, {shift = {x, presses, 0}})
    end,
})
runtime:bind_key("Move", "A", {-1, 0})
runtime:bind_key("Move", "D", {1, 0})
runtime:bind_key("Boost", "Shift")
runtime:bind_key("Switch", "Tab")
runtime:bind_pointer("Place", 0)
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    auto runtime = context.runtime.runtime;
    auto controller = context.input.controller;
    CHECK(runtime && controller);
    CHECK(runtime && runtime->claimsKey(static_cast<uint32_t>(input::Key::D)));
    CHECK(runtime && runtime->claimsKey(static_cast<uint32_t>(input::Key::Shift)));
    CHECK(runtime && runtime->claimsKey(static_cast<uint32_t>(input::Key::Tab)));
    CHECK(runtime && !runtime->claimsKey(static_cast<uint32_t>(input::Key::Space)));
    CHECK(runtime && runtime->claimsPointer(0u) == 1u);
    CHECK(runtime && runtime->claimsPointer(1u) == 0u);
    CHECK(runtime && controller
          && runtime->input(controller->state()) == Result::Success);
    CHECK(controller && controller->state()->begin(0.0f) == Result::Success);
    CHECK(controller
          && controller->input(key(input::InputKind::KeyDown, input::Key::D, 0.01f)).status
                 == Result::Success);
    CHECK(runtime && runtime->advance(0.2f) == Result::Success);
    CHECK(runtime && runtime->steps() == 2u);

    auto renderer = Renderer::gen();
    auto box = scene ? scene->object("box") : nullptr;
    BBox bounds;
    CHECK(renderer && box
          && renderer->bounds(scene, box, 0.0f, bounds) == Result::Success);
    CHECK(bounds.y < 90.0f);
    CHECK(controller && controller->state()->begin(0.2f) == Result::Success);
    CHECK(runtime && runtime->advance(0.1f) == Result::Success);
    CHECK(runtime && runtime->tick() == 3u);
    delete renderer;
    delete scene;
}
#endif

// A two-component scale keeps z at 1, as the states module reads it: tilted about a pivot off
// the object's plane, the object lands where an explicit z of 1 puts it.
static void flatScale()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {width = 320, height = 180, camera = {view = "2d", height = 18}}
local flat = scene:rectangle {center = {0, 0}, size = {2, 2}, id = "flat"}
local full = scene:rectangle {center = {0, 0}, size = {2, 2}, id = "full"}
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx)
        ctx:update(flat, {origin = {0, 0, 1}, rotation = {0.5, 0, 0}, scale = {2, 2}})
        ctx:update(full, {origin = {0, 0, 1}, rotation = {0.5, 0, 0}, scale = {2, 2, 1}})
    end,
})
return scene
)lua";
    Scene* scene = nullptr;
    char error[1024] = {};
    detail::LuaHostContext context;
    auto result = load(source, scene, context, error, sizeof(error));
    CHECK(result == Result::Success && scene);
    auto runtime = context.runtime.runtime;
    auto renderer = Renderer::gen();
    auto flat = scene ? scene->object("flat") : nullptr;
    auto full = scene ? scene->object("full") : nullptr;
    BBox flatBounds;
    BBox fullBounds;
    CHECK(runtime && runtime->advance(0.1f) == Result::Success);
    CHECK(renderer && flat
          && renderer->bounds(scene, flat, 0.0f, flatBounds) == Result::Success);
    CHECK(renderer && full
          && renderer->bounds(scene, full, 0.0f, fullBounds) == Result::Success);
    CHECK(near(flatBounds.x, fullBounds.x) && near(flatBounds.y, fullBounds.y)
          && near(flatBounds.width, fullBounds.width)
          && near(flatBounds.height, fullBounds.height));
    delete renderer;
    delete scene;
}

int main()
{
    fixedStep();
    objectLimit();
    soundEvents();
    soundAsset();
    groupFill();
    callbackFailure();
    optionalRuntime();
    publicLoader();
    authoringLock();
#if defined(VIS_INPUT)
    inputActions();
#endif
    flatScale();
    return report();
}
