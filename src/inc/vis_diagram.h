// The diagram module's public API: nodes, edges and zones, their layouts, routes and constraint
// reports, built into a scene.
#ifndef _VIS_DIAGRAM_H_
#define _VIS_DIAGRAM_H_

#include <cstddef>

#include "vis.h"

namespace vis::diagram
{

namespace receipt_detail
{
struct DiagramRoot;
}

enum struct Direction : uint8_t
{
    LeftToRight = 0,
    TopToBottom
};

enum struct Layout : uint8_t
{
    Ranked = 0,
    Manual,
    Grid,
    Timeline
};

enum struct NodeKind : uint8_t
{
    Default = 0,
    State,
    Decision,
    Terminal,
    Entity,
    Cell,
    Task,
    Evidence
};

enum struct EdgeKind : uint8_t
{
    Directed = 0,
    Relation,
    Return,
    Error,
    Optional
};

enum struct Route : uint8_t
{
    Auto = 0,
    Straight,
    Orthogonal
};

enum struct Port : uint8_t
{
    Auto = 0,
    Left,
    Right,
    Top,
    Bottom
};

struct Node
{
    static constexpr uint32_t Invalid = 0xffffffffu;
    uint32_t index = Invalid;

    explicit operator bool() const noexcept { return index != Invalid; }
};

struct Edge
{
    static constexpr uint32_t Invalid = 0xffffffffu;
    uint32_t index = Invalid;

    explicit operator bool() const noexcept { return index != Invalid; }
};

struct Zone
{
    static constexpr uint32_t Invalid = 0xffffffffu;
    uint32_t index = Invalid;

    explicit operator bool() const noexcept { return index != Invalid; }
};

struct Layers
{
    int32_t zones = -30;
    int32_t routes = -20;
    int32_t nodes = 0;
    int32_t annotations = 10;
};

struct Config
{
    const char* id = "diagram";
    Direction direction = Direction::LeftToRight;
    Vec2 origin;
    Vec2 nodeSize = {3.2f, 1.2f};
    float rankGap = 2.2f;
    float nodeGap = 0.7f;
    float zonePadding = 0.55f;
    float routeWidth = 0.08f;
    float arrowLength = 0.32f;
    float arrowWidth = 0.28f;
    // A negative corner follows the Theme given to build(): its roundness times the shorter
    // side of nodeSize. Zero or more is an explicit radius in world units. Zones round 1.5
    // times as much, and Terminal nodes are pills unless the corner resolves to 0.
    float corner = -1.0f;
    Layers layers;
    Layout layout = Layout::Ranked;
    float timeUnit = 1.0f;
};

struct NodeSpec
{
    const char* id = nullptr;
    const char* label = nullptr;
    const char* detail = nullptr;
    int32_t rank = -1;
    Vec2 position;
    Vec2 size;
    bool manualPosition = false;
    NodeKind kind = NodeKind::Default;
    int32_t row = -1;
    int32_t column = -1;
    float start = 0.0f;
    float span = 0.0f;
};

struct EdgeSpec
{
    const char* id = nullptr;
    const char* label = nullptr;
    Node from;
    Node to;
    Port fromPort = Port::Auto;
    Port toPort = Port::Auto;
    Route route = Route::Auto;
    const Vec2* waypoints = nullptr;
    uint32_t waypointCount = 0;
    float flowOffset = 0.0f;
    bool flow = false;
    EdgeKind kind = EdgeKind::Directed;
};

struct ZoneSpec
{
    const char* id = nullptr;
    const char* label = nullptr;
    const Node* members = nullptr;
    uint32_t memberCount = 0;
    bool fullWidth = false;
};

struct NodePlacement
{
    Vec2 center;
    Vec2 size;
    uint32_t rank = 0u;
};

struct EdgePlacement
{
    const Vec2* centerline = nullptr;
    uint32_t centerlineCount = 0u;
    Vec2 labelPoint;
    Port fromPort = Port::Auto;
    Port toPort = Port::Auto;
    Route route = Route::Auto;
    bool hasLabelPoint = false;
};

struct ZonePlacement
{
    Vec2 center;
    Vec2 size;
};

enum struct ConstraintCode : uint8_t
{
    NodeOverlap = 0,
    RouteCrossesNode,
    EdgeEndpointDetached,
    EdgePortMismatch,
    ZoneExcludesMember,
    FullWidthZoneMismatch,
    EdgeEndpointDirection
};

enum struct EdgeEndpoint : uint8_t
{
    None = 0,
    From,
    To
};

struct ConstraintIssue
{
    Node node;
    Node relatedNode;
    Edge edge;
    Zone zone;
    ConstraintCode code = ConstraintCode::NodeOverlap;
    EdgeEndpoint endpoint = EdgeEndpoint::None;
    Port port = Port::Auto;
};

struct ConstraintReport
{
    ConstraintReport() noexcept;
    ~ConstraintReport();
    ConstraintReport(const ConstraintReport&) = delete;
    ConstraintReport& operator=(const ConstraintReport&) = delete;

    uint32_t count() const noexcept;
    const ConstraintIssue* issueAt(uint32_t index) const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;
    friend Result validate(const struct IR& receipt, ConstraintReport& report) noexcept;
};

enum struct PhysicalConstraintCode : uint8_t
{
    RouteNodeCollision = 0,
    RouteLabelCollision,
    RouteRouteCollision,
    RouteCanvasOverflow
};

enum struct PhysicalEntityKind : uint8_t
{
    None = 0,
    NodeBody,
    NodeLabel,
    NodeDetail,
    EdgeRoute,
    EdgeLabel,
    ZoneLabel
};

struct PhysicalConstraintIssue
{
    Edge edge;
    Edge relatedEdge;
    Node node;
    Zone zone;
    PhysicalConstraintCode code = PhysicalConstraintCode::RouteNodeCollision;
    PhysicalEntityKind relatedKind = PhysicalEntityKind::None;
    float actual = 0.0f;
    float required = 0.0f;
    BBox routeBounds;
    BBox relatedBounds;
};

struct PhysicalConstraintReport
{
    PhysicalConstraintReport() noexcept;
    ~PhysicalConstraintReport();
    PhysicalConstraintReport(const PhysicalConstraintReport&) = delete;
    PhysicalConstraintReport& operator=(const PhysicalConstraintReport&) = delete;

    uint32_t count() const noexcept;
    const PhysicalConstraintIssue* issueAt(uint32_t index) const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;
    friend Result validatePhysical(const struct IR& receipt, const LayoutReport& layout,
                                   const char* scenePath, float routeGap,
                                   float canvasInset,
                                   PhysicalConstraintReport& report) noexcept;
};

// An immutable deep snapshot owned by the generated root. Returned pointers remain
// valid only while that root is alive.
struct IR
{
    static constexpr const char* Kind = "vis.diagram.compile-receipt";
    static constexpr uint32_t Version = 1u;
    static constexpr const char* LayoutAlgorithm = "vis.diagram.layout/v2";

    IR(const IR&) = delete;
    IR& operator=(const IR&) = delete;

    const Config& config() const noexcept;
    uint32_t nodeCount() const noexcept;
    const NodeSpec* nodeAt(uint32_t index) const noexcept;
    const NodePlacement* nodePlacementAt(uint32_t index) const noexcept;
    uint32_t edgeCount() const noexcept;
    const EdgeSpec* edgeAt(uint32_t index) const noexcept;
    const EdgePlacement* edgePlacementAt(uint32_t index) const noexcept;
    uint32_t zoneCount() const noexcept;
    const ZoneSpec* zoneAt(uint32_t index) const noexcept;
    const ZonePlacement* zonePlacementAt(uint32_t index) const noexcept;

    const Object* object(Node node) const noexcept;
    const Rectangle* body(Node node) const noexcept;
    const Text* label(Node node) const noexcept;
    const Text* detail(Node node) const noexcept;
    const Object* object(Edge edge) const noexcept;
    const Text* label(Edge edge) const noexcept;
    const Object* object(Zone zone) const noexcept;
    const Rectangle* body(Zone zone) const noexcept;
    const Text* label(Zone zone) const noexcept;
    size_t byteSize() const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;

    ~IR();
    explicit IR(Impl* impl) noexcept;
    friend struct Diagram;
    friend struct Built;
    friend struct receipt_detail::DiagramRoot;
};

// Success means validation completed; semantic violations are returned in report.
// Validation never mutates the receipt or changes whether Diagram::build() succeeds.
Result validate(const IR& receipt, ConstraintReport& report) noexcept;
// Checks sampled root-pixel geometry without changing routes. Dashes use a continuous
// corridor; endpoint families and bounded shared junctions are exempt. Sampled-frame
// evidence does not establish continuous or raster/3D collision freedom.
Result validatePhysical(const IR& receipt, const LayoutReport& layout,
                        const char* scenePath, float routeGap, float canvasInset,
                        PhysicalConstraintReport& report) noexcept;
// Only the exact root produced by Diagram::build() carries a receipt.
const IR* ir(const Object* root) noexcept;

struct Built
{
    ~Built();
    Built(const Built&) = delete;
    Built& operator=(const Built&) = delete;

    // The Built instance owns this detached tree until release() transfers it.
    Group* root() const noexcept;
    Group* release() noexcept;
    Group* zones() const noexcept;
    Group* routes() const noexcept;
    Group* nodes() const noexcept;
    Group* annotations() const noexcept;

    // These read the receipt that root() owns, so after release() they may be called only
    // while the released tree is alive.
    Object* object(Node node) const noexcept;
    Rectangle* body(Node node) const noexcept;
    Text* label(Node node) const noexcept;
    Text* detail(Node node) const noexcept;

    Object* object(Edge edge) const noexcept;
    Text* label(Edge edge) const noexcept;

    Object* object(Zone zone) const noexcept;
    Rectangle* body(Zone zone) const noexcept;
    Text* label(Zone zone) const noexcept;
    // The returned receipt is owned by root(), including after release().
    const IR* ir() const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;

    explicit Built(Impl* impl) noexcept;
    friend struct Diagram;
};

struct Diagram
{
    static constexpr uint32_t NodeLimit = 256u;
    static constexpr uint32_t EdgeLimit = 512u;
    static constexpr uint32_t ZoneLimit = 64u;
    static constexpr uint32_t WaypointLimit = 16u;
    static constexpr uint32_t IdLimit = 63u;

    ~Diagram();
    Diagram(const Diagram&) = delete;
    Diagram& operator=(const Diagram&) = delete;

    static Diagram* gen(const Config& config = {}) noexcept;
    Result add(const NodeSpec& spec, Node& output) noexcept;
    Result add(const EdgeSpec& spec, Edge& output) noexcept;
    Result add(const ZoneSpec& spec, Zone& output) noexcept;

    uint32_t nodeCount() const noexcept;
    uint32_t edgeCount() const noexcept;
    uint32_t zoneCount() const noexcept;
    // output must be null and remains null when construction fails atomically.
    // Layout/routing use bounded search and may retain unresolved fallback geometry.
    // Ranked pins remain fixed; automatic nodes reflow with local float-ULP spacing
    // tolerance, never overlap tolerance. Manual/timeline/straight/waypoint geometry stays
    // authored. Automatic routes respect port normals and terminal arrow length.
    // A failed route search does not prove global infeasibility or canvas fit.
    Result build(const Theme& theme, Built*& output) const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;

    explicit Diagram(Impl* impl) noexcept;
};

struct Lua
{
    static Result load(const char* source, uint32_t size, const char* name,
                       Scene** scene, char* error, uint32_t errorSize,
                       vis::Lua::AssetResolver resolver = nullptr,
                       void* resolverData = nullptr,
                       const Theme* adaptiveTheme = nullptr,
                       bool* adaptiveThemeUsed = nullptr) noexcept;
};

}  // namespace vis::diagram

#endif
