// The UI module: buttons, sliders and sample areas on a panel, their input, and the retargeted
// visuals they show through runtime modifiers.
#include <cmath>
#include <new>

#include "vis_ui.h"
#include "../common/visControl.h"
#include "../common/visFinite.h"
#include "../common/visRetarget.h"

namespace vis::ui
{

namespace
{

uint8_t RuntimeKey = 0u;

static bool _valid(const InputEvent& event) noexcept
{
    return static_cast<uint8_t>(event.kind) <= static_cast<uint8_t>(InputKind::KeyDown)
           && _finite(event.position) && _finite(event.delta) && _finite(event.wheel)
           && std::isfinite(event.time) && event.time >= 0.0f;
}

static bool _valid(const UITransform& transform) noexcept
{
    return _finite(transform.shift) && _finite(transform.scale) && _finite(transform.rotation)
           && std::isfinite(transform.opacity) && transform.opacity >= 0.0f
           && std::isfinite(transform.progress) && transform.progress >= 0.0f;
}

static UITransform _lerp(const UITransform& from, const UITransform& to,
                         float progress) noexcept
{
    progress = clamp01(progress);
    UITransform output;
    output.shift = from.shift + (to.shift - from.shift) * progress;
    output.scale = from.scale + (to.scale - from.scale) * progress;
    output.rotation = from.rotation + (to.rotation - from.rotation) * progress;
    output.opacity = from.opacity + (to.opacity - from.opacity) * progress;
    output.progress = from.progress + (to.progress - from.progress) * progress;
    return output;
}

static UITransform _buttonLerp(const UITransform& from, const UITransform& to,
                               float progress) noexcept
{
    auto output = _lerp(from, to, progress);
    // A selected-state layer usually sits over the state it replaces.  Give
    // opacity a fast-out / late-in exchange curve so their labels do not stay
    // superimposed through the middle of an otherwise smooth target change.
    auto opacityProgress = progress;
    if (to.opacity < from.opacity) {
        auto inverse = 1.0f - progress;
        opacityProgress = 1.0f - inverse * inverse * inverse;
    } else if (to.opacity > from.opacity) {
        opacityProgress = progress * progress * progress;
    }
    output.opacity = from.opacity + (to.opacity - from.opacity) * opacityProgress;
    return output;
}

static bool _compose(const UITransform& transform, Mat4& model, float& opacity,
                     float& progress) noexcept
{
    auto local = Mat4::translate(transform.shift)
                 * Mat4::rotateZ(transform.rotation.z)
                 * Mat4::rotateY(transform.rotation.y)
                 * Mat4::rotateX(transform.rotation.x)
                 * Mat4::scale(transform.scale);
    model = local * model;
    opacity = clamp01(opacity * transform.opacity);
    progress = clamp01(progress * transform.progress);
    return _finite(model) && std::isfinite(opacity) && std::isfinite(progress);
}

}  // namespace

UIObject::UIObject(Object* visual, const BBox& region) noexcept
    : uiVisual(visual), uiRegion(region)
{
}

Object* UIObject::visual() const noexcept
{
    return uiVisual;
}

Result UIObject::visual(Object* object) noexcept
{
    if (object == this || uiPanel) return Result::InvalidArguments;
    uiVisual = object;
    return Result::Success;
}

const BBox& UIObject::region() const noexcept
{
    return uiRegion;
}

Result UIObject::region(const BBox& bounds) noexcept
{
    if (!validRegion(bounds)) return Result::InvalidArguments;
    uiRegion = bounds;
    return Result::Success;
}

bool UIObject::enabled() const noexcept
{
    return uiEnabled;
}

UIObject& UIObject::enabled(bool value) noexcept
{
    if (uiEnabled == value) return *this;
    if (!value) {
        resetInteraction();
        uiHovered = false;
        if (uiPanel) uiPanel->invalidate(this);
    }
    uiEnabled = value;
    return *this;
}

void UIObject::resetInteraction() noexcept
{
    InputEvent event;
    event.kind = InputKind::PointerCancel;
    input(event);
}

Button::Button(Object* visual, const BBox& region, Callback callback, void* data) noexcept
    : UIObject(visual, region), callback(callback), callbackData(data)
{
}

Button* Button::gen(Object* visual, const BBox& region, Callback callback,
                    void* data) noexcept
{
    if (!validRegion(region)) return nullptr;
    return new (std::nothrow) Button(visual, region, callback, data);
}

bool Button::pressed() const noexcept
{
    return isPressed;
}

bool Button::hovered() const noexcept
{
    return uiEnabled && uiHovered;
}

bool Button::toggled() const noexcept
{
    return toggleValue;
}

InputResult Button::input(const InputEvent& event) noexcept
{
    if (event.kind == InputKind::PointerCancel) {
        auto release = isPressed;
        isPressed = false;
        return {release, release, false, release};
    }
    if (!uiEnabled) return {};
    switch (event.kind) {
        case InputKind::PointerDown:
            if (event.button != 0 || !contains(uiRegion, event.position)) return {};
            isPressed = true;
            return {true, true, true, false};
        case InputKind::PointerMove:
            return isPressed ? InputResult{true, false, false, false} : InputResult{};
        case InputKind::PointerUp: {
            if (!isPressed) return {};
            isPressed = false;
            auto activate = contains(uiRegion, event.position);
            if (activate) {
                if (toggleMode) toggleValue = !toggleValue;
                uiRevision++;
                if (callback) callback(this, event, callbackData);
            }
            return {true, true, false, true};
        }
        case InputKind::PointerCancel: break;
        case InputKind::Wheel:
        case InputKind::KeyDown: break;
    }
    return {};
}

Slider::Slider(Object* visual, const BBox& region, SliderAxis axis, float value,
               Callback callback, void* data) noexcept
    : UIObject(visual, region), callback(callback), callbackData(data), sliderAxis(axis),
      sliderValue(clamp01(value))
{
}

Slider* Slider::gen(Object* visual, const BBox& region, SliderAxis axis, float value,
                    Callback callback, void* data) noexcept
{
    if (!validRegion(region)
        || static_cast<uint8_t>(axis) > static_cast<uint8_t>(SliderAxis::Vertical)
        || !std::isfinite(value)) {
        return nullptr;
    }
    return new (std::nothrow) Slider(visual, region, axis, value, callback, data);
}

SliderAxis Slider::axis() const noexcept
{
    return sliderAxis;
}

float Slider::value() const noexcept
{
    return sliderValue;
}

Slider& Slider::value(float value) noexcept
{
    if (!std::isfinite(value)) return *this;
    auto clamped = clamp01(value);
    if (clamped != sliderValue) {
        sliderValue = clamped;
        uiRevision++;
    }
    return *this;
}

bool Slider::dragging() const noexcept
{
    return isDragging;
}

bool Slider::update(const InputEvent& event) noexcept
{
    auto next = sliderAxis == SliderAxis::Horizontal
                    ? (event.position.x - uiRegion.x) / uiRegion.width
                    : 1.0f - (event.position.y - uiRegion.y) / uiRegion.height;
    next = clamp01(next);
    if (next == sliderValue) return false;
    sliderValue = next;
    uiRevision++;
    if (callback) callback(this, sliderValue, event, callbackData);
    return true;
}

InputResult Slider::input(const InputEvent& event) noexcept
{
    if (event.kind == InputKind::PointerCancel) {
        if (!isDragging) return {};
        isDragging = false;
        return {true, true, false, true};
    }
    if (!uiEnabled) return {};
    switch (event.kind) {
        case InputKind::PointerDown:
            if (event.button != 0 || !contains(uiRegion, event.position)) return {};
            isDragging = true;
            update(event);
            return {true, true, true, false};
        case InputKind::PointerMove:
            if (!isDragging) return {};
            return {true, update(event), false, false};
        case InputKind::PointerUp:
            if (!isDragging) return {};
            update(event);
            isDragging = false;
            return {true, true, false, true};
        case InputKind::PointerCancel: break;
        case InputKind::Wheel:
        case InputKind::KeyDown: break;
    }
    return {};
}

SampleArea::SampleArea(const Object* const* targets, uint32_t count,
                       const BBox& region) noexcept
    : UIObject(nullptr, region), sampleTargetCount(count)
{
    sampleTargets = new (std::nothrow) const Object*[count];
    if (!sampleTargets) return;
    for (auto i = 0u; i < count; i++) sampleTargets[i] = targets[i];
}

SampleArea::~SampleArea()
{
    delete[] sampleTargets;
}

SampleArea* SampleArea::gen(const Object* const* targets, uint32_t count,
                            const BBox& region) noexcept
{
    if (!targets || !count || count > TargetLimit || !validRegion(region)) return nullptr;
    for (auto i = 0u; i < count; i++) {
        if (!targets[i]) return nullptr;
        for (auto j = 0u; j < i; j++) {
            if (family(targets[i], targets[j])) return nullptr;
        }
    }
    auto area = new (std::nothrow) SampleArea(targets, count, region);
    if (area && area->sampleTargets) return area;
    delete area;
    return nullptr;
}

uint32_t SampleArea::targetCount() const noexcept
{
    return sampleTargetCount;
}

const Object* SampleArea::targetAt(uint32_t index) const noexcept
{
    return index < sampleTargetCount ? sampleTargets[index] : nullptr;
}

bool SampleArea::pressed() const noexcept
{
    return isPressed;
}

bool SampleArea::selected() const noexcept
{
    return hasSelection;
}

const Vec2& SampleArea::position() const noexcept
{
    return samplePosition;
}

const Vec2& SampleArea::value() const noexcept
{
    return sampleValue;
}

const PixelSample& SampleArea::selection() const noexcept
{
    return sampleSelection;
}

void SampleArea::resolve(const PixelSample& sample) noexcept
{
    samplePosition = requestPosition;
    sampleValue = requestValue;
    sampleSelection = sample;
    sampleSelection.position = requestPosition;
    hasSelection = sample.object != nullptr;
}

InputResult SampleArea::input(const InputEvent& event) noexcept
{
    if (event.kind == InputKind::PointerCancel) {
        auto release = isPressed;
        isPressed = false;
        return {release, false, false, release};
    }
    if (!uiEnabled) return {};
    switch (event.kind) {
        case InputKind::PointerDown:
            if (event.button != 0 || !contains(uiRegion, event.position)) return {};
            isPressed = true;
            return {true, false, true, false};
        case InputKind::PointerMove:
            return isPressed ? InputResult{true, false, false, false} : InputResult{};
        case InputKind::PointerUp:
            if (!isPressed) return {};
            isPressed = false;
            if (contains(uiRegion, event.position)) {
                requestPosition = event.position;
                requestValue = {
                    clamp01((event.position.x - uiRegion.x) / uiRegion.width),
                    clamp01((event.position.y - uiRegion.y) / uiRegion.height),
                };
                uiRevision++;
            }
            return {true, false, false, true};
        case InputKind::PointerCancel: break;
        case InputKind::Wheel:
        case InputKind::KeyDown: break;
    }
    return {};
}

struct Panel::Impl
{
    enum struct BindingKind : uint8_t
    {
        ButtonCamera,
        ButtonObject,
        ButtonToggle,
        SliderObject,
        ButtonStates,
        SampleArea
    };

    struct Binding
    {
        BindingKind kind = BindingKind::ButtonCamera;
        UIObject* control = nullptr;
        Object* object = nullptr;
        Object* hover = nullptr;
        Object* pressed = nullptr;
        Object* swatch = nullptr;
        CameraAction action = CameraAction::Pan;
        Vec2 delta;
        UITransform from;
        UITransform to;
        UITransform off;
        UITransform on;
        detail::Retarget<UITransform> transition;
        float duration = 0.0f;
        Vec3 markerOrigin;
        Vec3 markerX;
        Vec3 markerY;
        bool active = false;
    };

    Scene* scene = nullptr;
    UIObject** controls = nullptr;
    Binding* bindings = nullptr;
    uint32_t controlCount = 0;
    uint32_t controlCapacity = 0;
    uint32_t bindingCount = 0;
    uint32_t bindingCapacity = 0;
    UIObject* captured = nullptr;
    UIObject* hovered = nullptr;
    uint32_t capturedPointer = 0;
    bool pendingRedraw = false;
    bool pendingRelease = false;
    CameraDrive camera;
    SampleCallback sample = nullptr;
    void* sampleData = nullptr;

    ~Impl()
    {
        delete[] controls;
        delete[] bindings;
    }

    bool growControls() noexcept
    {
        if (controlCount < controlCapacity) return true;
        auto capacity = controlCapacity ? controlCapacity * 2u : 8u;
        if (capacity < controlCapacity) return false;
        auto grown = new (std::nothrow) UIObject*[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < controlCount; i++) grown[i] = controls[i];
        delete[] controls;
        controls = grown;
        controlCapacity = capacity;
        return true;
    }

    bool growBindings() noexcept
    {
        if (bindingCount < bindingCapacity) return true;
        auto capacity = bindingCapacity ? bindingCapacity * 2u : 8u;
        if (capacity < bindingCapacity) return false;
        auto grown = new (std::nothrow) Binding[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < bindingCount; i++) grown[i] = bindings[i];
        delete[] bindings;
        bindings = grown;
        bindingCapacity = capacity;
        return true;
    }

    // Whether object can take a transform: the scene's own, nested with no sample marker or
    // swatch.
    bool bindable(const Object* object) const noexcept
    {
        if (!object || !scene || scene->object(object->id()) != object) return false;
        for (auto i = 0u; i < bindingCount; i++) {
            auto& binding = bindings[i];
            if (binding.kind == BindingKind::SampleArea
                && (family(object, binding.object) || family(object, binding.swatch))) {
                return false;
            }
        }
        return true;
    }

    Result add(const Binding& binding) noexcept
    {
        if (bindingCount >= BindingLimit) return Result::InsufficientCondition;
        if (!growBindings()) return Result::OutOfMemory;
        bindings[bindingCount++] = binding;
        return Result::Success;
    }
};

Panel::Panel(Scene* scene) noexcept : UIObject(nullptr, {}), pImpl(new (std::nothrow) Impl)
{
    if (pImpl) pImpl->scene = scene;
}

Panel::~Panel()
{
    if (pImpl && pImpl->scene) pImpl->scene->runtimeRemove(this);
    delete pImpl;
}

Panel* Panel::gen(Scene* scene) noexcept
{
    if (!scene) return nullptr;
    auto panel = new (std::nothrow) Panel(scene);
    if (!panel || !panel->pImpl) {
        delete panel;
        return nullptr;
    }
    RuntimeModifier modifier;
    modifier.object = modifyObject;
    modifier.fill = modifyFill;
    modifier.camera = modifyCamera;
    modifier.input = modifyInput;
    modifier.data = panel;
    modifier.key = &RuntimeKey;
    if (scene->runtimeAdd(&modifier) != Result::Success) {
        delete panel;
        return nullptr;
    }
    if (scene->add(panel) == Result::Success) return panel;
    scene->runtimeRemove(panel);
    delete panel;
    return nullptr;
}

Scene* Panel::scene() const noexcept
{
    return pImpl ? pImpl->scene : nullptr;
}

bool Panel::registered(const UIObject* control) const noexcept
{
    if (!pImpl || !control) return false;
    for (auto i = 0u; i < pImpl->controlCount; i++) {
        if (pImpl->controls[i] == control) return true;
    }
    return false;
}

Result Panel::add(UIObject* control) noexcept
{
    if (!pImpl || !control || control == this || control->uiPanel || registered(control)
        || !validRegion(control->region())) {
        return Result::InvalidArguments;
    }
    if (control->visual()) {
        for (auto i = 0u; i < pImpl->bindingCount; i++) {
            auto& binding = pImpl->bindings[i];
            if (binding.kind == Impl::BindingKind::ButtonStates) {
                if (family(control->visual(), binding.object)
                    || family(control->visual(), binding.hover)
                    || family(control->visual(), binding.pressed)) {
                    return Result::InvalidArguments;
                }
            } else if (binding.kind == Impl::BindingKind::SampleArea) {
                if (family(control->visual(), binding.object)
                    || family(control->visual(), binding.swatch)) {
                    return Result::InvalidArguments;
                }
                auto area = static_cast<SampleArea*>(binding.control);
                for (auto j = 0u; j < area->sampleTargetCount; j++) {
                    if (family(control->visual(), area->sampleTargets[j])) {
                        return Result::InvalidArguments;
                    }
                }
            }
        }
    }
    if (pImpl->controlCount >= ControlLimit) return Result::InsufficientCondition;
    if (!pImpl->growControls()) return Result::OutOfMemory;
    auto result = Group::add(control);
    if (result != Result::Success) return result;
    control->uiPanel = this;
    pImpl->controls[pImpl->controlCount++] = control;
    return Result::Success;
}

uint32_t Panel::count() const noexcept
{
    return pImpl ? pImpl->controlCount : 0u;
}

UIObject* Panel::controlAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->controlCount ? pImpl->controls[index] : nullptr;
}

void Panel::invalidate(UIObject* control) noexcept
{
    if (!pImpl || !control) return;
    auto changed = false;
    if (pImpl->hovered == control) {
        pImpl->hovered = nullptr;
        changed = true;
    }
    if (pImpl->captured == control) {
        pImpl->captured = nullptr;
        pImpl->capturedPointer = 0;
        pImpl->pendingRelease = true;
        changed = true;
    }
    if (changed) pImpl->pendingRedraw = true;
}

void Panel::resetInteraction() noexcept
{
    if (!pImpl) return;
    auto captured = pImpl->captured;
    auto hovered = pImpl->hovered;
    if (captured) {
        captured->resetInteraction();
        captured->uiHovered = false;
        pImpl->pendingRelease = true;
    }
    if (hovered && hovered != captured) {
        hovered->resetInteraction();
        hovered->uiHovered = false;
    }
    if (captured || hovered) pImpl->pendingRedraw = true;
    pImpl->captured = nullptr;
    pImpl->hovered = nullptr;
    pImpl->capturedPointer = 0;
}

InputResult Panel::input(const InputEvent& event) noexcept
{
    if (!pImpl || !_valid(event)) return {};
    InputResult result;
    auto consumePending = [this, &result]() {
        result.handled = result.handled || pImpl->pendingRelease;
        result.redraw = result.redraw || pImpl->pendingRedraw;
        result.release = result.release || pImpl->pendingRelease;
        pImpl->pendingRedraw = false;
        pImpl->pendingRelease = false;
    };
    consumePending();
    if (result.release) return result;
    auto merge = [&result](const InputResult& input) {
        result.handled = result.handled || input.handled;
        result.redraw = result.redraw || input.redraw;
        result.capture = result.capture || input.capture;
        result.release = result.release || input.release;
        if (input.sample) result.sample = input.sample;
        if (input.status != Result::Success) result.status = input.status;
    };
    auto hit = [this, &event]() -> UIObject* {
        for (auto i = pImpl->controlCount; i > 0u; i--) {
            auto candidate = pImpl->controls[i - 1u];
            if (candidate->enabled() && contains(candidate->region(), event.position)) {
                return candidate;
            }
        }
        return nullptr;
    };
    auto hover = [this, &result](UIObject* target) {
        if (pImpl->hovered == target) return;
        if (pImpl->hovered) pImpl->hovered->uiHovered = false;
        pImpl->hovered = target;
        if (target) target->uiHovered = true;
        result.redraw = true;
    };
    auto activate = [this, &result, &event](UIObject* target) {
        for (auto i = 0u; i < pImpl->bindingCount; i++) {
            auto& binding = pImpl->bindings[i];
            if (binding.control != target) continue;
            if (binding.kind == Impl::BindingKind::ButtonCamera) {
                if (camera(binding.action, binding.delta) == Result::Success) {
                    result.redraw = true;
                }
            } else if (binding.kind == Impl::BindingKind::ButtonObject
                       || binding.kind == Impl::BindingKind::ButtonToggle) {
                UITransform current;
                for (auto j = 0u; j < pImpl->bindingCount; j++) {
                    auto& candidate = pImpl->bindings[j];
                    if ((candidate.kind == Impl::BindingKind::ButtonObject
                        || candidate.kind == Impl::BindingKind::ButtonToggle)
                        && candidate.object == binding.object) {
                        if (candidate.active) {
                            current = detail::retargetSample(
                                candidate.transition, event.time,
                                pImpl->scene->duration(), _buttonLerp);
                        }
                        candidate.active = false;
                    }
                }
                binding.from = current;
                if (binding.kind == Impl::BindingKind::ButtonToggle) {
                    auto button = static_cast<Button*>(target);
                    binding.to = button->toggleValue ? binding.on : binding.off;
                }
                detail::retargetSet(binding.transition, current);
                detail::retarget(binding.transition, binding.to, event.time,
                                 binding.duration,
                                 AnimCurve::preset(AnimCurvePreset::Smooth),
                                 pImpl->scene->duration(), _buttonLerp);
                binding.active = true;
                result.redraw = true;
            } else if (binding.kind == Impl::BindingKind::SampleArea) {
                auto area = static_cast<SampleArea*>(target);
                if (!pImpl->sample) continue;
                PixelSample sample;
                auto resolved = pImpl->sample(pImpl->scene, event.time,
                                              area->requestPosition,
                                              area->sampleTargets,
                                              area->sampleTargetCount,
                                              sample, pImpl->sampleData);
                if (resolved == Result::InsufficientCondition) continue;
                if (resolved != Result::Success) {
                    result.status = resolved;
                    continue;
                }
                if (!sample.object) {
                    result.status = Result::InvalidArguments;
                    continue;
                }
                auto candidate = false;
                for (auto j = 0u; j < area->sampleTargetCount; j++) {
                    if (sample.object == area->sampleTargets[j]) {
                        candidate = true;
                        break;
                    }
                }
                if (!candidate) {
                    result.status = Result::InvalidArguments;
                    continue;
                }
                area->resolve(sample);
                result.sample = area;
                result.redraw = true;
            }
        }
    };
    // Gives target this event, then runs the bindings of a control the event changed.
    auto send = [&event, &merge, &activate, &consumePending](UIObject* target) {
        auto revision = target->uiRevision;
        merge(target->input(event));
        if (target->uiRevision != revision) activate(target);
        consumePending();
    };
    // Ends target's interaction with this event turned into a cancel.
    auto cancel = [&event, &merge](UIObject* target) {
        auto cancelled = event;
        cancelled.kind = InputKind::PointerCancel;
        merge(target->input(cancelled));
    };
    // Lets the captured control go; a forced release is the panel's to report.
    auto release = [this, &result](bool forced) {
        pImpl->captured = nullptr;
        pImpl->capturedPointer = 0;
        result.capture = false;
        if (!forced) return;
        result.handled = true;
        result.release = true;
    };

    if (!uiEnabled) {
        if (pImpl->captured) {
            cancel(pImpl->captured);
            release(true);
        } else if (pImpl->hovered) {
            cancel(pImpl->hovered);
        }
        hover(nullptr);
        return result;
    }

    if (pImpl->captured) {
        if (event.pointer != pImpl->capturedPointer) return result;
        auto target = pImpl->captured;
        if (!target->enabled()) {
            cancel(target);
            release(true);
            hover(nullptr);
            return result;
        }
        if (event.kind == InputKind::PointerMove) {
            hover(contains(target->region(), event.position) ? target : nullptr);
        }
        send(target);
        if (result.capture) pImpl->capturedPointer = event.pointer;
        if (result.release || event.kind == InputKind::PointerCancel) {
            release(event.kind == InputKind::PointerCancel);
            hover(event.kind == InputKind::PointerUp ? hit() : nullptr);
        }
        return result;
    }

    if (event.kind != InputKind::PointerMove && event.kind != InputKind::PointerDown
        && event.kind != InputKind::PointerCancel) {
        return result;
    }
    // A cancel leaves the hovered control as a move onto no control does.
    auto target = event.kind == InputKind::PointerCancel ? nullptr : hit();
    if (pImpl->hovered && pImpl->hovered != target) cancel(pImpl->hovered);
    hover(target);
    if (!target) return result;

    send(target);
    if (!target->enabled() && result.capture) {
        result.handled = true;
        result.capture = false;
        result.release = true;
    }
    if (result.capture) {
        pImpl->captured = target;
        pImpl->capturedPointer = event.pointer;
    }
    return result;
}

Result Panel::bind(Button* control, CameraAction action, const Vec2& delta) noexcept
{
    if (!pImpl || !registered(control) || control->toggleMode
        || static_cast<uint8_t>(action) > static_cast<uint8_t>(CameraAction::View3D)
        || !_finite(delta)) {
        return Result::InvalidArguments;
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::ButtonCamera;
    binding.control = control;
    binding.action = action;
    binding.delta = delta;
    return pImpl->add(binding);
}

Result Panel::bind(Button* control, Object* object, const UITransform& transform,
                   float duration) noexcept
{
    if (!pImpl || !registered(control) || control->toggleMode || !_valid(transform)
        || !std::isfinite(duration) || duration < 0.0f || !pImpl->bindable(object)) {
        return Result::InvalidArguments;
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::ButtonObject;
    binding.control = control;
    binding.object = object;
    binding.to = transform;
    binding.duration = duration;
    return pImpl->add(binding);
}

Result Panel::toggle(Button* control, Object* object, const UITransform& off,
                     const UITransform& on, bool value, float duration) noexcept
{
    if (!pImpl || !registered(control) || !_valid(off) || !_valid(on)
        || !std::isfinite(duration) || duration < 0.0f || !pImpl->bindable(object)) {
        return Result::InvalidArguments;
    }
    for (auto i = 0u; i < pImpl->bindingCount; i++) {
        auto& binding = pImpl->bindings[i];
        if (binding.control == control
            && (binding.kind == Impl::BindingKind::ButtonCamera
                || binding.kind == Impl::BindingKind::ButtonObject
                || binding.kind == Impl::BindingKind::ButtonToggle)) {
            return Result::InvalidArguments;
        }
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::ButtonToggle;
    binding.control = control;
    binding.object = object;
    binding.off = off;
    binding.on = on;
    binding.from = value ? on : off;
    binding.to = binding.from;
    detail::retargetSet(binding.transition, binding.from);
    binding.duration = duration;
    binding.active = true;
    auto result = pImpl->add(binding);
    if (result != Result::Success) return result;
    // The toggle shows its object from now on, over the bindings before it.
    for (auto i = 0u; i + 1u < pImpl->bindingCount; i++) {
        auto& other = pImpl->bindings[i];
        if ((other.kind == Impl::BindingKind::ButtonObject
            || other.kind == Impl::BindingKind::ButtonToggle)
            && other.object == object) {
            other.active = false;
        }
    }
    control->toggleMode = true;
    control->toggleValue = value;
    return Result::Success;
}

Result Panel::bind(Slider* control, Object* object, const UITransform& from,
                   const UITransform& to) noexcept
{
    if (!pImpl || !registered(control) || !_valid(from) || !_valid(to)
        || !pImpl->bindable(object)) {
        return Result::InvalidArguments;
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::SliderObject;
    binding.control = control;
    binding.object = object;
    binding.from = from;
    binding.to = to;
    return pImpl->add(binding);
}

Result Panel::bind(SampleArea* control, const SampleBinding& sample) noexcept
{
    if (!pImpl || !registered(control) || !pImpl->scene
        || !_finite(sample.markerOrigin) || !_finite(sample.markerX)
        || !_finite(sample.markerY)) {
        return Result::InvalidArguments;
    }
    auto owned = [this](const Object* object) {
        return object && pImpl->scene->object(object->id()) == object;
    };
    for (auto i = 0u; i < control->sampleTargetCount; i++) {
        auto target = control->sampleTargets[i];
        if (!owned(target) || family(target, this)
            || family(target, sample.marker) || family(target, sample.swatch)) {
            return Result::InvalidArguments;
        }
    }
    if ((sample.marker && (!owned(sample.marker) || family(sample.marker, this)
                           || (sample.marker->childCount()
                               && sample.marker->type() != Type::Group)))
        || (sample.swatch && (!owned(sample.swatch)
                              || family(sample.swatch, this)
                              || sample.swatch->type() != Type::Rectangle
                              || sample.swatch->style.gradient.enabled()))
        || family(sample.marker, sample.swatch)) {
        return Result::InvalidArguments;
    }
    Object* faces[] = {sample.marker, sample.swatch};
    for (auto i = 0u; i < pImpl->controlCount; i++) {
        auto other = pImpl->controls[i];
        if (!other->visual()) continue;
        for (auto j = 0u; j < control->sampleTargetCount; j++) {
            if (family(control->sampleTargets[j], other->visual())) {
                return Result::InvalidArguments;
            }
        }
        for (auto face : faces) {
            if (family(face, other->visual())) return Result::InvalidArguments;
        }
    }
    for (auto i = 0u; i < pImpl->bindingCount; i++) {
        auto& binding = pImpl->bindings[i];
        if (binding.control == control && binding.kind == Impl::BindingKind::SampleArea) {
            return Result::InvalidArguments;
        }
        Object* bound[] = {binding.object, binding.hover, binding.pressed, binding.swatch};
        if (binding.kind == Impl::BindingKind::ButtonStates) {
            for (auto j = 0u; j < control->sampleTargetCount; j++) {
                for (auto object : bound) {
                    if (family(control->sampleTargets[j], object)) {
                        return Result::InvalidArguments;
                    }
                }
            }
        }
        for (auto face : faces) {
            for (auto object : bound) {
                if (family(face, object)) return Result::InvalidArguments;
            }
        }
        if (binding.kind == Impl::BindingKind::SampleArea) {
            auto area = static_cast<SampleArea*>(binding.control);
            for (auto j = 0u; j < control->sampleTargetCount; j++) {
                if (family(control->sampleTargets[j], binding.object)
                    || family(control->sampleTargets[j], binding.swatch)) {
                    return Result::InvalidArguments;
                }
            }
            for (auto j = 0u; j < area->sampleTargetCount; j++) {
                for (auto face : faces) {
                    if (family(face, area->sampleTargets[j])) {
                        return Result::InvalidArguments;
                    }
                }
            }
        }
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::SampleArea;
    binding.control = control;
    binding.object = sample.marker;
    binding.swatch = sample.swatch;
    binding.markerOrigin = sample.markerOrigin;
    binding.markerX = sample.markerX;
    binding.markerY = sample.markerY;
    return pImpl->add(binding);
}

Result Panel::sampler(SampleCallback callback, void* data) noexcept
{
    if (!pImpl || (!callback && data)) return Result::InvalidArguments;
    pImpl->sample = callback;
    pImpl->sampleData = data;
    return Result::Success;
}

Result Panel::states(Button* control, Object* hover, Object* pressed) noexcept
{
    auto normal = control ? control->visual() : nullptr;
    if (!pImpl || !registered(control) || !normal || (!hover && !pressed)
        || !pImpl->scene) {
        return Result::InvalidArguments;
    }
    Object* faces[] = {normal, hover, pressed};
    for (auto i = 0u; i < 3u; i++) {
        if (!faces[i]) continue;
        if (pImpl->scene->object(faces[i]->id()) != faces[i]) {
            return Result::InvalidArguments;
        }
        for (auto j = i + 1u; j < 3u; j++) {
            if (family(faces[i], faces[j])) return Result::InvalidArguments;
        }
    }
    for (auto i = 0u; i < pImpl->controlCount; i++) {
        auto other = pImpl->controls[i];
        if (other == control || !other->visual()) continue;
        for (auto face : faces) {
            if (family(face, other->visual())) return Result::InvalidArguments;
        }
    }
    Impl::Binding* current = nullptr;
    for (auto i = 0u; i < pImpl->bindingCount; i++) {
        auto& binding = pImpl->bindings[i];
        if (binding.kind == Impl::BindingKind::SampleArea) {
            auto area = static_cast<SampleArea*>(binding.control);
            for (auto face : faces) {
                if (family(face, binding.object) || family(face, binding.swatch)) {
                    return Result::InvalidArguments;
                }
                for (auto j = 0u; j < area->sampleTargetCount; j++) {
                    if (family(face, area->sampleTargets[j])) {
                        return Result::InvalidArguments;
                    }
                }
            }
            continue;
        }
        if (binding.kind != Impl::BindingKind::ButtonStates) continue;
        if (binding.control == control) {
            current = &binding;
            continue;
        }
        Object* otherFaces[] = {binding.object, binding.hover, binding.pressed};
        for (auto face : faces) {
            for (auto other : otherFaces) {
                if (family(face, other)) return Result::InvalidArguments;
            }
        }
    }
    if (current) {
        current->object = normal;
        current->hover = hover;
        current->pressed = pressed;
        return Result::Success;
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::ButtonStates;
    binding.control = control;
    binding.object = normal;
    binding.hover = hover;
    binding.pressed = pressed;
    return pImpl->add(binding);
}

Result Panel::camera(CameraAction action, const Vec2& delta) noexcept
{
    return pImpl ? pImpl->camera.act(action, delta) : Result::InvalidArguments;
}

Result Panel::cameraMove(const Vec2& delta) noexcept
{
    return camera(CameraAction::Pan, delta);
}

Result Panel::cameraOrbit(const Vec2& delta) noexcept
{
    return camera(CameraAction::Orbit, delta);
}

Result Panel::cameraZoom(float delta) noexcept
{
    return camera(CameraAction::Zoom, {0.0f, delta});
}

Result Panel::cameraReset() noexcept
{
    return camera(CameraAction::Reset, {});
}

Result Panel::cameraView(CameraView view) noexcept
{
    return pImpl ? pImpl->camera.view(view) : Result::InvalidArguments;
}

Result Panel::modifyObject(const Object* object, float time, Mat4& model, float& opacity,
                           float& progress, void* data) noexcept
{
    auto panel = static_cast<Panel*>(data);
    if (!panel || !panel->pImpl || !object || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    for (auto i = 0u; i < panel->pImpl->bindingCount; i++) {
        auto& binding = panel->pImpl->bindings[i];
        if (binding.kind == Impl::BindingKind::SampleArea) {
            if (binding.object != object) continue;
            auto area = static_cast<SampleArea*>(binding.control);
            if (!area->selected()) {
                opacity = 0.0f;
                continue;
            }
            UITransform transform;
            transform.shift = binding.markerOrigin
                            + binding.markerX * area->value().x
                            + binding.markerY * area->value().y;
            if (!_compose(transform, model, opacity, progress)) {
                return Result::InvalidArguments;
            }
            continue;
        }
        if (binding.kind == Impl::BindingKind::ButtonStates) {
            if (binding.object != object && binding.hover != object
                && binding.pressed != object) {
                continue;
            }
            auto button = static_cast<Button*>(binding.control);
            auto active = binding.object;
            if (panel->uiEnabled && button->hovered() && binding.hover) {
                active = binding.hover;
            }
            if (panel->uiEnabled && button->hovered() && button->pressed()
                && binding.pressed) {
                active = binding.pressed;
            }
            if (object != active) opacity = 0.0f;
            continue;
        }
        if (binding.object != object) continue;
        if (binding.kind == Impl::BindingKind::ButtonObject
            || binding.kind == Impl::BindingKind::ButtonToggle) {
            if (binding.active) {
                auto transform = detail::retargetSample(
                    binding.transition, time, panel->pImpl->scene->duration(),
                    _buttonLerp);
                if (!_compose(transform, model, opacity, progress)) {
                    return Result::InvalidArguments;
                }
            }
        } else if (binding.kind == Impl::BindingKind::SliderObject) {
            auto slider = static_cast<Slider*>(binding.control);
            if (!_compose(_lerp(binding.from, binding.to, slider->value()), model, opacity,
                          progress)) {
                return Result::InvalidArguments;
            }
        }
    }
    return Result::Success;
}

Result Panel::modifyFill(const Object* object, float time, Color& fill, void* data) noexcept
{
    auto panel = static_cast<Panel*>(data);
    if (!panel || !panel->pImpl || !object || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    for (auto i = 0u; i < panel->pImpl->bindingCount; i++) {
        auto& binding = panel->pImpl->bindings[i];
        if (binding.kind != Impl::BindingKind::SampleArea || binding.swatch != object) {
            continue;
        }
        auto area = static_cast<SampleArea*>(binding.control);
        if (area->selected()) fill = area->selection().color;
    }
    return Result::Success;
}

Result Panel::modifyCamera(float time, Camera& camera, CameraView& view, void* data) noexcept
{
    auto panel = static_cast<Panel*>(data);
    if (!panel || !panel->pImpl || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    return panel->pImpl->camera.apply(camera, view);
}

Result Panel::modifyInput(const CameraInput& input, void* data) noexcept
{
    auto panel = static_cast<Panel*>(data);
    return panel ? panel->camera(input.action, input.delta) : Result::InvalidArguments;
}

}  // namespace vis::ui
