#ifndef _VIS_ASSET_DECODER_H_
#define _VIS_ASSET_DECODER_H_

#include <cstdint>

#include "vis.h"

namespace vis::asset
{

// Validate encoded bytes and report the intrinsic size.
Result probe(const void* data, uint32_t size, const char* mime,
             uint32_t& width, uint32_t& height) noexcept;
// Rasterize encoded bytes to RGBA at the intrinsic size.
Result decode(const void* data, uint32_t size, const char* mime,
              uint32_t*& pixels, uint32_t& width, uint32_t& height) noexcept;

}  // namespace vis::asset

#endif
