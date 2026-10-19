// The scene's private side: visual states, object entries, clips, cameras, views, transitions,
// markers, saves, warps and Scene::Impl, which the renderer and the modules read.
#ifndef _VIS_SCENE_H_
#define _VIS_SCENE_H_

#include <cmath>

#include "vis.h"
#include "visTimeline.h"

namespace vis
{

struct DisplayList;

// A billboard's font grows by the lesser XY stretch since its reference model was authored.
float billboardScale(const Mat4& reference, const Mat4& model, float fallback) noexcept;

bool valid(const Config& config) noexcept;
bool valid(const Object* object) noexcept;
const Color* sampleColors(const Cell* cell, uint32_t frame) noexcept;
uint32_t sampleFrames(const Cell* cell) noexcept;
float sampleDuration(const Object* object) noexcept;
Gradient mix(const Gradient& from, const Gradient& to, float progress) noexcept;
uint64_t fingerprint(const Object* object) noexcept;
bool localBounds(const Object* object, Bounds& bounds) noexcept;
bool localFamilyBounds(const Object* object, Bounds& bounds) noexcept;
// Points the object's stroked outline passes through, in its local space and in drawing
// order, tessellated like the renderer does. Writes at most capacity points and returns
// the full count; zero means the object has no outline (text, groups, pictures, ...).
// A warped object gives the outline its latest warp left (see WarpShape).
uint32_t outline(const Object* object, Vec3* points, uint32_t capacity, bool& closed) noexcept;
// The color of map at share, from 0 at its first stop to 1 at its last; a share past either end
// takes that end's color.
Color colormapColor(Colormap map, float share) noexcept;
// Where a local point falls in a surface colormap: 0 at its min and 1 at its max, unclamped.
float colormapShare(const SurfaceColormap& colormap, const Vec3& point) noexcept;

// The most numbers a space writes.
static constexpr uint32_t MAX_SPACE_NUMBERS = 256;
// The points a circle draws with, counterclockwise from +x, and each corner of a rounded
// rectangle (see circlePoint() and rectangleOutline()); a warp samples them with no fewer.
static constexpr uint32_t CIRCLE_POINTS = 64;
static constexpr uint32_t CORNER_POINTS = 8;

// Calls tick with each multiple of step inside range but zero, where a space draws a tick across
// that axis (see Space::ticks) or, with the step of the range, a grid line or a number; at most
// 4096 of them, counting zero, and false once tick returns false, which stops it. Each is a whole
// count of the step as a script writes it (see _decimal()), so no rounding piles up along the
// range and a step of 0.1 gives 0.9 where 9 * 0.1f gives 0.90000004.
template<typename Tick>
inline bool spaceTicks(const Range& range, float step, Tick tick)
{
    if (!(step > 0.0f)) return true;
    auto unit = _decimal(step);
    auto first = std::ceil(static_cast<double>(range.min) / unit - 1.0e-4);
    auto last = std::floor(static_cast<double>(range.max) / unit + 1.0e-4);
    if (!(first <= last)) return true;
    if (last - first > 4095.0) last = first + 4095.0;
    for (auto k = first; k <= last; k += 1.0) {
        if (k == 0.0) continue;
        if (!tick(static_cast<float>(k * unit))) return false;
    }
    return true;
}

// How far past the tick at value a space looks along its axis to tell which way the axis runs.
inline float spaceTickLead(float value, float step) noexcept
{
    return std::fmax(step * 0.01f, std::fabs(value) * 1.0e-5f);
}

// What a run of the points of a warp draws (see WarpShape).
enum struct WarpPart : uint8_t
{
    // The outline of a shape, one contour of a path, or the one point where a warp takes hold
    // of a point, a text or a picture, which it moves without bending.
    Outline,
    // A grid line of a space, and one it draws only in the 3D view.
    Grid,
    DepthGrid,
    // An axis of a space, in its own color above the grid; z shows only in the 3D view.
    AxisX,
    AxisY,
    AxisZ,
    // The one point where a space writes the number value on an axis.
    NumberX,
    NumberY,
    NumberZ,
    // A tick of a space: its point on an axis, then one a little along the axis, which give the
    // way the axis runs there; z shows only in the 3D view.
    TickX,
    TickY,
    TickZ,
    // The retained origin of a space with screen labels; it guides placement and draws nothing.
    Origin
};

// Metadata of a space that its geometry bounds leave out: pixel-sized numbers and ticks, and
// the retained origin used by screen labels.
inline bool spaceMark(WarpPart part) noexcept
{
    return part == WarpPart::Origin || part == WarpPart::NumberX || part == WarpPart::NumberY || part == WarpPart::NumberZ
           || part == WarpPart::TickX || part == WarpPart::TickY || part == WarpPart::TickZ;
}

// The axis, 0 for x to 2 for z, that an axis, a tick or a number of a space stands for.
inline uint8_t spaceAxis(WarpPart part) noexcept
{
    if (part == WarpPart::AxisY || part == WarpPart::TickY || part == WarpPart::NumberY) return 1u;
    if (part == WarpPart::AxisZ || part == WarpPart::TickZ || part == WarpPart::NumberZ) return 2u;
    return 0u;
}

// Calls line(part, from, to) with each line of a space in the order the renderer draws them, then
// number(part, point, value) with each point where the space writes a number, until a call
// returns false: the grid lines in the xy plane and, with depth, those in the xz and yz planes;
// the axes, z only with depth; the ticks across each axis, from the point of a tick to one a
// little along its axis, which give the way the axis runs there; and the numbers of each axis.
template<typename Line, typename Number>
inline void spaceParts(const Space* space, bool depth, Line line, Number number)
{
    const Range* ranges[] = {&space->x, &space->y, &space->z};
    const float steps[] = {space->ticks.x, space->ticks.y, space->ticks.z};
    const WarpPart axes[] = {WarpPart::AxisX, WarpPart::AxisY, WarpPart::AxisZ};
    const WarpPart ticks[] = {WarpPart::TickX, WarpPart::TickY, WarpPart::TickZ};
    const WarpPart numbers[] = {WarpPart::NumberX, WarpPart::NumberY, WarpPart::NumberZ};
    // The point at value along axis and at across along other, zero along the third axis.
    auto point = [](uint32_t axis, float value, uint32_t other, float across) {
        float at[3] = {0.0f, 0.0f, 0.0f};
        at[other] = across;
        at[axis] = value;
        return Vec3{at[0], at[1], at[2]};
    };
    auto on = [&point](uint32_t axis, float value) { return point(axis, value, axis, value); };
    // The grid lines at the values along axis, across other from its min to its max.
    auto grid = [&](WarpPart part, uint32_t axis, uint32_t other) {
        auto& across = *ranges[other];
        return spaceTicks(*ranges[axis], ranges[axis]->step, [&](float value) {
            return line(part, point(axis, value, other, across.min), point(axis, value, other, across.max));
        });
    };
    auto going = grid(WarpPart::Grid, 0u, 1u) && grid(WarpPart::Grid, 1u, 0u);
    if (going && depth) {
        going = grid(WarpPart::DepthGrid, 0u, 2u) && grid(WarpPart::DepthGrid, 2u, 0u)
                && grid(WarpPart::DepthGrid, 1u, 2u) && grid(WarpPart::DepthGrid, 2u, 1u);
    }
    auto count = depth ? 3u : 2u;
    for (auto axis = 0u; going && axis < count; axis++)
        going = line(axes[axis], on(axis, ranges[axis]->min), on(axis, ranges[axis]->max));
    for (auto axis = 0u; going && axis < count; axis++) {
        going = spaceTicks(*ranges[axis], steps[axis], [&](float value) {
            return line(ticks[axis], on(axis, value), on(axis, value + spaceTickLead(value, steps[axis])));
        });
    }
    for (auto axis = 0u; going && space->numbers && axis < count; axis++) {
        going = spaceTicks(*ranges[axis], ranges[axis]->step,
                           [&](float value) { return number(numbers[axis], on(axis, value), value); });
    }
}

struct WarpRun
{
    // One past the last point of the run.
    uint32_t end = 0;
    WarpPart part = WarpPart::Outline;
    float value = 0.0f;
};

// The geometry of a warp, in the local space of its object: point i travels straight from
// from[i] to to[i], and the runs split the points into the lines the object draws, in order.
// Outline runs are closed when closed is set. The scene keeps every warp until it is deleted.
struct WarpShape
{
    Vec3* from = nullptr;
    Vec3* to = nullptr;
    WarpRun* runs = nullptr;
    uint32_t count = 0;
    uint32_t runCount = 0;
    bool closed = false;

    ~WarpShape()
    {
        delete[] from;
        delete[] to;
        delete[] runs;
    }

    // Point i at progress, which may run past 0 and 1 as a curve overshoots.
    Vec3 at(uint32_t i, float progress) const noexcept
    {
        return from[i] + (to[i] - from[i]) * progress;
    }
};

inline bool spaceOrigin(const WarpShape* warp, float progress, Vec3& point) noexcept
{
    if (!warp || !warp->runCount || warp->runs[0].part != WarpPart::Origin) return false;
    point = warp->at(0u, progress);
    return true;
}

// The latest warp of object, whose images later warps, bounds and outlines start from, or null
// when no warp mapped it.
const WarpShape* warped(const Object* object) noexcept;
// The warp a copy shows from the moment it is attached, the one its source showed when it was
// cloned, which the start of its timeline holds; null for any other object.
const WarpShape* bornWarp(const Object* object) noexcept;
// Where a warp takes hold of an object that it moves without bending: a point's center, a
// text's anchor or a picture's center. False for any other object.
bool anchor(const Object* object, Vec3& point) noexcept;
// The local bounds of object as warp leaves it at progress: its own bounds moved with its
// anchor, or those of the points it draws, where a space leaves out its numbers. A null warp
// gives the bounds of the object's own geometry, as it shows before any warp.
bool localBounds(const Object* object, const WarpShape* warp, float progress, Bounds& bounds) noexcept;
// Whether AnimationTarget::geometry() gives object new points: a line, arrow, vector, curve,
// polygon, plot, route, arc, angle or path of one contour.
bool geometric(const Object* object) noexcept;
// How many points define the shape of object for AnimationTarget::geometry(): 2 for a line,
// arrow or vector, 4 for a curve, every point of a polygon, plot, route or path of one contour,
// 1 for an arc center, 3 for an angle vertex and ray endpoints, none for a plot that a
// geometry clip left empty, and 0 for objects whose geometry does not
// animate.
uint32_t shapeCount(const Object* object) noexcept;
// Writes the shapeCount(object) points of the object's own geometry, in the order
// AnimationTarget::geometry() takes them.
void shapeOf(const Object* object, Vec3* points) noexcept;
// Point index of count points along the cubic Bezier curve of controls (from, control1,
// control2, to), placed as Curve::gen() places them.
Vec3 curvePoint(const Vec3* controls, uint32_t index, uint32_t count) noexcept;

// Which group opacities world() multiplies into an object's opacity.
enum struct WorldOpacity : uint8_t
{
    // Every group above the object, for bounds and hit tests.
    All,
    // The groups below the object's nearest layer: the layer applies its own opacity and
    // that of the groups above it, so the object's drawings must not repeat them.
    Drawing,
    // For the layer of a composited object: its own opacity if it is a group (a shape keeps
    // its opacity in its drawings), times the groups above it up to the next layer.
    Layer,
    // For the layer of a mask target, which draws on its own: as Layer, but through every
    // layer, up to the nearest group the target shares with the object it masks.
    Mask
};

// Visual properties a clip writes. Clips on one object may overlap in time only when
// their channels are disjoint, so independent tracks can animate one object at once.
enum : uint32_t
{
    CHANNEL_MODEL = 1u << 0u,
    CHANNEL_OPACITY = 1u << 1u,
    CHANNEL_STROKE = 1u << 2u,
    CHANNEL_FILL = 1u << 3u,
    CHANNEL_DASH_OFFSET = 1u << 4u,
    CHANNEL_TAIL = 1u << 5u,
    CHANNEL_TIP = 1u << 6u,
    // Progress also carries the draw direction, the trace-fill flag and where the stretch a
    // windowed shape draws starts.
    CHANNEL_PROGRESS = 1u << 7u,
    CHANNEL_FILL_PROGRESS = 1u << 8u,
    CHANNEL_PIXEL_SCALE = 1u << 9u,
    CHANNEL_WIDTH = 1u << 10u,
    CHANNEL_RADIUS = 1u << 11u,
    CHANNEL_GRADIENT = 1u << 12u,
    // The geometry a morph or a warp moves.
    CHANNEL_MORPH = 1u << 13u,
    // A circle's radius, a rectangle's size or a text's size.
    CHANNEL_SIZE = 1u << 14u,
    CHANNEL_CORNER = 1u << 15u,
    CHANNEL_BLUR = 1u << 16u,
    CHANNEL_SHADOW = 1u << 17u,
    // The number a counting text shows.
    CHANNEL_VALUE = 1u << 18u,
    // The string a text shows.
    CHANNEL_TEXT = 1u << 19u,
    // The draw layer, which changes at a step.
    CHANNEL_LAYER = 1u << 20u,
    // The points that define a shape (see AnimationTarget::geometry).
    CHANNEL_GEOMETRY = 1u << 21u,
    CHANNEL_TRIM_DISTANCE = 1u << 22u,
    CHANNEL_ARC_START = 1u << 23u,
    CHANNEL_ARC_ANGLE = 1u << 24u,
    CHANNEL_COUNT = 25u,
    CHANNEL_ALL = 0xffffffffu
};

// The outlines of a morph, paired when it is played: point i of from, in the local space of
// the source, becomes point i of to, in the local space of the target.
struct MorphShape
{
    Vec3* from = nullptr;
    Vec3* to = nullptr;
    uint32_t count = 0;
    bool closed = false;
    // Two surfaces: the colors of the source and the target at the points between them, steps
    // + 1 samples of count colors each, as Animation::sourceColors and targetColors hold them.
    Color* colors[2] = {};
    uint32_t steps = 0;
    // Multi-piece morphs: cumulative ends, with closed contours before open pieces. A single
    // outline keeps no ends and uses closed, preserving its original sampling and drawing.
    uint32_t* ends = nullptr;
    uint32_t pieces = 0;
    uint32_t closedPieces = 0;
    // Unmatched pieces also grow or shrink their stroke width, avoiding a full-size cap at a point.
    int8_t* growth = nullptr;
};

// The points that define a shape (see AnimationTarget::geometry) as the scene keeps them for its
// geometry clips: count points, which a plot breaks into pieces as Plot::gen() takes them, piece
// i ending just before the point at ends[i]. Every other shape, and a plot of one piece, has one
// piece and no ends; a plot left empty has no points and no pieces. A function plot also
// retains its uncut samples, which matching grids blend vertically before clipping.
struct GeometryShape
{
    const Vec3* points = nullptr;
    const uint32_t* ends = nullptr;
    uint32_t count = 0;
    uint32_t pieces = 1;
    PlotSamples samples;
    ContourGrid contour;
};

struct ContourPlacement
{
    const Text* text = nullptr;
    Vec3 point;
    float angle = 0.0f;
    Vec2 half;
    uint32_t piece = 0;
    float distance = 0.0f;
};

bool sameGrid(const GeometryShape& from, const GeometryShape& to) noexcept;

struct VisualState
{
    Mat4 model;
    Color stroke;
    Color fill;
    Color gradientEnd;
    float width;
    float radius;
    float dashOffset;
    float markerTail = 0.0f;
    float markerTip = 0.0f;
    float opacity;
    float progress;
    // world() records an empty progress inherited from a parent that passes it to this object.
    bool inheritedProgressEmpty = false;
    // A windowed shape draws its outline from start to progress, as shares of its length.
    float start = 0.0f;
    float fillProgress = 1.0f;
    float pixelScale = 1.0f;
    Gradient gradient;
    bool traceFill = false;
    DrawDirection direction = DrawDirection::Forward;
    const Object* morph = nullptr;
    const MorphShape* morphShape = nullptr;
    float morphProgress = 0.0f;
    // The warp the object's drawing shows, at warpProgress from its points before to their
    // images, or null when it draws its own geometry.
    const WarpShape* warp = nullptr;
    float warpProgress = 0.0f;
    // Geometry that animates: a circle's radius in size.x, a rectangle's size and corner,
    // and a text's size in size.x. Other objects leave them zero.
    Vec2 size;
    float corner = 0.0f;
    Vec2 trimDistance;
    float arcStart = 0.0f;
    float arcAngle = 0.0f;
    // The layer effects of the object itself; groups pass them to no one.
    float blur = 0.0f;
    Shadow shadow;
    // The number a counting text shows; other objects leave it zero.
    float value = 0.0f;
    // The string a text shows: text, on its way from textFrom by textProgress in the way
    // textChange tells (see shown()), both strings the text owns. Counting texts and other
    // objects leave them null.
    const char* text = nullptr;
    const char* textFrom = nullptr;
    float textProgress = 1.0f;
    TextChange textChange = TextChange::Sweep;
    // The object's own draw layer. world() adds how far the groups above it moved theirs.
    int32_t layer = 0;
    // The points that define the shape (see AnimationTarget::geometry): geometry, on its way
    // from geometryFrom by geometryProgress. Matching function columns blend vertically;
    // other compatible shapes keep as many points in the same pieces. Null while the object
    // keeps the geometry it was added with.
    const GeometryShape* geometry = nullptr;
    const GeometryShape* geometryFrom = nullptr;
    float geometryProgress = 1.0f;
};

struct ArcGeometry
{
    Vec3 center;
    float radius = 0.0f;
    float start = 0.0f;
    float angle = 0.0f;
    bool square = false;
    bool sector = false;
};

ArcGeometry arcGeometry(const Object* object, const VisualState* state = nullptr) noexcept;
uint32_t arcOutline(const ArcGeometry& arc, Vec3* points, uint32_t capacity) noexcept;
// Point index of the CIRCLE_POINTS a circle of radius about center draws with.
Vec3 circlePoint(const Vec3& center, float radius, uint32_t index) noexcept;
// The outline a rectangle about center draws, half its size across and with corners of radius:
// its four corners, or CORNER_POINTS on each rounded one, counterclockwise from the right side.
// Writes at most 4 * CORNER_POINTS points and returns how many.
uint32_t rectangleOutline(const Vec3& center, const Vec2& half, float radius, Vec3* points) noexcept;
bool arcBounds(const ArcGeometry& arc, Bounds& bounds) noexcept;
Vec3 angleLabelPoint(const Angle* angle, const VisualState* state = nullptr) noexcept;

// Trim the sampled ends of a line, arrow or vector; false when nothing remains.
bool segment(const Object* object, const VisualState* state, Vec3* ends) noexcept;
// How many points the object draws from its sampled geometry: a trimmed segment, or every
// point of a polygon, plot, route, path or curve outline. An empty segment or plot draws none.
uint32_t drawnCount(const Object* object, const VisualState& state) noexcept;
// Writes the drawnCount() points object draws in state, in its own space.
void drawn(const Object* object, const VisualState& state, Vec3* points) noexcept;
// Writes the count points that define the shape in state, which reshapes it, on their way, in
// the order AnimationTarget::geometry() takes them.
void shapeAt(const VisualState& state, uint32_t count, Vec3* points) noexcept;
// Recut a vertical blend, or read its endpoint geometry. Optional buffers take its points
// and piece ends; with neither, only the counts are returned.
GeometryShape plotAt(const VisualState& state, Vec3* points = nullptr, uint32_t* ends = nullptr) noexcept;
void plotSamplesAt(const VisualState& state, Vec3* points) noexcept;

// The string text shows in state: the number a counting text writes, or the string a text
// change has reached. When the string has to be written, value points into written, which
// the caller deletes; otherwise written is null.
Result shown(const Text* text, const VisualState& state, const char*& value, char*& written) noexcept;

// The largest blur and shadow sigma, in screen pixels.
static constexpr float EFFECT_SIGMA_LIMIT = 256.0f;
bool valid(const Shadow& shadow) noexcept;

// Ends a list of clips: no further clip of the object.
static constexpr uint32_t NO_CLIP = UINT32_MAX;

struct ObjectEntry
{
    Object* object = nullptr;
    VisualState base;
    uint64_t fingerprint = 0;
    float born = 0.0f;
    float dead = -1.0f;
    bool revealScheduled = false;
    bool fillRevealScheduled = false;
    // A shape of an emphasis, which draws the stretch of its outline from start to progress
    // by length.
    bool windowed = false;
    // Latest clip end per channel; clips on a channel are authored in time order.
    float channelEnd[CHANNEL_COUNT] = {};
    // The indices of the first and last clip of the object: its clips form a list in storage
    // order, each naming the next in Clip::next, so sampling reads only the object's own clips.
    uint32_t firstClip = NO_CLIP;
    uint32_t lastClip = NO_CLIP;
    // The clips of that list, and the objects of the subtree the object heads, itself included:
    // each clip is read once for the object and once for every object below it.
    uint32_t clipCount = 0;
    uint32_t family = 1;
    // The scene's copy of the geometry the object has now, the one its latest geometry clip
    // ends at, or null while it keeps the geometry it was added with.
    const GeometryShape* geometry = nullptr;
};

// How a clip carries the model from its start to its end.
enum struct ClipMotion : uint8_t
{
    // Element-wise interpolation keeps point travel linear.
    Linear,
    // Circular-arc point travel about axis; matching a rotation angle keeps the shape rigid.
    Arc,
    // Turns by angle and scales by stretch about a parent-space pivot that travels by offset.
    Pivot,
    // The center follows a motion path, optionally turning with it.
    Path,
    // A shared world-space path detour added to each matched piece's straight travel.
    MatchPath
};

struct MotionPath;

// A shape that takes the stroke, fill or width of a group above it.
struct Heir
{
    Object* object = nullptr;
    ObjectEntry* entry = nullptr;
    // The animation target whose group passes the styles, when a play call collects heirs.
    uint32_t target = 0;
    // TARGET_STROKE, TARGET_FILL and TARGET_WIDTH bits the shape takes from the group.
    uint32_t properties = 0;
};

struct Clip
{
    Object* object = nullptr;
    VisualState from;
    VisualState to;
    float begin = 0.0f;
    float end = 0.0f;
    AnimCurve curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    AnimationKind kind = AnimationKind::Create;
    // Empty progress imposed by a reveal, including states hidden before its start.
    bool progressReveal = false;
    Object* related = nullptr;
    uint32_t channels = CHANNEL_ALL;
    ClipMotion motion = ClipMotion::Linear;
    bool orient = false;
    // Pivot: the parent-space pivot at the start. Path: the parent-space center of the object
    // at the start.
    Vec3 pivot;
    // Rotation axis, and the axis the travel arc turns about.
    Vec3 axis = {0.0f, 0.0f, 1.0f};
    Vec3 stretch = {1.0f, 1.0f, 1.0f};
    // MatchPath: axis, stretch and offset are the columns mapping world detours to parent space.
    Vec3 offset;
    float angle = 0.0f;
    float arc = 0.0f;
    // Wiggle: how many times the object tips to a side; it grows to stretch.x and turns by up
    // to angle about pivot.
    float swings = 0.0f;
    // Create of a windowed shape: the share of its outline the drawn stretch covers. The
    // stretch runs from before the start of the outline to past its end, so nothing stays.
    float window = 0.0f;
    // Path: travelled span as fractions of the path length, and the 1-based path index.
    float pathFrom = 0.0f;
    float pathTo = 1.0f;
    uint32_t path = 0;
    // Morph: the 1-based index of the paired outlines.
    uint32_t morph = 0;
    // Layered: the clip began while another clip moved the object, so its models hold only its
    // own change of the model, which applies on top of the clips before it. When the change
    // turns or scales about a point of the object, follows is set and anchor is that point in
    // the object's own space, so the pivot travels with the other motion.
    bool layered = false;
    bool follows = false;
    Vec3 anchor;
    // The index of the next clip of the same object in storage order, or NO_CLIP, which append()
    // sets.
    uint32_t next = NO_CLIP;
};

struct CameraClip
{
    Camera from;
    Camera to;
    CameraView fromView = CameraView::TwoD;
    CameraView toView = CameraView::TwoD;
    float begin = 0.0f;
    float end = 0.0f;
    AnimCurve curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    // An orbit turns the eye and up of from by orbit radians about axis through the target,
    // and to is from after the whole turn. Zero: interpolate from to to along the shortest turn.
    Vec3 axis;
    float orbit = 0.0f;
    // Layered camera clips add deltas to the accumulated camera instead of replacing its pose.
    // Orbits use the current target; look adds target motion/rotation and distance/height ratios.
    // FOV and clipping planes ease from the current state. Return to 2D waits for earlier moves
    // and sets the final pose exactly.
    bool layered = false;
};

struct ViewportEntry
{
    Scene* scene = nullptr;
    Viewport viewport;
};

// The camera timeline a look or orbit adds to: the scene's own, or that of a view. camera and
// view are the latest pose the moves leave, and initial and initialView the pose before the
// first move.
struct CameraTrack
{
    CameraClip*& clips;
    uint32_t& count;
    uint32_t& capacity;
    Camera& initial;
    CameraView& initialView;
    Camera& camera;
    CameraView& view;
};

// A fade of a whole view from one opacity to another.
struct ViewFade
{
    float begin = 0.0f;
    float end = 0.0f;
    float from = 1.0f;
    float to = 1.0f;
    AnimCurve curve = AnimCurve::preset(AnimCurvePreset::Smooth);
};

// A view of Scene::view(). settings is the view as made, its hidden list pointing at the copy
// the entry owns; camera, cameraView and opacity are the latest the moves and fades leave.
struct ViewEntry
{
    ~ViewEntry();
    SceneView settings;
    const Object** hidden = nullptr;
    CameraClip* cameras = nullptr;
    uint32_t cameraCnt = 0;
    uint32_t cameraCap = 0;
    Camera initialCamera;
    CameraView initialView = CameraView::TwoD;
    Camera camera;
    CameraView cameraView = CameraView::TwoD;
    ViewFade* fades = nullptr;
    uint32_t fadeCnt = 0;
    uint32_t fadeCap = 0;
    float opacity = 1.0f;
    float born = 0.0f;
};

// How a view shows at a moment. The scene lowers for it at the size of its own frame through
// camera, which looks where the view's camera looks; scale and offset then carry that picture
// into rect, the view's rectangle in frame pixels, and clip it there.
struct ViewFrame
{
    Camera camera;
    CameraView cameraView = CameraView::TwoD;
    float scale = 1.0f;
    Vec2 offset;
    float rect[4] = {};
    float opacity = 1.0f;
    int32_t layer = INT32_MAX;
    Color background = {0, 0, 0, 0};
    const Object* const* hidden = nullptr;
    uint32_t hiddenCount = 0;
    // False before the view is made and while it is faded out; the other fields then do not
    // count.
    bool shown = false;
};

struct SceneStage
{
    Scene* scene = nullptr;
    float start = 0.0f;
    float end = 0.0f;
};

// Where a styled transition shows its two scenes at a moment: each as a viewport of the parent
// scene, at an opacity from 0 to 1, the incoming one above unless outgoingAbove.
struct TransitionFrame
{
    Viewport outgoing;
    Viewport incoming;
    float outgoingOpacity = 1.0f;
    float incomingOpacity = 1.0f;
    bool outgoingAbove = false;
};

struct SceneTransition
{
    uint32_t* matches = nullptr;
    float begin = 0.0f;
    float end = 0.0f;
    AnimCurve curve = AnimCurve::preset(AnimCurvePreset::Smooth);
    TransitionKind kind = TransitionKind::Match;
    TransitionSide from = TransitionSide::Right;
    // The area of a zoom inside the viewport of the sequence, already of its shape and inside it.
    Viewport area;
    // A style other than Match that also matches, with matches.
    bool match = false;

    // The frame of a style other than Match at eased progress, inside viewport, the viewport of
    // the sequence; the alphas are those of the backgrounds of the outgoing and incoming scenes.
    void frame(const Viewport& viewport, float progress, uint8_t outgoingAlpha,
               uint8_t incomingAlpha, TransitionFrame& output) const noexcept;
};

struct SceneSequence
{
    ~SceneSequence();
    SceneStage* stages = nullptr;
    SceneTransition* transitions = nullptr;
    uint32_t count = 0;
    Viewport viewport;
    float begin = 0.0f;
    bool live = true;
    bool owned = false;

    float time(uint32_t index, float parentTime) const noexcept
    {
        auto& stage = stages[index];
        return live ? std::fmin(stage.scene->duration(), std::fmax(0.0f, parentTime - stage.start))
                    : stage.scene->duration();
    }
};

struct SceneMarker
{
    char* name = nullptr;
    float time = 0.0f;
};

// The source and target of a match or a cross-fade, which meet on the way while it plays.
struct SceneHandoff
{
    const Object* source = nullptr;
    const Object* target = nullptr;
    float begin = 0.0f;
    float end = 0.0f;
    // A cross-fade hands over whole families; a match records individual contacts.
    bool family = true;
    // A fading replacement slot is consumed; its original stays in a match from a copy.
    bool counterpart = true;
};

// The state of one object that Scene::save() recorded.
struct SavedObject
{
    Object* object = nullptr;
    // The object's own state at the cursor. A text's string is the one a change in progress
    // changes into, which the text keeps; a save before the timeline begins points it at text,
    // a copy, since the text may still replace its string then.
    VisualState state;
    char* text = nullptr;
    // The starting layer, against which restore() measures how far the starting layers of the
    // objects below moved since the save, so that they still draw in the order they did.
    int32_t baseLayer = 0;
    // The record of the parent, which comes earlier, or UINT32_MAX for the target.
    uint32_t parent = UINT32_MAX;
    // Whether a text counted, and which styles the object set itself for the shapes below a
    // group to take (see Object::objStrokeAuthored).
    bool counting = false;
    bool strokeAuthored = false;
    bool fillAuthored = false;
    bool widthAuthored = false;
};

// What one Scene::save() recorded for a target: its own state, then those of the objects below.
struct SavedState
{
    Object* target = nullptr;
    SavedObject* objects = nullptr;
    uint32_t count = 0;
};

struct Scene::Impl
{
    Config cfg;
    // Unique per Scene instance; renderers key retained paint trees on it so a new Scene
    // allocated at a recycled address never reuses stale paints.
    uint64_t serial = 0;
    Theme sceneTheme;
    ObjectEntry* objects = nullptr;
    Clip* clips = nullptr;
    CameraClip* cameras = nullptr;
    ViewportEntry* viewports = nullptr;
    ViewEntry** views = nullptr;
    SceneSequence* sequence = nullptr;
    SceneMarker* markers = nullptr;
    SceneHandoff* handoffs = nullptr;
    // The states save() recorded, oldest first; restore() takes the newest of a target.
    SavedState* saves = nullptr;
    MotionPath* paths = nullptr;
    MorphShape* morphs = nullptr;
    WarpShape** warps = nullptr;
    // The geometries that geometry clips travel between, each the shape of its object at the
    // end of a clip, or at its start.
    GeometryShape** geometries = nullptr;
    Scene* owner = nullptr;
    // The entries of the attached objects, in the order attach() stored them; an object's id is
    // one past the index of its entry, so entry() and Scene::object() find it at once.
    uint32_t objectCnt = 0;
    uint32_t objectCap = 0;
    uint32_t clipCnt = 0;
    uint32_t clipCap = 0;
    // The clips that placing every object once reads: the clip count of each entry times its
    // family. append() and attach() keep it, and Scene::sampleWork() adds the nested scenes.
    uint64_t work = 0;
    uint32_t cameraCnt = 0;
    uint32_t cameraCap = 0;
    uint32_t viewportCnt = 0;
    uint32_t viewportCap = 0;
    uint32_t viewCnt = 0;
    uint32_t viewCap = 0;
    uint32_t markerCnt = 0;
    uint32_t markerCap = 0;
    uint32_t handoffCnt = 0;
    uint32_t handoffCap = 0;
    uint32_t saveCnt = 0;
    uint32_t saveCap = 0;
    uint32_t pathCnt = 0;
    uint32_t pathCap = 0;
    uint32_t pathVertices = 0;
    uint32_t morphCnt = 0;
    uint32_t morphCap = 0;
    uint32_t morphPoints = 0;
    uint32_t warpCnt = 0;
    uint32_t warpCap = 0;
    uint32_t warpPoints = 0;
    uint32_t geometryCnt = 0;
    uint32_t geometryCap = 0;
    uint32_t geometryPoints = 0;
    uint32_t colorCursor = 0;
    // Authoring time for the next call. It may move backwards (seek); extent keeps the
    // latest time any call reached, so the duration never shrinks.
    float cursor = 0.0f;
    float extent = 0.0f;
    // Why the most recent timeline call failed, for script error messages.
    const char* diagnostic = nullptr;
    // Holds a diagnostic that names something, such as a missing font.
    char reason[256] = {};
    bool definitionsSealed = false;
    Camera initialCamera;
    CameraView initialView = CameraView::TwoD;
    RuntimeModifier runtimeModifiers[Scene::RuntimeLimit];
    uint32_t runtimeModifierCnt = 0;
    char* themeFonts[5] = {};
    // The names Scene::missingFonts() gave last, one after another, each ending in a NUL.
    char* missingFonts = nullptr;
    uint32_t missingFontSize = 0;
    uint32_t missingFontCap = 0;

    ~Impl();
    bool assign(const Theme& theme) noexcept;
    bool growClips(uint32_t count) noexcept;
    // Stores the next clip, which growClips() made room for, as a clip of object on channels from
    // begin to end, and adds it to the list of the object's clips, whose channels then end at end
    // or later. The clip starts empty unless prepare() has filled it. Every clip is appended
    // before anything samples its object.
    Clip& append(Object* object, uint32_t channels, float begin, float end, const AnimCurve& curve,
                 bool prepared = false) noexcept;
    // The camera timeline of the scene, or of view index - 1 for an index from 1.
    CameraTrack track(uint32_t index) noexcept;
    // Add a look or an orbit to track after the checks of Scene::look() and Scene::orbit() that
    // do not depend on whose camera it is.
    Result look(const CameraTrack& track, const Camera& camera, CameraView view, float duration,
                const AnimCurve& curve) noexcept;
    Result orbit(const CameraTrack& track, float angle, const Vec3& axis, float duration,
                 const AnimCurve& curve) noexcept;
    // How view index shows at time (see ViewFrame).
    Result frame(uint32_t index, float time, ViewFrame& frame) const noexcept;
    // Makes room for count more handoffs.
    bool growHandoffs(uint32_t count = 1u) noexcept;
    // Copies the outline of source into the parent space of follower, or reuses a copy.
    Result follow(const Object* source, const Object* follower, uint32_t& index, bool relative = false) noexcept;
    void dropPaths(uint32_t count, uint32_t vertices) noexcept;
    // Pairs the outlines of a morph from its target to its related object, or the points of two
    // surfaces with the colors sampled between them, as the index of the pair.
    Result pair(const Animation& animation, uint32_t& index) noexcept;
    void dropMorphs(uint32_t count, uint32_t points) noexcept;
    // A copy of shape for a geometry clip, or nullptr when memory or the point budget runs out;
    // a copy of the same shape as reuse is reuse itself.
    const GeometryShape* reshape(const GeometryShape& shape, const GeometryShape* reuse = nullptr) noexcept;
    void dropGeometries(uint32_t count, uint32_t points) noexcept;
    // Copies initial and changed geometry into shaped after previousCount/previousPoints.
    // Repeated geometry shares its prior copy; failure drops new copies but retains initial ones.
    // Plots reserve point/piece storage through room().
    Result reshape(const AnimationTarget* targets, uint32_t count, const GeometryShape** shaped,
                   uint32_t& previousCount, uint32_t& previousPoints) noexcept;
    // The shape target gives its object, whose shape is current: the target's points, in the
    // pieces it names or else in those of current.
    static GeometryShape shaping(const AnimationTarget& target, const GeometryShape& current) noexcept;
    // Makes entry's object keep a copy of its geometry as the one it starts with, before its
    // first geometry clip changes its own; false when memory runs out.
    bool reshapable(ObjectEntry& entry) noexcept;
    // The shape object has now: its shapeCount() points, which shapeOf() writes into points, and
    // the pieces of a plot, which the shape reads from the plot while it stays as it is.
    static GeometryShape own(const Object* object, Vec3* points) noexcept;
    // Writes the points of shape into the fields that define object's shape, and the pieces of a
    // plot, for which room() has made room.
    static void writeShape(Object* object, const GeometryShape& shape) noexcept;
    // Makes object, when it is a plot, hold room for the points and pieces of shape, so that
    // writeShape() gives them to it without allocating; false when memory runs out. The room
    // changes nothing the plot shows. Other shapes keep their point counts.
    static bool room(Object* object, const GeometryShape& shape) noexcept;
    Result prepare(const Animation& animation, Clip& clip, float begin, float end) noexcept;
    // Adds the clip of a checked animation from begin to end, with the motion prepare() gave its
    // slot, and leaves its object in the state it ends in. A reveal hides the object in its
    // clips before previous, the count of clips before the call.
    void emit(const Animation& animation, float begin, float end, const AnimCurve& curve, uint32_t previous) noexcept;
    void advance(float time) noexcept;
    Result fail(Result result, const char* reason) noexcept;
    ObjectEntry* entry(const Object* object) const noexcept;
    // Takes back the copy a clone attached last, whose objects hold the entries from first on,
    // with what attach() added for them to the families and the work; the next object attached
    // takes the id after them again.
    void discard(Object* copy, uint32_t first) noexcept;
    // A new object of the kind of source, with its geometry, composition and flags and the
    // values of state: a text shows the string of state, and a connector joins from and to.
    // nullptr when memory runs out.
    static Object* replica(const Object* source, const VisualState& state, Object* from,
                           Object* to) noexcept;
    // Appends style heirs after count, up to capacity: descendants without their own or nearer
    // group stroke/fill/width. Fill needs area, or unauthored text inside a typeset block.
    // Returns the updated count.
    uint32_t heirs(const Object* group, uint32_t properties, Heir* out, uint32_t capacity,
                   uint32_t count = 0) const noexcept;
    uint32_t heirs(const Object* group, uint32_t properties, bool texts, Heir* out,
                   uint32_t capacity, uint32_t count) const noexcept;
    // Why target cannot give its object the values it names, or nullptr. Checks the values
    // against the object, not against the timeline.
    static const char* unanimatable(const AnimationTarget& target) noexcept;
    // The state of target's object once it takes the values target names, starting from state;
    // a text change takes text, the copy of its string that keep() gave, and a geometry change
    // geometry, the copy of its points that reshape() gave, when there are.
    static VisualState targeted(const AnimationTarget& target, const VisualState& state,
                                const char* text = nullptr, const GeometryShape* geometry = nullptr) noexcept;
    // The copy of value that text keeps for a text change, or nullptr when memory runs out: its
    // own string or a recent copy that reads the same, or else a new copy, which sets added.
    // The first copy also keeps the string the text had, which earlier states point at.
    // drop() lets go of the copy added last.
    static const char* keep(Text* text, const char* value, bool& added) noexcept;
    static void drop(Text* text) noexcept;
    // Marks the stroke, fill and width that properties name as object's own, which the shapes
    // added below it later then inherit, and object as a layer when they name an effect.
    static void authored(Object* object, uint32_t properties) noexcept;
    // Fails a clip of target from begin that would begin before its object is added or inside an
    // earlier clip of the object, but for a change of the transform that may add to another.
    Result fits(const ObjectEntry& entry, const AnimationTarget& target, float begin) noexcept;
    // Adds the clips of count targets that passed the checks of play(): one for each target, from
    // its begin to its end in the group lagged from the cursor, and one for each shape that takes
    // a stroke, fill or width that a target animates on a group. The cursor stays where it is.
    Result animate(const AnimationTarget* targets, uint32_t count, float duration, float lag,
                   const AnimCurve& curve) noexcept;
    bool sample(const ObjectEntry& entry, float time, VisualState& state) const noexcept;
    bool plotEndpoint(const Object* object, float time, const VisualState& state) const noexcept;
    Result contourPlacements(const Plot* plot, float time, ContourPlacement* output,
                             uint32_t capacity, uint32_t& count) const noexcept;
    // Writes state into the object's fields, as its latest authored state; a counting text
    // rewrites what it shows, and a text takes the string a text change ends at.
    static void store(Object* object, const VisualState& state) noexcept;
    bool motion(const Clip& clip, float progress, float time, Mat4& model) const noexcept;
    Result sample(float time, Camera& camera, CameraView& view) const noexcept;
    // The authored model chain used as a billboard's font-size reference.
    Mat4 billboardReference(const Object* object) const noexcept;
    // With WorldOpacity::Mask, group opacities stop at shared, the nearest common ancestor of
    // the target and the object it masks (nullptr: none). Known, one flag for each object, keeps
    // which objects a build has found valid and as authored, so it checks each once.
    Result world(const Object* object, float time, VisualState& state, bool& visible,
                 WorldOpacity opacity = WorldOpacity::All, const Object* shared = nullptr,
                 bool* known = nullptr) const noexcept;
    Result bounds(const Object* object, float time, Bounds& bounds, bool& defined, bool hidden = false) const noexcept;
    // The layer object draws at at time, as world() gives it: its own, moved as far as the objects
    // above it moved theirs. An object not shown then keeps the layer it was authored with.
    int32_t drawLayer(const Object* object, float time) const noexcept;
    // Where connector runs in the world at time: between the closed outlines of its
    // ends (bounds when there is no outline), padding in from each. Defined is false when it draws nothing then.
    Result ends(const Connector* connector, float time, Vec3& from, Vec3& to, bool& defined) const noexcept;
    // Whether the object draws itself and its descendants into a layer of their own: it is
    // isolated, masked, the target of a mask, or blends with what lies beneath.
    static bool composited(const Object* object) noexcept;
    // Finds or adds the layer that the drawings of object go into: the layer of its nearest
    // composited ancestor, the object included, or 0 when it has none. The layer of a masked
    // object comes with the layer of its mask target.
    Result layer(const Object* object, float time, DisplayList& list, uint32_t& index) const noexcept;
    // With frame, the list of a view: through its camera, without the objects above its layer
    // or among those it hides.
    Result build(float time, DisplayList& list, bool sort = true,
                 const ViewFrame* frame = nullptr) const noexcept;
    // The three parts of a transition that also matches, all in the space of the outgoing
    // scene from: each scene at its local time without the objects that pair, on its own background,
    // then the pairs at progress on none.
    static Result matched(const Scene* from, const Scene* to, const SceneTransition& transition,
                          float progress, float fromTime, float toTime,
                          DisplayList* parts, Config* spaces) noexcept;
};

}  // namespace vis

#endif
