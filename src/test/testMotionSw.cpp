#include <cmath>
#include <cstdio>

#include "vis_motion.h"

using namespace vis;
using namespace vis::motion;

static int failures = 0;

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                                        \
        }                                                                                      \
    } while (false)

static bool _near(float lhs, float rhs, float epsilon = 1.0e-4f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

static void _clocks(SwRenderer* renderer)
{
    Config config;
    config.width = 200u;
    config.height = 100u;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto object = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && object && renderer);
    if (!scene || !object || !renderer) {
        delete scene;
        delete object;
        return;
    }
    CHECK(scene->add(object) == Result::Success);
    CHECK(scene->play(Animation::shift(object, {0.0f, 1.0f, 0.0f}), 1.0f,
                      Easing::Linear)
          == Result::Success);
    auto controller = Controller::gen(scene);
    CHECK(controller != nullptr);
    if (!controller) {
        delete scene;
        return;
    }
    State focused;
    focused.shift = {2.0f, 0.0f, 0.0f};
    CHECK(controller->define(object, "focused", focused) == Result::Success);
    CHECK(controller->transition(object, "focused", 1.0f, Easing::Linear)
          == Result::Success);
    CHECK(controller->advance(0.5f) == Result::Success);
    BBox begin;
    BBox end;
    BBox resampled;
    CHECK(renderer->bounds(scene, object, 0.0f, begin) == Result::Success);
    CHECK(renderer->bounds(scene, object, 1.0f, end) == Result::Success);
    CHECK(renderer->bounds(scene, object, 0.0f, resampled) == Result::Success);
    CHECK(_near(begin.center().x, 125.0f, 0.1f));
    CHECK(_near(end.center().x, begin.center().x, 0.1f));
    CHECK(end.center().y < begin.center().y);
    CHECK(_near(resampled.center().x, begin.center().x, 0.1f));
    CHECK(_near(resampled.center().y, begin.center().y, 0.1f));
    CHECK(_near(static_cast<float>(controller->time()), 0.5f));
    delete scene;
}

static void _visibility(SwRenderer* renderer)
{
    auto scene = Scene::gen();
    auto object = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && object && renderer);
    if (!scene || !object || !renderer) {
        delete scene;
        delete object;
        return;
    }
    CHECK(scene->add(object) == Result::Success);
    auto controller = Controller::gen(scene);
    CHECK(controller != nullptr);
    if (!controller) {
        delete scene;
        return;
    }
    State visible;
    State transparent;
    transparent.opacity = 0.0f;
    State undrawn;
    undrawn.progress = 0.0f;
    CHECK(controller->define(object, "visible", visible) == Result::Success);
    CHECK(controller->define(object, "transparent", transparent) == Result::Success);
    CHECK(controller->define(object, "undrawn", undrawn) == Result::Success);
    BBox bounds;
    CHECK(renderer->bounds(scene, object, 0.0f, bounds) == Result::Success);
    CHECK(controller->transition(object, "transparent", 0.0f) == Result::Success);
    CHECK(renderer->bounds(scene, object, 0.0f, bounds)
          == Result::InsufficientCondition);
    CHECK(controller->transition(object, "undrawn", 0.0f) == Result::Success);
    CHECK(renderer->bounds(scene, object, 0.0f, bounds)
          == Result::InsufficientCondition);
    CHECK(controller->transition(object, "visible", 0.0f) == Result::Success);
    CHECK(renderer->bounds(scene, object, 0.0f, bounds) == Result::Success);
    delete scene;
}

static void _pivot(SwRenderer* renderer)
{
    Config config;
    config.width = 200u;
    config.height = 100u;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto object = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(scene && object && renderer);
    if (!scene || !object || !renderer) {
        delete scene;
        delete object;
        return;
    }
    CHECK(scene->add(object) == Result::Success);
    auto target = Mat4::translate({0.0f, 1.0f, 0.0f})
        * Mat4::rotateZ(0.4f)
        * Mat4::scale({1.2f, 0.8f, 1.0f})
        * object->model;
    CHECK(scene->play(Animation::transform(object, target), 1.0f, Easing::Linear)
          == Result::Success);
    auto controller = Controller::gen(scene);
    CHECK(controller != nullptr);
    if (!controller) {
        delete scene;
        return;
    }
    State focused;
    focused.scale = {2.0f, 2.0f, 1.0f};
    CHECK(controller->define(object, "focused", focused) == Result::Success);
    BBox before;
    BBox beforeEnd;
    BBox after;
    BBox afterEnd;
    CHECK(renderer->bounds(scene, object, 0.0f, before) == Result::Success);
    CHECK(renderer->bounds(scene, object, 1.0f, beforeEnd) == Result::Success);
    CHECK(controller->transition(object, "focused", 0.0f) == Result::Success);
    CHECK(renderer->bounds(scene, object, 0.0f, after) == Result::Success);
    CHECK(renderer->bounds(scene, object, 1.0f, afterEnd) == Result::Success);
    CHECK(_near(after.center().x, before.center().x, 0.1f));
    CHECK(_near(after.center().y, before.center().y, 0.1f));
    CHECK(_near(afterEnd.center().x, beforeEnd.center().x, 0.1f));
    CHECK(_near(afterEnd.center().y, beforeEnd.center().y, 0.1f));
    CHECK(after.width > before.width * 1.5f);
    CHECK(after.height > before.height * 1.5f);
    State sampled;
    CHECK(controller->sample(object, sampled) == Result::Success);
    CHECK(sampled.originEnabled);
    CHECK(_near(sampled.origin.x, 2.0f));
    CHECK(_near(sampled.origin.y, 0.0f));
    delete scene;
}

int main()
{
    auto renderer = SwRenderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        _clocks(renderer);
        _pivot(renderer);
        _visibility(renderer);
    }
    delete renderer;
    if (failures) std::fprintf(stderr, "%d motion render test(s) failed\n", failures);
    return failures ? 1 : 0;
}
