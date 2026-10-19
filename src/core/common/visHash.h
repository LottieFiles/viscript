// Hashes bytes into a running 64-bit FNV-1a hash.
#ifndef _VIS_HASH_H_
#define _VIS_HASH_H_

#include <cstddef>
#include <cstdint>

namespace vis
{

static inline void _hash(uint64_t& hash, const void* data, size_t size)
{
    auto bytes = static_cast<const uint8_t*>(data);
    for (auto i = 0u; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
}

template<typename T>
static inline void _hash(uint64_t& hash, const T& value)
{
    _hash(hash, &value, sizeof(value));
}

}  // namespace vis

#endif
