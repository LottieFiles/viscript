// The input module's public API: input events, key and pointer state, action maps, and the
// controller that moves objects and the camera from them.
#ifndef _VIS_INPUT_H_
#define _VIS_INPUT_H_

#include "vis.h"

namespace vis::input
{

enum struct InputKind : uint8_t
{
    PointerDown = 0,
    PointerMove,
    PointerUp,
    PointerCancel,
    Wheel,
    KeyDown,
    KeyUp
};

enum struct Key : uint8_t
{
    Unknown = 0,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    Space,
    Enter,
    Escape,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Number0,
    Number1,
    Number2,
    Number3,
    Number4,
    Number5,
    Number6,
    Number7,
    Number8,
    Number9,
    Plus,
    Minus,
    Shift,
    Tab
};

enum struct KeyTrigger : uint8_t
{
    Tap = 0,
    Down,
    Up
};

struct KeyState
{
    float begin = 0.0f;
    float end = 0.0f;
    bool down = false;
};

struct KeySignal
{
    Key key = Key::Unknown;
    KeyTrigger trigger = KeyTrigger::Tap;
    float begin = 0.0f;
    float end = 0.0f;
};

using KeyCallback = void (*)(const KeySignal& signal, void* data);

struct KeyTriggerBinding
{
    Key key = Key::Unknown;
    KeyTrigger trigger = KeyTrigger::Tap;
    KeyCallback callback = nullptr;
    void* data = nullptr;
};

// Positions are logical host coordinates; targets need an explicit map.
// 3D pointing needs a hit plane or intersection; repeated key-down events are ignored.
struct InputEvent
{
    InputKind kind = InputKind::PointerMove;
    Key key = Key::Unknown;
    uint32_t pointer = 0u;
    Vec2 position;
    Vec2 delta;
    Vec2 wheel;
    int32_t button = -1;
    uint32_t buttons = 0u;
    uint32_t modifiers = 0u;
    float time = 0.0f;
    bool repeat = false;
};

struct InputResult
{
    bool handled = false;
    bool redraw = false;
    Result status = Result::Success;
};

struct DigitalState
{
    float begin = 0.0f;
    float end = 0.0f;
    bool previous = false;
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct PointerState
{
    Vec2 position;
    Vec2 delta;
    Vec2 wheel;
    uint32_t pointer = 0u;
    uint32_t previousButtons = 0u;
    uint32_t buttons = 0u;
    uint32_t pressed = 0u;
    uint32_t released = 0u;
    bool active = false;
};

struct ActionState
{
    Vec2 previous;
    Vec2 value;
    Vec2 delta;
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct State final
{
    static constexpr uint32_t PointerLimit = 16u;

    static State* gen() noexcept;
    ~State();
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    // Call once per host frame before events: snapshots held state and clears only edges,
    // delta and wheel. Ordered events accumulate; same-frame press/release preserves both.
    Result begin(float time) noexcept;
    Result input(const InputEvent& event) noexcept;
    // Ends held keys/buttons atomically while retaining this frame's released edges.
    Result release(float time) noexcept;
    Result key(Key key, DigitalState& state) const noexcept;
    Result pointer(uint32_t pointer, PointerState& state) const noexcept;
    uint64_t frame() const noexcept;
    float time() const noexcept;

private:
    struct Impl;

    State() noexcept;

    Impl* pImpl = nullptr;
};

struct ActionMap final
{
    static constexpr uint32_t BindingLimit = 256u;
    static constexpr uint32_t NameLimit = 63u;

    static ActionMap* gen() noexcept;
    ~ActionMap();
    ActionMap(const ActionMap&) = delete;
    ActionMap& operator=(const ActionMap&) = delete;
    Result key(const char* action, Key key, const Vec2& value = {1.0f, 0.0f}) noexcept;
    Result pointer(const char* action, uint32_t button,
                   const Vec2& value = {1.0f, 0.0f}, uint32_t pointer = 0u) noexcept;
    Result remove(const char* action) noexcept;
    void clear() noexcept;
    // Sums held aliases and clamps components to [-1, 1]; edges follow aggregate state.
    // Re-registering an action/source replaces its value, not the other aliases.
    Result state(const State* input, const char* action, ActionState& state) const noexcept;
    uint32_t count() const noexcept;

private:
    struct Impl;

    ActionMap() noexcept;

    Impl* pImpl = nullptr;
};

// Finite nonnegative period; one binding per target, excluding KeyMove on that target.
// Observes motion without capture; coordinates use the explicit map basis, with no y flip.
struct PointerFollow
{
    Object* target = nullptr;
    BBox region;
    Vec3 targetOrigin;
    Vec3 mapOrigin;
    Vec3 mapX;
    Vec3 mapY;
    float period = 0.18f;
    float angleOffset = 0.0f;
    bool rotate = true;
    bool resetOnLeave = false;
};

// Distinct keys may share a target; duplicate target/key pairs fail. Positive period
// ramps then moves at shift / period; zero shifts once on down. Repeat is ignored.
struct KeyMove
{
    Object* target = nullptr;
    Key key = Key::Unknown;
    Vec3 shift;
    float period = 0.12f;
};

struct Controller final : Group
{
    static constexpr uint32_t BindingLimit = 256u;

    // One Input controller per scene. Its target families may not nest with UI faces.
    static Controller* gen(Scene* scene) noexcept;
    Scene* scene() const noexcept;
    State* state() const noexcept;
    Result pointerFollow(const PointerFollow& binding) noexcept;
    Result keyMove(const KeyMove& binding) noexcept;
    Result keyTrigger(const KeyTriggerBinding& binding) noexcept;
    Result keyState(Key key, KeyState& state) const noexcept;
    InputResult input(const InputEvent& event) noexcept;
    InputResult release(float time) noexcept;

    Result cameraMove(const Vec2& delta) noexcept;
    Result cameraOrbit(const Vec2& delta) noexcept;
    Result cameraZoom(float delta) noexcept;
    Result cameraReset() noexcept;
    Result cameraView(CameraView view) noexcept;

private:
    struct Impl;

    explicit Controller(Scene* scene) noexcept;
    ~Controller() override;
    static Result modifyObject(const Object* object, float time, Mat4& model,
                               float& opacity, float& progress, void* data) noexcept;
    static Result modifyCamera(float time, Camera& camera, CameraView& view,
                               void* data) noexcept;
    static Result modifyInput(const CameraInput& input, void* data) noexcept;
    Result camera(CameraAction action, const Vec2& delta) noexcept;

    Impl* pImpl = nullptr;
};

struct Lua
{
    static Result load(const char* source, uint32_t size, const char* name,
                       Scene** scene, Controller** controller, char* error,
                       uint32_t errorSize, vis::Lua::AssetResolver resolver = nullptr,
                       void* resolverData = nullptr, const Theme* adaptiveTheme = nullptr,
                       bool* adaptiveThemeUsed = nullptr) noexcept;
};

}  // namespace vis::input

#endif
