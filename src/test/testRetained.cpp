// Retained renderer determinism: one renderer reused across a whole render sequence
// (forward playback, backward and random seeks, scene switches, scene reloads at a
// recycled address, resize, pixel ratio and background changes) must produce exactly the
// pixels of a fresh renderer for every frame.
#include <cstdio>
#include <cstring>

#include "vis.h"
#include "testLuaLoad.h"

using namespace vis;

static auto frames = 0;

static const char* Scenes[] = {
    "animation_gallery", "scene_transition", "gradient_kinds",
    "image_cells", "text_reveal_transitions", "object_gallery",
};
static constexpr uint32_t SceneCount = sizeof(Scenes) / sizeof(Scenes[0]);

static Scene* _example(const char* name)
{
    char path[1024];
    std::snprintf(path, sizeof(path), "%s/%s.lua", VIS_TEST_EXAMPLES, name);
    return _loadedFile(path);
}

static Renderer* _renderer()
{
    auto renderer = Renderer::gen();
    CHECK(renderer);
    if (renderer) CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
    CHECK(renderer && renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
    return renderer;
}

// Renders one frame with the reused renderer and with a fresh one, and compares them.
static void _frame(Renderer* reused, Surface& surface, const Scene* scene, float time,
                   const char* label, uint32_t pixelRatio = 1u)
{
    auto fresh = _renderer();
    if (!reused || !fresh || !scene) {
        delete fresh;
        return;
    }
    Surface expected;
    auto first = reused->render(scene, time, surface, pixelRatio);
    auto second = fresh->render(scene, time, expected, pixelRatio);
    CHECK(first == Result::Success);
    CHECK(second == Result::Success);
    auto same = first == Result::Success && second == Result::Success && equal(surface, expected);
    if (!same) std::fprintf(stderr, "retained frame differs: %s t=%g ratio=%u\n", label, time, pixelRatio);
    CHECK(same);
    frames++;
    delete fresh;
}

static void _sequence()
{
    auto reused = _renderer();
    Surface surface;
    Scene* scenes[SceneCount] = {};
    for (auto i = 0u; i < SceneCount; i++)
        scenes[i] = _example(Scenes[i]);

    // Forward playback, then seeks backwards and at random, for every scene in turn
    // (each switch also changes the Scene the retained tree belongs to).
    for (auto i = 0u; i < SceneCount; i++) {
        auto scene = scenes[i];
        if (!scene) continue;
        auto duration = scene->duration();
        for (auto k = 0u; k <= 12u; k++)
            _frame(reused, surface, scene, duration * static_cast<float>(k) / 12.0f, Scenes[i]);
        for (auto k = 12u; k > 0u; k -= 3u)
            _frame(reused, surface, scene, duration * static_cast<float>(k - 1u) / 12.0f, Scenes[i]);
        uint32_t seed = 7u + i;
        for (auto k = 0u; k < 8u; k++) {
            seed = seed * 1103515245u + 12345u;
            _frame(reused, surface, scene, duration * static_cast<float>((seed >> 8) % 1000u) / 999.0f, Scenes[i]);
        }
    }

    // Interleave scenes frame by frame.
    for (auto k = 0u; k < 6u; k++) {
        for (auto i = 0u; i < SceneCount; i++) {
            if (scenes[i]) _frame(reused, surface, scenes[i], scenes[i]->duration() * (0.15f * k), Scenes[i]);
        }
    }

    // Pixel ratio changes keep the tree but rescale its root.
    if (scenes[0]) {
        _frame(reused, surface, scenes[0], 0.5f, "ratio", 2u);
        _frame(reused, surface, scenes[0], 0.6f, "ratio", 2u);
        _frame(reused, surface, scenes[0], 0.7f, "ratio", 1u);
    }

    // Resize: a new target size and background rectangle on the same scene.
    if (scenes[2]) {
        auto& config = scenes[2]->config();
        auto width = config.width;
        auto height = config.height;
        _frame(reused, surface, scenes[2], 1.0f, "resize");
        config.width = width / 2u;
        config.height = height / 2u;
        _frame(reused, surface, scenes[2], 1.1f, "resize");
        _frame(reused, surface, scenes[2], 0.4f, "resize");
        config.width = width;
        config.height = height;
        _frame(reused, surface, scenes[2], 1.2f, "resize");
    }

    // Background change between frames of the same scene.
    if (scenes[3]) {
        auto& config = scenes[3]->config();
        auto background = config.background;
        _frame(reused, surface, scenes[3], 1.0f, "background");
        config.background = Color::hex("#204060");
        _frame(reused, surface, scenes[3], 1.0f, "background");
        config.background = background;
        _frame(reused, surface, scenes[3], 1.5f, "background");
    }

    // Reload: delete the scene and load another one, likely at the same address.
    for (auto i = 0u; i < SceneCount; i++) {
        if (!scenes[i]) continue;
        _frame(reused, surface, scenes[i], scenes[i]->duration() * 0.5f, Scenes[i]);
        delete scenes[i];
        scenes[i] = _example(Scenes[(i + 1u) % SceneCount]);
        if (scenes[i]) _frame(reused, surface, scenes[i], scenes[i]->duration() * 0.5f, "reload");
    }

    for (auto i = 0u; i < SceneCount; i++)
        delete scenes[i];
    delete reused;
}

// Texts set in several fonts, as runs that the tree recolors, fades and moves in place and sets
// again when their strings change, and texts that change between one font and several, one of
// them in a font list.
static void _fallback()
{
    static const char source[] = R"(
local s = vis.scene {width = 320, height = 200, camera = {view = "2d", target = {0, 0}, height = 200}}
local angle = s:text {text = "\u{2220}APB = 35\u{b0}", point = {-60, 60}, size = 18, font = "Pretendard"}
local mono = s:text {text = "a\u{2090} \u{2220}x", point = {60, 60}, size = 18, font = "JetBrainsMono", slant = 0.2}
local figures = s:text {text = "12 \u{2220} 3", point = {0, 0}, size = 18, font = "Pretendard", tabular = true}
local lines = s:text {text = "\u{2220}AB\n\u{22a5}CD", point = {0, -60}, size = 16, font = "Pretendard"}
local listed = s:text {text = "mono", point = {100, -60}, size = 16, font = "Missing, JetBrainsMono, Pretendard"}
s:tween({target = angle, fill = "#ffd166", shift = {20, 0}}, 1, "smooth")
s:tween({target = mono, text = "b\u{2091} \u{2245}y"}, 1)
s:fade(figures, 0.3, 1)
s:tween({target = lines, text = "plain"}, 1)
s:tween({target = lines, text = "\u{2220}AB again"}, 1)
s:tween({target = listed, text = "mono \u{d55c}", fill = "#06d6a0"}, 1)
return s
)";
    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, sizeof(source) - 1u, "fallback.lua", &scene, error, sizeof(error));
    if (status != Result::Success) std::fprintf(stderr, "load fallback.lua: %s\n", error);
    CHECK(status == Result::Success && scene);
    auto reused = _renderer();
    Surface surface;
    if (scene) {
        auto duration = scene->duration();
        for (auto k = 0u; k <= 24u; k++)
            _frame(reused, surface, scene, duration * static_cast<float>(k) / 24.0f, "fallback");
        for (auto k = 24u; k > 0u; k -= 4u)
            _frame(reused, surface, scene, duration * static_cast<float>(k - 1u) / 24.0f, "fallback");
    }
    delete scene;
    delete reused;
}

int main()
{
    _sequence();
    _fallback();
    if (!failures) std::printf("retained renderer: %d frames match a fresh renderer\n", frames);
    return report();
}
