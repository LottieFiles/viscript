#include <cstdio>
#include <cstring>
#include <type_traits>

#include "vis.h"
#include "testCheck.h"

using namespace vis;

static_assert(!std::is_copy_constructible<Renderer>::value, "Renderer owns a canvas");
static_assert(!std::is_copy_assignable<Renderer>::value, "Renderer owns a canvas");

static bool _says(const Renderer* renderer, const char* text)
{
    auto reason = renderer->diagnostic();
    if (reason && std::strstr(reason, text)) return true;
    std::fprintf(stderr, "renderer diagnostic \"%s\" lacks \"%s\"\n", reason ? reason : "(none)", text);
    return false;
}

// No font but the engine's default one is loaded in this process, so text in a font that is
// not loaded is measured and drawn in the default font, and measuring it says so.
static void _fontDiagnostics()
{
    auto renderer = Renderer::gen();
    auto scene = Scene::gen();
    auto named = Text::gen("named", {-1.0f, 0.0f, 0.0f});
    CHECK(renderer && scene && named);
    if (!renderer || !scene || !named) {
        delete renderer;
        delete scene;
        delete named;
        return;
    }
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(renderer->diagnostic() == nullptr);

    CHECK(named->font("Missing Family") == Result::Success);
    CHECK(scene->add(named) == Result::Success);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(renderer->diagnostic() == nullptr);
    BBox bounds;
    CHECK(renderer->bounds(scene, named, 0.0f, bounds) == Result::Success);
    CHECK(bounds.width > 0.0f && bounds.height > 0.0f);
    LayoutReport report;
    CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
    TextExtent extent;
    CHECK(scene->measure(named, extent) == Result::Success);
    CHECK(extent.fallback && extent.width > 0.0f && extent.missing == 0u);
    // The default font is Latin, Greek and signs; Hangul is left out, as a font without it leaves it.
    CHECK(scene->measure(named, "θ ≤ π 한", extent) == Result::Success);
    CHECK(extent.fallback && extent.missing == 1u && std::strcmp(extent.firstMissing, "한") == 0);
    const char* fonts[2] = {};
    CHECK(scene->missingFonts(fonts, 2u) == 1u && std::strcmp(fonts[0], "Missing Family") == 0);
    CHECK(scene->missingFonts(fonts, 0u) == 0u && scene->missingFonts(nullptr, 2u) == 0u);
    CHECK(renderer->render(scene, -1.0f, surface) == Result::InvalidArguments);
    CHECK(renderer->diagnostic() == nullptr);

    CHECK(renderer->font("/nonexistent/vis-missing-font.ttf") != Result::Success);
    CHECK(_says(renderer, "could not load the font file '/nonexistent/vis-missing-font.ttf'"));
    static const uint8_t junk[] = {1, 2, 3, 4};
    CHECK(renderer->font("Junk", junk, sizeof(junk)) != Result::Success);
    CHECK(_says(renderer, "could not load the font 'Junk'"));
    delete scene;
    delete renderer;
}

// Saving refuses what it cannot write before it renders: no scene, an unknown extension, or a
// soundtrack for anything but an MP4.
static void _savingArguments()
{
    auto renderer = Renderer::gen();
    auto scene = Scene::gen();
    CHECK(renderer && scene);
    if (renderer && scene) {
        CHECK(Saver::save(nullptr, *renderer, "frame.mp4") == Result::InvalidArguments);
        CHECK(Saver::save(scene, *renderer, "frame.webm", 30u) == Result::InvalidArguments);
        CHECK(Saver::save(scene, *renderer, "", 30u) == Result::InvalidArguments);
        CHECK(Saver::save(scene, *renderer, "frame.gif", 30u, "track.wav") == Result::InvalidArguments);
        CHECK(Saver::save(scene, *renderer, "frame.mp4", 30u, "") == Result::InvalidArguments);
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
        CHECK(Saver::save(scene, *renderer, "frame.mp4", 30u) == Result::NonSupport);
        CHECK(Saver::save(scene, *renderer, "frame.gif", 30u) == Result::NonSupport);
#endif
    }
    delete scene;
    delete renderer;
}

int main()
{
    auto renderer = Renderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        CHECK(renderer->diagnostic() == nullptr);
        Surface surface;
        CHECK(renderer->render(nullptr, 0.0f, surface) == Result::InvalidArguments);
        BBox bounds;
        CHECK(renderer->bounds(nullptr, nullptr, 0.0f, bounds) == Result::InvalidArguments);
    }
    delete renderer;
    _savingArguments();
    _fontDiagnostics();

    return report();
}
