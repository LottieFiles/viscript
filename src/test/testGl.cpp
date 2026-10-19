#include <cstdio>
#include <new>

#include "testGlContext.h"
#include "vis.h"
#include "vis_gl.h"

using namespace vis;

static auto failures = 0;

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                                        \
        }                                                                                      \
    } while (false)

int main()
{
    GlTestContext context;
    auto ready = context.init();
    CHECK(ready);
    if (!ready) return 1;

    Config config;
    config.width = context.width;
    config.height = context.height;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    auto space = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    auto circle = Circle::gen({}, 1.25f);
    CHECK(scene && space && circle);
    if (!scene || !space || !circle) {
        delete circle;
        delete space;
        delete scene;
        return 1;
    }
    space->style.stroke.a = 0;
    space->axisX.a = 0;
    space->axisY.a = 0;
    space->axisZ.a = 0;
    circle->stroke(Color::hex("#ffd166"), 4.0f).fill(Color::hex("#4cc9f080"));
    CHECK(space->add(circle) == Result::Success);
    auto added = scene->add(space);
    CHECK(added == Result::Success);
    if (added != Result::Success) {
        delete space;
        delete scene;
        return 1;
    }

    auto child = Scene::gen(config);
    auto childSpace = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    auto childCircle = Circle::gen({}, 10.0f);
    CHECK(child && childSpace && childCircle);
    if (!child || !childSpace || !childCircle) {
        delete childCircle;
        delete childSpace;
        delete child;
        delete scene;
        return 1;
    }
    child->config().background.a = 0;
    childSpace->progress = 0.0f;
    childCircle->style.stroke.a = 0;
    childCircle->fill(Color::hex("#40d8a0"));
    CHECK(childSpace->add(childCircle) == Result::Success);
    CHECK(child->add(childSpace) == Result::Success);
    auto mounted = scene->viewport(child, {0.25f, 0.25f, 0.5f, 0.5f});
    CHECK(mounted == Result::Success);
    if (mounted != Result::Success) {
        delete child;
        delete scene;
        return 1;
    }

    CGLSetCurrentContext(context.context);
    auto renderer = gl::Renderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        gl::Target target;
        target.context = context.context;
        target.id = static_cast<int32_t>(context.fbo);
        target.width = context.width;
        target.height = context.height;
        CHECK(renderer->render(scene, 0.0f, target) == Result::Success);

        auto bytes = new (std::nothrow) uint8_t[static_cast<size_t>(context.width) * context.height * 4];
        CHECK(bytes != nullptr);
        if (bytes) {
            context.bindFramebuffer(GL_FRAMEBUFFER, context.fbo);
            context.readPixels(0, 0, context.width, context.height, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
            auto changed = 0u;
            for (auto i = 0u; i < context.width * context.height; i++) {
                auto pixel = bytes + static_cast<size_t>(i) * 4;
                if (pixel[0] != config.background.r || pixel[1] != config.background.g || pixel[2] != config.background.b || pixel[3] != config.background.a) changed++;
            }
            CHECK(changed > 1000);
            auto outside = bytes + (45u * context.width + 20u) * 4u;
            CHECK(outside[0] == config.background.r && outside[1] == config.background.g
                  && outside[2] == config.background.b && outside[3] == config.background.a);
            auto center = bytes + (45u * context.width + 80u) * 4u;
            auto green = Color::hex("#40d8a0");
            CHECK(center[0] == green.r && center[1] == green.g
                  && center[2] == green.b && center[3] == green.a);
            delete[] bytes;
        }
    }
    delete renderer;

    delete scene;
    return failures ? 1 : 0;
}
