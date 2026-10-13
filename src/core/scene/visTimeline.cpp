#include <cstring>

#include "visScene.h"
#include "visTimeline.h"
#include "../common/visString.h"

namespace vis
{

Result Scene::wait(float duration) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!std::isfinite(duration) || duration < 0.0f) return pImpl->fail(Result::InvalidArguments, WAIT_DURATION);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto nextCursor = pImpl->cursor;
    if (duration > 0.0f && !_timeline(pImpl->cursor, duration, 0.0f, 1u, nextCursor)) {
        return pImpl->fail(Result::InvalidArguments, TIMELINE_RANGE);
    }
    pImpl->advance(nextCursor);
    pImpl->definitionsSealed = true;
    return Result::Success;
}

float Scene::cursor() const noexcept
{
    return pImpl->cursor;
}

Result Scene::seek(float time) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!std::isfinite(time) || time < 0.0f) return pImpl->fail(Result::InvalidArguments, SEEK_TIME);
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    if (time == pImpl->cursor) return Result::Success;
    pImpl->advance(time);
    pImpl->definitionsSealed = true;
    return Result::Success;
}

Result Scene::mark(const char* name) noexcept
{
    pImpl->diagnostic = nullptr;
    if (!name || !name[0]) return pImpl->fail(Result::InvalidArguments, "a marker needs a name");
    if (pImpl->owner) return pImpl->fail(Result::InsufficientCondition, SCENE_MOUNTED);
    auto size = std::strlen(name);
    if (size > MAX_MARKER_NAME) return pImpl->fail(Result::InvalidArguments, "marker names are limited to 128 bytes");
    for (auto i = 0u; i < pImpl->markerCnt; i++) {
        if (std::strcmp(pImpl->markers[i].name, name) == 0) {
            return pImpl->fail(Result::InvalidArguments, "a marker with this name already exists");
        }
    }
    if (pImpl->markerCnt >= MAX_MARKERS) return pImpl->fail(Result::InsufficientCondition, "a scene holds at most 256 markers");
    if (!pImpl->growMarkers()) return Result::OutOfMemory;
    auto copy = _duplicate(name);
    if (!copy) return Result::OutOfMemory;
    auto& marker = pImpl->markers[pImpl->markerCnt++];
    marker.name = copy;
    marker.time = pImpl->cursor;
    return Result::Success;
}

bool Scene::marker(const char* name, float& time) const noexcept
{
    if (!name) return false;
    for (auto i = 0u; i < pImpl->markerCnt; i++) {
        if (std::strcmp(pImpl->markers[i].name, name) != 0) continue;
        time = pImpl->markers[i].time;
        return true;
    }
    return false;
}

uint32_t Scene::markerCount() const noexcept
{
    return pImpl->markerCnt;
}

uint32_t Scene::clipCount() const noexcept
{
    return pImpl->clipCnt;
}

uint64_t Scene::sampleWork() const noexcept
{
    auto work = pImpl->work;
    for (auto i = 0u; i < pImpl->viewportCnt; i++) work += pImpl->viewports[i].scene->sampleWork();
    if (auto sequence = pImpl->sequence) {
        for (auto i = 0u; i < sequence->count; i++) work += sequence->stages[i].scene->sampleWork();
    }
    return work;
}

const char* Scene::markerAt(uint32_t index, float& time) const noexcept
{
    if (index >= pImpl->markerCnt) return nullptr;
    time = pImpl->markers[index].time;
    return pImpl->markers[index].name;
}

const char* Scene::diagnostic() const noexcept
{
    return pImpl->diagnostic;
}

float Scene::duration() const noexcept
{
    auto value = pImpl->extent > pImpl->cursor ? pImpl->extent : pImpl->cursor;
    for (auto i = 0u; i < pImpl->viewportCnt; i++) {
        auto duration = pImpl->viewports[i].scene->duration();
        if (duration > value) value = duration;
    }
    return value;
}

}  // namespace vis
