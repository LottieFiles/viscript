#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#include "vis_ui.h"
#include "testLuaLoad.h"

using namespace vis;
using namespace vis::ui;

static_assert(std::is_base_of<Group, UIObject>::value, "UI controls remain renderer groups");

static bool _near(float lhs, float rhs, float epsilon = 1.0e-3f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

static InputEvent pointer(InputKind kind, float x, float y, uint32_t id = 1u,
                          float time = 0.0f)
{
    InputEvent event;
    event.kind = kind;
    event.pointer = id;
    event.position = {x, y};
    if (kind == InputKind::PointerDown || kind == InputKind::PointerUp) event.button = 0;
    event.time = time;
    return event;
}

struct SampleResolver
{
    Renderer* renderer = nullptr;
    uint32_t pixelRatio = 1u;
    Result failure = Result::Success;
};

static Result resolveSample(const Scene* scene, float time, const Vec2& position,
                            const Object* const* candidates, uint32_t count,
                            PixelSample& output, void* data) noexcept
{
    auto resolver = static_cast<SampleResolver*>(data);
    if (!resolver || !resolver->renderer) return Result::InvalidArguments;
    if (resolver->failure != Result::Success) return resolver->failure;
    return resolver->renderer->sample(scene, time, position, candidates, count, output,
                                      resolver->pixelRatio);
}

struct CallbackState
{
    uint32_t buttons = 0;
    uint32_t sliders = 0;
    float sliderValue = 0.0f;
    bool toggleValue = false;
    // Whether the control was still pressed when it last called back.
    bool pressed = false;
};

static void buttonCallback(Button* button, const InputEvent&, void* data) noexcept
{
    auto state = static_cast<CallbackState*>(data);
    state->buttons++;
    state->pressed = button->pressed();
}

static void toggleCallback(Button* button, const InputEvent&, void* data) noexcept
{
    auto state = static_cast<CallbackState*>(data);
    state->buttons++;
    state->toggleValue = button->toggled();
}

static void sliderCallback(Slider* slider, float value, const InputEvent&, void* data) noexcept
{
    auto state = static_cast<CallbackState*>(data);
    state->sliders++;
    state->sliderValue = value;
    state->pressed = slider->dragging();
}

struct PointerProbe final : UIObject
{
    explicit PointerProbe(const BBox& region) : UIObject(nullptr, region) {}

    InputResult input(const InputEvent& event) noexcept override
    {
        if (event.kind == InputKind::PointerDown) {
            return {true, true, true, false};
        }
        if (event.kind == InputKind::PointerMove) {
            moves++;
            position = event.position;
            delta = event.delta;
            return {true, true, false, false};
        }
        if (event.kind == InputKind::PointerCancel) cancels++;
        return {};
    }

    Vec2 position;
    Vec2 delta;
    uint32_t moves = 0;
    uint32_t cancels = 0;
};

static void pointerPosition()
{
    auto scene = Scene::gen();
    auto panel = Panel::gen(scene);
    auto probe = new PointerProbe({0.0f, 0.0f, 100.0f, 100.0f});
    CHECK(scene && panel && probe);
    if (!scene || !panel || !probe) {
        delete probe;
        delete scene;
        return;
    }
    CHECK(panel->add(probe) == Result::Success);
    auto event = pointer(InputKind::PointerMove, 20.0f, 30.0f);
    event.delta = {4.0f, -3.0f};
    auto result = panel->input(event);
    CHECK(result.handled && result.redraw && probe->moves == 1u);
    CHECK(_near(probe->position.x, 20.0f) && _near(probe->position.y, 30.0f));
    CHECK(_near(probe->delta.x, 4.0f) && _near(probe->delta.y, -3.0f));
    result = panel->input(pointer(InputKind::PointerMove, 120.0f, 30.0f));
    CHECK(!result.handled && result.redraw && probe->cancels == 1u);
    result = panel->input(pointer(InputKind::PointerDown, 20.0f, 30.0f));
    CHECK(result.handled && result.capture);
    result = panel->input(pointer(InputKind::PointerCancel, 20.0f, 30.0f));
    CHECK(result.handled && result.release && !result.capture && probe->cancels == 2u);
    delete scene;
}

static void controls()
{
    auto scene = Scene::gen();
    auto visual = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && visual);
    if (!scene || !visual) {
        delete scene;
        delete visual;
        return;
    }
    CHECK(scene->add(visual) == Result::Success);
    auto panel = Panel::gen(scene);
    CHECK(panel && panel->type() == Type::Group && panel->scene() == scene);
    CHECK(panel && scene->object(panel->id()) == panel);
    CHECK(Panel::gen(scene) == nullptr);
    if (!panel) {
        delete scene;
        return;
    }
    // A clone copies only groups of the scene's own kind, not a panel.
    Object* copy = nullptr;
    CHECK(scene->clone(panel, copy) == Result::NonSupport && !copy);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "a group made by another module"));

    CallbackState state;
    auto button = Button::gen(visual, {0.0f, 0.0f, 50.0f, 50.0f}, buttonCallback, &state);
    auto top = Button::gen(nullptr, {0.0f, 0.0f, 50.0f, 50.0f}, buttonCallback, &state);
    auto horizontal = Slider::gen(nullptr, {0.0f, 60.0f, 100.0f, 20.0f},
                                  SliderAxis::Horizontal, 0.0f, sliderCallback, &state);
    auto vertical = Slider::gen(nullptr, {120.0f, 0.0f, 20.0f, 100.0f},
                                SliderAxis::Vertical, 0.0f, sliderCallback, &state);
    CHECK(button && top && horizontal && vertical);
    if (!button || !top || !horizontal || !vertical) {
        delete button;
        delete top;
        delete horizontal;
        delete vertical;
        delete scene;
        return;
    }
    CHECK(button->visual() == visual);
    CHECK(button->visual(nullptr) == Result::Success && button->visual() == nullptr);
    CHECK(button->region({0.0f, 0.0f, -1.0f, 1.0f}) == Result::InvalidArguments);
    CHECK(panel->add(button) == Result::Success);
    CHECK(panel->add(top) == Result::Success);
    CHECK(panel->add(horizontal) == Result::Success);
    CHECK(panel->add(vertical) == Result::Success);
    CHECK(button->visual(visual) == Result::InvalidArguments && button->visual() == nullptr);
    CHECK(panel->count() == 4u && panel->controlAt(0u) == button
          && panel->controlAt(3u) == vertical && panel->controlAt(4u) == nullptr);
    CHECK(panel->add(button) == Result::InvalidArguments);

    // An event the panel cannot read is refused and moves nothing.
    auto nan = std::numeric_limits<float>::quiet_NaN();
    auto result = panel->input(pointer(InputKind::PointerMove, nan, 25.0f));
    CHECK(result.status == Result::InvalidArguments && !result.handled && !result.redraw);

    // Passive motion hovers only the last overlapping control and never activates it.
    result = panel->input(pointer(InputKind::PointerMove, 25.0f, 25.0f));
    CHECK(!result.handled && result.redraw && top->hovered() && !button->hovered());
    result = panel->input(pointer(InputKind::PointerMove, 25.0f, 25.0f));
    CHECK(!result.handled && !result.redraw && top->hovered());
    auto secondary = pointer(InputKind::PointerDown, 25.0f, 25.0f);
    secondary.button = 2;
    CHECK(!panel->input(secondary).handled);
    result = panel->input(pointer(InputKind::PointerMove, 80.0f, 25.0f));
    CHECK(!result.handled && result.redraw && !top->hovered());

    result = panel->input(pointer(InputKind::PointerDown, 25.0f, 25.0f));
    CHECK(result.handled && result.capture && result.redraw && top->pressed());
    result = panel->input(pointer(InputKind::PointerUp, 25.0f, 25.0f));
    CHECK(result.handled && result.release && top->hovered() && !top->pressed());
    CHECK(state.buttons == 1u && !state.pressed);

    // A disabled top control is transparent to hit testing.
    top->enabled(false);
    result = panel->input(pointer(InputKind::PointerDown, 25.0f, 25.0f, 2u));
    CHECK(result.capture && button->hovered() && button->pressed());
    CHECK(!panel->input(pointer(InputKind::PointerMove, 80.0f, 80.0f, 3u)).handled);
    result = panel->input(pointer(InputKind::PointerCancel, 80.0f, 80.0f, 2u));
    CHECK(result.handled && result.release && !button->hovered() && !button->pressed()
          && state.buttons == 1u);

    result = panel->input(pointer(InputKind::PointerDown, 25.0f, 25.0f, 4u));
    CHECK(result.capture);
    result = panel->input(pointer(InputKind::PointerMove, 80.0f, 80.0f, 4u));
    CHECK(result.handled && !result.release);
    result = panel->input(pointer(InputKind::PointerUp, 80.0f, 80.0f, 4u));
    CHECK(result.release && state.buttons == 1u);
    panel->input(pointer(InputKind::PointerDown, 25.0f, 25.0f, 5u));
    result = panel->input(pointer(InputKind::PointerUp, 25.0f, 25.0f, 5u));
    CHECK(result.release && state.buttons == 2u);

    // Disabling a captured control invalidates pressed/hover state even if it is
    // re-enabled before the host delivers the release event.
    panel->input(pointer(InputKind::PointerDown, 25.0f, 25.0f, 10u));
    CHECK(button->pressed() && button->hovered());
    button->enabled(false);
    button->enabled(true);
    CHECK(!button->pressed() && !button->hovered());
    result = panel->input(pointer(InputKind::PointerUp, 25.0f, 25.0f, 10u));
    CHECK(result.handled && result.release && !result.capture && state.buttons == 2u);

    panel->input(pointer(InputKind::PointerDown, 25.0f, 25.0f, 11u));
    CHECK(button->pressed());
    panel->enabled(false);
    panel->enabled(true);
    CHECK(!button->pressed() && !button->hovered());
    result = panel->input(pointer(InputKind::PointerUp, 25.0f, 25.0f, 11u));
    CHECK(result.handled && result.release && state.buttons == 2u);

    panel->input(pointer(InputKind::PointerMove, 25.0f, 25.0f, 12u));
    CHECK(button->hovered());
    panel->enabled(false);
    result = panel->input(pointer(InputKind::PointerMove, 25.0f, 25.0f, 5u));
    CHECK(result.redraw && !button->hovered());
    panel->enabled(true);

    // A slider calls back as the down, the moves and the up change its value; like a button's,
    // its press has ended when the up calls back.
    result = panel->input(pointer(InputKind::PointerDown, 25.0f, 70.0f, 6u));
    CHECK(result.capture && horizontal->dragging() && _near(horizontal->value(), 0.25f));
    CHECK(state.pressed);
    result = panel->input(pointer(InputKind::PointerMove, 160.0f, 70.0f, 6u));
    CHECK(result.handled && _near(horizontal->value(), 1.0f) && state.pressed);
    result = panel->input(pointer(InputKind::PointerUp, -20.0f, 70.0f, 6u));
    CHECK(result.release && !horizontal->dragging() && _near(horizontal->value(), 0.0f));
    CHECK(_near(state.sliderValue, 0.0f) && !state.pressed);

    result = panel->input(pointer(InputKind::PointerDown, 130.0f, 75.0f, 7u));
    CHECK(result.capture && vertical->dragging() && _near(vertical->value(), 0.25f));
    panel->input(pointer(InputKind::PointerMove, 130.0f, -20.0f, 7u));
    result = panel->input(pointer(InputKind::PointerUp, 130.0f, -20.0f, 7u));
    CHECK(result.release && _near(vertical->value(), 1.0f));
    CHECK(state.sliders >= 5u && _near(state.sliderValue, 1.0f));

    // Disabling a captured control releases it through the next event.
    horizontal->enabled(true);
    panel->input(pointer(InputKind::PointerDown, 20.0f, 70.0f, 8u));
    horizontal->enabled(false);
    result = panel->input(pointer(InputKind::PointerMove, 40.0f, 70.0f, 8u));
    CHECK(result.handled && result.release && !horizontal->dragging());

    delete scene;
}

static void limits()
{
    auto scene = Scene::gen();
    auto panel = Panel::gen(scene);
    CHECK(scene && panel);
    if (!scene || !panel) {
        delete scene;
        return;
    }
    for (auto i = 0u; i < 257u; i++) {
        auto control = Button::gen(nullptr, {static_cast<float>(i), 0.0f, 1.0f, 1.0f});
        CHECK(control != nullptr);
        if (!control) break;
        auto result = panel->add(control);
        if (i < 256u) {
            CHECK(result == Result::Success);
            CHECK(panel->bind(control, CameraAction::Reset) == Result::Success);
        } else {
            CHECK(result == Result::InsufficientCondition);
            delete control;
        }
    }
    CHECK(panel->count() == 256u);
    CHECK(panel->bind(static_cast<Button*>(panel->controlAt(0u)), CameraAction::Reset)
          == Result::InsufficientCondition);
    delete scene;
}

static void objectBindings(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto box = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && box && renderer);
    if (!scene || !box || !renderer) {
        delete scene;
        delete box;
        return;
    }
    box->fill(Color::hex("#ffffff"));
    CHECK(scene->add(box) == Result::Success);
    CHECK(scene->play(Animation::shift(box, {1.0f, 0.0f, 0.0f}), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    auto authoredModel = box->model;
    auto panel = Panel::gen(scene);
    auto slider = Slider::gen(nullptr, {0.0f, 0.0f, 100.0f, 20.0f});
    auto action = Button::gen(nullptr, {0.0f, 60.0f, 20.0f, 20.0f});
    CHECK(panel && slider && action);
    if (!panel || !slider || !action) {
        delete slider;
        delete action;
        delete scene;
        return;
    }
    CHECK(panel->add(slider) == Result::Success);
    CHECK(panel->add(action) == Result::Success);
    UITransform from;
    UITransform to;
    to.shift = {2.0f, 0.0f, 0.0f};
    CHECK(panel->bind(slider, box, from, to) == Result::Success);
    UITransform selected;
    selected.shift = {-1.0f, 0.0f, 0.0f};
    CHECK(panel->bind(action, box, selected) == Result::Success);
    CHECK(panel->bind(slider, panel, from, to) == Result::Success);

    BBox base;
    BBox runtime;
    BBox timeline;
    slider->value(0.0f);
    CHECK(renderer->bounds(scene, box, 0.0f, base) == Result::Success);
    slider->value(1.0f);
    CHECK(renderer->bounds(scene, box, 0.0f, runtime) == Result::Success);
    CHECK(_near(runtime.center().x - base.center().x, 50.0f, 0.1f));
    CHECK(renderer->bounds(scene, box, 1.0f, timeline) == Result::Success);
    CHECK(_near(timeline.center().x - runtime.center().x, 25.0f, 0.1f));
    for (auto i = 0u; i < 16u; i++) CHECK(_near(box->model.e[i], authoredModel.e[i]));

    CHECK(_near(box->opacity, 1.0f) && _near(box->progress, 1.0f));

    slider->value(0.0f);
    BBox actionBefore;
    BBox actionAfter;
    CHECK(renderer->bounds(scene, box, 0.0f, actionBefore) == Result::Success);
    panel->input(pointer(InputKind::PointerDown, 10.0f, 70.0f, 9u));
    panel->input(pointer(InputKind::PointerUp, 10.0f, 70.0f, 9u));
    CHECK(renderer->bounds(scene, box, 0.0f, actionAfter) == Result::Success);
    CHECK(_near(actionAfter.center().x - actionBefore.center().x, -25.0f, 0.1f));

    auto animated = Button::gen(nullptr, {30.0f, 60.0f, 20.0f, 20.0f});
    CHECK(animated && panel->add(animated) == Result::Success);
    UITransform animatedTarget;
    animatedTarget.shift = {1.0f, 0.0f, 0.0f};
    CHECK(panel->bind(animated, box, animatedTarget, 1.0f) == Result::Success);
    panel->input(pointer(InputKind::PointerDown, 40.0f, 70.0f, 10u, 0.0f));
    panel->input(pointer(InputKind::PointerUp, 40.0f, 70.0f, 10u, 0.0f));
    BBox transitionStart;
    BBox transitionMid;
    BBox transitionEnd;
    CHECK(renderer->bounds(scene, box, 0.0f, transitionStart) == Result::Success);
    CHECK(renderer->bounds(scene, box, 0.5f, transitionMid) == Result::Success);
    CHECK(renderer->bounds(scene, box, 1.0f, transitionEnd) == Result::Success);
    CHECK(_near(transitionStart.center().x, actionAfter.center().x, 0.1f));
    CHECK(transitionMid.center().x > transitionStart.center().x
          && transitionMid.center().x < transitionEnd.center().x);
    CHECK(panel->bind(animated, box, animatedTarget, -0.1f) == Result::InvalidArguments);
    // Opacity and progress end at one.
    UITransform over;
    over.opacity = 1.5f;
    CHECK(panel->bind(animated, box, over) == Result::InvalidArguments);
    over.opacity = 1.0f;
    over.progress = 2.0f;
    CHECK(panel->bind(slider, box, from, over) == Result::InvalidArguments);

    delete scene;
}

static void toggleBindings(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto target = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && target && renderer);
    if (!scene || !target || !renderer) {
        delete scene;
        delete target;
        return;
    }
    target->fill(Color::hex("#ffffff"));
    CHECK(scene->add(target) == Result::Success);
    CHECK(scene->wait(3.0f) == Result::Success);
    auto panel = Panel::gen(scene);
    CallbackState state;
    auto toggle = Button::gen(nullptr, {0.0f, 0.0f, 40.0f, 30.0f}, toggleCallback,
                              &state);
    CHECK(panel && toggle);
    if (!panel || !toggle) {
        delete toggle;
        delete scene;
        return;
    }
    CHECK(panel->add(toggle) == Result::Success);
    UITransform off;
    UITransform on;
    off.shift = {-1.0f, 0.0f, 0.0f};
    on.shift = {1.0f, 0.0f, 0.0f};
    CHECK(panel->toggle(toggle, target, off, on, false, 0.4f) == Result::Success);
    CHECK(!toggle->toggled());
    CHECK(panel->bind(toggle, CameraAction::Reset) == Result::InvalidArguments);
    CHECK(panel->bind(toggle, target, on) == Result::InvalidArguments);
    CHECK(panel->toggle(toggle, target, off, on) == Result::InvalidArguments);

    auto foreignScene = Scene::gen(config);
    auto foreign = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(foreignScene && foreign);
    if (foreignScene && foreign) CHECK(foreignScene->add(foreign) == Result::Success);
    auto invalid = Button::gen(nullptr, {50.0f, 0.0f, 40.0f, 30.0f});
    CHECK(invalid && panel->add(invalid) == Result::Success);
    if (invalid) {
        CHECK(panel->toggle(invalid, target, off, on, false, -0.1f)
              == Result::InvalidArguments);
        CHECK(panel->toggle(invalid, foreign, off, on) == Result::InvalidArguments);
        CHECK(panel->bind(invalid, CameraAction::Reset) == Result::Success);
        CHECK(panel->toggle(invalid, target, off, on) == Result::InvalidArguments);
    }
    delete foreignScene;

    BBox initial;
    CHECK(renderer->bounds(scene, target, 0.0f, initial) == Result::Success);
    CHECK(_near(initial.center().x, 75.0f, 0.1f));

    panel->input(pointer(InputKind::PointerDown, 20.0f, 15.0f, 2u, 0.2f));
    panel->input(pointer(InputKind::PointerUp, 80.0f, 15.0f, 2u, 0.2f));
    CHECK(!toggle->toggled() && state.buttons == 0u);
    panel->input(pointer(InputKind::PointerDown, 20.0f, 15.0f, 3u, 0.3f));
    panel->input(pointer(InputKind::PointerCancel, 20.0f, 15.0f, 3u, 0.3f));
    CHECK(!toggle->toggled() && state.buttons == 0u);

    panel->input(pointer(InputKind::PointerDown, 20.0f, 15.0f, 4u, 0.5f));
    panel->input(pointer(InputKind::PointerUp, 20.0f, 15.0f, 4u, 0.5f));
    CHECK(toggle->toggled() && state.buttons == 1u && state.toggleValue);
    BBox start;
    BBox middle;
    BBox end;
    CHECK(renderer->bounds(scene, target, 0.5f, start) == Result::Success);
    CHECK(renderer->bounds(scene, target, 0.7f, middle) == Result::Success);
    CHECK(renderer->bounds(scene, target, 0.9f, end) == Result::Success);
    CHECK(_near(start.center().x, 75.0f, 0.1f));
    CHECK(_near(middle.center().x, 100.0f, 0.1f));
    CHECK(_near(end.center().x, 125.0f, 0.1f));
    BBox resampledMiddle;
    CHECK(renderer->bounds(scene, target, 0.7f, resampledMiddle) == Result::Success);
    CHECK(_near(resampledMiddle.center().x, middle.center().x, 0.1f));

    panel->input(pointer(InputKind::PointerDown, 20.0f, 15.0f, 5u, 1.0f));
    panel->input(pointer(InputKind::PointerUp, 20.0f, 15.0f, 5u, 1.0f));
    CHECK(!toggle->toggled() && state.buttons == 2u && !state.toggleValue);
    CHECK(renderer->bounds(scene, target, 1.4f, end) == Result::Success);
    CHECK(_near(end.center().x, 75.0f, 0.1f));

    panel->input(pointer(InputKind::PointerDown, 20.0f, 15.0f, 6u, 1.5f));
    panel->input(pointer(InputKind::PointerUp, 20.0f, 15.0f, 6u, 1.5f));
    BBox reversalBefore;
    BBox reversalStart;
    BBox reversalMiddle;
    BBox reversalEnd;
    CHECK(renderer->bounds(scene, target, 1.7f, reversalBefore) == Result::Success);
    panel->input(pointer(InputKind::PointerDown, 20.0f, 15.0f, 7u, 1.7f));
    panel->input(pointer(InputKind::PointerUp, 20.0f, 15.0f, 7u, 1.7f));
    CHECK(!toggle->toggled() && state.buttons == 4u && !state.toggleValue);
    CHECK(renderer->bounds(scene, target, 1.7f, reversalStart) == Result::Success);
    CHECK(renderer->bounds(scene, target, 1.9f, reversalMiddle) == Result::Success);
    CHECK(renderer->bounds(scene, target, 2.1f, reversalEnd) == Result::Success);
    CHECK(_near(reversalStart.center().x, reversalBefore.center().x, 0.1f));
    CHECK(reversalMiddle.center().x < reversalStart.center().x
          && reversalMiddle.center().x > reversalEnd.center().x);
    CHECK(_near(reversalEnd.center().x, 75.0f, 0.1f));
    BBox wrapped;
    CHECK(renderer->bounds(scene, target, 0.1f, wrapped) == Result::Success);
    CHECK(_near(wrapped.center().x, reversalEnd.center().x, 0.1f));
    delete scene;
}

static void buttonStates(Renderer* renderer)
{
    Config config;
    config.width = 100;
    config.height = 100;
    config.camera.orthoHeight = 2.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto normal = Rectangle::gen({}, {1.2f, 1.2f});
    auto hover = Rectangle::gen({}, {1.2f, 1.2f});
    auto pressed = Rectangle::gen({}, {1.2f, 1.2f});
    CHECK(scene && normal && hover && pressed && renderer);
    if (!scene || !normal || !hover || !pressed || !renderer) {
        delete scene;
        delete normal;
        delete hover;
        delete pressed;
        return;
    }
    normal->fill(Color::hex("#e5484d"));
    hover->fill(Color::hex("#30a46c"));
    pressed->fill(Color::hex("#3e63dd"));
    CHECK(scene->add(normal) == Result::Success);
    CHECK(scene->add(hover) == Result::Success);
    CHECK(scene->add(pressed) == Result::Success);
    auto panel = Panel::gen(scene);
    auto button = Button::gen(normal, {0.0f, 0.0f, 50.0f, 50.0f});
    CHECK(panel && button);
    if (!panel || !button) {
        delete button;
        delete scene;
        return;
    }
    CHECK(panel->add(button) == Result::Success);
    CHECK(panel->states(button, normal, pressed) == Result::InvalidArguments);
    CHECK(panel->states(button, hover, pressed) == Result::Success);

    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    auto center = 50u * surface.stride() + 50u;
    CHECK(surface.data() && surface.data()[center] == pixel(Color::hex("#e5484d")));

    auto result = panel->input(pointer(InputKind::PointerMove, 10.0f, 10.0f));
    CHECK(result.redraw && button->hovered() && !button->pressed());
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data() && surface.data()[center] == pixel(Color::hex("#30a46c")));

    result = panel->input(pointer(InputKind::PointerDown, 10.0f, 10.0f));
    CHECK(result.capture && button->hovered() && button->pressed());
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data() && surface.data()[center] == pixel(Color::hex("#3e63dd")));

    panel->input(pointer(InputKind::PointerMove, 80.0f, 80.0f));
    CHECK(!button->hovered() && button->pressed());
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data() && surface.data()[center] == pixel(Color::hex("#e5484d")));

    panel->input(pointer(InputKind::PointerMove, 10.0f, 10.0f));
    panel->input(pointer(InputKind::PointerUp, 10.0f, 10.0f));
    CHECK(button->hovered() && !button->pressed());
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data() && surface.data()[center] == pixel(Color::hex("#30a46c")));

    panel->input(pointer(InputKind::PointerCancel, 10.0f, 10.0f));
    CHECK(!button->hovered());
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data() && surface.data()[center] == pixel(Color::hex("#e5484d")));

    panel->input(pointer(InputKind::PointerDown, 10.0f, 10.0f, 2u));
    panel->enabled(false);
    panel->enabled(true);
    CHECK(!button->hovered() && !button->pressed());
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data() && surface.data()[center] == pixel(Color::hex("#e5484d")));
    result = panel->input(pointer(InputKind::PointerUp, 10.0f, 10.0f, 2u));
    CHECK(result.handled && result.release && !result.capture);

    // State faces are Panel-exclusive and cannot be nested because an inactive
    // ancestor or a second binding would mask the active face.
    auto reused = Button::gen(hover, {60.0f, 0.0f, 20.0f, 20.0f});
    CHECK(reused && panel->add(reused) == Result::InvalidArguments);
    delete reused;

    auto normal2 = Rectangle::gen({2.0f, 0.0f, 0.0f}, {0.2f, 0.2f});
    auto hover2 = Rectangle::gen({2.0f, 0.0f, 0.0f}, {0.2f, 0.2f});
    CHECK(normal2 && hover2);
    if (normal2 && hover2) {
        CHECK(scene->add(normal2) == Result::Success);
        CHECK(scene->add(hover2) == Result::Success);
        auto second = Button::gen(normal2, {60.0f, 30.0f, 20.0f, 20.0f});
        CHECK(second && panel->add(second) == Result::Success);
        if (second) {
            CHECK(panel->states(second, hover) == Result::InvalidArguments);
            CHECK(panel->states(second, hover2) == Result::Success);
        }
    } else {
        delete normal2;
        delete hover2;
    }

    auto parent = Group::gen();
    auto child = Rectangle::gen({3.0f, 0.0f, 0.0f}, {0.2f, 0.2f});
    CHECK(parent && child);
    if (parent && child) {
        CHECK(parent->add(child) == Result::Success);
        CHECK(scene->add(parent) == Result::Success);
        auto nested = Button::gen(parent, {60.0f, 60.0f, 20.0f, 20.0f});
        CHECK(nested && panel->add(nested) == Result::Success);
        if (nested) CHECK(panel->states(nested, child) == Result::InvalidArguments);
    } else {
        delete parent;
        delete child;
    }
    delete scene;
}

static void sampleAreas(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto gradient = Rectangle::gen({}, {4.0f, 2.0f});
    auto overlay = Rectangle::gen({}, {1.0f, 1.0f});
    auto marker = Rectangle::gen({}, {0.12f, 0.12f});
    auto swatch = Rectangle::gen({3.2f, 1.45f, 0.0f}, {1.0f, 0.5f});
    CHECK(scene && gradient && overlay && marker && swatch && renderer);
    if (!scene || !gradient || !overlay || !marker || !swatch || !renderer) {
        delete scene;
        delete gradient;
        delete overlay;
        delete marker;
        delete swatch;
        return;
    }
    gradient->fill(Color::hex("#ff0000"));
    gradient->style.gradient.type = GradientType::Linear;
    gradient->style.gradientEnd = Color::hex("#0000ff");
    overlay->fill(Color::hex("#30a46c"));
    overlay->layer = 1;
    marker->fill(Color::hex("#ffffff"));
    marker->layer = 2;
    swatch->fill(Color::hex("#111111"));
    auto authoredMarker = marker->model;
    auto authoredSwatch = swatch->style.fill;
    CHECK(scene->add(gradient) == Result::Success);
    CHECK(scene->add(overlay) == Result::Success);
    CHECK(scene->add(marker) == Result::Success);
    CHECK(scene->add(swatch) == Result::Success);
    auto panel = Panel::gen(scene);
    const Object* targets[] = {gradient, overlay};
    auto area = SampleArea::gen(targets, 2u, {0.0f, 0.0f, 200.0f, 100.0f});
    CHECK(panel && area);
    if (!panel || !area) {
        delete area;
        delete scene;
        return;
    }
    CHECK(panel->add(area) == Result::Success);
    SampleBinding binding;
    binding.marker = marker;
    binding.swatch = swatch;
    binding.markerOrigin = {-4.0f, 2.0f, 0.0f};
    binding.markerX = {8.0f, 0.0f, 0.0f};
    binding.markerY = {0.0f, -4.0f, 0.0f};
    CHECK(panel->bind(area, binding) == Result::Success);
    SampleResolver resolver{renderer, 1u};
    CHECK(panel->sampler(resolveSample, &resolver) == Result::Success);

    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    BBox markerBounds;
    CHECK(renderer->bounds(scene, marker, 0.0f, markerBounds)
          == Result::InsufficientCondition);

    auto result = panel->input(pointer(InputKind::PointerDown, 100.0f, 50.0f, 20u));
    CHECK(result.handled && result.capture && area->pressed() && !result.sample);
    result = panel->input(pointer(InputKind::PointerUp, 100.0f, 50.0f, 20u));
    CHECK(result.handled && result.release && result.redraw && result.sample == area);
    CHECK(area->selected() && area->selection().object == overlay);
    CHECK(area->selection().color.r == 0x30 && area->selection().color.g == 0xa4
          && area->selection().color.b == 0x6c && area->selection().color.a == 0xff);
    CHECK(_near(area->value().x, 0.5f) && _near(area->value().y, 0.5f));
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    BBox swatchBounds;
    CHECK(renderer->bounds(scene, swatch, 0.0f, swatchBounds) == Result::Success);
    auto swatchX = static_cast<uint32_t>(swatchBounds.center().x);
    auto swatchY = static_cast<uint32_t>(swatchBounds.center().y);
    CHECK(surface.data()[swatchY * surface.stride() + swatchX]
          == pixel(area->selection().color));

    result = panel->input(pointer(InputKind::PointerDown, 60.0f, 50.0f, 21u));
    CHECK(result.capture);
    result = panel->input(pointer(InputKind::PointerUp, 60.0f, 50.0f, 21u));
    CHECK(result.sample == area && area->selection().object == gradient);
    auto gradientColor = area->selection().color;
    CHECK(gradientColor.a == 0xff && gradientColor.r > gradientColor.b);
    CHECK(_near(area->value().x, 0.3f) && _near(area->value().y, 0.5f));
    CHECK(renderer->bounds(scene, marker, 0.0f, markerBounds) == Result::Success);
    CHECK(_near(markerBounds.center().x, 60.0f, 0.2f)
          && _near(markerBounds.center().y, 50.0f, 0.2f));

    // The selected marker now overlaps the source, but target-isolated sampling
    // must return the same source pixel on a repeated click.
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    panel->input(pointer(InputKind::PointerDown, 60.0f, 50.0f, 22u));
    result = panel->input(pointer(InputKind::PointerUp, 60.0f, 50.0f, 22u));
    CHECK(result.sample == area && area->selection().object == gradient);
    CHECK(area->selection().color.r == gradientColor.r
          && area->selection().color.g == gradientColor.g
          && area->selection().color.b == gradientColor.b
          && area->selection().color.a == gradientColor.a);

    // A transparent miss completes the click but preserves the last selection.
    panel->input(pointer(InputKind::PointerDown, 10.0f, 90.0f, 23u));
    result = panel->input(pointer(InputKind::PointerUp, 10.0f, 90.0f, 23u));
    CHECK(result.handled && result.release && !result.sample);
    CHECK(area->selection().object == gradient
          && _near(area->position().x, 60.0f) && _near(area->position().y, 50.0f));

    resolver.failure = Result::Unknown;
    panel->input(pointer(InputKind::PointerDown, 60.0f, 50.0f, 24u));
    result = panel->input(pointer(InputKind::PointerUp, 60.0f, 50.0f, 24u));
    CHECK(result.handled && result.release && !result.sample
          && result.status == Result::Unknown);
    resolver.failure = Result::Success;

    for (auto i = 0u; i < 16u; i++) CHECK(_near(marker->model.e[i], authoredMarker.e[i]));
    CHECK(swatch->style.fill.r == authoredSwatch.r && swatch->style.fill.g == authoredSwatch.g
          && swatch->style.fill.b == authoredSwatch.b && swatch->style.fill.a == authoredSwatch.a);

    // Sample faces stay exclusive even when another binding is registered later.
    auto normal = Rectangle::gen({3.5f, 0.0f, 0.0f}, {0.2f, 0.2f});
    auto laterButton = Button::gen(normal, {170.0f, 0.0f, 20.0f, 20.0f});
    auto laterSlider = Slider::gen(nullptr, {170.0f, 25.0f, 20.0f, 20.0f});
    CHECK(normal && laterButton && laterSlider);
    if (normal && laterButton && laterSlider) {
        CHECK(scene->add(normal) == Result::Success);
        CHECK(panel->add(laterButton) == Result::Success);
        CHECK(panel->add(laterSlider) == Result::Success);
        UITransform transform;
        CHECK(panel->bind(laterButton, marker, transform) == Result::InvalidArguments);
        CHECK(panel->bind(laterSlider, swatch, transform, transform)
              == Result::InvalidArguments);
        CHECK(panel->states(laterButton, marker) == Result::InvalidArguments);
    } else {
        delete normal;
        delete laterButton;
        delete laterSlider;
    }

    auto reusedTarget = Button::gen(gradient, {170.0f, 50.0f, 20.0f, 20.0f});
    CHECK(reusedTarget && panel->add(reusedTarget) == Result::InvalidArguments);
    delete reusedTarget;

    const Object* visualTargets[] = {normal};
    auto visualArea = SampleArea::gen(visualTargets, 1u,
                                      {0.0f, 0.0f, 200.0f, 100.0f});
    CHECK(visualArea && panel->add(visualArea) == Result::Success);
    if (visualArea) {
        SampleBinding visualBinding;
        CHECK(panel->bind(visualArea, visualBinding) == Result::InvalidArguments);
    }

    // Panel-owned UI families are never valid marker or swatch faces.
    auto panelChild = Rectangle::gen({3.7f, 0.0f, 0.0f}, {0.1f, 0.1f});
    auto invalidMarker = Rectangle::gen({3.8f, 0.0f, 0.0f}, {0.1f, 0.1f});
    auto invalidMarkerChild = Rectangle::gen({}, {0.05f, 0.05f});
    const Object* invalidTargets[] = {gradient};
    auto invalidArea = SampleArea::gen(invalidTargets, 1u,
                                       {0.0f, 0.0f, 200.0f, 100.0f});
    CHECK(panelChild && invalidMarker && invalidMarkerChild && invalidArea);
    if (panelChild && invalidMarker && invalidMarkerChild && invalidArea) {
        CHECK(panel->Group::add(panelChild) == Result::Success);
        CHECK(invalidMarker->add(invalidMarkerChild) == Result::Success);
        CHECK(scene->add(invalidMarker) == Result::Success);
        CHECK(panel->add(invalidArea) == Result::Success);
        SampleBinding invalid;
        invalid.marker = panel;
        CHECK(panel->bind(invalidArea, invalid) == Result::InvalidArguments);
        invalid.marker = invalidArea;
        CHECK(panel->bind(invalidArea, invalid) == Result::InvalidArguments);
        invalid.marker = nullptr;
        invalid.swatch = panelChild;
        CHECK(panel->bind(invalidArea, invalid) == Result::InvalidArguments);
        invalid.swatch = nullptr;
        invalid.marker = invalidMarker;
        CHECK(panel->bind(invalidArea, invalid) == Result::InvalidArguments);
    } else {
        delete panelChild;
        delete invalidMarkerChild;
        delete invalidMarker;
        delete invalidArea;
    }
    delete scene;
}

static void cameraBindings(Renderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto box = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && box && renderer);
    if (!scene || !box || !renderer) {
        delete scene;
        delete box;
        return;
    }
    box->fill(Color::hex("#ffffff"));
    CHECK(scene->add(box) == Result::Success);
    auto camera = config.camera;
    camera.eye.x = 1.0f;
    camera.target.x = 1.0f;
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    auto panel = Panel::gen(scene);
    auto view = Button::gen(nullptr, {0.0f, 0.0f, 20.0f, 20.0f});
    auto reset = Button::gen(nullptr, {30.0f, 0.0f, 20.0f, 20.0f});
    CHECK(panel && view && reset);
    if (!panel || !view || !reset) {
        delete view;
        delete reset;
        delete scene;
        return;
    }
    CHECK(panel->add(view) == Result::Success);
    CHECK(panel->add(reset) == Result::Success);
    CHECK(panel->bind(view, CameraAction::View3D) == Result::Success);
    CHECK(panel->bind(reset, CameraAction::Reset) == Result::Success);

    BBox timeline;
    BBox moved;
    BBox restored;
    CHECK(renderer->bounds(scene, box, 0.5f, timeline) == Result::Success);
    CHECK(panel->camera(CameraAction::Pan, {0.1f, 0.0f}) == Result::Success);
    CHECK(renderer->bounds(scene, box, 0.5f, moved) == Result::Success);
    CHECK(std::fabs(moved.center().x - timeline.center().x) > 1.0f);
    CHECK(panel->camera(CameraAction::Reset) == Result::Success);
    CHECK(renderer->bounds(scene, box, 0.5f, restored) == Result::Success);
    CHECK(_near(restored.center().x, timeline.center().x, 0.1f));

    panel->input(pointer(InputKind::PointerDown, 10.0f, 10.0f, 1u, 0.5f));
    panel->input(pointer(InputKind::PointerUp, 10.0f, 10.0f, 1u, 0.5f));
    CHECK(scene->view(0.5f) == CameraView::ThreeD);
    panel->input(pointer(InputKind::PointerDown, 40.0f, 10.0f, 2u, 0.5f));
    panel->input(pointer(InputKind::PointerUp, 40.0f, 10.0f, 2u, 0.5f));
    CHECK(scene->view(0.5f) == CameraView::TwoD);

    delete scene;

    Config inputConfig;
    inputConfig.cameraMode = CameraMode::Interactive;
    auto interactive = Scene::gen(inputConfig);
    CHECK(interactive != nullptr);
    if (interactive) {
        CHECK(interactive->cameraInput({CameraAction::View3D, {}}) == Result::NonSupport);
        auto inputPanel = Panel::gen(interactive);
        CHECK(inputPanel != nullptr);
        CHECK(interactive->cameraInput({CameraAction::View3D, {}}) == Result::Success);
        CHECK(interactive->view(0.0f) == CameraView::ThreeD);
        CHECK(interactive->cameraInput({CameraAction::Orbit, {0.1f, -0.05f}})
              == Result::Success);
    }
    delete interactive;

    // Installing an idle Panel must preserve the core's scaled camera-validity domain.
    Config extremeConfig;
    extremeConfig.width = 32;
    extremeConfig.height = 32;
    extremeConfig.cameraView = CameraView::ThreeD;
    extremeConfig.camera.projection = Projection::Perspective;
    auto limit = std::numeric_limits<float>::max();
    extremeConfig.camera.eye = {limit, 0.0f, 0.0f};
    extremeConfig.camera.target = {-limit, 0.0f, 0.0f};
    auto extreme = Scene::gen(extremeConfig);
    CHECK(extreme != nullptr);
    if (extreme) {
        CHECK(Panel::gen(extreme) != nullptr);
        Surface surface;
        CHECK(renderer->render(extreme, 0.0f, surface) == Result::Success);
    }
    delete extreme;
}

static void luaBindings()
{
    static constexpr const char source[] = R"(
local scene = vis.scene {
    width = 200,
    height = 100,
    camera = { mode = "interactive", view = "2d", target = { 0, 0 }, height = 4 },
}
local visual = scene:rectangle { center = { 0, 0 }, size = { 1, 1 } }
local hover_visual = scene:rectangle { center = { 0, 0 }, size = { 1, 1 }, fill = "accent" }
local pressed_visual = scene:rectangle { center = { 0, 0 }, size = { 1, 1 }, fill = "secondary" }
local target = scene:rectangle { center = { 1, 0 }, size = { 1, 1 } }
local action_visual = scene:rectangle { center = { 2, 0 }, size = { 1, 1 } }
local toggle_visual = scene:rectangle { center = { 2, -1 }, size = { 1, 0.5 } }
local toggle_target = scene:circle { center = { -1, 1 }, radius = 0.1 }
local slider_visual = scene:line { from = { -1, -1 }, to = { 1, -1 } }
local slider_handle = scene:circle { center = { -1, -1 }, radius = 0.1 }
local sample_marker = scene:circle { center = { -2, 1 }, radius = 0.1 }
local sample_swatch = scene:rectangle { center = { 3, 1 }, size = { 0.5, 0.5 } }
local panel = vis.ui.panel(scene)
panel:button {
    visual = action_visual,
    hover_visual = hover_visual,
    pressed_visual = pressed_visual,
    region = { x = 0, y = 0, width = 20, height = 20 },
    camera = "reset",
}
panel:button {
    visual = visual,
    region = { 120, 0, 20, 20 },
    target = target,
    transform = { shift = { -1, 0, 0 } },
    duration = 0.45,
}
panel:toggle_button {
    visual = toggle_visual,
    region = { 120, 25, 20, 20 },
    target = toggle_target,
    off = { shift = { -1, 0, 0 } },
    on = { shift = { 1, 0, 0 } },
    value = false,
    duration = 0.3,
}
panel:slider {
    visual = slider_visual,
    region = { 0, 30, 100, 20 },
    bindings = {
        {
            target = target,
            from = { scale = { 0.5, 0.5, 1 } },
            to = { scale = { 2, 2, 1 } },
        },
        {
            target = slider_handle,
            from = {},
            to = { shift = { 2, 0, 0 } },
        },
    },
}
panel:sample_area {
    region = { 150, 60, 40, 30 },
    targets = { target },
    marker = sample_marker,
    marker_origin = { -1, 1, 0 },
    marker_x = { 2, 0, 0 },
    marker_y = { 0, -2, 0 },
    swatch = sample_swatch,
}
return scene
    )";
    detail::LuaHostContext context;
    auto scene = _loaded(source, "ui-test.lua", context);
    auto panel = scene ? context.ui.panel : nullptr;
    CHECK(panel && panel->scene() == scene && panel->count() == 5u);
    if (scene && panel) {
        auto button = static_cast<Button*>(panel->controlAt(0u));
        auto toggle = static_cast<Button*>(panel->controlAt(2u));
        auto slider = static_cast<Slider*>(panel->controlAt(3u));
        auto sample = static_cast<SampleArea*>(panel->controlAt(4u));
        CHECK(sample && sample->targetCount() == 1u && !sample->selected());
        panel->input(pointer(InputKind::PointerMove, 5.0f, 5.0f));
        CHECK(button && button->hovered());
        panel->input(pointer(InputKind::PointerMove, 25.0f, 5.0f));
        CHECK(button && !button->hovered());
        CHECK(toggle && !toggle->toggled());
        panel->input(pointer(InputKind::PointerDown, 130.0f, 35.0f, 3u));
        panel->input(pointer(InputKind::PointerUp, 130.0f, 35.0f, 3u));
        CHECK(toggle && toggle->toggled());
        panel->input(pointer(InputKind::PointerDown, 75.0f, 35.0f, 2u));
        panel->input(pointer(InputKind::PointerUp, 75.0f, 35.0f, 2u));
        CHECK(slider && _near(slider->value(), 0.75f));
    }
    delete scene;

    static constexpr const char mismatchedSource[] = R"(
local abandoned = vis.scene { width = 32, height = 32 }
vis.ui.panel(abandoned)
abandoned = nil
for index = 1, 50000 do
    local garbage = { index, index + 1, index + 2, index + 3 }
end
return vis.scene { width = 32, height = 32 }
    )";
    char error[512] = {};
    context = {};
    CHECK(_load(mismatchedSource, "ui-mismatched-scene.lua", scene, error, sizeof(error), context)
              == Result::InvalidArguments
          && !scene);

    static constexpr const char samplingSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local space = scene:space { x = { 0, 0, 1 }, y = { 0, 0, 1 } }
space:cell(function()
    vis.ui.panel(scene)
    return "#ffffff"
end)
return scene
    )";
    _scriptError(samplingSource, "ui-sampling.lua", "sampling callback");

    static constexpr const char limitSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local visual = scene:rectangle { size = { 1, 1 } }
local panel = vis.ui.panel(scene)
for index = 1, 257 do
    panel:button {
        visual = visual,
        region = { index, 0, 1, 1 },
        camera = "reset",
    }
end
return scene
    )";
    _scriptError(limitSource, "ui-limit.lua", "InsufficientCondition");

    static constexpr const char typoSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local visual = scene:rectangle { size = { 1, 1 } }
local panel = vis.ui.panel(scene)
panel:button {
    visual = visual,
    region = { 0, 0, 1, 1 },
    target = visual,
    tranform = { opacity = 0.5 },
}
return scene
    )";
    _scriptError(typoSource, "ui-typo.lua", "unknown UI config field 'tranform'");

    static constexpr const char mixedSliderSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local visual = scene:line { from = { -1, 0 }, to = { 1, 0 } }
local target = scene:circle { radius = 0.1 }
local panel = vis.ui.panel(scene)
panel:slider {
    visual = visual,
    region = { 0, 0, 20, 10 },
    target = target,
    bindings = {{ target = target }},
}
return scene
    )";
    _scriptError(mixedSliderSource, "ui-mixed-slider.lua",
                 "slider target/from/to cannot be combined with bindings");

    static constexpr const char toggleTypeSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local visual = scene:rectangle { size = { 1, 1 } }
local target = scene:circle { radius = 0.1 }
local panel = vis.ui.panel(scene)
panel:toggle_button {
    visual = visual,
    region = { 0, 0, 1, 1 },
    target = target,
    off = {},
    on = { shift = { 1, 0, 0 } },
    value = "on",
}
return scene
    )";
    _scriptError(toggleTypeSource, "ui-toggle-type.lua", "value must be a boolean");

    static constexpr const char duplicateStateSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local visual = scene:rectangle { size = { 1, 1 } }
local panel = vis.ui.panel(scene)
panel:button {
    visual = visual,
    hover_visual = visual,
    region = { 0, 0, 1, 1 },
    camera = "reset",
}
return scene
    )";
    _scriptError(duplicateStateSource, "ui-duplicate-state.lua", "InvalidArguments");
}

// A list of slider bindings or sample targets out of range stops the script with its limit,
// at the line of the call.
static void luaLimits()
{
    static constexpr const char sliderSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local visual = scene:line { from = { -1, 0 }, to = { 1, 0 } }
local panel = vis.ui.panel(scene)
panel:slider { visual = visual, region = { 0, 0, 20, 10 }, bindings = {} }
return scene
    )";
    _scriptError(sliderSource, "ui-slider-bindings.lua",
                 "ui-slider-bindings.lua:5: slider bindings must contain between 1 and 256 items");

    static constexpr const char sampleSource[] = R"(
local scene = vis.scene { width = 32, height = 32 }
local panel = vis.ui.panel(scene)
panel:sample_area { region = { 0, 0, 20, 10 }, targets = {} }
return scene
    )";
    _scriptError(sampleSource, "ui-sample-targets.lua",
                 "ui-sample-targets.lua:4: sample_area targets must contain between 1 and 64 objects");
}

// A value that must be a table and is not stops the script with the name of the option.
static void luaTables()
{
    static constexpr const char* cases[][2] = {
        {"panel:button { visual = visual, region = region, camera = 'move', delta = 'up' }",
         "delta must be a table, got string"},
        {"panel:button { visual = visual, region = region, target = visual, transform = 1 }",
         "transform must be a table, got number"},
        {"panel:slider { visual = visual, region = region, bindings = 'all' }",
         "slider bindings must be a table, got string"},
        {"panel:slider { visual = visual, region = region, bindings = { 1 } }",
         "a slider binding must be a table, got number"},
        {"panel:sample_area { region = region }", "sample_area targets must be a table, got nil"},
    };
    for (auto& entry : cases) {
        char source[512];
        std::snprintf(source, sizeof(source), R"(
local scene = vis.scene { width = 32, height = 32 }
local visual = scene:rectangle { size = { 1, 1 } }
local region = { 0, 0, 1, 1 }
local panel = vis.ui.panel(scene)
%s
return scene
        )", entry[0]);
        _scriptError(source, "ui-table.lua", entry[1]);
    }
}

int main()
{
    pointerPosition();
    controls();
    limits();
    luaBindings();
    luaLimits();
    luaTables();
    auto renderer = Renderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        objectBindings(renderer);
        toggleBindings(renderer);
        buttonStates(renderer);
        sampleAreas(renderer);
        cameraBindings(renderer);
    }
    delete renderer;
    return report();
}
