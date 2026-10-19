// The image decoder in a build without ThorVG's picture loaders: probe and decode report
// NonSupport.
#include "visAssetDecoder.h"

namespace vis::asset
{

Result probe(const void* data, uint32_t size, const char* mime,
             uint32_t& width, uint32_t& height) noexcept
{
    width = 0;
    height = 0;
    if (!data || !size || !mime || !mime[0]) return Result::InvalidArguments;
    return Result::NonSupport;
}

Result decode(const void* data, uint32_t size, const char* mime,
              uint32_t*& pixels, uint32_t& width, uint32_t& height) noexcept
{
    pixels = nullptr;
    width = 0;
    height = 0;
    if (!data || !size || !mime || !mime[0]) return Result::InvalidArguments;
    return Result::NonSupport;
}

}  // namespace vis::asset
