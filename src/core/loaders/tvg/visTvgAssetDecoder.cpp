// Probes and decodes encoded images to RGBA pixels with ThorVG's picture loaders.
#include <cmath>
#include <new>

#include <thorvg.h>

#include "vis.h"
#include "visAssetDecoder.h"
#include "visTvgRenderer.h"

namespace vis::asset
{

static Result _result(tvg::Result result)
{
    switch (result) {
        case tvg::Result::Success: return Result::Success;
        case tvg::Result::InvalidArguments: return Result::InvalidArguments;
        case tvg::Result::InsufficientCondition: return Result::InsufficientCondition;
        case tvg::Result::FailedAllocation: return Result::OutOfMemory;
        case tvg::Result::NonSupport: return Result::NonSupport;
        default: return Result::Unknown;
    }
}

// Loads data into picture and gives its size in whole pixels. The caller unrefs picture, which
// a reference of its own keeps alive past a canvas that holds it.
static Result _load(const void* data, uint32_t size, const char* mime, tvg::Picture*& picture,
                    uint32_t& outputWidth, uint32_t& outputHeight)
{
    picture = tvg::Picture::gen();
    if (!picture) return Result::OutOfMemory;
    picture->ref();
    auto result = _result(picture->load(static_cast<const char*>(data), size, mime, nullptr, true));
    if (result != Result::Success) return result;
    float sourceWidth;
    float sourceHeight;
    result = _result(picture->size(&sourceWidth, &sourceHeight));
    if (result != Result::Success) return result;
    if (!std::isfinite(sourceWidth) || !std::isfinite(sourceHeight) || sourceWidth <= 0.0f || sourceHeight <= 0.0f) {
        return Result::InvalidArguments;
    }
    auto widthValue = std::ceil(static_cast<double>(sourceWidth));
    auto heightValue = std::ceil(static_cast<double>(sourceHeight));
    if (widthValue > UINT32_MAX || heightValue > UINT32_MAX) return Result::InvalidArguments;
    auto width = static_cast<uint32_t>(widthValue);
    auto height = static_cast<uint32_t>(heightValue);
    if (!width || !height || height > MAX_ASSET_PIXELS / width) return Result::InvalidArguments;
    outputWidth = width;
    outputHeight = height;
    return Result::Success;
}

// Draws picture, stretched to width by height, into new pixels.
static Result _draw(tvg::Picture* picture, uint32_t width, uint32_t height, uint32_t*& output)
{
    auto pixels = new (std::nothrow) uint32_t[static_cast<size_t>(width) * height]();
    if (!pixels) return Result::OutOfMemory;
    auto canvas = tvg::SwCanvas::gen();
    auto drawn = canvas && picture->size(static_cast<float>(width), static_cast<float>(height)) == tvg::Result::Success
              && canvas->target(pixels, width, width, height, tvg::ColorSpace::ABGR8888S) == tvg::Result::Success
              && canvas->add(picture) == tvg::Result::Success && renderer::tvgDraw(canvas);
    delete canvas;
    if (!drawn) {
        delete[] pixels;
        return Result::NonSupport;
    }
    output = pixels;
    return Result::Success;
}

Result probe(const void* data, uint32_t size, const char* mime,
             uint32_t& width, uint32_t& height) noexcept
{
    width = 0;
    height = 0;
    if (!data || !size || !mime || !mime[0]) return Result::InvalidArguments;
    if (!renderer::tvgInit()) return Result::NonSupport;
    tvg::Picture* picture = nullptr;
    auto result = _load(data, size, mime, picture, width, height);
    if (picture) picture->unref();
    renderer::tvgTerm();
    return result;
}

Result decode(const void* data, uint32_t size, const char* mime,
              uint32_t*& pixels, uint32_t& width, uint32_t& height) noexcept
{
    pixels = nullptr;
    width = 0;
    height = 0;
    if (!data || !size || !mime || !mime[0]) return Result::InvalidArguments;
    if (!renderer::tvgInit()) return Result::NonSupport;
    tvg::Picture* picture = nullptr;
    auto result = _load(data, size, mime, picture, width, height);
    if (result == Result::Success) result = _draw(picture, width, height, pixels);
    if (result != Result::Success) width = height = 0;
    if (picture) picture->unref();
    renderer::tvgTerm();
    return result;
}

}  // namespace vis::asset
