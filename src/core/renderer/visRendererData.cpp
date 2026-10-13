#include <cstddef>
#include <limits>
#include <new>

#include "vis.h"
#include "visRendererData.h"

namespace vis
{

LayoutReport::LayoutReport() noexcept
{
    pImpl = new (std::nothrow) Impl;
}

LayoutReport::~LayoutReport()
{
    delete pImpl;
}

float LayoutReport::time() const noexcept
{
    return pImpl ? pImpl->sampleTime : 0.0f;
}

float LayoutReport::padding() const noexcept
{
    return pImpl ? pImpl->samplePadding : 0.0f;
}

uint32_t LayoutReport::sceneCount() const noexcept
{
    return pImpl ? pImpl->sceneCnt : 0u;
}

const LayoutScene* LayoutReport::sceneAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->sceneCnt ? pImpl->scenes + index : nullptr;
}

uint32_t LayoutReport::objectCount() const noexcept
{
    return pImpl ? pImpl->objectCnt : 0u;
}

const LayoutObject* LayoutReport::objectAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->objectCnt ? pImpl->objects + index : nullptr;
}

uint32_t LayoutReport::visualCount() const noexcept
{
    return pImpl ? pImpl->visualCnt : 0u;
}

const LayoutVisual* LayoutReport::visualAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->visualCnt ? pImpl->visuals + index : nullptr;
}

uint32_t LayoutReport::pathCount() const noexcept
{
    return pImpl ? pImpl->pathCnt : 0u;
}

const LayoutPath* LayoutReport::pathAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->pathCnt ? pImpl->paths + index : nullptr;
}

uint32_t LayoutReport::collisionCount() const noexcept
{
    return pImpl ? pImpl->collisionCnt : 0u;
}

const LayoutCollision* LayoutReport::collisionAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->collisionCnt ? pImpl->collisions + index : nullptr;
}

uint32_t LayoutReport::containmentCount() const noexcept
{
    return pImpl ? pImpl->containmentCnt : 0u;
}

const LayoutContainment* LayoutReport::containmentAt(uint32_t index) const noexcept
{
    return pImpl && index < pImpl->containmentCnt ? pImpl->containments + index : nullptr;
}

Surface::~Surface()
{
    delete[] buffer;
}

Result Surface::resize(uint32_t width, uint32_t height) noexcept
{
    if (!width || !height) return Result::InvalidArguments;
    if (buffer && bufferWidth == width && bufferHeight == height) return Result::Success;
    auto limit = std::numeric_limits<size_t>::max() / sizeof(uint32_t);
    if (static_cast<size_t>(height) > limit / width) return Result::OutOfMemory;
    auto buffer = new (std::nothrow) uint32_t[static_cast<size_t>(width) * height];
    if (!buffer) return Result::OutOfMemory;
    delete[] this->buffer;
    this->buffer = buffer;
    bufferWidth = width;
    bufferHeight = height;
    bufferStride = width;
    return Result::Success;
}

uint32_t* Surface::data() noexcept
{
    return buffer;
}

const uint32_t* Surface::data() const noexcept
{
    return buffer;
}

uint32_t Surface::width() const noexcept
{
    return bufferWidth;
}

uint32_t Surface::height() const noexcept
{
    return bufferHeight;
}

uint32_t Surface::stride() const noexcept
{
    return bufferStride;
}

}  // namespace vis
