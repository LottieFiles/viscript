#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#include "vis.h"
#include "visAudit.h"
#include "visLuaHost.h"
#include "visCliCommon.h"
#include "visSaver.h"
#ifdef VIS_DIAGRAM
#include "vis_diagram.h"
#endif

using namespace vis;

namespace
{

struct Slice
{
    const char* data = nullptr;
    size_t size = 0u;
};

struct Containment
{
    Slice text;
    Slice panel;
    Slice scenePath;
    Slice diagramId;
    Slice entityKind;
    Slice entityId;
    Slice role;
    float inset = -1.0f;
    bool textSeen = false;
    bool textInvalid = false;
    bool panelInvalid = false;
    bool generated = false;
};

struct AllowedText
{
    Slice id;
    bool matched = false;
};

struct AllowedOverlap
{
    Slice firstPath;
    Slice firstId;
    Slice secondPath;
    Slice secondId;
    bool firstMatched = false;
    bool secondMatched = false;
    bool used = false;
    bool duplicate = false;
};

struct AllowedOccludedScene
{
    Slice path;
    bool matched = false;
    bool used = false;
    bool duplicate = false;
};

struct Options
{
    const char* source = nullptr;
    // Font files given with --font, in command-line order.
    const char** fonts = nullptr;
    uint32_t fontCount = 0u;
    const char* format = "json";
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t fps = 0u;
    uint32_t maxSamples = 0u;
    float textGap = 4.0f;
    float canvasInset = 4.0f;
    float panelInset = 12.0f;
    float diagramRouteGap = 2.0f;
    bool requireLedger = false;
    bool allowViewportStretch = false;
    Containment* containments = nullptr;
    uint32_t containmentCount = 0u;
    uint32_t containmentCapacity = 0u;
    AllowedText* allowedTexts = nullptr;
    uint32_t allowedTextCount = 0u;
    AllowedOverlap* allowedOverlaps = nullptr;
    uint32_t allowedOverlapCount = 0u;
    AllowedOccludedScene* allowedOccludedScenes = nullptr;
    uint32_t allowedOccludedSceneCount = 0u;

    ~Options()
    {
        delete[] fonts;
        delete[] containments;
        delete[] allowedTexts;
        delete[] allowedOverlaps;
        delete[] allowedOccludedScenes;
    }
};

struct Sample
{
    uint32_t index = 0u;
    float time = 0.0f;
    bool final = false;
    bool valid = false;
};

enum struct IssueCode : uint8_t
{
    MissingTextId,
    DuplicateId,
    CanvasOverflow,
    TextGap,
    ContainmentOverflow,
    ContainerNotVisible,
    InvalidContainerType,
    MissingContainmentOwner,
    UnmatchedContainment,
    UnmatchedUncontained,
    NeverVisibleText,
    MissingGlyphs,
    PlaneTextSize,
    ViewportStretch,
    TextOccluded,
    DuplicateOccludedSceneAllowance,
    UnmatchedOccludedSceneAllowance,
    UnusedOccludedSceneAllowance,
    DuplicateOverlapAllowance,
    UnmatchedOverlapAllowance,
    UnusedOverlapAllowance,
#ifdef VIS_DIAGRAM
    DiagramNodeOverlap,
    DiagramRouteCrossesNode,
    DiagramEdgeEndpointDetached,
    DiagramEdgePortMismatch,
    DiagramZoneExcludesMember,
    DiagramFullWidthZoneMismatch,
    DiagramEdgeEndpointDirection,
    DiagramSampledRouteNodeCollision,
    DiagramSampledRouteLabelCollision,
    DiagramSampledRouteRouteCollision,
    DiagramSampledRouteCanvasOverflow,
#endif
};

struct Subject
{
    const Object* object = nullptr;
    Slice authoredId;
};

struct Scope
{
    const Scene* scene = nullptr;
    const char* path = nullptr;
};

struct VisualEvidence
{
    Scope counterpartScope;
    Subject counterpart;
    LayoutVisualKind kind = LayoutVisualKind::Stable;
    bool valid = false;
};

struct Issue
{
    IssueCode code = IssueCode::MissingTextId;
    Scope scope;
    Scope relatedScope;
    Subject subject;
    Subject related;
    VisualEvidence visual;
    VisualEvidence relatedVisual;
    Sample first;
    Sample last;
    Sample worst;
    float actual = 0.0f;
    float required = 0.0f;
    float score = 0.0f;
    float overflowLeft = 0.0f;
    float overflowTop = 0.0f;
    float overflowRight = 0.0f;
    float overflowBottom = 0.0f;
    Vec2 separation;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    BBox subjectBounds;
    BBox relatedBounds;
    const char* provenance = nullptr;
    Slice diagramId;
    Slice entityKind;
    Slice entityId;
    Slice relatedEntityKind;
    Slice relatedEntityId;
    bool measured = false;
    bool hasOverflow = false;
    bool hasSeparation = false;
    bool hasScale = false;
    // MissingGlyphs: how many characters the font lacks, and the first of them in UTF-8.
    uint32_t missing = 0u;
    char firstMissing[5] = {};
    // PlaneTextSize: the em in world units, and the world height the camera shows at its target.
    float planeEm = 0.0f;
    float viewHeight = 0.0f;
};

struct IssueList
{
    Issue* values = nullptr;
    uint32_t count = 0u;
    uint32_t capacity = 0u;

    ~IssueList()
    {
        delete[] values;
    }

    bool grow() noexcept
    {
        auto nextCapacity = capacity ? capacity * 2u : 32u;
        if (nextCapacity < capacity)
            return false;
        auto next = new (std::nothrow) Issue[nextCapacity];
        if (!next)
            return false;
        for (auto i = 0u; i < count; i++)
            next[i] = values[i];
        delete[] values;
        values = next;
        capacity = nextCapacity;
        return true;
    }
};

struct TextRecord
{
    const Scene* scene = nullptr;
    const Object* object = nullptr;
    const char* path = nullptr;
    bool painted = false;
};

struct TextList
{
    TextRecord* values = nullptr;
    uint32_t count = 0u;
    uint32_t capacity = 0u;

    ~TextList()
    {
        delete[] values;
    }

    TextRecord* find(const Scene* scene, const Object* object, const char* path) const noexcept
    {
        for (auto i = 0u; i < count; i++)
        {
            auto& value = values[i];
            if (value.scene == scene && value.object == object && value.path == path)
                return values + i;
        }
        return nullptr;
    }

    TextRecord* add(const Scene* scene, const Object* object, const char* path) noexcept
    {
        if (auto value = find(scene, object, path))
            return value;
        if (count == capacity)
        {
            auto nextCapacity = capacity ? capacity * 2u : 32u;
            if (nextCapacity < capacity)
                return nullptr;
            auto next = new (std::nothrow) TextRecord[nextCapacity];
            if (!next)
                return nullptr;
            for (auto i = 0u; i < count; i++)
                next[i] = values[i];
            delete[] values;
            values = next;
            capacity = nextCapacity;
        }
        auto value = values + count++;
        *value = {scene, object, path, false};
        return value;
    }
};

struct ScenePath
{
    const Scene* scene = nullptr;
    char* value = nullptr;
};

struct ScenePathList
{
    ScenePath* values = nullptr;
    uint32_t count = 0u;
    uint32_t capacity = 0u;

    ~ScenePathList()
    {
        for (auto i = 0u; i < count; i++)
            delete[] values[i].value;
        delete[] values;
    }

    const char* find(const Scene* scene, const char* value) const noexcept
    {
        if (!value)
            return nullptr;
        for (auto i = 0u; i < count; i++)
        {
            if (values[i].scene == scene && std::strcmp(values[i].value, value) == 0)
                return values[i].value;
        }
        return nullptr;
    }

    bool remember(const Scene* scene, const char* value, const char*& output) noexcept
    {
        output = find(scene, value);
        if (output || !value)
            return true;
        if (count == capacity)
        {
            auto nextCapacity = capacity ? capacity * 2u : 16u;
            if (nextCapacity < capacity)
                return false;
            auto next = new (std::nothrow) ScenePath[nextCapacity];
            if (!next)
                return false;
            for (auto i = 0u; i < count; i++)
                next[i] = values[i];
            delete[] values;
            values = next;
            capacity = nextCapacity;
        }
        auto size = std::strlen(value);
        auto copy = new (std::nothrow) char[size + 1u];
        if (!copy)
            return false;
        std::memcpy(copy, value, size + 1u);
        values[count++] = {scene, copy};
        output = copy;
        return true;
    }
};

#ifdef VIS_DIAGRAM
struct DiagramRecord
{
    const diagram::IR* receipt = nullptr;
    const Scene* scene = nullptr;
    const char* path = nullptr;
    const Object* root = nullptr;
    diagram::ConstraintReport* constraints = nullptr;
    diagram::PhysicalConstraintReport* physical = nullptr;
};

struct DiagramList
{
    DiagramRecord* values = nullptr;
    uint32_t count = 0u;
    uint32_t capacity = 0u;

    ~DiagramList()
    {
        for (auto i = 0u; i < count; i++)
        {
            delete values[i].constraints;
            delete values[i].physical;
        }
        delete[] values;
    }

    bool contains(const diagram::IR* receipt, const Scene* scene) const noexcept
    {
        for (auto i = 0u; i < count; i++)
        {
            if (values[i].receipt == receipt && values[i].scene == scene)
                return true;
        }
        return false;
    }

    DiagramRecord* add(const diagram::IR* receipt, const Scene* scene, const char* path,
                       const Object* root) noexcept
    {
        if (contains(receipt, scene))
            return values;
        if (count == capacity)
        {
            auto nextCapacity = capacity ? capacity * 2u : 8u;
            if (nextCapacity < capacity)
                return nullptr;
            auto next = new (std::nothrow) DiagramRecord[nextCapacity];
            if (!next)
                return nullptr;
            for (auto i = 0u; i < count; i++)
                next[i] = values[i];
            delete[] values;
            values = next;
            capacity = nextCapacity;
        }
        auto constraints = new (std::nothrow) diagram::ConstraintReport;
        auto physical = new (std::nothrow) diagram::PhysicalConstraintReport;
        if (!constraints || !physical)
        {
            delete constraints;
            delete physical;
            return nullptr;
        }
        auto output = values + count++;
        *output = {receipt, scene, path, root, constraints, physical};
        return output;
    }
};
#endif

static void _usage()
{
    std::fprintf(stderr, "Usage:\n"
                         "  viscript audit <scene.lua> --font FILE [--font FILE]... [--format json|text]\n"
                         "              [--width PX] [--height PX] [--fps N]\n"
                         "              [--max-samples N]\n"
                         "              [--text-gap PX] [--canvas-inset PX] [--panel-inset PX]\n"
                         "              [--diagram-route-gap PX]\n"
                         "              [--contain TEXT PANEL [INSET]]\n"
                         "              [--require-containment-ledger] [--allow-uncontained "
                         "TEXT]\n"
                         "              [--allow-text-overlap FIRST_PATH FIRST_ID SECOND_PATH SECOND_ID]\n"
                         "              [--allow-occluded-scene PATH]\n"
                         "              [--allow-viewport-stretch]\n");
}

static bool _equal(Slice first, Slice second)
{
    return first.size == second.size && (!first.size || std::memcmp(first.data, second.data, first.size) == 0);
}

static bool _equal(Slice first, const char* second)
{
    return second && first.size == std::strlen(second) &&
           (!first.size || std::memcmp(first.data, second, first.size) == 0);
}

static Slice _slice(const char* value)
{
    return {value, value ? std::strlen(value) : 0u};
}

static bool _operand(const char* value)
{
    return value && value[0] && !(value[0] == '-' && value[1] == '-');
}

#ifdef VIS_DIAGRAM
static bool _reserveContainments(Options& options, uint32_t additional) noexcept
{
    if (additional > UINT32_MAX - options.containmentCount)
        return false;
    auto required = options.containmentCount + additional;
    if (required <= options.containmentCapacity)
        return true;
    auto nextCapacity = options.containmentCapacity ? options.containmentCapacity : 8u;
    while (nextCapacity < required)
    {
        if (nextCapacity > UINT32_MAX / 2u)
        {
            nextCapacity = required;
            break;
        }
        nextCapacity *= 2u;
    }
    auto next = new (std::nothrow) Containment[nextCapacity];
    if (!next)
        return false;
    for (auto i = 0u; i < options.containmentCount; i++)
        next[i] = options.containments[i];
    delete[] options.containments;
    options.containments = next;
    options.containmentCapacity = nextCapacity;
    return true;
}

static bool _appendContainment(Options& options, const Containment& containment) noexcept
{
    if (!_reserveContainments(options, 1u))
        return false;
    options.containments[options.containmentCount++] = containment;
    return true;
}
#endif

static void _json(Slice value)
{
    _jsonString(value.data, value.size);
}

static void _json(const char* value)
{
    if (value)
        _json(_slice(value));
    else
        std::fputs("null", stdout);
}

static const char* _code(IssueCode code)
{
    switch (code)
    {
    case IssueCode::MissingTextId:
        return "missing_text_id";
    case IssueCode::DuplicateId:
        return "duplicate_id";
    case IssueCode::CanvasOverflow:
        return "canvas_overflow";
    case IssueCode::TextGap:
        return "text_gap";
    case IssueCode::ContainmentOverflow:
        return "containment_overflow";
    case IssueCode::ContainerNotVisible:
        return "container_not_visible";
    case IssueCode::InvalidContainerType:
        return "invalid_container_type";
    case IssueCode::MissingContainmentOwner:
        return "missing_containment_owner";
    case IssueCode::UnmatchedContainment:
        return "unmatched_containment";
    case IssueCode::UnmatchedUncontained:
        return "unmatched_uncontained";
    case IssueCode::NeverVisibleText:
        return "never_visible_text";
    case IssueCode::MissingGlyphs:
        return "missing_glyphs";
    case IssueCode::PlaneTextSize:
        return "plane_text_size";
    case IssueCode::ViewportStretch:
        return "viewport_stretch";
    case IssueCode::TextOccluded:
        return "text_occluded";
    case IssueCode::DuplicateOccludedSceneAllowance:
        return "duplicate_occluded_scene_allowance";
    case IssueCode::UnmatchedOccludedSceneAllowance:
        return "unmatched_occluded_scene_allowance";
    case IssueCode::UnusedOccludedSceneAllowance:
        return "unused_occluded_scene_allowance";
    case IssueCode::DuplicateOverlapAllowance:
        return "duplicate_overlap_allowance";
    case IssueCode::UnmatchedOverlapAllowance:
        return "unmatched_overlap_allowance";
    case IssueCode::UnusedOverlapAllowance:
        return "unused_overlap_allowance";
#ifdef VIS_DIAGRAM
    case IssueCode::DiagramNodeOverlap:
        return "diagram_node_overlap";
    case IssueCode::DiagramRouteCrossesNode:
        return "diagram_route_crosses_node";
    case IssueCode::DiagramEdgeEndpointDetached:
        return "diagram_edge_endpoint_detached";
    case IssueCode::DiagramEdgePortMismatch:
        return "diagram_edge_port_mismatch";
    case IssueCode::DiagramZoneExcludesMember:
        return "diagram_zone_excludes_member";
    case IssueCode::DiagramFullWidthZoneMismatch:
        return "diagram_full_width_zone_mismatch";
    case IssueCode::DiagramEdgeEndpointDirection:
        return "diagram_edge_endpoint_direction";
    case IssueCode::DiagramSampledRouteNodeCollision:
        return "diagram_sampled_route_node_collision";
    case IssueCode::DiagramSampledRouteLabelCollision:
        return "diagram_sampled_route_label_collision";
    case IssueCode::DiagramSampledRouteRouteCollision:
        return "diagram_sampled_route_route_collision";
    case IssueCode::DiagramSampledRouteCanvasOverflow:
        return "diagram_sampled_route_canvas_overflow";
#endif
    }
    return "unknown";
}

static const char* _repair(IssueCode code)
{
    switch (code)
    {
    case IssueCode::MissingTextId:
        return "assign-stable-id";
    case IssueCode::DuplicateId:
        return "make-id-unique";
    case IssueCode::CanvasOverflow:
        return "move-or-reflow-content";
    case IssueCode::TextGap:
        return "separate-text";
    case IssueCode::ContainmentOverflow:
        return "grow-container-or-wrap-text";
    case IssueCode::ContainerNotVisible:
        return "align-container-visibility";
    case IssueCode::InvalidContainerType:
        return "use-rectangle-container";
    case IssueCode::MissingContainmentOwner:
        return "declare-containment-or-uncontained";
    case IssueCode::UnmatchedContainment:
        return "fix-containment-ids";
    case IssueCode::UnmatchedUncontained:
        return "fix-uncontained-id";
    case IssueCode::NeverVisibleText:
        return "register-font-or-fix-visibility";
    case IssueCode::MissingGlyphs:
        return "use-a-font-with-these-glyphs";
    case IssueCode::PlaneTextSize:
        return "give-plane-text-size-in-world-units";
    case IssueCode::ViewportStretch:
        return "match-viewport-aspect-or-reflow";
    case IssueCode::TextOccluded:
        return "remove-occluder-or-allow-scene-occlusion";
    case IssueCode::DuplicateOccludedSceneAllowance:
        return "remove-duplicate-occlusion-allowance";
    case IssueCode::UnmatchedOccludedSceneAllowance:
        return "fix-occluded-scene-path";
    case IssueCode::UnusedOccludedSceneAllowance:
        return "remove-stale-occlusion-allowance";
    case IssueCode::DuplicateOverlapAllowance:
        return "remove-duplicate-overlap-allowance";
    case IssueCode::UnmatchedOverlapAllowance:
        return "fix-overlap-scene-path-or-id";
    case IssueCode::UnusedOverlapAllowance:
        return "remove-stale-overlap-allowance";
#ifdef VIS_DIAGRAM
    case IssueCode::DiagramNodeOverlap:
        return "separate-diagram-nodes";
    case IssueCode::DiagramRouteCrossesNode:
        return "reroute-or-move-diagram-node";
    case IssueCode::DiagramEdgeEndpointDetached:
        return "reattach-diagram-edge";
    case IssueCode::DiagramEdgePortMismatch:
        return "correct-diagram-port-or-route";
    case IssueCode::DiagramZoneExcludesMember:
        return "grow-diagram-zone-or-fix-membership";
    case IssueCode::DiagramFullWidthZoneMismatch:
        return "align-full-width-diagram-zone";
    case IssueCode::DiagramEdgeEndpointDirection:
        return "align-diagram-route-with-port";
    case IssueCode::DiagramSampledRouteNodeCollision:
        return "reroute-or-adjust-diagram-motion";
    case IssueCode::DiagramSampledRouteLabelCollision:
        return "move-diagram-label-or-reroute";
    case IssueCode::DiagramSampledRouteRouteCollision:
        return "separate-diagram-route-corridors";
    case IssueCode::DiagramSampledRouteCanvasOverflow:
        return "move-diagram-route-inside-canvas";
#endif
    }
    return "review-layout";
}

static bool _subjectEqual(const Subject& first, const Subject& second)
{
    if (first.object || second.object)
        return first.object == second.object;
    return _equal(first.authoredId, second.authoredId);
}

static bool _sameIssue(const Issue& first, IssueCode code, Scope scope, const Subject& subject, Scope relatedScope,
                       const Subject& related)
{
    return first.code == code && first.scope.scene == scope.scene && first.scope.path == scope.path &&
           first.relatedScope.scene == relatedScope.scene && first.relatedScope.path == relatedScope.path &&
           _subjectEqual(first.subject, subject) && _subjectEqual(first.related, related);
}

static Issue* _record(IssueList& issues, IssueCode code, Scope scope, Subject subject, Scope relatedScope,
                      Subject related, const Sample& sample, float actual = 0.0f, float required = 0.0f,
                      float score = 1.0f)
{
    Issue* issue = nullptr;
    for (auto i = 0u; i < issues.count; i++)
    {
        if (_sameIssue(issues.values[i], code, scope, subject, relatedScope, related))
        {
            issue = issues.values + i;
            break;
        }
    }
    if (!issue)
    {
        if (issues.count == issues.capacity && !issues.grow())
            return nullptr;
        issue = issues.values + issues.count++;
        *issue = {};
        issue->code = code;
        issue->scope = scope;
        issue->relatedScope = relatedScope;
        issue->subject = subject;
        issue->related = related;
        issue->first = sample;
        issue->last = sample;
        issue->worst = sample;
        issue->actual = actual;
        issue->required = required;
        issue->score = score;
        return issue;
    }
    if (sample.valid)
        issue->last = sample;
    if (score > issue->score)
    {
        issue->worst = sample;
        issue->actual = actual;
        issue->required = required;
        issue->score = score;
    }
    return issue;
}

static bool _painted(const LayoutVisual* visual)
{
    return visual && visual->opacity > 0.0f && visual->paintBounds.width > 0.0f && visual->paintBounds.height > 0.0f;
}

// The world height the camera shows at its target, or zero when the camera does not say.
static float _viewHeight(const Camera& camera)
{
    if (camera.projection == Projection::Orthographic)
        return camera.orthoHeight > 0.0f ? camera.orthoHeight : 0.0f;
    auto dx = camera.eye.x - camera.target.x, dy = camera.eye.y - camera.target.y, dz = camera.eye.z - camera.target.z;
    auto distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(distance > 0.0f) || !(camera.fov > 0.0f))
        return 0.0f;
    return 2.0f * distance * std::tan(0.5f * camera.fov);
}

static float _clearance(const BBox& inner, const BBox& outer)
{
    auto left = inner.x - outer.x;
    auto top = inner.y - outer.y;
    auto right = outer.x + outer.width - inner.x - inner.width;
    auto bottom = outer.y + outer.height - inner.y - inner.height;
    return std::fmin(std::fmin(left, top), std::fmin(right, bottom));
}

static void _overflow(const BBox& inner, const BBox& outer, float inset, Issue& issue)
{
    issue.overflowLeft = std::fmax(0.0f, outer.x + inset - inner.x);
    issue.overflowTop = std::fmax(0.0f, outer.y + inset - inner.y);
    issue.overflowRight = std::fmax(0.0f, inner.x + inner.width - outer.x - outer.width + inset);
    issue.overflowBottom = std::fmax(0.0f, inner.y + inner.height - outer.y - outer.height + inset);
    issue.hasOverflow = true;
}

static float _axisClearance(float first, float firstSize, float second, float secondSize)
{
    auto firstEnd = first + firstSize;
    auto secondEnd = second + secondSize;
    if (firstEnd <= second)
        return second - firstEnd;
    if (secondEnd <= first)
        return first - secondEnd;
    return -std::fmin(firstEnd, secondEnd) + std::fmax(first, second);
}

static float _gap(const BBox& first, const BBox& second, Vec2& separation, float required)
{
    Vec2 clearance = {
        _axisClearance(first.x, first.width, second.x, second.width),
        _axisClearance(first.y, first.height, second.y, second.height),
    };
    auto horizontal = required - clearance.x;
    auto vertical = required - clearance.y;
    if (horizontal <= vertical)
    {
        separation.x = second.center().x < first.center().x ? -horizontal : horizontal;
    }
    else
    {
        separation.y = second.center().y < first.center().y ? -vertical : vertical;
    }
    return std::fmax(clearance.x, clearance.y);
}

static bool _endpoint(Slice allowedPath, Slice allowedId, const char* path, const Object* object)
{
    return object && object->tag() && _equal(allowedPath, path) && _equal(allowedId, object->tag());
}

static bool _sameEndpoint(Slice firstPath, Slice firstId, Slice secondPath, Slice secondId)
{
    return _equal(firstPath, secondPath) && _equal(firstId, secondId);
}

static bool _sameOverlap(const AllowedOverlap& first, const AllowedOverlap& second)
{
    auto forward = _sameEndpoint(first.firstPath, first.firstId, second.firstPath, second.firstId) &&
                   _sameEndpoint(first.secondPath, first.secondId, second.secondPath, second.secondId);
    auto reverse = _sameEndpoint(first.firstPath, first.firstId, second.secondPath, second.secondId) &&
                   _sameEndpoint(first.secondPath, first.secondId, second.firstPath, second.firstId);
    return forward || reverse;
}

static uint32_t _textIdentities(const LayoutReport& report, const LayoutVisual* visual, uint32_t (&indices)[2])
{
    auto count = 0u;
    if (!visual || visual->object >= report.objectCount())
        return count;
    auto primary = report.objectAt(visual->object);
    if (primary->object->type() == Type::Text)
        indices[count++] = visual->object;
    if (visual->kind == LayoutVisualKind::Morph && visual->counterpart < report.objectCount())
    {
        auto counterpart = report.objectAt(visual->counterpart);
        if (counterpart->object->type() == Type::Text && visual->counterpart != visual->object)
            indices[count++] = visual->counterpart;
    }
    return count;
}

static uint32_t _parentIndex(const LayoutReport& report, uint32_t index)
{
    auto object = report.objectAt(index);
    return object ? object->parent : report.objectCount();
}

// Whether two texts sit below one typeset group, at any depth, which reads as one block of
// text, such as the lines of a code block.
static bool _typeset(const LayoutReport& report, uint32_t first, uint32_t second)
{
    auto count = report.objectCount();
    for (auto group = _parentIndex(report, first); group < count; group = _parentIndex(report, group))
    {
        auto object = report.objectAt(group)->object;
        if (!object || object->type() != Type::Group || !static_cast<const Group*>(object)->typeset)
            continue;
        for (auto other = _parentIndex(report, second); other < count; other = _parentIndex(report, other))
        {
            if (other == group)
                return true;
        }
    }
    return false;
}

// Whether two texts are pieces of the source and the target of one match or cross-fade of
// their Scene playing at the sample, which meet on the way by design.
static bool _handoff(const LayoutReport& report, uint32_t first, uint32_t second)
{
    auto a = report.objectAt(first);
    auto b = report.objectAt(second);
    if (a->scene != b->scene)
        return false;
    auto scene = report.sceneAt(a->scene);
    return scene->scene && scene->scene->handoff(a->object, b->object, scene->time);
}

static bool _allowed(Options& options, const LayoutReport& report, const LayoutVisual* first,
                     const LayoutVisual* second)
{
    uint32_t firstIndices[2] = {};
    uint32_t secondIndices[2] = {};
    if (!_textIdentities(report, first, firstIndices) ||
        !_textIdentities(report, second, secondIndices))
    {
        return false;
    }
    auto firstObject = report.objectAt(firstIndices[0]);
    auto secondObject = report.objectAt(secondIndices[0]);
    auto firstPath = report.sceneAt(firstObject->scene)->path;
    auto secondPath = report.sceneAt(secondObject->scene)->path;
    for (auto allowance = 0u; allowance < options.allowedOverlapCount; allowance++)
    {
        auto& pair = options.allowedOverlaps[allowance];
        auto forward = _endpoint(pair.firstPath, pair.firstId, firstPath, firstObject->object) &&
                       _endpoint(pair.secondPath, pair.secondId, secondPath, secondObject->object);
        auto reverse = _endpoint(pair.firstPath, pair.firstId, secondPath, secondObject->object) &&
                       _endpoint(pair.secondPath, pair.secondId, firstPath, firstObject->object);
        if (forward || reverse)
        {
            pair.used = true;
            return true;
        }
    }
    return false;
}

static const LayoutObject* _find(const LayoutReport& report, uint32_t scene, Slice id, uint32_t& index,
                                 uint32_t* matches = nullptr)
{
    const LayoutObject* found = nullptr;
    index = UINT32_MAX;
    auto count = 0u;
    for (auto i = 0u; i < report.objectCount(); i++)
    {
        auto object = report.objectAt(i);
        auto tag = object->object->tag();
        if (object->scene == scene && tag && _equal(id, tag))
        {
            found = object;
            index = i;
            count++;
        }
    }
    if (matches)
        *matches = count;
    return found;
}

static const Object* _find(const Scene* scene, Slice id, uint32_t* matches = nullptr)
{
    const Object* found = nullptr;
    auto count = 0u;
    for (auto i = 0u; i < scene->count(); i++)
    {
        auto object = scene->objectAt(i);
        auto tag = object->tag();
        if (tag && _equal(id, tag))
        {
            found = object;
            count++;
        }
    }
    if (matches)
        *matches = count;
    return found;
}

static char* _childPath(const char* parent, const char* kind, uint32_t index)
{
    auto parentSize = std::strlen(parent);
    char suffix[48];
    auto suffixSize = std::snprintf(suffix, sizeof(suffix), "/%s:%u", kind, index);
    if (suffixSize < 0 || static_cast<size_t>(suffixSize) >= sizeof(suffix) ||
        parentSize > std::numeric_limits<size_t>::max() - static_cast<size_t>(suffixSize) - 1u)
    {
        return nullptr;
    }
    auto path = new (std::nothrow) char[parentSize + static_cast<size_t>(suffixSize) + 1u];
    if (!path)
        return nullptr;
    std::memcpy(path, parent, parentSize);
    std::memcpy(path + parentSize, suffix, static_cast<size_t>(suffixSize) + 1u);
    return path;
}

static Scope _scope(const LayoutReport& report, const ScenePathList& paths, uint32_t scene)
{
    auto item = report.sceneAt(scene);
    return {item->scene, paths.find(item->scene, item->path)};
}

static Subject _subject(const Object* object)
{
    Subject subject;
    subject.object = object;
    return subject;
}

static Subject _subject(Slice id)
{
    Subject subject;
    subject.authoredId = id;
    return subject;
}

static bool _evidence(const LayoutReport& report, const ScenePathList& paths, const LayoutVisual* visual,
                      uint32_t identity, VisualEvidence& evidence)
{
    if (!visual || identity >= report.objectCount())
        return false;
    evidence = {};
    evidence.kind = visual->kind;
    evidence.valid = true;
    auto counterpart = visual->counterpart;
    if (visual->kind == LayoutVisualKind::Morph && identity == visual->counterpart)
        counterpart = visual->object;
    if (counterpart == UINT32_MAX)
        return true;
    if (counterpart >= report.objectCount())
        return false;
    auto object = report.objectAt(counterpart);
    evidence.counterpartScope = _scope(report, paths, object->scene);
    if (!evidence.counterpartScope.path)
        return false;
    evidence.counterpart = _subject(object->object);
    return true;
}

static bool _visualApplies(const LayoutVisual* visual, uint32_t object)
{
    return visual->object == object || (visual->kind == LayoutVisualKind::Morph && visual->counterpart == object);
}

static bool _markPainted(const LayoutReport& report, const ScenePathList& paths, uint32_t object, TextList& texts)
{
    if (object >= report.objectCount())
        return false;
    auto item = report.objectAt(object);
    if (item->object->type() != Type::Text)
        return true;
    auto scope = _scope(report, paths, item->scene);
    if (!scope.path)
        return false;
    auto record = texts.find(scope.scene, item->object, scope.path);
    if (!record)
        return false;
    record->painted = true;
    return true;
}

static const LayoutVisual* _visualFor(const LayoutReport& report, uint32_t object)
{
    const LayoutVisual* found = nullptr;
    for (auto i = 0u; i < report.visualCount(); i++)
    {
        auto visual = report.visualAt(i);
        if (_visualApplies(visual, object))
        {
            if (_painted(visual) && visual->visible)
                return visual;
            if (!found || (_painted(visual) && !_painted(found)))
                found = visual;
        }
    }
    return found;
}

static bool _containmentApplies(const Containment& containment, const char* path)
{
    return !containment.generated || _equal(containment.scenePath, path);
}

#ifdef VIS_DIAGRAM
static bool _hasUserPolicy(const Options& options, const char* id)
{
    if (!id)
        return false;
    for (auto i = 0u; i < options.containmentCount; i++)
    {
        if (!options.containments[i].generated && _equal(options.containments[i].text, id))
            return true;
    }
    for (auto i = 0u; i < options.allowedTextCount; i++)
    {
        if (_equal(options.allowedTexts[i].id, id))
            return true;
    }
    return false;
}

static bool _hasGeneratedPolicy(const Options& options, const char* path, const char* id)
{
    if (!path || !id)
        return false;
    for (auto i = 0u; i < options.containmentCount; i++)
    {
        auto& containment = options.containments[i];
        if (containment.generated && _equal(containment.scenePath, path) && _equal(containment.text, id))
            return true;
    }
    return false;
}

static IssueCode _diagramIssueCode(diagram::ConstraintCode code)
{
    switch (code)
    {
    case diagram::ConstraintCode::NodeOverlap:
        return IssueCode::DiagramNodeOverlap;
    case diagram::ConstraintCode::RouteCrossesNode:
        return IssueCode::DiagramRouteCrossesNode;
    case diagram::ConstraintCode::EdgeEndpointDetached:
        return IssueCode::DiagramEdgeEndpointDetached;
    case diagram::ConstraintCode::EdgePortMismatch:
        return IssueCode::DiagramEdgePortMismatch;
    case diagram::ConstraintCode::ZoneExcludesMember:
        return IssueCode::DiagramZoneExcludesMember;
    case diagram::ConstraintCode::FullWidthZoneMismatch:
        return IssueCode::DiagramFullWidthZoneMismatch;
    case diagram::ConstraintCode::EdgeEndpointDirection:
        return IssueCode::DiagramEdgeEndpointDirection;
    }
    return IssueCode::DiagramNodeOverlap;
}

static Subject _diagramSubject(const diagram::IR& receipt, diagram::Node node)
{
    if (auto object = receipt.object(node))
        return _subject(object);
    if (node && node.index < receipt.nodeCount())
    {
        if (auto spec = receipt.nodeAt(node.index))
            return _subject(_slice(spec->id));
    }
    return {};
}

static Subject _diagramSubject(const diagram::IR& receipt, diagram::Edge edge)
{
    if (auto object = receipt.object(edge))
        return _subject(object);
    if (edge && edge.index < receipt.edgeCount())
    {
        if (auto spec = receipt.edgeAt(edge.index))
            return _subject(_slice(spec->id));
    }
    return {};
}

static Subject _diagramSubject(const diagram::IR& receipt, diagram::Zone zone)
{
    if (auto object = receipt.object(zone))
        return _subject(object);
    if (zone && zone.index < receipt.zoneCount())
    {
        if (auto spec = receipt.zoneAt(zone.index))
            return _subject(_slice(spec->id));
    }
    return {};
}

static Slice _diagramId(const diagram::IR& receipt, diagram::Node node)
{
    auto spec = node && node.index < receipt.nodeCount() ? receipt.nodeAt(node.index) : nullptr;
    return _slice(spec ? spec->id : nullptr);
}

static Slice _diagramId(const diagram::IR& receipt, diagram::Edge edge)
{
    auto spec = edge && edge.index < receipt.edgeCount() ? receipt.edgeAt(edge.index) : nullptr;
    return _slice(spec ? spec->id : nullptr);
}

static Slice _diagramId(const diagram::IR& receipt, diagram::Zone zone)
{
    auto spec = zone && zone.index < receipt.zoneCount() ? receipt.zoneAt(zone.index) : nullptr;
    return _slice(spec ? spec->id : nullptr);
}

static void _diagramProvenance(Issue& issue, const diagram::IR& receipt,
                               const char* provenance, const char* entityKind,
                               Slice entityId, const char* relatedKind = nullptr,
                               Slice relatedId = {})
{
    issue.provenance = provenance;
    issue.diagramId = _slice(receipt.config().id);
    issue.entityKind = _slice(entityKind);
    issue.entityId = entityId;
    issue.relatedEntityKind = _slice(relatedKind);
    issue.relatedEntityId = relatedId;
}

static bool _addDiagramContainment(Options& options, const char* path, const diagram::IR& receipt,
                                   const char* entityKind, const char* entityId, const char* role,
                                   const Text* text, const Rectangle* body)
{
    if (!text || !body || !text->tag() || !body->tag())
        return true;
    if (_hasUserPolicy(options, text->tag()) || _hasGeneratedPolicy(options, path, text->tag()))
        return true;
    Containment containment;
    containment.text = _slice(text->tag());
    containment.panel = _slice(body->tag());
    containment.scenePath = _slice(path);
    containment.diagramId = _slice(receipt.config().id);
    containment.entityKind = _slice(entityKind);
    containment.entityId = _slice(entityId);
    containment.role = _slice(role);
    containment.inset = options.panelInset;
    containment.generated = true;
    return _appendContainment(options, containment);
}

static bool _addDiagramPolicies(Options& options, const char* path, const diagram::IR& receipt)
{
    for (auto i = 0u; i < receipt.nodeCount(); i++)
    {
        auto node = diagram::Node{i};
        auto spec = receipt.nodeAt(i);
        if (!spec)
            return false;
        if (!_addDiagramContainment(options, path, receipt, "node", spec->id, "label", receipt.label(node),
                                    receipt.body(node)))
            return false;
        if (receipt.detail(node) &&
            !_addDiagramContainment(options, path, receipt, "node", spec->id, "detail", receipt.detail(node),
                                    receipt.body(node)))
            return false;
    }
    for (auto i = 0u; i < receipt.zoneCount(); i++)
    {
        auto zone = diagram::Zone{i};
        auto spec = receipt.zoneAt(i);
        if (!spec)
            return false;
        if (!_addDiagramContainment(options, path, receipt, "zone", spec->id, "label", receipt.label(zone),
                                    receipt.body(zone)))
            return false;
    }
    return true;
}

static bool _addDiagramIssues(const DiagramRecord& diagramRecord, IssueList& issues)
{
    auto& receipt = *diagramRecord.receipt;
    auto scope = Scope{diagramRecord.scene, diagramRecord.path};
    Sample structural;
    for (auto i = 0u; i < diagramRecord.constraints->count(); i++)
    {
        auto constraint = diagramRecord.constraints->issueAt(i);
        if (!constraint)
            return false;
        Subject subject;
        Subject related;
        const char* entityKind = nullptr;
        const char* relatedKind = nullptr;
        Slice entityId;
        Slice relatedId;
        switch (constraint->code)
        {
        case diagram::ConstraintCode::NodeOverlap:
            subject = _diagramSubject(receipt, constraint->node);
            related = _diagramSubject(receipt, constraint->relatedNode);
            entityKind = "node";
            relatedKind = "node";
            entityId = _diagramId(receipt, constraint->node);
            relatedId = _diagramId(receipt, constraint->relatedNode);
            break;
        case diagram::ConstraintCode::RouteCrossesNode:
        case diagram::ConstraintCode::EdgeEndpointDetached:
        case diagram::ConstraintCode::EdgePortMismatch:
        case diagram::ConstraintCode::EdgeEndpointDirection:
            subject = _diagramSubject(receipt, constraint->edge);
            related = _diagramSubject(receipt, constraint->node);
            entityKind = "edge";
            relatedKind = "node";
            entityId = _diagramId(receipt, constraint->edge);
            relatedId = _diagramId(receipt, constraint->node);
            break;
        case diagram::ConstraintCode::ZoneExcludesMember:
            subject = _diagramSubject(receipt, constraint->zone);
            related = _diagramSubject(receipt, constraint->node);
            entityKind = "zone";
            relatedKind = "node";
            entityId = _diagramId(receipt, constraint->zone);
            relatedId = _diagramId(receipt, constraint->node);
            break;
        case diagram::ConstraintCode::FullWidthZoneMismatch:
            subject = _diagramSubject(receipt, constraint->zone);
            entityKind = "zone";
            entityId = _diagramId(receipt, constraint->zone);
            break;
        }
        auto issue = _record(issues, _diagramIssueCode(constraint->code), scope, subject,
                             related.object || related.authoredId.data ? scope : Scope{},
                             related, structural);
        if (!issue)
            return false;
        _diagramProvenance(*issue, receipt, "diagram-ir", entityKind, entityId,
                           relatedKind, relatedId);
    }
    return true;
}

static IssueCode _diagramPhysicalIssueCode(diagram::PhysicalConstraintCode code)
{
    switch (code)
    {
    case diagram::PhysicalConstraintCode::RouteNodeCollision:
        return IssueCode::DiagramSampledRouteNodeCollision;
    case diagram::PhysicalConstraintCode::RouteLabelCollision:
        return IssueCode::DiagramSampledRouteLabelCollision;
    case diagram::PhysicalConstraintCode::RouteRouteCollision:
        return IssueCode::DiagramSampledRouteRouteCollision;
    case diagram::PhysicalConstraintCode::RouteCanvasOverflow:
        return IssueCode::DiagramSampledRouteCanvasOverflow;
    }
    return IssueCode::DiagramSampledRouteNodeCollision;
}

static const char* _diagramPhysicalEntityKind(diagram::PhysicalEntityKind kind)
{
    switch (kind)
    {
    case diagram::PhysicalEntityKind::None:
        return nullptr;
    case diagram::PhysicalEntityKind::NodeBody:
        return "node-body";
    case diagram::PhysicalEntityKind::NodeLabel:
        return "node-label";
    case diagram::PhysicalEntityKind::NodeDetail:
        return "node-detail";
    case diagram::PhysicalEntityKind::EdgeRoute:
        return "edge-route";
    case diagram::PhysicalEntityKind::EdgeLabel:
        return "edge-label";
    case diagram::PhysicalEntityKind::ZoneLabel:
        return "zone-label";
    }
    return nullptr;
}

static Subject _diagramPhysicalSubject(const diagram::IR& receipt,
                                       const diagram::PhysicalConstraintIssue& issue)
{
    switch (issue.relatedKind)
    {
    case diagram::PhysicalEntityKind::None:
        return {};
    case diagram::PhysicalEntityKind::NodeBody:
        return _subject(receipt.body(issue.node));
    case diagram::PhysicalEntityKind::NodeLabel:
        return _subject(receipt.label(issue.node));
    case diagram::PhysicalEntityKind::NodeDetail:
        return _subject(receipt.detail(issue.node));
    case diagram::PhysicalEntityKind::EdgeRoute:
        return _subject(receipt.object(issue.relatedEdge));
    case diagram::PhysicalEntityKind::EdgeLabel:
        return _subject(receipt.label(issue.relatedEdge));
    case diagram::PhysicalEntityKind::ZoneLabel:
        return _subject(receipt.label(issue.zone));
    }
    return {};
}

static Result _auditDiagramSample(const LayoutReport& layout, const Options& options,
                                  const Sample& sample, IssueList& issues,
                                  const DiagramList& diagrams, bool& failed)
{
    for (auto i = 0u; i < diagrams.count; i++)
    {
        auto& record = diagrams.values[i];
        auto& receipt = *record.receipt;
        auto status = diagram::validatePhysical(receipt, layout, record.path,
                                                options.diagramRouteGap,
                                                options.canvasInset, *record.physical);
        if (status != Result::Success)
            return status;
        auto scope = Scope{record.scene, record.path};
        for (auto j = 0u; j < record.physical->count(); j++)
        {
            auto physical = record.physical->issueAt(j);
            if (!physical)
                return Result::Unknown;
            auto subject = _diagramSubject(receipt, physical->edge);
            auto related = _diagramPhysicalSubject(receipt, *physical);
            Slice relatedId;
            if (physical->node)
            {
                relatedId = _diagramId(receipt, physical->node);
            }
            else if (physical->relatedEdge)
            {
                relatedId = _diagramId(receipt, physical->relatedEdge);
            }
            else if (physical->zone)
            {
                relatedId = _diagramId(receipt, physical->zone);
            }
            auto score = std::fmax(0.0f, physical->required - physical->actual);
            auto issue = _record(issues, _diagramPhysicalIssueCode(physical->code),
                                 scope, subject,
                                 related.object || related.authoredId.data ? scope : Scope{},
                                 related, sample, physical->actual,
                                 physical->required, score);
            if (!issue)
                return Result::OutOfMemory;
            if (issue->worst.index == sample.index && issue->worst.final == sample.final)
            {
                issue->subjectBounds = physical->routeBounds;
                issue->relatedBounds = physical->relatedBounds;
                issue->measured = true;
            }
            _diagramProvenance(*issue, receipt, "sampled-rendered-geometry",
                               "edge", _diagramId(receipt, physical->edge),
                               _diagramPhysicalEntityKind(physical->relatedKind),
                               relatedId);
            failed = true;
        }
    }
    return Result::Success;
}

static Result _discoverDiagrams(const Scene* scene, const char* authoredPath, Options& options,
                                IssueList& issues, ScenePathList& paths, DiagramList& diagrams)
{
    if (!scene || !authoredPath)
        return Result::InvalidArguments;
    const char* path = nullptr;
    if (!paths.remember(scene, authoredPath, path))
        return Result::OutOfMemory;
    for (auto i = 0u; i < scene->count(); i++)
    {
        auto receipt = diagram::ir(scene->objectAt(i));
        if (!receipt || diagrams.contains(receipt, scene))
            continue;
        auto record = diagrams.add(receipt, scene, path, scene->objectAt(i));
        if (!record)
            return Result::OutOfMemory;
        auto status = diagram::validate(*receipt, *record->constraints);
        if (status != Result::Success)
            return status;
        if (!_addDiagramPolicies(options, path, *receipt) || !_addDiagramIssues(*record, issues))
            return Result::OutOfMemory;
    }
    for (auto i = 0u; i < scene->viewportCount(); i++)
    {
        auto childPath = _childPath(path, "viewport", i);
        if (!childPath)
            return Result::OutOfMemory;
        auto status = _discoverDiagrams(scene->sceneAt(i), childPath, options, issues, paths, diagrams);
        delete[] childPath;
        if (status != Result::Success)
            return status;
    }
    for (auto i = 0u; i < scene->transitionCount(); i++)
    {
        auto childPath = _childPath(path, "transition", i);
        if (!childPath)
            return Result::OutOfMemory;
        auto status = _discoverDiagrams(scene->transitionAt(i), childPath, options, issues, paths, diagrams);
        delete[] childPath;
        if (status != Result::Success)
            return status;
    }
    return Result::Success;
}
#endif

static bool _covered(const Options& options, const char* id, const char* path)
{
    if (!id)
        return false;
    for (auto i = 0u; i < options.containmentCount; i++)
    {
        if (_containmentApplies(options.containments[i], path) && _equal(options.containments[i].text, id))
            return true;
    }
    for (auto i = 0u; i < options.allowedTextCount; i++)
    {
        if (_equal(options.allowedTexts[i].id, id))
            return true;
    }
    return false;
}

static bool _containmentTextMatched(const Containment& containment)
{
    return containment.textSeen && !containment.textInvalid;
}

static bool _containmentPanelMatched(const Containment& containment)
{
    return containment.textSeen && !containment.panelInvalid;
}

static bool _containmentMatched(const Containment& containment)
{
    return _containmentTextMatched(containment) && _containmentPanelMatched(containment);
}

static bool _occlusionAllowed(Options& options, const char* path)
{
    auto allowed = false;
    for (auto i = 0u; i < options.allowedOccludedSceneCount; i++)
    {
        if (_equal(options.allowedOccludedScenes[i].path, path))
        {
            options.allowedOccludedScenes[i].used = true;
            allowed = true;
        }
    }
    return allowed;
}

static bool _inventory(const Scene* scene, const char* authoredPath, Options& options, IssueList& issues,
                       TextList& texts, ScenePathList& paths)
{
    if (!scene || !authoredPath)
        return false;
    const char* path = nullptr;
    if (!paths.remember(scene, authoredPath, path))
        return false;
    Scope scope = {scene, path};
    Sample structural;

    for (auto i = 0u; i < options.allowedOccludedSceneCount; i++)
    {
        if (_equal(options.allowedOccludedScenes[i].path, path))
            options.allowedOccludedScenes[i].matched = true;
    }

    for (auto i = 0u; i < scene->count(); i++)
    {
        auto object = scene->objectAt(i);
        if (object->type() == Type::Text)
        {
            if (!texts.add(scene, object, path))
                return false;
            if (!object->tag())
            {
                if (!_record(issues, IssueCode::MissingTextId, scope, _subject(object), {}, {}, structural))
                    return false;
            }
            else if (options.requireLedger && !_covered(options, object->tag(), path))
            {
                if (!_record(issues, IssueCode::MissingContainmentOwner, scope, _subject(object), {}, {}, structural))
                    return false;
            }
            // A character the font has no glyph for vanishes from the frame without an error.
            // A text whose string changes shows only characters of the strings it changes
            // between, so they all count.
            auto text = static_cast<const Text*>(object);
            auto stringCount = text->texts(nullptr, 0u);
            auto strings = new (std::nothrow) const char*[stringCount];
            if (!strings)
                return false;
            text->texts(strings, stringCount);
            uint32_t missing = 0u;
            char firstMissing[5] = {};
            for (auto s = 0u; s < stringCount; s++)
            {
                TextExtent extent;
                if (text->measure(strings[s], extent) != Result::Success || !extent.missing)
                    continue;
                if (!missing)
                    std::memcpy(firstMissing, extent.firstMissing, sizeof(firstMissing));
                missing += extent.missing;
            }
            delete[] strings;
            if (missing)
            {
                auto issue = _record(issues, IssueCode::MissingGlyphs, scope, _subject(object), {}, {}, structural);
                if (!issue)
                    return false;
                issue->missing = missing;
                std::memcpy(issue->firstMissing, firstMissing, sizeof(issue->firstMissing));
            }
            // Plane text takes its size in world units, while billboard text takes points. A
            // size written in points on plane text makes an em taller than the whole view,
            // which covers the frame with one glyph.
            auto viewHeight = _viewHeight(scene->config().camera);
            if (text->orientation == TextOrientation::Plane && viewHeight > 0.0f && text->size > viewHeight)
            {
                auto issue = _record(issues, IssueCode::PlaneTextSize, scope, _subject(object), {}, {}, structural,
                                     text->size, viewHeight, text->size - viewHeight);
                if (!issue)
                    return false;
                issue->planeEm = text->size;
                issue->viewHeight = viewHeight;
            }
            for (auto allowed = 0u; allowed < options.allowedTextCount; allowed++)
            {
                if (object->tag() && _equal(options.allowedTexts[allowed].id, object->tag()))
                    options.allowedTexts[allowed].matched = true;
            }
            for (auto allowed = 0u; allowed < options.allowedOverlapCount; allowed++)
            {
                auto& overlap = options.allowedOverlaps[allowed];
                if (object->tag() && _equal(overlap.firstPath, path) && _equal(overlap.firstId, object->tag()))
                    overlap.firstMatched = true;
                if (object->tag() && _equal(overlap.secondPath, path) && _equal(overlap.secondId, object->tag()))
                    overlap.secondMatched = true;
            }
        }
        auto tag = object->tag();
        if (!tag)
            continue;
        for (auto j = i + 1u; j < scene->count(); j++)
        {
            auto other = scene->objectAt(j);
            if (other->tag() && std::strcmp(tag, other->tag()) == 0)
            {
                if (!_record(issues, IssueCode::DuplicateId, scope, _subject(object), scope, _subject(other),
                             structural))
                {
                    return false;
                }
            }
        }
    }

    for (auto constraint = 0u; constraint < options.containmentCount; constraint++)
    {
        auto& containment = options.containments[constraint];
        if (!_containmentApplies(containment, path))
            continue;
        uint32_t textMatches = 0u;
        uint32_t panelMatches = 0u;
        auto text = _find(scene, containment.text, &textMatches);
        auto panel = _find(scene, containment.panel, &panelMatches);
        if (!textMatches)
            continue;
        containment.textSeen = true;
        if (textMatches != 1u || text->type() != Type::Text)
            containment.textInvalid = true;
        if (panelMatches != 1u || panel->type() != Type::Rectangle)
            containment.panelInvalid = true;
        if (textMatches != 1u)
            continue;
        if (panelMatches != 1u)
        {
            if (!_record(issues, IssueCode::UnmatchedContainment, scope, _subject(text), scope,
                         _subject(containment.panel), structural))
                return false;
            continue;
        }
        if (text->type() != Type::Text || panel->type() != Type::Rectangle)
        {
            if (!_record(issues, IssueCode::InvalidContainerType, scope, _subject(text), scope, _subject(panel),
                         structural))
                return false;
        }
    }

    for (auto i = 0u; i < scene->viewportCount(); i++)
    {
        auto childPath = _childPath(path, "viewport", i);
        if (!childPath)
            return false;
        auto result = _inventory(scene->sceneAt(i), childPath, options, issues, texts, paths);
        delete[] childPath;
        if (!result)
            return false;
    }
    for (auto i = 0u; i < scene->transitionCount(); i++)
    {
        auto childPath = _childPath(path, "transition", i);
        if (!childPath)
            return false;
        auto result = _inventory(scene->transitionAt(i), childPath, options, issues, texts, paths);
        delete[] childPath;
        if (!result)
            return false;
    }
    // A view draws this scene's own objects again, so it adds a scene path to the report and
    // nothing to take stock of; an opaque view names that path when it hides a text.
    for (auto i = 0u; i < scene->viewCount(); i++)
    {
        auto childPath = _childPath(path, "view", i);
        if (!childPath)
            return false;
        const char* viewPath = nullptr;
        auto result = paths.remember(scene, childPath, viewPath);
        delete[] childPath;
        if (!result)
            return false;
    }
    return true;
}

static bool _auditSample(const LayoutReport& report, Options& options, const Sample& sample, IssueList& issues,
                         TextList& texts, const ScenePathList& paths, bool& failed)
{
    failed = false;
    for (auto i = 0u; i < report.sceneCount(); i++)
    {
        auto item = report.sceneAt(i);
        auto scope = _scope(report, paths, i);
        if (!scope.path)
            return false;
        if (item->stretched && !options.allowViewportStretch)
        {
            auto issue = _record(issues, IssueCode::ViewportStretch, scope, {}, {}, {}, sample,
                                 std::fabs(item->scaleX - item->scaleY), 0.0f, std::fabs(item->scaleX - item->scaleY));
            if (!issue)
                return false;
            if (issue->worst.index == sample.index && issue->worst.final == sample.final)
            {
                issue->scaleX = item->scaleX;
                issue->scaleY = item->scaleY;
                issue->hasScale = true;
            }
            failed = true;
        }
    }

    for (auto i = 0u; i < report.visualCount(); i++)
    {
        auto visual = report.visualAt(i);
        uint32_t identities[2] = {};
        auto identityCount = _textIdentities(report, visual, identities);
        if (!identityCount)
            continue;
        auto item = report.objectAt(identities[0]);
        auto object = item->object;
        auto scope = _scope(report, paths, item->scene);
        if (!scope.path)
            return false;
        if (_painted(visual))
        {
            for (auto identity = 0u; identity < identityCount; identity++)
            {
                if (!_markPainted(report, paths, identities[identity], texts))
                    return false;
            }
            if (visual->occluded)
            {
                if (visual->occluder >= report.sceneCount())
                    return false;
                auto occluderScope = _scope(report, paths, visual->occluder);
                if (!occluderScope.path)
                    return false;
                for (auto identity = 0u; identity < identityCount; identity++)
                {
                    auto identityItem = report.objectAt(identities[identity]);
                    auto identityScope = _scope(report, paths, identityItem->scene);
                    if (!identityScope.path)
                        return false;
                    if (_occlusionAllowed(options, identityScope.path))
                        continue;
                    auto issue = _record(issues, IssueCode::TextOccluded, identityScope,
                                         _subject(identityItem->object), occluderScope, {}, sample);
                    if (!issue)
                        return false;
                    if (issue->worst.index == sample.index && issue->worst.final == sample.final &&
                        !_evidence(report, paths, visual, identities[identity], issue->visual))
                    {
                        return false;
                    }
                    failed = true;
                }
                continue;
            }
            auto actual = _clearance(visual->paintBounds, visual->clipBounds);
            if (!visual->visible || visual->clipped || actual < options.canvasInset)
            {
                auto score = std::fmax(0.0f, options.canvasInset - actual);
                auto issue = _record(issues, IssueCode::CanvasOverflow, scope, _subject(object), {}, {}, sample, actual,
                                     options.canvasInset, score);
                if (!issue)
                    return false;
                if (issue->worst.index == sample.index && issue->worst.final == sample.final)
                {
                    issue->subjectBounds = visual->paintBounds;
                    issue->relatedBounds = visual->clipBounds;
                    issue->measured = true;
                    _overflow(visual->paintBounds, visual->clipBounds, options.canvasInset, *issue);
                    if (!_evidence(report, paths, visual, identities[0], issue->visual))
                        return false;
                }
                failed = true;
            }
        }

        if (!visual->visible || visual->occluded || !_painted(visual))
            continue;
        for (auto j = i + 1u; j < report.visualCount(); j++)
        {
            auto otherVisual = report.visualAt(j);
            uint32_t otherIdentities[2] = {};
            auto otherIdentityCount = _textIdentities(report, otherVisual, otherIdentities);
            if (!otherIdentityCount || !otherVisual->visible || otherVisual->occluded ||
                !_painted(otherVisual) ||
                !visual->paintBounds.intersects(otherVisual->paintBounds, options.textGap) ||
                _typeset(report, identities[0], otherIdentities[0]) ||
                _handoff(report, identities[0], otherIdentities[0]) ||
                _allowed(options, report, visual, otherVisual))
            {
                continue;
            }
            auto other = report.objectAt(otherIdentities[0]);
            auto otherScope = _scope(report, paths, other->scene);
            if (!otherScope.path)
                return false;
            Vec2 separation;
            auto actual = _gap(visual->paintBounds, otherVisual->paintBounds, separation, options.textGap);
            auto issue = _record(issues, IssueCode::TextGap, scope, _subject(object), otherScope,
                                 _subject(other->object), sample, actual, options.textGap, options.textGap - actual);
            if (!issue)
                return false;
            if (issue->worst.index == sample.index && issue->worst.final == sample.final)
            {
                issue->subjectBounds = visual->paintBounds;
                issue->relatedBounds = otherVisual->paintBounds;
                issue->separation = separation;
                issue->measured = true;
                issue->hasSeparation = true;
                if (!_evidence(report, paths, visual, identities[0], issue->visual) ||
                    !_evidence(report, paths, otherVisual, otherIdentities[0], issue->relatedVisual))
                {
                    return false;
                }
            }
            failed = true;
        }
    }

    for (auto constraint = 0u; constraint < options.containmentCount; constraint++)
    {
        auto& containment = options.containments[constraint];
        for (auto sceneIndex = 0u; sceneIndex < report.sceneCount(); sceneIndex++)
        {
            auto scope = _scope(report, paths, sceneIndex);
            if (!scope.path)
                return false;
            if (!_containmentApplies(containment, scope.path))
                continue;
            uint32_t textMatches = 0u;
            uint32_t panelMatches = 0u;
            uint32_t textIndex = UINT32_MAX;
            uint32_t panelIndex = UINT32_MAX;
            auto text = _find(report, sceneIndex, containment.text, textIndex, &textMatches);
            auto panel = _find(report, sceneIndex, containment.panel, panelIndex, &panelMatches);
            if (textMatches != 1u || panelMatches != 1u || text->object->type() != Type::Text ||
                panel->object->type() != Type::Rectangle)
            {
                continue;
            }
            auto textVisual = _visualFor(report, textIndex);
            if (!textVisual || textVisual->occluded || !_painted(textVisual))
                continue;
            auto panelVisual = _visualFor(report, panelIndex);
            if (!panelVisual || !panelVisual->visible || panelVisual->clipped || panelVisual->occluded ||
                !_painted(panelVisual))
            {
                auto issue = _record(issues, IssueCode::ContainerNotVisible, scope, _subject(text->object), scope,
                                     _subject(panel->object), sample);
                if (!issue)
                {
                    return false;
                }
                if (issue->worst.index == sample.index && issue->worst.final == sample.final)
                {
                    if (!_evidence(report, paths, textVisual, textIndex, issue->visual))
                        return false;
                    if (panelVisual && !_evidence(report, paths, panelVisual, panelIndex, issue->relatedVisual))
                        return false;
                }
                failed = true;
                continue;
            }
            auto actual = _clearance(textVisual->paintBounds, panelVisual->paintBounds);
            if (actual < containment.inset)
            {
                auto issue =
                    _record(issues, IssueCode::ContainmentOverflow, scope, _subject(text->object), scope,
                            _subject(panel->object), sample, actual, containment.inset, containment.inset - actual);
                if (!issue)
                    return false;
                if (issue->worst.index == sample.index && issue->worst.final == sample.final)
                {
                    issue->subjectBounds = textVisual->paintBounds;
                    issue->relatedBounds = panelVisual->paintBounds;
                    issue->measured = true;
                    _overflow(textVisual->paintBounds, panelVisual->paintBounds, containment.inset, *issue);
                    if (!_evidence(report, paths, textVisual, textIndex, issue->visual) ||
                        !_evidence(report, paths, panelVisual, panelIndex, issue->relatedVisual))
                    {
                        return false;
                    }
                }
                failed = true;
            }
        }
    }
    return true;
}

static bool _finish(Options& options, IssueList& issues, const TextList& texts)
{
    Sample structural;
    for (auto i = 0u; i < texts.count; i++)
    {
        // A text its scene never shows, as a piece a match from a copy lets go of at once, is
        // not missing from the frames.
        auto& text = texts.values[i];
        if (!text.painted && text.scene->shows(text.object) &&
            !_record(issues, IssueCode::NeverVisibleText, {text.scene, text.path}, _subject(text.object), {}, {},
                     structural))
            return false;
    }
    for (auto i = 0u; i < options.containmentCount; i++)
    {
        auto& containment = options.containments[i];
        auto scope = containment.generated ? Scope{nullptr, containment.scenePath.data} : Scope{};
        if (!containment.textSeen &&
            !_record(issues, IssueCode::UnmatchedContainment, scope, _subject(containment.text), scope,
                     _subject(containment.panel), structural))
            return false;
    }
    if (options.requireLedger)
    {
        for (auto i = 0u; i < options.allowedTextCount; i++)
        {
            if (!options.allowedTexts[i].matched && !_record(issues, IssueCode::UnmatchedUncontained, {},
                                                             _subject(options.allowedTexts[i].id), {}, {}, structural))
                return false;
        }
    }
    for (auto i = 0u; i < options.allowedOverlapCount; i++)
    {
        auto& allowed = options.allowedOverlaps[i];
        auto firstScope = Scope{nullptr, allowed.firstPath.data};
        auto secondScope = Scope{nullptr, allowed.secondPath.data};
        if (allowed.duplicate)
        {
            auto alreadyReported = false;
            for (auto previous = 0u; previous < i; previous++)
            {
                if (_sameOverlap(allowed, options.allowedOverlaps[previous]))
                {
                    alreadyReported = true;
                    break;
                }
            }
            if (alreadyReported)
                continue;
            if (!_record(issues, IssueCode::DuplicateOverlapAllowance, firstScope, _subject(allowed.firstId),
                         secondScope, _subject(allowed.secondId), structural))
                return false;
        }
        else if (!allowed.firstMatched || !allowed.secondMatched)
        {
            if (!_record(issues, IssueCode::UnmatchedOverlapAllowance, firstScope, _subject(allowed.firstId),
                         secondScope, _subject(allowed.secondId), structural))
                return false;
        }
        else if (!allowed.used &&
                 !_record(issues, IssueCode::UnusedOverlapAllowance, firstScope, _subject(allowed.firstId),
                          secondScope, _subject(allowed.secondId), structural))
        {
            return false;
        }
    }
    for (auto i = 0u; i < options.allowedOccludedSceneCount; i++)
    {
        auto& allowed = options.allowedOccludedScenes[i];
        if (allowed.duplicate)
        {
            if (!_record(issues, IssueCode::DuplicateOccludedSceneAllowance, {}, _subject(allowed.path), {}, {},
                         structural))
                return false;
        }
        else if (!allowed.matched &&
                 !_record(issues, IssueCode::UnmatchedOccludedSceneAllowance, {}, _subject(allowed.path), {}, {},
                          structural))
        {
            return false;
        }
        else if (allowed.matched && !allowed.used &&
                 !_record(issues, IssueCode::UnusedOccludedSceneAllowance, {}, _subject(allowed.path), {}, {},
                          structural))
        {
            return false;
        }
    }
    return true;
}

static void _bbox(const BBox& bounds)
{
    std::printf("{\"x\":%.6g,\"y\":%.6g,\"width\":%.6g,\"height\":%.6g}", bounds.x, bounds.y, bounds.width,
                bounds.height);
}

static void _sample(const Sample& sample)
{
    if (!sample.valid)
    {
        std::fputs("null", stdout);
        return;
    }
    std::printf("{\"kind\":\"%s\",\"index\":%u,\"time\":%.6g}", sample.final ? "final" : "frame", sample.index,
                sample.time);
}

static void _subject(const Subject& subject)
{
    std::putchar('{');
    if (subject.object)
    {
        std::printf("\"handle\":%u,\"id\":", subject.object->id());
        _json(subject.object->tag());
        std::fputs(",\"type\":", stdout);
        _json(type(subject.object->type()));
    }
    else
    {
        std::fputs("\"handle\":null,\"id\":", stdout);
        if (subject.authoredId.data)
            _json(subject.authoredId);
        else
            std::fputs("null", stdout);
        std::fputs(",\"type\":null", stdout);
    }
    std::putchar('}');
}

static const char* _kind(LayoutVisualKind kind)
{
    switch (kind)
    {
    case LayoutVisualKind::Stable:
        return "stable";
    case LayoutVisualKind::Morph:
        return "morph";
    case LayoutVisualKind::FadeOut:
        return "fade-out";
    case LayoutVisualKind::FadeIn:
        return "fade-in";
    }
    return "unknown";
}

static void _evidence(const VisualEvidence& evidence)
{
    if (!evidence.valid)
    {
        std::fputs("null", stdout);
        return;
    }
    std::fputs("{\"kind\":", stdout);
    _json(_kind(evidence.kind));
    std::fputs(",\"counterpartScenePath\":", stdout);
    _json(evidence.counterpartScope.path);
    std::fputs(",\"counterpart\":", stdout);
    if (evidence.counterpart.object || evidence.counterpart.authoredId.data)
        _subject(evidence.counterpart);
    else
        std::fputs("null", stdout);
    std::putchar('}');
}

static void _policy(const Options& options)
{
    std::fputs("{\"font\":{\"path\":", stdout);
    _json(options.fonts[0]);
    std::fputs(",\"paths\":[", stdout);
    for (auto i = 0u; i < options.fontCount; i++)
    {
        if (i)
            std::putchar(',');
        _json(options.fonts[i]);
    }
    std::printf("]},\"thresholds\":{\"textGap\":%.6g,\"canvasInset\":%.6g,"
                "\"panelInset\":%.6g,\"diagramRouteGap\":%.6g},"
                "\"containmentLedger\":{\"required\":%s,\"relationships\":[",
                options.textGap, options.canvasInset, options.panelInset,
                options.diagramRouteGap, options.requireLedger ? "true" : "false");
    for (auto i = 0u; i < options.containmentCount; i++)
    {
        auto& containment = options.containments[i];
        if (i)
            std::putchar(',');
        std::fputs("{\"textId\":", stdout);
        _json(containment.text);
        std::fputs(",\"panelId\":", stdout);
        _json(containment.panel);
        std::printf(",\"inset\":%.6g,\"textMatched\":%s,\"panelMatched\":%s,\"matched\":%s",
                    containment.inset, _containmentTextMatched(containment) ? "true" : "false",
                    _containmentPanelMatched(containment) ? "true" : "false",
                    _containmentMatched(containment) ? "true" : "false");
        if (containment.generated)
        {
            std::fputs(",\"provenance\":{\"kind\":\"diagram-ir\",\"scenePath\":", stdout);
            _json(containment.scenePath);
            std::fputs(",\"diagramId\":", stdout);
            _json(containment.diagramId);
            std::fputs(",\"entityKind\":", stdout);
            _json(containment.entityKind);
            std::fputs(",\"entityId\":", stdout);
            _json(containment.entityId);
            std::fputs(",\"role\":", stdout);
            _json(containment.role);
            std::putchar('}');
        }
        std::putchar('}');
    }
    std::fputs("],\"uncontained\":[", stdout);
    for (auto i = 0u; i < options.allowedTextCount; i++)
    {
        auto& allowed = options.allowedTexts[i];
        if (i)
            std::putchar(',');
        std::fputs("{\"id\":", stdout);
        _json(allowed.id);
        std::printf(",\"matched\":%s}", allowed.matched ? "true" : "false");
    }
    std::fputs("]},\"textOverlapAllowances\":[", stdout);
    for (auto i = 0u; i < options.allowedOverlapCount; i++)
    {
        auto& allowed = options.allowedOverlaps[i];
        if (i)
            std::putchar(',');
        std::fputs("{\"first\":{\"scenePath\":", stdout);
        _json(allowed.firstPath);
        std::fputs(",\"id\":", stdout);
        _json(allowed.firstId);
        std::printf(",\"matched\":%s},\"second\":{\"scenePath\":",
                    allowed.firstMatched ? "true" : "false");
        _json(allowed.secondPath);
        std::fputs(",\"id\":", stdout);
        _json(allowed.secondId);
        std::printf(",\"matched\":%s},\"matched\":%s,\"used\":%s,\"duplicate\":%s}",
                    allowed.secondMatched ? "true" : "false",
                    allowed.firstMatched && allowed.secondMatched ? "true" : "false",
                    allowed.used ? "true" : "false", allowed.duplicate ? "true" : "false");
    }
    std::fputs("],\"occludedSceneAllowances\":[", stdout);
    for (auto i = 0u; i < options.allowedOccludedSceneCount; i++)
    {
        auto& allowed = options.allowedOccludedScenes[i];
        if (i)
            std::putchar(',');
        std::fputs("{\"scenePath\":", stdout);
        _json(allowed.path);
        std::printf(",\"matched\":%s,\"used\":%s,\"duplicate\":%s}",
                    allowed.matched ? "true" : "false", allowed.used ? "true" : "false",
                    allowed.duplicate ? "true" : "false");
    }
    std::printf("],\"viewportStretch\":{\"allowed\":%s}}", options.allowViewportStretch ? "true" : "false");
}

#ifdef VIS_DIAGRAM
static const char* _diagramDirection(diagram::Direction direction)
{
    switch (direction)
    {
    case diagram::Direction::LeftToRight:
        return "left-to-right";
    case diagram::Direction::TopToBottom:
        return "top-to-bottom";
    }
    return "unknown";
}

static const char* _diagramLayout(diagram::Layout layout)
{
    switch (layout)
    {
    case diagram::Layout::Ranked:
        return "ranked";
    case diagram::Layout::Manual:
        return "manual";
    case diagram::Layout::Grid:
        return "grid";
    case diagram::Layout::Timeline:
        return "timeline";
    }
    return "unknown";
}

static const char* _diagramNodeKind(diagram::NodeKind kind)
{
    switch (kind)
    {
    case diagram::NodeKind::Default:
        return "default";
    case diagram::NodeKind::State:
        return "state";
    case diagram::NodeKind::Decision:
        return "decision";
    case diagram::NodeKind::Terminal:
        return "terminal";
    case diagram::NodeKind::Entity:
        return "entity";
    case diagram::NodeKind::Cell:
        return "cell";
    case diagram::NodeKind::Task:
        return "task";
    case diagram::NodeKind::Evidence:
        return "evidence";
    }
    return "unknown";
}

static const char* _diagramEdgeKind(diagram::EdgeKind kind)
{
    switch (kind)
    {
    case diagram::EdgeKind::Directed:
        return "directed";
    case diagram::EdgeKind::Relation:
        return "relation";
    case diagram::EdgeKind::Return:
        return "return";
    case diagram::EdgeKind::Error:
        return "error";
    case diagram::EdgeKind::Optional:
        return "optional";
    }
    return "unknown";
}

static const char* _diagramRoute(diagram::Route route)
{
    switch (route)
    {
    case diagram::Route::Auto:
        return "auto";
    case diagram::Route::Straight:
        return "straight";
    case diagram::Route::Orthogonal:
        return "orthogonal";
    }
    return "unknown";
}

static const char* _diagramPort(diagram::Port port)
{
    switch (port)
    {
    case diagram::Port::Auto:
        return "auto";
    case diagram::Port::Left:
        return "left";
    case diagram::Port::Right:
        return "right";
    case diagram::Port::Top:
        return "top";
    case diagram::Port::Bottom:
        return "bottom";
    }
    return "unknown";
}

static const char* _diagramEndpoint(diagram::EdgeEndpoint endpoint)
{
    switch (endpoint)
    {
    case diagram::EdgeEndpoint::None:
        return "none";
    case diagram::EdgeEndpoint::From:
        return "from";
    case diagram::EdgeEndpoint::To:
        return "to";
    }
    return "unknown";
}

static void _diagramVec(const Vec2& value)
{
    std::printf("{\"x\":%.6g,\"y\":%.6g}", value.x, value.y);
}

static void _diagramBinding(const Object* object)
{
    if (!object)
    {
        std::fputs("null", stdout);
        return;
    }
    std::fputs("{\"id\":", stdout);
    _json(object->tag());
    std::fputs(",\"type\":", stdout);
    _json(type(object->type()));
    std::putchar('}');
}

static void _diagramNodeId(const diagram::IR& receipt, diagram::Node node)
{
    auto spec = node && node.index < receipt.nodeCount() ? receipt.nodeAt(node.index) : nullptr;
    _json(spec ? spec->id : nullptr);
}

static void _diagramEdgeId(const diagram::IR& receipt, diagram::Edge edge)
{
    auto spec = edge && edge.index < receipt.edgeCount() ? receipt.edgeAt(edge.index) : nullptr;
    _json(spec ? spec->id : nullptr);
}

static void _diagramZoneId(const diagram::IR& receipt, diagram::Zone zone)
{
    auto spec = zone && zone.index < receipt.zoneCount() ? receipt.zoneAt(zone.index) : nullptr;
    _json(spec ? spec->id : nullptr);
}

static void _diagramConstraints(const DiagramRecord& record)
{
    auto& receipt = *record.receipt;
    auto& constraints = *record.constraints;
    std::printf("{\"valid\":%s,\"issues\":[", constraints.count() ? "false" : "true");
    for (auto i = 0u; i < constraints.count(); i++)
    {
        auto issue = constraints.issueAt(i);
        if (i)
            std::putchar(',');
        std::fputs("{\"code\":", stdout);
        _json(_code(_diagramIssueCode(issue->code)));
        std::fputs(",\"nodeId\":", stdout);
        _diagramNodeId(receipt, issue->node);
        std::fputs(",\"relatedNodeId\":", stdout);
        _diagramNodeId(receipt, issue->relatedNode);
        std::fputs(",\"edgeId\":", stdout);
        _diagramEdgeId(receipt, issue->edge);
        std::fputs(",\"zoneId\":", stdout);
        _diagramZoneId(receipt, issue->zone);
        std::fputs(",\"endpoint\":", stdout);
        _json(_diagramEndpoint(issue->endpoint));
        std::fputs(",\"port\":", stdout);
        _json(_diagramPort(issue->port));
        std::fputs(",\"repair\":", stdout);
        _json(_repair(_diagramIssueCode(issue->code)));
        std::putchar('}');
    }
    std::fputs("]}", stdout);
}

static void _diagramReceipt(const DiagramRecord& record)
{
    auto& receipt = *record.receipt;
    auto& config = receipt.config();
    std::fputs("{\"kind\":", stdout);
    _json(diagram::IR::Kind);
    std::printf(",\"version\":%u,\"layoutAlgorithm\":", diagram::IR::Version);
    _json(diagram::IR::LayoutAlgorithm);
    std::fputs(",\"scenePath\":", stdout);
    _json(record.path);
    std::fputs(",\"root\":", stdout);
    _diagramBinding(record.root);
    std::printf(",\"byteSize\":%zu,\"config\":{\"id\":", receipt.byteSize());
    _json(config.id);
    std::fputs(",\"direction\":", stdout);
    _json(_diagramDirection(config.direction));
    std::fputs(",\"layout\":", stdout);
    _json(_diagramLayout(config.layout));
    std::fputs(",\"origin\":", stdout);
    _diagramVec(config.origin);
    std::fputs(",\"nodeSize\":", stdout);
    _diagramVec(config.nodeSize);
    std::printf(",\"rankGap\":%.6g,\"nodeGap\":%.6g,\"zonePadding\":%.6g,"
                "\"routeWidth\":%.6g,\"arrowLength\":%.6g,\"arrowWidth\":%.6g,"
                "\"corner\":%.6g,\"timeUnit\":%.6g,\"layers\":{\"zones\":%d,"
                "\"routes\":%d,\"nodes\":%d,\"annotations\":%d}},\"nodes\":[",
                config.rankGap, config.nodeGap, config.zonePadding, config.routeWidth,
                config.arrowLength, config.arrowWidth, config.corner, config.timeUnit,
                config.layers.zones, config.layers.routes, config.layers.nodes,
                config.layers.annotations);
    for (auto i = 0u; i < receipt.nodeCount(); i++)
    {
        auto node = diagram::Node{i};
        auto spec = receipt.nodeAt(i);
        auto placement = receipt.nodePlacementAt(i);
        if (i)
            std::putchar(',');
        std::fputs("{\"id\":", stdout);
        _json(spec->id);
        std::fputs(",\"label\":", stdout);
        _json(spec->label);
        std::fputs(",\"detail\":", stdout);
        _json(spec->detail);
        std::fputs(",\"kind\":", stdout);
        _json(_diagramNodeKind(spec->kind));
        std::printf(",\"rank\":%d,\"manualPosition\":%s,\"position\":", spec->rank,
                    spec->manualPosition ? "true" : "false");
        _diagramVec(spec->position);
        std::fputs(",\"size\":", stdout);
        _diagramVec(spec->size);
        std::printf(",\"row\":%d,\"column\":%d,\"start\":%.6g,\"span\":%.6g,"
                    "\"placement\":{\"center\":",
                    spec->row, spec->column, spec->start, spec->span);
        _diagramVec(placement->center);
        std::fputs(",\"size\":", stdout);
        _diagramVec(placement->size);
        std::printf(",\"rank\":%u},\"bindings\":{\"object\":", placement->rank);
        _diagramBinding(receipt.object(node));
        std::fputs(",\"body\":", stdout);
        _diagramBinding(receipt.body(node));
        std::fputs(",\"label\":", stdout);
        _diagramBinding(receipt.label(node));
        std::fputs(",\"detail\":", stdout);
        _diagramBinding(receipt.detail(node));
        std::fputs("}}", stdout);
    }
    std::fputs("],\"edges\":[", stdout);
    for (auto i = 0u; i < receipt.edgeCount(); i++)
    {
        auto edge = diagram::Edge{i};
        auto spec = receipt.edgeAt(i);
        auto placement = receipt.edgePlacementAt(i);
        if (i)
            std::putchar(',');
        std::fputs("{\"id\":", stdout);
        _json(spec->id);
        std::fputs(",\"label\":", stdout);
        _json(spec->label);
        std::fputs(",\"kind\":", stdout);
        _json(_diagramEdgeKind(spec->kind));
        std::fputs(",\"fromNodeId\":", stdout);
        _diagramNodeId(receipt, spec->from);
        std::fputs(",\"toNodeId\":", stdout);
        _diagramNodeId(receipt, spec->to);
        std::fputs(",\"fromPort\":", stdout);
        _json(_diagramPort(spec->fromPort));
        std::fputs(",\"toPort\":", stdout);
        _json(_diagramPort(spec->toPort));
        std::fputs(",\"route\":", stdout);
        _json(_diagramRoute(spec->route));
        std::fputs(",\"waypoints\":[", stdout);
        for (auto j = 0u; j < spec->waypointCount; j++)
        {
            if (j)
                std::putchar(',');
            _diagramVec(spec->waypoints[j]);
        }
        std::printf("],\"flowOffset\":%.6g,\"flow\":%s,\"placement\":{\"centerline\":[",
                    spec->flowOffset, spec->flow ? "true" : "false");
        for (auto j = 0u; j < placement->centerlineCount; j++)
        {
            if (j)
                std::putchar(',');
            _diagramVec(placement->centerline[j]);
        }
        std::fputs("],\"labelPoint\":", stdout);
        if (placement->hasLabelPoint)
            _diagramVec(placement->labelPoint);
        else
            std::fputs("null", stdout);
        std::fputs(",\"fromPort\":", stdout);
        _json(_diagramPort(placement->fromPort));
        std::fputs(",\"toPort\":", stdout);
        _json(_diagramPort(placement->toPort));
        std::fputs(",\"route\":", stdout);
        _json(_diagramRoute(placement->route));
        std::fputs("},\"bindings\":{\"object\":", stdout);
        _diagramBinding(receipt.object(edge));
        std::fputs(",\"label\":", stdout);
        _diagramBinding(receipt.label(edge));
        std::fputs("}}", stdout);
    }
    std::fputs("],\"zones\":[", stdout);
    for (auto i = 0u; i < receipt.zoneCount(); i++)
    {
        auto zone = diagram::Zone{i};
        auto spec = receipt.zoneAt(i);
        auto placement = receipt.zonePlacementAt(i);
        if (i)
            std::putchar(',');
        std::fputs("{\"id\":", stdout);
        _json(spec->id);
        std::fputs(",\"label\":", stdout);
        _json(spec->label);
        std::fputs(",\"memberNodeIds\":[", stdout);
        for (auto j = 0u; j < spec->memberCount; j++)
        {
            if (j)
                std::putchar(',');
            _diagramNodeId(receipt, spec->members[j]);
        }
        std::printf("],\"fullWidth\":%s,\"placement\":{\"center\":",
                    spec->fullWidth ? "true" : "false");
        _diagramVec(placement->center);
        std::fputs(",\"size\":", stdout);
        _diagramVec(placement->size);
        std::fputs("},\"bindings\":{\"object\":", stdout);
        _diagramBinding(receipt.object(zone));
        std::fputs(",\"body\":", stdout);
        _diagramBinding(receipt.body(zone));
        std::fputs(",\"label\":", stdout);
        _diagramBinding(receipt.label(zone));
        std::fputs("}}", stdout);
    }
    std::fputs("],\"constraints\":", stdout);
    _diagramConstraints(record);
    std::putchar('}');
}

static void _diagramReceipts(const DiagramList& diagrams)
{
    std::putchar('[');
    for (auto i = 0u; i < diagrams.count; i++)
    {
        if (i)
            std::putchar(',');
        _diagramReceipt(diagrams.values[i]);
    }
    std::putchar(']');
}
#endif

static void _jsonReport(const Options& options, const Scene* scene, const IssueList& issues, uint32_t fps,
                        uint32_t encodedFrames, uint32_t samples, uint32_t failingGeometrySamples
#ifdef VIS_DIAGRAM
                        , const DiagramList& diagrams
#endif
)
{
    auto& config = scene->config();
    std::fputs("{\"schema\":\"viscript.layout-audit/v1\",\"source\":", stdout);
    _json(options.source);
    std::printf(",\"profile\":{\"width\":%u,\"height\":%u,\"fps\":%u,\"duration\":%.6g}", config.width, config.height,
                fps, scene->duration());
    std::printf(",\"sampling\":{\"encodedFrames\":%u,\"samples\":%u,"
                "\"finalSampleIncluded\":true}",
                encodedFrames, samples);
    std::printf(",\"thresholds\":{\"textGap\":%.6g,\"canvasInset\":%.6g,"
                "\"panelInset\":%.6g,\"diagramRouteGap\":%.6g}",
                options.textGap, options.canvasInset, options.panelInset,
                options.diagramRouteGap);
    std::fputs(",\"policy\":", stdout);
    _policy(options);
#ifdef VIS_DIAGRAM
    std::fputs(",\"diagramIR\":", stdout);
    _diagramReceipts(diagrams);
#endif
    std::fputs(",\"issues\":[", stdout);
    for (auto i = 0u; i < issues.count; i++)
    {
        auto& issue = issues.values[i];
        if (i)
            std::putchar(',');
        std::fputs("{\"code\":", stdout);
        _json(_code(issue.code));
        std::fputs(",\"severity\":\"error\",\"scenePath\":", stdout);
        _json(issue.scope.path);
        std::fputs(",\"relatedScenePath\":", stdout);
        _json(issue.relatedScope.path);
        std::fputs(",\"subject\":", stdout);
        _subject(issue.subject);
        std::fputs(",\"related\":", stdout);
        if (issue.related.object || issue.related.authoredId.data)
            _subject(issue.related);
        else
            std::fputs("null", stdout);
        std::fputs(",\"visual\":", stdout);
        _evidence(issue.visual);
        std::fputs(",\"relatedVisual\":", stdout);
        _evidence(issue.relatedVisual);
        std::fputs(",\"provenance\":", stdout);
        if (issue.provenance)
        {
            std::fputs("{\"kind\":", stdout);
            _json(issue.provenance);
            std::fputs(",\"diagramId\":", stdout);
            _json(issue.diagramId);
            std::fputs(",\"subject\":{\"kind\":", stdout);
            _json(issue.entityKind);
            std::fputs(",\"id\":", stdout);
            _json(issue.entityId);
            std::fputs("},\"related\":", stdout);
            if (issue.relatedEntityKind.data)
            {
                std::fputs("{\"kind\":", stdout);
                _json(issue.relatedEntityKind);
                std::fputs(",\"id\":", stdout);
                _json(issue.relatedEntityId);
                std::putchar('}');
            }
            else
            {
                std::fputs("null", stdout);
            }
            std::putchar('}');
        }
        else
        {
            std::fputs("null", stdout);
        }
        std::fputs(",\"firstSample\":", stdout);
        _sample(issue.first);
        std::fputs(",\"lastSample\":", stdout);
        _sample(issue.last);
        std::fputs(",\"worstSample\":", stdout);
        _sample(issue.worst);
        if (issue.measured)
        {
            std::printf(",\"measurement\":{\"actual\":%.6g,\"required\":%.6g,"
                        "\"deficit\":%.6g}",
                        issue.actual, issue.required, issue.score);
            std::fputs(",\"bounds\":{\"subject\":", stdout);
            _bbox(issue.subjectBounds);
            std::fputs(",\"related\":", stdout);
            _bbox(issue.relatedBounds);
            std::putchar('}');
        }
        if (issue.hasOverflow)
        {
            std::printf(",\"overflow\":{\"left\":%.6g,\"top\":%.6g,\"right\":%.6g,"
                        "\"bottom\":%.6g}",
                        issue.overflowLeft, issue.overflowTop, issue.overflowRight, issue.overflowBottom);
        }
        if (issue.hasSeparation)
        {
            std::printf(",\"separation\":{\"x\":%.6g,\"y\":%.6g}", issue.separation.x, issue.separation.y);
        }
        if (issue.hasScale)
        {
            std::printf(",\"scale\":{\"x\":%.6g,\"y\":%.6g}", issue.scaleX, issue.scaleY);
        }
        if (issue.missing)
        {
            std::printf(",\"glyphs\":{\"missing\":%u,\"first\":", issue.missing);
            if (issue.firstMissing[0])
                _json(issue.firstMissing);
            else
                std::fputs("null", stdout);
            std::putchar('}');
        }
        if (issue.code == IssueCode::PlaneTextSize)
            std::printf(",\"size\":{\"em\":%.6g,\"viewHeight\":%.6g}", issue.planeEm, issue.viewHeight);
        std::fputs(",\"repair\":", stdout);
        _json(_repair(issue.code));
        std::putchar('}');
    }
    std::printf("],\"summary\":{\"errors\":%u,\"failingGeometrySamples\":%u},\"valid\":%s}\n", issues.count,
                failingGeometrySamples, issues.count ? "false" : "true");
}

static void _textSubject(const Subject& subject)
{
    if (subject.object)
    {
        std::printf("%s#%u", type(subject.object->type()), subject.object->id());
        if (subject.object->tag())
            std::printf(" id=%s", subject.object->tag());
    }
    else if (subject.authoredId.data)
    {
        std::fwrite(subject.authoredId.data, 1, subject.authoredId.size, stdout);
    }
    else
    {
        std::fputs("scene", stdout);
    }
}

static void _textBounds(const BBox& bounds)
{
    std::printf("[x=%.3f y=%.3f width=%.3f height=%.3f]", bounds.x, bounds.y, bounds.width, bounds.height);
}

static void _textReport(const Options& options, const IssueList& issues, uint32_t samples)
{
    std::printf("viscript audit: %s (%u issue%s, %u samples)\n", issues.count ? "FAIL" : "PASS", issues.count,
                issues.count == 1u ? "" : "s", samples);
    for (auto i = 0u; i < issues.count; i++)
    {
        auto& issue = issues.values[i];
        std::printf("- [%s] ", _code(issue.code));
        if (issue.scope.path)
            std::printf("%s ", issue.scope.path);
        _textSubject(issue.subject);
        if (issue.related.object || issue.related.authoredId.data)
        {
            std::fputs(" with ", stdout);
            _textSubject(issue.related);
        }
        else if (issue.relatedScope.path)
            std::fputs(" with scene", stdout);
        if (issue.relatedScope.path)
            std::printf(" [relatedScenePath=%s]", issue.relatedScope.path);
        if (issue.measured)
        {
            std::printf(" measurement[actual=%.3fpx required=%.3fpx deficit=%.3fpx] bounds[subject=",
                        issue.actual, issue.required, issue.score);
            _textBounds(issue.subjectBounds);
            std::fputs(" related=", stdout);
            _textBounds(issue.relatedBounds);
            std::putchar(']');
        }
        if (issue.hasOverflow)
            std::printf(" overflow[left=%.3f top=%.3f right=%.3f bottom=%.3f]", issue.overflowLeft,
                        issue.overflowTop, issue.overflowRight, issue.overflowBottom);
        if (issue.hasSeparation)
            std::printf(" separation[x=%.3f y=%.3f]", issue.separation.x, issue.separation.y);
        if (issue.hasScale)
            std::printf(" scale[x=%.6g y=%.6g]", issue.scaleX, issue.scaleY);
        if (issue.missing)
            std::printf(" glyphs[missing=%u first=%s]", issue.missing, issue.firstMissing[0] ? issue.firstMissing : "?");
        if (issue.code == IssueCode::PlaneTextSize)
            std::printf(" size[em=%.3f viewHeight=%.3f]", issue.planeEm, issue.viewHeight);
        if (issue.worst.valid)
            std::printf(" at %.6gs", issue.worst.time);
        std::printf("; %s\n", _repair(issue.code));
    }
    static_cast<void>(options);
}

enum struct ParseResult : uint8_t
{
    Success,
    Invalid,
    OutOfMemory,
};

static ParseResult _parse(int argc, char** argv, Options& options)
{
    if (argc < 3)
        return ParseResult::Invalid;
    options.source = argv[2];
    options.fonts = new (std::nothrow) const char*[argc]{};
    options.containments = new (std::nothrow) Containment[argc]{};
    options.containmentCapacity = static_cast<uint32_t>(argc);
    options.allowedTexts = new (std::nothrow) AllowedText[argc]{};
    options.allowedOverlaps = new (std::nothrow) AllowedOverlap[argc]{};
    options.allowedOccludedScenes = new (std::nothrow) AllowedOccludedScene[argc]{};
    if (!options.fonts || !options.containments || !options.allowedTexts || !options.allowedOverlaps ||
        !options.allowedOccludedScenes)
        return ParseResult::OutOfMemory;

    for (auto i = 3; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc && _operand(argv[i + 1]))
        {
            options.fonts[options.fontCount++] = argv[++i];
        }
        else if (std::strcmp(argv[i], "--format") == 0 && i + 1 < argc &&
                 (std::strcmp(argv[i + 1], "json") == 0 || std::strcmp(argv[i + 1], "text") == 0))
        {
            options.format = argv[++i];
        }
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc && _uint(argv[i + 1], options.width))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc && _uint(argv[i + 1], options.height))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc && _uint(argv[i + 1], options.fps))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--max-samples") == 0 && i + 1 < argc &&
                 _uint(argv[i + 1], options.maxSamples))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--text-gap") == 0 && i + 1 < argc && _float(argv[i + 1], options.textGap))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--canvas-inset") == 0 && i + 1 < argc &&
                 _float(argv[i + 1], options.canvasInset))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--panel-inset") == 0 && i + 1 < argc && _float(argv[i + 1], options.panelInset))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--diagram-route-gap") == 0 && i + 1 < argc &&
                 _float(argv[i + 1], options.diagramRouteGap))
        {
            i++;
        }
        else if (std::strcmp(argv[i], "--contain") == 0 && i + 2 < argc && _operand(argv[i + 1]) &&
                 _operand(argv[i + 2]))
        {
            auto& containment = options.containments[options.containmentCount++];
            containment.text = _slice(argv[++i]);
            containment.panel = _slice(argv[++i]);
            float inset = 0.0f;
            if (i + 1 < argc && _float(argv[i + 1], inset))
            {
                containment.inset = inset;
                i++;
            }
        }
        else if (std::strcmp(argv[i], "--require-containment-ledger") == 0)
        {
            options.requireLedger = true;
        }
        else if (std::strcmp(argv[i], "--allow-uncontained") == 0 && i + 1 < argc && _operand(argv[i + 1]))
        {
            options.allowedTexts[options.allowedTextCount++].id = _slice(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--allow-text-overlap") == 0 && i + 4 < argc && _operand(argv[i + 1]) &&
                 _operand(argv[i + 2]) && _operand(argv[i + 3]) && _operand(argv[i + 4]))
        {
            auto& overlap = options.allowedOverlaps[options.allowedOverlapCount++];
            overlap.firstPath = _slice(argv[++i]);
            overlap.firstId = _slice(argv[++i]);
            overlap.secondPath = _slice(argv[++i]);
            overlap.secondId = _slice(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--allow-occluded-scene") == 0 && i + 1 < argc && _operand(argv[i + 1]))
        {
            options.allowedOccludedScenes[options.allowedOccludedSceneCount++].path = _slice(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--allow-viewport-stretch") == 0)
        {
            options.allowViewportStretch = true;
        }
        else
        {
            return ParseResult::Invalid;
        }
    }
    if (!options.fontCount)
        return ParseResult::Invalid;
    for (auto i = 0u; i < options.fontCount; i++)
    {
        if (!options.fonts[i][0])
            return ParseResult::Invalid;
    }
    if (options.allowedTextCount && !options.requireLedger)
        return ParseResult::Invalid;
    for (auto i = 0u; i < options.containmentCount; i++)
    {
        if (options.containments[i].inset < 0.0f)
            options.containments[i].inset = options.panelInset;
        for (auto j = i + 1u; j < options.containmentCount; j++)
        {
            if (_equal(options.containments[i].text, options.containments[j].text))
                return ParseResult::Invalid;
        }
        for (auto j = 0u; j < options.allowedTextCount; j++)
        {
            if (_equal(options.containments[i].text, options.allowedTexts[j].id))
                return ParseResult::Invalid;
        }
    }
    for (auto i = 0u; i < options.allowedTextCount; i++)
    {
        for (auto j = i + 1u; j < options.allowedTextCount; j++)
        {
            if (_equal(options.allowedTexts[i].id, options.allowedTexts[j].id))
                return ParseResult::Invalid;
        }
    }
    for (auto i = 0u; i < options.allowedOverlapCount; i++)
    {
        auto& first = options.allowedOverlaps[i];
        for (auto j = i + 1u; j < options.allowedOverlapCount; j++)
        {
            auto& second = options.allowedOverlaps[j];
            if (_sameOverlap(first, second))
            {
                first.duplicate = true;
                second.duplicate = true;
            }
        }
    }
    for (auto i = 0u; i < options.allowedOccludedSceneCount; i++)
    {
        for (auto j = i + 1u; j < options.allowedOccludedSceneCount; j++)
        {
            if (_equal(options.allowedOccludedScenes[i].path, options.allowedOccludedScenes[j].path))
            {
                options.allowedOccludedScenes[i].duplicate = true;
                options.allowedOccludedScenes[j].duplicate = true;
            }
        }
    }
    return ParseResult::Success;
}

} // namespace

int visAudit(int argc, char** argv) noexcept
{
    Options options;
    auto parse = _parse(argc, argv, options);
    if (parse != ParseResult::Success)
    {
        if (parse == ParseResult::OutOfMemory)
        {
            std::fprintf(stderr, "viscript: audit failed: out of memory\n");
            return 4;
        }
        _usage();
        return 2;
    }

    // Fonts load before the script runs, which may measure text in them.
    auto renderer = Renderer::gen(RenderEngine::Cpu);
    if (!renderer)
    {
        std::fprintf(stderr, "viscript: SW renderer is unavailable\n");
        return 3;
    }
    for (auto i = 0u; i < options.fontCount; i++)
    {
        if (renderer->font(options.fonts[i]) == Result::Success)
            continue;
        auto reason = renderer->diagnostic();
        if (reason)
            std::fprintf(stderr, "viscript: %s\n", reason);
        else
            std::fprintf(stderr, "viscript: could not load the font file '%s'\n", options.fonts[i]);
        delete renderer;
        return 3;
    }

    auto scene = _load(options.source);
    if (!scene)
    {
        delete renderer;
        return 2;
    }
    auto& config = scene->config();
    if (options.width)
        config.width = options.width;
    if (options.height)
        config.height = options.height;
    if (options.fps)
        config.fps = options.fps;

    auto fps = config.fps;
    uint32_t encodedFrames = 0u;
    auto timeline = saver::timeline(scene, fps, encodedFrames);
    if (timeline != Result::Success)
    {
        std::fprintf(stderr, "viscript: audit timeline failed: the scene needs a positive fps, width and "
                             "height, and a duration whose frame count fits in 32 bits\n");
        delete renderer;
        delete scene;
        return 4;
    }
    auto duration = scene->duration();
    if (duration > 0.0f && encodedFrames == UINT32_MAX)
    {
        std::fprintf(stderr, "viscript: audit sample count exceeds the supported range\n");
        delete renderer;
        delete scene;
        return 4;
    }
    auto totalSamples = encodedFrames + (duration > 0.0f ? 1u : 0u);
    if (options.maxSamples && totalSamples > options.maxSamples)
    {
        std::fprintf(stderr, "viscript: audit sample count %u exceeds the configured limit %u\n",
                     totalSamples, options.maxSamples);
        delete renderer;
        delete scene;
        return 4;
    }

    IssueList issues;
    TextList texts;
    ScenePathList paths;
#ifdef VIS_DIAGRAM
    DiagramList diagrams;
    auto diagramStatus = _discoverDiagrams(scene, "root", options, issues, paths, diagrams);
    if (diagramStatus != Result::Success)
    {
        std::fprintf(stderr, "viscript: audit Diagram IR failed: %s\n",
                     diagramStatus == Result::OutOfMemory ? "out of memory" : "the scene could not be read");
        delete renderer;
        delete scene;
        return 4;
    }
#endif
    if (!_inventory(scene, "root", options, issues, texts, paths))
    {
        std::fprintf(stderr, "viscript: audit failed: out of memory\n");
        delete renderer;
        delete scene;
        return 4;
    }
    uint32_t sampleCount = 0u;
    uint32_t failingGeometrySamples = 0u;
    LayoutReport report;
    auto failed = false;
    for (auto index = 0u; index < totalSamples; index++)
    {
        Sample sample;
        sample.index = index;
        sample.final = index == encodedFrames;
        sample.time = sample.final ? duration : static_cast<float>(index) / fps;
        sample.valid = true;
        auto status = renderer->layout(scene, sample.time, report, options.textGap);
        if (status != Result::Success)
        {
            auto reason = renderer->diagnostic();
            std::fprintf(stderr, "viscript: audit layout failed at %.6gs: %s\n", sample.time,
                         reason ? reason : status == Result::OutOfMemory ? "out of memory" : "the scene could not be painted");
            delete renderer;
            delete scene;
            return 4;
        }
        if (!_auditSample(report, options, sample, issues, texts, paths, failed))
        {
            std::fprintf(stderr, "viscript: audit failed: inconsistent scene inventory or out of memory\n");
            delete renderer;
            delete scene;
            return 4;
        }
#ifdef VIS_DIAGRAM
        auto diagramSample = _auditDiagramSample(report, options, sample, issues,
                                                 diagrams, failed);
        if (diagramSample != Result::Success)
        {
            std::fprintf(stderr, "viscript: audit sampled Diagram geometry failed at %.6gs: %s\n",
                         sample.time, diagramSample == Result::OutOfMemory ? "out of memory"
                                                                          : "the sampled geometry does not match the diagram");
            delete renderer;
            delete scene;
            return 4;
        }
#endif
        if (failed)
            failingGeometrySamples++;
        sampleCount++;
    }

    if (!_finish(options, issues, texts))
    {
        std::fprintf(stderr, "viscript: audit failed: out of memory\n");
        delete renderer;
        delete scene;
        return 4;
    }
    if (std::strcmp(options.format, "json") == 0)
    {
        _jsonReport(options, scene, issues, fps, encodedFrames, sampleCount, failingGeometrySamples
#ifdef VIS_DIAGRAM
                    , diagrams
#endif
        );
    }
    else
    {
        _textReport(options, issues, sampleCount);
    }
    auto exitCode = issues.count ? 1 : 0;
    delete renderer;
    delete scene;
    return exitCode;
}
