#ifndef _VIS_TEST_SCENE_CHECKS_H_
#define _VIS_TEST_SCENE_CHECKS_H_

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>

#include "vis.h"

using namespace vis;

extern int failures;

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                                        \
        }                                                                                      \
    } while (false)

static inline uint32_t changed(const Surface& surface, Color background)
{
    auto pixel = static_cast<uint32_t>(background.a) << 24 | static_cast<uint32_t>(background.b) << 16 | static_cast<uint32_t>(background.g) << 8 | background.r;
    auto count = 0u;
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] != pixel) count++;
        }
    }
    return count;
}

static inline uint64_t checksum(const Surface& surface)
{
    auto hash = 14695981039346656037ull;
    for (auto i = 0u; i < surface.width() * surface.height(); i++) {
        hash ^= surface.data()[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static inline uint32_t pixel(Color color)
{
    return static_cast<uint32_t>(color.a) << 24 | static_cast<uint32_t>(color.b) << 16
        | static_cast<uint32_t>(color.g) << 8 | color.r;
}

static inline bool same(Color first, Color second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b
           && first.a == second.a;
}

static inline bool near(float lhs, float rhs, float epsilon = 1.0e-4f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

static inline const LayoutVisual* _layoutVisual(const LayoutReport& report,
                                                const Object* object)
{
    for (auto i = 0u; i < report.visualCount(); i++) {
        auto visual = report.visualAt(i);
        if (!visual) continue;
        auto primary = report.objectAt(visual->object);
        if (primary && primary->object == object) return visual;
        auto counterpart = report.objectAt(visual->counterpart);
        if (counterpart && counterpart->object == object) return visual;
    }
    return nullptr;
}

struct PixelExtent
{
    bool valid = false;
    uint32_t minX = 0;
    uint32_t minY = 0;
    uint32_t maxX = 0;
    uint32_t maxY = 0;
};

static inline PixelExtent extent(const Surface& surface, Color background)
{
    PixelExtent output;
    auto bg = pixel(background);
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] == bg) continue;
            if (!output.valid) {
                output = {true, x, y, x, y};
                continue;
            }
            if (x < output.minX) output.minX = x;
            if (y < output.minY) output.minY = y;
            if (x > output.maxX) output.maxX = x;
            if (y > output.maxY) output.maxY = y;
        }
    }
    return output;
}

static inline Space* _space()
{
    auto space = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    if (!space) return nullptr;
    space->style.stroke.a = 0;
    space->axisX.a = 0;
    space->axisY.a = 0;
    space->axisZ.a = 0;
    return space;
}

// Square the complex coordinate without changing its depth.
static inline bool _squared(const Vec3& point, Vec3& image, void* data) noexcept
{
    if (data) ++*static_cast<uint32_t*>(data);
    image = {point.x * point.x - point.y * point.y, 2.0f * point.x * point.y, point.z};
    return true;
}

// Lifts every point by the square of its x.
static inline bool _lifted(const Vec3& point, Vec3& image, void*) noexcept
{
    image = {point.x, point.y + point.x * point.x, point.z};
    return true;
}

// Moves every point by the vector data points at.
static inline bool _moved(const Vec3& point, Vec3& image, void* data) noexcept
{
    image = point + *static_cast<const Vec3*>(data);
    return true;
}

// The pixel of a frame at x and y, as a color.
static inline Color framePixel(const Surface& frame, uint32_t x, uint32_t y)
{
    auto value = frame.data()[static_cast<size_t>(y) * frame.stride() + x];
    return {static_cast<uint8_t>(value & 0xffu), static_cast<uint8_t>((value >> 8) & 0xffu),
            static_cast<uint8_t>((value >> 16) & 0xffu), static_cast<uint8_t>(value >> 24)};
}

// Whether the color channels of first and second differ by at most tolerance.
static inline bool close(Color first, Color second, int tolerance)
{
    return std::abs(first.r - second.r) <= tolerance && std::abs(first.g - second.g) <= tolerance
           && std::abs(first.b - second.b) <= tolerance;
}

// A 200 by 200 frame looking straight down at the origin, 40 pixels to a world unit.
static inline Config topView()
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.cameraView = CameraView::ThreeD;
    config.camera.eye = {0.0f, 0.0f, 10.0f};
    config.camera.target = {};
    config.camera.up = {0.0f, 1.0f, 0.0f};
    config.camera.projection = Projection::Orthographic;
    config.camera.orthoHeight = 5.0f;
    config.background = Color::hex("#101010");
    return config;
}

// The plane z = height over [-1, 1]^2 in one face of color, seen from above in pixels 60 to 140
// each way.
static inline SurfaceMesh* depthPlate(float height, Color color)
{
    Vec3 corners[] = {{-1.0f, -1.0f, height}, {1.0f, -1.0f, height}, {-1.0f, 1.0f, height}, {1.0f, 1.0f, height}};
    auto surface = SurfaceMesh::gen(corners, 2u, 2u);
    if (surface) {
        surface->shading = false;
        surface->fill(color);
    }
    return surface;
}

// What text shows at time, or "?" when it does not fit.
static inline const char* _shown(const Scene* scene, const Text* text, float time)
{
    static char output[64];
    return scene->text(text, time, output, sizeof(output)) < sizeof(output) ? output : "?";
}

// The opacity a black shape shows over the white background of a 320x180 frame, six units
// high, at a world point inside it.
static inline float _shownOpacity(SwRenderer* renderer, Scene* scene, float time, const Vec3& point)
{
    Surface surface;
    if (renderer->render(scene, time, surface) != Result::Success || !surface.data()) return -1.0f;
    auto x = static_cast<uint32_t>(160.0f + 30.0f * point.x);
    auto y = static_cast<uint32_t>(90.0f - 30.0f * point.y);
    auto red = surface.data()[static_cast<size_t>(y) * surface.stride() + x] & 0xffu;
    return 1.0f - static_cast<float>(red) / 255.0f;
}

// Pixels at least half dark on a white background.
static inline uint32_t _inked(SwRenderer* renderer, Scene* scene, float time)
{
    Surface surface;
    if (renderer->render(scene, time, surface) != Result::Success || !surface.data()) return 0u;
    auto count = 0u;
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            if ((surface.data()[static_cast<size_t>(y) * surface.stride() + x] & 0xffu) < 128u) count++;
        }
    }
    return count;
}

static inline uint8_t red(const Surface& surface, uint32_t x, uint32_t y)
{
    return static_cast<uint8_t>(surface.data()[static_cast<size_t>(y) * surface.stride() + x] & 0xffu);
}

static inline Mat4 _rotationAbout(float radians, const Vec3& pivot)
{
    return Mat4::translate(pivot) * Mat4::rotateZ(radians) * Mat4::translate(pivot * -1.0f);
}

static inline bool _says(const Scene* scene, const char* text)
{
    auto diagnostic = scene->diagnostic();
    return diagnostic && std::strstr(diagnostic, text);
}

static inline uint32_t _pixelAt(const Surface& surface, uint32_t x, uint32_t y)
{
    return surface.data() ? surface.data()[y * surface.stride() + x] : 0u;
}

// Whether the object's pixel bounds at time center on world point (x, y) and, when size is
// positive, measure size world units on each side, in the 200 by 200 scene of layeredMotions.
static inline bool _placed(SwRenderer* renderer, Scene* scene, Object* object, float time, float x, float y,
                           float size = 0.0f)
{
    BBox bounds;
    if (renderer->bounds(scene, object, time, bounds) != Result::Success) return false;
    auto center = bounds.center();
    if (!near(center.x, 100.0f + 20.0f * x, 1.0f) || !near(center.y, 100.0f - 20.0f * y, 1.0f)) {
        std::printf("  %.2f s: center (%.2f, %.2f) px, expected (%.2f, %.2f)\n", static_cast<double>(time),
                    static_cast<double>(center.x), static_cast<double>(center.y),
                    static_cast<double>(100.0f + 20.0f * x), static_cast<double>(100.0f - 20.0f * y));
        return false;
    }
    if (size <= 0.0f) return true;
    if (!near(bounds.width, 20.0f * size, 1.5f) || !near(bounds.height, 20.0f * size, 1.5f)) {
        std::printf("  %.2f s: size %.2f x %.2f px, expected %.2f\n", static_cast<double>(time),
                    static_cast<double>(bounds.width), static_cast<double>(bounds.height),
                    static_cast<double>(20.0f * size));
        return false;
    }
    return true;
}

// The pixels of color c inside the rectangle from (x0, y0) to (x1, y1), excluded: how many, and
// their mean column and row.
static inline uint32_t _viewPixels(const Surface& surface, Color c, uint32_t x0, uint32_t y0, uint32_t x1,
                                   uint32_t y1, float& column, float& row)
{
    auto count = 0u;
    auto sumX = 0.0;
    auto sumY = 0.0;
    for (auto y = y0; y < y1 && y < surface.height(); y++) {
        for (auto x = x0; x < x1 && x < surface.width(); x++) {
            if (_pixelAt(surface, x, y) != pixel(c)) continue;
            count++;
            sumX += x;
            sumY += y;
        }
    }
    column = count ? static_cast<float>(sumX / count) : -1.0f;
    row = count ? static_cast<float>(sumY / count) : -1.0f;
    return count;
}

void textAnimationRegression(SwRenderer* renderer);
void repeatedCreateRegression(SwRenderer* renderer);
void drawDirectionAnimations(SwRenderer* renderer);
void arrowCompositing(SwRenderer* renderer);
void directedRouteRendering(SwRenderer* renderer);
void fadeAnimations(SwRenderer* renderer);
void scaleAndIndicateAnimations(SwRenderer* renderer);
void emphasisEffects(SwRenderer* renderer);
void warpedGeometry(SwRenderer* renderer);
void spaceTicks(SwRenderer* renderer);
void spaceLabels();
void spaceScreenLabels(SwRenderer* renderer);
void spaceScreenLabelWarps(SwRenderer* renderer);
void spaceLabelOpacity(SwRenderer* renderer);
void cursorDecimals(SwRenderer* renderer);
void morphAnimations(SwRenderer* renderer);
void voxelSamples(SwRenderer* renderer);
void cellSamples();
void riemannSums();
void vectorFields();
void streamLines();
void sampleTime(SwRenderer* renderer);
void viewports(SwRenderer* renderer);
void liveSceneTransitions(SwRenderer* renderer);
void sceneTransitions(SwRenderer* renderer);
void transitionLayoutVisuals(SwRenderer* renderer);
void transitionStyles(SwRenderer* renderer);
void objectTree(SwRenderer* renderer);
void objectIds();
void offsetBoxes(SwRenderer* renderer);
void boundsAndLayout();
void sceneUpdate(SwRenderer* renderer);
void groupAndRectangleRendering(SwRenderer* renderer);
void connectorOutlines(SwRenderer* renderer);
void connectorAnimation(SwRenderer* renderer);
void animationTargets(SwRenderer* renderer);
void surfaceColors(SwRenderer* renderer);
void surfaceMeshLines(SwRenderer* renderer);
void surfaceMorph(SwRenderer* renderer);
void surfaceLight(SwRenderer* renderer);
void depthSplitStrokes(SwRenderer* renderer);
void depthSplitSpace(SwRenderer* renderer);
void depthSplitPlotPieces(SwRenderer* renderer);
void depthSplitSpaceTicks(SwRenderer* renderer);
void surfacePlaneTextAndWrite(SwRenderer* renderer);
void countingText(SwRenderer* renderer);
void textChanges(SwRenderer* renderer);
void matchedPieces(SwRenderer* renderer);
void diffedLines(SwRenderer* renderer);
void arrangedRows(SwRenderer* renderer);
void handedOver(SwRenderer* renderer);
void replacementContacts();
void matchedShapes(SwRenderer* renderer);
void morphedOutlines(SwRenderer* renderer);
void pieceMorphs(SwRenderer* renderer);
void contouredPaths(SwRenderer* renderer);
void contourPlots(SwRenderer* renderer);
void contourLabels(SwRenderer* renderer);
void contourStates(SwRenderer* renderer);
void plotEndpoints(SwRenderer* renderer);
void plotEndpointColor(SwRenderer* renderer);
void plotEndpointProgress(SwRenderer* renderer);
void plotPieces(SwRenderer* renderer);
void combinedShapes(SwRenderer* renderer);
void highlightedParts(SwRenderer* renderer);
void sceneThemes(SwRenderer* renderer);
void groupStyles(SwRenderer* renderer);
void timelineTracks(SwRenderer* renderer);
void clipLists(SwRenderer* renderer);
void sampleWork();
void cameraOrbit(SwRenderer* renderer);
void intentMotion(SwRenderer* renderer);
void timelineDiagnostics();
void arcsAndAngles(SwRenderer* renderer);
void mathSymbolFallback(SwRenderer* renderer);
void shapeProperties(SwRenderer* renderer);
void trimmedSegments(SwRenderer* renderer);
void zeroLengthArrows(SwRenderer* renderer);
void geometryClips(SwRenderer* renderer);
void plotFunctionPlay(SwRenderer* renderer);
void plotReplot(SwRenderer* renderer);
void cameraLayers(SwRenderer* renderer);
void layeredMotions(SwRenderer* renderer);
void layeredTransforms(SwRenderer* renderer);
void layeredMorphs(SwRenderer* renderer);
void instantUpdates(SwRenderer* renderer);
void clonedObjects(SwRenderer* renderer);
void clonedLinks(SwRenderer* renderer);
void matchedBillboard(SwRenderer* renderer);
void matchedPath(SwRenderer* renderer);
void copiedTransforms(SwRenderer* renderer);
void clonedWarps(SwRenderer* renderer);
void rebasedCopies(SwRenderer* renderer);
void geometryCopies(SwRenderer* renderer);
void drawLayers(SwRenderer* renderer);
void saveRestore(SwRenderer* renderer);
void viewRendering(SwRenderer* renderer);
void viewTimelines();

#endif
