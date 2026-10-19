// The audio module's public API: cues on a scene's soundscape, and the player that plays them.
#ifndef _VIS_AUDIO_H_
#define _VIS_AUDIO_H_

#include "vis.h"

namespace vis::audio
{

enum struct Bus : uint8_t
{
    Music = 0,
    Effect,
    Ui
};

struct Cue
{
    static constexpr uint32_t Invalid = 0xffffffffu;
    uint32_t index = Invalid;

    explicit operator bool() const noexcept { return index != Invalid; }
};

// Opaque generation-tagged handle; a reused slot does not revive an old handle.
struct Voice
{
    static constexpr uint32_t Invalid = 0xffffffffu;
    uint32_t index = Invalid;
    uint32_t generation = 0u;

    explicit operator bool() const noexcept { return index != Invalid; }
};

// Cue times are absolute scene seconds; end = 0 has no explicit cutoff.
struct CueConfig
{
    const char* asset = nullptr;
    Bus bus = Bus::Effect;
    float begin = 0.0f;
    float end = 0.0f;
    float gain = 1.0f;
    bool loop = false;
};

// Gain is in [0, 4], rate in [0.125, 8]. Playback has no reverse or pitch-preserving stretch.
struct Playback
{
    const char* asset = nullptr;
    Bus bus = Bus::Effect;
    float gain = 1.0f;
    float rate = 1.0f;
    bool loop = false;
};

// playing = false pauses transport; seek explicitly resets decoding position.
struct Transport
{
    float time = 0.0f;
    float rate = 1.0f;
    bool playing = false;
    bool seek = false;
};

struct PlayerConfig
{
    uint32_t voiceLimit = 32u;
    bool device = true;
};

struct Soundscape final : Group
{
    static constexpr uint32_t CueLimit = 256u;
    static constexpr uint32_t GainLimit = 1024u;
    static constexpr uint32_t AssetNameLimit = 255u;

    ~Soundscape() override;
    static Soundscape* gen(Scene* scene) noexcept;

    Scene* scene() const noexcept;
    Result cue(const CueConfig& config, Cue& output) noexcept;
    // Finite gain is in [0, 4]; gain clips on a cue may not overlap.
    Result gain(Cue cue, float value, float begin, float duration = 1.0f,
                Easing easing = Easing::Smooth) noexcept;
    Result gain(Cue cue, float value, float begin, float duration,
                const AnimCurve& curve) noexcept;
    Result gain(Cue cue, float value, float begin, const TimingOptions& timing) noexcept
    {
        return gain(cue, value, begin, timing.duration, timing.curve);
    }
    uint32_t count() const noexcept;
    bool cueAt(uint32_t index, CueConfig& output) const noexcept;
    Result sample(Cue cue, float time, float& output) const noexcept;
    float duration() const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;

    explicit Soundscape(Scene* scene) noexcept;
    friend struct Player;
};

struct Player
{
    static constexpr uint32_t AssetLimit = 64u;
    static constexpr uint32_t AssetByteLimit = 32u * 1024u * 1024u;
    static constexpr uint32_t AssetTotalByteLimit = 64u * 1024u * 1024u;
    static constexpr uint32_t VoiceLimit = 256u;
    static constexpr uint32_t MixRateMin = 8000u;
    static constexpr uint32_t MixRateMax = 192000u;

    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    // Voice capacity is 1..256 (default 32); saturation fails without stealing voices.
    static Player* gen(const PlayerConfig& config = {}) noexcept;
    Result load(const char* name, const uint8_t* data, uint32_t size) noexcept;
    // Browser audio must start from a user gesture on the window/main thread.
    Result start() noexcept;
    Result play(const Playback& playback, Voice* output = nullptr) noexcept;
    Result stop(Voice voice) noexcept;
    bool active(Voice voice) const noexcept;
    Result gain(float value) noexcept;
    Result gain(Bus bus, float value) noexcept;
    // Load assets before bind/play/mix. Bound soundscape must remain alive; detach before
    // scene destruction and rebind after adding cues.
    Result bind(const Soundscape* soundscape) noexcept;
    Result sync(const Transport& transport) noexcept;
    // Mixes the bound Soundscape into interleaved stereo output (left, right), at frame/rate
    // scene time. Excludes immediate voices; applies cue/bus/master gain and clamps to [-1, 1].
    // Rates are 8000..192000 Hz; gain is sampled every 32 frames. All assets must be loaded
    // or InsufficientCondition is returned. Sequential blocks reuse decoder position.
    Result mix(uint32_t rate, uint64_t first, float* output, uint32_t frames) noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;

    explicit Player(const PlayerConfig& config) noexcept;
};

struct Lua
{
    static Result load(const char* source, uint32_t size, const char* name,
                       Scene** scene, Soundscape** soundscape,
                       char* error, uint32_t errorSize,
                       vis::Lua::AssetResolver resolver = nullptr,
                       void* resolverData = nullptr,
                       const Theme* adaptiveTheme = nullptr,
                       bool* adaptiveThemeUsed = nullptr) noexcept;
};

}  // namespace vis::audio

#endif
