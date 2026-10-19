// LayoutReport::Impl: the records a layout report keeps and grows.
#ifndef _VIS_RENDERER_DATA_H_
#define _VIS_RENDERER_DATA_H_

#include "vis.h"
#include "../common/visGrow.h"

namespace vis
{

struct LayoutReport::Impl
{
    // The objects behind a visual record, until the walk turns them into object indices.
    struct Source
    {
        const Object* object = nullptr;
        const Object* counterpart = nullptr;
    };

    LayoutScene* scenes = nullptr;
    LayoutObject* objects = nullptr;
    LayoutVisual* visuals = nullptr;
    Source* sources = nullptr;
    LayoutPath* paths = nullptr;
    LayoutCollision* collisions = nullptr;
    LayoutContainment* containments = nullptr;
    uint32_t sceneCnt = 0;
    uint32_t sceneCap = 0;
    uint32_t objectCnt = 0;
    uint32_t objectCap = 0;
    uint32_t visualCnt = 0;
    uint32_t visualCap = 0;
    uint32_t sourceCap = 0;
    uint32_t pathCnt = 0;
    uint32_t pathCap = 0;
    uint32_t collisionCnt = 0;
    uint32_t collisionCap = 0;
    uint32_t containmentCnt = 0;
    uint32_t containmentCap = 0;
    float sampleTime = 0.0f;
    float samplePadding = 0.0f;

    ~Impl()
    {
        reset();
        delete[] scenes;
        delete[] objects;
        delete[] visuals;
        delete[] sources;
        delete[] paths;
        delete[] collisions;
        delete[] containments;
    }

    void reset()
    {
        for (auto i = 0u; i < sceneCnt; i++)
            delete[] scenes[i].path;
        for (auto i = 0u; i < pathCnt; i++)
            delete[] paths[i].points;
        sceneCnt = 0;
        objectCnt = 0;
        visualCnt = 0;
        pathCnt = 0;
        collisionCnt = 0;
        containmentCnt = 0;
        sampleTime = 0.0f;
        samplePadding = 0.0f;
    }

    // Appends value to items, which grow when full.
    template<typename T>
    static bool append(T*& items, uint32_t& count, uint32_t& capacity, const T& value)
    {
        if (!grow(items, count, capacity, 1u, 16u)) return false;
        items[count++] = value;
        return true;
    }

    bool add(const LayoutScene& value)
    {
        return append(scenes, sceneCnt, sceneCap, value);
    }

    bool add(const LayoutObject& value)
    {
        return append(objects, objectCnt, objectCap, value);
    }

    bool add(const LayoutVisual& value, const Object* object, const Object* counterpart)
    {
        auto count = visualCnt;
        return append(sources, count, sourceCap, Source{object, counterpart})
            && append(visuals, visualCnt, visualCap, value);
    }

    bool add(const LayoutPath& value)
    {
        return append(paths, pathCnt, pathCap, value);
    }

    bool add(const LayoutCollision& value)
    {
        return append(collisions, collisionCnt, collisionCap, value);
    }

    bool add(const LayoutContainment& value)
    {
        return append(containments, containmentCnt, containmentCap, value);
    }
};

}  // namespace vis

#endif
