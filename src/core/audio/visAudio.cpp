// The audio module's soundscape: the cues a scene schedules on its buses, their gains over
// time, and the mix sampled at any scene time.
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

#include "vis_audio.h"

namespace vis::audio
{

namespace
{

struct GainClip
{
    Cue cue;
    float begin = 0.0f;
    float end = 0.0f;
    float from = 1.0f;
    float to = 1.0f;
    AnimCurve curve;
};

static char* _copy(const char* value) noexcept
{
    if (!value) return nullptr;
    auto size = std::strlen(value) + 1u;
    auto copy = new (std::nothrow) char[size];
    if (copy) std::memcpy(copy, value, size);
    return copy;
}

static bool _bus(Bus bus) noexcept
{
    return static_cast<uint8_t>(bus) <= static_cast<uint8_t>(Bus::Ui);
}

static bool _gain(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0f && value <= 4.0f;
}

static AnimCurve _curve(Easing easing) noexcept
{
    auto preset = AnimCurvePreset::Linear;
    switch (easing) {
        case Easing::Smooth: preset = AnimCurvePreset::Smooth; break;
        case Easing::EaseIn: preset = AnimCurvePreset::EaseIn; break;
        case Easing::EaseOut: preset = AnimCurvePreset::EaseOut; break;
        case Easing::EaseInOut: preset = AnimCurvePreset::EaseInOut; break;
        default: break;
    }
    return AnimCurve::preset(preset);
}

}  // namespace

struct Soundscape::Impl
{
    ~Impl()
    {
        for (auto i = 0u; i < cueCount; i++) delete[] const_cast<char*>(cues[i].asset);
        delete[] cues;
        delete[] gains;
    }

    bool growCues() noexcept
    {
        if (cueCount < cueCapacity) return true;
        auto capacity = cueCapacity ? cueCapacity * 2u : 8u;
        if (capacity > CueLimit) capacity = CueLimit;
        if (capacity <= cueCapacity) return false;
        auto grown = new (std::nothrow) CueConfig[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < cueCount; i++) grown[i] = cues[i];
        delete[] cues;
        cues = grown;
        cueCapacity = capacity;
        return true;
    }

    bool growGains() noexcept
    {
        if (gainCount < gainCapacity) return true;
        auto capacity = gainCapacity ? gainCapacity * 2u : 16u;
        if (capacity > GainLimit) capacity = GainLimit;
        if (capacity <= gainCapacity) return false;
        auto grown = new (std::nothrow) GainClip[capacity];
        if (!grown) return false;
        for (auto i = 0u; i < gainCount; i++) grown[i] = gains[i];
        delete[] gains;
        gains = grown;
        gainCapacity = capacity;
        return true;
    }

    Scene* scene = nullptr;
    CueConfig* cues = nullptr;
    GainClip* gains = nullptr;
    uint32_t cueCount = 0u;
    uint32_t cueCapacity = 0u;
    uint32_t gainCount = 0u;
    uint32_t gainCapacity = 0u;
};

Soundscape::Soundscape(Scene* scene) noexcept : pImpl(new (std::nothrow) Impl)
{
    if (pImpl) pImpl->scene = scene;
}

Soundscape::~Soundscape()
{
    delete pImpl;
}

Soundscape* Soundscape::gen(Scene* scene) noexcept
{
    if (!scene) return nullptr;
    auto soundscape = new (std::nothrow) Soundscape(scene);
    if (!soundscape || !soundscape->pImpl) {
        delete soundscape;
        return nullptr;
    }
    if (scene->add(soundscape) == Result::Success) return soundscape;
    delete soundscape;
    return nullptr;
}

Scene* Soundscape::scene() const noexcept
{
    return pImpl ? pImpl->scene : nullptr;
}

Result Soundscape::cue(const CueConfig& config, Cue& output) noexcept
{
    output = {};
    if (!pImpl || !config.asset || !_bus(config.bus)
        || !std::isfinite(config.begin) || config.begin < 0.0f
        || !std::isfinite(config.end) || (config.end != 0.0f && config.end <= config.begin)
        || !_gain(config.gain)) {
        return Result::InvalidArguments;
    }
    auto size = std::strlen(config.asset);
    if (!size || size > AssetNameLimit || pImpl->cueCount >= CueLimit) {
        return Result::InvalidArguments;
    }
    auto asset = _copy(config.asset);
    if (!asset || !pImpl->growCues()) {
        delete[] asset;
        return Result::OutOfMemory;
    }
    auto index = pImpl->cueCount++;
    pImpl->cues[index] = config;
    pImpl->cues[index].asset = asset;
    output.index = index;
    return Result::Success;
}

Result Soundscape::gain(Cue cue, float value, float begin, float duration,
                        Easing easing) noexcept
{
    if (static_cast<uint8_t>(easing) > static_cast<uint8_t>(Easing::EaseInOut)) {
        return Result::InvalidArguments;
    }
    return gain(cue, value, begin, duration, _curve(easing));
}

Result Soundscape::gain(Cue cue, float value, float begin, float duration,
                        const AnimCurve& curve) noexcept
{
    if (!pImpl || cue.index >= pImpl->cueCount || !_gain(value)
        || !std::isfinite(begin) || begin < 0.0f
        || !std::isfinite(duration) || duration <= 0.0f || !curve.valid()
        || pImpl->gainCount >= GainLimit) {
        return Result::InvalidArguments;
    }
    auto end = static_cast<double>(begin) + duration;
    if (!std::isfinite(end) || end > static_cast<double>(std::numeric_limits<float>::max())) {
        return Result::InvalidArguments;
    }
    auto lastEnd = 0.0f;
    auto found = false;
    for (auto i = 0u; i < pImpl->gainCount; i++) {
        auto& clip = pImpl->gains[i];
        if (clip.cue.index != cue.index) continue;
        lastEnd = clip.end;
        found = true;
    }
    if (found && begin < lastEnd) return Result::InvalidArguments;
    auto from = 0.0f;
    auto result = sample(cue, begin, from);
    if (result != Result::Success) return result;
    if (!pImpl->growGains()) return Result::OutOfMemory;
    auto& clip = pImpl->gains[pImpl->gainCount++];
    clip.cue = cue;
    clip.begin = begin;
    clip.end = static_cast<float>(end);
    clip.from = from;
    clip.to = value;
    clip.curve = curve;
    return Result::Success;
}

uint32_t Soundscape::count() const noexcept
{
    return pImpl ? pImpl->cueCount : 0u;
}

bool Soundscape::cueAt(uint32_t index, CueConfig& output) const noexcept
{
    if (!pImpl || index >= pImpl->cueCount) return false;
    output = pImpl->cues[index];
    return true;
}

Result Soundscape::sample(Cue cue, float time, float& output) const noexcept
{
    if (!pImpl || cue.index >= pImpl->cueCount || !std::isfinite(time) || time < 0.0f) {
        return Result::InvalidArguments;
    }
    output = pImpl->cues[cue.index].gain;
    for (auto i = 0u; i < pImpl->gainCount; i++) {
        auto& clip = pImpl->gains[i];
        if (clip.cue.index != cue.index) continue;
        if (time < clip.begin) break;
        if (time >= clip.end) {
            output = clip.to;
            continue;
        }
        auto progress = (time - clip.begin) / (clip.end - clip.begin);
        auto sampled = clip.from + (clip.to - clip.from) * ease(clip.curve, progress);
        output = std::fmax(0.0f, std::fmin(4.0f, sampled));
        break;
    }
    return Result::Success;
}

float Soundscape::duration() const noexcept
{
    if (!pImpl) return 0.0f;
    auto value = 0.0f;
    for (auto i = 0u; i < pImpl->cueCount; i++) {
        auto& cue = pImpl->cues[i];
        auto end = cue.end > cue.begin ? cue.end : cue.begin;
        if (end > value) value = end;
    }
    for (auto i = 0u; i < pImpl->gainCount; i++) {
        if (pImpl->gains[i].end > value) value = pImpl->gains[i].end;
    }
    return value;
}

}  // namespace vis::audio
