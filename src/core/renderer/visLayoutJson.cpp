// The JSON a layout report is written as (see visLayoutJson.h).
#include "visLayoutJson.h"

namespace vis
{

const char* layoutSceneKind(LayoutSceneKind kind) noexcept
{
    switch (kind) {
        case LayoutSceneKind::Root: return "root";
        case LayoutSceneKind::Viewport: return "viewport";
        case LayoutSceneKind::Transition: return "transition";
        case LayoutSceneKind::View: return "view";
    }
    return "unknown";
}

const char* layoutVisualKind(LayoutVisualKind kind) noexcept
{
    switch (kind) {
        case LayoutVisualKind::Stable: return "stable";
        case LayoutVisualKind::Morph: return "morph";
        case LayoutVisualKind::FadeOut: return "fade-out";
        case LayoutVisualKind::FadeIn: return "fade-in";
    }
    return "unknown";
}

static const char* _bool(bool value)
{
    return value ? "true" : "false";
}

static bool _painted(const BBox& bounds)
{
    return bounds.width > 0.0f && bounds.height > 0.0f;
}

static bool _box(JsonBuffer& json, const BBox& bounds)
{
    return json.format("{\"x\":%.9g,\"y\":%.9g,\"width\":%.9g,\"height\":%.9g}", bounds.x, bounds.y,
                       bounds.width, bounds.height);
}

// The box, or null when there is none to give.
static bool _box(JsonBuffer& json, const BBox& bounds, bool given)
{
    return given ? _box(json, bounds) : json.append("null");
}

// An index into a list of the report, or null for none.
static bool _index(JsonBuffer& json, uint32_t index)
{
    return index == UINT32_MAX ? json.append("null") : json.format("%u", index);
}

static bool _scene(JsonBuffer& json, const LayoutScene& scene, uint32_t index)
{
    return json.format("{\"index\":%u,\"path\":", index) && json.string(scene.path)
        && json.append(",\"kind\":") && json.string(layoutSceneKind(scene.kind))
        && json.append(",\"parent\":") && _index(json, scene.parent)
        && json.format(",\"depth\":%u,\"bounds\":", scene.depth) && _box(json, scene.bounds)
        && json.append(",\"clipBounds\":") && _box(json, scene.clipBounds)
        && json.format(",\"scale\":{\"x\":%.9g,\"y\":%.9g},\"clipped\":%s,\"stretched\":%s}", scene.scaleX,
                       scene.scaleY, _bool(scene.clipped), _bool(scene.stretched));
}

// outside: the object paints, but all its paint lies outside its clip; familyOutside, the same
// for the object and its descendants together.
static bool _object(JsonBuffer& json, const LayoutReport& report, const LayoutObject& item, uint32_t index)
{
    auto object = item.object;
    auto typeset = object->type() == Type::Group && static_cast<const Group*>(object)->typeset;
    auto painted = _painted(item.paintBounds);
    auto familyPainted = _painted(item.familyBounds);
    return json.format("{\"index\":%u,\"scene\":%u,\"scenePath\":", index, item.scene)
        && json.string(report.sceneAt(item.scene)->path)
        && json.format(",\"handle\":%u,\"id\":", object->id()) && json.string(object->tag())
        && json.append(",\"parent\":") && _index(json, item.parent)
        && json.append(",\"type\":") && json.string(type(object->type()))
        && json.format(",\"typeset\":%s,\"layer\":%d,\"paintBounds\":", _bool(typeset), item.layer)
        && _box(json, item.paintBounds, painted)
        && json.append(",\"familyBounds\":") && _box(json, item.familyBounds, familyPainted)
        && json.append(",\"visibleBounds\":") && _box(json, item.visibleBounds, item.visible)
        && json.append(",\"visibleFamilyBounds\":") && _box(json, item.visibleFamilyBounds, item.familyVisible)
        && json.append(",\"clipBounds\":") && _box(json, item.clipBounds)
        && json.format(",\"visible\":%s,\"familyVisible\":%s,\"clipped\":%s,\"familyClipped\":%s,"
                       "\"outside\":%s,\"familyOutside\":%s}",
                       _bool(item.visible), _bool(item.familyVisible), _bool(item.clipped),
                       _bool(item.familyClipped), _bool(painted && !item.visible),
                       _bool(familyPainted && !item.familyVisible));
}

static bool _visual(JsonBuffer& json, const LayoutVisual& visual, uint32_t index)
{
    return json.format("{\"index\":%u,\"object\":%u,\"counterpart\":", index, visual.object)
        && _index(json, visual.counterpart)
        && json.append(",\"occluder\":") && _index(json, visual.occluder)
        && json.append(",\"kind\":") && json.string(layoutVisualKind(visual.kind))
        && json.format(",\"layer\":%d,\"paintBounds\":", visual.layer)
        && _box(json, visual.paintBounds, _painted(visual.paintBounds))
        && json.append(",\"visibleBounds\":") && _box(json, visual.visibleBounds, visual.visible)
        && json.append(",\"clipBounds\":") && _box(json, visual.clipBounds)
        && json.format(",\"opacity\":%.9g,\"visible\":%s,\"clipped\":%s,\"occluded\":%s}", visual.opacity,
                       _bool(visual.visible), _bool(visual.clipped), _bool(visual.occluded));
}

static bool _path(JsonBuffer& json, const LayoutPath& path, uint32_t index)
{
    auto written = json.format("{\"index\":%u,\"visual\":%u,\"paintBounds\":", index, path.visual)
                && _box(json, path.paintBounds)
                && json.format(",\"strokeWidth\":%.9g,\"closed\":%s,\"filled\":%s,\"stroked\":%s,\"points\":[",
                               path.strokeWidth, _bool(path.closed), _bool(path.filled), _bool(path.stroked));
    for (auto i = 0u; written && i < path.count; i++) {
        written = (!i || json.append(","))
               && json.format("{\"x\":%.9g,\"y\":%.9g}", path.points[i].x, path.points[i].y);
    }
    return written && json.append("]}");
}

static bool _collision(JsonBuffer& json, const LayoutCollision& collision)
{
    return json.format("{\"first\":%u,\"second\":%u,\"kind\":\"%s\",\"overlap\":", collision.first,
                       collision.second, collision.overlapping ? "overlap" : "insufficient-gap")
        && _box(json, collision.overlap, collision.overlapping)
        && json.format(",\"clearance\":{\"x\":%.9g,\"y\":%.9g},\"separation\":{\"x\":%.9g,\"y\":%.9g}}",
                       collision.clearance.x, collision.clearance.y, collision.separation.x,
                       collision.separation.y);
}

static bool _containment(JsonBuffer& json, const LayoutContainment& containment)
{
    return json.format("{\"container\":%u,\"content\":%u,\"depth\":%u,\"inset\":%.9g,\"contained\":%s,"
                       "\"overflow\":{\"left\":%.9g,\"top\":%.9g,\"right\":%.9g,\"bottom\":%.9g}}",
                       containment.container, containment.content, containment.depth, containment.inset,
                       _bool(containment.contained), containment.overflowLeft, containment.overflowTop,
                       containment.overflowRight, containment.overflowBottom);
}

bool layoutJson(const LayoutReport& report, const Scene* root, JsonBuffer& json) noexcept
{
    auto& config = root->config();
    auto written = json.format("{\"time\":%.9g,\"padding\":%.9g,\"width\":%u,\"height\":%u,\"scenes\":[",
                               report.time(), report.padding(), config.width, config.height);
    for (auto i = 0u; written && i < report.sceneCount(); i++)
        written = (!i || json.append(",")) && _scene(json, *report.sceneAt(i), i);
    written = written && json.append("],\"objects\":[");
    for (auto i = 0u; written && i < report.objectCount(); i++)
        written = (!i || json.append(",")) && _object(json, report, *report.objectAt(i), i);
    written = written && json.append("],\"visuals\":[");
    for (auto i = 0u; written && i < report.visualCount(); i++)
        written = (!i || json.append(",")) && _visual(json, *report.visualAt(i), i);
    written = written && json.append("],\"paths\":[");
    for (auto i = 0u; written && i < report.pathCount(); i++)
        written = (!i || json.append(",")) && _path(json, *report.pathAt(i), i);
    written = written && json.append("],\"collisions\":[");
    for (auto i = 0u; written && i < report.collisionCount(); i++)
        written = (!i || json.append(",")) && _collision(json, *report.collisionAt(i));
    written = written && json.append("],\"containments\":[");
    for (auto i = 0u; written && i < report.containmentCount(); i++)
        written = (!i || json.append(",")) && _containment(json, *report.containmentAt(i));
    return written && json.append("]}");
}

}  // namespace vis
