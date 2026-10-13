#ifndef _VIS_DIAGRAM_PRIVATE_H_
#define _VIS_DIAGRAM_PRIVATE_H_

#include "vis_diagram.h"

namespace vis::diagram::detail
{

struct NodeRecord
{
    char* id = nullptr;
    char* label = nullptr;
    char* detail = nullptr;
    NodeKind kind = NodeKind::Default;
    int32_t rank = -1;
    int32_t row = -1;
    int32_t column = -1;
    float start = 0.0f;
    float span = 0.0f;
    Vec2 position;
    Vec2 size;
    bool manualPosition = false;
};

struct EdgeRecord
{
    char* id = nullptr;
    char* label = nullptr;
    Node from;
    Node to;
    Port fromPort = Port::Auto;
    Port toPort = Port::Auto;
    Route route = Route::Auto;
    EdgeKind kind = EdgeKind::Directed;
    Vec2* waypoints = nullptr;
    uint32_t waypointCount = 0;
    float flowOffset = 0.0f;
    bool flow = false;
};

struct ZoneRecord
{
    char* id = nullptr;
    char* label = nullptr;
    Node* members = nullptr;
    uint32_t memberCount = 0;
    bool fullWidth = false;
};

struct NodeLayout
{
    Vec2 center;
    Vec2 size;
    uint32_t rank = 0;
};

struct EdgeLayout
{
    Port fromPort = Port::Auto;
    Port toPort = Port::Auto;
    Vec2 from;
    Vec2 to;
};

struct ZoneLayout
{
    Vec2 center;
    Vec2 size;
};

static inline bool _dashed(const EdgeRecord& edge) noexcept
{
    return edge.flow || edge.kind == EdgeKind::Return || edge.kind == EdgeKind::Error
           || edge.kind == EdgeKind::Optional;
}

Result layoutNodes(const Config& config, const NodeRecord* nodes, uint32_t nodeCount,
                   const EdgeRecord* edges, uint32_t edgeCount, NodeLayout* layouts) noexcept;
Result layoutEdges(const Config& config, const EdgeRecord* edges, uint32_t edgeCount,
                   const NodeLayout* layouts, EdgeLayout* edgeLayouts) noexcept;
Result layoutZones(const Config& config, const ZoneRecord* zones, uint32_t zoneCount,
                   const NodeLayout* layouts, uint32_t nodeCount, ZoneLayout* zoneLayouts) noexcept;

bool axisAligned(const Vec2* points, uint32_t count) noexcept;
uint32_t centerline(const Config& config, const EdgeRecord& edge, const EdgeLayout& layout,
                    Route& route, const NodeLayout* nodes, uint32_t nodeCount, Vec2* output) noexcept;
Path* orthogonalPath(const Config& config, const Vec2* route, uint32_t count) noexcept;
Vec2 labelPoint(const Config& config, const Vec2* points, uint32_t count) noexcept;

}  // namespace vis::diagram::detail

#endif
