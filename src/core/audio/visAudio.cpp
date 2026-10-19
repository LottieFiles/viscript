// The audio module's soundscape: the cues a scene schedules on its buses, their gains over
// time, and the mix sampled at any scene time.
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

#include "vis_audio.h"
#include "../common/visGrow.h"
#include "../common/visString.h"

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

static bool _bus(Bus bus) noexcept
{
    return static_cast<uint8_t>(bus) <= static_cast<uint8_t>(Bus::Ui);
}

static bool _gain(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0f && value <= 4.0f;
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
    return pImpl->scene;
}

Result Soundscape::cue(const CueConfig& config, Cue& output) noexcept
{
    output = {};
    if (!config.asset || !_bus(config.bus)
        || !std::isfinite(config.begin) || config.begin < 0.0f
        || !std::isfinite(config.end) || (config.end != 0.0f && config.end <= config.begin)
        || !_gain(config.gain)) {
        return Result::InvalidArguments;
    }
    auto size = std::strlen(config.asset);
    if (!size || size > AssetNameLimit) return Result::InvalidArguments;
    if (pImpl->cueCount >= CueLimit) return Result::InsufficientCondition;
    auto asset = _duplicate(config.asset);
    if (!asset || !grow(pImpl->cues, pImpl->cueCount, pImpl->cueCapacity, 1u, 8u, CueLimit)) {
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
                        const AnimCurve& curve) noexcept
{
    if (cue.index >= pImpl->cueCount || !_gain(value)
        || !std::isfinite(begin) || begin < 0.0f
        || !std::isfinite(duration) || duration <= 0.0f || !curve.valid()) {
        return Result::InvalidArguments;
    }
    auto end = static_cast<double>(begin) + duration;
    if (!std::isfinite(end) || end > static_cast<double>(std::numeric_limits<float>::max())) {
        return Result::InvalidArguments;
    }
    // A clip begins no sooner than the cue's last one ends.
    auto lastEnd = 0.0f;
    for (auto i = 0u; i < pImpl->gainCount; i++) {
        if (pImpl->gains[i].cue.index == cue.index) lastEnd = pImpl->gains[i].end;
    }
    if (begin < lastEnd) return Result::InvalidArguments;
    if (pImpl->gainCount >= GainLimit) return Result::InsufficientCondition;
    auto from = 0.0f;
    sample(cue, begin, from);
    if (!grow(pImpl->gains, pImpl->gainCount, pImpl->gainCapacity, 1u, 16u, GainLimit)) {
        return Result::OutOfMemory;
    }
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
    return pImpl->cueCount;
}

bool Soundscape::cueAt(uint32_t index, CueConfig& output) const noexcept
{
    if (index >= pImpl->cueCount) return false;
    output = pImpl->cues[index];
    return true;
}

Result Soundscape::sample(Cue cue, float time, float& output) const noexcept
{
    if (cue.index >= pImpl->cueCount || !std::isfinite(time) || time < 0.0f) {
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
