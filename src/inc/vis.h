// viscript's public C++ API. A Scene owns a tree of objects and a timeline: every play,
// transition, camera move and wait is recorded from the cursor, and a Renderer draws the scene
// as it stands at any time into a Surface. A Saver writes frames to PNG, GIF or MP4, and Lua
// loads a scene from a script. The header reads in this order:
//   results and enumerations    Result, Type, and the modes of masks, blends, cells, text, cameras
//   values                      Vec2, Vec3, Mat4 (row-major), Color, Range, SampleTime
//   samplers                    the callbacks that cells, plots, contours, fields and warps call
//   assets, cameras and styles  Asset, AssetLoader, Camera, Gradient, Style, Theme
//   layout reports              Bounds, BBox and the Layout* records a Renderer measures
//   objects                     Object, Group, Space, shapes, Plot, Text, Picture, Cell, Connector
//   animations                  Animation, AnimationTarget, MatchOptions, Emphasis, CloneOptions
//   scenes                      Config, Viewport, SceneView, transitions, RuntimeModifier, Scene
//   rendering and saving        Surface, Renderer, Saver, Lua
//
// Words the API uses:
//   object    a node of the tree, with a style, a model matrix, a layer, an opacity and a progress.
//             A group draws nothing itself; a space is a group with axes that places by its ranges.
//   tag       an object's name, unique among the objects attached to a scene; object(tag) finds it.
//   cursor    where the next play, look, wait or add starts in time; wait() and seek() move it.
//   clip      one animated change of one object's property over a span of time; a play adds one per
//             target, and clipCount() counts them.
//   channel   the property a clip animates: model, opacity, stroke, fill, width, geometry and more.
//             On one object, channels other than the model cannot overlap in time.
//   family    an object with everything below it, as plays, fades and handoffs treat it.
//   piece     one shape or text below an object that a match transform carries or fades on its own.
//   handoff   the scene's record that two families overlap on purpose while pieces travel between
//             them, which handoff() reports.
//   layer     the draw order of an object; a subtree that isolate(), a mask, a blend, a blur or a
//             shadow composites as one is also called a layer.
//   viewport  a rectangle of the frame showing another scene, mounted by viewport(); a transition
//             shows a sequence of scenes there in turn.
//   view      a second look at this scene's own objects through a camera of its own (SceneView).
//   warp      a mapping of points applied to subtrees, which bends their geometry from then on.
//   save      a pushed copy of a subtree's state, which restore() animates back to.
//   runtime   callbacks a host registers to adjust objects and the camera each sample, and to take
//             input; the UI, input, states and Lua runtime modules work through them.
//   marker    a named cursor time, set by mark().
//   theme     the fonts, sizes and colors objects take when they set none of their own.
#ifndef _VIS_H_
#define _VIS_H_

#include <cstdint>

namespace vis
{

struct Scene;
struct Space;
struct Object;
struct Text;
struct WarpShape;

enum struct Result : uint8_t
{
    Success = 0,
    InvalidArguments = 1,
    InsufficientCondition = 2,
    NonSupport = 3,
    OutOfMemory = 4,
    IoError = 5,
    ScriptError = 6,
    Unknown = 7
};

enum struct Type : uint8_t
{
    Space,
    Point,
    Line,
    Arrow,
    Vector,
    Circle,
    Polygon,
    Plot,
    Text,
    Picture,
    Cell,
    Group,
    Rectangle,
    Connector,
    Path,
    Curve,
    SurfaceMesh,
    Route,
    Arc,
    Angle
};

enum struct LayoutSceneKind : uint8_t
{
    Root = 0,
    Viewport,
    Transition,
    // A view of Scene::view(): the scene seen again through the view's camera.
    View
};

enum struct LayoutVisualKind : uint8_t
{
    Stable = 0,
    Morph,
    FadeOut,
    FadeIn
};

enum struct PathVerb : uint8_t
{
    Move = 0,
    Line,
    Quadratic,
    Cubic,
    Close
};

// Region operation applied by Path::combine().
enum struct PathOperation : uint8_t
{
    Union = 0,
    Intersection,
    Difference,
    Exclusion
};

enum struct ImageFilter : uint8_t
{
    Bilinear = 0,
    Nearest
};

// Where a mask lets the object it masks show.
enum struct MaskMode : uint8_t
{
    // Where the mask is opaque, in proportion to its alpha.
    Alpha = 0,
    // Where the mask is transparent.
    InverseAlpha,
    // Where the mask is bright, in proportion to its luminance.
    Luma,
    // Where the mask is dark.
    InverseLuma
};

// How a layer combines with what lies beneath it, as in CSS mix-blend-mode. S is the layer
// and D what lies beneath; Normal draws S over D.
enum struct BlendMode : uint8_t
{
    Normal = 0,
    // S * D: darkens; white leaves D unchanged.
    Multiply,
    // S + D - S * D: lightens; black leaves D unchanged.
    Screen,
    // Multiply where D is dark, Screen where it is light.
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    // Overlay with the roles of S and D swapped.
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
    // S + D, clamped.
    Add
};

enum struct CellMode : uint8_t
{
    Full = 0,
    Padd
};

enum struct SurfaceMode : uint8_t
{
    Solid = 0,
    Mesh,
    SolidMesh
};

// Continuous colormap interpolated from nine evenly spaced samples.
enum struct Colormap : uint8_t
{
    None = 0,
    Viridis,
    Plasma,
    Inferno,
    Magma,
    Cividis
};

// Where in its step a rectangle of a Riemann sum (see Group::riemann()) reads the function.
enum struct RiemannSample : uint8_t
{
    Left = 0,
    Right,
    Midpoint
};

// Which way a stream line (see Group::streamLines()) runs from its seed: along the field,
// against it, or both ways.
enum struct StreamDirection : uint8_t
{
    Forward = 0,
    Backward,
    Both
};

enum struct TextOrientation : uint8_t
{
    Billboard = 0,
    Plane
};

enum struct TextRole : uint8_t
{
    H1 = 0,
    H2,
    H3,
    Text,
    Code
};

enum struct ThemePreset : uint8_t
{
    ProWhite = 0,
    ProBlack,
    AdaptiveVscode
};

enum struct ThemeColorRole : uint8_t
{
    Background = 0,
    Foreground,
    Muted,
    Accent,
    Secondary,
    Success,
    Warning,
    Danger,
    Info,
    Surface,
    Border,
    Result,
    Focus
};

enum struct NumberMode : uint8_t
{
    Fixed = 0,
    Relative
};

enum struct AnimCurvePreset : uint8_t
{
    Linear = 0,
    Smooth,
    EaseIn,
    EaseOut,
    EaseInOut,
    Gentle,
    Snappy,
    Back,
    Bounce,
    Elastic,
    Custom
};

enum struct Projection : uint8_t
{
    Orthographic = 0,
    Perspective
};

enum struct CameraMode : uint8_t
{
    Fixed = 0,
    Interactive
};

enum struct CameraView : uint8_t
{
    TwoD = 0,
    ThreeD
};

enum struct CameraAction : uint8_t
{
    Pan = 0,
    Orbit,
    Zoom,
    Reset,
    View2D,
    View3D
};

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(const Vec2& v) const noexcept;
    Vec2 operator-(const Vec2& v) const noexcept;
    Vec2 operator*(float s) const noexcept;
    Vec2 operator/(float s) const noexcept;
    float dot(const Vec2& v) const noexcept;
    float length() const noexcept;
};

struct AnimCurve
{
    AnimCurvePreset kind = AnimCurvePreset::Smooth;
    Vec2 control1 = {0.25f, 0.1f};
    Vec2 control2 = {0.25f, 1.0f};
    float strength = 1.0f;
    bool reverse = false;

    // Strength is in [0, 2]; cubic control x in [0, 1], y in [-2, 2].
    // Endpoints are exact. Transform overshoot is retained; bounded paint/camera values clamp.
    static AnimCurve preset(AnimCurvePreset preset, float strength = 1.0f) noexcept;
    static AnimCurve cubicBezier(const Vec2& control1, const Vec2& control2,
                                 float strength = 1.0f) noexcept;
    bool valid() const noexcept;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3 operator+(const Vec3& v) const noexcept;
    Vec3 operator-(const Vec3& v) const noexcept;
    Vec3 operator*(float s) const noexcept;
    Vec3 operator/(float s) const noexcept;
    float dot(const Vec3& v) const noexcept;
    Vec3 cross(const Vec3& v) const noexcept;
    float length() const noexcept;
    Vec3 normalized() const noexcept;
};

// A 4x4 matrix in row-major order, e[row * 4 + column], applied to column vectors.
struct Mat4
{
    float e[16];

    static Mat4 identity() noexcept;
    static Mat4 translate(const Vec3& v) noexcept;
    static Mat4 scale(const Vec3& v) noexcept;
    static Mat4 rotateX(float radians) noexcept;
    static Mat4 rotateY(float radians) noexcept;
    static Mat4 rotateZ(float radians) noexcept;
    Mat4 operator*(const Mat4& m) const noexcept;
    Vec3 operator*(const Vec3& v) const noexcept;
};

struct Color
{
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;

    // Reads #rgb, #rgba, #rrggbb or #rrggbbaa, with or without the #; anything else gives fallback.
    static Color hex(const char* value, Color fallback = {255, 255, 255, 255}) noexcept;
};

using CellSampler = bool (*)(float x, float y, float time,
                             Color& color, void* data) noexcept;
using VoxelSampler = bool (*)(float x, float y, float z, float time,
                              Color& color, void* data) noexcept;
// Writes the mapped point to image; false means the point has no image.
using PointMap = bool (*)(const Vec3& point, Vec3& image, void* data) noexcept;
// A scalar field sampled while a contour is made, never while it draws. False means no value.
using ContourSampler = bool (*)(double x, double y, double& value, void* data) noexcept;
// Writes the function value at x; false aborts construction.
using FunctionSampler = bool (*)(float x, float& value, void* data) noexcept;
// Writes the planar field at (x, y); false aborts construction.
// Streamline tracing retains double precision.
using FieldSampler = bool (*)(double x, double y, double& u, double& v, void* data) noexcept;

struct SampleTime
{
    float duration = 0.0f;
    uint32_t fps = 30;
};

struct Range
{
    float min = -5.0f;
    float max = 5.0f;
    float step = 1.0f;

    // Counts intervals from min to max, shortening the last to end at max.
    // A 0.0001-step tolerance prevents a rounding sliver. Returns zero for non-finite,
    // empty or reversed ranges, nonpositive step, or a count above UINT32_MAX.
    uint32_t steps() const noexcept;
    // Returns min + index * step, clamped to max; interval i ends at stepAt(i + 1).
    float stepAt(uint32_t index) const noexcept;
    // Counts samples from min through max, allowing a 0.0001-step endpoint tolerance.
    // Returns zero for non-finite or reversed ranges, nonpositive step,
    // or a count above UINT32_MAX.
    uint32_t samples() const noexcept;
    // Sample index: min plus index steps.
    float sampleAt(uint32_t index) const noexcept;
};

// How Group::vectorField() draws the arrows of a field.
struct VectorFieldOptions
{
    // Arrow length is length * tanh(magnitude / length), in world units.
    // Zero uses 0.8 times the smaller grid step.
    float length = 0.0f;
    // When positive, every arrow is its vector scaled by this instead, and length must be zero.
    float scale = 0.0f;
    // Centers each arrow on its sample instead of starting it there.
    bool centered = false;
    // The size of each arrow's tip in pixels (see Vector::tip).
    float tip = 10.0f;
    // Optional 2..8-color magnitude ramp; no colors preserves the default stroke.
    // Unless low < high, the ramp spans zero to the largest magnitude.
    const Color* colors = nullptr;
    uint32_t colorCount = 0;
    float low = 0.0f;
    float high = 0.0f;
};

// How Group::streamLines() traces and draws its lines.
struct StreamLineOptions
{
    // Integrates the field for time seconds in dt-sized Runge-Kutta steps.
    float time = 3.0f;
    float dt = 0.05f;
    StreamDirection direction = StreamDirection::Forward;
    // The colors the mean speeds along the lines map onto, as VectorFieldOptions::colors.
    const Color* colors = nullptr;
    uint32_t colorCount = 0;
    float low = 0.0f;
    float high = 0.0f;

    // Returns ceil(time / dt), at least one, per direction. Returns zero for nonpositive
    // or non-finite inputs, or a count above UINT32_MAX.
    uint32_t steps() const noexcept;
};

struct CellRegion
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct PathCommand
{
    PathVerb verb = PathVerb::Move;
    Vec3 to;
    Vec3 control1;
    Vec3 control2;

    static PathCommand move(const Vec3& to) noexcept;
    static PathCommand line(const Vec3& to) noexcept;
    static PathCommand quadratic(const Vec3& control, const Vec3& to) noexcept;
    static PathCommand cubic(const Vec3& control1, const Vec3& control2,
                             const Vec3& to) noexcept;
    static PathCommand close() noexcept;
};

struct Asset
{
    ~Asset();
    Asset(const Asset&) = delete;
    Asset& operator=(const Asset&) = delete;

    uint32_t width() const noexcept;
    uint32_t height() const noexcept;
    // RGBA pixels; encoded assets are rasterized at intrinsic size on first call.
    const uint32_t* data() const noexcept;
    // Original encoded bytes (null for assets created from raw pixels).
    const uint8_t* encoded() const noexcept;
    uint32_t encodedSize() const noexcept;
    const char* mime() const noexcept;

private:
    mutable uint32_t* pixels = nullptr;
    uint8_t* bytes = nullptr;
    char* type = nullptr;
    uint32_t byteSize = 0;
    uint32_t pixelWidth = 0;
    uint32_t pixelHeight = 0;

    Asset(uint32_t* pixels, uint8_t* bytes, uint32_t size, char* mime,
          uint32_t width, uint32_t height) noexcept;
    friend struct AssetLoader;
};

struct AssetLoader
{
    // Encoded assets require a CPU decoder; raw pixels work with either renderer.
    // Path loading uses the host filesystem and needs the host's asset access policy.
    static Result load(const char* path, Asset*& output) noexcept;
    static Result load(const void* data, uint32_t size, const char* mime, Asset*& output) noexcept;
    static Result load(const uint32_t* pixels, uint32_t width, uint32_t height, Asset*& output) noexcept;
};

struct CameraInput
{
    CameraAction action = CameraAction::Pan;
    Vec2 delta;
};

struct Camera
{
    Vec3 eye = {0.0f, 0.0f, 10.0f};
    Vec3 target;
    Vec3 up = {0.0f, 1.0f, 0.0f};
    Projection projection = Projection::Orthographic;
    float fov = 0.7853981634f;
    float orthoHeight = 8.0f;
    float near = 0.05f;
    float far = 1000.0f;

    // Finite camera values require 0 < near < far and a nondegenerate view basis.
    // FOV is radians; a 2D scene uses an orthographic front view.
    bool project(const Vec3& world, float aspect, Vec3& ndc) const noexcept;
    bool segment(Vec3& from, Vec3& to) const noexcept;
};

enum struct GradientType : uint8_t
{
    None = 0,
    Linear,
    Radial,
    Conic
};

struct GradientStop
{
    float offset = 0.0f;
    Color color;
};

// Stroke and fill gradient. With no explicit stops, paint color blends to gradientEnd
// with the paint alpha; explicit stop alpha is multiplied by paint alpha and opacity.
// Geometry uses object-local coordinates. Defaults use the projected shape: linear
// uses open endpoints or the bounds diagonal; radial uses the circle or bounds center
// and radius or half-diagonal; conic uses the bounds center at angle zero.
// Conic angle and stop order run clockwise from local +x, in degrees.
struct Gradient
{
    static constexpr uint32_t StopLimit = 8;

    GradientType type = GradientType::None;
    GradientStop stops[StopLimit] = {};
    Vec3 from;
    Vec3 to;
    Vec3 center;
    Vec3 focal;
    float radius = 0.0f;
    float focalRadius = 0.0f;
    float angle = 0.0f;
    uint8_t stopCount = 0;
    bool line = false;         // from/to are explicit (Linear)
    bool centered = false;     // center is explicit (Radial, Conic)
    bool sized = false;        // radius is explicit (Radial)
    bool focused = false;      // focal center is explicit (Radial)

    bool enabled() const noexcept { return type != GradientType::None; }
    // Explicit gradients need 2..8 sorted finite stops in [0, 1]. Linear endpoints are
    // paired; radial radius is positive and focalRadius nonnegative. Geometry is finite.
    bool valid() const noexcept;
    bool operator==(const Gradient& other) const noexcept;
    bool operator!=(const Gradient& other) const noexcept { return !(*this == other); }
};

struct Style
{
    static constexpr uint32_t DashLimit = 4;

    Color stroke = {235, 238, 245, 255};
    Color fill = {0, 0, 0, 0};
    Color gradientEnd = {247, 37, 133, 255};
    float width = 2.0f;
    float radius = 5.0f;
    float dash[DashLimit] = {};
    float dashOffset = 0.0f;
    uint8_t dashCount = 0;
    Gradient gradient;
};

struct TextTheme
{
    const char* font = "Pretendard";
    float size = 17.0f;
    Color color = {219, 231, 243, 255};
};

struct AxisTheme
{
    Color x = {239, 83, 80, 255};
    Color y = {102, 187, 106, 255};
    Color z = {66, 165, 245, 255};
    Color grid = {55, 65, 81, 180};
    Color label = {174, 183, 195, 255};
};

struct SemanticTheme
{
    Color foreground = {32, 33, 36, 255};
    Color muted = {106, 112, 121, 255};
    Color accent = {0, 95, 184, 255};
    Color secondary = {103, 80, 164, 255};
    Color success = {46, 125, 50, 255};
    Color warning = {138, 93, 0, 255};
    Color danger = {179, 38, 30, 255};
    Color info = {0, 99, 155, 255};
    Color surface = {245, 246, 248, 255};
    Color border = {216, 218, 221, 255};
    Color result = {138, 101, 0, 255};
    Color focus = {49, 95, 140, 255};
};

// Theme fonts are nonempty; role sizes and object width are finite and positive, and
// roundness is 0 to 0.5. Use 1..10 palette colors. Scene::theme() copies font names.
struct Theme
{
    static constexpr uint32_t ColorLimit = 10;

    Color background = {255, 255, 255, 255};
    TextTheme h1 = {"Pretendard", 36.0f, {32, 33, 36, 255}};
    TextTheme h2 = {"Pretendard", 27.0f, {32, 33, 36, 255}};
    TextTheme h3 = {"Pretendard", 21.0f, {60, 64, 67, 255}};
    TextTheme text = {"Pretendard", 16.0f, {85, 91, 100, 255}};
    TextTheme code = {"JetBrainsMono", 14.0f, {155, 54, 0, 255}};
    Color objects[ColorLimit] = {
        {180, 95, 6, 255},
        {182, 58, 60, 255},
        {46, 127, 122, 255},
        {63, 125, 53, 255},
        {129, 86, 129, 255},
        {185, 86, 104, 255},
        {118, 80, 61, 255},
        {98, 102, 106, 255},
    };
    uint32_t objectCount = 8;
    float objectWidth = 1.25f;
    // Corner radius of Diagram boxes that follow the Theme, as a share of the shorter side of
    // the Diagram's node size (zones round 1.5 times as much); 0 keeps them square.
    float roundness = 0.0f;
    bool gradient = false;
    Color endGradientStop = {102, 102, 102, 255};
    AxisTheme axis = {
        {216, 218, 221, 255},
        {216, 218, 221, 255},
        {216, 218, 221, 255},
        {216, 218, 221, 255},
        {85, 91, 100, 255},
    };
    SemanticTheme colors;

    static Theme preset(ThemePreset preset) noexcept;
    Color color(ThemeColorRole role) const noexcept;
};

struct Bounds
{
    Vec3 min;
    Vec3 max;

    Vec3 center() const noexcept;
    Vec3 size() const noexcept;
};

struct BBox
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    Vec2 center() const noexcept;
    Vec2 size() const noexcept;
    bool intersects(const BBox& other, float padding = 0.0f) const noexcept;
};

struct LayoutScene
{
    const Scene* scene = nullptr;
    const char* path = nullptr;
    BBox bounds;
    BBox clipBounds;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    uint32_t parent = 0xffffffffu;
    uint32_t depth = 0;
    LayoutSceneKind kind = LayoutSceneKind::Root;
    bool clipped = false;
    bool stretched = false;
    // The time of its own timeline the scene shows, which differs from the report's for a
    // stage of a transition.
    float time = 0.0f;
};

struct LayoutObject
{
    const Object* object = nullptr;
    BBox familyBounds;
    BBox paintBounds;
    BBox visibleFamilyBounds;
    BBox visibleBounds;
    BBox clipBounds;
    uint32_t scene = 0;
    uint32_t parent = 0xffffffffu;
    int32_t layer = 0;
    bool familyVisible = false;
    bool visible = false;
    bool familyClipped = false;
    bool clipped = false;
};

// visible records sampled drawing. occluded means full opaque mounted-scene coverage;
// partial coverage remains conservative.
struct LayoutVisual
{
    uint32_t object = 0xffffffffu;
    uint32_t counterpart = 0xffffffffu;
    uint32_t occluder = 0xffffffffu;
    BBox paintBounds;
    BBox visibleBounds;
    BBox clipBounds;
    int32_t layer = 0;
    float opacity = 1.0f;
    LayoutVisualKind kind = LayoutVisualKind::Stable;
    bool visible = false;
    bool clipped = false;
    bool occluded = false;
};

struct LayoutPath
{
    // Owned by LayoutReport and valid until the report is destroyed or reused.
    // Points and paintBounds use root output pixels after camera and Viewport mapping.
    const Vec2* points = nullptr;
    // Encloses fill plus the renderer's round-join and butt/round-cap stroke footprint.
    BBox paintBounds;
    uint32_t visual = 0xffffffffu;
    uint32_t count = 0;
    // Maximum-axis width; conservative when a Viewport stretches one axis.
    float strokeWidth = 0.0f;
    bool closed = false;
    bool filled = false;
    bool stroked = false;
};

struct LayoutCollision
{
    uint32_t first = 0;
    uint32_t second = 0;
    BBox overlap;
    Vec2 clearance;
    Vec2 separation;
    bool overlapping = false;
};

struct LayoutContainment
{
    uint32_t container = 0;
    uint32_t content = 0;
    uint32_t depth = 0;
    float inset = 0.0f;
    float overflowLeft = 0.0f;
    float overflowTop = 0.0f;
    float overflowRight = 0.0f;
    float overflowBottom = 0.0f;
    bool contained = false;
};

// Sampled root-pixel geometry; returned data lasts until report reuse or destruction.
// Text uses measured ink/line boxes and stretched strokes use the larger scale axis.
// Collision evidence describes this frame, not continuous/raster/3D intersection proof.
struct LayoutReport
{
    LayoutReport() noexcept;
    ~LayoutReport();
    LayoutReport(const LayoutReport&) = delete;
    LayoutReport& operator=(const LayoutReport&) = delete;

    float time() const noexcept;
    float padding() const noexcept;
    uint32_t sceneCount() const noexcept;
    const LayoutScene* sceneAt(uint32_t index) const noexcept;
    uint32_t objectCount() const noexcept;
    const LayoutObject* objectAt(uint32_t index) const noexcept;
    uint32_t visualCount() const noexcept;
    const LayoutVisual* visualAt(uint32_t index) const noexcept;
    uint32_t pathCount() const noexcept;
    const LayoutPath* pathAt(uint32_t index) const noexcept;
    uint32_t collisionCount() const noexcept;
    const LayoutCollision* collisionAt(uint32_t index) const noexcept;
    uint32_t containmentCount() const noexcept;
    const LayoutContainment* containmentAt(uint32_t index) const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;
    friend struct Renderer;
};

// A soft shadow that a layer casts beneath itself.
struct Shadow
{
    // Alpha 0 casts no shadow.
    Color color = {0, 0, 0, 0};
    // Screen pixels, +x right and +y up like world coordinates.
    Vec2 offset;
    // Gaussian blur sigma in screen pixels; 0 casts a hard shadow.
    float blur = 0.0f;
};

struct Object
{
    virtual ~Object();
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    Style style;
    Mat4 model = Mat4::identity();
    // Draw order: a higher layer draws over a lower one, and within a layer nearer drawings draw
    // over farther ones and then later objects over earlier ones. A group's own layer orders
    // only an isolated group (see isolate()); AnimationTarget::layer() changes layers over time.
    int32_t layer = 0;
    float opacity = 1.0f;
    // Fraction revealed in [0, 1]; segments reveal their already-trimmed length.
    float progress = 1.0f;
    // Finite Gaussian sigma in [0, 256] screen pixels, also for shadow.blur.
    // Any authored blur/shadow composites the subtree as one effect layer.
    float blur = 0.0f;
    Shadow shadow;

    Type type() const noexcept;
    uint32_t id() const noexcept;
    Object* parent() const noexcept;
    Space* space() const noexcept;
    // Success transfers the subtree; failure leaves ownership unchanged.
    // A child has one parent, cannot be shared/reparented, and must not form a cycle.
    // Attached subtree tags must be unique within the scene; nesting is at most 64 levels.
    Result add(Object* object) noexcept;
    Result label(Text* text) noexcept;
    uint32_t childCount() const noexcept;
    Object* childAt(uint32_t index) const noexcept;
    bool bounds(Bounds& output) const noexcept;
    // Direct layout edits require an unsealed definition at cursor zero.
    // nextTo/alignTo also require the same direct parent as target.
    Result moveTo(const Vec3& point) noexcept;
    Result nextTo(const Object* target, const Vec3& direction, float gap = 0.25f) noexcept;
    Result alignTo(const Object* target, const Vec3& direction) noexcept;
    Object& tag(const char* value) noexcept;
    const char* tag() const noexcept;
    // The chaining setters below style a detached object. Once it is attached to a scene they
    // return without changing it: edit its fields and call Scene::update(object) at cursor zero,
    // or play an AnimationTarget.
    Object& stroke(Color color) noexcept;
    Object& stroke(Color color, float width) noexcept;
    Object& strokeWidth(float width) noexcept;
    Object& gradient(bool enabled = true) noexcept;
    Object& gradient(Color end) noexcept;
    // Validates and applies a full gradient description; GradientType::None disables it.
    Result gradient(const Gradient& value) noexcept;
    Result dash(const float* pattern, uint32_t count, float offset = 0.0f) noexcept;
    Object& fill(Color color) noexcept;
    Object& shift(const Vec3& by) noexcept;
    // Composites this subtree as one layer, so group opacity fades overlapping children once.
    // The layer sorts by this object's layer, mean drawing depth and first emission order;
    // child layers sort inside it. Composition may change until the scene is mounted.
    Result isolate(bool enabled = true) noexcept;
    bool isolated() const noexcept;
    // Uses target and its descendants as this subtree's mask, instead of drawing them.
    // Shared ancestors fade the result once; target is empty before birth and after removal.
    // Target must share the scene, mask only this object, and exclude this object,
    // its ancestors and any masked descendants. nullptr clears the mask.
    // Masks may change until the scene is mounted.
    Result mask(Object* target, MaskMode mode = MaskMode::Alpha) noexcept;
    // The object's mask target, or nullptr.
    Object* mask() const noexcept;
    MaskMode maskMode() const noexcept;
    // Blends this subtree as one layer into its enclosing layer.
    // Normal preserves ordinary composition. May change until the scene is mounted.
    Result blend(BlendMode mode) noexcept;
    BlendMode blend() const noexcept;

private:
    explicit Object(Type type) noexcept;
    bool owned() const noexcept;
    Result layout(const Mat4& model) noexcept;
    // Records why a layout call failed on the owning scene and returns result.
    Result fail(Result result, const char* reason) const noexcept;

    Type objType;
    uint32_t objId = 0;
    char* objTag = nullptr;
    Scene* objOwner = nullptr;
    Object* objParent = nullptr;
    Object** children = nullptr;
    uint32_t childCnt = 0;
    uint32_t childCap = 0;
    bool objColorAuthored = false;
    // Set by stroke() and fill(), by an animation of the color, or when the object is attached
    // with a color that is not its default. A group passes the colors it sets to descendants
    // that set none.
    bool objStrokeAuthored = false;
    bool objFillAuthored = false;
    bool objWidthAuthored = false;
    bool objGradientAuthored = false;
    bool objGradientEndAuthored = false;
    bool objThemeGradient = false;
    bool objThemeColor = false;
    bool objIsolated = false;
    MaskMode objMaskMode = MaskMode::Alpha;
    BlendMode objBlend = BlendMode::Normal;
    // Set once an animation gives the object a blur or a shadow.
    bool objEffects = false;
    // The latest warp of the object's geometry (see Scene::warp()), which then draws, bounds
    // and outlines it as the warp left it.
    const WarpShape* objWarp = nullptr;
    // The warp a copy shows from the moment it is attached: the one its source showed when
    // Scene::clone() copied it. Its timeline starts from it, and its own warps follow it.
    const WarpShape* objBornWarp = nullptr;
    // The target this object shows through, and the object a target masks.
    Object* objMask = nullptr;
    Object* objMasked = nullptr;
    // Set by Group::gen(): a group of no other module's kind, which Scene::clone() can copy.
    bool objPlain = false;
    // Set on the copies Scene::clone() attaches, which keep the font, size, colors and flags of
    // what they copy instead of taking those of the theme and of the groups above them again.
    bool objCopied = false;
    // The space that placed this object as a label (see Space::place()) while it lay below the
    // space or was to be added below it. While it lies below that space, the space passes its
    // opacity and creation progress down to it as a group does.
    const Space* objLabelOf = nullptr;
    friend struct Group;
    friend struct Space;
    friend struct Point;
    friend struct Line;
    friend struct Arrow;
    friend struct Vector;
    friend struct Circle;
    friend struct Arc;
    friend struct Angle;
    friend struct Polygon;
    friend struct Plot;
    friend struct DirectedRoute;
    friend struct Text;
    friend struct Picture;
    friend struct Cell;
    friend struct Rectangle;
    friend struct Connector;
    friend struct Path;
    friend struct Curve;
    friend struct SurfaceMesh;
    friend struct Scene;
    friend const WarpShape* warped(const Object* object) noexcept;
    friend const WarpShape* bornWarp(const Object* object) noexcept;
};

// Draws no geometry. Shapes inherit unauthored stroke, fill and width from the nearest
// group; text inherits fill only within a typeset block. Group color animations propagate.
struct Group : Object
{
    // Treats descendant texts as one layout block. Text gaps are checked against other texts;
    // unauthored text color inherits the nearest fill within the block.
    bool typeset = false;

    static Group* gen() noexcept;
    Result arrange(const Vec3& direction = {1.0f, 0.0f, 0.0f}, float gap = 0.25f) noexcept;
    Result arrangeGrid(uint32_t columns, float columnGap = 0.25f, float rowGap = 0.25f) noexcept;
    // Builds a detached group of default-style rectangles, ordered left to right,
    // from y = 0 to each height; zero height preserves a flat rectangle.
    // InvalidArguments: count differs from x.steps(), or a height is non-finite.
    // InsufficientCondition: no intervals or more than 4096.
    static Result riemann(const Range& x, const float* heights, uint32_t count,
                          Group*& output) noexcept;
    // Samples fn once per interval at the selected endpoint or midpoint, left to right.
    // InvalidArguments also covers null fn, false return or non-finite output.
    static Result riemann(const Range& x, FunctionSampler fn, void* data,
                          RiemannSample sample, Group*& output) noexcept;
    // Builds a detached arrow group from the field, sampled bottom-left first in row order.
    // Drops zero vectors and arrows shorter than 0.001 of the configured or largest length.
    // Optional indices receives column + row * columns for each arrow; allocate one per sample.
    // InvalidArguments: invalid ranges/options, null fn, false return or non-float-safe vector.
    // InsufficientCondition: more than 4096 samples/axis or 16384 samples total.
    static Result vectorField(const Range& x, const Range& y, FieldSampler fn, void* data,
                              Group*& output, const VectorFieldOptions& options = {},
                              uint32_t* indices = nullptr) noexcept;
    // Uses count row-major vectors, bottom-left first.
    // InvalidArguments also covers a count mismatch or non-finite vector.
    static Result vectorField(const Range& x, const Range& y, const Vec2* vectors, uint32_t count,
                              Group*& output, const VectorFieldOptions& options = {},
                              uint32_t* indices = nullptr) noexcept;
    // Builds a detached group of bounded streamline plots, ordered by row-major seeds.
    // Lines stop at zero/non-finite field or before leaving the grid box. Drops unmoving seeds
    // and lines shorter than 0.001 of the longest. Optional indices receives each seed index;
    // allocate one entry per grid sample. InvalidArguments: invalid ranges/options or failed fn.
    // InsufficientCondition: over 4096 samples/axis, 16384 seeds, 4096 steps/direction,
    // or 1048576 retained points.
    static Result streamLines(const Range& x, const Range& y, FieldSampler fn, void* data,
                              Group*& output, const StreamLineOptions& options = {},
                              uint32_t* indices = nullptr) noexcept;
    // Uses up to 16384 explicit seeds, optionally bounded by region in x and y.
    // Optional indices receives each retained line's seed index.
    static Result streamLines(const Vec2* seeds, uint32_t count, const Bounds* region,
                              FieldSampler fn, void* data, Group*& output,
                              const StreamLineOptions& options = {},
                              uint32_t* indices = nullptr) noexcept;

protected:
    Group() noexcept;
};

struct Space final : Object
{
    Range x;
    Range y;
    Range z;
    Color axisX = {239, 83, 80, 255};
    Color axisY = {102, 187, 106, 255};
    Color axisZ = {66, 165, 245, 255};
    // Axis and tick stroke width in pixels; -1, the default, uses the grid width or 1.5,
    // whichever is larger. Zero hides their strokes; other values must be non-negative.
    float axisWidth = -1.0f;
    Color numberColor = {174, 183, 195, 255};
    NumberMode numberMode = NumberMode::Fixed;
    float numberSize = 15.0f;
    bool numbers = false;
    // Ticks at multiples of each component, excluding the origin; zero disables that axis.
    // Each uses its axis color and width, with constant tickLength pixels across zoom.
    // The z ticks appear in 3D; warps move tick anchors.
    Vec3 ticks;
    float tickLength = 8.0f;

    ~Space() override;
    static Space* gen(const Range& x = {}, const Range& y = {}, const Range& z = {}) noexcept;
    Result add(Object* object) noexcept;
    uint32_t count() const noexcept;
    Object* objectAt(uint32_t index) const noexcept;
    Vec3 c2w(const Vec3& coordinate) const noexcept;
    bool w2c(const Vec3& world, Vec3& coordinate) const noexcept;
    // Places label beside an axis tick (0 = x, 1 = y, 2 = z), with a screen-pixel gap
    // beyond half tickLength. Zero direction chooses below x or left of y/z.
    // Text without a box aligns its glyph edge; other objects use bounds. An unattached
    // parentless label is prepared as a space child. Attached labels must share this scene,
    // exclude the space and its ancestors, and be placed before timeline sealing.
    // Child labels inherit space opacity and draw progress; other labels keep their parent.
    // InvalidArguments: invalid label/axis/value/gap or flattened direction.
    // InsufficientCondition: detached space, missing bounds or sealed layout.
    Result place(Object* label, uint8_t axis, float value, const Vec3& direction = {}, float gap = 4.0f) noexcept;
    // Anchors a direct-child billboard Text at value on axis with tickLength / 2 + gap
    // screen pixels outward from the projected origin at every camera sample.
    // A detached parentless Text may be added here. Author before sealing and the first
    // space warp; later warps retain the origin. Degenerate or clipped origin uses screen up.
    // Placement preserves depth and does not avoid neighboring drawings or canvas edges.
    Result placeScreen(Text* label, uint8_t axis, float value, float gap = 4.0f) noexcept;
    // Samples synchronously; callbacks must not mutate the scene or recursively sample/add.
    // Failure attaches no partial group. Success returns a group owned by this space.
    // Padding is in [0, 0.5). Limits: 4096 samples/axis, 16384 XY/slice, 65536 coordinates,
    // 4096 time frames and 262144 retained coordinate/color samples. Applies to voxel too.
    Result cell(CellSampler callback, Group*& cells, void* data = nullptr,
                CellMode mode = CellMode::Padd, float padding = 0.05f,
                const SampleTime& time = {}) noexcept;
    Result voxel(VoxelSampler callback, Group*& voxels, void* data = nullptr,
                 CellMode mode = CellMode::Padd, float padding = 0.05f,
                 const SampleTime& time = {}) noexcept;

private:
    bool objSampling = false;
    Space(const Range& x, const Range& y, const Range& z) noexcept;
    Result sample(VoxelSampler callback, Group*& output, void* data, CellMode mode,
                  float padding, const SampleTime& time, bool spatial) noexcept;
    friend struct Scene;
};

struct Point final : Object
{
    Vec3 point;

    static Point* gen(const Vec3& point) noexcept;

private:
    explicit Point(const Vec3& point) noexcept;
};

struct Line final : Object
{
    Vec3 from;
    Vec3 to;

    // Finite nonnegative start/end distances in object-local units, before transforms.
    // A combined distance at least the segment length leaves no ink, bounds or outline.
    Vec2 trimDistance;

    static Line* gen(const Vec3& from, const Vec3& to) noexcept;

private:
    Line(const Vec3& from, const Vec3& to) noexcept;
};

struct Arrow final : Object
{
    Vec3 from;
    Vec3 to;
    float tail = 0.0f;
    float tip = 16.0f;

    // Finite nonnegative start/end distances in object-local units, before transforms.
    // A combined distance at least the segment length also removes the arrow markers.
    Vec2 trimDistance;

    static Arrow* gen(const Vec3& from, const Vec3& to) noexcept;

private:
    Arrow(const Vec3& from, const Vec3& to) noexcept;
};

struct Vector final : Object
{
    Vec3 value;
    Vec3 origin;
    float tail = 0.0f;
    float tip = 16.0f;

    // Finite nonnegative start/end distances in object-local units, before transforms.
    // A combined distance at least the segment length also removes the arrow markers.
    Vec2 trimDistance;

    static Vector* gen(const Vec3& value, const Vec3& origin = {}) noexcept;

private:
    Vector(const Vec3& value, const Vec3& origin) noexcept;
};

struct Circle final : Object
{
    Vec3 center;
    float radius = 1.0f;

    static Circle* gen(const Vec3& center, float radius) noexcept;

private:
    Circle(const Vec3& center, float radius) noexcept;
};

// Angles are radians counterclockwise from +x in the local xy plane.
struct Arc final : Object
{
    Vec3 center;
    float radius = 1.0f;
    float start = 0.0f;
    float angle = 1.57079632679f;
    bool sector = false;

    static Arc* gen(const Vec3& center, float radius, float start, float angle) noexcept;

private:
    Arc(const Vec3& center, float radius, float start, float angle) noexcept;
};

enum struct RightAngle : uint8_t
{
    Auto = 0,
    Never,
    Always
};

// The smaller angle between two points seen from vertex; other selects its complement.
// Children are labels: their origin follows the bisector at labelDistance at every sample.
struct Angle final : Object
{
    Vec3 vertex;
    Vec3 from;
    Vec3 to;
    float radius = 1.0f;
    bool other = false;
    RightAngle right = RightAngle::Auto;
    float labelDistance = 1.7f;

    static Angle* gen(const Vec3& vertex, const Vec3& from, const Vec3& to, float radius = 1.0f) noexcept;
    Vec3 labelPoint() const noexcept;

private:
    Angle(const Vec3& vertex, const Vec3& from, const Vec3& to, float radius) noexcept;
};

struct Polygon final : Object
{
    ~Polygon() override;
    static Polygon* gen(const Vec3* points, uint32_t count) noexcept;
    const Vec3* points() const noexcept;
    uint32_t count() const noexcept;

private:
    Vec3* objPoints = nullptr;
    uint32_t objCount = 0;
    Polygon(const Vec3* points, uint32_t count) noexcept;
    friend struct Scene;
};

// Uncut function samples in increasing x order, with a non-finite x marking a discontinuity
// and a non-finite y an undefined value. The optional band clips their vertical blend.
struct PlotSamples
{
    const Vec3* points = nullptr;
    uint32_t count = 0;
    bool band = false;
    Vec2 yRange;
    bool valid() const noexcept;
};

struct ContourGrid
{
    Range x;
    Range y;
    double level = 0.0;
};

struct ContourLabelOptions
{
    // One fixed-point conversion, %.0f through %.6f. Copied by labelContours().
    const char* format = "%.1f";
    // Plane text size and distances in local contour units. Zero spacing labels each
    // connected curve once; positive spacing repeats labels along longer curves.
    float size = 0.12f;
    float spacing = 0.0f;
    float padding = 0.04f;
    bool upright = true;
};

struct Plot final : Object
{
    // Ends the plot, and each of its pieces, flat at its first and last point, where every other
    // stroke ends round, so that an end cut at a boundary lies on it rather than past it by half
    // the stroke width.
    bool flat = false;
    // Optional solid marker at the active draw endpoint, in pixels, in the sampled stroke
    // property color. The plot gradient does not recolor it.
    // Zero disables it. A positive radius shows the first vertex at manually set progress 0;
    // scheduled create and uncreate still hide it at their empty boundaries.
    float endpointRadius = 0.0f;

    ~Plot() override;
    static Plot* gen(const Vec3* points, uint32_t count) noexcept;
    // Builds copied, contiguous pieces with increasing ends and ends[pieces - 1] == count;
    // each piece needs at least two points. Gaps add no line or draw-progress distance.
    // One piece permits null ends. Multiple pieces cannot provide one outline for moveAlong
    // or passingFlash. Optional uncut samples support vertical timeline blending.
    static Plot* gen(const Vec3* points, uint32_t count, const uint32_t* ends,
                     uint32_t pieces, const PlotSamples* samples = nullptr) noexcept;
    // Retained function samples, copied by gen(). Matching x grids blend y during play and
    // are cut again at each sample; the supplied geometry remains exact at the endpoints.
    PlotSamples samples() const noexcept;
    // Traces a row-major scalar grid (x varies fastest) with marching squares.
    // Each axis needs at least two samples; at most 65536 finite, float-safe values.
    // Saddles use the bilinear determinant; zero connects bottom to right. Shared edges
    // use 40 bisections against sampler or linear values. Loops repeat their first point.
    // Out-of-range levels return an empty plot. Copies output, retains no callback,
    // and assigns output only on success. InvalidArguments: invalid grid/value or failed fn.
    // NonSupport: grid or vertex count exceeds its limit, including closing points.
    static Result contour(const Range& xRange, const Range& yRange, const double* values,
                          uint32_t count, double level, Plot*& output,
                          ContourSampler sampler = nullptr, void* data = nullptr,
                          uint32_t pointLimit = 1000000) noexcept;
    const Vec3* points() const noexcept;
    uint32_t count() const noexcept;
    // The pieces follow one another in points(): piece i ends just before the point at
    // contourEnd(i), and the last one ends at count(). A geometry animation can change them
    // (see AnimationTarget::geometry()), and leave a plot with no points and no pieces.
    uint32_t contours() const noexcept;
    uint32_t contourEnd(uint32_t index) const noexcept;
    // Labels a detached contour with tangent-aligned plane Text children, <tag>/label/N.
    // Set tag and stroke first. Labels avoid grid edges and earlier labels; strokes leave
    // gaps beneath them, and curves too short for text are skipped. At most 4096 slots/level.
    // InvalidArguments: invalid format/sizes. InsufficientCondition: attached, already
    // labelled or nonempty child tree. NonSupport: too many label slots.
    Result labelContours(const ContourLabelOptions& options = {}) noexcept;
    const ContourLabelOptions* contourLabels() const noexcept;
    ContourGrid contourGrid() const noexcept;

private:
    ContourGrid objGrid;
    ContourLabelOptions objLabels;
    char objLabelFormat[5] = {};
    bool objLabelled = false;
    uint32_t objLabelCount = 0;
    uint32_t labelSlots(const Vec3* points, uint32_t count, const uint32_t* ends, uint32_t pieces) const noexcept;
    Result reserveLabels(const Vec3* points, uint32_t count, const uint32_t* ends, uint32_t pieces) noexcept;
    Vec3* objPoints = nullptr;
    uint32_t* objEnds = nullptr;
    uint32_t objCount = 0;
    uint32_t objContours = 1;
    // How many points and ends the arrays above have room for, which a geometry animation that
    // changes the pieces makes before it writes them.
    uint32_t objRoom = 0;
    uint32_t objEndRoom = 0;
    PlotSamples objSamples;
    Vec3* objSamplePoints = nullptr;
    uint32_t objSampleRoom = 0;
    Plot(const Vec3* points, uint32_t count) noexcept;
    friend struct Scene;
};

struct DirectedRoute final : Object
{
    float tail = 0.0f;
    float tip = 16.0f;

    ~DirectedRoute() override;
    static DirectedRoute* gen(const Vec3* points, uint32_t count) noexcept;
    const Vec3* points() const noexcept;
    uint32_t count() const noexcept;

private:
    Vec3* objPoints = nullptr;
    uint32_t objCount = 0;
    DirectedRoute(const Vec3* points, uint32_t count) noexcept;
    friend struct Scene;
};

struct Path final : Object
{
    ~Path() override;
    // Each move starts a contour and a close ends the contour before it; the contours of one
    // path are all closed or all open. A path of several contours fills by the even-odd rule,
    // so a contour inside another cuts a hole in it, whichever way it runs.
    static Path* gen(const PathCommand* commands, uint32_t count,
                     uint32_t samples = 24) noexcept;
    const Vec3* points() const noexcept;
    uint32_t count() const noexcept;
    bool closed() const noexcept;
    // The contours follow one another in points(): contour i ends just before the point at
    // contourEnd(i), and the last one ends at count().
    uint32_t contours() const noexcept;
    uint32_t contourEnd(uint32_t index) const noexcept;
    // Applies operation left to right to circles, rectangles, polygons and closed paths,
    // including parent transforms. Multiple contours use even-odd coverage. Returns a
    // static closed path in parent xy space (world if null), outer contours CCW and holes CW.
    // InvalidArguments: fewer than two shapes, open/unsupported/invalid shape.
    // InsufficientCondition: empty result. NonSupport: over 8192 combined edges or crossings.
    static Result combine(const Object* const* shapes, uint32_t count, PathOperation operation,
                          const Object* parent, Path*& output) noexcept;

private:
    Vec3* objPoints = nullptr;
    uint32_t* objEnds = nullptr;
    uint32_t objCount = 0;
    uint32_t objContours = 1;
    bool objClosed = false;
    Path(Vec3* points, uint32_t count, bool closed, uint32_t* ends, uint32_t contours) noexcept;
    friend struct Scene;
};

struct Curve final : Object
{
    ~Curve() override;
    static Curve* gen(const Vec3& from, const Vec3& control1, const Vec3& control2,
                      const Vec3& to, uint32_t samples = 32) noexcept;
    const Vec3* points() const noexcept;
    uint32_t count() const noexcept;
    // The four points the curve runs through and bends toward: from, control1, control2 and
    // to, as gen() took them or as a geometry animation left them.
    const Vec3* controls() const noexcept;

private:
    Vec3* objPoints = nullptr;
    uint32_t objCount = 0;
    Vec3 objControls[4];
    Curve(Vec3* points, uint32_t count) noexcept;
    friend struct Scene;
};

// Maps dot(local point, axis) from min..max to colormap colors, clamping endpoints.
// Reversing min and max reverses the ramp.
struct SurfaceColormap
{
    Colormap map = Colormap::None;
    Vec3 axis = {0.0f, 0.0f, 1.0f};
    float min = -1.0f;
    float max = 1.0f;
};

// World-space surface light: camera-facing normals receive ambient plus diffuse cosine.
// direction points toward the light; zero keeps the camera headlight. Ambient is in [0, 1].
struct SurfaceLight
{
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    float ambient = 0.5f;
};

// Row-major rectangular point grid with flat face shading; not a general triangle mesh.
// No depth-buffer material interpolation is performed.
struct SurfaceMesh final : Object
{
    SurfaceMode mode = SurfaceMode::Solid;
    // Shades the faces, by the camera's headlight unless light sets a direction; false leaves
    // them flat.
    bool shading = true;
    SurfaceLight light;
    // Colors the faces by position instead of the fill when its map is set, at the value of the
    // middle of each face, recomputed whenever the points move. It wins over colors().
    SurfaceColormap colormap;
    // Mesh/SolidMesh line counts independent of the point grid: zero or 2..axis point count.
    // Lines interpolate evenly across the grid and split at faces for depth ordering.
    // Both zero outlines every face; Mesh draws only lines.
    uint32_t meshColumns = 0;
    uint32_t meshRows = 0;

    ~SurfaceMesh() override;
    static SurfaceMesh* gen(const Vec3* points, uint32_t columns,
                            uint32_t rows) noexcept;
    const Vec3* points() const noexcept;
    uint32_t columns() const noexcept;
    uint32_t rows() const noexcept;
    // Copies columns * rows colors in point order; each face averages its four corners.
    // Object opacity multiplies their alpha. nullptr restores fill.
    // InvalidArguments: count mismatch.
    Result colors(const Color* colors, uint32_t count) noexcept;
    const Color* colors() const noexcept;

private:
    Vec3* objPoints = nullptr;
    Color* objColors = nullptr;
    uint32_t objColumns = 0;
    uint32_t objRows = 0;
    SurfaceMesh(const Vec3* points, uint32_t columns, uint32_t rows) noexcept;
};

// The room a text takes once set in its font, in pixels for billboard text and in world units
// for plane text, the units in which its em spans 4/3 of its size.
struct TextExtent
{
    // Advance width of the widest line, as the renderer places the glyphs.
    float width = 0.0f;
    // Height of the box that align pins to the text's point: every line's advance but the
    // last one's line gap.
    float height = 0.0f;
    // From the top of the box down to the first baseline.
    float ascent = 0.0f;
    // From the last baseline down to the bottom of the box.
    float descent = 0.0f;
    // Ink bounds from the first left-aligned baseline: x runs right, y runs up.
    // Includes slant; bottom may be negative. Empty ink gives zero bounds.
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    uint32_t lines = 0;
    // Counts missing glyphs and records the first as UTF-8. Excludes controls and invisible
    // formatting characters such as joiners and variation selectors.
    uint32_t missing = 0;
    char firstMissing[5] = {};
    // The text's font is not loaded, so it was measured in the default font, which draws it.
    bool fallback = false;
};

// How a counting text writes its value (see Text::count).
struct NumberFormat
{
    // Digits after the decimal point, at most 6; the value rounds to the nearest.
    uint8_t decimals = 0;
    // Writes a plus sign before a positive value. A value that rounds to zero has no sign.
    bool sign = false;
    // Groups the digits before the point in threes with this printable ASCII character, such
    // as ',' or ' '. Zero groups nothing.
    char separator = 0;
};

struct Text final : Object
{
    Vec3 point;
    // Font size in points at 96 dpi: em = size * 4/3, in billboard pixels or plane world units.
    // Zero takes the theme role size on attachment; positive sizes remain authored.
    float size = 0.0f;
    Vec2 align = {0.5f, 0.5f};
    TextOrientation orientation = TextOrientation::Billboard;
    TextRole role = TextRole::Text;
    // The room the text takes in layout, in world units, pinned to point by align as the
    // glyphs are: bounds() and the layout calls see this box. Zero, the default, leaves only
    // the point; measure() tells how large the glyphs are.
    Vec2 box;
    // Offsets only the layout box in text-plane world units (+x right, +y up).
    // Glyph alignment stays fixed; zero retains the line box.
    Vec2 boxOffset;
    // Horizontal shear per unit height above the first baseline, in [-1, 1].
    // Line-box metrics exclude slant; ink extents include it.
    float slant = 0.0f;
    // Centers ASCII digits, minus and space in cells of the font's widest digit advance.
    // Other characters keep their advances; false, the default, preserves proportional text.
    bool tabular = false;

    ~Text() override;
    static Text* gen(const char* text, const Vec3& point = {}) noexcept;
    const char* text() const noexcept;
    // Direct text edits require unsealed authoring; failed scene refresh restores the string.
    Result text(const char* value) noexcept;
    const char* font() const noexcept;
    Result font(const char* value) noexcept;
    // Measures value as rendered lines in the text's font and size, without legacy font kerning;
    // text() gives the text's own string. Fonts registered by Renderer::font are process-wide; a
    // renderer that registered the font must remain alive. A font that is not loaded is measured,
    // and drawn, as the default font the engine registers, "default", and output.fallback says so.
    // InsufficientCondition: no font loaded at all.
    // InvalidArguments: null value, unset font or size.
    Result measure(const char* value, TextExtent& output) const noexcept;
    // Returns retained text-change endpoint strings, including the initial string; writes
    // at most capacity pointers. Unchanged text returns its sole string.
    uint32_t texts(const char** output, uint32_t capacity) const noexcept;
    // Formats a counting value between prefix and suffix; AnimationTarget::value animates it.
    // text() restores plain text. InvalidArguments: non-finite value, over 6 decimals
    // or a non-printable ASCII separator.
    Result count(float value, const NumberFormat& format = {}, const char* prefix = nullptr,
                 const char* suffix = nullptr) noexcept;
    bool counting() const noexcept;
    // The number a counting text shows once its animations so far have run; zero otherwise.
    float value() const noexcept;
    NumberFormat format() const noexcept;
    // Writes what the counting text shows at value into output, with a terminating zero, and
    // returns its length; zero when the text does not count, value is not finite or output
    // has too little room.
    uint32_t write(float value, char* output, uint32_t capacity) const noexcept;

private:
    char* objText = nullptr;
    char* objFont = nullptr;
    // A non-negative gap retains outward screen placement by Space::placeScreen().
    float objScreenGap = -1.0f;
    // A counting text writes into objText, which has room for any value.
    char* objPrefix = nullptr;
    char* objSuffix = nullptr;
    uint32_t objCapacity = 0;
    float objValue = 0.0f;
    NumberFormat objFormat;
    bool objCounting = false;
    // The strings of its text changes once it has one; objText then points at one of them.
    char** objTexts = nullptr;
    uint32_t objTextCnt = 0;
    Text(const char* text, const Vec3& point) noexcept;
    friend struct Space;
    friend struct Scene;
};

struct Picture final : Object
{
    Vec3 center;
    float width = 1.0f;
    ImageFilter filter = ImageFilter::Bilinear;

    ~Picture() override;
    static Picture* gen(const Asset* asset, const Vec3& center = {}, float width = 1.0f) noexcept;
    uint32_t pixelWidth() const noexcept;
    uint32_t pixelHeight() const noexcept;
    const uint32_t* pixels() const noexcept;
    const uint8_t* encoded() const noexcept;
    uint32_t encodedSize() const noexcept;
    const char* mime() const noexcept;

private:
    uint32_t* objPixels = nullptr;
    uint8_t* objEncoded = nullptr;
    char* objMime = nullptr;
    uint32_t objEncodedSize = 0;
    uint32_t objWidth = 0;
    uint32_t objHeight = 0;
    Picture(const Vec3& center, float width) noexcept;
    friend struct Scene;
};

struct Cell final : Object
{
    Vec3 origin;
    float depth = 1.0f;
    CellMode mode = CellMode::Full;
    float padding = 0.05f;

    ~Cell() override;
    static Cell* gen(const Vec3& origin, uint32_t columns, uint32_t rows,
                     Color color = {}, CellMode mode = CellMode::Full,
                     float padding = 0.05f) noexcept;
    uint32_t columns() const noexcept;
    uint32_t rows() const noexcept;
    const Color* colors() const noexcept;
    Result fill(const CellRegion& region, Color color) noexcept;
    Result texture(const Asset* asset, const CellRegion& source = {},
                   const CellRegion& destination = {}) noexcept;

private:
    Color* objColors = nullptr;
    uint32_t objColumns = 0;
    uint32_t objRows = 0;
    uint32_t objFrames = 1;
    float objFrameDuration = 0.0f;
    Cell(const Vec3& origin, uint32_t columns, uint32_t rows, Color color,
         CellMode mode, float padding) noexcept;
    Result frames(uint32_t count, float duration) noexcept;
    friend const Color* sampleColors(const Cell* cell, uint32_t frame) noexcept;
    friend uint32_t sampleFrames(const Cell* cell) noexcept;
    friend float sampleDuration(const Object* object) noexcept;
    friend struct Space;
};

struct Rectangle final : Object
{
    Vec3 center;
    Vec2 size = {1.0f, 1.0f};
    float corner = 0.0f;

    static Rectangle* gen(const Vec3& center = {}, const Vec2& size = {1.0f, 1.0f},
                          float corner = 0.0f) noexcept;

private:
    Rectangle(const Vec3& center, const Vec2& size, float corner) noexcept;
};

// Joins the sampled closed outlines of its endpoint families, using bounds when there is
// no crossing. Groups use the outermost crossing; hidden endpoints still anchor the edge.
// Endpoints must share this scene. Connector local model is identity and cannot animate.
// A transformed ancestor must contain an endpoint; moveAlong endpoint/follower cycles fail.
struct Connector final : Object
{
    // World distance beyond each outline along the line between the family bounds centers.
    float padding = 0.0f;
    float tail = 0.0f;
    float tip = 0.0f;

    static Connector* gen(Object* from, Object* to) noexcept;
    Object* from() const noexcept;
    Object* to() const noexcept;

private:
    Object* source = nullptr;
    Object* destination = nullptr;
    Connector(Object* from, Object* to) noexcept;
};

enum struct AnimationKind : uint8_t
{
    Transform,
    Create,
    Fade,
    Stroke,
    Fill,
    Uncreate,
    FadeIn,
    FadeOut,
    Grow,
    GrowFromEdge,
    Shrink,
    Indicate,
    Morph,
    FillReveal,
    DrawBorderThenFill,
    Write,
    Rotate,
    Scale,
    MoveAlong,
    Wiggle
};

enum struct DrawDirection : uint8_t
{
    Forward,
    Reverse,
    Clockwise,
    CounterClockwise
};

enum struct GrowthEdge : uint8_t
{
    Left,
    Right,
    Bottom,
    Top
};

struct Animation
{
    Object* target = nullptr;
    AnimationKind kind = AnimationKind::Create;
    Mat4 model = Mat4::identity();
    Color color;
    float value = 1.0f;
    Object* related = nullptr;
    Vec3 offset;
    DrawDirection direction = DrawDirection::Forward;
    GrowthEdge growthEdge = GrowthEdge::Bottom;
    float scale = 1.0f;
    // Rotate/Scale use a parent-space pivot, related bounds center, or own center when centered.
    // Offset animates the pivot; Rotate uses angle radians about axis, and Scale uses stretch.
    Vec3 pivot;
    Vec3 axis = {0.0f, 0.0f, 1.0f};
    Vec3 stretch = {1.0f, 1.0f, 1.0f};
    float angle = 0.0f;
    bool centered = false;
    // Transform points and Rotate/Scale pivots follow a circular arc with this central angle
    // in radians; positive turns counterclockwise about axis. Zero moves straight.
    float arc = 0.0f;
    // MoveAlong: the target's center follows the outline of related from the fraction from
    // of its length to the fraction to (to < from travels backwards), turning with the path
    // when orient is set. Ordinary outlines are copied into the target's parent space when
    // played (single-piece Plot/Path, at most 2048 vertices/path and 262144/scene).
    // A connector reads trimmed ends
    // and parent space at every sample, then leaves the target at its clip-end position.
    // Endpoint dependencies must not form a cycle.
    float from = 0.0f;
    float to = 1.0f;
    bool orient = false;
    // Valid only for Morph: morphs a cursor-state copy, preserving the original source.
    // Completion removes the copy from the timeline; failed play discards it.
    bool copy = false;
    // Morph: explicit piece pairs as alternating zero-based source and target indices,
    // pairCount of each. Each piece pairs once; the remaining pieces pair automatically.
    // Paths and plots number contours in their stored order. Copied when played; surfaces
    // take no pairs. Lua and JavaScript use one-based indices.
    const uint32_t* pairs = nullptr;
    uint32_t pairCount = 0;
    // Surface morph colors sampled at colorSteps + 1 eased positions, one color/point/step.
    // colorCount covers all samples. Blends neighboring steps and source/target face colors;
    // null delegates to colormap, vertex colors or fill. Arrays are copied when played.
    const Color* sourceColors = nullptr;
    const Color* targetColors = nullptr;
    uint32_t colorSteps = 0;
    uint32_t colorCount = 0;

    static Animation create(Object* target, DrawDirection direction = DrawDirection::Forward) noexcept;
    static Animation uncreate(Object* target, DrawDirection direction = DrawDirection::Forward) noexcept;
    static Animation fillReveal(Object* target) noexcept;
    static Animation drawBorderThenFill(Object* target,
                                        DrawDirection direction = DrawDirection::Forward) noexcept;
    static Animation write(Object* target,
                           DrawDirection direction = DrawDirection::Forward) noexcept;
    static Animation fade(Object* target, float opacity) noexcept;
    static Animation fadeIn(Object* target, const Vec3& shift = {}, float scale = 1.0f) noexcept;
    static Animation fadeOut(Object* target, const Vec3& shift = {}, float scale = 1.0f) noexcept;
    static Animation growFromCenter(Object* target) noexcept;
    static Animation growFromEdge(Object* target, GrowthEdge edge) noexcept;
    static Animation shrinkToCenter(Object* target) noexcept;
    static Animation indicate(Object* target, Color color = {110, 168, 254, 255},
                               float scale = 1.2f) noexcept;
    // Morph childless, same-parent line/circle/rectangle/polygon/plot/path/curve outlines,
    // or two surface grids. Target must be newly attached at the cursor with no clips.
    // Multiple pieces pair by closed/open kind, area/length and normalized centroid;
    // extra pieces grow/shrink at their centroid. Keeps corners/winding and even-odd fill.
    // Limits: 4096 pieces/side and 1048576 retained points/scene. Explicit pairs are unique;
    // SurfaceMesh accepts no piece pairs. Disjoint families are required.
    // Source draws the interpolated shape until target takes over at completion.
    // Source then leaves the timeline; both objects remain Scene-owned.
    static Animation morph(Object* source, Object* target) noexcept;
    static Animation shift(Object* target, const Vec3& by) noexcept;
    static Animation transform(Object* target, const Mat4& model) noexcept;
    static Animation stroke(Object* target, Color color) noexcept;
    static Animation fill(Object* target, Color color) noexcept;
    static Animation rotate(Object* target, float radians, const Vec3& axis = {0.0f, 0.0f, 1.0f}) noexcept;
    static Animation rotate(Object* target, float radians, const Vec3& pivot, const Vec3& axis) noexcept;
    static Animation rescale(Object* target, const Vec3& factors) noexcept;
    static Animation rescale(Object* target, const Vec3& factors, const Vec3& pivot) noexcept;
    static Animation moveAlong(Object* target, Object* path, bool orient = false) noexcept;
    // Scales about the bounds center and returns while rotating up to angle radians,
    // with swings stored in value. May overlap a motion clip, as indicate does.
    static Animation wiggle(Object* target, float scale = 1.1f, float angle = 0.0628318531f,
                            float swings = 6.0f) noexcept;
};

// How a text change (AnimationTarget::text) turns one string into another. It counts in
// characters, never splitting one: combining marks stay with the character they mark, and
// an emoji with the modifiers and joined emoji that make it one picture.
enum struct TextChange : uint8_t
{
    // Replaces characters left to right as length changes; shortening replaces from the right.
    Sweep,
    // Erases back to the characters both strings begin with, then types the rest of the new
    // string, one character at a time.
    Retype
};

struct AnimationTarget
{
    static AnimationTarget from(Object* target) noexcept;
    AnimationTarget& shift(const Vec3& by) noexcept;
    AnimationTarget& transform(const Mat4& model) noexcept;
    AnimationTarget& opacity(float value) noexcept;
    AnimationTarget& stroke(Color color) noexcept;
    AnimationTarget& fill(Color color) noexcept;
    AnimationTarget& dashOffset(float value) noexcept;
    AnimationTarget& tail(float value) noexcept;
    AnimationTarget& tip(float value) noexcept;
    AnimationTarget& progress(float value) noexcept;
    // Stroke width in pixels. On a group it animates the shapes that take their width from it.
    AnimationTarget& strokeWidth(float pixels) noexcept;
    // A circle, arc or angle radius in world units, or a point's radius in pixels.
    AnimationTarget& radius(float value) noexcept;
    // The start direction and signed sweep of an arc, in radians.
    AnimationTarget& start(float radians) noexcept;
    AnimationTarget& angle(float radians) noexcept;
    // A text's size in points: its em box spans 4/3 of it, in pixels for billboard text and in
    // world units for plane text. A rectangle becomes a square with sides of this length.
    AnimationTarget& size(float value) noexcept;
    // A rectangle's width and height in world units.
    AnimationTarget& size(const Vec2& value) noexcept;
    // A rectangle's corner radius in world units, at most half its shorter side.
    AnimationTarget& corner(float value) noexcept;
    // Line/Arrow/Vector start/end distances in object-local units, before transforms.
    // Values must be finite and nonnegative; consumed segments are empty.
    // Warped geometry keeps the trim it had when warped.
    AnimationTarget& trimDistance(const Vec2& value) noexcept;
    // The gradient's stops and geometry. It keeps the current gradient's type, stop count and
    // explicit geometry fields, so each stop and point has a place to travel to.
    AnimationTarget& gradient(const Gradient& value) noexcept;
    // The end stop of a two-stop gradient that starts at the paint color.
    AnimationTarget& gradient(Color end) noexcept;
    // Carries every point along a circular arc of this central angle (radians) about +z
    // instead of a straight line.
    AnimationTarget& arc(float radians) noexcept;
    // Gaussian blur sigma in screen pixels, at most 256 (see Object::blur).
    AnimationTarget& blur(float pixels) noexcept;
    // The shadow's color, offset and blur travel to these.
    AnimationTarget& shadow(const Shadow& value) noexcept;
    // The number a counting text shows (see Text::count). It follows the curve past its ends,
    // so a back or elastic curve counts beyond the value and returns.
    AnimationTarget& value(float number) noexcept;
    // Animates plain Text content by Unicode clusters; counting Text uses value instead.
    // May overlap motion/fade/color. The string must remain valid until play() copies it.
    AnimationTarget& text(const char* value, TextChange change = TextChange::Sweep) noexcept;
    // Uses the higher endpoint layer during the clip, then switches to the final layer.
    // Descendant layers shift equally, preserving their relative order.
    AnimationTarget& layer(int32_t value) noexcept;
    // Animates finite local points with matching count: line/arrow endpoints, vector value/origin,
    // curve controls, polygon/plot/route/path vertices, arc center, or angle vertex/from/to.
    // Arrays must remain valid until play() copies them. Other kinds use size/radius/transform.
    // Warped geometry refuses new points; this claims the geometry channel.
    AnimationTarget& geometry(const Vec3* points, uint32_t count) noexcept;
    // Copies Plot geometry, piece ends and optional samples when played. Matching topology
    // interpolates points; changed counts/pieces switch halfway. Empty geometry draws nothing.
    // Null ends permits one piece. Matching x grids/bands blend y and clip each frame;
    // gaps and undefined values break interpolation, with exact endpoint geometry.
    // Optional contour copies grid/level for labels; omitted retains the previous grid.
    // No sampler is retained.
    AnimationTarget& geometry(const Vec3* points, uint32_t count, const uint32_t* ends,
                              uint32_t pieces, const PlotSamples* samples = nullptr,
                              const ContourGrid* contour = nullptr) noexcept;

private:
    Object* target = nullptr;
    Mat4 targetModel = Mat4::identity();
    Color targetStroke;
    Color targetFill;
    float targetOpacity = 1.0f;
    float targetDashOffset = 0.0f;
    float targetTail = 0.0f;
    float targetTip = 0.0f;
    float targetProgress = 1.0f;
    float targetArc = 0.0f;
    float targetWidth = 0.0f;
    float targetRadius = 0.0f;
    float targetStart = 0.0f;
    float targetAngle = 0.0f;
    Vec2 targetSize;
    float targetCorner = 0.0f;
    Vec2 targetTrimDistance;
    Gradient targetGradient;
    Color targetGradientEnd;
    float targetBlur = 0.0f;
    Shadow targetShadow;
    float targetValue = 0.0f;
    const char* targetText = nullptr;
    TextChange targetTextChange = TextChange::Sweep;
    int32_t targetLayer = 0;
    const Vec3* targetGeometry = nullptr;
    uint32_t targetGeometryCount = 0;
    // The pieces of a plot's new points, when geometry() was given them.
    const uint32_t* targetGeometryEnds = nullptr;
    uint32_t targetGeometryPieces = 0;
    bool targetGeometryPieced = false;
    PlotSamples targetSamples;
    ContourGrid targetContour;
    bool targetContoured = false;
    uint32_t properties = 0;
    friend struct Scene;
};

// How Scene::matchTransform() pairs and carries pieces.
struct MatchOptions
{
    // The central angle in radians of the arc each traveling piece follows, counterclockwise
    // about +z and strictly between -2 * pi and 2 * pi; zero travels straight.
    float arc = 0.0f;
    // Delays each later piece of either object as play() delays each later target.
    float lag = 0.0f;
    // Explicit alternating source/target piece pairs, pairCount pairs. Circles/rectangles
    // interpolate one shape; other nonmatching pieces cross-fade while traveling.
    Object* const* pairs = nullptr;
    uint32_t pairCount = 0;
    // Pairs each remaining text of the target with the first unpaired text of the source that
    // reads the same. False keeps only the given pairs, as a diff that knows better does.
    bool strings = true;
    // Matches unchanged lines by longest common subsequence, then moved identical lines,
    // then text runs inside changed lines. Children represent lines; at most 4096 per object.
    bool diff = false;
    // Matches remaining circles/rectangles in order. Unequal counts split or merge evenly:
    // one shape travels as the primary pair; extras fade from or toward its geometry.
    bool shapes = false;
    // Fraction (0, 1] for unmatched source fade-out at the start and target fade-in at the end.
    // One uses the whole duration; below 0.5 separates the fades.
    float fade = 1.0f;
    // Matches a cursor-state clone and preserves source, including an animated source.
    // Explicit pairs name source pieces; their copies are used.
    bool copy = false;
    // Adds a shared arc-length detour from an outline snapshot in world space, up to 2048 points.
    // Subtracts its endpoint chord to preserve each pair's exact endpoints and relative spacing.
    // Path must share this scene; live connectors and nonzero arc are excluded.
    const Object* path = nullptr;
};

enum struct EmphasisKind : uint8_t
{
    // A temporary frame draws and erases while a bright segment circles it.
    Circumscribe,
    // A temporary bright segment follows the target outline from start to beyond its end.
    // Uses one-piece outlines, or the live drawn outline of a connector.
    PassingFlash,
    // Temporary radial lines start padding beyond bounds, evenly spaced counterclockwise
    // from +x, and draw outward before erasing.
    Flash,
    // A temporary ring expands by size on every side and fades linearly.
    // Uses circle/rounded rectangle geometry, or padded bounds for other kinds; curve eases growth.
    Ripple
};

// The frame of a circumscribe.
enum struct EmphasisShape : uint8_t
{
    Rectangle,
    // An ellipse through the corners of the padded bounds, a circle around a square.
    Ellipse
};

// A mark that Scene::emphasize() draws once on an object with shapes of its own.
struct Emphasis
{
    EmphasisKind kind = EmphasisKind::Circumscribe;
    Object* target = nullptr;
    Color color = {110, 168, 254, 255};
    // Stroke width in pixels: of the frame of a circumscribe, whose bright stretch is twice as
    // wide, of the stretch of a passing flash, 6 from passingFlash(), and of the lines of a
    // flash and the ring of a ripple.
    float width = 3.0f;
    // World units between the target's bounds and the frame, the lines of a flash or the ring
    // of a ripple, 0 from ripple().
    float padding = 0.1f;
    // The share of the outline the bright stretch covers, greater than 0 and at most 1.
    float window = 0.3f;
    EmphasisShape shape = EmphasisShape::Rectangle;
    // The world length of each line of a flash, or how far a ripple grows on every side, 0.4
    // from ripple().
    float size = 0.2f;
    // How many lines a flash bursts into, from 1 to 64.
    uint32_t count = 12;
    // Where a flash without a target bursts, in world coordinates.
    Vec3 point;

    static Emphasis circumscribe(Object* target,
                                 EmphasisShape shape = EmphasisShape::Rectangle) noexcept;
    static Emphasis passingFlash(Object* target) noexcept;
    static Emphasis flash(Object* target) noexcept;
    static Emphasis flash(const Vec3& point) noexcept;
    static Emphasis ripple(Object* target) noexcept;
};

// How Scene::clone() attaches the copy.
struct CloneOptions
{
    // The parent of the copy, an object of the scene that is not removed. nullptr attaches the
    // copy beside the source: to the source's parent, or to the scene.
    Object* parent = nullptr;
    // The id of the copy; nullptr derives one from the source's (see Scene::clone()).
    const char* tag = nullptr;
    // Called with the copy before it is attached, to change its own settings, such as its
    // colors, opacity or model, as an object is set up before it is added; what is below it
    // must stay as it is. A result other than Success stops the clone, which attaches nothing.
    Result (*edit)(Object* copy, void* data) noexcept = nullptr;
    void* data = nullptr;
};

struct Config
{
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t fps = 30;
    Color background = {255, 255, 255, 255};
    Camera camera;
    CameraMode cameraMode = CameraMode::Fixed;
    CameraView cameraView = CameraView::TwoD;
    bool antialiasing = true;
    bool loop = false;
};

struct Viewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};

// A second look at a scene's own objects through a camera of its own, drawn into a rectangle
// of the frame and only inside it (see Scene::view()). The view draws the objects, not the
// scene's viewports, transitions or other views.
struct SceneView
{
    // Where the view shows, as fractions of the frame from its top left corner, as a Viewport
    // gives them.
    Viewport viewport;
    // The camera the view opens with, in the scene's world: its height or field of view spans
    // the height of the rectangle. Looks and orbits of the view move it later.
    Camera camera;
    CameraView cameraView = CameraView::TwoD;
    // The view draws the objects whose layer is at most this one and lies above them, so an
    // object on a higher layer draws over the view and stays out of it. A group counts by its
    // own layer, and a mask goes with the layer it masks.
    int32_t layer = INT32_MAX;
    // Optical: stroke widths, point radii, dashes and billboard text grow with the view's
    // magnification over the scene's camera, as a lens would show them. Otherwise they keep
    // their pixel sizes, as they do when the scene's own camera zooms.
    bool optical = true;
    // Fills the rectangle below what the view draws; alpha 0 lets the frame show through.
    Color background = {0, 0, 0, 0};
    // How opaque the whole view starts, from 0 to 1.
    float opacity = 1.0f;
    // An object whose drawn center the view's target stays on, the eye moving with it, at every
    // moment the object draws something (nullptr: none).
    const Object* follow = nullptr;
    // Objects the view leaves out, each with everything below it.
    const Object* const* hidden = nullptr;
    uint32_t hiddenCount = 0;
};

// How a transition of Scene::transition() hands one scene over to the next.
enum struct TransitionKind : uint8_t
{
    // Objects with the same tag in both scenes travel from their place in the outgoing scene
    // to their place in the incoming one, and every other object fades out or in on its own.
    Match = 0,
    // The incoming scene fades in over the outgoing one as a whole, and nothing matches. When
    // the background of the incoming scene lets the outgoing one show through, the outgoing
    // scene fades out as far as that background would leave it in view.
    Fade,
    // The incoming scene pushes the outgoing one out of the viewport: it enters from the side
    // named by from while the outgoing scene leaves by the opposite side, both whole and
    // opaque, and each shows only inside the viewport.
    Slide,
    // The outgoing scene grows until its area fills the viewport, and the incoming scene,
    // drawn inside that area, grows and fades in with it.
    ZoomIn,
    // The incoming scene starts grown so that its area fills the viewport and shrinks back to
    // its own size, while the outgoing scene, drawn inside that area, shrinks and fades out.
    ZoomOut
};

// The side of the viewport a Slide brings the incoming scene in from.
enum struct TransitionSide : uint8_t
{
    Left,
    Right,
    Bottom,
    Top
};

// The style of one transition of Scene::transition().
struct TransitionStyle
{
    TransitionKind kind = TransitionKind::Match;
    TransitionSide from = TransitionSide::Right;
    // The area of a zoom, as fractions of the viewport from its top left corner. It widens
    // about its center to the shape of the viewport and moves inside it where it would stick
    // out, so it must overlap the viewport and fit in it once widened.
    Viewport area = {0.0f, 0.0f, 0.0f, 0.0f};
    // An object of either scene of the transition that gives a zoom its area instead: the part
    // of the frame its geometry bounds cover at the end of its scene when the transition is
    // made, without stroke or other paint extents, with the margin below.
    const Object* object = nullptr;
    // A slide or a zoom that also matches: objects with the same tag in both scenes travel as
    // they do in Match, above both scenes and in the viewport itself, while each scene moves
    // without them.
    bool match = false;
    // Finite nonnegative zoom margin as a fraction of the larger normalized object side.
    // Clamped to available viewport space; zero uses exact bounds. Applies only to object areas.
    float areaMargin = 0.05f;
};

struct RuntimeModifier
{
    using ObjectCallback = Result (*)(const Object* object, float time, Mat4& model,
                                      float& opacity, float& progress, void* data) noexcept;
    using CameraCallback = Result (*)(float time, Camera& camera, CameraView& view,
                                      void* data) noexcept;
    using InputCallback = Result (*)(const CameraInput& input, void* data) noexcept;
    using FillCallback = Result (*)(const Object* object, float time, Color& fill,
                                    void* data) noexcept;

    ObjectCallback object = nullptr;
    CameraCallback camera = nullptr;
    InputCallback input = nullptr;
    void* data = nullptr;
    FillCallback fill = nullptr;
    const void* key = nullptr;
};

struct PixelSample
{
    const Object* object = nullptr;
    Vec2 position;
    Color color = {0, 0, 0, 0};
};

struct Scene
{
    static constexpr uint32_t RuntimeLimit = 8u;

    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    static Scene* gen(const Config& config = {}) noexcept;
    // Assign before objects, mounts or timeline authoring; names are copied.
    Result theme(const Theme& theme) noexcept;
    const Theme& theme() const noexcept;
    // Transfers ownership only after validating the complete subtree and its unique tags.
    // Failure preserves the subtree and scene. Objects have one parent and cannot be reused.
    Result add(Object* object) noexcept;
    // Refreshes direct edits only at cursor zero before definitions are sealed.
    // Later direct field changes fail snapshot validation; use timeline targets instead.
    Result update(Object* object) noexcept;
    // Measures detached or attached text in world units using authored or theme font/size.
    // Billboard pixels convert at the authored camera, at its target in perspective.
    // InsufficientCondition: no font loaded at all (see Text::measure()).
    Result measure(const Text* text, TextExtent& output) noexcept;
    // Measures a sample string in the same style and world units without changing text.
    Result measure(const Text* text, const char* sample, TextExtent& output) noexcept;
    // Gives a detached text a fixed box holding its starting string and sample. Later strings
    // may overflow it; align keeps their anchor fixed. Add the text after reserving its box.
    // InvalidArguments: null text or sample, or text already attached to a scene.
    Result reserve(Text* text, const char* sample) noexcept;
    // Gives the object of target its values at once, after the checks of play(). Before the
    // first play, wait, look or remove they become the object's starting state, as a direct
    // edit followed by update(object) does; afterwards they take effect at the cursor.
    Result update(const AnimationTarget& target) noexcept;
    // Mounts a normalized finite positive rectangle inside [0, 1], transferring ownership
    // on success and sealing child authoring. No self/reuse/cycles; 64 children/node, depth 16.
    // Lua loads share whole-tree budgets, with at most 64 mounted children.
    Result viewport(Scene* scene, const Viewport& viewport) noexcept;
    // Transfers and seals distinct stages after complete validation; one sequence/parent.
    // Stage reuse is invalid. Holds are finite nonnegative, one value or one per stage;
    // style count is zero, one or count - 1. Match overlay applies only to slide/zoom.
    Result transition(Scene* const* scenes, uint32_t count, const Viewport& viewport = {},
                      float duration = 1.0f, float hold = 0.0f, const AnimCurve& curve = {}) noexcept;
    // Styles the transitions as well: styleCount is 1 to give every transition the same style,
    // or count - 1 to give each its own in order, and 0 leaves them all to Match.
    Result transition(Scene* const* scenes, uint32_t count, const Viewport& viewport,
                      float duration, float hold, const AnimCurve& curve,
                      const TransitionStyle* styles, uint32_t styleCount, bool live = true) noexcept;
    // Holds is one duration for all stages, or count durations in stage order. Live stages
    // start at zero when their incoming transition begins and hold their last frame at the end.
    // With live false, every stage shows its completed frame throughout.
    Result transition(Scene* const* scenes, uint32_t count, const Viewport& viewport,
                      float duration, const float* holds, uint32_t holdCount, const AnimCurve& curve,
                      const TransitionStyle* styles = nullptr, uint32_t styleCount = 0u,
                      bool live = true) noexcept;
    // Ends this subtree's visibility at the cursor. Objects remain Scene-owned for earlier samples.
    Result remove(Object* object) noexcept;
    // Clones cursor-sampled subtree state, including style/composition, without animation clips.
    // Attaches at the cursor and preserves authored fonts/colors, local transform and warped shape.
    // Uses options.tag or a unique <tag>:copy[N]; descendant tags replace the source prefix
    // or acquire the copy prefix. Untagged objects stay untagged. Internal connector endpoints
    // remap to copies; external endpoints stay original. Hidden descendants remain hidden.
    // Mask and masked object must both lie in source. edit must preserve the subtree.
    // Failure attaches nothing. InvalidArguments: foreign object, tag collision, external mask
    // or structural edit. InsufficientCondition: hidden source or active morph/warp.
    // NonSupport: Space, Cell, module group or nested connector-dependent endpoint.
    Result clone(Object* source, Object*& output, const CloneOptions& options = {}) noexcept;
    // Cross-fades disjoint families and advances the cursor by duration. Source leaves
    // the timeline at completion; both families remain Scene-owned for earlier samples.
    // Target must be newly attached at the cursor without clips; source must be live,
    // and its scheduled clips must finish by the fade's end.
    Result fadeTransform(Object* source, Object* target, float duration = 1.0f,
                         const AnimCurve& curve = {}) noexcept;
    // Matches descendant pieces; equal Text strings pair first, preserving sampled style
    // and placement while traveling. Unpaired source/target pieces fade out/in. Attach target
    // at the cursor immediately before this call, without clips.
    // Source must be at rest unless options.copy snapshots its cursor state.
    // Completion removes source or its temporary copy from the timeline; Scene ownership stays.
    Result matchTransform(Object* source, Object* target, float duration = 1.0f,
                          const AnimCurve& curve = {}, const MatchOptions& options = {}) noexcept;
    // Reports intentional overlap during a match/cross-fade: paired pieces, yielding slots
    // and roots; copies also exempt their originals. Cross-fades exempt both families.
    bool handoff(const Object* first, const Object* second, float time) const noexcept;
    // False for foreign objects or families removed at birth, including hidden clone pieces.
    bool shows(const Object* object) const noexcept;
    // Fades selected nongroup descendants to full opacity and other descendants to dim.
    // Groups keep their opacity; empty selection restores all. Unchanged values stay still;
    // if none changes, advances the cursor by duration.
    Result highlight(Object* target, Object* const* selection, uint32_t count, float dim = 0.25f,
                     float duration = 0.5f, const AnimCurve& curve = {}) noexcept;
    // Adds temporary root shapes for duration and removes them afterward, advancing the cursor.
    // Snapshots target geometry at the cursor; frames/rings use world xy bounds and passing
    // flash uses its placed outline. Uses the family's highest layer, or zero for a point flash.
    // Tags use <target>.<effect>.<n>.<part> with the first unused n.
    // InvalidArguments: foreign target, invalid values or unavailable geometry.
    Result emphasize(const Emphasis& emphasis, float duration = 1.0f,
                     const AnimCurve& curve = {}) noexcept;
    // Animates same-parent items along direction, gap apart, centered in parent space.
    // Newly attached unclipped items grow into place; existing items slide.
    // If no item moves, advances the cursor by duration.
    Result arrange(Object* const* items, uint32_t count, const Vec3& direction = {1.0f, 0.0f, 0.0f},
                   float gap = 0.25f, const Vec3& center = {}, float duration = 1.0f,
                   const AnimCurve& curve = {}) noexcept;
    // Maps target subtrees in their parent spaces, sampling map once during construction.
    // Subdivides outlines to step-sized pieces, then interpolates to the mapped geometry.
    // Shapes bend; text/pictures/points move anchors; connectors retain live endpoints;
    // Space warps grids, axes and labels. Arrow tips retain pixel size. Advances the cursor.
    // Requires invertible intermediate models; no Cells or SurfaceMesh. Warps claim geometry,
    // morph and circle/rectangle size/corner channels; overlapping clips fail. Warped geometry,
    // trim and shape sizes remain fixed; restore does not undo warp and clone retains it.
    // InvalidArguments: empty/duplicate/nested/foreign targets, invalid map/duration/step.
    // InsufficientCondition: descendants born later, singular model, overlap, or more than
    // 2097152 retained warp points.
    Result warp(Object* const* targets, uint32_t count, PointMap map, void* data = nullptr,
                float duration = 1.0f, const AnimCurve& curve = {}, float step = 0.05f) noexcept;
    // Pushes cursor state for each target subtree, up to 4096 objects/call, without advancing time.
    // Saves transforms, paint/effects, geometry, layer, text and counting value. Nested saves
    // form a stack; active text/geometry changes save their destinations.
    // Warp geometry stays as authored.
    Result save(Object* const* targets, uint32_t count) noexcept;
    // Pops the latest saved subtree state and restores changed properties over duration;
    // zero applies immediately. lag delays later targets. No saved state fails atomically.
    // Conflicting active channels fail, except additive translation; layer changes discretely.
    // Added/removed descendants retain their state. An unchanged restore waits for duration.
    // Does not revive removed descendants, undo warps or switch plain/counting Text modes.
    // Ends layout authoring, as play() does.
    Result restore(Object* const* targets, uint32_t count, float duration = 0.0f,
                   const AnimCurve& curve = {}, float lag = 0.0f) noexcept;
    // Validates and reserves the whole command atomically; targets are distinct scene objects.
    // Non-model channels require chronological nonoverlap. Model motion may layer with
    // invertible authored models; shift needs no inverse. Times need distinct finite float
    // boundaries. Family birth/removal gates apply; remove waits for active clips.
    Result play(const Animation& animation, float duration = 1.0f, const AnimCurve& curve = {}) noexcept;
    Result play(const Animation* animations, uint32_t count, float duration,
                const AnimCurve& curve = {}, float lag = 0.0f) noexcept;
    Result play(const AnimationTarget& target, float duration = 1.0f, const AnimCurve& curve = {}) noexcept;
    Result play(const AnimationTarget* targets, uint32_t count, float duration,
                const AnimCurve& curve = {}, float lag = 0.0f) noexcept;
    // Adds a modifier, or replaces the one with the same data, and removes one by its data;
    // other modules' modifiers stay. Registered callback/data lifetimes must cover every sample.
    Result runtimeAdd(const RuntimeModifier* modifier) noexcept;
    Result runtimeRemove(void* data) noexcept;
    // Applies interactive camera input through registered runtime modifiers.
    // Fixed mode returns InsufficientCondition; no accepting modifier returns NonSupport.
    Result cameraInput(const CameraInput& input) noexcept;
    // Validates a float-safe camera envelope with positive rounded clipping planes.
    // 2D rejects orbit; layered return to 2D waits for earlier moves. Turning/projection
    // cannot layer while earlier motion remains 2D.
    Result look(const Camera& camera, CameraView view, float duration = 1.0f,
                const AnimCurve& curve = {}) noexcept;
    // Circles a 3D camera about its target: the eye and the up direction turn by angle
    // radians about axis, counterclockwise seen from the axis tip. Unlike look, which takes
    // the shortest turn, any angle is kept, so 2 * pi circles the target once.
    Result orbit(float angle, const Vec3& axis, float duration = 1.0f,
                 const AnimCurve& curve = {}) noexcept;
    // Move the camera of view index as look and orbit move the scene's camera, from the cursor.
    // A move that begins while another move of the same view runs adds its change to it; the
    // cameras of the scene and of each view move on their own.
    Result look(uint32_t view, const Camera& camera, CameraView cameraView, float duration,
                const AnimCurve& curve) noexcept;
    Result orbit(uint32_t view, float angle, const Vec3& axis, float duration,
                 const AnimCurve& curve) noexcept;
    // Fades the whole of view index to opacity, from 0 to 1, from the cursor.
    Result fade(uint32_t view, float opacity, float duration, const AnimCurve& curve) noexcept;
    Result wait(float duration = 1.0f) noexcept;
    // Cursor starts the next play/look/wait/add. seek may move backward for parallel tracks;
    // non-model property clips remain chronological and nonoverlapping. Layered camera moves
    // add their deltas. Times sum shortest float decimals to avoid frame-boundary drift.
    float cursor() const noexcept;
    Result seek(float time) noexcept;
    // Unique named cursor times: at most 256 markers, names up to 128 bytes.
    Result mark(const char* name) noexcept;
    bool marker(const char* name, float& time) const noexcept;
    uint32_t markerCount() const noexcept;
    const char* markerAt(uint32_t index, float& time) const noexcept;
    // Why the most recent failed timeline, attachment, update or layout call was rejected, in
    // plain words, or nullptr when no detail is known.
    const char* diagnostic() const noexcept;
    float duration() const noexcept;
    // Object animation clips authored so far. A play call adds one per target and one per
    // shape that takes an animated stroke, fill or width from a target group.
    uint32_t clipCount() const noexcept;
    // Estimated clip reads/frame: each clip counts for its object and every descendant.
    // Includes mounted viewport and transition scenes.
    uint64_t sampleWork() const noexcept;
    CameraView view(float time = 0.0f) const noexcept;
    // Returns the sampled Text byte length, or zero for a foreign/hidden Text.
    // Writes a terminating zero only when output has room for length + 1 bytes.
    uint32_t text(const Text* text, float time, char* output, uint32_t capacity) const noexcept;
    uint32_t count() const noexcept;
    Object* object(uint32_t id) const noexcept;
    Object* objectAt(uint32_t index) const noexcept;
    Object* object(const char* tag) const noexcept;
    // Names the fonts that this scene's texts, and those of the scenes it shows in viewports
    // and transitions, are set in but that are not loaded, each once and at most capacity of
    // them; the default font draws those texts. The names live as long as the texts. Returns
    // how many it names.
    uint32_t missingFonts(const char** output, uint32_t capacity) const noexcept;
    uint32_t viewportCount() const noexcept;
    Scene* sceneAt(uint32_t index) const noexcept;
    bool viewportAt(uint32_t index, Viewport& viewport) const noexcept;
    // Adds a zero-based view from the cursor; optional index receives its position.
    // Follow/hidden objects must share this scene; hidden list is copied (at most 256).
    // At most 64 native views (8 Lua/JS); excludes other views, mounts and transitions.
    // View camera/fade clips cannot precede birth.
    Result view(const SceneView& view, uint32_t* index = nullptr) noexcept;
    uint32_t viewCount() const noexcept;
    // The view as made, with the camera and opacity its latest look, orbit or fade leaves.
    bool viewAt(uint32_t index, SceneView& view) const noexcept;
    uint32_t transitionCount() const noexcept;
    Scene* transitionAt(uint32_t index) const noexcept;
    Config& config() noexcept;
    const Config& config() const noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;

    explicit Scene(const Config& config) noexcept;
    static uint32_t depth(const Scene* scene) noexcept;
    Result attach(Object* object, Object* parent) noexcept;
    friend struct RendererBuilder;
    friend struct Renderer;
    friend struct Object;
    friend struct Space;
};

struct Surface
{
    Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    ~Surface();
    Result resize(uint32_t width, uint32_t height) noexcept;
    uint32_t* data() noexcept;
    const uint32_t* data() const noexcept;
    uint32_t width() const noexcept;
    uint32_t height() const noexcept;
    uint32_t stride() const noexcept;

private:
    uint32_t* buffer = nullptr;
    uint32_t bufferWidth = 0;
    uint32_t bufferHeight = 0;
    uint32_t bufferStride = 0;
};

// Draws a scene on the CPU, into a Surface, and measures what it drew. The experimental GL
// module (vis_gl.h) draws through OpenGL instead.
struct Renderer
{
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    static Renderer* gen() noexcept;
    Result font(const char* path) noexcept;
    Result font(const char* name, const void* data, uint32_t size, const char* mime = "ttf") noexcept;
    // Why the most recent font, bounds, intersects, layout, sample or render call failed, in
    // plain words, such as the font a text needs; nullptr when the renderer has no reason.
    const char* diagnostic() const noexcept;
    Result bounds(const Scene* scene, const Object* object, float time, BBox& output) noexcept;
    Result intersects(const Scene* scene, const Object* first, const Object* second, float time,
                      bool& output, float padding = 0.0f) noexcept;
    Result layout(const Scene* scene, float time, LayoutReport& output,
                  float padding = 0.0f) noexcept;
    Result sample(const Scene* scene, float time, const Vec2& position,
                  const Object* const* candidates, uint32_t count,
                  PixelSample& output, uint32_t pixelRatio = 1u) noexcept;
    Result render(const Scene* scene, float time, Surface& surface) noexcept;
    Result render(const Scene* scene, float time, Surface& surface,
                  uint32_t pixelRatio) noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;
    Renderer() noexcept;
};

struct Saver
{
    // Built-in savers sample scene time; interaction clocks must be advanced by the host.
    static Result png(const Surface& surface, const char* path) noexcept;
    // Saves every frame of the timeline as a GIF or an MP4, by the path's extension, at fps
    // frames per second (0: the scene's). Both run ffmpeg from the PATH, so neither is
    // supported in WebAssembly or Windows builds. An MP4 takes soundtrack as its audio: any
    // file ffmpeg reads, such as a WAV or MP3, played from Scene time 0. A longer track is cut
    // where the video ends and a shorter one leaves the rest silent. Only .mp4 carries audio;
    // nullptr saves no audio.
    static Result save(const Scene* scene, Renderer& renderer, const char* path,
                       uint32_t fps = 0, const char* soundtrack = nullptr) noexcept;
};

struct Lua
{
    using AssetResolver = const Asset* (*)(const char* name, void* data);

    // Runs construction callbacks and releases them before rendering; closes the VM unless
    // a runtime module retains it until scene destruction and seals authoring.
    // One load shares scene/object/clip/point budgets and separate 64 MiB Lua/native budgets.
    // Text-only source needs a returned owning root scene; bytecode is rejected.
    static Result load(const char* source, uint32_t size, const char* name, Scene** scene,
                       char* error, uint32_t errorSize, AssetResolver resolver = nullptr,
                       void* resolverData = nullptr, const Theme* adaptiveTheme = nullptr,
                       bool* adaptiveThemeUsed = nullptr) noexcept;
};

// Displacement for m*x'' + c*x' + k*x = 0, without endpoint/settling clamps.
// Finite inputs require seconds/damping >= 0 and mass/stiffness > 0. Units: displacement
// scene units, velocity units/s, stiffness mass/s^2, damping mass/s. Invalid coefficient
// ratios, phase or result return NaN.
double springResponse(double seconds, double mass, double stiffness, double damping,
                      double displacement, double velocity = 0.0) noexcept;

float ease(const AnimCurve& curve, float progress) noexcept;
const char* result(Result result) noexcept;
const char* type(Type type) noexcept;

}  // namespace vis

#endif
