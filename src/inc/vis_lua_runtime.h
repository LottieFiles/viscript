// The retained Lua runtime's public API: per-frame callbacks over a loaded scene, its overlays,
// input and sounds.
#ifndef _VIS_LUA_RUNTIME_H_
#define _VIS_LUA_RUNTIME_H_

#include "vis.h"

namespace vis::input
{
struct State;
}

namespace vis::lua_runtime
{

namespace detail
{
struct RuntimeAccess;
}

// fixedStep is finite in (0, 1] seconds; maxSteps is in [1, 64].
struct Config
{
    float fixedStep = 1.0f / 120.0f;
    uint32_t maxSteps = 8u;
};

// Overlay opacity multiplies authored opacity; use nonzero base opacity for later visibility.
struct ObjectState
{
    Vec3 origin;
    Vec3 shift;
    Vec3 rotation;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    Color fill;
    float opacity = 1.0f;
    float progress = 1.0f;
    bool originEnabled = false;
    bool fillEnabled = false;
};

enum struct SoundBus : uint8_t
{
    Music = 0,
    Effect,
    Ui
};

struct SoundEvent
{
    static constexpr uint32_t AssetNameLimit = 255u;

    char asset[AssetNameLimit + 1u] = {};
    SoundBus bus = SoundBus::Effect;
    float gain = 1.0f;
    float rate = 1.0f;
    bool loop = false;
};

struct Runtime final : Group
{
    static constexpr uint32_t ObjectLimit = 512u;
    static constexpr uint32_t SoundEventLimit = 64u;

    static Runtime* gen(Scene* scene, const Config& config = {}) noexcept;
    Scene* scene() const noexcept;
    // At most 512 overlays. Runtime retains its Lua VM and seals construction authoring.
    // Overlay updates inside a callback commit on success or roll back on failure.
    Result update(Object* object, const ObjectState& state) noexcept;
    Result clear(Object* object) noexcept;
    void clear() noexcept;
    // Stages up to 64 sounds/advance with bounded asset names and valid bus/gain/rate.
    // Saturation fails; callback failure discards its staged sounds.
    Result sound(const SoundEvent& event) noexcept;
    uint32_t soundCount() const noexcept;
    bool soundAt(uint32_t index, SoundEvent& output) const noexcept;
    // Clamps elapsed to fixedStep * maxSteps and runs at most 64 callbacks/advance.
    // Each callback has a 1000000-instruction budget; overlay/sound changes commit atomically.
    // Failure is permanent; Lua-local mutations are not rolled back.
    // Only the first catch-up step consumes input edges, pointer delta and wheel.
    Result advance(float elapsed) noexcept;
    Result input(const input::State* state) noexcept;
    bool claimsKey(uint32_t key) const noexcept;
    uint32_t claimsPointer(uint32_t pointer) const noexcept;
    float fixedStep() const noexcept;
    float interpolation() const noexcept;
    double time() const noexcept;
    double dropped() const noexcept;
    uint64_t tick() const noexcept;
    uint32_t steps() const noexcept;
    bool failed() const noexcept;
    const char* error() const noexcept;

private:
    struct Impl;

    explicit Runtime(Scene* scene, const Config& config) noexcept;
    ~Runtime() override;
    static Result modifyObject(const Object* object, float time, Mat4& model,
                               float& opacity, float& progress, void* data) noexcept;
    static Result modifyFill(const Object* object, float time, Color& fill,
                             void* data) noexcept;

    Impl* pImpl = nullptr;
    friend struct detail::RuntimeAccess;
};

struct Lua
{
    static Result load(const char* source, uint32_t size, const char* name,
                       Scene** scene, Runtime** runtime, char* error,
                       uint32_t errorSize, vis::Lua::AssetResolver resolver = nullptr,
                       void* resolverData = nullptr, const Theme* adaptiveTheme = nullptr,
                       bool* adaptiveThemeUsed = nullptr) noexcept;
};

}  // namespace vis::lua_runtime

#endif
