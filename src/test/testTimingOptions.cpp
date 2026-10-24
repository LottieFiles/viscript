#include "testSceneChecks.h"

static constexpr auto prefix =
    "local s=vis.scene{width=200,height=100,camera={height=4}}; "
    "local a=s:rectangle{id='a',center={-1,0},size={1,1},fill='#ffffff'}; "
    "local b=s:rectangle{id='b',center={1,0},size={1,1},fill='#ffffff'}; "
    "local timing={duration=.4,curve='linear'}; "
    "local stagger={duration=.4,curve='linear',lag=.25}; ";

static Scene* _loaded(const char* source, const char* name)
{
    Scene* scene = nullptr;
    char error[1024] = {};
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name,
                            &scene, error, sizeof(error));
    CHECK(status == Result::Success && scene);
    if (status != Result::Success) std::fprintf(stderr, "%s: %s\n", name, error);
    return scene;
}

static void _reject(const char* source, const char* name, const char* expected = nullptr)
{
    Scene* scene = nullptr;
    char error[1024] = {};
    CHECK(Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name,
                    &scene, error, sizeof(error)) == Result::ScriptError);
    CHECK(!scene && std::strstr(error, name));
    if (expected) CHECK(std::strstr(error, expected));
    delete scene;
}

static void acceptedOptions(Renderer* renderer)
{
    static const struct {
        const char* call;
        float duration;
    } cases[] = {
        {"s:create({a,b},{duration=.4,curve='linear',lag=.25,direction='reverse'})", .5f},
        {"s:uncreate({a,b},stagger)", .5f},
        {"s:fill_reveal({a,b},stagger)", .5f},
        {"s:draw_border_then_fill({a,b},stagger)", .5f},
        {"s:write({a,b},stagger)", .5f},
        {"s:grow_from_center(a,timing)", .4f},
        {"s:grow_from_edge(a,'left',timing)", .4f},
        {"s:shrink_to_center(a,timing)", .4f},
        {"s:shift(a,{0,1},timing)", .4f},
        {"s:transform(a,{1,0,0,1,0,1,0,0,0,0,1,0,0,0,0,1},timing)", .4f},
        {"s:fade(a,.3,timing)", .4f},
        {"s:stroke(a,'accent',timing)", .4f},
        {"s:fill(a,'accent',timing)", .4f},
        {"s:set{target=b,opacity=0}; s:transition(a,b,timing)", .4f},
        {"s:morph(a,b,{duration=.4,curve='linear',lag=.25,copy=true})", .4f},
        {"s:fade_transform(a,b,timing)", .4f},
        {"s:match_transform(a,b,{duration=.4,curve='linear',lag=.25,copy=true})", .4f},
        {"s:arrange({a,b},{duration=.4,curve='linear',gap=.5})", .4f},
        {"s:warp(a,function(x,y,z) return x+.2,y,z end,{duration=.4,curve='linear',step=.1})", .4f},
        {"s:tween({{target=a,shift={0,1}},{target=b,opacity=.3}},stagger)", .5f},
        {"s:save({a,b}); s:shift(a,{0,1}); s:restore({a,b},stagger)", 1.5f},
        {"s:look({target={1,0},height=4},timing)", .4f},
        {"local t=s:tracker(0); s:bind(a,t,function(x) return {shift={x,0}} end); s:track(t,1,timing)", .4f},
        {"local t=s:tracker(0); s:bind(a,t,function(x) return {shift={x,0}} end); s:track({{t,1}},timing)", .4f},
        {"local t=s:tracker(0); s:track(t,1,{})", 1.0f},
        {"local v=s:view{x=.5,y=0,width=.5,height=.5}; v:look({target={1,0}},timing); v:fade(.3,timing)", .8f},
    };
    for (auto& entry : cases) {
        char script[2048];
        std::snprintf(script, sizeof(script), "%s%s; "
            "assert(timing.duration==.4 and timing.curve=='linear' and timing.lag==nil); "
            "assert(stagger.duration==.4 and stagger.curve=='linear' and stagger.lag==.25); return s",
            prefix, entry.call);
        auto scene = _loaded(script, "timing.lua");
        if (scene) {
            CHECK(near(scene->duration(), entry.duration));
            Surface frame;
            CHECK(renderer->render(scene, entry.duration * .5f, frame) == Result::Success);
        }
        delete scene;
    }

    // One caller-owned table can schedule several methods without acquiring internal fields.
    char script[1024];
    std::snprintf(script, sizeof(script), "%ss:shift(a,{0,1},timing); s:fade(a,.3,timing); "
        "assert(timing.duration==.4 and timing.curve=='linear'); "
        "assert(#vis.keys(timing)==2); return s", prefix);
    auto scene = _loaded(script, "reused-timing.lua");
    CHECK(scene && near(scene->duration(), .8f));
    delete scene;
}

static void defaults(Renderer* renderer)
{
    static const char* const cases[][2] = {
        {"s:shift(a,{0,1})", "s:shift(a,{0,1},{})"},
        {"s:create({a,b})", "s:create({a,b},{})"},
        {"s:look({target={1,0}})", "s:look({target={1,0}},{})"},
        {"s:save(a); s:shift(a,{0,1}); s:restore(a)",
         "s:save(a); s:shift(a,{0,1}); s:restore(a,{})"},
    };
    for (auto& calls : cases) {
        Scene* scenes[2] = {};
        for (auto i = 0; i < 2; i++) {
            char script[1024];
            std::snprintf(script, sizeof(script), "%s%s; return s", prefix, calls[i]);
            scenes[i] = _loaded(script, "default-timing.lua");
        }
        if (scenes[0] && scenes[1]) {
            CHECK(near(scenes[0]->duration(), 1.0f));
            CHECK(near(scenes[1]->duration(), 1.0f));
            CHECK(scenes[0]->clipCount() == scenes[1]->clipCount());
            Surface a, b;
            for (auto time : {0.0f, .25f, .5f, .75f, 1.0f}) {
                CHECK(renderer->render(scenes[0], time, a) == Result::Success);
                CHECK(renderer->render(scenes[1], time, b) == Result::Success);
                if (a.data() && b.data()) {
                    CHECK(std::memcmp(a.data(), b.data(), sizeof(uint32_t) * a.stride() * a.height()) == 0);
                }
            }
        }
        delete scenes[0];
        delete scenes[1];
    }
}

static void sampledTiming(Renderer* renderer)
{
    // Lag is a share of duration: the second target starts at 1 s and ends at 3 s.
    char script[1024];
    std::snprintf(script, sizeof(script), "%ss:tween({{target=a,shift={2,0}},"
        "{target=b,shift={2,0}}},{duration=2,curve='linear',lag=.5}); return s", prefix);
    auto scene = _loaded(script, "staggered-timing.lua");
    CHECK(scene && near(scene->duration(), 3.0f));
    if (scene) {
        BBox a0, b0;
        CHECK(renderer->bounds(scene, scene->object("a"), 0.0f, a0) == Result::Success);
        CHECK(renderer->bounds(scene, scene->object("b"), 0.0f, b0) == Result::Success);
        for (auto time : {1.0f, 2.0f, 3.0f}) {
            BBox a, b;
            CHECK(renderer->bounds(scene, scene->object("a"), time, a) == Result::Success);
            CHECK(renderer->bounds(scene, scene->object("b"), time, b) == Result::Success);
            CHECK(near(a.center().x - a0.center().x, time < 2.0f ? 25.0f : 50.0f, .1f));
            CHECK(near(b.center().x - b0.center().x, (time - 1.0f) * 25.0f, .1f));
        }
    }
    delete scene;
}

static void rejectedOptions()
{
    static const char* const invalid[] = {
        "s:shift(a,{0,1},{duration=0})", "s:shift(a,{0,1},{duration=-1})",
        "s:shift(a,{0,1},{duration=math.huge})", "s:shift(a,{0,1},{lag=.25})",
        "s:shift(a,{0,1},{easing='linear'})", "s:shift(a,{0,1},{curve='unknown'})",
        "s:create(a,{lag=-.1})", "s:fill_reveal(a,{direction='reverse'})",
        "s:track({{s:tracker(0),1}},{lag=.2})",
        "s:match_transform(a,b,{duration=.4,color='accent'})",
    };
    for (auto call : invalid) {
        char script[2048];
        std::snprintf(script, sizeof(script), "%s%s; return s", prefix, call);
        _reject(script, "invalid-timing.lua");
    }
    static const char* const positional[] = {
        "s:create(a,.4)", "s:create(a,.4,'linear',.25,'reverse')",
        "s:shift(a,{0,1},.4)", "s:shift(a,{0,1},timing,'linear')",
        "s:tween({target=a,shift={0,1}},.4,'linear',.25)",
        "s:save(a); s:restore(a,.4)", "s:look({target={1,0}},.4,'linear')",
        "s:morph(a,b,.4,'linear',{copy=true})",
        "s:warp(a,function(x,y,z) return x,y,z end,.4,'linear',{step=.1})",
        "s:track(s:tracker(0),1,.4)",
        "s:track(s:tracker(0),1)",
    };
    for (auto call : positional) {
        char script[2048];
        std::snprintf(script, sizeof(script), "%s%s; return s", prefix, call);
        _reject(script, "positional-timing.lua", "options");
    }
}

int main()
{
    auto renderer = Renderer::gen();
    CHECK(renderer);
    if (renderer) {
        acceptedOptions(renderer);
        defaults(renderer);
        sampledTiming(renderer);
    }
    rejectedOptions();
    delete renderer;
    return report();
}
