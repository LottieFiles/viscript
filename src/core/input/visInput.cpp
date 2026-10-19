// The input module's controller: pointer follows, key moves and triggers, and camera actions,
// applied to a scene through runtime modifiers.
#include <cmath>
#include <new>

#include "vis_input.h"
#include "visInputEvent.h"
#include "../common/visControl.h"
#include "../common/visFinite.h"
#include "../common/visGrow.h"
#include "../common/visRetarget.h"

namespace vis::input
{

namespace
{

constexpr uint32_t ShortcutModifiers = 2u | 4u | 8u;
uint8_t RuntimeKey = 0u;

// The angle a whole number of turns from target that lies nearest current, so that a turn takes
// the short way. Target comes within half a turn of zero first, which keeps its distance from
// current exact in double however large it is, so that any finite target settles on one angle.
static float _angle(float current, float target) noexcept
{
    auto turn = static_cast<double>(TWO_PI);
    auto angle = std::remainder(static_cast<double>(target), turn);
    return static_cast<float>(current + std::remainder(angle - current, turn));
}

static float _keyAmount(float elapsed, float period) noexcept
{
    if (period == 0.0f) return 1.0f;
    auto amount = elapsed / period;
    if (amount >= 1.0f) return amount;
    return amount * amount * (2.0f - amount);
}

}  // namespace

struct Controller::Impl
{
    struct Motion
    {
        Vec3 shift;
        Vec3 rotation;
    };

    struct State
    {
        Object* object = nullptr;
        Vec3 origin;
        detail::Retarget<Motion> motion;
    };

    struct PointerBinding
    {
        PointerFollow config;
        uint32_t state = 0u;
        bool hovered = false;
    };

    struct KeyBinding
    {
        KeyMove config;
        uint32_t state = 0u;
        float sampled = 0.0f;
        float held = 0.0f;
    };

    Scene* scene = nullptr;
    State* states = nullptr;
    PointerBinding* pointers = nullptr;
    KeyBinding* keys = nullptr;
    KeyTriggerBinding* triggers = nullptr;
    vis::input::State* inputState = nullptr;
    uint32_t stateCount = 0u;
    uint32_t stateCapacity = 0u;
    uint32_t pointerCount = 0u;
    uint32_t pointerCapacity = 0u;
    uint32_t keyCount = 0u;
    uint32_t keyCapacity = 0u;
    uint32_t triggerCount = 0u;
    uint32_t triggerCapacity = 0u;
    CameraDrive camera;

    ~Impl()
    {
        delete[] states;
        delete[] pointers;
        delete[] keys;
        delete[] triggers;
        delete inputState;
    }

    uint32_t find(const Object* object) const noexcept
    {
        for (auto i = 0u; i < stateCount; i++) {
            if (states[i].object == object) return i;
        }
        return UINT32_MAX;
    }

    // Checks what a pointer follow and a key move share: a target of the scene's own apart from
    // the controller, a period, room under the binding limit and no pointer follow on target.
    Result admit(const Object* target, float period, const Object* controller) const noexcept
    {
        if (!scene || !target || !std::isfinite(period) || period < 0.0f
            || scene->object(target->id()) != target || family(target, controller)) {
            return Result::InvalidArguments;
        }
        if (pointerCount + keyCount + triggerCount >= BindingLimit) {
            return Result::InsufficientCondition;
        }
        for (auto i = 0u; i < pointerCount; i++) {
            if (pointers[i].config.target == target) return Result::InsufficientCondition;
        }
        return Result::Success;
    }

    // The state that moves target, added at origin for a target with none; UINT32_MAX when the
    // states cannot grow.
    uint32_t track(Object* target, const Vec3& origin) noexcept
    {
        auto index = find(target);
        if (index != UINT32_MAX) return index;
        if (!grow(states, stateCount, stateCapacity, 1u, 8u)) return UINT32_MAX;
        states[stateCount].object = target;
        states[stateCount].origin = origin;
        return stateCount++;
    }

    float elapsed(float time, float started) const noexcept
    {
        auto value = time - started;
        if (value < 0.0f && scene && scene->duration() > 0.0f) value += scene->duration();
        return std::fmax(0.0f, value);
    }

    static Motion interpolate(const Motion& from, const Motion& to,
                              float progress) noexcept
    {
        Motion output;
        output.shift = from.shift + (to.shift - from.shift) * progress;
        output.rotation = from.rotation + (to.rotation - from.rotation) * progress;
        return output;
    }

    void sample(const State& state, float time, Vec3& shift, Vec3& rotation) const noexcept
    {
        auto cycle = scene ? scene->duration() : 0.0f;
        auto motion = detail::retargetSample(state.motion, time, cycle, interpolate);
        shift = motion.shift;
        rotation = motion.rotation;
    }

    void sample(uint32_t index, float time, Vec3& shift, Vec3& rotation) noexcept
    {
        sample(states[index], time, shift, rotation);
        for (auto i = 0u; i < keyCount; i++) {
            auto& binding = keys[i];
            DigitalState key;
            if (binding.state != index || !inputState
                || inputState->key(binding.config.key, key) != Result::Success
                || !key.down) continue;
            auto delta = elapsed(time, binding.sampled);
            binding.held += delta;
            binding.sampled = time;
            shift = shift + binding.config.shift * _keyAmount(binding.held,
                                                               binding.config.period);
        }
    }

    void rebase(uint32_t index, float time) noexcept
    {
        Vec3 shift;
        Vec3 rotation;
        sample(index, time, shift, rotation);
        auto& state = states[index];
        detail::retargetSet(state.motion, Motion{shift, rotation});
        for (auto i = 0u; i < keyCount; i++) {
            auto& binding = keys[i];
            DigitalState key;
            if (binding.state != index || !inputState
                || inputState->key(binding.config.key, key) != Result::Success
                || !key.down) continue;
            binding.sampled = time;
            binding.held = 0.0f;
        }
    }

    bool hasMove(Key key) const noexcept
    {
        for (auto i = 0u; i < keyCount; i++) {
            if (keys[i].config.key == key) return true;
        }
        return false;
    }

    bool hasTrigger(Key key) const noexcept
    {
        for (auto i = 0u; i < triggerCount; i++) {
            if (triggers[i].key == key) return true;
        }
        return false;
    }

    void emit(Key key, KeyTrigger trigger, float begin, float end) noexcept
    {
        auto count = triggerCount;
        for (auto i = 0u; i < count; i++) {
            auto binding = triggers[i];
            if (binding.key != key || binding.trigger != trigger) continue;
            binding.callback({key, trigger, begin, end}, binding.data);
        }
    }

    void transition(State& state, float time, const Vec3& shift, const Vec3& rotation,
                    float period) noexcept
    {
        auto cycle = scene ? scene->duration() : 0.0f;
        detail::retarget(state.motion, Motion{shift, rotation}, time, period,
                         AnimCurve::preset(AnimCurvePreset::Smooth), cycle,
                         interpolate);
    }
};

Controller::Controller(Scene* scene) noexcept : pImpl(new (std::nothrow) Impl)
{
    if (!pImpl) return;
    pImpl->scene = scene;
    pImpl->inputState = vis::input::State::gen();
}

Controller::~Controller()
{
    if (pImpl && pImpl->scene) pImpl->scene->runtimeRemove(this);
    delete pImpl;
}

Controller* Controller::gen(Scene* scene) noexcept
{
    if (!scene) return nullptr;
    auto controller = new (std::nothrow) Controller(scene);
    if (!controller || !controller->pImpl || !controller->pImpl->inputState) {
        delete controller;
        return nullptr;
    }
    RuntimeModifier modifier;
    modifier.object = modifyObject;
    modifier.camera = modifyCamera;
    modifier.input = modifyInput;
    modifier.data = controller;
    modifier.key = &RuntimeKey;
    if (scene->runtimeAdd(&modifier) != Result::Success) {
        delete controller;
        return nullptr;
    }
    if (scene->add(controller) == Result::Success) return controller;
    scene->runtimeRemove(controller);
    delete controller;
    return nullptr;
}

Scene* Controller::scene() const noexcept
{
    return pImpl ? pImpl->scene : nullptr;
}

State* Controller::state() const noexcept
{
    return pImpl ? pImpl->inputState : nullptr;
}

Result Controller::pointerFollow(const PointerFollow& binding) noexcept
{
    if (!pImpl || !validRegion(binding.region) || !_finite(binding.targetOrigin)
        || !_finite(binding.mapOrigin) || !_finite(binding.mapX) || !_finite(binding.mapY)
        || !std::isfinite(binding.angleOffset)) {
        return Result::InvalidArguments;
    }
    auto result = pImpl->admit(binding.target, binding.period, this);
    if (result != Result::Success) return result;
    for (auto i = 0u; i < pImpl->keyCount; i++) {
        if (pImpl->keys[i].config.target == binding.target) return Result::InsufficientCondition;
    }
    if (!grow(pImpl->pointers, pImpl->pointerCount, pImpl->pointerCapacity, 1u, 4u)) {
        return Result::OutOfMemory;
    }
    auto state = pImpl->track(binding.target, binding.targetOrigin);
    if (state == UINT32_MAX) return Result::OutOfMemory;
    pImpl->pointers[pImpl->pointerCount++] = {binding, state};
    return Result::Success;
}

Result Controller::keyMove(const KeyMove& binding) noexcept
{
    if (!pImpl || !validKey(binding.key) || !_finite(binding.shift)) {
        return Result::InvalidArguments;
    }
    auto result = pImpl->admit(binding.target, binding.period, this);
    if (result != Result::Success) return result;
    for (auto i = 0u; i < pImpl->keyCount; i++) {
        auto& candidate = pImpl->keys[i].config;
        if (candidate.target == binding.target && candidate.key == binding.key) {
            return Result::InsufficientCondition;
        }
    }
    if (!grow(pImpl->keys, pImpl->keyCount, pImpl->keyCapacity, 1u, 8u)) return Result::OutOfMemory;
    auto state = pImpl->track(binding.target, {});
    if (state == UINT32_MAX) return Result::OutOfMemory;
    pImpl->keys[pImpl->keyCount++] = {binding, state};
    return Result::Success;
}

Result Controller::keyTrigger(const KeyTriggerBinding& binding) noexcept
{
    if (!pImpl || !pImpl->scene || !validKey(binding.key)
        || static_cast<uint8_t>(binding.trigger) > static_cast<uint8_t>(KeyTrigger::Up)
        || !binding.callback) {
        return Result::InvalidArguments;
    }
    if (pImpl->pointerCount + pImpl->keyCount + pImpl->triggerCount >= BindingLimit) {
        return Result::InsufficientCondition;
    }
    if (!grow(pImpl->triggers, pImpl->triggerCount, pImpl->triggerCapacity, 1u, 8u)) {
        return Result::OutOfMemory;
    }
    pImpl->triggers[pImpl->triggerCount++] = binding;
    return Result::Success;
}

Result Controller::keyState(Key key, KeyState& state) const noexcept
{
    if (!pImpl || !validKey(key)) return Result::InvalidArguments;
    DigitalState input;
    auto result = pImpl->inputState->key(key, input);
    if (result != Result::Success) return result;
    state.begin = input.begin;
    state.end = input.end;
    state.down = input.down;
    return result;
}

InputResult Controller::input(const InputEvent& event) noexcept
{
    if (!pImpl || !validEvent(event)) return {false, false, Result::InvalidArguments};
    InputResult result;
    if (event.kind == InputKind::KeyDown && event.modifiers & ShortcutModifiers) {
        return result;
    }
    DigitalState previous;
    auto keyEvent = event.kind == InputKind::KeyDown || event.kind == InputKind::KeyUp;
    if (keyEvent && pImpl->inputState->key(event.key, previous) != Result::Success) {
        return {false, false, Result::InvalidArguments};
    }
    if (!keyEvent) {
        result.status = pImpl->inputState->input(event);
        if (result.status != Result::Success) return result;
    }
    if (event.kind == InputKind::PointerMove || event.kind == InputKind::PointerCancel) {
        for (auto i = 0u; i < pImpl->pointerCount; i++) {
            auto& binding = pImpl->pointers[i];
            auto& config = binding.config;
            auto& state = pImpl->states[binding.state];
            // A cancel leaves every region.
            if (event.kind == InputKind::PointerCancel
                || !contains(config.region, event.position)) {
                if (!binding.hovered) continue;
                binding.hovered = false;
                result.handled = true;
                if (!config.resetOnLeave) continue;
                pImpl->transition(state, event.time, {}, {}, config.period);
                result.redraw = true;
                continue;
            }
            binding.hovered = true;
            auto u = clamp01((event.position.x - config.region.x) / config.region.width);
            auto v = clamp01((event.position.y - config.region.y) / config.region.height);
            auto mapped = config.mapOrigin + config.mapX * u + config.mapY * v;
            Vec3 currentShift;
            Vec3 currentRotation;
            pImpl->sample(state, event.time, currentShift, currentRotation);
            auto targetShift = mapped - config.targetOrigin;
            auto targetRotation = currentRotation;
            auto direction = targetShift - currentShift;
            if (config.rotate && (std::fabs(direction.x) > 1.0e-6f
                                  || std::fabs(direction.y) > 1.0e-6f)) {
                targetRotation.z = _angle(currentRotation.z,
                    std::atan2(direction.y, direction.x) + config.angleOffset);
            }
            pImpl->transition(state, event.time, targetShift, targetRotation, config.period);
            result.handled = true;
            result.redraw = true;
        }
    } else if (keyEvent) {
        auto moved = pImpl->hasMove(event.key);
        auto triggered = pImpl->hasTrigger(event.key);
        if (event.kind == InputKind::KeyDown) {
            result.handled = moved || triggered;
            if (previous.down) {
                result.status = pImpl->inputState->input(event);
                return result;
            }
            for (auto i = 0u; i < pImpl->keyCount; i++) {
                auto& binding = pImpl->keys[i];
                if (binding.config.key != event.key) continue;
                pImpl->rebase(binding.state, event.time);
                binding.sampled = event.time;
                binding.held = 0.0f;
            }
            result.status = pImpl->inputState->input(event);
            if (result.status != Result::Success) return result;
            pImpl->emit(event.key, KeyTrigger::Down, event.time, event.time);
            result.redraw = moved;
        } else {
            if (!previous.down) {
                result.status = pImpl->inputState->input(event);
                return result;
            }
            result.handled = moved || triggered;
            for (auto i = 0u; i < pImpl->keyCount; i++) {
                auto& binding = pImpl->keys[i];
                if (binding.config.key != event.key) continue;
                pImpl->rebase(binding.state, event.time);
            }
            result.status = pImpl->inputState->input(event);
            if (result.status != Result::Success) return result;
            pImpl->emit(event.key, KeyTrigger::Up, previous.begin, event.time);
            pImpl->emit(event.key, KeyTrigger::Tap, previous.begin, event.time);
            result.redraw = moved;
        }
    }
    return result;
}

InputResult Controller::release(float time) noexcept
{
    if (!pImpl || !std::isfinite(time) || time < 0.0f) {
        return {false, false, Result::InvalidArguments};
    }
    InputResult released;
    for (auto value = static_cast<uint32_t>(Key::ArrowLeft); value < KeyCount; value++) {
        auto key = static_cast<Key>(value);
        DigitalState state;
        if (pImpl->inputState->key(key, state) != Result::Success || !state.down) continue;
        InputEvent event;
        event.kind = InputKind::KeyUp;
        event.key = key;
        event.time = time;
        auto result = input(event);
        if (result.status != Result::Success) return result;
        released.handled = released.handled || result.handled;
        released.redraw = released.redraw || result.redraw;
    }
    InputEvent cancel;
    cancel.kind = InputKind::PointerCancel;
    cancel.time = time;
    auto result = input(cancel);
    if (result.status != Result::Success) return result;
    released.handled = released.handled || result.handled;
    released.redraw = released.redraw || result.redraw;
    released.status = pImpl->inputState->release(time);
    return released;
}

Result Controller::camera(CameraAction action, const Vec2& delta) noexcept
{
    return pImpl ? pImpl->camera.act(action, delta) : Result::InvalidArguments;
}

Result Controller::cameraMove(const Vec2& delta) noexcept
{
    return camera(CameraAction::Pan, delta);
}

Result Controller::cameraOrbit(const Vec2& delta) noexcept
{
    return camera(CameraAction::Orbit, delta);
}

Result Controller::cameraZoom(float delta) noexcept
{
    return camera(CameraAction::Zoom, {0.0f, delta});
}

Result Controller::cameraReset() noexcept
{
    return camera(CameraAction::Reset, {});
}

Result Controller::cameraView(CameraView view) noexcept
{
    return pImpl ? pImpl->camera.view(view) : Result::InvalidArguments;
}

Result Controller::modifyObject(const Object* object, float time, Mat4& model, float&,
                                float&, void* data) noexcept
{
    auto controller = static_cast<Controller*>(data);
    if (!controller || !controller->pImpl || !object
        || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    auto& impl = *controller->pImpl;
    for (auto i = 0u; i < impl.stateCount; i++) {
        auto& state = impl.states[i];
        if (state.object != object) continue;
        Vec3 shift;
        Vec3 rotation;
        impl.sample(i, time, shift, rotation);
        auto transform = Mat4::translate(state.origin + shift)
                       * Mat4::rotateZ(rotation.z)
                       * Mat4::rotateY(rotation.y)
                       * Mat4::rotateX(rotation.x)
                       * Mat4::translate(state.origin * -1.0f);
        model = transform * model;
        return _finite(model) ? Result::Success : Result::InvalidArguments;
    }
    return Result::Success;
}

Result Controller::modifyCamera(float time, Camera& camera, CameraView& view,
                                void* data) noexcept
{
    auto controller = static_cast<Controller*>(data);
    if (!controller || !controller->pImpl || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    return controller->pImpl->camera.apply(camera, view);
}

Result Controller::modifyInput(const CameraInput& input, void* data) noexcept
{
    auto controller = static_cast<Controller*>(data);
    return controller ? controller->camera(input.action, input.delta)
                      : Result::InvalidArguments;
}

}  // namespace vis::input
