// LayoutReport::Impl: the records a layout report keeps and grows.
#ifndef _VIS_RENDERER_DATA_H_
#define _VIS_RENDERER_DATA_H_

#include <new>

#include "vis.h"
#include "../common/visGrow.h"

namespace vis
{

struct LayoutReport::Impl
{
    LayoutScene* scenes = nullptr;
    LayoutObject* objects = nullptr;
    LayoutVisual* visuals = nullptr;
    LayoutPath* paths = nullptr;
    const Object** visualObjects = nullptr;
    const Object** visualCounterparts = nullptr;
    LayoutCollision* collisions = nullptr;
    LayoutContainment* containments = nullptr;
    uint32_t sceneCnt = 0;
    uint32_t sceneCap = 0;
    uint32_t objectCnt = 0;
    uint32_t objectCap = 0;
    uint32_t visualCnt = 0;
    uint32_t visualCap = 0;
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
        delete[] paths;
        delete[] visualObjects;
        delete[] visualCounterparts;
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

    bool add(const LayoutScene& value)
    {
        if (!grow(scenes, sceneCnt, sceneCap, 1u, 16u)) return false;
        scenes[sceneCnt++] = value;
        return true;
    }

    bool add(const LayoutObject& value)
    {
        if (!grow(objects, objectCnt, objectCap, 1u, 16u)) return false;
        objects[objectCnt++] = value;
        return true;
    }

    bool add(const LayoutVisual& value, const Object* object,
             const Object* counterpart)
    {
        if (visualCnt >= visualCap) {
            auto next = visualCap ? visualCap * 2u : 16u;
            if (next < visualCap || next <= visualCnt) next = visualCnt + 1u;
            auto grown = new (std::nothrow) LayoutVisual[next];
            auto grownObjects = new (std::nothrow) const Object*[next];
            auto grownCounterparts = new (std::nothrow) const Object*[next];
            if (!grown || !grownObjects || !grownCounterparts) {
                delete[] grown;
                delete[] grownObjects;
                delete[] grownCounterparts;
                return false;
            }
            for (auto i = 0u; i < visualCnt; i++) {
                grown[i] = visuals[i];
                grownObjects[i] = visualObjects[i];
                grownCounterparts[i] = visualCounterparts[i];
            }
            delete[] visuals;
            delete[] visualObjects;
            delete[] visualCounterparts;
            visuals = grown;
            visualObjects = grownObjects;
            visualCounterparts = grownCounterparts;
            visualCap = next;
        }
        visuals[visualCnt] = value;
        visualObjects[visualCnt] = object;
        visualCounterparts[visualCnt++] = counterpart;
        return true;
    }

    bool add(const LayoutCollision& value)
    {
        if (!grow(collisions, collisionCnt, collisionCap, 1u, 16u)) return false;
        collisions[collisionCnt++] = value;
        return true;
    }

    bool add(const LayoutPath& value)
    {
        if (!grow(paths, pathCnt, pathCap, 1u, 16u)) return false;
        paths[pathCnt++] = value;
        return true;
    }

    bool add(const LayoutContainment& value)
    {
        if (!grow(containments, containmentCnt, containmentCap, 1u, 16u)) return false;
        containments[containmentCnt++] = value;
        return true;
    }
};

}  // namespace vis

#endif
