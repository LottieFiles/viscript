#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#include "vis_motion.h"

using namespace vis;
using namespace vis::motion;

static_assert(std::is_base_of<Group, Controller>::value,
              "Motion Controller remains a renderer group");

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

static void _definitions()
{
    auto scene = Scene::gen();
    auto object = Rectangle::gen({}, {1.0f, 1.0f});
    auto foreignScene = Scene::gen();
    auto foreign = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && object && foreignScene && foreign);
    if (!scene || !object || !foreignScene || !foreign) {
        delete scene;
        delete object;
        delete foreignScene;
        delete foreign;
        return;
    }
    CHECK(scene->add(object) == Result::Success);
    CHECK(foreignScene->add(foreign) == Result::Success);
    auto controller = Controller::gen(scene);
    CHECK(controller && controller->scene() == scene);
    CHECK(Controller::gen(scene) == nullptr);
    if (!controller) {
        delete scene;
        delete foreignScene;
        return;
    }

    State idle;
    State active;
    active.shift = {2.0f, 1.0f, 0.0f};
    active.scale = {1.2f, 1.2f, 1.0f};
    active.opacity = 0.75f;
    active.progress = 0.5f;
    CHECK(controller->define(object, "idle", idle) == Result::Success);
    CHECK(controller->define(object, "answer.correct", active) == Result::Success);
    CHECK(controller->definitionCount() == 2u);
    CHECK(controller->define(object, "idle", idle) == Result::InsufficientCondition);
    CHECK(controller->define(object, "Bad", idle) == Result::InvalidArguments);
    CHECK(controller->define(object, "bad-", idle) == Result::InvalidArguments);
    CHECK(controller->define(foreign, "idle", idle) == Result::InvalidArguments);
    auto invalid = idle;
    invalid.opacity = std::numeric_limits<float>::quiet_NaN();
    CHECK(controller->define(object, "invalid", invalid) == Result::InvalidArguments);
    CHECK(controller->transition(object, "missing") == Result::InsufficientCondition);
    CHECK(controller->transition(nullptr, "idle") == Result::InvalidArguments);
    CHECK(controller->advance(-0.1f) == Result::InvalidArguments);
    CHECK(controller->advance(std::numeric_limits<float>::infinity())
          == Result::InvalidArguments);

    delete foreignScene;
    delete scene;
}

static void _retarget()
{
    auto scene = Scene::gen();
    auto object = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && object);
    if (!scene || !object) {
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

    State right;
    right.shift = {2.0f, 0.0f, 0.0f};
    State correct;
    correct.shift = {2.0f, 1.0f, 0.0f};
    correct.scale = {1.2f, 1.2f, 1.0f};
    correct.opacity = 0.5f;
    CHECK(controller->define(object, "right", right) == Result::Success);
    CHECK(controller->define(object, "correct", correct) == Result::Success);
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    CHECK(controller->transition(object, "right", 1.0f, linear) == Result::Success);
    CHECK(controller->active());
    State state;
    CHECK(controller->sample(object, state) == Result::Success);
    CHECK(_near(state.shift.x, 0.0f));
    CHECK(controller->advance(0.4f) == Result::Success);
    CHECK(controller->sample(object, state) == Result::Success);
    CHECK(_near(state.shift.x, 0.8f));
    auto before = state;
    CHECK(controller->transition(object, "correct", 1.0f, linear) == Result::Success);
    CHECK(controller->sample(object, state) == Result::Success);
    CHECK(_near(state.shift.x, before.shift.x));
    CHECK(_near(state.shift.y, before.shift.y));
    CHECK(_near(state.scale.x, before.scale.x));
    CHECK(controller->advance(0.5f) == Result::Success);
    CHECK(controller->sample(object, state) == Result::Success);
    CHECK(_near(state.shift.x, 1.4f));
    CHECK(_near(state.shift.y, 0.5f));
    CHECK(_near(state.scale.x, 1.1f));
    CHECK(_near(state.opacity, 0.75f));
    CHECK(controller->advance(0.5f) == Result::Success);
    CHECK(controller->sample(object, state) == Result::Success);
    CHECK(_near(state.shift.x, correct.shift.x));
    CHECK(_near(state.shift.y, correct.shift.y));
    CHECK(!controller->active());
    CHECK(controller->eventCount() == 2u);
    Event event;
    CHECK(controller->eventAt(0u, event));
    CHECK(event.object == object->id() && std::strcmp(event.state, "right") == 0);
    auto firstTransaction = event.transaction;
    CHECK(firstTransaction != 0u);
    CHECK(_near(static_cast<float>(event.time), 0.0f));
    CHECK(controller->eventAt(1u, event));
    CHECK(std::strcmp(event.state, "correct") == 0);
    CHECK(event.transaction == firstTransaction + 1u);
    CHECK(_near(static_cast<float>(event.time), 0.4f));
    CHECK(!controller->eventAt(2u, event));
    controller->clearEvents();
    CHECK(controller->eventCount() == 0u);

    delete scene;
}

static void _atomicBatch()
{
    auto scene = Scene::gen();
    auto first = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto second = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(scene && first && second);
    if (!scene || !first || !second) {
        delete scene;
        delete first;
        delete second;
        return;
    }
    CHECK(scene->add(first) == Result::Success);
    CHECK(scene->add(second) == Result::Success);
    auto controller = Controller::gen(scene);
    CHECK(controller != nullptr);
    if (!controller) {
        delete scene;
        return;
    }
    State selected;
    selected.scale = {1.5f, 1.5f, 1.0f};
    CHECK(controller->define(first, "selected", selected) == Result::Success);
    CHECK(controller->define(second, "selected", selected) == Result::Success);
    Target invalid[] = {{first, "selected"}, {second, "missing"}};
    CHECK(controller->transition(invalid, 2u) == Result::InsufficientCondition);
    CHECK(controller->eventCount() == 0u);
    State sample;
    CHECK(controller->sample(first, sample) == Result::InsufficientCondition);
    Target duplicate[] = {{first, "selected"}, {first, "selected"}};
    CHECK(controller->transition(duplicate, 2u) == Result::InvalidArguments);
    Target valid[] = {{second, "selected"}, {first, "selected"}};
    CHECK(controller->transition(valid, 2u, 0.0f) == Result::Success);
    CHECK(controller->eventCount() == 2u);
    Event firstEvent;
    Event secondEvent;
    CHECK(controller->eventAt(0u, firstEvent));
    CHECK(controller->eventAt(1u, secondEvent));
    CHECK(firstEvent.transaction != 0u);
    CHECK(firstEvent.transaction == secondEvent.transaction);
    CHECK(controller->sample(first, sample) == Result::Success);
    CHECK(_near(sample.scale.x, 1.5f));
    CHECK(controller->sample(second, sample) == Result::Success);
    CHECK(_near(sample.scale.y, 1.5f));
    delete scene;
}

static void _boundaries()
{
    auto scene = Scene::gen();
    auto object = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && object);
    if (!scene || !object) {
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
    char longest[Controller::StateNameLimit + 1u];
    char tooLong[Controller::StateNameLimit + 2u];
    std::memset(longest, 'a', Controller::StateNameLimit);
    std::memset(tooLong, 'a', Controller::StateNameLimit + 1u);
    longest[Controller::StateNameLimit] = '\0';
    tooLong[Controller::StateNameLimit + 1u] = '\0';
    State state;
    CHECK(controller->define(object, longest, state) == Result::Success);
    CHECK(controller->define(object, tooLong, state) == Result::InvalidArguments);
    auto target = Target{object, longest};
    CHECK(controller->transition(static_cast<const Target*>(nullptr), 0u)
          == Result::InvalidArguments);
    CHECK(controller->transition(&target, 0u) == Result::InvalidArguments);
    CHECK(controller->transition(&target, Controller::ObjectLimit + 1u)
          == Result::InvalidArguments);
    CHECK(controller->transition(object, longest,
                                 std::numeric_limits<float>::quiet_NaN())
          == Result::InvalidArguments);
    CHECK(controller->transition(object, longest,
                                 std::numeric_limits<float>::infinity())
          == Result::InvalidArguments);
    CHECK(controller->transition(object, longest, -0.1f)
          == Result::InvalidArguments);
    CHECK(controller->transition(object, longest, 0.1f,
                                 static_cast<Easing>(UINT8_MAX))
          == Result::InvalidArguments);
    auto invalidCurve = AnimCurve::preset(AnimCurvePreset::Linear);
    invalidCurve.kind = AnimCurvePreset::Custom;
    invalidCurve.control1.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(controller->transition(object, longest, 0.1f, invalidCurve)
          == Result::InvalidArguments);
    CHECK(controller->eventCount() == 0u);
    delete scene;
}

static void _journalBackpressure()
{
    auto scene = Scene::gen();
    auto object = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && object);
    if (!scene || !object) {
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
    State left;
    State right;
    left.shift.x = -1.0f;
    right.shift.x = 1.0f;
    CHECK(controller->define(object, "left", left) == Result::Success);
    CHECK(controller->define(object, "right", right) == Result::Success);
    for (auto i = 0u; i < Controller::EventLimit; i++) {
        auto name = i % 2u ? "right" : "left";
        CHECK(controller->transition(object, name, 0.0f) == Result::Success);
    }
    CHECK(controller->eventCount() == Controller::EventLimit);
    Event last;
    CHECK(controller->eventAt(Controller::EventLimit - 1u, last));
    State before;
    State after;
    CHECK(controller->sample(object, before) == Result::Success);
    CHECK(controller->transition(object, "left", 0.0f)
          == Result::InsufficientCondition);
    CHECK(controller->eventCount() == Controller::EventLimit);
    CHECK(controller->sample(object, after) == Result::Success);
    CHECK(_near(after.shift.x, before.shift.x));
    controller->clearEvents();
    CHECK(controller->transition(object, "left", 0.0f) == Result::Success);
    Event resumed;
    CHECK(controller->eventAt(0u, resumed));
    CHECK(resumed.transaction == last.transaction + 1u);
    delete scene;
}

int main()
{
    _definitions();
    _retarget();
    _atomicBatch();
    _boundaries();
    _journalBackpressure();
    if (failures) std::fprintf(stderr, "%d motion test(s) failed\n", failures);
    return failures ? 1 : 0;
}
