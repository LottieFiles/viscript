// The states module: named states of an object's transform, opacity and progress, with
// transitions between them applied through a runtime modifier.
#include <cmath>
#include <cstring>
#include <new>

#include "vis_states.h"
#include "../common/visFinite.h"
#include "../common/visRetarget.h"
#include "../scene/visScene.h"

namespace vis::states
{

namespace
{

uint8_t RuntimeKey = 0u;

static bool _alphanumeric(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
}

static bool _valid(const State& state) noexcept
{
    return _finite(state.origin) && _finite(state.shift)
        && _finite(state.rotation) && _finite(state.scale)
        && std::isfinite(state.opacity) && state.opacity >= 0.0f
        && state.opacity <= 1.0f && std::isfinite(state.progress)
        && state.progress >= 0.0f && state.progress <= 1.0f;
}

static bool _localOrigin(const Object* object, Vec3& output) noexcept
{
    Bounds bounds;
    if (!localFamilyBounds(object, bounds)) return false;
    output = bounds.center();
    return true;
}

static bool _identifier(const char* value) noexcept
{
    if (!value || !_alphanumeric(*value)) return false;
    auto length = 0u;
    while (value[length]) {
        if (length == Controller::StateNameLimit) return false;
        auto ch = value[length++];
        if (_alphanumeric(ch) || ch == '.' || ch == '_' || ch == '-') continue;
        return false;
    }
    return _alphanumeric(value[length - 1u]);
}

static void _copy(char* output, const char* value) noexcept
{
    auto length = std::strlen(value);
    std::memcpy(output, value, length + 1u);
}

static float _clamp(float value) noexcept
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static State _interpolate(const State& from, const State& to,
                          float progress) noexcept
{
    auto bounded = _clamp(progress);
    State state;
    state.origin = from.origin + (to.origin - from.origin) * progress;
    state.shift = from.shift + (to.shift - from.shift) * progress;
    state.rotation = from.rotation + (to.rotation - from.rotation) * progress;
    state.scale = from.scale + (to.scale - from.scale) * progress;
    state.opacity = from.opacity + (to.opacity - from.opacity) * bounded;
    state.progress = from.progress + (to.progress - from.progress) * bounded;
    state.originEnabled = true;
    return state;
}

static Mat4 _matrix(const State& state, const Vec3& origin) noexcept
{
    return Mat4::translate(origin + state.shift)
        * Mat4::rotateZ(state.rotation.z)
        * Mat4::rotateY(state.rotation.y)
        * Mat4::rotateX(state.rotation.x)
        * Mat4::scale(state.scale)
        * Mat4::translate(origin * -1.0f);
}

template<typename Value>
static bool _grow(Value*& values, uint32_t count, uint32_t& capacity,
                  uint32_t required, uint32_t limit) noexcept
{
    if (required <= capacity) return true;
    if (required > limit) return false;
    auto grownCapacity = capacity ? capacity * 2u : 8u;
    if (grownCapacity < capacity || grownCapacity > limit) grownCapacity = limit;
    while (grownCapacity < required) {
        if (grownCapacity > limit / 2u) {
            grownCapacity = limit;
            break;
        }
        grownCapacity *= 2u;
    }
    auto grown = new (std::nothrow) Value[grownCapacity];
    if (!grown) return false;
    for (auto i = 0u; i < count; i++)
        grown[i] = values[i];
    delete[] values;
    values = grown;
    capacity = grownCapacity;
    return true;
}

static AnimCurve _curve(Easing easing) noexcept
{
    if (static_cast<uint8_t>(easing) > static_cast<uint8_t>(Easing::EaseInOut)) {
        auto invalid = AnimCurve::preset(AnimCurvePreset::Linear);
        invalid.kind = static_cast<AnimCurvePreset>(UINT8_MAX);
        return invalid;
    }
    return AnimCurve::preset(static_cast<AnimCurvePreset>(easing));
}

}  // namespace

struct Controller::Impl
{
    struct Definition
    {
        State value;
        uint32_t object = 0u;
        char name[Controller::StateNameLimit + 1u] = {};
    };

    struct Track
    {
        detail::Retarget<State> motion;
        uint32_t object = 0u;
    };

    Scene* scene = nullptr;
    Definition* definitions = nullptr;
    Track* tracks = nullptr;
    Event* events = nullptr;
    double clock = 0.0;
    uint32_t definitionCount = 0u;
    uint32_t definitionCapacity = 0u;
    uint32_t trackCount = 0u;
    uint32_t trackCapacity = 0u;
    uint32_t eventCount = 0u;
    uint32_t eventCapacity = 0u;
    uint64_t nextTransaction = 1u;

    ~Impl()
    {
        delete[] definitions;
        delete[] tracks;
        delete[] events;
    }

    Definition* definition(uint32_t object, const char* name) const noexcept
    {
        for (auto i = 0u; i < definitionCount; i++) {
            auto& candidate = definitions[i];
            if (candidate.object == object && std::strcmp(candidate.name, name) == 0) {
                return definitions + i;
            }
        }
        return nullptr;
    }

    Track* track(uint32_t object) const noexcept
    {
        auto begin = 0u;
        auto end = trackCount;
        while (begin < end) {
            auto middle = begin + (end - begin) / 2u;
            if (tracks[middle].object < object) begin = middle + 1u;
            else end = middle;
        }
        return begin < trackCount && tracks[begin].object == object
            ? tracks + begin
            : nullptr;
    }

    Track* addTrack(uint32_t object) noexcept
    {
        auto index = trackCount;
        while (index && tracks[index - 1u].object > object) {
            tracks[index] = tracks[index - 1u];
            index--;
        }
        tracks[index] = {};
        tracks[index].object = object;
        trackCount++;
        return tracks + index;
    }
};

Controller::Controller(Scene* scene) noexcept : pImpl(new (std::nothrow) Impl)
{
    if (pImpl) pImpl->scene = scene;
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
    if (!controller || !controller->pImpl) {
        delete controller;
        return nullptr;
    }
    RuntimeModifier modifier;
    modifier.object = modifyObject;
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

Result Controller::define(Object* object, const char* name, const State& state) noexcept
{
    if (!pImpl || !pImpl->scene || !object || object == this || !_identifier(name)
        || !_valid(state) || !object->id()
        || pImpl->scene->object(object->id()) != object) {
        return Result::InvalidArguments;
    }
    if (pImpl->definition(object->id(), name)) return Result::InsufficientCondition;
    if (pImpl->definitionCount == StateLimit) return Result::InsufficientCondition;
    auto resolved = state;
    if (!resolved.originEnabled) {
        if (!_localOrigin(object, resolved.origin)) {
            return Result::InsufficientCondition;
        }
        resolved.originEnabled = true;
    }
    if (!_grow(pImpl->definitions, pImpl->definitionCount,
               pImpl->definitionCapacity, pImpl->definitionCount + 1u,
               StateLimit)) {
        return Result::OutOfMemory;
    }
    auto& definition = pImpl->definitions[pImpl->definitionCount++];
    definition.object = object->id();
    definition.value = resolved;
    _copy(definition.name, name);
    return Result::Success;
}

Result Controller::transition(Object* object, const char* state, float duration,
                              Easing easing) noexcept
{
    auto target = Target{object, state};
    return transition(&target, 1u, duration, _curve(easing));
}

Result Controller::transition(Object* object, const char* state, float duration,
                              const AnimCurve& curve) noexcept
{
    auto target = Target{object, state};
    return transition(&target, 1u, duration, curve);
}

Result Controller::transition(const Target* targets, uint32_t count, float duration,
                              Easing easing) noexcept
{
    return transition(targets, count, duration, _curve(easing));
}

Result Controller::transition(const Target* targets, uint32_t count, float duration,
                              const AnimCurve& curve) noexcept
{
    if (!pImpl || !pImpl->scene || !targets || !count || count > ObjectLimit
        || !std::isfinite(duration) || duration < 0.0f || !curve.valid()) {
        return Result::InvalidArguments;
    }
    auto newTracks = 0u;
    for (auto i = 0u; i < count; i++) {
        auto& target = targets[i];
        if (!target.object || target.object == this || !_identifier(target.state)
            || !target.object->id()
            || pImpl->scene->object(target.object->id()) != target.object) {
            return Result::InvalidArguments;
        }
        if (!pImpl->definition(target.object->id(), target.state)) {
            return Result::InsufficientCondition;
        }
        if (!pImpl->track(target.object->id())) newTracks++;
        for (auto j = i + 1u; j < count; j++) {
            if (target.object == targets[j].object) return Result::InvalidArguments;
        }
    }
    if (newTracks > ObjectLimit - pImpl->trackCount
        || count > EventLimit - pImpl->eventCount
        || !pImpl->nextTransaction) {
        return Result::InsufficientCondition;
    }
    if (!_grow(pImpl->tracks, pImpl->trackCount, pImpl->trackCapacity,
               pImpl->trackCount + newTracks, ObjectLimit)
        || !_grow(pImpl->events, pImpl->eventCount, pImpl->eventCapacity,
                  pImpl->eventCount + count, EventLimit)) {
        return Result::OutOfMemory;
    }
    auto transaction = pImpl->nextTransaction++;
    for (auto i = 0u; i < count; i++) {
        auto& target = targets[i];
        auto definition = pImpl->definition(target.object->id(), target.state);
        auto track = pImpl->track(target.object->id());
        if (!track) {
            track = pImpl->addTrack(target.object->id());
            State initial;
            initial.origin = definition->value.origin;
            initial.originEnabled = true;
            detail::retargetSet(track->motion, initial);
        }
        detail::retarget(track->motion, definition->value, pImpl->clock,
                         duration, curve, 0.0, _interpolate);
        auto& event = pImpl->events[pImpl->eventCount++];
        event.time = pImpl->clock;
        event.curve = curve;
        event.duration = duration;
        event.object = target.object->id();
        event.transaction = transaction;
        _copy(event.state, target.state);
    }
    return Result::Success;
}

Result Controller::advance(float elapsed) noexcept
{
    if (!pImpl || !std::isfinite(elapsed) || elapsed < 0.0f) {
        return Result::InvalidArguments;
    }
    auto next = pImpl->clock + static_cast<double>(elapsed);
    if (!std::isfinite(next)) return Result::InvalidArguments;
    pImpl->clock = next;
    return Result::Success;
}

Result Controller::sample(const Object* object, State& state) const noexcept
{
    if (!pImpl || !pImpl->scene || !object || !object->id()
        || pImpl->scene->object(object->id()) != object) {
        return Result::InvalidArguments;
    }
    auto track = pImpl->track(object->id());
    if (!track) return Result::InsufficientCondition;
    state = detail::retargetSample(track->motion, pImpl->clock, 0.0,
                                   _interpolate);
    return Result::Success;
}

double Controller::time() const noexcept
{
    return pImpl ? pImpl->clock : 0.0;
}

bool Controller::active() const noexcept
{
    if (!pImpl) return false;
    for (auto i = 0u; i < pImpl->trackCount; i++) {
        if (detail::retargetActive(pImpl->tracks[i].motion, pImpl->clock)) {
            return true;
        }
    }
    return false;
}

uint32_t Controller::definitionCount() const noexcept
{
    return pImpl ? pImpl->definitionCount : 0u;
}

uint32_t Controller::eventCount() const noexcept
{
    return pImpl ? pImpl->eventCount : 0u;
}

bool Controller::eventAt(uint32_t index, Event& event) const noexcept
{
    if (!pImpl || index >= pImpl->eventCount) return false;
    event = pImpl->events[index];
    return true;
}

void Controller::clearEvents() noexcept
{
    if (pImpl) pImpl->eventCount = 0u;
}

Result Controller::modifyObject(const Object* object, float time, Mat4& model,
                                float& opacity, float& progress, void* data) noexcept
{
    auto controller = static_cast<Controller*>(data);
    if (!controller || !controller->pImpl || !object || !std::isfinite(time)
        || time < 0.0f) {
        return Result::InvalidArguments;
    }
    auto track = controller->pImpl->track(object->id());
    if (!track) return Result::Success;
    auto state = detail::retargetSample(track->motion,
                                        controller->pImpl->clock, 0.0,
                                        _interpolate);
    auto origin = model * state.origin;
    if (!_finite(origin)) return Result::InvalidArguments;
    model = _matrix(state, origin) * model;
    opacity = _clamp(opacity * state.opacity);
    progress = _clamp(progress * state.progress);
    return _finite(model) && std::isfinite(opacity) && std::isfinite(progress)
        ? Result::Success
        : Result::InvalidArguments;
}

}  // namespace vis::states
