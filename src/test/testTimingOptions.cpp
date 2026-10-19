#include "testSceneChecks.h"

static void _sameFrames(Renderer* renderer, Scene* first, Scene* second)
{
    CHECK(near(first->duration(), second->duration()));
    CHECK(first->clipCount() == second->clipCount());
    Surface a, b;
    for (auto share : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        auto time = first->duration() * share;
        CHECK(renderer->render(first, time, a) == Result::Success);
        CHECK(renderer->render(second, time, b) == Result::Success);
        if (!a.data() || !b.data()) continue;
        CHECK(a.width() == b.width() && a.height() == b.height() && a.stride() == b.stride());
        CHECK(std::memcmp(a.data(), b.data(), sizeof(uint32_t) * a.stride() * a.height()) == 0);
    }
}

static void _lua(Renderer* renderer)
{
    static constexpr auto prefix =
        "local s=vis.scene{width=96,height=64}; "
        "local a=s:rectangle{id='a',center={-1,0},size={1,1}}; "
        "local b=s:rectangle{id='b',center={1,0},size={1,1}}; "
        "local timing={duration=.4,curve='linear'}; "
        "local stagger={duration=.4,curve='linear',lag=.25}; ";
    static const char* const cases[][2] = {
        {"s:create({a,b},.4,'linear',.25,'reverse')", "s:create({a,b},{duration=.4,curve='linear',lag=.25,direction='reverse'})"},
        {"s:uncreate({a,b},.4,'linear',.25)", "s:uncreate({a,b},stagger)"},
        {"s:fill_reveal({a,b},.4,'linear',.25)", "s:fill_reveal({a,b},stagger)"},
        {"s:draw_border_then_fill({a,b},.4,'linear',.25)", "s:draw_border_then_fill({a,b},stagger)"},
        {"s:write({a,b},.4,'linear',.25)", "s:write({a,b},stagger)"},
        {"s:grow_from_center(a,.4,'linear')", "s:grow_from_center(a,timing)"},
        {"s:grow_from_edge(a,'left',.4,'linear')", "s:grow_from_edge(a,'left',timing)"},
        {"s:shrink_to_center(a,.4,'linear')", "s:shrink_to_center(a,timing)"},
        {"s:shift(a,{0,1},.4,'linear')", "s:shift(a,{0,1},timing)"},
        {"s:transform(a,{1,0,0,1,0,1,0,0,0,0,1,0,0,0,0,1},.4,'linear')", "s:transform(a,{1,0,0,1,0,1,0,0,0,0,1,0,0,0,0,1},timing)"},
        {"s:fade(a,.3,.4,'linear')", "s:fade(a,.3,timing)"},
        {"s:stroke(a,'accent',.4,'linear')", "s:stroke(a,'accent',timing)"},
        {"s:fill(a,'accent',.4,'linear')", "s:fill(a,'accent',timing)"},
        {"s:set{target=b,opacity=0}; s:transition(a,b,.4,'linear')", "s:set{target=b,opacity=0}; s:transition(a,b,timing)"},
        {"s:morph(a,b,.4,'linear',{lag=.25,copy=true})", "s:morph(a,b,{duration=.4,curve='linear',lag=.25,copy=true})"},
        {"s:fade_transform(a,b,.4,'linear')", "s:fade_transform(a,b,timing)"},
        {"s:match_transform(a,b,.4,'linear',{lag=.25,copy=true})", "s:match_transform(a,b,{duration=.4,curve='linear',lag=.25,copy=true})"},
        {"s:arrange({a,b},.4,'linear',{gap=.5})", "s:arrange({a,b},{duration=.4,curve='linear',gap=.5})"},
        {"s:warp(a,function(x,y,z) return x+.2,y,z end,.4,'linear',{step=.1})", "s:warp(a,function(x,y,z) return x+.2,y,z end,{duration=.4,curve='linear',step=.1})"},
        {"s:play({{target=a,shift={0,1}},{target=b,opacity=.3}},.4,'linear',.25)", "s:play({{target=a,shift={0,1}},{target=b,opacity=.3}},stagger)"},
        {"s:save({a,b}); s:shift(a,{0,1}); s:restore({a,b},.4,'linear',.25)", "s:save({a,b}); s:shift(a,{0,1}); s:restore({a,b},stagger)"},
        {"s:save(a); s:shift(a,{0,1}); s:restore(a)", "s:save(a); s:shift(a,{0,1}); s:restore(a,{})"},
        {"s:look({target={1,0},height=4},.4,'linear')", "s:look({target={1,0},height=4},timing)"},
        {"s:focus(a,{duration=.4,curve='linear'})", "s:focus(a,{duration=.4,easing='linear'})"},
        {"s:shift(a,{0,1})", "s:shift(a,{0,1},{})"},
        {"local t=s:tracker(0); s:bind(a,t,function(x) return {shift={x,0}} end); s:track(t,1,.4,'linear')",
         "local t=s:tracker(0); s:bind(a,t,function(x) return {shift={x,0}} end); s:track(t,1,timing)"},
        {"local t=s:tracker(0); s:bind(a,t,function(x) return {shift={x,0}} end); s:track({{t,1}},.4,'linear')",
         "local t=s:tracker(0); s:bind(a,t,function(x) return {shift={x,0}} end); s:track({{t,1}},timing)"},
    };
    char script[2048], error[256];
    for (auto& calls : cases) {
        Scene* scenes[2] = {};
        for (auto i = 0; i < 2; i++) {
            std::snprintf(script, sizeof(script), "%s%s; assert(timing.duration==.4 and timing.curve=='linear'); "
                "assert(stagger.duration==.4 and stagger.lag==.25); return s", prefix, calls[i]);
            auto result = Lua::load(script, static_cast<uint32_t>(std::strlen(script)), "timing.lua", scenes + i, error, sizeof(error));
            if (result != Result::Success) std::fprintf(stderr, "%s: %s\n", calls[i], error);
            CHECK(result == Result::Success);
        }
        if (scenes[0] && scenes[1]) {
            auto before = failures;
            _sameFrames(renderer, scenes[0], scenes[1]);
            if (failures != before) std::fprintf(stderr, "Frame mismatch: %s / %s\n", calls[0], calls[1]);
        }
        delete scenes[0];
        delete scenes[1];
    }
    static const char* const invalid[] = {
        "s:shift(a,{0,1},{duration=0})", "s:shift(a,{0,1},{duration=-1})",
        "s:shift(a,{0,1},{duration=math.huge})", "s:shift(a,{0,1},{lag=.25})",
        "s:shift(a,{0,1},{curve='smooth',easing='linear'})", "s:shift(a,{0,1},{curve='unknown'})",
        "s:shift(a,{0,1},timing,'linear')", "s:create(a,{lag=-.1})",
        "s:fill_reveal(a,{direction='reverse'})", "s:track({{s:tracker(0),1}},{lag=.2})",
    };
    for (auto call : invalid) {
        std::snprintf(script, sizeof(script), "%s%s; return s", prefix, call);
        Scene* scene = nullptr;
        CHECK(Lua::load(script, static_cast<uint32_t>(std::strlen(script)), "invalid-timing.lua", &scene, error, sizeof(error))
              == Result::ScriptError);
        CHECK(!scene && error[0]);
        delete scene;
    }
}

int main()
{
    auto renderer = Renderer::gen();
    CHECK(renderer);
    if (renderer) {
        _lua(renderer);
    }
    delete renderer;
    return report();
}
