// A stable sort through a comparison function, which the sorts of the engine share instead of
// building std::sort once for each comparison.
#ifndef _VIS_SORT_H_
#define _VIS_SORT_H_

#include <cstdint>

namespace vis
{

// Sorts count items of size bytes each so that none comes after an item it is before(first,
// second, data); items where neither is before the other keep their order. scratch holds count
// items. Items move by memcpy, so they must be trivially copyable.
void stableSort(void* items, void* scratch, uint32_t count, uint32_t size,
                bool (*before)(const void* first, const void* second, const void* data),
                const void* data = nullptr) noexcept;

}  // namespace vis

#endif
