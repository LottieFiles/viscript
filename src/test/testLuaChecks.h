#ifndef _VIS_TEST_LUA_CHECKS_H_
#define _VIS_TEST_LUA_CHECKS_H_

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "vis.h"
#include "testCheck.h"

using namespace vis;

static inline bool _near(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 1.0e-4f;
}

// The scene a script makes. A script that does not load fails the check and says why.
static inline Scene* _loaded(const char* source, const char* name, Lua::AssetResolver resolver = nullptr,
                             void* resolverData = nullptr)
{
    char error[1024] = {};
    Scene* scene = nullptr;
    auto status = Lua::load(source, static_cast<uint32_t>(std::strlen(source)), name, &scene, error, sizeof(error),
                            resolver, resolverData);
    CHECK(status == Result::Success && scene);
    if (status != Result::Success) std::fprintf(stderr, "%s: %s\n", name, error);
    return scene;
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

// The script must load.
static inline void _scriptLoads(const char* source)
{
    delete _loaded(source, "loads.lua");
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
void _measuring(Renderer* renderer);
void _scalingTexts(Renderer* renderer);
void _slanting(Renderer* renderer);
void _spanning();
void _spanLeading();
void _baselines();
void _counting(Renderer* renderer);
void _textChanges();
void _cloning(Renderer* renderer);
void _copyTransforms(Renderer* renderer);
void _matchingBillboard(Renderer* renderer);
void _matchingPath(Renderer* renderer);
void _matching(Renderer* renderer);
void _coding();
void _codeBaselines();
void _highlighting();
void _searching();
void _editing();
void _plotBreaks();
void _plotEndpoints(Renderer* renderer);
void _plotEndpointProgress(Renderer* renderer);
void _plotDiscontinuities();
void _spaceTicksAndLabels();
void _spaceScreenLabels(Renderer* renderer);
void _spaceLabelIds();
void _areas();
void _riemannSums();
void _surfaceColors();
void _contours();
void _contourLabels(Renderer* renderer);
void _contourBindings(Renderer* renderer);
void _vectorFields();
void _streamLines();
void _shapeMorphs();
void _contourPaths();
void _regions();
void _arranging();
void _advecting();
void _emphasizing(Renderer* renderer);
void _framingTexts(Renderer* renderer);
void _surrounding();
void _boundsQueries();
void _bracing(Renderer* renderer);
void _matrices();
void _tables();
void _viewports(Renderer* renderer);
void _sceneTransitions(Renderer* renderer);
void _transitionStyles(Renderer* renderer);
void _sceneTree(Renderer* renderer);
void _animationMethods(Renderer* renderer);
void _dashTimeline(Renderer* renderer);
void _themes(Renderer* renderer);
void _cellVoxelBindings(Renderer* renderer);
void _gradients();
void _functionPlay(Renderer* renderer);
void _animatedPlotGaps(Renderer* renderer);
void _replotting(Renderer* renderer);
void _trimmedSegments(Renderer* renderer);
void _boundGeometry(Renderer* renderer);
void _overlappingTracks(Renderer* renderer);
void _pendingReads();
void _longTracks(Renderer* renderer);
void _springResponses(Renderer* renderer);
void _trackerPrecision();
void _trackerTiming();
void _trackers();
void _trackerLines();
void _timeline();
void _cameraCenters();
void _cameraHome();
void _views(Renderer* renderer);
void _motion(Renderer* renderer);
void _layering(Renderer* renderer);
void _pieceMorphs(Renderer* renderer);
void _surfaceMorph(Renderer* renderer);
void _savingStates(Renderer* renderer);
void _warping(Renderer* renderer);
void _warpedClones(Renderer* renderer);

#endif
