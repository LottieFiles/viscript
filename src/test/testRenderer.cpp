#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "vis.h"

using namespace vis;

static auto failures = 0;

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                                        \
        }                                                                                      \
    } while (false)

static_assert(!std::is_copy_constructible<Renderer>::value, "Renderer owns one backend");
static_assert(!std::is_copy_assignable<Renderer>::value, "Renderer owns one backend");

static bool _says(const Renderer* renderer, const char* text)
{
    auto reason = renderer->diagnostic();
    if (reason && std::strstr(reason, text)) return true;
    std::fprintf(stderr, "renderer diagnostic \"%s\" lacks \"%s\"\n", reason ? reason : "(none)", text);
    return false;
}

#if defined(VIS_EXPECT_CPU)
// No font is loaded in this process, so every text paint fails and names what it needs.
static void _fontDiagnostics()
{
    auto renderer = Renderer::gen(RenderEngine::Cpu);
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
    CHECK(renderer->render(scene, 0.0f, surface) == Result::NonSupport);
    CHECK(_says(renderer, "text needs the font 'Missing Family', which is not loaded"));
    CHECK(_says(renderer, "Missing Family.ttf"));
    BBox bounds;
    CHECK(renderer->bounds(scene, named, 0.0f, bounds) == Result::NonSupport);
    CHECK(_says(renderer, "'Missing Family'"));
    LayoutReport report;
    CHECK(renderer->layout(scene, 0.0f, report) == Result::NonSupport);
    CHECK(_says(renderer, "'Missing Family'"));
    CHECK(renderer->render(scene, -1.0f, surface) == Result::InvalidArguments);
    CHECK(renderer->diagnostic() == nullptr);

    CHECK(renderer->font("/nonexistent/vis-missing-font.ttf") != Result::Success);
    CHECK(_says(renderer, "could not load the font file '/nonexistent/vis-missing-font.ttf'"));
    static const uint8_t junk[] = {1, 2, 3, 4};
    CHECK(renderer->font("Junk", junk, sizeof(junk)) != Result::Success);
    CHECK(_says(renderer, "could not load the font 'Junk'"));

    GlTarget target;
    CHECK(renderer->render(scene, 0.0f, target) == Result::NonSupport);
    CHECK(_says(renderer, "needs the GL engine"));
    delete scene;
    delete renderer;
}
#endif

int main()
{
    auto legacyVideo = &Saver::video;
    CHECK(legacyVideo(nullptr, nullptr, "frame.mp4", 0u) == Result::InvalidArguments);
    auto invalid = static_cast<RenderEngine>(0xffu);
    CHECK(!Renderer::enabled(invalid));
    CHECK(Renderer::gen(invalid) == nullptr);
    CHECK(Renderer::defaultEngine(invalid) == Result::InvalidArguments);

#if defined(VIS_EXPECT_CPU)
    CHECK(Renderer::enabled(RenderEngine::Cpu));
#else
    CHECK(!Renderer::enabled(RenderEngine::Cpu));
    CHECK(Renderer::gen(RenderEngine::Cpu) == nullptr);
#endif

#if defined(VIS_EXPECT_GL)
    CHECK(Renderer::enabled(RenderEngine::Gl));
#else
    CHECK(!Renderer::enabled(RenderEngine::Gl));
    CHECK(Renderer::gen(RenderEngine::Gl) == nullptr);
#endif

#if defined(VIS_EXPECT_CPU)
    CHECK(Renderer::defaultEngine() == RenderEngine::Cpu);
    CHECK(Renderer::defaultEngine(RenderEngine::Cpu) == Result::Success);
    auto cpuRenderer = Renderer::gen(RenderEngine::Cpu);
    CHECK(cpuRenderer != nullptr);
    if (cpuRenderer) {
        CHECK(cpuRenderer->engine() == RenderEngine::Cpu);
        GlTarget target;
        CHECK(cpuRenderer->render(nullptr, 0.0f, target) == Result::NonSupport);
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
        auto scene = Scene::gen();
        auto swRenderer = SwRenderer::gen();
        CHECK(scene != nullptr && swRenderer != nullptr);
        if (scene && swRenderer) {
            CHECK(Saver::video(scene, swRenderer, "frame.webm", 30u)
                  == Result::InvalidArguments);
            CHECK(Saver::video(scene, swRenderer, "frame.mp4", 30u)
                  == Result::NonSupport);
        }
        delete scene;
        delete swRenderer;
#endif
    }
    auto renderer = Renderer::gen();
    CHECK(renderer != nullptr);
    CHECK(!renderer || renderer->engine() == RenderEngine::Cpu);
    delete renderer;
#elif defined(VIS_EXPECT_GL)
    CHECK(Renderer::defaultEngine() == RenderEngine::Gl);
#endif

#if defined(VIS_EXPECT_GL)
    CHECK(Renderer::defaultEngine(RenderEngine::Gl) == Result::Success);
    CHECK(Renderer::defaultEngine() == RenderEngine::Gl);
    auto glRenderer = Renderer::gen();
    CHECK(glRenderer != nullptr);
    if (glRenderer) {
        CHECK(glRenderer->engine() == RenderEngine::Gl);
        Surface surface;
        CHECK(glRenderer->render(nullptr, 0.0f, surface) == Result::NonSupport);
        auto scene = Scene::gen();
        GlTarget target;
        target.width = scene ? scene->config().width : 1u;
        target.height = scene ? scene->config().height : 1u;
        CHECK(scene != nullptr);
        CHECK(glRenderer->render(scene, 0.0f, target) == Result::InvalidArguments);
        delete scene;
        BBox bounds;
        CHECK(glRenderer->bounds(nullptr, nullptr, 0.0f, bounds) == Result::NonSupport);
        CHECK(Saver::save(nullptr, *glRenderer, "frame.mp4") == Result::NonSupport);
    }
    delete glRenderer;
#if defined(VIS_EXPECT_CPU)
    CHECK(cpuRenderer && cpuRenderer->engine() == RenderEngine::Cpu);
#endif
#else
    CHECK(Renderer::defaultEngine(RenderEngine::Gl) == Result::NonSupport);
#endif

#if !defined(VIS_EXPECT_CPU)
    CHECK(Renderer::defaultEngine(RenderEngine::Cpu) == Result::NonSupport);
#else
    delete cpuRenderer;
    _fontDiagnostics();
#endif

    if (failures) std::fprintf(stderr, "%d renderer check(s) failed\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
