#ifndef _VIS_UI_H_
#define _VIS_UI_H_

#include "vis.h"

namespace vis::ui
{

enum struct InputKind : uint8_t
{
    PointerDown = 0,
    PointerMove,
    PointerUp,
    PointerCancel,
    Wheel,
    KeyDown
};

enum struct SliderAxis : uint8_t
{
    Horizontal = 0,
    Vertical
};

struct SampleArea;

struct InputEvent
{
    InputKind kind = InputKind::PointerMove;
    uint32_t pointer = 0;
    Vec2 position;
    Vec2 delta;
    Vec2 wheel;
    int32_t button = -1;
    uint32_t buttons = 0;
    uint32_t modifiers = 0;
    float time = 0.0f;
};

struct InputResult
{
    bool handled = false;
    bool redraw = false;
    bool capture = false;
    bool release = false;
    const SampleArea* sample = nullptr;
    Result status = Result::Success;
};

// Finite transforms; rotation is radians and opacity/progress are in [0, 1].
struct UITransform
{
    Vec3 shift;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    Vec3 rotation;
    float opacity = 1.0f;
    float progress = 1.0f;
};

struct SampleBinding
{
    Object* marker = nullptr;
    Object* swatch = nullptr;
    Vec3 markerOrigin;
    Vec3 markerX;
    Vec3 markerY;
};

struct Panel;

struct UIObject : Group
{
    Object* visual() const noexcept;
    // Registered visual identity is immutable; control faces are exclusive and non-nested.
    Result visual(Object* object) noexcept;
    const BBox& region() const noexcept;
    Result region(const BBox& bounds) noexcept;
    bool enabled() const noexcept;
    UIObject& enabled(bool value) noexcept;

protected:
    UIObject(Object* visual, const BBox& region) noexcept;
    virtual InputResult input(const InputEvent& event) noexcept = 0;
    virtual void resetInteraction() noexcept;

    Object* uiVisual = nullptr;
    BBox uiRegion;
    uint32_t uiRevision = 0;
    bool uiEnabled = true;
    bool uiHovered = false;
    Panel* uiPanel = nullptr;
    friend struct Panel;
};

struct Button final : UIObject
{
    using Callback = void (*)(Button* button, const InputEvent& event, void* data) noexcept;

    static Button* gen(Object* visual, const BBox& region, Callback callback = nullptr,
                       void* data = nullptr) noexcept;
    bool hovered() const noexcept;
    bool pressed() const noexcept;
    bool toggled() const noexcept;

private:
    Button(Object* visual, const BBox& region, Callback callback, void* data) noexcept;
    InputResult input(const InputEvent& event) noexcept override;

    Callback callback = nullptr;
    void* callbackData = nullptr;
    bool isPressed = false;
    bool toggleMode = false;
    bool toggleValue = false;
    friend struct Panel;
};

struct Slider final : UIObject
{
    using Callback = void (*)(Slider* slider, float value, const InputEvent& event,
                              void* data) noexcept;

    // Values clamp to [0, 1]; drag capture persists until release/cancellation.
    static Slider* gen(Object* visual, const BBox& region,
                       SliderAxis axis = SliderAxis::Horizontal, float value = 0.0f,
                       Callback callback = nullptr, void* data = nullptr) noexcept;
    SliderAxis axis() const noexcept;
    float value() const noexcept;
    Slider& value(float value) noexcept;
    bool dragging() const noexcept;

private:
    Slider(Object* visual, const BBox& region, SliderAxis axis, float value,
           Callback callback, void* data) noexcept;
    InputResult input(const InputEvent& event) noexcept override;
    bool update(const InputEvent& event) noexcept;

    Callback callback = nullptr;
    void* callbackData = nullptr;
    SliderAxis sliderAxis = SliderAxis::Horizontal;
    float sliderValue = 0.0f;
    bool isDragging = false;
};

struct SampleArea final : UIObject
{
    static constexpr uint32_t TargetLimit = 64u;

    ~SampleArea() override;
    // Requires 1..64 distinct, non-nested candidates in the root scene.
    static SampleArea* gen(const Object* const* targets, uint32_t count,
                           const BBox& region) noexcept;
    uint32_t targetCount() const noexcept;
    const Object* targetAt(uint32_t index) const noexcept;
    bool pressed() const noexcept;
    bool selected() const noexcept;
    const Vec2& position() const noexcept;
    const Vec2& value() const noexcept;
    // Selection updates only after a primary press/release inside the region.
    // Picks the topmost nonzero-alpha candidate at backing density; a miss keeps prior state.
    const PixelSample& selection() const noexcept;

private:
    SampleArea(const Object* const* targets, uint32_t count, const BBox& region) noexcept;
    InputResult input(const InputEvent& event) noexcept override;
    void resolve(const PixelSample& sample) noexcept;

    const Object** sampleTargets = nullptr;
    uint32_t sampleTargetCount = 0u;
    Vec2 requestPosition;
    Vec2 requestValue;
    Vec2 samplePosition;
    Vec2 sampleValue;
    PixelSample sampleSelection;
    bool isPressed = false;
    bool hasSelection = false;
    friend struct Panel;
};

struct Panel final : UIObject
{
    static constexpr uint32_t ControlLimit = 256u;
    static constexpr uint32_t BindingLimit = 256u;
    using SampleCallback = Result (*)(const Scene* scene, float time,
                                      const Vec2& position,
                                      const Object* const* candidates, uint32_t count,
                                      PixelSample& output, void* data) noexcept;

    // One panel per scene; face families cannot be data targets or nest with Input targets.
    static Panel* gen(Scene* scene) noexcept;
    Scene* scene() const noexcept;
    Result add(UIObject* control) noexcept;
    uint32_t count() const noexcept;
    UIObject* controlAt(uint32_t index) const noexcept;
    InputResult input(const InputEvent& event) noexcept override;

    Result bind(Button* control, CameraAction action, const Vec2& delta = {}) noexcept;
    // Multiple button bindings may share a target; the latest activated transform wins.
    // Targets nested with other bindings can receive composed transforms twice.
    Result bind(Button* control, Object* object, const UITransform& transform,
                float duration = 0.0f) noexcept;
    Result toggle(Button* control, Object* object, const UITransform& off,
                  const UITransform& on, bool value = false,
                  float duration = 0.0f) noexcept;
    Result bind(Slider* control, Object* object, const UITransform& from,
                const UITransform& to) noexcept;
    // Marker/swatch outputs are exclusive and independent of candidates and other controls.
    // Marker is childless or a Group; swatch is a nongradient Rectangle.
    Result bind(SampleArea* control, const SampleBinding& binding) noexcept;
    Result states(Button* control, Object* hover, Object* pressed = nullptr) noexcept;
    // Transparent miss returns InsufficientCondition. Errors commit no marker/color selection.
    Result sampler(SampleCallback callback, void* data = nullptr) noexcept;

    Result cameraMove(const Vec2& delta) noexcept;
    Result cameraOrbit(const Vec2& delta) noexcept;
    Result cameraZoom(float delta) noexcept;
    Result cameraReset() noexcept;
    Result cameraView(CameraView view) noexcept;

private:
    struct Impl;

    explicit Panel(Scene* scene) noexcept;
    ~Panel() override;
    static Result modifyObject(const Object* object, float time, Mat4& model,
                               float& opacity, float& progress, void* data) noexcept;
    static Result modifyFill(const Object* object, float time, Color& fill,
                             void* data) noexcept;
    static Result modifyCamera(float time, Camera& camera, CameraView& view,
                               void* data) noexcept;
    static Result modifyInput(const CameraInput& input, void* data) noexcept;
    bool registered(const UIObject* control) const noexcept;
    void invalidate(UIObject* control) noexcept;
    void resetInteraction() noexcept override;
    Result camera(CameraAction action, const Vec2& delta) noexcept;

    Impl* pImpl = nullptr;
    friend struct UIObject;
};

struct Lua
{
    static Result load(const char* source, uint32_t size, const char* name,
                       Scene** scene, Panel** panel, char* error, uint32_t errorSize,
                       vis::Lua::AssetResolver resolver = nullptr,
                       void* resolverData = nullptr, const Theme* adaptiveTheme = nullptr,
                       bool* adaptiveThemeUsed = nullptr) noexcept;
};

}  // namespace vis::ui

#endif
