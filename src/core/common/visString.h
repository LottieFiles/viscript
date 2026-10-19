// Copies a C string into new storage, or gives nullptr.
#ifndef _VIS_STRING_H_
#define _VIS_STRING_H_

#include <cstring>
#include <new>

namespace vis
{

static inline char* _duplicate(const char* text)
{
    if (!text) return nullptr;
    auto size = std::strlen(text) + 1u;
    auto copy = new (std::nothrow) char[size];
    if (copy) std::memcpy(copy, text, size);
    return copy;
}

}  // namespace vis

#endif
