// The diagram module's internal records and the layout passes its sources share.
#ifndef _VIS_DIAGRAM_PRIVATE_H_
#define _VIS_DIAGRAM_PRIVATE_H_

#include "vis_diagram.h"
#include "../common/visGrow.h"

namespace vis::diagram::detail
{

// Diagram::add() keeps each spec as a record with the same fields, its strings, waypoints and
// members copied into arrays the record owns.
using NodeRecord = NodeSpec;
using EdgeRecord = EdgeSpec;
using ZoneRecord = ZoneSpec;

// The layout places nodes and zones as the receipt reports them.
using NodeLayout = NodePlacement;
using ZoneLayout = ZonePlacement;

struct EdgeLayout
{
    Port fromPort = Port::Auto;
    Port toPort = Port::Auto;
    Vec2 from;
    Vec2 to;
};

// The issues a report holds in the order its checks found them: the Impl of ConstraintReport
// and of PhysicalConstraintReport.
template<typename Issue>
struct IssueList
{
    Issue* issues = nullptr;
    uint32_t count = 0u;
    uint32_t capacity = 0u;

    ~IssueList()
    {
        delete[] issues;
    }

    bool add(const Issue& issue) noexcept
    {
        if (!grow(issues, count, capacity, 1u, 8u)) return false;
        issues[count++] = issue;
        return true;
    }
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
