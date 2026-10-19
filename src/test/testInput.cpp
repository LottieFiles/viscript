#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "vis_input.h"
#if defined(VIS_UI)
#include "vis_ui.h"
#endif
#include "testLuaLoad.h"

using namespace vis;
using namespace vis::input;

static_assert(std::is_base_of<Group, Controller>::value,
              "Input Controller remains a renderer group");
static_assert(!std::is_copy_constructible<State>::value,
              "Input State has unique ownership");
static_assert(!std::is_copy_constructible<ActionMap>::value,
              "Input ActionMap has unique ownership");

static bool _near(float lhs, float rhs, float epsilon = 0.2f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

static bool precise(float lhs, float rhs)
{
    return _near(lhs, rhs, 1.0e-5f);
}

static InputEvent pointer(float x, float y, float time)
{
    InputEvent event;
    event.kind = InputKind::PointerMove;
    event.position = {x, y};
    event.time = time;
    return event;
}

static InputEvent key(InputKind kind, Key value, float time)
{
    InputEvent event;
    event.kind = kind;
    event.key = value;
    event.time = time;
    return event;
}

static void rawState()
{
    auto state = State::gen();
    CHECK(state != nullptr);
    if (!state) return;

    CHECK(state->frame() == 0u);
    CHECK(state->begin(1.0f) == Result::Success);
    CHECK(state->frame() == 1u && precise(state->time(), 1.0f));

    DigitalState digital;
    CHECK(state->key(Key::A, digital) == Result::Success);
    CHECK(!digital.previous && !digital.down && !digital.pressed && !digital.released);
    auto event = key(InputKind::KeyDown, Key::A, 1.01f);
    CHECK(state->input(event) == Result::Success);
    CHECK(state->key(Key::A, digital) == Result::Success);
    CHECK(!digital.previous && digital.down && digital.pressed && !digital.released);
    CHECK(precise(digital.begin, 1.01f) && precise(digital.end, 1.01f));

    // A key-down repeated while the key is held keeps the press where it began.
    event.time = 1.02f;
    CHECK(state->input(event) == Result::Success);
    CHECK(state->key(Key::A, digital) == Result::Success);
    CHECK(digital.down && digital.pressed && !digital.released && precise(digital.begin, 1.01f));

    event = {};
    event.kind = InputKind::PointerDown;
    event.pointer = 7u;
    event.position = {20.0f, 30.0f};
    event.button = 0;
    event.buttons = 1u;
    event.time = 1.03f;
    CHECK(state->input(event) == Result::Success);
    event.kind = InputKind::PointerMove;
    event.position = {23.0f, 28.0f};
    event.delta = {3.0f, -2.0f};
    event.time = 1.04f;
    CHECK(state->input(event) == Result::Success);
    event.position = {25.0f, 31.0f};
    event.delta = {2.0f, 3.0f};
    event.time = 1.045f;
    CHECK(state->input(event) == Result::Success);
    event.kind = InputKind::Wheel;
    event.wheel = {1.0f, -4.0f};
    event.time = 1.05f;
    CHECK(state->input(event) == Result::Success);
    event.wheel = {-0.5f, 1.0f};
    event.time = 1.055f;
    CHECK(state->input(event) == Result::Success);

    PointerState pointerState;
    CHECK(state->pointer(7u, pointerState) == Result::Success);
    CHECK(pointerState.active && pointerState.pointer == 7u);
    CHECK(pointerState.buttons == 1u && pointerState.pressed == 1u
          && pointerState.released == 0u);
    CHECK(precise(pointerState.position.x, 25.0f)
          && precise(pointerState.position.y, 31.0f));
    CHECK(precise(pointerState.delta.x, 5.0f) && precise(pointerState.delta.y, 1.0f));
    CHECK(precise(pointerState.wheel.x, 0.5f)
          && precise(pointerState.wheel.y, -3.0f));

    CHECK(state->begin(1.1f) == Result::Success);
    CHECK(state->key(Key::A, digital) == Result::Success);
    CHECK(digital.previous && digital.down && !digital.pressed && !digital.released);
    CHECK(state->pointer(7u, pointerState) == Result::Success);
    CHECK(pointerState.previousButtons == 1u && pointerState.buttons == 1u);
    CHECK(pointerState.pressed == 0u && pointerState.released == 0u);
    CHECK(precise(pointerState.delta.x, 0.0f)
          && precise(pointerState.delta.y, 0.0f));
    CHECK(precise(pointerState.wheel.x, 0.0f)
          && precise(pointerState.wheel.y, 0.0f));

    CHECK(state->input(key(InputKind::KeyUp, Key::A, 1.11f)) == Result::Success);
    event.kind = InputKind::PointerUp;
    event.button = 0;
    event.buttons = 0u;
    event.time = 1.12f;
    CHECK(state->input(event) == Result::Success);
    CHECK(state->key(Key::A, digital) == Result::Success);
    CHECK(digital.previous && !digital.down && !digital.pressed && digital.released);
    CHECK(state->pointer(7u, pointerState) == Result::Success);
    CHECK(pointerState.previousButtons == 1u && pointerState.buttons == 0u
          && pointerState.released == 1u);

    CHECK(state->begin(1.15f) == Result::Success);
    CHECK(state->input(key(InputKind::KeyDown, Key::C, 1.16f)) == Result::Success);
    CHECK(state->input(key(InputKind::KeyUp, Key::C, 1.17f)) == Result::Success);
    CHECK(state->key(Key::C, digital) == Result::Success);
    CHECK(!digital.previous && !digital.down && digital.pressed && digital.released);
    CHECK(precise(digital.begin, 1.16f) && precise(digital.end, 1.17f));
    CHECK(state->begin(1.18f) == Result::Success);
    CHECK(state->key(Key::C, digital) == Result::Success);
    CHECK(!digital.previous && !digital.down && !digital.pressed && !digital.released);

    CHECK(state->begin(1.2f) == Result::Success);
    CHECK(state->input(key(InputKind::KeyDown, Key::B, 1.21f)) == Result::Success);
    event.kind = InputKind::PointerDown;
    event.buttons = 2u;
    event.button = 1;
    event.time = 1.22f;
    CHECK(state->input(event) == Result::Success);
    CHECK(state->release(1.3f) == Result::Success);
    CHECK(state->key(Key::B, digital) == Result::Success);
    CHECK(!digital.down && digital.pressed && digital.released
          && precise(digital.end, 1.3f));
    CHECK(state->pointer(7u, pointerState) == Result::Success);
    CHECK(!pointerState.active && pointerState.buttons == 0u
          && (pointerState.released & 2u) != 0u);

    event = key(InputKind::KeyDown, Key::Unknown, 1.4f);
    CHECK(state->input(event) == Result::InvalidArguments);
    CHECK(state->begin(-1.0f) == Result::InvalidArguments);
    CHECK(state->key(Key::Unknown, digital) == Result::InvalidArguments);
    CHECK(state->pointer(999u, pointerState) == Result::InsufficientCondition);
    delete state;

    state = State::gen();
    CHECK(state != nullptr);
    if (!state) return;
    for (auto i = 0u; i < State::PointerLimit; i++) {
        event = pointer(static_cast<float>(i), 0.0f, 0.0f);
        event.pointer = i;
        event.delta = {1.0f, 0.0f};
        CHECK(state->input(event) == Result::Success);
    }
    event.pointer = State::PointerLimit;
    CHECK(state->input(event) == Result::InsufficientCondition);

    // A pointer that has left gives its slot to a new one once its edges are reported, so
    // touches, which bring a new ID each, keep working past the limit.
    event.kind = InputKind::PointerCancel;
    event.pointer = 5u;
    CHECK(state->input(event) == Result::Success);
    event.kind = InputKind::PointerMove;
    event.pointer = State::PointerLimit;
    CHECK(state->input(event) == Result::Success);
    CHECK(state->pointer(5u, pointerState) == Result::InsufficientCondition);
    CHECK(state->pointer(State::PointerLimit, pointerState) == Result::Success);
    CHECK(pointerState.active && precise(pointerState.delta.x, 1.0f));

    event.kind = InputKind::PointerDown;
    event.pointer = 3u;
    event.button = 0;
    event.buttons = 1u;
    CHECK(state->input(event) == Result::Success);
    event.kind = InputKind::PointerCancel;
    CHECK(state->input(event) == Result::Success);
    event.kind = InputKind::PointerMove;
    event.pointer = State::PointerLimit + 1u;
    event.buttons = 0u;
    CHECK(state->input(event) == Result::InsufficientCondition);
    CHECK(state->begin(0.1f) == Result::Success);
    CHECK(state->input(event) == Result::Success);
    CHECK(state->pointer(3u, pointerState) == Result::InsufficientCondition);
    delete state;
}

static void actions()
{
    auto state = State::gen();
    auto actions = ActionMap::gen();
    CHECK(state && actions);
    if (!state || !actions) {
        delete state;
        delete actions;
        return;
    }

    CHECK(actions->key("Move", Key::A, {-1.0f, 0.0f}) == Result::Success);
    CHECK(actions->key("Move", Key::D, {1.0f, 0.0f}) == Result::Success);
    CHECK(actions->key("Move", Key::W, {0.0f, 1.0f}) == Result::Success);
    CHECK(actions->key("Move", Key::S, {0.0f, -1.0f}) == Result::Success);
    CHECK(actions->key("Boost", Key::Space) == Result::Success);
    CHECK(actions->key("Boost", Key::Enter) == Result::Success);
    CHECK(actions->key("Boost", Key::Shift) == Result::Success);
    CHECK(actions->key("Switch", Key::Tab) == Result::Success);
    CHECK(actions->pointer("Place", 0u) == Result::Success);
    CHECK(actions->count() == 9u);

    CHECK(state->begin(2.0f) == Result::Success);
    CHECK(state->input(key(InputKind::KeyDown, Key::A, 2.01f)) == Result::Success);
    CHECK(state->input(key(InputKind::KeyDown, Key::W, 2.02f)) == Result::Success);
    ActionState action;
    CHECK(actions->state(state, "Move", action) == Result::Success);
    CHECK(action.down && action.pressed && !action.released);
    CHECK(precise(action.previous.x, 0.0f) && precise(action.previous.y, 0.0f));
    CHECK(precise(action.value.x, -1.0f) && precise(action.value.y, 1.0f));
    CHECK(precise(action.delta.x, -1.0f) && precise(action.delta.y, 1.0f));

    CHECK(state->begin(2.1f) == Result::Success);
    CHECK(actions->state(state, "Move", action) == Result::Success);
    CHECK(action.down && !action.pressed && !action.released);
    CHECK(precise(action.previous.x, -1.0f) && precise(action.previous.y, 1.0f));
    CHECK(precise(action.value.x, -1.0f) && precise(action.value.y, 1.0f));
    CHECK(precise(action.delta.x, 0.0f) && precise(action.delta.y, 0.0f));

    CHECK(state->input(key(InputKind::KeyDown, Key::D, 2.11f)) == Result::Success);
    CHECK(actions->state(state, "Move", action) == Result::Success);
    CHECK(action.down && !action.pressed && !action.released);
    CHECK(precise(action.value.x, 0.0f) && precise(action.value.y, 1.0f));
    CHECK(precise(action.delta.x, 1.0f) && precise(action.delta.y, 0.0f));

    auto event = InputEvent{};
    event.kind = InputKind::PointerDown;
    event.pointer = 0u;
    event.button = 0;
    event.buttons = 1u;
    event.position = {40.0f, 30.0f};
    event.time = 2.12f;
    CHECK(state->input(event) == Result::Success);
    CHECK(actions->state(state, "Place", action) == Result::Success);
    CHECK(action.down && action.pressed && precise(action.value.x, 1.0f));

    CHECK(state->release(2.2f) == Result::Success);
    CHECK(actions->state(state, "Move", action) == Result::Success);
    CHECK(!action.down && !action.pressed && action.released);
    CHECK(precise(action.value.x, 0.0f) && precise(action.value.y, 0.0f));
    CHECK(actions->state(state, "Place", action) == Result::Success);
    CHECK(!action.down && action.released);

    CHECK(state->begin(2.3f) == Result::Success);
    CHECK(state->input(key(InputKind::KeyDown, Key::Space, 2.31f)) == Result::Success);
    CHECK(state->input(key(InputKind::KeyDown, Key::Enter, 2.32f)) == Result::Success);
    CHECK(state->input(key(InputKind::KeyDown, Key::Shift, 2.33f)) == Result::Success);
    CHECK(actions->state(state, "Boost", action) == Result::Success);
    CHECK(action.down && action.pressed && !action.released);
    CHECK(state->begin(2.4f) == Result::Success);
    CHECK(state->input(key(InputKind::KeyUp, Key::Space, 2.41f)) == Result::Success);
    CHECK(actions->state(state, "Boost", action) == Result::Success);
    CHECK(action.down && !action.pressed && !action.released);
    CHECK(state->input(key(InputKind::KeyUp, Key::Enter, 2.42f)) == Result::Success);
    CHECK(actions->state(state, "Boost", action) == Result::Success);
    CHECK(action.down && !action.pressed && !action.released);
    CHECK(state->input(key(InputKind::KeyUp, Key::Shift, 2.43f)) == Result::Success);
    CHECK(actions->state(state, "Boost", action) == Result::Success);
    CHECK(!action.down && !action.pressed && action.released);

    CHECK(state->input(key(InputKind::KeyDown, Key::Tab, 2.44f)) == Result::Success);
    CHECK(actions->state(state, "Switch", action) == Result::Success);
    CHECK(action.down && action.pressed && !action.released);

    CHECK(actions->key("Boost", Key::Space, {0.5f, 0.0f}) == Result::Success);
    CHECK(actions->count() == 9u);
    CHECK(actions->remove("Boost") == Result::Success);
    CHECK(actions->state(state, "Boost", action) == Result::InsufficientCondition);
    CHECK(actions->remove("Boost") == Result::InsufficientCondition);
    CHECK(actions->key("", Key::Space) == Result::InvalidArguments);
    CHECK(actions->key("Zero", Key::Space, {}) == Result::InvalidArguments);
    CHECK(actions->pointer("Bad", 32u) == Result::InvalidArguments);
    char validName[ActionMap::NameLimit + 1u];
    std::memset(validName, 'a', ActionMap::NameLimit);
    validName[ActionMap::NameLimit] = '\0';
    CHECK(actions->key(validName, Key::Space) == Result::Success);
    char invalidName[ActionMap::NameLimit + 2u];
    std::memset(invalidName, 'a', ActionMap::NameLimit + 1u);
    invalidName[ActionMap::NameLimit + 1u] = '\0';
    CHECK(actions->key(invalidName, Key::Space) == Result::InvalidArguments);
    actions->clear();
    for (auto i = 0u; i < ActionMap::BindingLimit; i++) {
        char name[24];
        std::snprintf(name, sizeof(name), "Action%u", i);
        CHECK(actions->key(name, Key::Space) == Result::Success);
    }
    CHECK(actions->count() == ActionMap::BindingLimit);
    CHECK(actions->key("Overflow", Key::Space) == Result::InsufficientCondition);
    actions->clear();
    CHECK(actions->count() == 0u);
    delete actions;
    delete state;
}

static void pointerFollow(Renderer* renderer)
{
    Config config;
    config.width = 200u;
    config.height = 100u;
    config.loop = true;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto target = Rectangle::gen({}, {1.2f, 0.3f});
    CHECK(scene && target && renderer);
    if (!scene || !target || !renderer) {
        delete scene;
        delete target;
        return;
    }
    CHECK(scene->add(target) == Result::Success);
    CHECK(scene->wait(3.0f) == Result::Success);
    auto authored = target->model;
    auto controller = Controller::gen(scene);
    CHECK(controller && controller->scene() == scene);
    CHECK(Controller::gen(scene) == nullptr);
    if (!controller) {
        delete scene;
        return;
    }

    PointerFollow binding;
    binding.target = target;
    binding.region = {0.0f, 0.0f, 200.0f, 100.0f};
    binding.mapOrigin = {-4.0f, 2.0f, 0.0f};
    binding.mapX = {8.0f, 0.0f, 0.0f};
    binding.mapY = {0.0f, -4.0f, 0.0f};
    binding.period = 0.4f;
    binding.resetOnLeave = true;
    CHECK(controller->pointerFollow(binding) == Result::Success);
    CHECK(controller->pointerFollow(binding) == Result::InsufficientCondition);

    BBox initial;
    BBox middle;
    BBox end;
    CHECK(renderer->bounds(scene, target, 0.0f, initial) == Result::Success);
    auto result = controller->input(pointer(150.0f, 25.0f, 0.0f));
    CHECK(result.handled && result.redraw && result.status == Result::Success);
    CHECK(renderer->bounds(scene, target, 0.2f, middle) == Result::Success);
    CHECK(renderer->bounds(scene, target, 0.4f, end) == Result::Success);
    CHECK(_near(middle.center().x, 125.0f) && _near(middle.center().y, 37.5f));
    CHECK(_near(end.center().x, 150.0f) && _near(end.center().y, 25.0f));
    CHECK(end.height > initial.height);
    BBox resampledMiddle;
    CHECK(renderer->bounds(scene, target, 0.2f, resampledMiddle) == Result::Success);
    CHECK(_near(resampledMiddle.center().x, middle.center().x));
    CHECK(_near(resampledMiddle.center().y, middle.center().y));

    CHECK(controller->input(pointer(50.0f, 75.0f, 0.4f)).handled);
    BBox beforeRetarget;
    BBox afterRetarget;
    CHECK(renderer->bounds(scene, target, 0.5f, beforeRetarget) == Result::Success);
    CHECK(controller->input(pointer(100.0f, 50.0f, 0.5f)).handled);
    CHECK(renderer->bounds(scene, target, 0.5f, afterRetarget) == Result::Success);
    CHECK(_near(beforeRetarget.center().x, afterRetarget.center().x));
    CHECK(_near(beforeRetarget.center().y, afterRetarget.center().y));
    CHECK(renderer->bounds(scene, target, 0.9f, end) == Result::Success);
    CHECK(_near(end.center().x, 100.0f) && _near(end.center().y, 50.0f));
    BBox wrapped;
    CHECK(renderer->bounds(scene, target, 0.1f, wrapped) == Result::Success);
    CHECK(_near(wrapped.center().x, end.center().x));
    CHECK(_near(wrapped.center().y, end.center().y));

    auto cancel = pointer(100.0f, 50.0f, 1.0f);
    cancel.kind = InputKind::PointerCancel;
    result = controller->input(cancel);
    CHECK(result.handled && result.redraw);
    CHECK(renderer->bounds(scene, target, 1.4f, end) == Result::Success);
    CHECK(_near(end.center().x, initial.center().x) && _near(end.center().y, initial.center().y));
    for (auto i = 0u; i < 16u; i++) CHECK(_near(target->model.e[i], authored.e[i], 1.0e-4f));

    auto invalid = binding;
    invalid.period = -1.0f;
    invalid.target = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(controller->pointerFollow(invalid) == Result::InvalidArguments);
    delete invalid.target;
    delete scene;
}

// Any finite angle offset turns the follower to one angle, however many turns it holds.
static void pointerAngle(Renderer* renderer)
{
    Config config;
    config.width = 200u;
    config.height = 100u;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto target = Rectangle::gen({}, {1.2f, 0.3f});
    CHECK(scene && target && renderer);
    if (!scene || !target || !renderer) {
        delete scene;
        delete target;
        return;
    }
    CHECK(scene->add(target) == Result::Success);
    CHECK(scene->wait(3.0f) == Result::Success);
    auto controller = Controller::gen(scene);
    CHECK(controller != nullptr);
    if (!controller) {
        delete scene;
        return;
    }
    PointerFollow binding;
    binding.target = target;
    binding.region = {0.0f, 0.0f, 200.0f, 100.0f};
    binding.mapOrigin = {-4.0f, 2.0f, 0.0f};
    binding.mapX = {8.0f, 0.0f, 0.0f};
    binding.mapY = {0.0f, -4.0f, 0.0f};
    binding.period = 0.4f;
    binding.angleOffset = 1.0e30f;
    CHECK(controller->pointerFollow(binding) == Result::Success);
    auto result = controller->input(pointer(150.0f, 25.0f, 0.0f));
    CHECK(result.handled && result.redraw && result.status == Result::Success);
    BBox first;
    BBox second;
    CHECK(renderer->bounds(scene, target, 0.4f, first) == Result::Success);
    // Such an offset leaves no room for the pointer's direction, so a move the other way keeps
    // the angle.
    result = controller->input(pointer(50.0f, 75.0f, 0.4f));
    CHECK(result.handled && result.redraw && result.status == Result::Success);
    CHECK(renderer->bounds(scene, target, 0.8f, second) == Result::Success);
    CHECK(near(second.width, first.width, 0.5f) && near(second.height, first.height, 0.5f));
    delete scene;
}

static void keyboard(Renderer* renderer)
{
    Config config;
    config.width = 200u;
    config.height = 100u;
    config.loop = true;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto target = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && target && renderer);
    if (!scene || !target || !renderer) {
        delete scene;
        delete target;
        return;
    }
    CHECK(scene->add(target) == Result::Success);
    CHECK(scene->wait(3.0f) == Result::Success);
    auto controller = Controller::gen(scene);
    CHECK(controller != nullptr);
    if (!controller) {
        delete scene;
        return;
    }
    KeyMove right;
    right.target = target;
    right.key = Key::ArrowRight;
    right.shift = {1.0f, 0.0f, 0.0f};
    right.period = 0.2f;
    CHECK(controller->keyMove(right) == Result::Success);
    CHECK(controller->keyMove(right) == Result::InsufficientCondition);
    auto left = right;
    left.key = Key::A;
    left.shift = {-1.0f, 0.0f, 0.0f};
    CHECK(controller->keyMove(left) == Result::Success);

    BBox initial;
    BBox middle;
    BBox end;
    CHECK(renderer->bounds(scene, target, 1.0f, initial) == Result::Success);
    auto result = controller->input(key(InputKind::KeyDown, Key::ArrowRight, 1.0f));
    CHECK(result.handled && result.redraw);
    CHECK(renderer->bounds(scene, target, 1.1f, middle) == Result::Success);
    CHECK(renderer->bounds(scene, target, 1.2f, end) == Result::Success);
    CHECK(middle.center().x > initial.center().x && middle.center().x < end.center().x);
    CHECK(_near(end.center().x - initial.center().x, 25.0f));

    // A key-down repeated while the key is held is no new press: the move goes on as it was.
    result = controller->input(key(InputKind::KeyDown, Key::ArrowRight, 1.25f));
    CHECK(result.handled && !result.redraw);
    BBox held;
    CHECK(renderer->bounds(scene, target, 1.4f, held) == Result::Success);
    CHECK(_near(held.center().x - initial.center().x, 50.0f));
    result = controller->input(key(InputKind::KeyUp, Key::ArrowRight, 1.4f));
    CHECK(result.handled && result.redraw);
    CHECK(renderer->bounds(scene, target, 1.6f, end) == Result::Success);
    CHECK(_near(end.center().x, held.center().x));

    result = controller->input(key(InputKind::KeyDown, Key::A, 1.6f));
    CHECK(result.handled && result.redraw);
    CHECK(renderer->bounds(scene, target, 1.8f, end) == Result::Success);
    CHECK(_near(end.center().x - initial.center().x, 25.0f));
    result = controller->input(key(InputKind::KeyUp, Key::A, 1.8f));
    CHECK(result.handled && result.redraw);

    auto shortcut = key(InputKind::KeyDown, Key::ArrowRight, 2.0f);
    shortcut.modifiers = 2u;
    result = controller->input(shortcut);
    CHECK(!result.handled && !result.redraw && result.status == Result::Success);
    CHECK(renderer->bounds(scene, target, 2.2f, end) == Result::Success);
    CHECK(_near(end.center().x - initial.center().x, 25.0f));

    // A key with no binding is left to the host, and the controller's state still holds it.
    result = controller->input(key(InputKind::KeyDown, Key::B, 2.2f));
    CHECK(!result.handled && !result.redraw && result.status == Result::Success);
    DigitalState state;
    CHECK(controller->state()->key(Key::B, state) == Result::Success);
    CHECK(state.down && _near(state.begin, 2.2f) && _near(state.end, 2.2f));
    result = controller->input(key(InputKind::KeyUp, Key::B, 2.6f));
    CHECK(!result.handled && result.status == Result::Success);
    CHECK(controller->state()->key(Key::B, state) == Result::Success);
    CHECK(!state.down && _near(state.begin, 2.2f) && _near(state.end, 2.6f));

    CHECK(controller->input(key(InputKind::KeyDown, Key::ArrowRight, 2.8f)).handled);
    BBox beforeWrap;
    BBox afterWrap;
    CHECK(renderer->bounds(scene, target, 2.9f, beforeWrap) == Result::Success);
    CHECK(renderer->bounds(scene, target, 0.1f, afterWrap) == Result::Success);
    CHECK(afterWrap.center().x > beforeWrap.center().x);
    CHECK(controller->input(key(InputKind::KeyUp, Key::ArrowRight, 0.1f)).handled);
    delete scene;
}

static void camera()
{
    Config config;
    config.cameraMode = CameraMode::Interactive;
    auto scene = Scene::gen(config);
    auto controller = Controller::gen(scene);
    CHECK(scene && controller);
    if (scene && controller) {
        CHECK(scene->cameraInput({CameraAction::Pan, {0.1f, 0.0f}}) == Result::Success);
        CHECK(scene->cameraInput({CameraAction::View3D, {}}) == Result::Success);
        CHECK(scene->cameraInput({CameraAction::Orbit, {0.1f, -0.05f}}) == Result::Success);
        CHECK(scene->cameraInput({CameraAction::Zoom, {0.0f, -0.1f}}) == Result::Success);
        CHECK(scene->cameraInput({CameraAction::Reset, {}}) == Result::Success);
    }
    delete scene;
}

static void luaBinding()
{
    static constexpr char source[] = R"lua(
local scene = vis.scene {
    width = 200, height = 100,
    camera = {view = "2d", height = 4},
}
local pointer_target = scene:rectangle {center = {0, 0}, size = {1, 0.3}}
local key_target = scene:circle {center = {0, -1}, radius = 0.2}
local input = vis.input.controller(scene)
input:pointer_follow {
    target = pointer_target,
    region = {0, 0, 200, 100},
    target_origin = {0, 0, 0},
    map_origin = {-4, 2, 0},
    map_x = {8, 0, 0},
    map_y = {0, -4, 0},
    period = 0.2,
    rotate = true,
    reset_on_leave = true,
}
input:key_move {target = key_target, key = "ArrowRight", shift = {1, 0, 0}, period = 0.1}
scene:wait(3)
return scene
)lua";
    detail::LuaHostContext context;
    auto scene = _loaded(source, "input.lua", context);
    auto controller = scene ? context.input.controller : nullptr;
    CHECK(controller != nullptr);
    if (controller) {
        CHECK(controller->input(pointer(150.0f, 25.0f, 0.0f)).handled);
        CHECK(controller->input(key(InputKind::KeyDown, Key::ArrowRight, 0.0f)).handled);
    }
    delete scene;
}

#if defined(VIS_UI)
static void uiComposition()
{
    auto scene = Scene::gen();
    auto target = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && target);
    if (!scene || !target) {
        delete scene;
        delete target;
        return;
    }
    CHECK(scene->add(target) == Result::Success);
    auto controller = Controller::gen(scene);
    auto panel = ui::Panel::gen(scene);
    CHECK(controller && panel);
    delete scene;
}
#endif

int main()
{
    rawState();
    actions();
    auto renderer = Renderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        pointerFollow(renderer);
        pointerAngle(renderer);
        keyboard(renderer);
    }
    camera();
    luaBinding();
#if defined(VIS_UI)
    uiComposition();
#endif
    delete renderer;
    return report();
}
