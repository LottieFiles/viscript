// The display list: the draw commands and layers of one frame, between the scene and the
// renderers.
#ifndef _VIS_DISPLAY_LIST_H_
#define _VIS_DISPLAY_LIST_H_

#include "vis.h"

namespace vis
{

static constexpr uint32_t DISPLAY_COMMAND_LIMIT = 65536;

enum struct CommandType : uint8_t
{
    Path,
    Circle,
    Text,
    Number,
    Picture
};

// Picture source: either encoded bytes (svg/png/jpg/...) or raw RGBA pixels.
struct PictureSource
{
    const uint8_t* encoded = nullptr;
    uint32_t encodedSize = 0;
    const char* mime = nullptr;
    const uint32_t* pixels = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

enum struct PathCap : uint8_t
{
    Round,
    Butt
};

// The layer of a composited object (see Scene::Impl::composited()): its drawings and those
// of its descendants are drawn together and composited onto the enclosing layer at once.
struct DisplayLayer
{
    const Object* object = nullptr;
    // 1-based index of the enclosing layer; 0 is the scene itself, and the parent of a mask.
    uint32_t parent = 0;
    // 1-based index of the layer this one shows through, or 0.
    uint32_t mask = 0;
    MaskMode maskMode = MaskMode::Alpha;
    // How the layer combines with the enclosing one.
    BlendMode blend = BlendMode::Normal;
    // Effects applied to the layer, in screen pixels.
    float blur = 0.0f;
    Shadow shadow;
    // The layer is the mask of another one and draws only through it.
    bool masking = false;
    // Number of layers above this one; the scene is level 0.
    uint32_t level = 1;
    float opacity = 1.0f;
    // Place among the drawings and layers of the enclosing layer: the object's layer, the
    // mean depth and the first emission order of the drawings inside, nested ones included.
    int32_t layer = 0;
    float depth = 0.0f;
    uint32_t order = UINT32_MAX;
    uint32_t count = 0;
};

struct DrawCommand
{
    const Object* object = nullptr;
    const Object* counterpart = nullptr;
    // 1-based index of the layer the command draws into; 0 is the scene itself.
    uint32_t composite = 0;
    CommandType type = CommandType::Path;
    Vec2* points = nullptr;
    uint32_t count = 0;
    // A path of several contours: where each ends in points, the last at count. It fills by
    // the even-odd rule. A path of one contour leaves it empty.
    uint32_t* ends = nullptr;
    uint32_t contours = 0;
    Style style;
    Vec2 center;
    Vec2 align;
    Mat3 transform = Mat3::identity();
    const char* text = nullptr;
    // The command's own copy of text, for a string no object keeps, such as a counting
    // text's number at this frame.
    char* buffer = nullptr;
    const char* font = nullptr;
    PictureSource picture;
    ImageFilter filter = ImageFilter::Bilinear;
    float radius = 0.0f;
    float size = 0.0f;
    // A text's lean to the right per unit of height above its first baseline (Text::slant).
    float slant = 0.0f;
    bool tabular = false;
    float number = 0.0f;
    float depth = 0.0f;
    // Per-point screen-linear depth: reciprocal camera distance in perspective, distance
    // in orthographic. Present for unfilled single-contour strokes and surface/point commands;
    // split() uses it to resolve surface occlusion. Null for unsupported commands.
    float* depths = nullptr;
    // The depth the layers around the command average it at in sort(): depth, unless split()
    // cut or moved the drawing, whose first piece then keeps the depth of the whole (piece 1)
    // and whose later pieces count for nothing (piece 2), so a layer keeps its place.
    float whole = 0.0f;
    uint8_t piece = 0;
    // How many layers above its object's layer the command draws, as a space draws its axes
    // and numbers over its grid. split() lowers the parts a surface in that layer hides.
    uint8_t lift = 0;
    int32_t layer = 0;
    uint32_t order = 0;
    uint8_t opacity = 255;
    uint8_t visualOpacity = 255;
    bool closed = false;
    bool transformed = false;
    PathCap cap = PathCap::Round;
    LayoutVisualKind visualKind = LayoutVisualKind::Stable;
};

static constexpr uint32_t DISPLAY_LAYER_LIMIT = 4096;

// The parts of a blend of two lists: the objects that pair, and the objects of either list
// that do not.
static constexpr uint8_t BLEND_MATCHED = 1u;
static constexpr uint8_t BLEND_FROM = 2u;
static constexpr uint8_t BLEND_TO = 4u;
static constexpr uint8_t BLEND_ALL = BLEND_MATCHED | BLEND_FROM | BLEND_TO;

struct DisplayList
{
    DrawCommand* commands = nullptr;
    uint32_t count = 0;
    uint32_t capacity = 0;
    DisplayLayer* layers = nullptr;
    uint32_t layerCount = 0;
    uint32_t layerCapacity = 0;
    const Object* object = nullptr;
    const Object* counterpart = nullptr;
    // Layer of the commands appended next, as in DrawCommand::composite.
    uint32_t composite = 0;
    // Lift of the commands appended next, as in DrawCommand::lift.
    uint8_t lift = 0;
    LayoutVisualKind visualKind = LayoutVisualKind::Stable;
    uint8_t visualOpacity = 255;

    DisplayList() = default;
    ~DisplayList();
    DisplayList(const DisplayList&) = delete;
    DisplayList& operator=(const DisplayList&) = delete;
    // ends and contours: the contours of the path, as in DrawCommand; nullptr for one.
    // depths: the depth of each point, as in DrawCommand, or nullptr.
    Result path(const Vec2* points, uint32_t count, bool closed, const Style& style,
                int32_t layer, float depth = 0.0f,
                PathCap cap = PathCap::Round, const uint32_t* ends = nullptr,
                uint32_t contours = 0, const float* depths = nullptr) noexcept;
    // depths: the depth of the center, as in DrawCommand, or nullptr; likewise for number().
    Result circle(const Vec2& center, float radius, const Style& style, int32_t layer, float depth = 0.0f,
                  const float* depths = nullptr) noexcept;
    // copy: the command keeps a copy of value, which may then go away.
    Result text(const Vec2& point, const char* value, const char* font, float size,
                const Vec2& align, const Style& style, int32_t layer,
                float depth = 0.0f, const Mat3* transform = nullptr, bool copy = false,
                float slant = 0.0f, bool tabular = false) noexcept;
    Result number(const Vec2& point, float value, float size, const Vec2& align, const Style& style,
                  int32_t layer, float depth = 0.0f, const float* depths = nullptr) noexcept;
    Result picture(const Vec2* corners, const PictureSource& source, ImageFilter filter,
                   uint8_t opacity, int32_t layer, float depth) noexcept;
    Result blend(const DisplayList& from, const DisplayList& to, const uint32_t* matches,
                 uint32_t fromObjects, uint32_t toObjects, const Vec2& fromSize,
                 const Vec2& toSize, float progress, uint8_t parts = BLEND_ALL) noexcept;
    // Index (1-based) of the layer of object, or 0 when the list has none.
    uint32_t find(const Object* object) const noexcept;
    // Appends a layer and returns its 1-based index, or 0 when out of memory or past the limit.
    uint32_t add(const DisplayLayer& layer) noexcept;
    // Whether the command draws into a mask rather than onto the scene.
    bool masking(const DrawCommand& command) const noexcept;
    // Orders the commands back to front. Layers keep their drawings together: a layer takes
    // the place of its first drawing among its siblings, and its drawings sort within it.
    // OutOfMemory leaves the commands in their order.
    Result sort() noexcept;
    // Resolves surface occlusion within the same 3D layer/composite before sort().
    // Only unfilled single-contour strokes, route heads, points and numbers participate;
    // filled paths, text and pictures remain whole. At most 256 pieces/stroke and 8192 extra
    // commands; exhausted budgets preserve whole strokes. No change in 2D or without surfaces.
    Result split(const Config& config) noexcept;

private:
    DrawCommand* append() noexcept;
};

}  // namespace vis

#endif
