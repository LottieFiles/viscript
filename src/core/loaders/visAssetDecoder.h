// The image decoder the asset loader calls: probe for the size, decode for the pixels.
#ifndef _VIS_ASSET_DECODER_H_
#define _VIS_ASSET_DECODER_H_

#include <cstdint>

#include "vis.h"

namespace vis::asset
{

// The most pixels an asset holds, whether decoded or loaded as pixels.
static constexpr uint32_t MAX_ASSET_PIXELS = 16777216;

// Validate encoded bytes and report the intrinsic size.
Result probe(const void* data, uint32_t size, const char* mime,
             uint32_t& width, uint32_t& height) noexcept;
// Rasterize encoded bytes to RGBA at the intrinsic size.
Result decode(const void* data, uint32_t size, const char* mime,
              uint32_t*& pixels, uint32_t& width, uint32_t& height) noexcept;

}  // namespace vis::asset

#endif
