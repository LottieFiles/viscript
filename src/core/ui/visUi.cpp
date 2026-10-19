// The UI module: buttons, sliders and sample areas on a panel, their input, and the retargeted
// visuals they show through runtime modifiers.
#include <cmath>
#include <new>

#include "vis_ui.h"
#include "../common/visControl.h"
#include "../common/visFinite.h"
#include "../common/visGrow.h"
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
           && transform.opacity >= 0.0f && transform.opacity <= 1.0f
           && transform.progress >= 0.0f && transform.progress <= 1.0f;
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

// Applies transform over model, opacity and progress; the renderer checks what comes out.
static void _compose(const UITransform& transform, Mat4& model, float& opacity,
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
}

// Takes event into pressed, the press state of a control in region, and returns what the
// control reports: a primary-button down in the region begins a press and captures the pointer,
// moves while it lasts are handled, and the up or a cancel ends it and lets the pointer go. A
// press redraws as it begins and ends; a disabled control takes only a cancel.
static InputResult _press(const InputEvent& event, const BBox& region, bool enabled,
                          bool& pressed) noexcept
{
    if (event.kind == InputKind::PointerCancel) {
        if (!pressed) return {};
        pressed = false;
        return {true, true, false, true};
    }
    if (!enabled) return {};
    if (event.kind == InputKind::PointerDown) {
        if (event.button != 0 || !contains(region, event.position)) return {};
        pressed = true;
        return {true, true, true, false};
    }
    if (!pressed) return {};
    if (event.kind == InputKind::PointerMove) return {true, false, false, false};
    if (event.kind != InputKind::PointerUp) return {};
    pressed = false;
    return {true, true, false, true};
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
    auto result = _press(event, uiRegion, uiEnabled, isPressed);
    // An up in the region activates the button.
    if (result.release && event.kind == InputKind::PointerUp
        && contains(uiRegion, event.position)) {
        if (toggleMode) toggleValue = !toggleValue;
        uiRevision++;
        if (callback) callback(this, event, callbackData);
    }
    return result;
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
    auto result = _press(event, uiRegion, uiEnabled, isDragging);
    // The value follows the pointer from the down to the up; a move redraws when it changes it.
    if (!result.handled || event.kind == InputKind::PointerCancel) return result;
    auto changed = update(event);
    if (event.kind == InputKind::PointerMove) result.redraw = changed;
    return result;
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
    auto result = _press(event, uiRegion, uiEnabled, isPressed);
    // The press does not show; an up in the region asks for a sample where it is.
    result.redraw = false;
    if (result.release && event.kind == InputKind::PointerUp
        && contains(uiRegion, event.position)) {
        requestPosition = event.position;
        requestValue = {
            clamp01((event.position.x - uiRegion.x) / uiRegion.width),
            clamp01((event.position.y - uiRegion.y) / uiRegion.height),
        };
        uiRevision++;
    }
    return result;
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
        // A slider's ends or a toggle's off and on; a button's transform is in to alone.
        UITransform from;
        UITransform to;
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

    // What an object is to the panel: the visual of a control, a face of a button's states, the
    // marker or swatch of a sample area, a candidate it samples, or the target of a transform.
    enum struct Role : uint8_t
    {
        Visual,
        Face,
        Output,
        Candidate,
        Target
    };

    ~Impl()
    {
        delete[] controls;
        delete[] bindings;
    }

    // Whether objects in roles first and second may not nest: visuals, faces, outputs and
    // candidates exclude one another, save that two visuals or two candidates may nest, and
    // transform targets exclude only outputs.
    static bool exclusive(Role first, Role second) noexcept
    {
        if (first == Role::Target || second == Role::Target) {
            return first == Role::Output || second == Role::Output;
        }
        return first != second || first == Role::Face || first == Role::Output;
    }

    // Whether object, in role, nests with an object that the controls and bindings hold in a
    // role that excludes it; except is a control whose own objects do not count.
    bool overlaps(const Object* object, Role role,
                  const UIObject* except = nullptr) const noexcept
    {
        auto clash = [object, role](const Object* held, Role part) {
            return exclusive(role, part) && family(object, held);
        };
        for (auto i = 0u; i < controlCount; i++) {
            if (controls[i] != except && clash(controls[i]->visual(), Role::Visual)) return true;
        }
        for (auto i = 0u; i < bindingCount; i++) {
            auto& binding = bindings[i];
            if (binding.control == except) continue;
            if (binding.kind == BindingKind::ButtonStates) {
                if (clash(binding.object, Role::Face) || clash(binding.hover, Role::Face)
                    || clash(binding.pressed, Role::Face)) {
                    return true;
                }
            } else if (binding.kind == BindingKind::SampleArea) {
                if (clash(binding.object, Role::Output) || clash(binding.swatch, Role::Output)) {
                    return true;
                }
                auto area = static_cast<const SampleArea*>(binding.control);
                for (auto j = 0u; j < area->targetCount(); j++) {
                    if (clash(area->targetAt(j), Role::Candidate)) return true;
                }
            } else if (clash(binding.object, Role::Target)) {
                return true;
            }
        }
        return false;
    }

    // Whether object is one of the scene's own.
    bool owned(const Object* object) const noexcept
    {
        return object && scene->object(object->id()) == object;
    }

    // Whether object can take a transform: the scene's own, nested with no sample output.
    bool bindable(const Object* object) const noexcept
    {
        return owned(object) && !overlaps(object, Role::Target);
    }

    Result add(const Binding& binding) noexcept
    {
        if (bindingCount >= BindingLimit) return Result::InsufficientCondition;
        if (!grow(bindings, bindingCount, bindingCapacity, 1u, 8u)) return Result::OutOfMemory;
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
    if (pImpl) pImpl->scene->runtimeRemove(this);
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
    if (scene->runtimeAdd(&modifier) == Result::Success
        && scene->add(panel) == Result::Success) {
        return panel;
    }
    delete panel;
    return nullptr;
}

Scene* Panel::scene() const noexcept
{
    return pImpl->scene;
}

// add() sets a control's panel as it lists the control.
bool Panel::registered(const UIObject* control) const noexcept
{
    return control && control->uiPanel == this;
}

Result Panel::add(UIObject* control) noexcept
{
    if (!control || control == this || control->uiPanel || !validRegion(control->region())
        || pImpl->overlaps(control->visual(), Impl::Role::Visual)) {
        return Result::InvalidArguments;
    }
    if (pImpl->controlCount >= ControlLimit) return Result::InsufficientCondition;
    if (!grow(pImpl->controls, pImpl->controlCount, pImpl->controlCapacity, 1u, 8u)) {
        return Result::OutOfMemory;
    }
    auto result = Group::add(control);
    if (result != Result::Success) return result;
    control->uiPanel = this;
    pImpl->controls[pImpl->controlCount++] = control;
    return Result::Success;
}

uint32_t Panel::count() const noexcept
{
    return pImpl->controlCount;
}

UIObject* Panel::controlAt(uint32_t index) const noexcept
{
    return index < pImpl->controlCount ? pImpl->controls[index] : nullptr;
}

void Panel::invalidate(UIObject* control) noexcept
{
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
    if (!_valid(event)) return {false, false, false, false, nullptr, Result::InvalidArguments};
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
                auto off = binding.kind == Impl::BindingKind::ButtonToggle
                        && !static_cast<Button*>(target)->toggleValue;
                detail::retargetSet(binding.transition, current);
                detail::retarget(binding.transition, off ? binding.from : binding.to, event.time,
                                 binding.duration, AnimCurve::preset(AnimCurvePreset::Smooth),
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
    if (!registered(control) || control->toggleMode
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
    if (!registered(control) || control->toggleMode || !_valid(transform)
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
    if (!registered(control) || !_valid(off) || !_valid(on)
        || !std::isfinite(duration) || duration < 0.0f || !pImpl->bindable(object)) {
        return Result::InvalidArguments;
    }
    // A toggle is all a button does, though it may still change faces.
    for (auto i = 0u; i < pImpl->bindingCount; i++) {
        auto& binding = pImpl->bindings[i];
        if (binding.control == control && binding.kind != Impl::BindingKind::ButtonStates) {
            return Result::InvalidArguments;
        }
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::ButtonToggle;
    binding.control = control;
    binding.object = object;
    binding.from = off;
    binding.to = on;
    detail::retargetSet(binding.transition, value ? on : off);
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
    if (!registered(control) || !_valid(from) || !_valid(to) || !pImpl->bindable(object)) {
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
    auto marker = sample.marker;
    auto swatch = sample.swatch;
    if (!registered(control) || !_finite(sample.markerOrigin) || !_finite(sample.markerX)
        || !_finite(sample.markerY) || family(marker, swatch)
        || (marker && marker->childCount() && marker->type() != Type::Group)
        || (swatch && (swatch->type() != Type::Rectangle || swatch->style.gradient.enabled()))) {
        return Result::InvalidArguments;
    }
    // A sample area takes one binding.
    for (auto i = 0u; i < pImpl->bindingCount; i++) {
        if (pImpl->bindings[i].control == control) return Result::InvalidArguments;
    }
    for (auto i = 0u; i < control->sampleTargetCount; i++) {
        auto target = control->sampleTargets[i];
        if (!pImpl->owned(target) || family(target, this) || family(target, marker)
            || family(target, swatch) || pImpl->overlaps(target, Impl::Role::Candidate)) {
            return Result::InvalidArguments;
        }
    }
    const Object* outputs[] = {marker, swatch};
    for (auto output : outputs) {
        if (output && (!pImpl->owned(output) || family(output, this)
                       || pImpl->overlaps(output, Impl::Role::Output))) {
            return Result::InvalidArguments;
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
    if (!callback && data) return Result::InvalidArguments;
    pImpl->sample = callback;
    pImpl->sampleData = data;
    return Result::Success;
}

Result Panel::states(Button* control, Object* hover, Object* pressed) noexcept
{
    auto normal = control ? control->visual() : nullptr;
    if (!registered(control) || !normal || (!hover && !pressed)) return Result::InvalidArguments;
    Object* faces[] = {normal, hover, pressed};
    for (auto i = 0u; i < 3u; i++) {
        if (!faces[i]) continue;
        if (!pImpl->owned(faces[i]) || pImpl->overlaps(faces[i], Impl::Role::Face, control)) {
            return Result::InvalidArguments;
        }
        for (auto j = i + 1u; j < 3u; j++) {
            if (family(faces[i], faces[j])) return Result::InvalidArguments;
        }
    }
    Impl::Binding binding;
    binding.kind = Impl::BindingKind::ButtonStates;
    binding.control = control;
    binding.object = normal;
    binding.hover = hover;
    binding.pressed = pressed;
    // The faces take the place of those the button had.
    for (auto i = 0u; i < pImpl->bindingCount; i++) {
        auto& current = pImpl->bindings[i];
        if (current.control == control && current.kind == binding.kind) {
            current = binding;
            return Result::Success;
        }
    }
    return pImpl->add(binding);
}

Result Panel::camera(CameraAction action, const Vec2& delta) noexcept
{
    return pImpl->camera.act(action, delta);
}

Result Panel::modifyObject(const Object* object, float time, Mat4& model, float& opacity,
                           float& progress, void* data) noexcept
{
    if (!std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    auto panel = static_cast<Panel*>(data);
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
            _compose(transform, model, opacity, progress);
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
        if ((binding.kind == Impl::BindingKind::ButtonObject
             || binding.kind == Impl::BindingKind::ButtonToggle)
            && binding.active) {
            auto transform = detail::retargetSample(binding.transition, time,
                                                    panel->pImpl->scene->duration(), _buttonLerp);
            _compose(transform, model, opacity, progress);
        } else if (binding.kind == Impl::BindingKind::SliderObject) {
            auto slider = static_cast<Slider*>(binding.control);
            _compose(_lerp(binding.from, binding.to, slider->value()), model, opacity, progress);
        }
    }
    return Result::Success;
}

Result Panel::modifyFill(const Object* object, float time, Color& fill, void* data) noexcept
{
    if (!std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    auto panel = static_cast<Panel*>(data);
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
    if (!std::isfinite(time) || time < 0.0f) return Result::InvalidArguments;
    return static_cast<Panel*>(data)->pImpl->camera.apply(camera, view);
}

Result Panel::modifyInput(const CameraInput& input, void* data) noexcept
{
    return static_cast<Panel*>(data)->camera(input.action, input.delta);
}

}  // namespace vis::ui
