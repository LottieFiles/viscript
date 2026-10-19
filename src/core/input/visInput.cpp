#include <cmath>
#include <new>

#include "vis_input.h"
#include "../common/visCamera.h"
#include "../common/visFinite.h"
#include "../common/visRetarget.h"

namespace vis::input
{

namespace
{
constexpr uint32_t ShortcutModifiers = 2u | 4u | 8u;
uint8_t RuntimeKey = 0u;

static float _clamp01(float value) noexcept
{
    return _clamp(value, 0.0f, 1.0f);
}

static bool _valid(const BBox& bounds) noexcept
{
    return std::isfinite(bounds.x) && std::isfinite(bounds.y)
           && std::isfinite(bounds.width) && std::isfinite(bounds.height)
           && bounds.width > 0.0f && bounds.height > 0.0f;
}

static bool _contains(const BBox& bounds, const Vec2& point) noexcept
{
    return point.x >= bounds.x && point.x <= bounds.x + bounds.width
           && point.y >= bounds.y && point.y <= bounds.y + bounds.height;
}

static bool _valid(const InputEvent& event) noexcept
{
    if (static_cast<uint8_t>(event.kind) > static_cast<uint8_t>(InputKind::KeyUp)
        || !_finite(event.position) || !_finite(event.delta) || !_finite(event.wheel)
        || !std::isfinite(event.time) || event.time < 0.0f) {
        return false;
    }
    if ((event.kind == InputKind::KeyDown || event.kind == InputKind::KeyUp)
        && (event.key == Key::Unknown
            || static_cast<uint8_t>(event.key) > static_cast<uint8_t>(Key::Tab))) {
        return false;
    }
    return true;
}

static bool _family(const Object* first, const Object* second) noexcept
{
    if (!first || !second) return false;
    for (auto object = first; object; object = object->parent()) {
        if (object == second) return true;
    }
    for (auto object = second; object; object = object->parent()) {
        if (object == first) return true;
    }
    return false;
}

static float _angle(float current, float target) noexcept
{
    while (target - current > PI) target -= TWO_PI;
    while (target - current < -PI) target += TWO_PI;
    return target;
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
    Vec2 move;
    Vec2 orbit;
    float zoom = 0.0f;
    int8_t view = -1;

    ~Impl()
    {
        delete[] states;
        delete[] pointers;
        delete[] keys;
        delete[] triggers;
        delete inputState;
    }

    bool growStates() noexcept
    {
        if (stateCount < stateCapacity) return true;
        auto capacity = stateCapacity ? stateCapacity * 2u : 8u;
        if (capacity < stateCapacity) return false;
        auto grown = new (std::nothrow) State[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < stateCount; i++) grown[i] = states[i];
        delete[] states;
        states = grown;
        stateCapacity = capacity;
        return true;
    }

    bool growPointers() noexcept
    {
        if (pointerCount < pointerCapacity) return true;
        auto capacity = pointerCapacity ? pointerCapacity * 2u : 4u;
        if (capacity < pointerCapacity) return false;
        auto grown = new (std::nothrow) PointerBinding[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < pointerCount; i++) grown[i] = pointers[i];
        delete[] pointers;
        pointers = grown;
        pointerCapacity = capacity;
        return true;
    }

    bool growKeys() noexcept
    {
        if (keyCount < keyCapacity) return true;
        auto capacity = keyCapacity ? keyCapacity * 2u : 8u;
        if (capacity < keyCapacity) return false;
        auto grown = new (std::nothrow) KeyBinding[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < keyCount; i++) grown[i] = keys[i];
        delete[] keys;
        keys = grown;
        keyCapacity = capacity;
        return true;
    }

    bool growTriggers() noexcept
    {
        if (triggerCount < triggerCapacity) return true;
        auto capacity = triggerCapacity ? triggerCapacity * 2u : 8u;
        if (capacity < triggerCapacity) return false;
        auto grown = new (std::nothrow) KeyTriggerBinding[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < triggerCount; i++) grown[i] = triggers[i];
        delete[] triggers;
        triggers = grown;
        triggerCapacity = capacity;
        return true;
    }

    uint32_t find(const Object* object) const noexcept
    {
        for (auto i = 0u; i < stateCount; i++) {
            if (states[i].object == object) return i;
        }
        return UINT32_MAX;
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
    if (!pImpl || !pImpl->scene || !binding.target || !_valid(binding.region)
        || !_finite(binding.targetOrigin) || !_finite(binding.mapOrigin)
        || !_finite(binding.mapX) || !_finite(binding.mapY)
        || !std::isfinite(binding.period) || binding.period < 0.0f
        || !std::isfinite(binding.angleOffset)
        || pImpl->scene->object(binding.target->id()) != binding.target
        || _family(binding.target, this)) {
        return Result::InvalidArguments;
    }
    if (pImpl->pointerCount + pImpl->keyCount + pImpl->triggerCount >= BindingLimit) {
        return Result::InsufficientCondition;
    }
    for (auto i = 0u; i < pImpl->pointerCount; i++) {
        if (pImpl->pointers[i].config.target == binding.target) {
            return Result::InsufficientCondition;
        }
    }
    for (auto i = 0u; i < pImpl->keyCount; i++) {
        if (pImpl->keys[i].config.target == binding.target) {
            return Result::InsufficientCondition;
        }
    }
    auto state = pImpl->find(binding.target);
    auto addState = state == UINT32_MAX;
    if (!pImpl->growPointers() || (addState && !pImpl->growStates())) {
        return Result::OutOfMemory;
    }
    if (addState) {
        state = pImpl->stateCount++;
        pImpl->states[state].object = binding.target;
        pImpl->states[state].origin = binding.targetOrigin;
    }
    auto& pointer = pImpl->pointers[pImpl->pointerCount++];
    pointer.config = binding;
    pointer.state = state;
    return Result::Success;
}

Result Controller::keyMove(const KeyMove& binding) noexcept
{
    if (!pImpl || !pImpl->scene || !binding.target || binding.key == Key::Unknown
        || static_cast<uint8_t>(binding.key) > static_cast<uint8_t>(Key::Tab)
        || !_finite(binding.shift) || !std::isfinite(binding.period) || binding.period < 0.0f
        || pImpl->scene->object(binding.target->id()) != binding.target
        || _family(binding.target, this)) {
        return Result::InvalidArguments;
    }
    if (pImpl->pointerCount + pImpl->keyCount + pImpl->triggerCount >= BindingLimit) {
        return Result::InsufficientCondition;
    }
    for (auto i = 0u; i < pImpl->pointerCount; i++) {
        if (pImpl->pointers[i].config.target == binding.target) {
            return Result::InsufficientCondition;
        }
    }
    for (auto i = 0u; i < pImpl->keyCount; i++) {
        auto& candidate = pImpl->keys[i].config;
        if (candidate.target == binding.target && candidate.key == binding.key) {
            return Result::InsufficientCondition;
        }
    }
    auto state = pImpl->find(binding.target);
    auto addState = state == UINT32_MAX;
    if (!pImpl->growKeys() || (addState && !pImpl->growStates())) {
        return Result::OutOfMemory;
    }
    if (addState) {
        state = pImpl->stateCount++;
        pImpl->states[state].object = binding.target;
    }
    auto& key = pImpl->keys[pImpl->keyCount++];
    key.config = binding;
    key.state = state;
    return Result::Success;
}

Result Controller::keyTrigger(const KeyTriggerBinding& binding) noexcept
{
    if (!pImpl || !pImpl->scene || binding.key == Key::Unknown
        || static_cast<uint8_t>(binding.key) > static_cast<uint8_t>(Key::Tab)
        || static_cast<uint8_t>(binding.trigger) > static_cast<uint8_t>(KeyTrigger::Up)
        || !binding.callback) {
        return Result::InvalidArguments;
    }
    if (pImpl->pointerCount + pImpl->keyCount + pImpl->triggerCount >= BindingLimit) {
        return Result::InsufficientCondition;
    }
    if (!pImpl->growTriggers()) return Result::OutOfMemory;
    pImpl->triggers[pImpl->triggerCount++] = binding;
    return Result::Success;
}

Result Controller::keyState(Key key, KeyState& state) const noexcept
{
    if (!pImpl || key == Key::Unknown
        || static_cast<uint8_t>(key) > static_cast<uint8_t>(Key::Tab)) {
        return Result::InvalidArguments;
    }
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
    if (!pImpl || !_valid(event)) return {false, false, Result::InvalidArguments};
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
    if (event.kind == InputKind::PointerMove) {
        for (auto i = 0u; i < pImpl->pointerCount; i++) {
            auto& binding = pImpl->pointers[i];
            auto& config = binding.config;
            auto inside = _contains(config.region, event.position);
            if (!inside) {
                if (!binding.hovered) continue;
                binding.hovered = false;
                result.handled = true;
                if (!config.resetOnLeave) continue;
                auto& state = pImpl->states[binding.state];
                pImpl->transition(state, event.time, {}, {}, config.period);
                result.redraw = true;
                continue;
            }
            binding.hovered = true;
            auto u = _clamp01((event.position.x - config.region.x) / config.region.width);
            auto v = _clamp01((event.position.y - config.region.y) / config.region.height);
            auto mapped = config.mapOrigin + config.mapX * u + config.mapY * v;
            auto& state = pImpl->states[binding.state];
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
    } else if (event.kind == InputKind::PointerCancel) {
        for (auto i = 0u; i < pImpl->pointerCount; i++) {
            auto& binding = pImpl->pointers[i];
            if (!binding.hovered) continue;
            binding.hovered = false;
            result.handled = true;
            if (!binding.config.resetOnLeave) continue;
            auto& state = pImpl->states[binding.state];
            pImpl->transition(state, event.time, {}, {}, binding.config.period);
            result.redraw = true;
        }
    } else if (event.kind == InputKind::KeyDown || event.kind == InputKind::KeyUp) {
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
    for (auto value = static_cast<uint8_t>(Key::ArrowLeft);
         value <= static_cast<uint8_t>(Key::Tab); value++) {
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
    if (!pImpl || !_finite(delta)
        || static_cast<uint8_t>(action) > static_cast<uint8_t>(CameraAction::View3D)) {
        return Result::InvalidArguments;
    }
    if (action == CameraAction::Reset) return cameraReset();
    if (action == CameraAction::View2D) return cameraView(CameraView::TwoD);
    if (action == CameraAction::View3D) return cameraView(CameraView::ThreeD);
    auto bounded = Vec2{_clamp(delta.x, -4.0f, 4.0f), _clamp(delta.y, -4.0f, 4.0f)};
    if (action == CameraAction::Pan) {
        auto next = pImpl->move + bounded;
        if (!_finite(next)) return Result::InvalidArguments;
        pImpl->move = next;
    } else if (action == CameraAction::Orbit) {
        auto next = pImpl->orbit + bounded;
        if (!_finite(next)) return Result::InvalidArguments;
        pImpl->orbit = next;
    } else {
        auto next = pImpl->zoom + bounded.y;
        if (!std::isfinite(next)) return Result::InvalidArguments;
        pImpl->zoom = next;
    }
    return Result::Success;
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
    if (!pImpl) return Result::InvalidArguments;
    pImpl->move = {};
    pImpl->orbit = {};
    pImpl->zoom = 0.0f;
    pImpl->view = -1;
    return Result::Success;
}

Result Controller::cameraView(CameraView view) noexcept
{
    if (!pImpl || (view != CameraView::TwoD && view != CameraView::ThreeD)) {
        return Result::InvalidArguments;
    }
    pImpl->view = view == CameraView::TwoD ? 0 : 1;
    return Result::Success;
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
    auto& impl = *controller->pImpl;
    if (impl.view >= 0) {
        auto result = _cameraAction(camera, view,
                                    impl.view == 0 ? CameraAction::View2D
                                                   : CameraAction::View3D,
                                    {});
        if (result != Result::Success) return result;
    }
    auto result = _cameraAction(camera, view, CameraAction::Orbit, impl.orbit);
    if (result != Result::Success) return result;
    result = _cameraAction(camera, view, CameraAction::Zoom, {0.0f, impl.zoom});
    if (result != Result::Success) return result;
    result = _cameraAction(camera, view, CameraAction::Pan, impl.move);
    if (result != Result::Success) return result;
    return _validCamera(camera, view) ? Result::Success : Result::InvalidArguments;
}

Result Controller::modifyInput(const CameraInput& input, void* data) noexcept
{
    auto controller = static_cast<Controller*>(data);
    return controller ? controller->camera(input.action, input.delta)
                      : Result::InvalidArguments;
}

}  // namespace vis::input
