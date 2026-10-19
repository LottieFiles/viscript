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

int main()
{
    _sequence();
    if (!failures) std::printf("retained renderer: %d frames match a fresh renderer\n", frames);
    return report();
}
