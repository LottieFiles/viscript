#ifndef _VIS_WARP_H_
#define _VIS_WARP_H_

#include <new>

#include "visScene.h"

namespace vis
{

struct WarpDraft
{
    static constexpr uint32_t PointLimit = 2097152;
    Vec3* points = nullptr;
    WarpRun* runs = nullptr;
    uint32_t count = 0;
    uint32_t capacity = 0;
    uint32_t runCount = 0;
    uint32_t runCapacity = 0;
    bool closed = false;
    bool full = false;
    bool failed = false;

    ~WarpDraft()
    {
        delete[] points;
        delete[] runs;
    }

    void add(const Vec3& point)
    {
        if (full || failed) return;
        if (count == capacity) {
            if (capacity >= PointLimit) {
                full = true;
                return;
            }
            auto grown = capacity ? capacity * 2u : 64u;
            auto copy = new (std::nothrow) Vec3[grown];
            if (!copy) {
                failed = true;
                return;
            }
            for (auto i = 0u; i < count; i++)
                copy[i] = points[i];
            delete[] points;
            points = copy;
            capacity = grown;
        }
        points[count++] = point;
    }

    // Takes the images of a warp, in its runs.
    void copy(const WarpShape& shape)
    {
        auto begin = 0u;
        for (auto run = 0u; run < shape.runCount; run++) {
            for (auto i = begin; i < shape.runs[run].end; i++)
                add(shape.to[i]);
            end(shape.runs[run].part, shape.runs[run].value);
            begin = shape.runs[run].end;
        }
        closed = shape.closed;
    }

    // Ends the run of the points added since the previous run ended.
    void end(WarpPart part, float value = 0.0f)
    {
        if (full || failed) return;
        auto begin = runCount ? runs[runCount - 1u].end : 0u;
        if (count == begin) return;
        if (runCount == runCapacity) {
            auto grown = runCapacity ? runCapacity * 2u : 8u;
            auto copy = new (std::nothrow) WarpRun[grown];
            if (!copy) {
                failed = true;
                return;
            }
            for (auto i = 0u; i < runCount; i++)
                copy[i] = runs[i];
            delete[] runs;
            runs = copy;
            runCapacity = grown;
        }
        runs[runCount++] = {count, part, value};
    }
    void gather(const Object* object, const Mat4& space, float step);
    Result map(const Mat4& space, const Mat4& inverse, PointMap callback, void* data, float step,
               uint32_t budget, WarpShape*& output, Vec3& missing) const;
};

}  // namespace vis

#endif
