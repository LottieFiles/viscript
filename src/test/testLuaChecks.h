#ifndef _VIS_TEST_LUA_CHECKS_H_
#define _VIS_TEST_LUA_CHECKS_H_

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "vis.h"

using namespace vis;

extern int _failures;

static inline void _check(bool condition, const char* expression, const char* file, int line)
{
    if (condition) return;
    std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", file, line, expression);
    _failures++;
}

#define CHECK(condition) _check((condition), #condition, __FILE__, __LINE__)

static inline uint32_t _changed(const Surface& surface, Color background)
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

static inline uint32_t _pixel(Color color)
{
    return static_cast<uint32_t>(color.a) << 24 | static_cast<uint32_t>(color.b) << 16
        | static_cast<uint32_t>(color.g) << 8 | color.r;
}

static inline bool _near(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 1.0e-4f;
}

static inline void _error(const char* source, const char* name, Lua::AssetResolver resolver = nullptr,
                   void* resolverData = nullptr)
{
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error,
                            sizeof(error), resolver, resolverData);
    CHECK(status == Result::ScriptError);
    CHECK(scene == nullptr);
    CHECK(error[0] != '\0');
    auto location = std::strstr(error, name);
    CHECK(location != nullptr);
    if (!location) std::fprintf(stderr, "  %s does not name its script: %s\n", name, error);
    if (location) CHECK(location[std::strlen(name)] == ':');
    delete scene;
}

// The script must fail and the message must explain why.
static inline void _scriptError(const char* source, const char* expected)
{
    char error[512] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), "diagnostic.lua", &scene,
                            error, sizeof(error));
    CHECK(status == Result::ScriptError);
    CHECK(std::strstr(error, expected) != nullptr);
    if (!std::strstr(error, expected)) std::fprintf(stderr, "  expected '%s', got: %s\n", expected, error);
    delete scene;
}

static inline bool _same(Color first, Color second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b && first.a == second.a;
}

// The texts of a spans group, in order.
static inline uint32_t _spanTexts(Scene* scene, const char* id, Text** texts, uint32_t capacity)
{
    auto row = scene->object(id);
    if (!row) return 0;
    auto count = row->childCount();
    for (auto i = 0u; i < count && i < capacity; i++) {
        auto child = row->childAt(i);
        texts[i] = child && child->type() == Type::Text ? static_cast<Text*>(child) : nullptr;
    }
    return count;
}

// The baseline of a text, and the ink of a text set alone in a size, in pixels.
static inline float _baselineOf(Scene* scene, const char* id)
{
    auto object = scene->object(id);
    CHECK(object && object->type() == Type::Text);
    return object && object->type() == Type::Text ? static_cast<Text*>(object)->point.y : 0.0f;
}

// The string a text shows at time, or "?" when it is longer than the buffer.
static inline const char* _shows(Scene* scene, const char* id, float time)
{
    static char output[64];
    auto text = static_cast<const Text*>(scene->object(id));
    if (!text || scene->text(text, time, output, sizeof(output)) >= sizeof(output)) return "?";
    return output;
}

static inline bool _sameColor(const Color& first, const Color& second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b && first.a == second.a;
}

static inline bool _near(const Vec3& value, float x, float y, float z, float tolerance = 1.0e-4f)
{
    return std::fabs(value.x - x) < tolerance && std::fabs(value.y - y) < tolerance
        && std::fabs(value.z - z) < tolerance;
}

void _crossFades();
void _textSizes();
void _groupStyles();
void _evaluating();
void _composition();
void _arcsAndAngles();
void _shapeProperties();
void _instantSets();
void _tableHelpers();
void _queries();
void _rowQueries();
void _measuring(SwRenderer* renderer);
void _scalingTexts(SwRenderer* renderer);
void _slanting(SwRenderer* renderer);
void _spanning();
void _spanLeading();
void _baselines();
void _counting(SwRenderer* renderer);
void _textChanges();
void _cloning(SwRenderer* renderer);
void _copyTransforms(SwRenderer* renderer);
void _matchingBillboard(SwRenderer* renderer);
void _matchingPath(SwRenderer* renderer);
void _matching(SwRenderer* renderer);
void _coding();
void _codeBaselines();
void _highlighting();
void _searching();
void _editing();
void _plotBreaks();
void _plotEndpoints(SwRenderer* renderer);
void _plotEndpointProgress(SwRenderer* renderer);
void _plotDiscontinuities();
void _spaceTicksAndLabels();
void _spaceScreenLabels(SwRenderer* renderer);
void _spaceLabelIds();
void _areas();
void _riemannSums();
void _surfaceColors();
void _contours();
void _contourLabels(SwRenderer* renderer);
void _contourBindings(SwRenderer* renderer);
void _vectorFields();
void _streamLines();
void _shapeMorphs();
void _contourPaths();
void _regions();
void _arranging();
void _advecting();
void _emphasizing(SwRenderer* renderer);
void _framingTexts(SwRenderer* renderer);
void _surrounding();
void _boundsQueries();
void _bracing(SwRenderer* renderer);
void _matrices();
void _tables();
void _viewports(SwRenderer* renderer);
void _sceneTransitions(SwRenderer* renderer);
void _transitionStyles(SwRenderer* renderer);
void _sceneTree(SwRenderer* renderer);
void _animationMethods(SwRenderer* renderer);
void _dashTimeline(SwRenderer* renderer);
void _themes(SwRenderer* renderer);
void _cellVoxelBindings(SwRenderer* renderer);
void _gradients();
void _functionPlay(SwRenderer* renderer);
void _animatedPlotGaps(SwRenderer* renderer);
void _replotting(SwRenderer* renderer);
void _trimmedSegments(SwRenderer* renderer);
void _boundGeometry(SwRenderer* renderer);
void _overlappingTracks(SwRenderer* renderer);
void _pendingReads();
void _longTracks(SwRenderer* renderer);
void _springResponses(SwRenderer* renderer);
void _trackerPrecision();
void _trackerTiming();
void _trackers();
void _trackerLines();
void _timeline();
void _cameraCenters();
void _cameraHome();
void _views(SwRenderer* renderer);
void _motion(SwRenderer* renderer);
void _layering(SwRenderer* renderer);
void _pieceMorphs(SwRenderer* renderer);
void _surfaceMorph(SwRenderer* renderer);
void _savingStates(SwRenderer* renderer);
void _warping(SwRenderer* renderer);
void _warpedClones(SwRenderer* renderer);

#endif
