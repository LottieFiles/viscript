// The check the C++ tests count their failures with, the report that ends a test, and the
// comparisons of numbers, colors and frames that several tests share.
#ifndef _VIS_TEST_CHECK_H_
#define _VIS_TEST_CHECK_H_

#include <cmath>
#include <cstdio>
#include <cstring>

#include "vis.h"

// The checks that failed in this test executable, in any of its sources.
inline int failures = 0;

// Says where a check failed and counts it. CHECK calls it by its global name, so a local
// named check does not hide it.
static inline void check(bool condition, const char* expression, const char* file, int line)
{
    if (condition) return;
    std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
    failures++;
}

#define CHECK(condition) ::check(static_cast<bool>(condition), #condition, __FILE__, __LINE__)

// The exit status of a test: 1, after saying how many checks failed, when any did.
static inline int report()
{
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}

static inline bool near(float lhs, float rhs, float epsilon = 1.0e-4f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

// A color as a surface stores it, red in the lowest byte.
static inline uint32_t pixel(vis::Color color)
{
    return static_cast<uint32_t>(color.a) << 24 | static_cast<uint32_t>(color.b) << 16
        | static_cast<uint32_t>(color.g) << 8 | color.r;
}

static inline bool same(vis::Color first, vis::Color second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b && first.a == second.a;
}

// The pixels of a surface that differ from its background.
static inline uint32_t changed(const vis::Surface& surface, vis::Color background)
{
    auto unchanged = pixel(background);
    auto count = 0u;
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] != unchanged) count++;
        }
    }
    return count;
}

// A hash of the pixels of a surface: 64-bit FNV-1a over its 32-bit pixels.
static inline uint64_t checksum(const vis::Surface& surface)
{
    auto hash = 14695981039346656037ull;
    for (auto i = 0u; i < surface.width() * surface.height(); i++) {
        hash ^= surface.data()[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

// Whether two surfaces have the same size and pixels, whatever their strides.
static inline bool equal(const vis::Surface& first, const vis::Surface& second)
{
    if (first.width() != second.width() || first.height() != second.height()) return false;
    for (auto y = 0u; y < first.height(); y++) {
        auto a = first.data() + static_cast<size_t>(y) * first.stride();
        auto b = second.data() + static_cast<size_t>(y) * second.stride();
        if (std::memcmp(a, b, first.width() * sizeof(uint32_t)) != 0) return false;
    }
    return true;
}

#endif
