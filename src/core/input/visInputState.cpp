// The input module's state: the keys and pointers a frame holds, and the action maps that name
// them.
#include <cmath>
#include <cstring>
#include <new>

#include "vis_input.h"
#include "visInputEvent.h"
#include "../common/visFinite.h"
#include "../common/visGrow.h"

namespace vis::input
{

namespace
{

enum struct Source : uint8_t
{
    Key = 0,
    Pointer
};

static uint32_t _length(const char* value) noexcept
{
    if (!value) return UINT32_MAX;
    for (auto length = 0u; length <= ActionMap::NameLimit; length++) {
        if (!value[length]) return length;
    }
    return UINT32_MAX;
}

static float _clamp(float value) noexcept
{
    return std::fmax(-1.0f, std::fmin(1.0f, value));
}

}  // namespace

struct State::Impl
{
    DigitalState keys[KeyCount];
    PointerState pointers[PointerLimit];
    uint64_t frame = 0u;
    float time = 0.0f;
    uint32_t pointerCount = 0u;

    PointerState* pointer(uint32_t id) noexcept
    {
        for (auto i = 0u; i < pointerCount; i++) {
            if (pointers[i].pointer == id) return pointers + i;
        }
        return nullptr;
    }
};

State::State() noexcept : pImpl(new (std::nothrow) Impl)
{
}

State::~State()
{
    delete pImpl;
}

State* State::gen() noexcept
{
    auto state = new (std::nothrow) State;
    if (state && state->pImpl) return state;
    delete state;
    return nullptr;
}

Result State::begin(float time) noexcept
{
    if (!pImpl || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    if (pImpl->frame == UINT64_MAX) return Result::InsufficientCondition;
    for (auto& state : pImpl->keys) {
        state.previous = state.down;
        state.pressed = false;
        state.released = false;
    }
    for (auto i = 0u; i < pImpl->pointerCount; i++) {
        auto& state = pImpl->pointers[i];
        state.previousButtons = state.buttons;
        state.delta = {};
        state.wheel = {};
        state.pressed = 0u;
        state.released = 0u;
    }
    pImpl->time = time;
    pImpl->frame++;
    return Result::Success;
}

Result State::input(const InputEvent& event) noexcept
{
    if (!pImpl || !validEvent(event)) return Result::InvalidArguments;
    pImpl->time = event.time;
    if (event.kind == InputKind::KeyDown || event.kind == InputKind::KeyUp) {
        auto& state = pImpl->keys[static_cast<uint32_t>(event.key)];
        if (event.kind == InputKind::KeyDown) {
            if (state.down) return Result::Success;
            state.down = true;
            state.pressed = true;
            state.begin = state.end = event.time;
        } else {
            if (!state.down) return Result::Success;
            state.down = false;
            state.released = true;
            state.end = event.time;
        }
        return Result::Success;
    }

    auto state = pImpl->pointer(event.pointer);
    if (!state) {
        if (event.kind == InputKind::PointerCancel) return Result::Success;
        if (pImpl->pointerCount == PointerLimit) return Result::InsufficientCondition;
        state = pImpl->pointers + pImpl->pointerCount++;
        state->pointer = event.pointer;
    }
    state->position = event.position;
    if (event.kind == InputKind::PointerMove) state->delta = state->delta + event.delta;
    if (event.kind == InputKind::Wheel) state->wheel = state->wheel + event.wheel;

    auto buttons = state->buttons;
    if (event.kind == InputKind::PointerDown) buttons = event.buttons | (1u << event.button);
    else if (event.kind == InputKind::PointerUp) buttons = event.buttons & ~(1u << event.button);
    else if (event.kind == InputKind::PointerMove) buttons = event.buttons;
    else if (event.kind == InputKind::PointerCancel) buttons = 0u;
    state->pressed |= buttons & ~state->buttons;
    state->released |= state->buttons & ~buttons;
    state->buttons = buttons;
    state->active = event.kind != InputKind::PointerCancel;
    return Result::Success;
}

Result State::release(float time) noexcept
{
    if (!pImpl || !std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    for (auto& state : pImpl->keys) {
        if (!state.down) continue;
        state.down = false;
        state.released = true;
        state.end = time;
    }
    for (auto i = 0u; i < pImpl->pointerCount; i++) {
        auto& state = pImpl->pointers[i];
        state.released |= state.buttons;
        state.buttons = 0u;
        state.active = false;
    }
    pImpl->time = time;
    return Result::Success;
}

Result State::key(Key key, DigitalState& state) const noexcept
{
    if (!pImpl || !validKey(key)) return Result::InvalidArguments;
    state = pImpl->keys[static_cast<uint32_t>(key)];
    return Result::Success;
}

Result State::pointer(uint32_t pointer, PointerState& state) const noexcept
{
    if (!pImpl) return Result::InvalidArguments;
    auto found = pImpl->pointer(pointer);
    if (!found) return Result::InsufficientCondition;
    state = *found;
    return Result::Success;
}

uint64_t State::frame() const noexcept
{
    return pImpl ? pImpl->frame : 0u;
}

float State::time() const noexcept
{
    return pImpl ? pImpl->time : 0.0f;
}

struct ActionMap::Impl
{
    struct Binding
    {
        char action[NameLimit + 1u];
        Vec2 value;
        uint32_t pointer = 0u;
        Key key = Key::Unknown;
        uint8_t button = 0u;
        Source source = Source::Key;
    };

    Binding* bindings = nullptr;
    uint32_t bindingCount = 0u;
    uint32_t bindingCapacity = 0u;

    ~Impl()
    {
        delete[] bindings;
    }

    Binding* find(const char* action, Source source, Key key, uint32_t pointer,
                  uint8_t button) noexcept
    {
        for (auto i = 0u; i < bindingCount; i++) {
            auto& binding = bindings[i];
            if (binding.source != source || std::strcmp(binding.action, action) != 0) {
                continue;
            }
            if (source == Source::Key && binding.key == key) return &binding;
            if (source == Source::Pointer && binding.pointer == pointer
                && binding.button == button) {
                return &binding;
            }
        }
        return nullptr;
    }

    Result bind(const char* action, Source source, Key key, uint32_t pointer,
                uint8_t button, const Vec2& value) noexcept
    {
        auto length = _length(action);
        if (!length || length == UINT32_MAX || !_finite(value)
            || (value.x == 0.0f && value.y == 0.0f)) {
            return Result::InvalidArguments;
        }
        auto binding = find(action, source, key, pointer, button);
        if (binding) {
            binding->value = value;
            return Result::Success;
        }
        if (bindingCount == BindingLimit) return Result::InsufficientCondition;
        if (!grow(bindings, bindingCount, bindingCapacity, 1u, 8u, BindingLimit)) {
            return Result::OutOfMemory;
        }
        binding = bindings + bindingCount++;
        std::memcpy(binding->action, action, length + 1u);
        binding->source = source;
        binding->key = key;
        binding->pointer = pointer;
        binding->button = button;
        binding->value = value;
        return Result::Success;
    }
};

ActionMap::ActionMap() noexcept : pImpl(new (std::nothrow) Impl)
{
}

ActionMap::~ActionMap()
{
    delete pImpl;
}

ActionMap* ActionMap::gen() noexcept
{
    auto map = new (std::nothrow) ActionMap;
    if (map && map->pImpl) return map;
    delete map;
    return nullptr;
}

Result ActionMap::key(const char* action, Key key, const Vec2& value) noexcept
{
    if (!pImpl || !validKey(key)) return Result::InvalidArguments;
    return pImpl->bind(action, Source::Key, key, 0u, 0u, value);
}

Result ActionMap::pointer(const char* action, uint32_t button, const Vec2& value,
                          uint32_t pointer) noexcept
{
    if (!pImpl || button >= 32u) return Result::InvalidArguments;
    return pImpl->bind(action, Source::Pointer, Key::Unknown, pointer,
                       static_cast<uint8_t>(button), value);
}

Result ActionMap::remove(const char* action) noexcept
{
    auto length = _length(action);
    if (!pImpl || !length || length == UINT32_MAX) return Result::InvalidArguments;
    auto removed = false;
    for (auto i = 0u; i < pImpl->bindingCount;) {
        if (std::strcmp(pImpl->bindings[i].action, action) != 0) {
            i++;
            continue;
        }
        pImpl->bindings[i] = pImpl->bindings[--pImpl->bindingCount];
        removed = true;
    }
    return removed ? Result::Success : Result::InsufficientCondition;
}

void ActionMap::clear() noexcept
{
    if (pImpl) pImpl->bindingCount = 0u;
}

Result ActionMap::state(const State* input, const char* action,
                        ActionState& state) const noexcept
{
    auto length = _length(action);
    if (!pImpl || !input || !length || length == UINT32_MAX) {
        return Result::InvalidArguments;
    }
    ActionState sampled;
    auto found = false;
    auto previous = false;
    auto current = false;
    auto pressed = false;
    auto released = false;
    for (auto i = 0u; i < pImpl->bindingCount; i++) {
        auto& binding = pImpl->bindings[i];
        if (std::strcmp(binding.action, action) != 0) continue;
        found = true;
        auto wasDown = false;
        auto down = false;
        auto wasPressed = false;
        auto wasReleased = false;
        if (binding.source == Source::Key) {
            DigitalState digital;
            auto result = input->key(binding.key, digital);
            if (result != Result::Success) return result;
            wasDown = digital.previous;
            down = digital.down;
            wasPressed = digital.pressed;
            wasReleased = digital.released;
        } else {
            PointerState pointer;
            auto result = input->pointer(binding.pointer, pointer);
            if (result != Result::Success && result != Result::InsufficientCondition) {
                return result;
            }
            if (result == Result::Success) {
                auto mask = 1u << binding.button;
                wasDown = (pointer.previousButtons & mask) != 0u;
                down = (pointer.buttons & mask) != 0u;
                wasPressed = (pointer.pressed & mask) != 0u;
                wasReleased = (pointer.released & mask) != 0u;
            }
        }
        if (wasDown) sampled.previous = sampled.previous + binding.value;
        if (down) sampled.value = sampled.value + binding.value;
        previous = previous || wasDown;
        current = current || down;
        pressed = pressed || wasPressed;
        released = released || wasReleased;
    }
    if (!found) return Result::InsufficientCondition;
    sampled.previous = {_clamp(sampled.previous.x), _clamp(sampled.previous.y)};
    sampled.value = {_clamp(sampled.value.x), _clamp(sampled.value.y)};
    sampled.delta = sampled.value - sampled.previous;
    sampled.down = current;
    sampled.pressed = !previous && (current || pressed);
    sampled.released = !current && (previous || released);
    state = sampled;
    return Result::Success;
}

uint32_t ActionMap::count() const noexcept
{
    return pImpl ? pImpl->bindingCount : 0u;
}

}  // namespace vis::input
