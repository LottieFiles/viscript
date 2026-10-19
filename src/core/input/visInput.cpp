// The input module's controller: pointer follows, key moves and camera actions, applied to a
// scene through runtime modifiers.
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
    vis::input::State* inputState = nullptr;
    uint32_t stateCount = 0u;
    uint32_t stateCapacity = 0u;
    uint32_t pointerCount = 0u;
    uint32_t pointerCapacity = 0u;
    uint32_t keyCount = 0u;
    uint32_t keyCapacity = 0u;
    CameraDrive camera;

    ~Impl()
    {
        delete[] states;
        delete[] pointers;
        delete[] keys;
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
        if (!target || !std::isfinite(period) || period < 0.0f
            || scene->object(target->id()) != target || family(target, controller)) {
            return Result::InvalidArguments;
        }
        if (pointerCount + keyCount >= BindingLimit) {
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
        auto motion = detail::retargetSample(state.motion, time, scene->duration(), interpolate);
        shift = motion.shift;
        rotation = motion.rotation;
    }

    // Whether binding moves the state at index and its key is down.
    bool down(const KeyBinding& binding, uint32_t index) const noexcept
    {
        DigitalState key;
        inputState->key(binding.config.key, key);
        return binding.state == index && key.down;
    }

    void sample(uint32_t index, float time, Vec3& shift, Vec3& rotation) noexcept
    {
        sample(states[index], time, shift, rotation);
        for (auto i = 0u; i < keyCount; i++) {
            auto& binding = keys[i];
            if (!down(binding, index)) continue;
            binding.held += static_cast<float>(
                detail::retargetElapsed(time, binding.sampled, scene->duration()));
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
        detail::retargetSet(states[index].motion, Motion{shift, rotation});
        for (auto i = 0u; i < keyCount; i++) {
            if (!down(keys[i], index)) continue;
            keys[i].sampled = time;
            keys[i].held = 0.0f;
        }
    }

    bool hasMove(Key key) const noexcept
    {
        for (auto i = 0u; i < keyCount; i++) {
            if (keys[i].config.key == key) return true;
        }
        return false;
    }

    void transition(State& state, float time, const Vec3& shift, const Vec3& rotation,
                    float period) noexcept
    {
        detail::retarget(state.motion, Motion{shift, rotation}, time, period,
                         AnimCurve::preset(AnimCurvePreset::Smooth), scene->duration(),
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
    if (pImpl) pImpl->scene->runtimeRemove(this);
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
    if (scene->runtimeAdd(&modifier) == Result::Success
        && scene->add(controller) == Result::Success) {
        return controller;
    }
    delete controller;
    return nullptr;
}

Scene* Controller::scene() const noexcept
{
    return pImpl->scene;
}

State* Controller::state() const noexcept
{
    return pImpl->inputState;
}

Result Controller::pointerFollow(const PointerFollow& binding) noexcept
{
    if (!validRegion(binding.region) || !_finite(binding.targetOrigin)
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
    if (!validKey(binding.key) || !_finite(binding.shift)) {
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

InputResult Controller::input(const InputEvent& event) noexcept
{
    if (!validEvent(event)) return {false, false, Result::InvalidArguments};
    InputResult result;
    if (event.kind == InputKind::KeyDown || event.kind == InputKind::KeyUp) {
        auto down = event.kind == InputKind::KeyDown;
        if (down && event.modifiers & ShortcutModifiers) return result;
        // A key that goes down or up restarts the moves of its targets from where they are.
        DigitalState previous;
        pImpl->inputState->key(event.key, previous);
        auto moved = pImpl->hasMove(event.key);
        result.handled = moved && (down || previous.down);
        if (previous.down != down) {
            for (auto i = 0u; i < pImpl->keyCount; i++) {
                auto& binding = pImpl->keys[i];
                if (binding.config.key != event.key) continue;
                pImpl->rebase(binding.state, event.time);
                binding.sampled = event.time;
                binding.held = 0.0f;
            }
            result.redraw = moved;
        }
        result.status = pImpl->inputState->input(event);
        return result;
    }
    result.status = pImpl->inputState->input(event);
    if (result.status != Result::Success) return result;
    if (event.kind != InputKind::PointerMove && event.kind != InputKind::PointerCancel) {
        return result;
    }
    for (auto i = 0u; i < pImpl->pointerCount; i++) {
        auto& binding = pImpl->pointers[i];
        auto& config = binding.config;
        auto& state = pImpl->states[binding.state];
        // A cancel leaves every region.
        if (event.kind == InputKind::PointerCancel || !contains(config.region, event.position)) {
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
        if (config.rotate
            && (std::fabs(direction.x) > 1.0e-6f || std::fabs(direction.y) > 1.0e-6f)) {
            targetRotation.z = _angle(currentRotation.z,
                                      std::atan2(direction.y, direction.x) + config.angleOffset);
        }
        pImpl->transition(state, event.time, targetShift, targetRotation, config.period);
        result.handled = true;
        result.redraw = true;
    }
    return result;
}

InputResult Controller::release(float time) noexcept
{
    if (!std::isfinite(time) || time < 0.0f) return {false, false, Result::InvalidArguments};
    // A key-up for every key and a cancel for the pointer; neither fails at a valid time.
    InputResult released;
    InputEvent event;
    event.kind = InputKind::KeyUp;
    event.time = time;
    for (auto value = static_cast<uint32_t>(Key::ArrowLeft); value < KeyCount; value++) {
        event.key = static_cast<Key>(value);
        auto result = input(event);
        released.handled = released.handled || result.handled;
        released.redraw = released.redraw || result.redraw;
    }
    event.kind = InputKind::PointerCancel;
    auto result = input(event);
    released.handled = released.handled || result.handled;
    released.redraw = released.redraw || result.redraw;
    released.status = pImpl->inputState->release(time);
    return released;
}

Result Controller::camera(CameraAction action, const Vec2& delta) noexcept
{
    return pImpl->camera.act(action, delta);
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
    return pImpl->camera.view(view);
}

Result Controller::modifyObject(const Object* object, float time, Mat4& model, float&,
                                float&, void* data) noexcept
{
    if (!std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    auto& impl = *static_cast<Controller*>(data)->pImpl;
    auto index = impl.find(object);
    if (index == UINT32_MAX) return Result::Success;
    auto& state = impl.states[index];
    Vec3 shift;
    Vec3 rotation;
    impl.sample(index, time, shift, rotation);
    auto transform = Mat4::translate(state.origin + shift)
                   * Mat4::rotateZ(rotation.z)
                   * Mat4::rotateY(rotation.y)
                   * Mat4::rotateX(rotation.x)
                   * Mat4::translate(state.origin * -1.0f);
    model = transform * model;
    return Result::Success;
}

Result Controller::modifyCamera(float time, Camera& camera, CameraView& view,
                                void* data) noexcept
{
    if (!std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    return static_cast<Controller*>(data)->pImpl->camera.apply(camera, view);
}

Result Controller::modifyInput(const CameraInput& input, void* data) noexcept
{
    return static_cast<Controller*>(data)->camera(input.action, input.delta);
}

}  // namespace vis::input
