// The merge sort behind stableSort().
#include <cstring>

#include "visSort.h"

namespace vis
{

void stableSort(void* items, void* scratch, uint32_t count, uint32_t size,
                bool (*before)(const void* first, const void* second, const void* data),
                const void* data) noexcept
{
    // Each pass merges pairs of sorted runs from one buffer into the other, doubling their width.
    auto from = static_cast<uint8_t*>(items);
    auto to = static_cast<uint8_t*>(scratch);
    for (auto width = uint64_t{1}; width < count; width *= 2u) {
        for (auto low = uint64_t{0}; low < count; low += 2u * width) {
            auto middle = low + width < count ? low + width : count;
            auto high = low + 2u * width < count ? low + 2u * width : count;
            auto left = low;
            auto right = middle;
            for (auto out = low; out < high; out++) {
                auto next = left < middle && (right >= high || !before(from + right * size, from + left * size, data))
                          ? left++ : right++;
                std::memcpy(to + out * size, from + next * size, size);
            }
        }
        auto swap = from;
        from = to;
        to = swap;
    }
    if (from != items) std::memcpy(items, from, static_cast<size_t>(count) * size);
}

}  // namespace vis
