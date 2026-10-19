// The growth of the arrays the engine owns, by doubling their capacity, which their owners share
// instead of each growing its own.
#ifndef _VIS_GROW_H_
#define _VIS_GROW_H_

#include <cstdint>
#include <new>

namespace vis
{

// Makes room for more items after the count that items holds. The capacity doubles from initial
// (above 0) and stops at limit; the items held are assigned over one by one. False, with the
// array as it was, when count and more pass limit or memory runs out.
template<typename T>
static inline bool grow(T*& items, uint32_t count, uint32_t& capacity, uint32_t more, uint32_t initial,
                        uint32_t limit = UINT32_MAX) noexcept
{
    if (count > limit || more > limit - count) return false;
    if (more <= capacity - count) return true;
    auto next = capacity ? capacity : initial;
    while (next - count < more) next = next > limit / 2u ? limit : next * 2u;
    auto grown = new (std::nothrow) T[next];
    if (!grown) return false;
    for (auto i = 0u; i < count; i++) grown[i] = items[i];
    delete[] items;
    items = grown;
    capacity = next;
    return true;
}

}  // namespace vis

#endif
