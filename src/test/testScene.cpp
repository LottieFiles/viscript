#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <type_traits>
#include <vector>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "vis.h"

using namespace vis;

static_assert(!std::is_copy_constructible<Object>::value, "Object owns its tag");
static_assert(!std::is_copy_constructible<Scene>::value, "Scene owns its object graph");
static_assert(!std::is_copy_constructible<SwRenderer>::value, "SwRenderer owns a canvas");
static_assert(!std::is_copy_constructible<GlRenderer>::value, "GlRenderer owns a canvas");
static_assert(!std::is_constructible<Object, Type>::value, "Object hierarchy is closed");

static int failures = 0;

struct RuntimeProbe
{
    uint32_t objects = 0;
    uint32_t cameras = 0;
    uint32_t inputs = 0;
    bool invalidateObject = false;
    bool invalidateCamera = false;
};

static Result runtimeObject(const Object*, float, Mat4&, float& opacity, float&,
                            void* data) noexcept
{
    auto probe = static_cast<RuntimeProbe*>(data);
    probe->objects++;
    if (probe->invalidateObject) opacity = std::numeric_limits<float>::quiet_NaN();
    return Result::Success;
}

static Result runtimeCamera(float, Camera& camera, CameraView&, void* data) noexcept
{
    auto probe = static_cast<RuntimeProbe*>(data);
    probe->cameras++;
    if (probe->invalidateCamera) camera.near = -1.0f;
    return Result::Success;
}

static Result runtimeInput(const CameraInput&, void* data) noexcept
{
    static_cast<RuntimeProbe*>(data)->inputs++;
    return Result::Success;
}

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                                        \
        }                                                                                      \
    } while (false)

static uint32_t changed(const Surface& surface, Color background)
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

static uint64_t checksum(const Surface& surface)
{
    auto hash = 14695981039346656037ull;
    for (auto i = 0u; i < surface.width() * surface.height(); i++) {
        hash ^= surface.data()[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static uint32_t pixel(Color color)
{
    return static_cast<uint32_t>(color.a) << 24 | static_cast<uint32_t>(color.b) << 16
        | static_cast<uint32_t>(color.g) << 8 | color.r;
}

static bool same(Color first, Color second)
{
    return first.r == second.r && first.g == second.g && first.b == second.b
           && first.a == second.a;
}

static bool near(float lhs, float rhs, float epsilon = 1.0e-4f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

static const LayoutVisual* _layoutVisual(const LayoutReport& report,
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

static void renderBounds(SwRenderer* renderer)
{
    BBox first = {10.0f, 10.0f, 20.0f, 20.0f};
    BBox second = {35.0f, 10.0f, 20.0f, 20.0f};
    CHECK(near(first.center().x, 20.0f) && near(first.center().y, 20.0f));
    CHECK(near(first.size().x, 20.0f) && near(first.size().y, 20.0f));
    CHECK(!first.intersects(second));
    CHECK(first.intersects(second, 6.0f));
    CHECK(!first.intersects(second, -1.0f));

    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto left = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto overlap = Rectangle::gen({-0.25f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto right = Rectangle::gen({0.2f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto text = Text::gen("layout", {0.0f, 1.0f, 0.0f});
    CHECK(scene && left && overlap && right && text);
    if (scene && left && overlap && right && text) {
        left->fill(Color::hex("#ffffff"));
        overlap->fill(Color::hex("#ffffff"));
        right->fill(Color::hex("#ffffff"));
        text->fill(Color::hex("#ffffff"));
        text->size = 20.0f;
        CHECK(text->font("Pretendard") == Result::Success);
        CHECK(scene->add(left) == Result::Success);
        CHECK(scene->add(overlap) == Result::Success);
        CHECK(scene->add(right) == Result::Success);
        CHECK(scene->add(text) == Result::Success);
        BBox leftBounds;
        BBox textBounds;
        CHECK(renderer->bounds(scene, left, 0.0f, leftBounds) == Result::Success);
        CHECK(renderer->bounds(scene, text, 0.0f, textBounds) == Result::Success);
        CHECK(leftBounds.width > 20.0f && leftBounds.height > 20.0f);
        CHECK(textBounds.width > 20.0f && textBounds.height > 10.0f);
        auto intersects = false;
        CHECK(renderer->intersects(scene, left, overlap, 0.0f, intersects) == Result::Success);
        CHECK(intersects);
        CHECK(renderer->intersects(scene, left, right, 0.0f, intersects) == Result::Success);
        CHECK(!intersects);
        CHECK(renderer->intersects(scene, left, right, 0.0f, intersects, 6.0f) == Result::Success);
        CHECK(intersects);
        CHECK(renderer->intersects(scene, left, left, 0.0f, intersects) == Result::InvalidArguments);
    } else {
        delete left;
        delete overlap;
        delete right;
        delete text;
    }
    delete scene;

    Config rootConfig;
    rootConfig.width = 200;
    rootConfig.height = 100;
    rootConfig.camera.orthoHeight = 2.0f;
    Config childConfig;
    childConfig.width = 100;
    childConfig.height = 100;
    childConfig.camera.orthoHeight = 2.0f;
    auto root = Scene::gen(rootConfig);
    auto child = Scene::gen(childConfig);
    auto rootBox = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto childBox = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto panel = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto clippedText = Text::gen("overflow", {0.8f, 0.0f, 0.0f});
    CHECK(root && child && rootBox && childBox && panel && clippedText);
    if (root && child && rootBox && childBox && panel && clippedText) {
        rootBox->tag("root-box").fill(Color::hex("#ffffff"));
        childBox->tag("child-box").fill(Color::hex("#ffffff"));
        childBox->stroke(Color::hex("#ffffff"), 4.0f);
        panel->tag("panel").fill(Color::hex("#ffffff"));
        clippedText->tag("clipped-text").fill(Color::hex("#ffffff"));
        clippedText->size = 16.0f;
        CHECK(clippedText->font("Pretendard") == Result::Success);
        CHECK(root->add(rootBox) == Result::Success);
        CHECK(child->add(childBox) == Result::Success);
        CHECK(panel->add(clippedText) == Result::Success);
        CHECK(child->add(panel) == Result::Success);
        CHECK(root->viewport(child, {0.5f, 0.0f, 0.5f, 1.0f}) == Result::Success);

        LayoutReport report;
        CHECK(renderer->layout(root, 0.0f, report, 4.0f) == Result::Success);
        CHECK(near(report.time(), 0.0f) && near(report.padding(), 4.0f));
        CHECK(report.sceneCount() == 2u && report.objectCount() == 4u);
        auto rootLayout = report.sceneAt(0);
        auto childLayout = report.sceneAt(1);
        CHECK(rootLayout && childLayout);
        if (rootLayout && childLayout) {
            CHECK(std::strcmp(rootLayout->path, "root") == 0);
            CHECK(std::strcmp(childLayout->path, "root/viewport:0") == 0);
            CHECK(childLayout->parent == 0u && childLayout->depth == 1u);
            CHECK(childLayout->kind == LayoutSceneKind::Viewport);
            CHECK(!childLayout->stretched);
            CHECK(near(childLayout->bounds.x, 100.0f));
            CHECK(near(childLayout->bounds.width, 100.0f));
        }

        uint32_t rootIndex = UINT32_MAX;
        uint32_t childIndex = UINT32_MAX;
        uint32_t panelIndex = UINT32_MAX;
        uint32_t clippedIndex = UINT32_MAX;
        for (auto i = 0u; i < report.objectCount(); i++) {
            auto item = report.objectAt(i);
            auto tag = item && item->object ? item->object->tag() : nullptr;
            if (!tag) continue;
            if (std::strcmp(tag, "root-box") == 0) rootIndex = i;
            else if (std::strcmp(tag, "child-box") == 0) childIndex = i;
            else if (std::strcmp(tag, "panel") == 0) panelIndex = i;
            else if (std::strcmp(tag, "clipped-text") == 0) clippedIndex = i;
        }
        CHECK(rootIndex != UINT32_MAX && childIndex != UINT32_MAX && panelIndex != UINT32_MAX
              && clippedIndex != UINT32_MAX);
        const LayoutVisual* rootVisual = nullptr;
        const LayoutVisual* childVisual = nullptr;
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            if (!visual) continue;
            if (visual->object == rootIndex) rootVisual = visual;
            else if (visual->object == childIndex) childVisual = visual;
        }
        CHECK(rootVisual && rootVisual->visible && rootVisual->occluded
              && rootVisual->occluder == 1u);
        CHECK(childVisual && childVisual->visible && !childVisual->occluded
              && childVisual->occluder == UINT32_MAX);
        CHECK(report.pathCount() > 0u && report.pathAt(report.pathCount()) == nullptr);
        const LayoutPath* childPath = nullptr;
        for (auto i = 0u; i < report.pathCount(); i++) {
            auto path = report.pathAt(i);
            CHECK(path && path->visual < report.visualCount());
            if (path && report.visualAt(path->visual) == childVisual) childPath = path;
        }
        CHECK(childPath && childPath->count >= 4u && childPath->closed
              && childPath->filled && childPath->stroked
              && near(childPath->strokeWidth, 4.0f));
        if (childPath) {
            auto left = 200.0f;
            auto right = 0.0f;
            for (auto i = 0u; i < childPath->count; i++) {
                CHECK(childPath->points[i].x >= 100.0f
                      && childPath->points[i].x <= 200.0f);
                left = std::fmin(left, childPath->points[i].x);
                right = std::fmax(right, childPath->points[i].x);
            }
            CHECK(childPath->paintBounds.x < left);
            CHECK(childPath->paintBounds.x + childPath->paintBounds.width > right);
        }
        if (rootIndex != UINT32_MAX) {
            auto item = report.objectAt(rootIndex);
            CHECK(item && item->visible);
        }
        if (clippedIndex != UINT32_MAX) {
            auto item = report.objectAt(clippedIndex);
            CHECK(item->visible && item->clipped);
            CHECK(item->paintBounds.x + item->paintBounds.width > 200.0f);
            CHECK(item->visibleBounds.x + item->visibleBounds.width <= 200.0f);
            CHECK(item->parent == panelIndex);
        }
        auto crossSceneCollision = false;
        for (auto i = 0u; i < report.collisionCount(); i++) {
            auto collision = report.collisionAt(i);
            if (!collision) continue;
            if ((collision->first == rootIndex && collision->second == childIndex)
                || (collision->first == childIndex && collision->second == rootIndex)) {
                crossSceneCollision = collision->overlapping;
                CHECK(std::fabs(collision->separation.x) > 0.0f
                      || std::fabs(collision->separation.y) > 0.0f);
            }
        }
        CHECK(!crossSceneCollision);
        auto parentChildCompared = false;
        for (auto i = 0u; i < report.containmentCount(); i++) {
            auto containment = report.containmentAt(i);
            if (!containment || containment->container != panelIndex
                || containment->content != clippedIndex) {
                continue;
            }
            parentChildCompared = true;
            CHECK(containment->depth == 1u);
            CHECK(!containment->contained);
            CHECK(containment->overflowRight > 0.0f);
        }
        CHECK(parentChildCompared);
        CHECK(renderer->layout(root, -1.0f, report) == Result::InvalidArguments);
    } else {
        delete rootBox;
        delete childBox;
        delete panel;
        delete clippedText;
        delete child;
    }
    delete root;
}

static void layoutOcclusion(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 2.0f;
    config.background = Color::hex("#101820");

    auto transparentRoot = Scene::gen(config);
    auto transparentConfig = config;
    transparentConfig.background.a = 0u;
    auto transparentChild = Scene::gen(transparentConfig);
    auto transparentRootBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto transparentChildBox = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(transparentRoot && transparentChild && transparentRootBox
          && transparentChildBox);
    if (transparentRoot && transparentChild && transparentRootBox
        && transparentChildBox) {
        transparentRootBox->fill(Color::hex("#ffffff"));
        transparentChildBox->fill(Color::hex("#ffffff"));
        transparentRootBox->style.stroke.a = 0u;
        transparentChildBox->style.stroke.a = 0u;
        CHECK(transparentRoot->add(transparentRootBox) == Result::Success);
        CHECK(transparentChild->add(transparentChildBox) == Result::Success);
        CHECK(transparentRoot->viewport(transparentChild, {}) == Result::Success);
        LayoutReport report;
        CHECK(renderer->layout(transparentRoot, 0.0f, report) == Result::Success);
        auto rootVisual = _layoutVisual(report, transparentRootBox);
        auto childVisual = _layoutVisual(report, transparentChildBox);
        CHECK(rootVisual && rootVisual->visible && !rootVisual->occluded
              && rootVisual->occluder == UINT32_MAX);
        CHECK(childVisual && childVisual->visible && !childVisual->occluded
              && childVisual->occluder == UINT32_MAX);
        CHECK(report.collisionCount() == 1u);
        auto collision = report.collisionAt(0u);
        CHECK(collision && collision->overlapping);
    } else {
        delete transparentChild;
        delete transparentRootBox;
        delete transparentChildBox;
    }
    delete transparentRoot;

    auto partialRoot = Scene::gen(config);
    auto partialChild = Scene::gen(config);
    auto partialRootBox = Rectangle::gen({}, {2.0f, 1.0f});
    auto partialChildBox = Rectangle::gen({-1.0f, 0.0f, 0.0f},
                                          {2.0f, 1.0f});
    CHECK(partialRoot && partialChild && partialRootBox && partialChildBox);
    if (partialRoot && partialChild && partialRootBox && partialChildBox) {
        partialRootBox->fill(Color::hex("#ffffff"));
        partialChildBox->fill(Color::hex("#ffffff"));
        partialRootBox->style.stroke.a = 0u;
        partialChildBox->style.stroke.a = 0u;
        CHECK(partialRoot->add(partialRootBox) == Result::Success);
        CHECK(partialChild->add(partialChildBox) == Result::Success);
        CHECK(partialRoot->viewport(partialChild,
                                    {0.5f, 0.0f, 0.5f, 1.0f}) == Result::Success);
        LayoutReport report;
        CHECK(renderer->layout(partialRoot, 0.0f, report) == Result::Success);
        auto rootVisual = _layoutVisual(report, partialRootBox);
        auto childVisual = _layoutVisual(report, partialChildBox);
        CHECK(rootVisual && rootVisual->visible && !rootVisual->occluded
              && rootVisual->occluder == UINT32_MAX);
        CHECK(childVisual && childVisual->visible && !childVisual->occluded
              && childVisual->occluder == UINT32_MAX);
        CHECK(report.collisionCount() == 1u);
    } else {
        delete partialChild;
        delete partialRootBox;
        delete partialChildBox;
    }
    delete partialRoot;

    auto siblingRoot = Scene::gen(config);
    auto first = Scene::gen(config);
    auto second = Scene::gen(config);
    auto siblingRootBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto firstBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto secondBox = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(siblingRoot && first && second && siblingRootBox && firstBox
          && secondBox);
    if (siblingRoot && first && second && siblingRootBox && firstBox
        && secondBox) {
        siblingRootBox->fill(Color::hex("#ffffff"));
        firstBox->fill(Color::hex("#ffffff"));
        secondBox->fill(Color::hex("#ffffff"));
        siblingRootBox->style.stroke.a = 0u;
        firstBox->style.stroke.a = 0u;
        secondBox->style.stroke.a = 0u;
        CHECK(siblingRoot->add(siblingRootBox) == Result::Success);
        CHECK(first->add(firstBox) == Result::Success);
        CHECK(second->add(secondBox) == Result::Success);
        CHECK(siblingRoot->viewport(first, {}) == Result::Success);
        CHECK(siblingRoot->viewport(second, {}) == Result::Success);
        LayoutReport report;
        CHECK(renderer->layout(siblingRoot, 0.0f, report) == Result::Success);
        auto rootVisual = _layoutVisual(report, siblingRootBox);
        auto firstVisual = _layoutVisual(report, firstBox);
        auto secondVisual = _layoutVisual(report, secondBox);
        CHECK(rootVisual && rootVisual->visible && rootVisual->occluded
              && rootVisual->occluder == 1u);
        CHECK(firstVisual && firstVisual->visible && firstVisual->occluded
              && firstVisual->occluder == 2u);
        CHECK(secondVisual && secondVisual->visible && !secondVisual->occluded
              && secondVisual->occluder == UINT32_MAX);
        CHECK(report.collisionCount() == 0u);
    } else {
        delete first;
        delete second;
        delete siblingRootBox;
        delete firstBox;
        delete secondBox;
    }
    delete siblingRoot;

    auto nestedRoot = Scene::gen(config);
    auto middle = Scene::gen(config);
    auto leaf = Scene::gen(config);
    auto nestedRootBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto middleBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto leafBox = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(nestedRoot && middle && leaf && nestedRootBox && middleBox && leafBox);
    if (nestedRoot && middle && leaf && nestedRootBox && middleBox && leafBox) {
        nestedRootBox->fill(Color::hex("#ffffff"));
        middleBox->fill(Color::hex("#ffffff"));
        leafBox->fill(Color::hex("#ffffff"));
        nestedRootBox->style.stroke.a = 0u;
        middleBox->style.stroke.a = 0u;
        leafBox->style.stroke.a = 0u;
        CHECK(nestedRoot->add(nestedRootBox) == Result::Success);
        CHECK(middle->add(middleBox) == Result::Success);
        CHECK(leaf->add(leafBox) == Result::Success);
        CHECK(middle->viewport(leaf, {}) == Result::Success);
        CHECK(nestedRoot->viewport(middle, {}) == Result::Success);
        LayoutReport report;
        CHECK(renderer->layout(nestedRoot, 0.0f, report) == Result::Success);
        auto rootVisual = _layoutVisual(report, nestedRootBox);
        auto middleVisual = _layoutVisual(report, middleBox);
        auto leafVisual = _layoutVisual(report, leafBox);
        CHECK(rootVisual && rootVisual->visible && rootVisual->occluded
              && rootVisual->occluder == 1u);
        CHECK(middleVisual && middleVisual->visible && middleVisual->occluded
              && middleVisual->occluder == 2u);
        CHECK(leafVisual && leafVisual->visible && !leafVisual->occluded
              && leafVisual->occluder == UINT32_MAX);
        CHECK(report.collisionCount() == 0u);
    } else {
        delete middle;
        delete leaf;
        delete nestedRootBox;
        delete middleBox;
        delete leafBox;
    }
    delete nestedRoot;

    auto transitionRoot = Scene::gen(config);
    auto from = Scene::gen(config);
    auto to = Scene::gen(config);
    auto cover = Scene::gen(config);
    auto transitionRootBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto fromBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto toBox = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(transitionRoot && from && to && cover && transitionRootBox
          && fromBox && toBox);
    if (transitionRoot && from && to && cover && transitionRootBox
        && fromBox && toBox) {
        transitionRootBox->tag("root").fill(Color::hex("#ffffff"));
        fromBox->tag("stage").fill(Color::hex("#ffffff"));
        toBox->tag("stage").fill(Color::hex("#ffffff"));
        transitionRootBox->style.stroke.a = 0u;
        fromBox->style.stroke.a = 0u;
        toBox->style.stroke.a = 0u;
        CHECK(transitionRoot->add(transitionRootBox) == Result::Success);
        CHECK(from->add(fromBox) == Result::Success);
        CHECK(to->add(toBox) == Result::Success);
        CHECK(from->viewport(cover, {}) == Result::Success);
        Scene* stages[] = {from, to};
        CHECK(transitionRoot->transition(stages, 2u, {}, 0.6f, 0.2f,
                                         Easing::Linear) == Result::Success);
        LayoutReport report;
        CHECK(renderer->layout(transitionRoot, 0.1f, report) == Result::Success);
        auto rootVisual = _layoutVisual(report, transitionRootBox);
        auto stageVisual = _layoutVisual(report, fromBox);
        CHECK(rootVisual && rootVisual->visible && rootVisual->occluded
              && rootVisual->occluder == 1u);
        CHECK(stageVisual && stageVisual->visible && stageVisual->occluded
              && stageVisual->occluder == 2u);

        CHECK(renderer->layout(transitionRoot, 0.5f, report) == Result::Success);
        rootVisual = _layoutVisual(report, transitionRootBox);
        stageVisual = _layoutVisual(report, fromBox);
        CHECK(rootVisual && rootVisual->visible && rootVisual->occluded
              && rootVisual->occluder == 1u);
        CHECK(stageVisual && stageVisual->visible && !stageVisual->occluded
              && stageVisual->occluder == UINT32_MAX
              && stageVisual->kind == LayoutVisualKind::Morph);
    } else {
        delete from;
        delete to;
        delete cover;
        delete transitionRootBox;
        delete fromBox;
        delete toBox;
    }
    delete transitionRoot;

    auto painterConfig = config;
    painterConfig.background.a = 0u;
    auto painterRoot = Scene::gen(painterConfig);
    auto painterFrom = Scene::gen(painterConfig);
    auto painterTo = Scene::gen(painterConfig);
    auto sequenceFirst = Scene::gen(painterConfig);
    auto sequenceTop = Scene::gen(config);
    auto targetViewport = Scene::gen(painterConfig);
    auto sequenceBox = Rectangle::gen({}, {1.0f, 1.0f});
    auto targetBox = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(painterRoot && painterFrom && painterTo && sequenceFirst && sequenceTop
          && targetViewport && sequenceBox && targetBox);
    if (painterRoot && painterFrom && painterTo && sequenceFirst && sequenceTop
        && targetViewport && sequenceBox && targetBox) {
        sequenceBox->fill(Color::hex("#ffffff"));
        targetBox->fill(Color::hex("#ffffff"));
        sequenceBox->style.stroke.a = 0u;
        targetBox->style.stroke.a = 0u;
        CHECK(sequenceTop->add(sequenceBox) == Result::Success);
        CHECK(targetViewport->add(targetBox) == Result::Success);
        Scene* nestedStages[] = {sequenceFirst, sequenceTop};
        CHECK(painterFrom->transition(nestedStages, 2u, {}, 0.1f, 0.0f,
                                      Easing::Linear) == Result::Success);
        CHECK(painterTo->viewport(targetViewport, {}) == Result::Success);
        Scene* stages[] = {painterFrom, painterTo};
        CHECK(painterRoot->transition(stages, 2u, {}, 1.0f, 0.0f,
                                      AnimCurve::preset(AnimCurvePreset::Linear), nullptr, 0u, false) == Result::Success);

        LayoutReport report;
        CHECK(renderer->layout(painterRoot, 0.5f / 255.0f, report)
              == Result::Success);
        CHECK(report.sceneCount() == 5u);
        if (report.sceneCount() == 5u) {
            CHECK(std::strcmp(report.sceneAt(3u)->path,
                              "root/transition:1/viewport:0") == 0);
            CHECK(std::strcmp(report.sceneAt(4u)->path,
                              "root/transition:0/transition:1") == 0);
        }
        auto targetVisual = _layoutVisual(report, targetBox);
        auto sequenceVisual = _layoutVisual(report, sequenceBox);
        CHECK(targetVisual && targetVisual->visible && targetVisual->occluded
              && targetVisual->occluder == 4u
              && near(targetVisual->opacity, 1.0f / 255.0f));
        CHECK(sequenceVisual && sequenceVisual->visible
              && !sequenceVisual->occluded
              && sequenceVisual->occluder == UINT32_MAX);
        CHECK(report.collisionCount() == 0u);
    } else {
        delete painterFrom;
        delete painterTo;
        delete sequenceFirst;
        delete sequenceTop;
        delete targetViewport;
        delete sequenceBox;
        delete targetBox;
    }
    delete painterRoot;
}

static uint32_t strokePixels(SwRenderer* renderer, const float* pattern, uint32_t count)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto line = Line::gen({-3.0f, 0.0f}, {3.0f, 0.0f});
    CHECK(scene && line);
    if (!scene || !line) {
        delete scene;
        delete line;
        return 0u;
    }
    line->stroke(Color::hex("#202124"), 2.0f);
    CHECK(line->dash(pattern, count, 2.0f) == Result::Success);
    CHECK(scene->add(line) == Result::Success);
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    auto pixels = changed(surface, config.background);
    delete scene;
    return pixels;
}

static void dashedStrokes(SwRenderer* renderer)
{
    auto line = Line::gen({}, {1.0f, 0.0f});
    CHECK(line != nullptr);
    if (line) {
        float valid[] = {8.0f, 4.0f};
        float zero[] = {0.0f, 0.0f};
        float negative[] = {4.0f, -1.0f};
        float tooMany[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        CHECK(line->dash(valid, 2u, 2.0f) == Result::Success);
        CHECK(line->style.dashCount == 2u);
        CHECK(near(line->style.dashOffset, 2.0f));
        CHECK(line->dash(zero, 2u) == Result::InvalidArguments);
        CHECK(line->dash(negative, 2u) == Result::InvalidArguments);
        CHECK(line->dash(tooMany, 5u) == Result::InvalidArguments);
        CHECK(line->dash(nullptr, 1u) == Result::InvalidArguments);
        CHECK(line->dash(nullptr, 0u) == Result::Success);
    }
    delete line;

    float dashed[] = {8.0f, 8.0f};
    auto solidPixels = strokePixels(renderer, nullptr, 0u);
    auto dashedPixels = strokePixels(renderer, dashed, 2u);
    CHECK(solidPixels > 100u);
    CHECK(dashedPixels > solidPixels / 4u);
    CHECK(dashedPixels < solidPixels * 3u / 4u);
}

struct PixelExtent
{
    bool valid = false;
    uint32_t minX = 0;
    uint32_t minY = 0;
    uint32_t maxX = 0;
    uint32_t maxY = 0;
};

static PixelExtent extent(const Surface& surface, Color background)
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

static Space* _space()
{
    auto space = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    if (!space) return nullptr;
    space->style.stroke.a = 0;
    space->axisX.a = 0;
    space->axisY.a = 0;
    space->axisZ.a = 0;
    return space;
}

struct PngReader
{
    const uint8_t* data;
    size_t size;
    size_t bit = 0;
    bool valid = true;

    uint32_t read(uint32_t count)
    {
        if (bit + count > size * 8) {
            valid = false;
            return 0;
        }
        auto value = 0u;
        for (auto i = 0u; i < count; i++, bit++) value |= ((data[bit / 8] >> (bit % 8)) & 1u) << i;
        return value;
    }
};

struct PngTree
{
    uint32_t counts[16] = {};
    uint32_t offsets[16] = {};
    uint16_t symbols[288] = {};

    PngTree(const uint8_t* lengths, uint32_t size)
    {
        for (auto i = 0u; i < size; i++) {
            if (lengths[i]) counts[lengths[i]]++;
        }
        for (auto i = 1u; i < 16; i++) offsets[i] = offsets[i - 1] + counts[i - 1];
        uint32_t next[16];
        std::memcpy(next, offsets, sizeof(next));
        for (auto i = 0u; i < size; i++) {
            if (lengths[i]) symbols[next[lengths[i]]++] = static_cast<uint16_t>(i);
        }
    }

    uint32_t read(PngReader& reader) const
    {
        auto code = 0u;
        auto first = 0u;
        for (auto length = 1u; length < 16; length++) {
            code = (code << 1) | reader.read(1);
            if (code >= first && code - first < counts[length]) return symbols[offsets[length] + code - first];
            first = (first + counts[length]) << 1;
        }
        reader.valid = false;
        return UINT32_MAX;
    }
};

static std::vector<uint8_t> pngBytes(const char* path)
{
    std::vector<uint8_t> bytes;
    auto file = std::fopen(path, "rb");
    if (!file) return bytes;
    if (std::fseek(file, 0, SEEK_END) == 0) {
        auto size = std::ftell(file);
        if (size > 0 && std::fseek(file, 0, SEEK_SET) == 0) {
            bytes.resize(static_cast<size_t>(size));
            if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) bytes.clear();
        }
    }
    std::fclose(file);
    return bytes;
}

static uint32_t pngInteger(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) << 24 | static_cast<uint32_t>(bytes[1]) << 16
           | static_cast<uint32_t>(bytes[2]) << 8 | bytes[3];
}

// Decode compressed PNG data independently of the saver, including overlapping LZ77 copies.
static bool pngPixels(const std::vector<uint8_t>& bytes, const Surface& surface)
{
    if (bytes.size() < 33 || pngInteger(bytes.data() + 16) != surface.width()
        || pngInteger(bytes.data() + 20) != surface.height() || bytes[24] != 8 || bytes[25] != 6) return false;
    std::vector<uint8_t> stream;
    for (size_t offset = 8; offset + 12 <= bytes.size();) {
        auto size = pngInteger(bytes.data() + offset);
        if (size > bytes.size() - offset - 12) return false;
        if (std::memcmp(bytes.data() + offset + 4, "IDAT", 4) == 0)
            stream.insert(stream.end(), bytes.begin() + offset + 8, bytes.begin() + offset + 8 + size);
        offset += size + 12;
    }
    if (stream.size() < 6 || stream[0] != 0x78 || (stream[0] * 256u + stream[1]) % 31 != 0) return false;
    PngReader reader = {stream.data() + 2, stream.size() - 6};
    std::vector<uint8_t> raw;
    auto stride = static_cast<size_t>(surface.width()) * 4 + 1;
    auto expected = stride * surface.height();
    auto final = false;
    while (!final && reader.valid) {
        final = reader.read(1) != 0;
        auto type = reader.read(2);
        uint8_t lengths[320] = {};
        auto literals = 288u;
        auto distances = 32u;
        if (type == 1) {
            for (auto i = 0u; i < 288; i++) lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
            for (auto i = 288u; i < 320; i++) lengths[i] = 5;
        } else if (type == 2) {
            literals = reader.read(5) + 257;
            distances = reader.read(5) + 1;
            auto count = reader.read(4) + 4;
            static constexpr uint8_t order[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            uint8_t codeLengths[19] = {};
            for (auto i = 0u; i < count; i++) codeLengths[order[i]] = static_cast<uint8_t>(reader.read(3));
            PngTree tree(codeLengths, 19);
            for (auto i = 0u; i < literals + distances;) {
                auto value = tree.read(reader);
                if (!reader.valid) return false;
                if (value < 16) lengths[i++] = static_cast<uint8_t>(value);
                else {
                    if (value == 16 && !i) return false;
                    auto repeat = reader.read(value == 16 ? 2 : value == 17 ? 3 : 7) + (value == 18 ? 11 : 3);
                    auto length = value == 16 ? lengths[i - 1] : 0;
                    if (repeat > literals + distances - i) return false;
                    while (repeat--) lengths[i++] = static_cast<uint8_t>(length);
                }
            }
        } else return false;
        PngTree literalTree(lengths, literals);
        PngTree distanceTree(lengths + literals, distances);
        while (reader.valid) {
            auto value = literalTree.read(reader);
            if (value == 256) break;
            if (value < 256) {
                if (raw.size() == expected) return false;
                raw.push_back(static_cast<uint8_t>(value));
            } else {
                if (value > 285) return false;
                auto index = value - 257;
                auto extra = index < 8 || index == 28 ? 0u : (index - 4) / 4;
                auto length = index < 8 ? index + 3 : index == 28 ? 258 : 3 + ((4 + (index % 4)) << extra);
                length += reader.read(extra);
                auto code = distanceTree.read(reader);
                if (code > 29) return false;
                extra = code < 4 ? 0u : code / 2 - 1;
                auto distance = code < 4 ? code + 1 : 1 + ((2 + (code % 2)) << extra);
                distance += reader.read(extra);
                if (distance > raw.size() || length > expected - raw.size()) return false;
                while (length--) raw.push_back(raw[raw.size() - distance]);
            }
        }
    }
    if (!reader.valid || raw.size() != expected) return false;
    uint32_t a = 1;
    uint32_t b = 0;
    for (auto value : raw) {
        a = (a + value) % 65521;
        b = (b + a) % 65521;
    }
    if ((b << 16 | a) != pngInteger(stream.data() + stream.size() - 4)) return false;
    for (auto y = 0u; y < surface.height(); y++) {
        auto data = raw.data() + y * stride + 1;
        auto filter = data[-1];
        if (filter > 4) return false;
        for (size_t x = 0; x < stride - 1; x++) {
            int left = x >= 4 ? data[x - 4] : 0;
            int up = y ? (data - stride)[x] : 0;
            int corner = y && x >= 4 ? (data - stride)[x - 4] : 0;
            auto estimate = left + up - corner;
            auto da = std::abs(estimate - left);
            auto db = std::abs(estimate - up);
            auto dc = std::abs(estimate - corner);
            auto paeth = da <= db && da <= dc ? left : db <= dc ? up : corner;
            auto predictor = filter == 1 ? left : filter == 2 ? up : filter == 3 ? (left + up) / 2 : filter == 4 ? paeth : 0;
            data[x] = static_cast<uint8_t>(data[x] + predictor);
            auto pixel = surface.data()[static_cast<size_t>(y) * surface.stride() + x / 4];
            if (data[x] != static_cast<uint8_t>(pixel >> ((x % 4) * 8))) return false;
        }
    }
    return true;
}

static void pngCompression()
{
    Surface surface;
    uint32_t sizes[][2] = {{1, 1}, {1, 17}, {17, 1}, {129, 97}, {513, 41}, {960, 540}};
    for (auto& size : sizes) {
        CHECK(surface.resize(size[0], size[1]) == Result::Success);
        if (!surface.data()) return;
        uint32_t random = 0x12345678;
        auto flat = size[0] == 960;
        for (auto y = 0u; y < size[1]; y++) {
            for (auto x = 0u; x < size[0]; x++) {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                auto value = flat || x % 128 < 32 ? 0x7f332211u : x % 128 < 64
                             ? (x & 255) | ((y & 255) << 8) | (((x + y) & 255) << 16) | ((y * 7u) << 24)
                             : x % 128 < 96 ? random : 0u;
                surface.data()[static_cast<size_t>(y) * surface.stride() + x] = value;
            }
        }
        CHECK(Saver::png(surface, "test-sw-png.png") == Result::Success);
        auto first = pngBytes("test-sw-png.png");
        CHECK(pngPixels(first, surface));
        if (flat) CHECK(first.size() < 10000u);
        CHECK(Saver::png(surface, "test-sw-png.png") == Result::Success);
        CHECK(first == pngBytes("test-sw-png.png"));
    }
    std::remove("test-sw-png.png");
}

static void scene2D(SwRenderer* renderer)
{
    Config config;
    config.width = 640;
    config.height = 360;
    config.camera.orthoHeight = 7.0f;
    auto scene = Scene::gen(config);
    CHECK(scene != nullptr);

    auto space = Space::gen({-6.0f, 6.0f, 1.0f}, {-3.0f, 3.0f, 1.0f});
    auto vector = Vector::gen({3.0f, 2.0f});
    auto circle = Circle::gen({2.0f, -1.0f}, 0.6f);
    CHECK(space && vector && circle);
    vector->tail = 14.0f;
    vector->stroke(Color::hex("#ffd166"), 4.0f);
    circle->stroke(Color::hex("#4cc9f0"), 3.0f).fill(Color::hex("#4cc9f044"));
    CHECK(scene->add(space) == Result::Success);
    CHECK(scene->play(Animation::create(space), 0.5f, Easing::Linear) == Result::Success);
    CHECK(space->add(vector) == Result::Success);
    CHECK(space->add(circle) == Result::Success);
    Animation create[] = {Animation::create(vector), Animation::create(circle)};
    CHECK(scene->play(create, 2, 1.0f, Easing::Smooth) == Result::Success);
    CHECK(scene->play(Animation::transform(vector, Mat4::rotateZ(0.65f) * vector->model), 0.75f) == Result::Success);
    CHECK(scene->wait(0.25f) == Result::Success);
    CHECK(scene->duration() == 2.5f);
    CHECK(scene->object(vector->id()) == vector);
    CHECK(scene->play(Animation::transform(static_cast<Object*>(nullptr), Mat4::identity())) == Result::InvalidArguments);
    Animation wrong = Animation::create(vector);
    wrong.kind = static_cast<AnimationKind>(255);
    CHECK(scene->play(wrong) == Result::InvalidArguments);
    CHECK(scene->wait(std::numeric_limits<float>::quiet_NaN()) == Result::InvalidArguments);

    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(renderer->render(scene, 1.0e-8f, surface) == Result::Success);
    CHECK(renderer->render(scene, std::numeric_limits<float>::quiet_NaN(), surface) == Result::InvalidArguments);
    auto atStart = changed(surface, config.background);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    auto atMiddle = changed(surface, config.background);
    CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
    auto atEnd = changed(surface, config.background);
    CHECK(atStart < atMiddle);
    CHECK(atMiddle < atEnd);
    CHECK(atEnd > 5000);
    Surface denseSurface;
    CHECK(renderer->render(scene, scene->duration(), denseSurface, 2u) == Result::Success);
    CHECK(denseSurface.width() == config.width * 2u);
    CHECK(denseSurface.height() == config.height * 2u);
    CHECK(changed(denseSurface, config.background) > atEnd * 3u);
    CHECK(scene->config().width == config.width && scene->config().height == config.height);
    CHECK(renderer->render(scene, 0.0f, denseSurface, 0u) == Result::InvalidArguments);
    CHECK(Saver::png(surface, "test-sw-2d.png") == Result::Success);
    delete scene;
}

static void coplanarDrawOrder(SwRenderer* renderer)
{
    Config config;
    config.width = 100;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto rectangle = Rectangle::gen({}, {2.0f, 2.0f}, 0.2f);
    Vec3 points[91];
    for (auto i = 0u; i < 91u; i++) {
        points[i] = {-0.8f + 1.6f * static_cast<float>(i) / 90.0f, 0.0f, 0.0f};
    }
    auto plot = Plot::gen(points, 91u);
    CHECK(scene && rectangle && plot);
    if (scene && rectangle && plot) {
        auto foreground = Color::hex("#4080d8");
        rectangle->stroke(Color::hex("#00000000"), 0.0f).fill(Color::hex("#d84040"));
        plot->stroke(foreground, 8.0f);
        auto rectangleResult = scene->add(rectangle);
        auto plotResult = scene->add(plot);
        CHECK(rectangleResult == Result::Success && plotResult == Result::Success);
        if (rectangleResult == Result::Success && plotResult == Result::Success) {
            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            if (surface.data()) {
                CHECK(surface.data()[50u * surface.stride() + 50u] == pixel(foreground));
            }
        } else {
            if (rectangleResult != Result::Success) delete rectangle;
            if (plotResult != Result::Success) delete plot;
        }
    } else {
        delete rectangle;
        delete plot;
    }
    delete scene;

}

static void rendererPixelSampling(SwRenderer* renderer)
{
    Config config;
    config.width = 100;
    config.height = 100;
    config.camera.orthoHeight = 2.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto background = Rectangle::gen({}, {1.2f, 1.2f});
    auto foreground = Rectangle::gen({}, {0.6f, 0.6f});
    CHECK(scene && background && foreground && renderer);
    if (!scene || !background || !foreground || !renderer) {
        delete scene;
        delete background;
        delete foreground;
        return;
    }
    background->fill(Color::hex("#e5484d"));
    foreground->fill(Color::hex("#30a46c"));
    foreground->layer = 1;
    CHECK(scene->add(background) == Result::Success);
    CHECK(scene->add(foreground) == Result::Success);
    const Object* candidates[] = {background, foreground};
    PixelSample sample;
    CHECK(renderer->sample(scene, 0.0f, {50.0f, 50.0f}, candidates, 2u, sample)
          == Result::Success);
    CHECK(sample.object == foreground && sample.color.r == 0x30 && sample.color.g == 0xa4
          && sample.color.b == 0x6c && sample.color.a == 0xff);
    CHECK(renderer->sample(scene, 0.0f, {25.0f, 50.0f}, candidates, 2u, sample, 2u)
          == Result::Success);
    CHECK(sample.object == background && sample.color.r == 0xe5 && sample.color.g == 0x48
          && sample.color.b == 0x4d && sample.color.a == 0xff);
    CHECK(renderer->sample(scene, 0.0f, {5.0f, 5.0f}, candidates, 2u, sample)
          == Result::InsufficientCondition);
    CHECK(!sample.object && sample.color.a == 0u);
    CHECK(renderer->sample(scene, 0.0f, {-1.0f, 5.0f}, candidates, 2u, sample)
          == Result::InvalidArguments);
    delete scene;
}

static Scene* _numberScene(NumberMode mode, bool numbers)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto space = Space::gen({0.0f, 3.0f, 1.0f}, {0.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 1.0f});
    if (!scene || !space) {
        delete scene;
        delete space;
        return nullptr;
    }
    space->style.stroke.a = 0;
    space->axisX.a = 0;
    space->axisY.a = 0;
    space->axisZ.a = 0;
    space->numbers = numbers;
    space->numberMode = mode;
    space->numberSize = 15.0f;
    space->model = Mat4::scale({0.5f, 0.5f, 1.0f});
    if (scene->add(space) != Result::Success) {
        delete scene;
        delete space;
        return nullptr;
    }
    return scene;
}

static void spaceNumbers(SwRenderer* renderer)
{
#ifdef VIS_TEST_FONT
    CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
#ifdef VIS_TEST_MONO_FONT
    CHECK(renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
#endif
#endif
    auto hidden = _numberScene(NumberMode::Fixed, false);
    auto fixed = _numberScene(NumberMode::Fixed, true);
    auto relative = _numberScene(NumberMode::Relative, true);
    CHECK(hidden && fixed && relative);
    if (hidden && fixed && relative) {
        Surface hiddenSurface;
        Surface fixedSurface;
        Surface relativeSurface;
        CHECK(renderer->render(hidden, 0.0f, hiddenSurface) == Result::Success);
        CHECK(renderer->render(fixed, 0.0f, fixedSurface) == Result::Success);
        CHECK(renderer->render(relative, 0.0f, relativeSurface) == Result::Success);
        CHECK(changed(hiddenSurface, hidden->config().background) == 0);
        CHECK(changed(fixedSurface, fixed->config().background) > 50);
        CHECK(checksum(fixedSurface) != checksum(relativeSurface));
    }
    delete hidden;
    delete fixed;
    delete relative;
}

static void textAnimationRegression(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto rectangle = Rectangle::gen({-1.25f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto text = Text::gen("A", {1.25f, 0.0f, 0.0f});
    CHECK(scene && rectangle && text);
    if (scene && rectangle && text) {
        rectangle->stroke(Color::hex("#4cc9f0"), 3.0f).fill(Color::hex("#4cc9f044"));
        text->size = 30.0f;
        text->fill(Color::hex("#ffd166"));
        auto rectangleResult = scene->add(rectangle);
        auto textResult = scene->add(text);
        CHECK(rectangleResult == Result::Success && textResult == Result::Success);
        if (rectangleResult == Result::Success && textResult == Result::Success) {
            Animation prepare[] = {
                Animation::shift(rectangle, {0.25f, 0.0f, 0.0f}),
                Animation::shift(text, {0.25f, 0.0f, 0.0f})};
            CHECK(scene->play(prepare, 2u, 0.5f, Easing::Linear) == Result::Success);
            CHECK(scene->wait(0.5f) == Result::Success);
            Animation create[] = {Animation::create(rectangle), Animation::create(text)};
            CHECK(scene->play(create, 2u, 1.0f, Easing::Linear) == Result::Success);

            Surface surface;
            CHECK(renderer->render(scene, 0.25f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(scene, 0.75f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
            auto left = 0u;
            auto right = 0u;
            auto background = pixel(config.background);
            for (auto y = 0u; y < surface.height(); y++) {
                for (auto x = 0u; x < surface.width(); x++) {
                    if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] == background) continue;
                    if (x < surface.width() / 2u) left++;
                    else right++;
                }
            }
            CHECK(left > 0u);
            CHECK(right > 0u);
        } else {
            if (rectangleResult != Result::Success) delete rectangle;
            if (textResult != Result::Success) delete text;
        }
    } else {
        delete rectangle;
        delete text;
    }
    delete scene;

    auto motion = Scene::gen(config);
    auto group = Group::gen();
    auto body = Rectangle::gen({-1.25f, 0.0f, 0.0f}, {0.8f, 0.8f});
    auto label = Text::gen("A", {0.75f, 0.0f, 0.0f});
    CHECK(motion && group && body && label);
    if (motion && group && body && label) {
        auto bodyColor = Color::hex("#ef5350");
        auto labelColor = Color::hex("#42a5f5");
        body->style.stroke.a = 0;
        body->fill(bodyColor);
        label->size = 30.0f;
        label->fill(labelColor);
        auto bodyResult = group->add(body);
        auto labelResult = group->add(label);
        CHECK(bodyResult == Result::Success && labelResult == Result::Success);
        auto groupResult = bodyResult == Result::Success && labelResult == Result::Success
                         ? motion->add(group) : Result::InvalidArguments;
        CHECK(groupResult == Result::Success);
        if (groupResult == Result::Success) {
            CHECK(motion->play(Animation::shift(group, {1.0f, 0.0f, 0.0f}), 1.0f,
                               Easing::Linear) == Result::Success);
            auto centerX = [](const Surface& surface, Color color) {
                auto target = pixel(color);
                auto sum = 0u;
                auto count = 0u;
                for (auto y = 0u; y < surface.height(); y++) {
                    for (auto x = 0u; x < surface.width(); x++) {
                        if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] != target) continue;
                        sum += x;
                        count++;
                    }
                }
                return count ? static_cast<float>(sum) / static_cast<float>(count) : -1.0f;
            };
            Surface surface;
            CHECK(renderer->render(motion, 0.0f, surface) == Result::Success);
            auto bodyStart = centerX(surface, bodyColor);
            auto labelStart = centerX(surface, labelColor);
            CHECK(renderer->render(motion, 0.5f, surface) == Result::Success);
            auto bodyMiddle = centerX(surface, bodyColor);
            auto labelMiddle = centerX(surface, labelColor);
            CHECK(renderer->render(motion, 1.0f, surface) == Result::Success);
            auto bodyEnd = centerX(surface, bodyColor);
            auto labelEnd = centerX(surface, labelColor);
            CHECK(bodyStart >= 0.0f && labelStart >= 0.0f);
            CHECK(bodyMiddle >= 0.0f && labelMiddle >= 0.0f);
            CHECK(bodyEnd >= 0.0f && labelEnd >= 0.0f);
            CHECK(near(labelStart - bodyStart, labelMiddle - bodyMiddle, 1.0f));
            CHECK(near(labelMiddle - bodyMiddle, labelEnd - bodyEnd, 1.0f));
        } else {
            if (bodyResult != Result::Success) delete body;
            if (labelResult != Result::Success) delete label;
            delete group;
        }
    } else {
        delete group;
        delete body;
        delete label;
    }
    delete motion;
}

static void repeatedCreateRegression(SwRenderer* renderer)
{
    Config config;
    config.width = 96;
    config.height = 64;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto rectangle = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.5f, 1.0f});
    CHECK(scene && rectangle);
    if (scene && rectangle) {
        rectangle->style.stroke.a = 0;
        rectangle->fill(Color::hex("#4cc9f0"));
        auto addResult = scene->add(rectangle);
        CHECK(addResult == Result::Success);
        if (addResult == Result::Success) {
            CHECK(scene->play(Animation::create(rectangle), 0.5f, Easing::Linear) == Result::Success);
            CHECK(scene->wait(0.5f) == Result::Success);
            CHECK(scene->play(Animation::create(rectangle), 0.5f, Easing::Linear) == Result::Success);

            Surface surface;
            CHECK(renderer->render(scene, 0.75f, surface) == Result::Success);
            CHECK(changed(surface, config.background) > 0u);
        } else {
            delete rectangle;
        }
    } else {
        delete rectangle;
    }
    delete scene;
}

static void drawDirectionAnimations(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;

    auto circles = Scene::gen(config);
    auto forward = Circle::gen({-1.1f, 0.0f, 0.0f}, 0.7f);
    auto clockwise = Circle::gen({1.1f, 0.0f, 0.0f}, 0.7f);
    CHECK(circles && forward && clockwise);
    if (circles && forward && clockwise) {
        forward->stroke(Color::hex("#f5f5f5"), 4.0f);
        forward->style.fill.a = 0;
        clockwise->stroke(Color::hex("#f5f5f5"), 4.0f);
        clockwise->style.fill.a = 0;
        auto forwardResult = circles->add(forward);
        auto clockwiseResult = circles->add(clockwise);
        CHECK(forwardResult == Result::Success && clockwiseResult == Result::Success);
        if (forwardResult == Result::Success && clockwiseResult == Result::Success) {
            Animation animations[] = {
                Animation::create(forward, DrawDirection::Forward),
                Animation::create(clockwise, DrawDirection::Clockwise)};
            CHECK(circles->play(animations, 2u, 1.0f, Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(circles, 0.25f, surface) == Result::Success);
            auto background = pixel(config.background);
            auto forwardUpper = 0u;
            auto forwardLower = 0u;
            auto clockwiseUpper = 0u;
            auto clockwiseLower = 0u;
            for (auto y = 0u; y < surface.height(); y++) {
                for (auto x = 0u; x < surface.width(); x++) {
                    if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] == background) continue;
                    if (x < surface.width() / 2u) {
                        if (y < surface.height() / 2u) forwardUpper++;
                        else forwardLower++;
                    } else {
                        if (y < surface.height() / 2u) clockwiseUpper++;
                        else clockwiseLower++;
                    }
                }
            }
            CHECK(forwardUpper > forwardLower + 10u);
            CHECK(clockwiseLower > clockwiseUpper + 10u);
        } else {
            if (forwardResult != Result::Success) delete forward;
            if (clockwiseResult != Result::Success) delete clockwise;
        }
    } else {
        delete forward;
        delete clockwise;
    }
    delete circles;

    auto reverseScene = Scene::gen(config);
    auto reverse = Line::gen({-1.5f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f});
    CHECK(reverseScene && reverse);
    if (reverseScene && reverse) {
        reverse->stroke(Color::hex("#4cc9f0"), 5.0f);
        auto result = reverseScene->add(reverse);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(reverseScene->play(Animation::create(reverse, DrawDirection::Reverse), 1.0f,
                                     Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(reverseScene, 0.25f, surface) == Result::Success);
            auto background = pixel(config.background);
            auto left = 0u;
            auto right = 0u;
            for (auto y = 0u; y < surface.height(); y++) {
                for (auto x = 0u; x < surface.width(); x++) {
                    if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] == background) continue;
                    if (x < surface.width() / 2u) left++;
                    else right++;
                }
            }
            CHECK(left == 0u);
            CHECK(right > 20u);
        } else {
            delete reverse;
        }
    } else {
        delete reverse;
    }
    delete reverseScene;

    auto frames = [&](Object* object, DrawDirection direction, uint64_t& middle,
                      uint64_t& end, uint32_t& pixels) {
        auto scene = Scene::gen(config);
        if (!scene || !object) {
            delete object;
            delete scene;
            return false;
        }
        if (scene->add(object) != Result::Success) {
            delete object;
            delete scene;
            return false;
        }
        if (scene->play(Animation::create(object, direction), 1.0f, Easing::Linear)
            != Result::Success) {
            delete scene;
            return false;
        }
        Surface surface;
        if (renderer->render(scene, 0.25f, surface) != Result::Success) {
            delete scene;
            return false;
        }
        middle = checksum(surface);
        if (renderer->render(scene, 1.0f, surface) != Result::Success) {
            delete scene;
            return false;
        }
        end = checksum(surface);
        pixels = changed(surface, config.background);
        delete scene;
        return true;
    };
    auto arrow = []() {
        auto object = Arrow::gen({-1.5f, -0.4f, 0.0f}, {1.5f, 0.5f, 0.0f});
        if (!object) return object;
        object->tail = 7.0f;
        object->tip = 19.0f;
        object->stroke(Color::hex("#ffd166"), 4.0f);
        return object;
    };
    auto vector = []() {
        auto object = Vector::gen({3.0f, 0.9f, 0.0f}, {-1.5f, -0.4f, 0.0f});
        if (!object) return object;
        object->tail = 9.0f;
        object->tip = 17.0f;
        object->stroke(Color::hex("#4cc9f0"), 4.0f);
        return object;
    };
    auto forwardMiddle = uint64_t{0};
    auto forwardEnd = uint64_t{0};
    auto forwardPixels = 0u;
    auto reverseMiddle = uint64_t{0};
    auto reverseEnd = uint64_t{0};
    auto reversePixels = 0u;
    CHECK(frames(arrow(), DrawDirection::Forward, forwardMiddle, forwardEnd, forwardPixels));
    CHECK(frames(arrow(), DrawDirection::Reverse, reverseMiddle, reverseEnd, reversePixels));
    CHECK(forwardMiddle != reverseMiddle);
    CHECK(forwardEnd == reverseEnd);
    CHECK(forwardPixels > 0u && forwardPixels == reversePixels);
    CHECK(frames(vector(), DrawDirection::Forward, forwardMiddle, forwardEnd, forwardPixels));
    CHECK(frames(vector(), DrawDirection::Reverse, reverseMiddle, reverseEnd, reversePixels));
    CHECK(forwardMiddle != reverseMiddle);
    CHECK(forwardEnd == reverseEnd);
    CHECK(forwardPixels > 0u && forwardPixels == reversePixels);

    auto revealScene = Scene::gen(config);
    auto line = Line::gen({-1.5f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f});
    CHECK(revealScene && line);
    if (revealScene && line) {
        line->stroke(Color::hex("#ffd166"), 5.0f);
        auto result = revealScene->add(line);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(revealScene->play(Animation::create(line), 1.0f, Easing::Linear) == Result::Success);
            CHECK(revealScene->play(Animation::uncreate(line), 1.0f, Easing::Linear) == Result::Success);
            CHECK(revealScene->play(Animation::create(line), 1.0f, Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(revealScene, 0.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(revealScene, 0.5f, surface) == Result::Success);
            auto creating = changed(surface, config.background);
            CHECK(renderer->render(revealScene, 1.0f, surface) == Result::Success);
            auto created = changed(surface, config.background);
            CHECK(renderer->render(revealScene, 1.5f, surface) == Result::Success);
            auto uncreating = changed(surface, config.background);
            CHECK(renderer->render(revealScene, 2.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(revealScene, 2.5f, surface) == Result::Success);
            auto recreated = changed(surface, config.background);
            CHECK(creating > 0u && creating < created);
            CHECK(uncreating > 0u && uncreating < created);
            CHECK(recreated > 0u && recreated < created);
        } else {
            delete line;
        }
    } else {
        delete line;
    }
    delete revealScene;
}

static void arrowCompositing(SwRenderer* renderer)
{
    Config config;
    config.width = 200u;
    config.height = 100u;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#ffffff");
    config.antialiasing = false;

    auto scene = Scene::gen(config);
    auto arrow = Arrow::gen({-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f});
    CHECK(scene && arrow);
    if (scene && arrow) {
        arrow->tip = 20.0f;
        arrow->stroke(Color::hex("#ff0000"), 8.0f);
        CHECK(scene->add(arrow) == Result::Success);
        CHECK(scene->play(Animation::fade(arrow, 0.5f), 1.0f, Easing::Linear)
              == Result::Success);
        Surface surface;
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        auto shaft = surface.data()[50u * surface.stride() + 100u];
        auto junction = surface.data()[50u * surface.stride() + 158u];
        CHECK(shaft == junction);
    } else {
        delete arrow;
    }
    delete scene;

    scene = Scene::gen(config);
    arrow = Arrow::gen({-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f});
    CHECK(scene && arrow);
    if (scene && arrow) {
        float pattern[] = {1000.0f, 1.0f};
        arrow->tail = 16.0f;
        arrow->tip = 20.0f;
        arrow->stroke(Color::hex("#2563eb"), 8.0f);
        CHECK(arrow->dash(pattern, 2u) == Result::Success);
        CHECK(scene->add(arrow) == Result::Success);
        CHECK(scene->play(Animation::fade(arrow, 0.5f), 1.0f, Easing::Linear)
              == Result::Success);
        Surface surface;
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        auto shaft = surface.data()[50u * surface.stride() + 100u];
        auto tail = surface.data()[50u * surface.stride() + 31u];
        auto tip = surface.data()[50u * surface.stride() + 169u];
        CHECK(shaft == tail);
        CHECK(shaft == tip);
    } else {
        delete arrow;
    }
    delete scene;
}

static void directedRouteRendering(SwRenderer* renderer)
{
    Config config;
    config.width = 200u;
    config.height = 100u;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#ffffff");
    config.antialiasing = false;
    auto foreground = pixel(Color::hex("#2563eb"));

    auto scene = Scene::gen(config);
    auto arrow = Arrow::gen({-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f});
    CHECK(scene && arrow);
    if (scene && arrow) {
        float pattern[] = {12.0f, 8.0f};
        arrow->tail = 12.0f;
        arrow->tip = 20.0f;
        arrow->stroke(Color::hex("#2563eb"), 4.0f);
        CHECK(arrow->dash(pattern, 2u) == Result::Success);
        CHECK(scene->add(arrow) == Result::Success);
        CHECK(scene->play(Animation::create(arrow), 1.0f, Easing::Linear)
              == Result::Success);
        auto phase = AnimationTarget::from(arrow);
        phase.dashOffset(-8.0f).tail(18.0f).tip(26.0f);
        CHECK(scene->play(phase, 1.0f, Easing::Linear) == Result::Success);
        CHECK(near(arrow->tail, 18.0f) && near(arrow->tip, 26.0f));
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, config.background) == 0u);
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        auto partial = changed(surface, config.background);
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        auto complete = changed(surface, config.background);
        auto firstPhase = checksum(surface);
        CHECK(surface.data()[50u * surface.stride() + 31u] == foreground);
        CHECK(surface.data()[50u * surface.stride() + 165u] == foreground);
        CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
        CHECK(checksum(surface) != firstPhase);
        CHECK(surface.data()[50u * surface.stride() + 31u] == foreground);
        CHECK(surface.data()[50u * surface.stride() + 165u] == foreground);
        CHECK(partial > 0u && partial < complete);
    } else {
        delete arrow;
    }
    delete scene;

    Vec3 points[] = {
        {-3.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f}, {3.0f, -1.0f, 0.0f},
    };
    scene = Scene::gen(config);
    auto route = DirectedRoute::gen(points, 4u);
    CHECK(scene && route && route->type() == Type::Route && route->count() == 4u);
    if (scene && route) {
        float pattern[] = {10.0f, 6.0f};
        route->tail = 10.0f;
        route->tip = 18.0f;
        route->stroke(Color::hex("#2563eb"), 4.0f);
        CHECK(route->dash(pattern, 2u) == Result::Success);
        CHECK(scene->add(route) == Result::Success);
        CHECK(scene->play(Animation::create(route), 1.0f, Easing::Linear)
              == Result::Success);
        Surface surface;
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        auto partial = changed(surface, config.background);
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        auto complete = changed(surface, config.background);
        CHECK(partial > 0u && partial < complete);
        BBox bounds;
        CHECK(renderer->bounds(scene, route, 1.0f, bounds) == Result::Success);
        CHECK(bounds.width > 140.0f && bounds.height > 40.0f);
    } else {
        delete route;
    }
    delete scene;
}

static void fadeAnimations(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;

    auto entrance = Scene::gen(config);
    auto entering = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(entrance && entering);
    if (entrance && entering) {
        entering->style.stroke.a = 0;
        entering->fill(Color::hex("#4cc9f0"));
        auto result = entrance->add(entering);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(entrance->play(Animation::shift(entering, {1.5f, 0.0f, 0.0f}), 0.5f,
                                 Easing::Linear) == Result::Success);
            CHECK(entrance->wait(0.5f) == Result::Success);
            CHECK(entrance->play(Animation::fadeIn(entering, {-0.5f, 0.0f, 0.0f}, 0.8f),
                                 1.0f, Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(entrance, 0.25f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(entrance, 0.75f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(entrance, 1.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(entrance, 1.5f, surface) == Result::Success);
            CHECK(changed(surface, config.background) > 0u);
            CHECK(renderer->render(entrance, 2.0f, surface) == Result::Success);
            auto finalExtent = extent(surface, config.background);
            CHECK(finalExtent.valid);
            if (finalExtent.valid) CHECK(finalExtent.minX >= surface.width() / 2u);
        } else {
            delete entering;
        }
    } else {
        delete entering;
    }
    delete entrance;

    auto exit = Scene::gen(config);
    auto exiting = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(exit && exiting);
    if (exit && exiting) {
        exiting->style.stroke.a = 0;
        exiting->fill(Color::hex("#ef5350"));
        auto result = exit->add(exiting);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(exit->play(Animation::fadeOut(exiting, {2.0f, 0.0f, 0.0f}), 1.0f,
                             Easing::Linear) == Result::Success);
            CHECK(exit->play(Animation::fadeIn(exiting), 0.5f, Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(exit, 0.0f, surface) == Result::Success);
            auto start = extent(surface, config.background);
            CHECK(renderer->render(exit, 0.5f, surface) == Result::Success);
            auto middle = extent(surface, config.background);
            CHECK(renderer->render(exit, 1.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(exit, 1.5f, surface) == Result::Success);
            CHECK(changed(surface, config.background) > 0u);
            CHECK(start.valid && middle.valid);
            if (start.valid && middle.valid) {
                auto startCenter = (start.minX + start.maxX) / 2u;
                auto middleCenter = (middle.minX + middle.maxX) / 2u;
                CHECK(middleCenter > startCenter + 10u);
            }
        } else {
            delete exiting;
        }
    } else {
        delete exiting;
    }
    delete exit;

    auto handoff = Scene::gen(config);
    auto source = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto target = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    CHECK(handoff && source && target);
    if (handoff && source && target) {
        source->style.stroke.a = 0;
        source->fill(Color::hex("#ef5350"));
        target->style.stroke.a = 0;
        target->fill(Color::hex("#42a5f5"));
        auto sourceResult = handoff->add(source);
        auto targetResult = handoff->add(target);
        CHECK(sourceResult == Result::Success && targetResult == Result::Success);
        if (sourceResult == Result::Success && targetResult == Result::Success) {
            CHECK(handoff->fadeTransform(source, target, 1.0f, Easing::Linear)
                  == Result::Success);
            CHECK(near(handoff->duration(), 1.0f));
            CHECK(handoff->play(Animation::fadeOut(source), 0.25f, Easing::Linear)
                  == Result::InvalidArguments);
            CHECK(handoff->play(Animation::indicate(target), 0.25f, Easing::Linear)
                  == Result::Success);

            Surface surface;
            auto halves = [&](float time) {
                CHECK(renderer->render(handoff, time, surface) == Result::Success);
                auto background = pixel(config.background);
                auto left = 0u;
                auto right = 0u;
                for (auto y = 0u; y < surface.height(); y++) {
                    for (auto x = 0u; x < surface.width(); x++) {
                        if (surface.data()[static_cast<size_t>(y) * surface.stride() + x]
                            == background) continue;
                        if (x < surface.width() / 2u) left++;
                        else right++;
                    }
                }
                return Vec2{static_cast<float>(left), static_cast<float>(right)};
            };
            auto start = halves(0.0f);
            auto middle = halves(0.5f);
            auto end = halves(1.0f);
            CHECK(start.x > 0.0f && start.y == 0.0f);
            CHECK(middle.x > 0.0f && middle.y > 0.0f);
            CHECK(end.x == 0.0f && end.y > 0.0f);
        } else {
            if (sourceResult != Result::Success) delete source;
            if (targetResult != Result::Success) delete target;
        }
    } else {
        delete source;
        delete target;
    }
    delete handoff;

    auto invalid = Scene::gen(config);
    auto staleSource = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto staleTarget = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    auto foreignScene = Scene::gen(config);
    auto foreignTarget = Circle::gen({}, 0.5f);
    CHECK(invalid && staleSource && staleTarget && foreignScene && foreignTarget);
    if (invalid && staleSource && staleTarget && foreignScene && foreignTarget) {
        auto sourceResult = invalid->add(staleSource);
        auto targetResult = invalid->add(staleTarget);
        auto foreignResult = foreignScene->add(foreignTarget);
        CHECK(sourceResult == Result::Success && targetResult == Result::Success
              && foreignResult == Result::Success);
        if (sourceResult == Result::Success && targetResult == Result::Success
            && foreignResult == Result::Success) {
            CHECK(invalid->fadeTransform(staleSource, staleSource)
                  == Result::InvalidArguments);
            CHECK(invalid->fadeTransform(staleSource, foreignTarget)
                  == Result::InvalidArguments);
            CHECK(invalid->wait(0.1f) == Result::Success);
            CHECK(invalid->fadeTransform(staleSource, staleTarget)
                  == Result::InsufficientCondition);
            CHECK(near(invalid->duration(), 0.1f));
        } else {
            if (sourceResult != Result::Success) delete staleSource;
            if (targetResult != Result::Success) delete staleTarget;
            if (foreignResult != Result::Success) delete foreignTarget;
        }
    } else {
        delete staleSource;
        delete staleTarget;
        delete foreignTarget;
    }
    delete foreignScene;
    delete invalid;
}

static void scaleAndIndicateAnimations(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;

    auto scene = Scene::gen(config);
    auto group = Group::gen();
    auto body = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto label = Text::gen("A", {1.0f, 0.0f, 0.0f});
    CHECK(scene && group && body && label);
    if (scene && group && body && label) {
        body->style.stroke.a = 0;
        body->fill(Color::hex("#ef5350"));
        label->size = 36.0f;
        label->fill(Color::hex("#42a5f5"));
        auto bodyResult = group->add(body);
        auto labelResult = group->add(label);
        CHECK(bodyResult == Result::Success && labelResult == Result::Success);
        auto groupResult = bodyResult == Result::Success && labelResult == Result::Success
                         ? scene->add(group) : Result::InvalidArguments;
        CHECK(groupResult == Result::Success);
        if (groupResult == Result::Success) {
            CHECK(scene->play(Animation::growFromCenter(group), 1.0f, Easing::Linear)
                  == Result::Success);
            CHECK(scene->play(Animation::shrinkToCenter(group), 1.0f, Easing::Linear)
                  == Result::Success);
            auto halves = [&](const Surface& surface) {
                auto background = pixel(config.background);
                auto left = 0u;
                auto right = 0u;
                for (auto y = 0u; y < surface.height(); y++) {
                    for (auto x = 0u; x < surface.width(); x++) {
                        if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] == background) continue;
                        if (x < surface.width() / 2u) left++;
                        else right++;
                    }
                }
                return Vec2{static_cast<float>(left), static_cast<float>(right)};
            };
            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
            auto growExtent = extent(surface, config.background);
            auto growHalves = halves(surface);
            CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
            auto fullExtent = extent(surface, config.background);
            auto fullHalves = halves(surface);
            CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
            auto shrinkExtent = extent(surface, config.background);
            auto shrinkHalves = halves(surface);
            CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(growExtent.valid && fullExtent.valid && shrinkExtent.valid);
            if (growExtent.valid && fullExtent.valid && shrinkExtent.valid) {
                CHECK(growExtent.maxX - growExtent.minX < fullExtent.maxX - fullExtent.minX);
                CHECK(shrinkExtent.maxX - shrinkExtent.minX < fullExtent.maxX - fullExtent.minX);
                CHECK(growExtent.maxY - growExtent.minY < fullExtent.maxY - fullExtent.minY);
                CHECK(shrinkExtent.maxY - shrinkExtent.minY < fullExtent.maxY - fullExtent.minY);
            }
            CHECK(growHalves.x > 0.0f && fullHalves.x > growHalves.x);
            CHECK(shrinkHalves.x > 0.0f && fullHalves.x > shrinkHalves.x);
            if (fullHalves.y > 0.0f) {
                CHECK(growHalves.y > 0.0f && fullHalves.y > growHalves.y);
                CHECK(shrinkHalves.y > 0.0f && fullHalves.y > shrinkHalves.y);
            }
        } else {
            if (bodyResult != Result::Success) delete body;
            if (labelResult != Result::Success) delete label;
            delete group;
        }
    } else {
        delete group;
        delete body;
        delete label;
    }
    delete scene;

    config.width = 160;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    auto edgeScene = Scene::gen(config);
    auto edgeBar = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 2.0f});
    CHECK(edgeScene && edgeBar);
    if (edgeScene && edgeBar) {
        edgeBar->style.stroke.a = 0;
        edgeBar->fill(Color::hex("#40d890"));
        auto result = edgeScene->add(edgeBar);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(edgeScene->play(Animation::growFromEdge(edgeBar, GrowthEdge::Bottom),
                                  1.0f, Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(edgeScene, 0.5f, surface) == Result::Success);
            auto middleExtent = extent(surface, config.background);
            CHECK(renderer->render(edgeScene, 1.0f, surface) == Result::Success);
            auto finalExtent = extent(surface, config.background);
            CHECK(middleExtent.valid && finalExtent.valid);
            if (middleExtent.valid && finalExtent.valid) {
                auto nearPixel = [](uint32_t lhs, uint32_t rhs) {
                    return lhs > rhs ? lhs - rhs <= 1u : rhs - lhs <= 1u;
                };
                CHECK(nearPixel(middleExtent.maxY, finalExtent.maxY));
                CHECK(middleExtent.maxY - middleExtent.minY
                      < finalExtent.maxY - finalExtent.minY);
                CHECK(nearPixel(middleExtent.maxX, finalExtent.maxX));
                CHECK(nearPixel(middleExtent.minX, finalExtent.minX));
            }
        } else {
            delete edgeBar;
        }
    } else {
        delete edgeBar;
    }
    delete edgeScene;

    config.width = 160;
    config.height = 80;
    auto indicateScene = Scene::gen(config);
    auto indicated = Rectangle::gen({}, {1.4f, 0.8f});
    CHECK(indicateScene && indicated);
    if (indicateScene && indicated) {
        indicated->style.stroke.a = 0;
        indicated->fill(Color::hex("#40d890"));
        auto result = indicateScene->add(indicated);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(indicateScene->play(Animation::indicate(indicated, Color::hex("#ffd166"), 1.4f),
                                      1.0f, Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(indicateScene, 0.0f, surface) == Result::Success);
            auto startHash = checksum(surface);
            auto startPixels = changed(surface, config.background);
            auto startExtent = extent(surface, config.background);
            CHECK(renderer->render(indicateScene, 0.5f, surface) == Result::Success);
            auto middleHash = checksum(surface);
            auto middlePixels = changed(surface, config.background);
            auto middleExtent = extent(surface, config.background);
            CHECK(renderer->render(indicateScene, 1.0f, surface) == Result::Success);
            auto endHash = checksum(surface);
            CHECK(startHash == endHash);
            CHECK(startHash != middleHash);
            CHECK(middlePixels > startPixels);
            CHECK(startExtent.valid && middleExtent.valid);
            if (startExtent.valid && middleExtent.valid) {
                CHECK(middleExtent.maxX - middleExtent.minX > startExtent.maxX - startExtent.minX);
                CHECK(middleExtent.maxY - middleExtent.minY > startExtent.maxY - startExtent.minY);
            }
        } else {
            delete indicated;
        }
    } else {
        delete indicated;
    }
    delete indicateScene;
}

// Named emphasis effects in a 200 by 200 frame 10 units high, so a world unit is 20 pixels.
static void emphasisEffects(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    // Whether the object's pixel bounds at time center on world point (x, y) and measure width
    // by height world units.
    auto placed = [renderer](Scene* scene, Object* object, float time, float x, float y, float width,
                             float height) {
        BBox bounds;
        if (renderer->bounds(scene, object, time, bounds) != Result::Success) return false;
        auto center = bounds.center();
        auto fits = near(center.x, 100.0f + 20.0f * x, 1.0f) && near(center.y, 100.0f - 20.0f * y, 1.0f)
                    && near(bounds.width, 20.0f * width, 1.5f) && near(bounds.height, 20.0f * height, 1.5f);
        if (!fits) {
            std::printf("  %.2f s: %.2f x %.2f px at (%.2f, %.2f), expected %.2f x %.2f at (%.2f, %.2f)\n",
                        static_cast<double>(time), static_cast<double>(bounds.width),
                        static_cast<double>(bounds.height), static_cast<double>(center.x),
                        static_cast<double>(center.y), static_cast<double>(20.0f * width),
                        static_cast<double>(20.0f * height), static_cast<double>(100.0f + 20.0f * x),
                        static_cast<double>(100.0f - 20.0f * y));
        }
        return fits;
    };
    auto lit = [](const Surface& surface, float x, float y) {
        auto column = static_cast<uint32_t>(100.0f + 20.0f * x);
        auto row = static_cast<uint32_t>(100.0f - 20.0f * y);
        return surface.data()[static_cast<size_t>(row) * surface.stride() + column] != pixel(Color::hex("#000000"));
    };

    auto scene = Scene::gen(config);
    auto bar = Rectangle::gen({0.0f, 2.0f, 0.0f}, {4.0f, 0.4f});
    auto riding = Rectangle::gen({-2.0f, -2.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(scene && bar && riding);
    if (!scene || !bar || !riding) {
        delete riding;
        delete bar;
        delete scene;
        return;
    }
    Object* shapes[] = {bar, riding};
    for (auto shape : shapes) {
        shape->style.stroke.a = 0;
        shape->fill(Color::hex("#ffffff"));
        CHECK(scene->add(shape) == Result::Success);
    }

    // Growth peaks halfway; rotation starts counterclockwise and returns to the original pose.
    auto model = bar->model;
    CHECK(scene->play(Animation::wiggle(bar, 1.5f, 0.6f, 2.0f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(near(scene->duration(), 1.0f));
    auto tipped = [](float grow, float turn, float length, float thickness) {
        return Vec2{grow * (length * std::cos(turn) + thickness * std::fabs(std::sin(turn))),
                    grow * (length * std::fabs(std::sin(turn)) + thickness * std::cos(turn))};
    };
    auto quarter = tipped(1.25f, 0.3f, 4.0f, 0.4f);
    CHECK(placed(scene, bar, 0.0f, 0.0f, 2.0f, 4.0f, 0.4f));
    CHECK(placed(scene, bar, 0.25f, 0.0f, 2.0f, quarter.x, quarter.y));
    CHECK(placed(scene, bar, 0.5f, 0.0f, 2.0f, 6.0f, 0.6f));
    CHECK(placed(scene, bar, 0.75f, 0.0f, 2.0f, quarter.x, quarter.y));
    CHECK(placed(scene, bar, 1.0f, 0.0f, 2.0f, 4.0f, 0.4f));
    Surface surface;
    CHECK(renderer->render(scene, 0.25f, surface) == Result::Success);
    CHECK(lit(surface, 2.2f, 2.6f) && !lit(surface, 2.2f, 1.4f));
    CHECK(renderer->render(scene, 0.75f, surface) == Result::Success);
    CHECK(!lit(surface, 2.2f, 2.6f) && lit(surface, 2.2f, 1.4f));
    for (auto i = 0u; i < 16u; i++) CHECK(near(bar->model.e[i], model.e[i]));

    // It may begin while the object travels, growing and turning about the moving center.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(riding, {4.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::wiggle(riding, 1.5f, 0.5f, 1.0f), 1.0f, Easing::Linear) == Result::Success);
    auto peak = tipped(1.5f, 0.5f, 1.0f, 1.0f);
    auto late = tipped(1.25f, 0.5f * 0.5f * std::sin(0.75f * 3.14159265f), 1.0f, 1.0f);
    CHECK(placed(scene, riding, 0.5f, -1.0f, -2.0f, 1.0f, 1.0f));
    CHECK(placed(scene, riding, 1.0f, 0.0f, -2.0f, peak.x, peak.y));
    CHECK(placed(scene, riding, 1.25f, 0.5f, -2.0f, late.x, late.y));
    CHECK(placed(scene, riding, 2.0f, 2.0f, -2.0f, 1.0f, 1.0f));
    CHECK(near(riding->model.e[0], 1.0f) && near(riding->model.e[3], 4.0f));

    // A wiggle needs a positive scale and swings and a finite angle, and it shares the scale of
    // pixel sizes with the other effects, so it cannot overlap an indication.
    CHECK(scene->play(Animation::wiggle(bar, 0.0f), 1.0f) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "positive number of swings"));
    CHECK(scene->play(Animation::wiggle(bar, 1.1f, 0.1f, 0.0f), 1.0f) == Result::InvalidArguments);
    CHECK(scene->play(Animation::wiggle(bar, 1.1f, std::numeric_limits<float>::infinity()), 1.0f)
          == Result::InvalidArguments);
    CHECK(scene->play(Animation::indicate(bar), 1.0f) == Result::Success);
    CHECK(scene->seek(scene->cursor() - 0.5f) == Result::Success);
    CHECK(scene->play(Animation::wiggle(bar), 1.0f) == Result::InsufficientCondition);
    delete scene;

    // A circumscribe frames the box with shapes of its own, tagged after it, that exist only
    // while it plays. The frame, 4 by 2 units and 12 around, starts at its top right corner and
    // runs counterclockwise; it draws on and away along ShowCreationThenDestruction's window of
    // 2, so its drawn stretch spans 3t - 2 to 3t of the outline, while a stretch of the given
    // share, twice as wide, spans 1.25t - 0.25 to 1.25t. Strokes widen the drawn bounds.
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto marked = Scene::gen(config);
    auto box = Rectangle::gen({0.0f, 0.0f, 0.0f}, {3.8f, 1.8f});
    auto moving = Rectangle::gen({-2.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(marked && box && moving);
    if (!marked || !box || !moving) {
        delete moving;
        delete box;
        delete marked;
        return;
    }
    box->tag("box");
    box->layer = 2;
    Object* framed[] = {box, moving};
    for (auto shape : framed) {
        shape->style.stroke.a = 0;
        shape->fill(Color::hex("#ffffff"));
        CHECK(marked->add(shape) == Result::Success);
    }
    auto circumscribe = Emphasis::circumscribe(box);
    circumscribe.width = 1.0f;
    circumscribe.window = 0.25f;
    CHECK(marked->emphasize(circumscribe, 1.0f, linear) == Result::Success);
    CHECK(near(marked->cursor(), 1.0f) && near(marked->duration(), 1.0f) && marked->count() == 4u);
    auto frame = marked->object("box.circumscribe.1.frame");
    auto flash = marked->object("box.circumscribe.1.flash");
    CHECK(frame && flash && frame->type() == Type::Plot && frame->layer == 2 && flash->layer == 2);
    if (!frame || !flash) {
        delete marked;
        return;
    }
    BBox drawn;
    CHECK(renderer->bounds(marked, frame, 0.0f, drawn) == Result::InsufficientCondition);
    CHECK(placed(marked, frame, 1.0f / 12.0f, 0.5f, 1.0f, 3.05f, 0.05f));
    CHECK(placed(marked, frame, 0.5f, 0.0f, 0.0f, 4.05f, 2.05f));
    CHECK(placed(marked, frame, 0.9f, 1.2f, 0.0f, 1.65f, 2.05f));
    CHECK(placed(marked, flash, 0.4f, -1.5f, 0.0f, 1.1f, 2.1f));
    CHECK(placed(marked, flash, 0.8f, 1.5f, 0.0f, 1.1f, 2.1f));
    // Nothing of it stays.
    CHECK(renderer->bounds(marked, frame, 1.0f, drawn) == Result::InsufficientCondition);
    CHECK(renderer->bounds(marked, flash, 1.0f, drawn) == Result::InsufficientCondition);
    CHECK(placed(marked, box, 1.0f, 0.0f, 0.0f, 3.8f, 1.8f));

    // Another circumscribe of the box takes the next number. An ellipse passes through the
    // corners of the padded bounds, so its axes are those of the frame times the square root
    // of 2.
    CHECK(marked->wait(0.5f) == Result::Success);
    CHECK(marked->emphasize(Emphasis::circumscribe(box, EmphasisShape::Ellipse), 1.0f, linear) == Result::Success);
    auto ellipse = marked->object("box.circumscribe.2.frame");
    CHECK(ellipse && marked->object("box.circumscribe.2.flash"));
    CHECK(ellipse && placed(marked, ellipse, 2.0f, 0.0f, 0.0f, 5.657f + 0.15f, 2.828f + 0.15f));
    CHECK(renderer->bounds(marked, frame, 2.0f, drawn) == Result::InsufficientCondition);

    // It may begin while the target travels: the frame stands where the target is when it
    // begins, and the travel goes on. An untagged target leaves its tag out.
    CHECK(marked->seek(0.0f) == Result::Success);
    CHECK(marked->play(Animation::shift(moving, {4.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(marked->seek(0.5f) == Result::Success);
    auto around = Emphasis::circumscribe(moving);
    around.width = 1.0f;
    around.padding = 0.5f;
    CHECK(marked->emphasize(around, 1.0f, linear) == Result::Success);
    CHECK(near(marked->cursor(), 1.5f));
    auto rider = marked->object("circumscribe.1.frame");
    CHECK(rider && placed(marked, rider, 1.0f, -1.0f, -3.0f, 2.05f, 2.05f));
    CHECK(placed(marked, moving, 1.0f, 0.0f, -3.0f, 1.0f, 1.0f));
    CHECK(placed(marked, moving, 2.0f, 2.0f, -3.0f, 1.0f, 1.0f));

    // Values out of range, a target outside the scene, and a target with nothing to frame at
    // the cursor are refused, and a refused call adds nothing.
    auto objects = marked->count();
    auto clips = marked->clipCount();
    auto bad = Emphasis::circumscribe(box);
    bad.window = 0.0f;
    CHECK(marked->emphasize(bad) == Result::InvalidArguments);
    CHECK(marked->diagnostic() && std::strstr(marked->diagnostic(), "window greater than 0"));
    bad = Emphasis::circumscribe(box);
    bad.padding = -0.1f;
    CHECK(marked->emphasize(bad) == Result::InvalidArguments);
    bad = Emphasis::circumscribe(box);
    bad.width = 0.0f;
    CHECK(marked->emphasize(bad) == Result::InvalidArguments);
    CHECK(marked->emphasize(Emphasis::circumscribe(box), 0.0f) == Result::InvalidArguments);
    CHECK(marked->emphasize(Emphasis::circumscribe(nullptr)) == Result::InvalidArguments);
    auto group = Group::gen();
    CHECK(group && marked->add(group) == Result::Success);
    CHECK(marked->emphasize(Emphasis::circumscribe(group)) == Result::InvalidArguments);
    CHECK(marked->diagnostic() && std::strstr(marked->diagnostic(), "no geometry at the cursor"));
    CHECK(marked->count() == objects + 1u && marked->clipCount() == clips);
    delete marked;

    // A passing flash runs a stretch of the target's outline, placed by its model, ahead of the
    // curve: over a linear second with a window of 0.25 its stretch spans 1.25t - 0.25 to 1.25t
    // of the outline, so it enters at the start, leaves past the end and nothing stays. The line,
    // 4 units long, lies on y = -2 by the model of the group that holds it.
    auto traced = Scene::gen(config);
    auto holder = Group::gen();
    auto line = Line::gen({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
    auto node = Rectangle::gen({-3.0f, 3.0f, 0.0f}, {1.0f, 1.0f});
    auto leaf = Rectangle::gen({3.0f, 3.0f, 0.0f}, {1.0f, 1.0f});
    auto edge = Connector::gen(node, leaf);
    CHECK(traced && holder && line && node && leaf && edge);
    if (!traced || !holder || !line || !node || !leaf || !edge) {
        delete edge;
        delete leaf;
        delete node;
        delete line;
        delete holder;
        delete traced;
        return;
    }
    holder->model = Mat4::translate({0.0f, -2.0f, 0.0f});
    line->tag("line");
    line->layer = 1;
    line->strokeWidth(1.0f);
    CHECK(holder->add(line) == Result::Success);
    node->style.stroke.a = 0;
    node->fill(Color::hex("#ffffff"));
    leaf->style.stroke.a = 0;
    leaf->fill(Color::hex("#ffffff"));
    Object* tracedShapes[] = {holder, node, leaf, edge};
    for (auto shape : tracedShapes) CHECK(traced->add(shape) == Result::Success);
    auto passing = Emphasis::passingFlash(line);
    CHECK(near(passing.width, 6.0f) && near(passing.window, 0.3f));
    passing.width = 1.0f;
    passing.window = 0.25f;
    CHECK(traced->emphasize(passing, 1.0f, linear) == Result::Success);
    auto streak = traced->object("line.passing_flash.1.flash");
    CHECK(streak && streak->type() == Type::Plot && !streak->parent() && streak->layer == 1);
    if (!streak) {
        delete traced;
        return;
    }
    CHECK(renderer->bounds(traced, streak, 0.0f, drawn) == Result::InsufficientCondition);
    CHECK(placed(traced, streak, 0.1f, -1.75f, -2.0f, 0.55f, 0.05f));
    CHECK(placed(traced, streak, 0.4f, -0.5f, -2.0f, 1.05f, 0.05f));
    CHECK(placed(traced, streak, 0.9f, 1.75f, -2.0f, 0.55f, 0.05f));
    CHECK(renderer->bounds(traced, streak, 1.0f, drawn) == Result::InsufficientCondition);

    // A closed outline ends where it starts: the square's runs counterclockwise from its lower
    // left corner, so the end of the stretch comes down its left side.
    passing.target = node;
    CHECK(traced->emphasize(passing, 1.0f, linear) == Result::Success);
    auto perimeter = traced->object("passing_flash.1.flash");
    CHECK(perimeter && placed(traced, perimeter, 1.9f, -3.5f, 2.75f, 0.05f, 0.55f));

    // A connector flashes where it is drawn, between the edges of its ends, and an untagged
    // target leaves its tag out.
    passing.target = edge;
    passing.window = 0.2f;
    CHECK(traced->emphasize(passing, 1.0f, linear) == Result::Success);
    auto wire = traced->object("passing_flash.2.flash");
    CHECK(wire && wire->layer == edge->layer && placed(traced, wire, 2.5f, 0.0f, 3.0f, 1.05f, 0.05f));
    CHECK(wire && renderer->bounds(traced, wire, 3.0f, drawn) == Result::InsufficientCondition);

    // It may begin while its target travels: the stretch runs along the line where it is when
    // the flash begins, and the travel goes on.
    CHECK(traced->play(Animation::shift(holder, {0.0f, 4.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(traced->seek(3.5f) == Result::Success);
    passing.target = line;
    passing.window = 0.25f;
    CHECK(traced->emphasize(passing, 1.0f, linear) == Result::Success);
    CHECK(near(traced->cursor(), 4.5f));
    auto travelling = traced->object("line.passing_flash.2.flash");
    CHECK(travelling && placed(traced, travelling, 4.0f, 0.0f, -1.0f, 1.05f, 0.05f));
    CHECK(placed(traced, line, 5.0f, 0.0f, 2.0f, 4.05f, 0.05f));

    // A target without an outline of one stroke is refused and adds nothing.
    auto tracedObjects = traced->count();
    auto tracedClips = traced->clipCount();
    CHECK(traced->emphasize(Emphasis::passingFlash(holder)) == Result::InvalidArguments);
    CHECK(traced->diagnostic() && std::strstr(traced->diagnostic(), "no outline of one stroke"));
    CHECK(traced->count() == tracedObjects && traced->clipCount() == tracedClips);
    delete traced;

    // A flash bursts lines out of the target's bounds at even angles from the right,
    // counterclockwise: each starts the padding past where its direction from the center leaves
    // the bounds and runs size along it. Each draws on and away with a window of 2, so over a
    // linear second its drawn part spans 3t - 2 to 3t of the line.
    auto burst = Scene::gen(config);
    auto plate = Rectangle::gen({0.0f, 0.0f, 0.0f}, {2.0f, 1.0f});
    CHECK(burst && plate);
    if (!burst || !plate) {
        delete plate;
        delete burst;
        return;
    }
    plate->tag("plate");
    plate->style.stroke.a = 0;
    plate->fill(Color::hex("#ffffff"));
    CHECK(burst->add(plate) == Result::Success);
    auto bursting = Emphasis::flash(plate);
    CHECK(near(bursting.size, 0.2f) && bursting.count == 12u && near(bursting.padding, 0.1f)
          && near(bursting.width, 3.0f));
    bursting.count = 8u;
    bursting.padding = 0.5f;
    bursting.size = 1.0f;
    bursting.width = 1.0f;
    CHECK(burst->emphasize(bursting, 1.0f, linear) == Result::Success);
    CHECK(burst->count() == 9u && !burst->object("plate.flash.1.9"));
    auto right = burst->object("plate.flash.1.1");
    auto rising = burst->object("plate.flash.1.2");
    auto up = burst->object("plate.flash.1.3");
    CHECK(right && rising && up);
    if (!right || !rising || !up) {
        delete burst;
        return;
    }
    CHECK(placed(burst, right, 1.0f / 6.0f, 1.75f, 0.0f, 0.55f, 0.05f));
    CHECK(placed(burst, right, 0.5f, 2.0f, 0.0f, 1.05f, 0.05f));
    CHECK(placed(burst, right, 5.0f / 6.0f, 2.25f, 0.0f, 0.55f, 0.05f));
    CHECK(placed(burst, up, 0.5f, 0.0f, 1.5f, 0.05f, 1.05f));
    // The diagonal leaves the bounds through the top right corner, 0.5 from the center on
    // each axis.
    CHECK(placed(burst, rising, 0.5f, 1.207f, 1.207f, 0.757f, 0.757f));
    CHECK(renderer->bounds(burst, right, 1.0f, drawn) == Result::InsufficientCondition);

    // A flash may burst out of a point, on layer 0, without a target to name it after.
    auto spark = Emphasis::flash(Vec3{-2.0f, -3.0f, 0.0f});
    spark.count = 3u;
    spark.padding = 0.2f;
    spark.size = 0.4f;
    spark.width = 1.0f;
    CHECK(burst->emphasize(spark, 1.0f, linear) == Result::Success);
    auto first = burst->object("flash.1.1");
    CHECK(first && burst->object("flash.1.3") && first->layer == 0);
    CHECK(first && placed(burst, first, 1.5f, -1.6f, -3.0f, 0.45f, 0.05f));

    // Line counts, sizes and points out of range are refused.
    auto burstObjects = burst->count();
    auto wrong = Emphasis::flash(plate);
    wrong.count = 0u;
    CHECK(burst->emphasize(wrong) == Result::InvalidArguments);
    wrong.count = 65u;
    CHECK(burst->emphasize(wrong) == Result::InvalidArguments);
    wrong = Emphasis::flash(plate);
    wrong.size = 0.0f;
    CHECK(burst->emphasize(wrong) == Result::InvalidArguments);
    CHECK(burst->emphasize(Emphasis::flash(Vec3{NAN, 0.0f, 0.0f})) == Result::InvalidArguments);
    CHECK(burst->diagnostic() && std::strstr(burst->diagnostic(), "from 1 to 64 lines"));
    CHECK(burst->count() == burstObjects);
    delete burst;

    // A ripple grows a ring out from the target's outline by size on every side, its corners
    // with it, while it fades out evenly. The slab, 2 by 1 with corners of 0.2, padded by 0.25
    // and growing by 1 along ease in, gives a ring of 2.5 + 2t^2 by 1.5 + 2t^2 whose corners
    // round by 0.45 + t^2, and whose opacity falls as 1 - t. Its stroke of 4 pixels widens
    // the drawn bounds by 0.2.
    auto rippled = Scene::gen(config);
    auto slab = Rectangle::gen({1.0f, 2.0f, 0.0f}, {2.0f, 1.0f}, 0.2f);
    auto disc = Circle::gen({-2.0f, -2.0f, 0.0f}, 0.5f);
    auto tile = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, 0.25f);
    auto level = Line::gen({-4.0f, 4.0f, 0.0f}, {-2.0f, 4.0f, 0.0f});
    CHECK(rippled && slab && disc && tile && level);
    if (!rippled || !slab || !disc || !tile || !level) {
        delete level;
        delete tile;
        delete disc;
        delete slab;
        delete rippled;
        return;
    }
    slab->tag("slab");
    slab->layer = 3;
    tile->model = Mat4::translate({2.5f, -2.5f, 0.0f}) * Mat4::scale({2.0f, 2.0f, 1.0f});
    Object* rippling[] = {slab, disc, tile};
    for (auto shape : rippling) {
        shape->style.stroke.a = 0;
        shape->fill(Color::hex("#ffffff"));
        CHECK(rippled->add(shape) == Result::Success);
    }
    CHECK(rippled->add(level) == Result::Success);
    auto ripple = Emphasis::ripple(slab);
    CHECK(near(ripple.padding, 0.0f) && near(ripple.size, 0.4f) && near(ripple.width, 3.0f));
    ripple.padding = 0.25f;
    ripple.size = 1.0f;
    ripple.width = 4.0f;
    CHECK(rippled->emphasize(ripple, 1.0f, AnimCurve::preset(AnimCurvePreset::EaseIn)) == Result::Success);
    CHECK(near(rippled->cursor(), 1.0f) && near(rippled->duration(), 1.0f) && rippled->count() == 5u);
    auto ring = static_cast<Rectangle*>(rippled->object("slab.ripple.1.ring"));
    CHECK(ring && ring->type() == Type::Rectangle && !ring->parent() && ring->layer == 3);
    if (!ring) {
        delete rippled;
        return;
    }
    CHECK(near(ring->size.x, 4.5f) && near(ring->size.y, 3.5f) && near(ring->corner, 1.45f));
    CHECK(placed(rippled, ring, 0.0f, 1.0f, 2.0f, 2.7f, 1.7f));
    CHECK(placed(rippled, ring, 0.5f, 1.0f, 2.0f, 3.2f, 2.2f));
    // The right side of the ring, 2.5 and then 2.8125 from the left of the world, shows half and
    // then a quarter of its color's alpha.
    const Object* sampled[] = {ring};
    PixelSample sample;
    CHECK(renderer->sample(rippled, 0.5f, {150.5f, 60.5f}, sampled, 1u, sample) == Result::Success);
    CHECK(sample.object == ring && near(sample.color.a, 127.5f, 3.0f) && near(sample.color.b, 254.0f, 2.0f));
    CHECK(renderer->sample(rippled, 0.75f, {156.5f, 60.5f}, sampled, 1u, sample) == Result::Success);
    CHECK(sample.object == ring && near(sample.color.a, 63.75f, 3.0f));
    // Nothing of it stays.
    CHECK(renderer->bounds(rippled, ring, 1.0f, drawn) == Result::InsufficientCondition);
    CHECK(placed(rippled, slab, 1.0f, 1.0f, 2.0f, 2.0f, 1.0f));

    // A circle ripples as a circle, from its own outline by default, and an untagged target
    // leaves its tag out. A rectangle's corners round as much as its model scales them, and a
    // flat target leaves the ring a sliver to grow from, so a level line ripples into a stadium.
    CHECK(rippled->emphasize(Emphasis::ripple(disc), 1.0f, linear) == Result::Success);
    auto round = static_cast<Rectangle*>(rippled->object("ripple.1.ring"));
    CHECK(round && near(round->size.x, 1.8f) && near(round->size.y, 1.8f) && near(round->corner, 0.9f));
    CHECK(round && placed(rippled, round, 1.5f, -2.0f, -2.0f, 1.55f, 1.55f));
    auto scaled = Emphasis::ripple(tile);
    scaled.size = 0.5f;
    scaled.width = 1.0f;
    CHECK(rippled->emphasize(scaled, 1.0f, linear) == Result::Success);
    auto squared = static_cast<Rectangle*>(rippled->object("ripple.2.ring"));
    CHECK(squared && near(squared->size.x, 3.0f) && near(squared->corner, 1.0f));
    CHECK(squared && placed(rippled, squared, 2.0f, 2.5f, -2.5f, 2.05f, 2.05f));
    scaled.target = level;
    CHECK(rippled->emphasize(scaled, 1.0f, linear) == Result::Success);
    auto stadium = static_cast<Rectangle*>(rippled->object("ripple.3.ring"));
    CHECK(stadium && near(stadium->size.x, 3.0f) && near(stadium->size.y, 1.0f, 1e-3f)
          && near(stadium->corner, 0.5f));

    // It may begin while its target travels, and though its fade runs beside its growth, it
    // leaves the cursor at its end, as a play does.
    CHECK(rippled->play(Animation::shift(disc, {4.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(rippled->seek(4.5f) == Result::Success);
    CHECK(rippled->emphasize(Emphasis::ripple(disc), 1.0f, linear) == Result::Success);
    CHECK(near(rippled->cursor(), 5.5f) && near(rippled->duration(), 6.0f));
    auto wake = rippled->object("ripple.4.ring");
    CHECK(wake && placed(rippled, wake, 4.5f, -1.0f, -2.0f, 1.15f, 1.15f));
    CHECK(placed(rippled, disc, 6.0f, 2.0f, -2.0f, 1.0f, 1.0f));

    // A ripple needs a target with geometry and a positive size, and a refused call adds
    // nothing.
    auto rippledObjects = rippled->count();
    auto rippledClips = rippled->clipCount();
    auto still = Emphasis::ripple(slab);
    still.size = 0.0f;
    CHECK(rippled->emphasize(still) == Result::InvalidArguments);
    CHECK(rippled->emphasize(Emphasis::ripple(nullptr)) == Result::InvalidArguments);
    CHECK(rippled->count() == rippledObjects && rippled->clipCount() == rippledClips);
    delete rippled;
}

// Square the complex coordinate without changing its depth.
static bool _squared(const Vec3& point, Vec3& image, void* data) noexcept
{
    if (data) ++*static_cast<uint32_t*>(data);
    image = {point.x * point.x - point.y * point.y, 2.0f * point.x * point.y, point.z};
    return true;
}

// Lifts every point by the square of its x.
static bool _lifted(const Vec3& point, Vec3& image, void*) noexcept
{
    image = {point.x, point.y + point.x * point.x, point.z};
    return true;
}

// Moves every point by the vector data points at.
static bool _moved(const Vec3& point, Vec3& image, void* data) noexcept
{
    image = point + *static_cast<const Vec3*>(data);
    return true;
}

// Defined only right of the y axis.
static bool _rightHalf(const Vec3& point, Vec3& image, void*) noexcept
{
    if (point.x < 0.0f) return false;
    image = point;
    return true;
}

static bool _unbounded(const Vec3& point, Vec3& image, void*) noexcept
{
    image = {point.x, std::numeric_limits<float>::infinity(), point.z};
    return true;
}

static void warpedGeometry(SwRenderer* renderer)
{
    Config config;
    config.width = 480;
    config.height = 270;
    config.camera.orthoHeight = 9.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto background = pixel(config.background);
    // The pixels around world point (x, y), 30 pixels to a unit from the center of the frame:
    // whether any of the nine nearest shows something, or all of the 25 nearest show nothing.
    auto around = [](const Surface& surface, float x, float y, int reach, auto test) {
        auto column = static_cast<int>(std::lround(240.0f + 30.0f * x));
        auto row = static_cast<int>(std::lround(135.0f - 30.0f * y));
        for (auto dy = -reach; dy <= reach; dy++) {
            for (auto dx = -reach; dx <= reach; dx++) {
                auto value = surface.data()[static_cast<size_t>(row + dy) * surface.stride() + column + dx];
                if (test(value)) return true;
            }
        }
        return false;
    };
    auto lit = [&](const Surface& surface, float x, float y) {
        return around(surface, x, y, 1, [&](uint32_t value) { return value != background; });
    };
    auto dark = [&](const Surface& surface, float x, float y) {
        return !around(surface, x, y, 2, [&](uint32_t value) { return value != background; });
    };
    // Whether the object's pixel bounds at time center on world point (x, y) and measure width
    // by height world units, within tolerance pixels.
    auto placed = [renderer](Scene* scene, Object* object, float time, float x, float y, float width, float height,
                             float tolerance = 1.5f) {
        BBox bounds;
        if (renderer->bounds(scene, object, time, bounds) != Result::Success) return false;
        auto center = bounds.center();
        auto fits = near(center.x, 240.0f + 30.0f * x, tolerance) && near(center.y, 135.0f - 30.0f * y, tolerance)
                    && near(bounds.width, 30.0f * width, 2.0f * tolerance)
                    && near(bounds.height, 30.0f * height, 2.0f * tolerance);
        if (!fits) {
            std::printf("  %.2f s: %.2f x %.2f px at (%.2f, %.2f), expected %.2f x %.2f at (%.2f, %.2f)\n",
                        static_cast<double>(time), static_cast<double>(bounds.width),
                        static_cast<double>(bounds.height), static_cast<double>(center.x),
                        static_cast<double>(center.y), static_cast<double>(30.0f * width),
                        static_cast<double>(30.0f * height), static_cast<double>(240.0f + 30.0f * x),
                        static_cast<double>(135.0f - 30.0f * y));
        }
        return fits;
    };

    // The grid of a space under z -> z^2 lies on the exact image of every line: the line x = 2
    // becomes the parabola (4 - y^2, 4y), the line y = 1 the parabola (x^2 - 1, 2x). Each point
    // travels straight to its image, so halfway it is halfway there, and where the lines stood
    // nothing is left. The map is called once for each point when the call is made, on lines cut
    // into pieces a twentieth of a unit long, and never again.
    auto scene = Scene::gen(config);
    auto plane = Space::gen({-4.0f, 4.0f, 1.0f}, {-3.0f, 3.0f, 1.0f});
    CHECK(scene && plane);
    if (!scene || !plane) {
        delete plane;
        delete scene;
        return;
    }
    plane->stroke(Color::hex("#ffffff"), 2.0f);
    CHECK(scene->add(plane) == Result::Success);
    Object* planes[] = {plane};
    auto calls = 0u;
    CHECK(scene->warp(planes, 1u, _squared, &calls, 1.0f, linear) == Result::Success);
    CHECK(near(scene->cursor(), 1.0f) && scene->clipCount() == 1u);
    // Lines 6, 8 and 10 long hold 121, 161 and 201 points: 8 grid lines across x and 6 across y,
    // then for the 3D view 8 along z at each x, 10 across x at each z, 6 along z at each y and
    // 10 across y at each z, then the three axes.
    auto points = 8u * 121u + 6u * 161u + 8u * 201u + 10u * 161u + 6u * 201u + 10u * 121u + 161u + 121u + 201u;
    CHECK(calls == points);
    Surface surface;
    const float images[][2] = {{3.51f, 2.8f}, {1.56f, 3.2f}, {0.64f, -1.2f}, {-0.64f, -1.2f}, {-0.69f, -2.6f},
                               {3.7975f, -1.8f}};
    const float gone[][2] = {{2.5f, 1.0f}, {-2.5f, -1.0f}, {2.0f, -2.5f}, {-1.5f, 2.0f}, {2.5f, 2.0f}, {-3.5f, 1.0f}};
    const float halfway[][2] = {{2.755f, 1.75f}, {-0.845f, -0.65f}, {-1.805f, 1.6f}};
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    for (auto& point : images) CHECK(dark(surface, point[0], point[1]));
    for (auto& point : gone) CHECK(lit(surface, point[0], point[1]));
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    for (auto& point : halfway) CHECK(lit(surface, point[0], point[1]));
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    for (auto& point : images) CHECK(lit(surface, point[0], point[1]));
    for (auto& point : gone) CHECK(dark(surface, point[0], point[1]));
    CHECK(renderer->render(scene, 3.0f, surface) == Result::Success);
    for (auto& point : images) CHECK(lit(surface, point[0], point[1]));
    CHECK(calls == points);

    // A second warp starts from the first one's images: moving them up by one moves the
    // parabola of x = 2 with them.
    auto up = Vec3{0.0f, 1.0f, 0.0f};
    CHECK(scene->warp(planes, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(lit(surface, 3.51f, 3.8f) && lit(surface, 0.64f, -0.2f) && dark(surface, 0.64f, -1.2f));
    delete scene;

    // An arrow bends with its shaft, here along (x^2 - 1, 2x) from (3, -4) to (3, 4), and its
    // tip keeps its size in pixels and points along the bent end, (4, 2), not the chord.
    scene = Scene::gen(config);
    auto arrow = Arrow::gen({-2.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.0f});
    auto dot = Point::gen({1.0f, 1.0f, 0.0f});
    auto holder = Group::gen();
    auto held = Point::gen({0.5f, 0.5f, 0.0f});
    CHECK(scene && arrow && dot && holder && held);
    if (!scene || !arrow || !dot || !holder || !held) {
        delete held;
        delete holder;
        delete dot;
        delete arrow;
        delete scene;
        return;
    }
    arrow->stroke(Color::hex("#ffffff"), 2.0f);
    arrow->tip = 15.0f;
    dot->style.radius = 6.0f;
    held->style.radius = 6.0f;
    CHECK(holder->add(held) == Result::Success);
    Object* pieces[] = {arrow, dot, holder};
    for (auto piece : pieces) CHECK(scene->add(piece) == Result::Success);
    // The holder shears what it holds first, so the map meets the point at (1, 0.5).
    auto shear = Mat4::identity();
    shear.e[1] = 1.0f;
    CHECK(scene->play(Animation::transform(holder, shear), 1.0f, linear) == Result::Success);
    CHECK(scene->warp(pieces, 3u, _squared, nullptr, 1.0f, linear) == Result::Success);
    CHECK(scene->clipCount() == 4u);
    CHECK(placed(scene, arrow, 2.0f, 1.0f, 0.0f, 4.0f, 8.0f, 3.0f));
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(lit(surface, (321.0f - 240.0f) / 30.0f, (135.0f - 19.5f) / 30.0f));
    CHECK(dark(surface, 3.0f, 3.6f));
    // A point moves to the image of its center and keeps its size: (1, 1) to (0, 2).
    CHECK(placed(scene, dot, 1.0f, 1.0f, 1.0f, 0.4f, 0.4f) && placed(scene, dot, 2.0f, 0.0f, 2.0f, 0.4f, 0.4f));
    CHECK(placed(scene, held, 2.0f, 0.75f, 1.0f, 0.4f, 0.4f));
    delete scene;

    // Later calls start from the images: the bounds and the outline of a lifted line are those
    // of the parabola, and a dot moved along it ends at its lifted end. A circle is lifted as
    // the circle it is, to (cos t, sin t + cos^2 t), which spans -1 to 1.25.
    scene = Scene::gen(config);
    auto line = Line::gen({-1.5f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f});
    auto ring = Circle::gen({0.0f, 0.0f, 0.0f}, 1.0f);
    auto rider = Point::gen({-3.0f, -3.0f, 0.0f});
    CHECK(scene && line && ring && rider);
    if (!scene || !line || !ring || !rider) {
        delete rider;
        delete ring;
        delete line;
        delete scene;
        return;
    }
    rider->style.radius = 6.0f;
    Object* shapes[] = {line, ring, rider};
    for (auto shape : shapes) CHECK(scene->add(shape) == Result::Success);
    Object* lifting[] = {line, ring};
    CHECK(scene->warp(lifting, 2u, _lifted, nullptr, 1.0f, linear) == Result::Success);
    Bounds box;
    CHECK(line->bounds(box) && near(box.min.x, -1.5f) && near(box.max.x, 1.5f) && near(box.min.y, 0.0f)
          && near(box.max.y, 2.25f));
    CHECK(ring->bounds(box) && near(box.min.y, -1.0f, 2.0e-3f) && near(box.max.y, 1.25f, 2.0e-3f));
    CHECK(scene->play(Animation::moveAlong(rider, line), 1.0f, linear) == Result::Success);
    CHECK(placed(scene, rider, 2.0f, 1.5f, 2.25f, 0.4f, 0.4f));
    // Their radius, size and corner no longer change, and a warp waits for those that run.
    auto grow = AnimationTarget::from(ring).radius(2.0f);
    CHECK(scene->play(grow, 1.0f) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "keeps the outline its warp gave"));
    CHECK(scene->update(grow) == Result::InvalidArguments);
    // A ripple around the lifted circle starts from the bounds of its bent outline, which its
    // 3-pixel stroke widens by a tenth of a unit, and a matching transform no longer takes it
    // for a circle: it fades out instead of handing its place and size to the circle that
    // replaces it.
    CHECK(scene->emphasize(Emphasis::ripple(ring), 1.0f, linear) == Result::Success);
    auto ripple = scene->object("ripple.1.ring");
    CHECK(ripple && placed(scene, ripple, 2.001f, 0.0f, 0.125f, 2.1f, 2.35f));
    auto disc = Circle::gen({0.0f, 0.0f, 0.0f}, 1.0f);
    CHECK(disc && scene->add(disc) == Result::Success);
    MatchOptions matching;
    matching.shapes = true;
    CHECK(scene->matchTransform(ring, disc, 1.0f, linear, matching) == Result::Success);
    BBox fading;
    CHECK(renderer->bounds(scene, ring, 3.5f, fading) == Result::Success);
    delete scene;

    // A connector keeps joining its ends wherever a warp takes them, and where they stand
    // before it begins.
    scene = Scene::gen(config);
    Vec3 square[] = {{2.5f, -0.5f, 0.0f}, {3.5f, -0.5f, 0.0f}, {3.5f, 0.5f, 0.0f}, {2.5f, 0.5f, 0.0f}};
    auto left = Rectangle::gen({-3.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto right = Polygon::gen(square, 4u);
    auto wire = left && right ? Connector::gen(left, right) : nullptr;
    auto pair = Group::gen();
    CHECK(scene && left && right && wire && pair);
    if (!scene || !left || !right || !wire || !pair) {
        delete pair;
        delete wire;
        delete right;
        delete left;
        delete scene;
        return;
    }
    wire->padding = 0.0f;
    wire->tail = 0.0f;
    wire->tip = 0.0f;
    CHECK(pair->add(left) == Result::Success && pair->add(right) == Result::Success);
    CHECK(scene->add(pair) == Result::Success && scene->add(wire) == Result::Success);
    Object* paired[] = {pair};
    auto lift = Vec3{0.0f, 2.0f, 0.0f};
    CHECK(scene->wait(1.0f) == Result::Success);
    CHECK(scene->warp(paired, 1u, _moved, &lift, 1.0f, linear) == Result::Success);
    CHECK(scene->clipCount() == 2u);
    CHECK(placed(scene, wire, 0.5f, 0.0f, 0.0f, 5.0f, 0.0f, 2.0f));
    CHECK(placed(scene, wire, 1.5f, 0.0f, 1.0f, 5.0f, 0.0f, 2.0f));
    CHECK(placed(scene, wire, 2.0f, 0.0f, 2.0f, 5.0f, 0.0f, 2.0f));

    // Failures leave the scene as it was.
    auto clips = scene->clipCount();
    auto cursor = scene->cursor();
    auto refused = [&](Result result, const char* reason) {
        auto matches = scene->diagnostic() && std::strstr(scene->diagnostic(), reason);
        if (!matches) std::printf("  diagnostic: %s\n", scene->diagnostic() ? scene->diagnostic() : "(none)");
        return result != Result::Success && matches && scene->clipCount() == clips && near(scene->cursor(), cursor);
    };
    CHECK(refused(scene->warp(paired, 1u, nullptr), "function that maps points"));
    CHECK(refused(scene->warp(nullptr, 0u, _squared), "nothing to animate"));
    CHECK(refused(scene->warp(paired, 1u, _squared, nullptr, 0.0f), "duration"));
    CHECK(refused(scene->warp(paired, 1u, _squared, nullptr, 1.0f, linear, 0.0f), "step"));
    CHECK(refused(scene->warp(paired, 1u, _squared, nullptr, 1.0f, linear, NAN), "step"));
    Object* twice[] = {pair, left};
    CHECK(refused(scene->warp(twice, 2u, _squared), "no target below another"));
    CHECK(refused(scene->warp(paired, 1u, _rightHalf), "no finite image of the point (-3.5, 1.5, 0)"));
    CHECK(refused(scene->warp(paired, 1u, _unbounded), "no finite image"));
    CHECK(refused(scene->warp(paired, 1u, _squared, nullptr, 1.0f, linear, 1.0e-6f), "too many points"));
    CHECK(scene->seek(1.5f) == Result::Success);
    cursor = 1.5f;
    CHECK(refused(scene->warp(paired, 1u, _squared), "already animates its shape until 2.00 s"));
    CHECK(scene->seek(2.0f) == Result::Success);
    cursor = 2.0f;
    Vec3 grid[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    auto sheet = SurfaceMesh::gen(grid, 2u, 2u);
    auto flat = Group::gen();
    auto flattened = Point::gen({1.0f, 1.0f, 0.0f});
    CHECK(sheet && flat && flattened);
    if (sheet && flat && flattened) {
        CHECK(scene->add(sheet) == Result::Success);
        Object* surfaces[] = {sheet};
        CHECK(refused(scene->warp(surfaces, 1u, _squared), "cannot bend cells or surfaces"));
        flat->model = Mat4::scale({1.0f, 0.0f, 1.0f});
        CHECK(flat->add(flattened) == Result::Success && scene->add(flat) == Result::Success);
        Object* flats[] = {flat};
        CHECK(refused(scene->warp(flats, 1u, _squared), "cannot be inverted"));
    } else {
        delete flattened;
        delete flat;
        delete sheet;
    }
    delete scene;
}

// Turns every point a quarter turn about the z axis.
static bool _turned(const Vec3& point, Vec3& image, void*) noexcept
{
    image = {-point.y, point.x, point.z};
    return true;
}

// A space draws a tick across an axis at every multiple of its step along it but the origin,
// centered on the axis, square to it and tickLength pixels long, in the color of the axis. A warp
// carries each tick to the image of its point and turns it square to the axis it bends, and
// leaves it out of the bounds of the space, as it leaves out the numbers.
static void spaceTicks(SwRenderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto red = pixel(Color::hex("#ff0000"));
    auto green = pixel(Color::hex("#00ff00"));
    auto black = pixel(config.background);
    // A scene with a space 30 pixels to a unit, centered, with a red x axis, a green y axis and
    // no grid: the point (x, y) falls on column 120 + 30 x and row 60 - 30 y.
    auto make = [&](const Vec3& ticks, float length, Space*& space) -> Scene* {
        auto scene = Scene::gen(config);
        space = Space::gen({-3.0f, 3.0f, 1.0f}, {-1.5f, 1.5f, 0.5f}, {-1.0f, 1.0f, 1.0f});
        if (scene && space) {
            space->style.stroke.a = 0;
            space->axisX = Color::hex("#ff0000");
            space->axisY = Color::hex("#00ff00");
            space->ticks = ticks;
            space->tickLength = length;
            if (scene->add(space) == Result::Success) return scene;
        }
        delete space;
        delete scene;
        space = nullptr;
        return nullptr;
    };
    auto at = [](const Surface& surface, uint32_t x, uint32_t y) {
        return surface.data() ? surface.data()[static_cast<size_t>(y) * surface.stride() + x] : 0u;
    };
    auto image = [&](const Vec3& ticks, float length) {
        Space* space = nullptr;
        auto scene = make(ticks, length, space);
        Surface surface;
        auto sum = scene && renderer->render(scene, 0.0f, surface) == Result::Success ? checksum(surface) : 0u;
        delete scene;
        return sum;
    };

    // Axis width is independent of the grid, reaches ticks and warped axes, and grows
    // the drawn bounds while the space's coordinate bounds stay fixed.
    {
        Space* wide = nullptr;
        auto axes = make({1.0f, 0.5f, 0.0f}, 10.0f, wide);
        CHECK(axes && wide);
        if (axes && wide) {
            Surface frame;
            BBox thin, thick;
            CHECK(renderer->bounds(axes, wide, 0.0f, thin) == Result::Success);
            wide->axisWidth = 6.0f;
            CHECK(axes->update(wide) == Result::Success);
            CHECK(renderer->render(axes, 0.0f, frame) == Result::Success);
            CHECK(at(frame, 165u, 58u) == red && at(frame, 182u, 56u) == red);
            CHECK(renderer->bounds(axes, wide, 0.0f, thick) == Result::Success);
            CHECK(thick.width > thin.width + 3.0f && thick.height > thin.height + 3.0f);
            Bounds box;
            CHECK(wide->bounds(box) && near(box.max.x, 3.0f));
            Object* targets[] = {wide};
            auto up = Vec3{0.0f, 1.0f, 0.0f};
            CHECK(axes->warp(targets, 1u, _moved, &up, 1.0f, linear) == Result::Success);
            CHECK(renderer->render(axes, 1.0f, frame) == Result::Success);
            CHECK(at(frame, 165u, 28u) == red && at(frame, 182u, 26u) == red);
        }
        delete axes;
        axes = make({1.0f, 0.5f, 0.0f}, 10.0f, wide);
        wide->axisWidth = 0.0f;
        CHECK(axes->update(wide) == Result::Success);
        Surface hidden;
        CHECK(renderer->render(axes, 0.0f, hidden) == Result::Success);
        CHECK(changed(hidden, config.background) == 0u);
        delete axes;
        auto invalid = Space::gen();
        auto owner = Scene::gen();
        invalid->axisWidth = -2.0f;
        CHECK(owner->add(invalid) == Result::InvalidArguments);
        invalid->axisWidth = std::numeric_limits<float>::infinity();
        CHECK(owner->add(invalid) == Result::InvalidArguments);
        invalid->axisWidth = 0.0f;
        CHECK(owner->add(invalid) == Result::Success);
        delete owner;
    }

    Space* space = nullptr;
    auto scene = make({1.0f, 0.5f, 1.0f}, 10.0f, space);
    CHECK(scene != nullptr);
    if (!scene) return;
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    // The x ticks at -3 to 3, the ends of the axis among them, stand 10 pixels tall across row
    // 60, and none between them.
    const uint32_t columns[] = {30u, 60u, 90u, 150u, 180u, 210u};
    for (auto column : columns) {
        CHECK(at(surface, column, 57u) == red && at(surface, column, 63u) == red);
        CHECK(at(surface, column, 68u) == black && at(surface, column + 6u, 57u) == black);
    }
    // The y ticks at -1.5 to 1.5 lie 10 pixels wide across column 120.
    const uint32_t rows[] = {15u, 30u, 45u, 75u, 90u, 105u};
    for (auto row : rows) {
        CHECK(at(surface, 117u, row) == green && at(surface, 123u, row) == green);
        CHECK(at(surface, 112u, row) == black && at(surface, 117u, row + 7u) == black);
    }
    // None at the origin, where the green y axis shows, and none along z in the 2D view.
    CHECK(at(surface, 120u, 57u) == green && at(surface, 120u, 63u) == green);
    auto plain = image({}, 10.0f);
    CHECK(plain != 0u && plain != checksum(surface));
    CHECK(image({1.0f, 0.5f, 0.0f}, 0.0f) == plain);
    CHECK(image({0.0f, 0.0f, 1.0f}, 10.0f) == plain);

    // A warp carries each tick with its axis, here up a unit, so x = 2 crosses row 30, and the
    // bounds stay those of the same space without ticks.
    Space* bare = nullptr;
    auto unticked = make({}, 10.0f, bare);
    CHECK(unticked != nullptr);
    Object* targets[] = {space};
    Object* bareTargets[] = {bare};
    auto up = Vec3{0.0f, 1.0f, 0.0f};
    CHECK(scene->warp(targets, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(at(surface, 180u, 27u) == red && at(surface, 180u, 33u) == red && at(surface, 180u, 57u) == black);
    CHECK(at(surface, 117u, 45u) == green && at(surface, 117u, 105u) == black);
    if (unticked) {
        CHECK(unticked->warp(bareTargets, 1u, _moved, &up, 1.0f, linear) == Result::Success);
        Bounds ticked;
        Bounds plainBox;
        CHECK(space->bounds(ticked) && bare->bounds(plainBox));
        CHECK(near(ticked.min.x, plainBox.min.x) && near(ticked.max.x, plainBox.max.x) && near(ticked.max.x, 3.0f)
              && near(ticked.min.y, plainBox.min.y) && near(ticked.max.y, plainBox.max.y));
    }
    delete unticked;
    delete scene;

    // A quarter turn stands the x axis up along column 120, and its tick at x = 1 now lies
    // across it at row 30, where the y tick at y = 1 lay; that one stands across the turned y
    // axis at column 90, where the x tick at x = -1 stood.
    scene = make({1.0f, 0.5f, 0.0f}, 10.0f, space);
    CHECK(scene != nullptr);
    if (!scene) return;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(at(surface, 116u, 30u) == green && at(surface, 90u, 57u) == red);
    targets[0] = space;
    CHECK(scene->warp(targets, 1u, _turned, nullptr, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(at(surface, 116u, 30u) == red && at(surface, 124u, 30u) == red && at(surface, 120u, 26u) == red);
    CHECK(at(surface, 90u, 57u) == green && at(surface, 90u, 63u) == green && at(surface, 94u, 57u) == black);
    delete scene;

    // Ticks are lengths and steps: a negative or infinite one fails.
    auto invalid = Scene::gen(config);
    auto wrong = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    CHECK(invalid && wrong);
    if (invalid && wrong) {
        wrong->ticks.y = -0.5f;
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
        wrong->ticks = {std::numeric_limits<float>::infinity(), 0.0f, 0.0f};
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
        wrong->ticks = {1.0f, 1.0f, 0.0f};
        wrong->tickLength = -1.0f;
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
        wrong->tickLength = std::numeric_limits<float>::quiet_NaN();
        CHECK(invalid->add(wrong) == Result::InvalidArguments);
    }
    delete wrong;
    delete invalid;
}

// A space places a label beside the tick at a value: its bounds center on the line from the
// tick, below the x axis and left of the y axis unless told otherwise, with the side that faces
// the tick tickLength / 2 + gap pixels from the axis, through the transforms of the space and of
// the label's parent. A text without a box pins the side of its glyphs that faces the tick.
static void spaceLabels()
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    // 30 pixels to a unit, so the default tick and gap put a label 8 pixels from its axis.
    constexpr auto away = 8.0f / 30.0f;
    auto centered = [](const Object* object, float x, float y) {
        Bounds box;
        if (!object->bounds(box)) return false;
        auto center = box.center();
        auto fits = near(center.x, x, 1.0e-4f) && near(center.y, y, 1.0e-4f);
        if (!fits) {
            std::printf("  centered at (%.5f, %.5f), expected (%.5f, %.5f)\n", static_cast<double>(center.x),
                        static_cast<double>(center.y), static_cast<double>(x), static_cast<double>(y));
        }
        return fits;
    };
    auto scene = Scene::gen(config);
    auto space = Space::gen({-3.0f, 3.0f, 1.0f}, {-1.5f, 1.5f, 0.5f});
    auto below = Text::gen("a");
    auto right = Text::gen("b");
    auto boxed = Text::gen("c");
    auto bar = Rectangle::gen({}, {1.0f, 0.5f});
    auto root = Rectangle::gen({5.0f, 5.0f, 0.0f}, {1.0f, 0.5f});
    CHECK(scene && space && below && right && boxed && bar && root);
    if (!scene || !space || !below || !right || !boxed || !bar || !root) {
        delete root;
        delete bar;
        delete boxed;
        delete right;
        delete below;
        delete space;
        delete scene;
        return;
    }
    boxed->box = {0.6f, 0.3f};
    CHECK(scene->add(space) == Result::Success);
    Object* children[] = {below, right, boxed, bar};
    for (auto child : children) CHECK(space->add(child) == Result::Success);
    CHECK(scene->add(root) == Result::Success);
    // A text hangs from its top below the x axis, and stands by its left side right of the y axis.
    CHECK(space->place(below, 0u, 2.0f) == Result::Success);
    CHECK(centered(below, 2.0f, -away) && near(below->align.x, 0.5f) && near(below->align.y, 0.0f));
    CHECK(space->place(right, 1u, 1.5f, {1.0f, 0.0f, 0.0f}) == Result::Success);
    CHECK(centered(right, away, 1.5f) && near(right->align.x, 0.0f) && near(right->align.y, 0.5f));
    // A text with a box and any other object place their bounds; the box keeps its align.
    CHECK(space->place(boxed, 1u, -1.0f) == Result::Success);
    CHECK(centered(boxed, -away - 0.3f, -1.0f) && near(boxed->align.x, 0.5f) && near(boxed->align.y, 0.5f));
    CHECK(space->place(bar, 0u, -1.0f, {0.0f, 2.0f, 0.0f}, 10.0f) == Result::Success);
    CHECK(centered(bar, -1.0f, 14.0f / 30.0f + 0.25f));
    // The space's transform carries the tick, here to (3, 0.5), and its scale does not stretch
    // the distance, which is in pixels: a label at the root moves in the world, and one below
    // the space in the space, where the 8 pixels are half as long.
    space->model = Mat4::translate({1.0f, 0.5f, 0.0f}) * Mat4::scale({2.0f, 2.0f, 1.0f});
    CHECK(scene->update(space) == Result::Success);
    CHECK(space->place(root, 0u, 1.0f) == Result::Success);
    CHECK(centered(root, 3.0f, 0.5f - away - 0.25f));
    CHECK(space->place(bar, 0u, 1.0f) == Result::Success);
    CHECK(centered(bar, 1.0f, -away * 0.5f - 0.25f));
    // A text not attached yet is placed in the space, to be added below it; one inside a parent
    // that is not attached yet is refused.
    auto fresh = Text::gen("d");
    CHECK(fresh && space->place(fresh, 0u, -2.0f) == Result::Success);
    if (fresh) {
        CHECK(centered(fresh, -2.0f, -away * 0.5f) && near(fresh->align.x, 0.5f) && near(fresh->align.y, 0.0f));
        CHECK(space->add(fresh) == Result::Success);
    }
    // A gap of zero and no tick length leave the label touching the axis.
    space->tickLength = 0.0f;
    CHECK(scene->update(space) == Result::Success);
    CHECK(space->place(bar, 1u, 1.0f, {}, 0.0f) == Result::Success);
    CHECK(centered(bar, -0.5f, 1.0f));

    // Refusals leave the label where it was.
    Bounds before;
    CHECK(root->bounds(before));
    auto refused = [&](Result result, Result expected, const char* reason) {
        auto said = scene->diagnostic() && std::strstr(scene->diagnostic(), reason);
        if (!said) std::printf("  diagnostic: %s\n", scene->diagnostic() ? scene->diagnostic() : "(none)");
        return result == expected && said;
    };
    CHECK(refused(space->place(root, 3u, 1.0f), Result::InvalidArguments, "axis must be 0, 1 or 2"));
    CHECK(refused(space->place(root, 0u, std::numeric_limits<float>::quiet_NaN()), Result::InvalidArguments,
                  "value finite"));
    CHECK(refused(space->place(root, 0u, 1.0f, {}, -1.0f), Result::InvalidArguments, "gap"));
    CHECK(refused(space->place(root, 0u, 1.0f, {0.0f, std::numeric_limits<float>::infinity(), 0.0f}),
                  Result::InvalidArguments, "direction"));
    CHECK(refused(space->place(space, 0u, 1.0f), Result::InvalidArguments, "not the space or one above it"));
    CHECK(refused(space->place(nullptr, 0u, 1.0f), Result::InvalidArguments, "label must be another object"));
    auto holding = Group::gen();
    auto held = Text::gen("e");
    CHECK(holding && held && holding->add(held) == Result::Success);
    if (holding && held) CHECK(refused(space->place(held, 0u, 1.0f), Result::InvalidArguments, "no parent"));
    delete holding;
    auto empty = Group::gen();
    CHECK(empty && scene->add(empty) == Result::Success);
    if (empty) CHECK(refused(space->place(empty, 0u, 1.0f), Result::InsufficientCondition, "no bounds"));
    // A space squashed flat along y cannot set a label below its x axis.
    auto kept = space->model;
    space->model = Mat4::scale({1.0f, 0.0f, 1.0f});
    CHECK(refused(space->place(root, 0u, 1.0f), Result::InvalidArguments, "flatten"));
    space->model = kept;
    Bounds after;
    CHECK(root->bounds(after) && near(after.center().x, before.center().x) && near(after.center().y, before.center().y));
    // Once the timeline begins, an attached text can no longer be placed and keeps its align,
    // while one not attached yet still can.
    CHECK(scene->play(Animation::fadeIn(right), 0.5f) == Result::Success);
    CHECK(refused(space->place(right, 0u, 1.0f), Result::InsufficientCondition, "before the first play"));
    CHECK(near(right->align.x, 0.0f) && near(right->align.y, 0.5f));
    auto late = Text::gen("f");
    CHECK(late && space->place(late, 1u, 1.0f) == Result::Success && space->add(late) == Result::Success);
    if (late) CHECK(centered(late, -2.0f / 30.0f, 1.0f) && near(late->align.x, 1.0f) && near(late->align.y, 0.5f));
    delete scene;

    // A space cannot place the group above it, nor an object of another scene, and a space
    // that is not attached cannot tell how long a pixel is.
    scene = Scene::gen(config);
    auto other = Scene::gen(config);
    auto holder = Group::gen();
    auto inner = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    auto foreign = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && other && holder && inner && foreign);
    if (scene && other && holder && inner && foreign) {
        CHECK(holder->add(inner) == Result::Success && scene->add(holder) == Result::Success);
        CHECK(other->add(foreign) == Result::Success);
        CHECK(refused(inner->place(holder, 0u, 1.0f), Result::InvalidArguments, "one above it"));
        CHECK(inner->place(foreign, 0u, 1.0f) == Result::InvalidArguments);
        auto detached = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
        auto mark = Text::gen("g");
        CHECK(detached && mark && detached->place(mark, 0u, 1.0f) == Result::InsufficientCondition);
        delete mark;
        delete detached;
    } else {
        delete foreign;
        delete inner;
        delete holder;
    }
    delete other;
    delete scene;
}

// A space passes its opacity and creation progress to the labels it placed below it, at any
// depth, as a group passes them to its children, also when it draws into a layer of its own;
// the other objects below it and the labels it placed elsewhere keep their own.
static void spaceScreenLabels(SwRenderer* renderer)
{
#ifdef VIS_TEST_FONT
    constexpr auto pi = 3.14159265359f;
    Config config;
    config.width = 960;
    config.height = 540;
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.fov = 42.0f * pi / 180.0f;
    config.camera.up = {0.0f, 0.0f, 1.0f};
    auto polar = 65.0f * pi / 180.0f;
    auto eye = [polar](float azimuth) {
        return Vec3{8.0f * std::sin(polar) * std::cos(azimuth),
                    8.0f * std::sin(polar) * std::sin(azimuth), 8.0f * std::cos(polar)};
    };
    config.camera.eye = eye(-pi / 3.0f);
    auto scene = Scene::gen(config);
    auto space = Space::gen({-3.0f, 3.0f, 1.0f}, {-3.0f, 3.0f, 1.0f}, {-2.0f, 2.0f, 1.0f});
    CHECK(scene && space && scene->add(space) == Result::Success);
    if (!scene || !space) { delete scene; return; }
    Text* labels[6];
    const char* names[] = {"x", "y", "z"};
    for (auto i = 0u; i < 6u; i++) {
        labels[i] = Text::gen(names[i / 2u], {0.4f, 0.2f, 0.0f});
        labels[i]->size = 18.0f;
        labels[i]->slant = 0.25f;
        CHECK(space->placeScreen(labels[i], static_cast<uint8_t>(i / 2u),
                                 (i % 2u ? 1.0f : -1.0f) * (i < 4u ? 3.0f : 2.0f), 10.0f) == Result::Success);
        CHECK(space->add(labels[i]) == Result::Success);
    }
    auto centered = Text::gen("x");
    centered->size = 18.0f;
    CHECK(space->add(centered) == Result::Success);
    CHECK(space->placeScreen(centered, 0u, 0.0f, 10.0f) == Result::Success);
    LayoutReport initial;
    CHECK(renderer->layout(scene, 0.0f, initial) == Result::Success);
    auto centerVisual = _layoutVisual(initial, centered);
    CHECK(centerVisual && near(centerVisual->paintBounds.y + centerVisual->paintBounds.height, 256.0f, 0.02f));
    // World placement removes the retained screen placement, leaving the ordinary layout.
    auto ordinary = Text::gen("x");
    ordinary->size = 18.0f;
    CHECK(space->add(ordinary) == Result::Success);
    CHECK(space->place(centered, 0u, 0.0f) == Result::Success);
    CHECK(space->place(ordinary, 0u, 0.0f) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, initial) == Result::Success);
    centerVisual = _layoutVisual(initial, centered);
    auto ordinaryVisual = _layoutVisual(initial, ordinary);
    CHECK(centerVisual && ordinaryVisual && near(centerVisual->paintBounds.x, ordinaryVisual->paintBounds.x)
          && near(centerVisual->paintBounds.y, ordinaryVisual->paintBounds.y));
    auto root = Text::gen("root");
    CHECK(scene->add(root) == Result::Success);
    CHECK(space->placeScreen(root, 0u, 1.0f) == Result::InvalidArguments);
    CHECK(space->placeScreen(labels[0], 3u, 1.0f) == Result::InvalidArguments);
    CHECK(space->placeScreen(labels[0], 0u, 1.0f, -1.0f) == Result::InvalidArguments);
    auto other = Scene::gen(config);
    auto foreign = Text::gen("foreign");
    CHECK(other->add(foreign) == Result::Success);
    CHECK(space->placeScreen(foreign, 0u, 1.0f) == Result::InvalidArguments);
    delete other;
    auto plane = Text::gen("plane");
    plane->orientation = TextOrientation::Plane;
    CHECK(space->placeScreen(plane, 0u, 1.0f) == Result::InvalidArguments);
    delete plane;
    CHECK(scene->orbit(pi * 0.5f, {0.0f, 0.0f, 1.0f}, 2.0f, Easing::Linear) == Result::Success);
    CHECK(space->placeScreen(labels[0], 0u, 1.0f) == Result::InsufficientCondition);
    auto grow = AnimationTarget::from(labels[0]);
    grow.size(28.0f);
    CHECK(scene->play(&grow, 1u, 1.0f, Easing::Linear) == Result::Success);
    const float times[] = {0.0f, 1.25f, 0.2f, 2.0f, 2.5f, 3.0f, 1.25f};
    for (auto time : times) {
        auto camera = config.camera;
        camera.eye = eye(-pi / 3.0f + pi * 0.5f * std::fmin(time / 2.0f, 1.0f));
        for (auto i = 0u; i < 6u; i++) {
            Vec3 at;
            auto value = (i % 2u ? 1.0f : -1.0f) * (i < 4u ? 3.0f : 2.0f);
            if (i / 2u == 0u) at.x = value;
            else if (i / 2u == 1u) at.y = value;
            else at.z = value;
            Vec3 ndc;
            CHECK(camera.project(at, 960.0f / 540.0f, ndc));
            Vec2 anchor{(ndc.x + 1.0f) * 480.0f, (1.0f - ndc.y) * 270.0f};
            auto direction = anchor - Vec2{480.0f, 270.0f};
            direction = direction / direction.length();
            LayoutReport report;
            CHECK(renderer->layout(scene, time, report) == Result::Success);
            auto visual = _layoutVisual(report, labels[i]);
            CHECK(visual && visual->visible);
            if (!visual) continue;
            auto box = visual->paintBounds;
            auto center = box.center();
            auto support = std::fabs(direction.x) * box.width * 0.5f
                         + std::fabs(direction.y) * box.height * 0.5f;
            auto gap = (center.x - anchor.x) * direction.x + (center.y - anchor.y) * direction.y - support;
            CHECK(near(gap, 14.0f, 0.02f));
        }
    }
    Object* copy = nullptr;
    CHECK(scene->clone(labels[1], copy) == Result::Success && copy && copy->parent() == space);
    auto late = Text::gen("late");
    CHECK(space->placeScreen(late, 0u, 3.0f, 10.0f) == Result::Success);
    CHECK(space->add(late) == Result::Success);
    LayoutReport final;
    CHECK(renderer->layout(scene, 3.0f, final) == Result::Success);
    auto originalVisual = _layoutVisual(final, labels[1]);
    auto copyVisual = _layoutVisual(final, copy);
    auto lateVisual = _layoutVisual(final, late);
    CHECK(originalVisual && copyVisual && lateVisual && lateVisual->visible
          && near(originalVisual->paintBounds.x, copyVisual->paintBounds.x)
          && near(originalVisual->paintBounds.y, copyVisual->paintBounds.y));
    delete scene;
    // An origin behind the eye cannot supply an outward projection, but a visible anchor
    // still gets its gap in the deterministic upward fallback direction.
    config.camera.eye = {0.0f, 0.0f, 10.0f};
    config.camera.up = {0.0f, 1.0f, 0.0f};
    scene = Scene::gen(config);
    space = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {-3.0f, -1.0f, 1.0f});
    space->model = Mat4::translate({0.0f, 0.0f, 11.0f});
    auto clippedOrigin = Text::gen("x");
    clippedOrigin->size = 18.0f;
    CHECK(scene->add(space) == Result::Success);
    CHECK(space->placeScreen(clippedOrigin, 2u, -3.0f, 10.0f) == Result::Success);
    CHECK(space->add(clippedOrigin) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, initial) == Result::Success);
    centerVisual = _layoutVisual(initial, clippedOrigin);
    CHECK(centerVisual && centerVisual->visible
          && near(centerVisual->paintBounds.y + centerVisual->paintBounds.height, 256.0f, 0.02f));
    delete scene;
#else
    (void) renderer;
#endif
}

static void spaceScreenLabelWarps(SwRenderer* renderer)
{
#ifdef VIS_TEST_FONT
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    for (auto nested : {false, true}) {
        auto scene = Scene::gen(config);
        auto space = Space::gen({-2.0f, 2.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
        auto label = Text::gen("x");
        label->size = 18.0f;
        if (nested) {
            auto group = Group::gen();
            group->model = Mat4::translate({1.0f, 0.0f, 0.0f}) * Mat4::scale({2.0f, 2.0f, 1.0f});
            CHECK(group->add(space) == Result::Success && scene->add(group) == Result::Success);
        } else CHECK(scene->add(space) == Result::Success);
        CHECK(space->placeScreen(label, 0u, -2.0f, 10.0f) == Result::Success);
        CHECK(space->add(label) == Result::Success);
        Object* targets[] = {space};
        auto right = Vec3{3.0f, 0.0f, 0.0f};
        auto left = Vec3{-1.0f, 0.0f, 0.0f};
        CHECK(scene->warp(targets, 1u, _moved, &right, 1.0f, linear) == Result::Success);
        CHECK(scene->warp(targets, 1u, _moved, &left, 1.0f, linear) == Result::Success);
        CHECK(scene->warp(targets, 1u, _squared, nullptr, 1.0f, linear) == Result::Success);
        for (auto time : {0.0f, 1.0f, 0.75f, 0.25f, 2.0f, 1.5f, 3.0f, 2.5f, 1.0f}) {
            auto x = -2.0f + 3.0f * std::fmin(time, 1.0f)
                   - std::fmin(std::fmax(time - 1.0f, 0.0f), 1.0f);
            auto anchor = 160.0f + 30.0f * ((nested ? 1.0f : 0.0f) + (nested ? 2.0f : 1.0f) * x);
            LayoutReport report;
            CHECK(renderer->layout(scene, time, report) == Result::Success);
            auto visual = _layoutVisual(report, label);
            CHECK(visual && visual->visible);
            if (visual) {
                CHECK(near(visual->paintBounds.x + visual->paintBounds.width, anchor - 14.0f, 0.02f));
                CHECK(near(visual->paintBounds.center().y, 90.0f, 0.02f));
            }
        }
        auto late = Text::gen("late");
        CHECK(space->placeScreen(late, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "before the space's first warp"));
        CHECK(space->placeScreen(label, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
        CHECK(scene->seek(0.0f) == Result::Success);
        CHECK(space->placeScreen(late, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
        delete late;
        delete scene;
    }
    auto scene = Scene::gen(config);
    auto space = Space::gen({2.0f, 3.0f, 1.0f}, {2.0f, 3.0f, 1.0f}, {2.0f, 3.0f, 1.0f});
    auto label = Text::gen("x");
    label->size = 18.0f;
    CHECK(scene->add(space) == Result::Success);
    CHECK(space->placeScreen(label, 0u, 2.0f) == Result::Success && space->add(label) == Result::Success);
    auto radial = [](const Vec3& point, Vec3& image, void*) noexcept {
        image = {point.dot(point) + 3.0f, point.y, point.z};
        return true;
    };
    Object* targets[] = {space};
    CHECK(scene->warp(targets, 1u, radial, nullptr, 1.0f, linear) == Result::Success);
    Bounds bounds;
    CHECK(space->bounds(bounds) && near(bounds.min.x, 7.0f));
    delete scene;
    // A space warped without screen labels never captured their origin. World labels can
    // still be placed later, while screen placement must fail before changing the text.
    scene = Scene::gen(config);
    space = Space::gen({-2.0f, 2.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    CHECK(scene->add(space) == Result::Success);
    auto right = Vec3{3.0f, 0.0f, 0.0f};
    targets[0] = space;
    CHECK(scene->warp(targets, 1u, _moved, &right, 1.0f, linear) == Result::Success);
    auto late = Text::gen("late");
    CHECK(space->placeScreen(late, 0u, -2.0f, 10.0f) == Result::InsufficientCondition);
    CHECK(space->place(late, 0u, -2.0f) == Result::Success && space->add(late) == Result::Success);
    delete scene;
#else
    (void) renderer;
#endif
}

static void spaceLabelOpacity(SwRenderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    // 30 pixels to a unit: the point (x, y) falls on column 120 + 30 x and row 60 - 30 y. The
    // red channel of a white fill shows how much of it is drawn.
    auto red = [](const Surface& surface, float x, float y) -> uint32_t {
        auto column = static_cast<size_t>(120.0f + 30.0f * x);
        auto row = static_cast<size_t>(60.0f - 30.0f * y);
        return surface.data() ? surface.data()[row * surface.stride() + column] & 0xffu : 0u;
    };
    auto half = [](uint32_t value) { return value >= 120u && value <= 136u; };
    // A label sits 8 pixels, 4 / 15 of a unit, from its axis, and its box half as far again.
    constexpr auto away = 8.0f / 30.0f + 0.2f;
    const bool layers[] = {false, true};
    for (auto isolated : layers) {
        auto scene = Scene::gen(config);
        auto space = Space::gen({-3.0f, 3.0f, 1.0f}, {-1.5f, 1.5f, 0.5f});
        auto holder = Group::gen();
        auto label = Rectangle::gen({}, {0.6f, 0.4f});
        auto deep = Rectangle::gen({}, {0.6f, 0.4f});
        auto fresh = Rectangle::gen({}, {0.6f, 0.4f});
        auto other = Rectangle::gen({-2.0f, 0.8f, 0.0f}, {0.6f, 0.4f});
        auto loose = Rectangle::gen({}, {0.6f, 0.4f});
        CHECK(scene && space && holder && label && deep && fresh && other && loose);
        if (!scene || !space || !holder || !label || !deep || !fresh || !other || !loose) {
            delete loose;
            delete other;
            delete fresh;
            delete deep;
            delete label;
            delete holder;
            delete space;
            delete scene;
            return;
        }
        // The space draws nothing of its own, so only the rectangles show.
        space->style.stroke.a = 0;
        space->axisX.a = 0;
        space->axisY.a = 0;
        Rectangle* rectangles[] = {label, deep, fresh, other, loose};
        for (auto rectangle : rectangles) {
            rectangle->style.stroke.a = 0;
            rectangle->fill(Color::hex("#ffffff"));
        }
        CHECK(scene->add(space) == Result::Success && space->add(label) == Result::Success);
        CHECK(space->add(holder) == Result::Success && holder->add(deep) == Result::Success);
        CHECK(space->add(other) == Result::Success && scene->add(loose) == Result::Success);
        if (isolated) CHECK(space->isolate() == Result::Success);
        // Labels below the space, one inside a group and one placed before it was attached, and
        // a label at the root, which stays the root's.
        CHECK(space->place(label, 0u, 2.0f) == Result::Success);
        CHECK(space->place(deep, 0u, 0.0f) == Result::Success);
        CHECK(space->place(fresh, 0u, -2.0f) == Result::Success && space->add(fresh) == Result::Success);
        CHECK(space->place(loose, 1u, 1.0f) == Result::Success);
        CHECK(scene->play(Animation::fadeIn(space), 1.0f, Easing::Linear) == Result::Success);
        CHECK(scene->play(Animation::uncreate(space), 1.0f, Easing::Linear) == Result::Success);
        Surface surface;
        // Halfway through the fade the labels below the space show at half strength.
        CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
        CHECK(half(red(surface, 2.0f, -away)) && half(red(surface, 0.0f, -away)) && half(red(surface, -2.0f, -away)));
        CHECK(red(surface, -2.0f, 0.8f) == 255u && red(surface, -away - 0.1f, 1.0f) == 255u);
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        CHECK(red(surface, 2.0f, -away) == 255u && red(surface, 0.0f, -away) == 255u);
        // Once the space is uncreated, its labels are gone with it, and the rest stays.
        CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
        CHECK(red(surface, 2.0f, -away) == 0u && red(surface, 0.0f, -away) == 0u && red(surface, -2.0f, -away) == 0u);
        CHECK(red(surface, -2.0f, 0.8f) == 255u && red(surface, -away - 0.1f, 1.0f) == 255u);
        delete scene;
    }
}

// The cursor reckons each wait and play from the decimals a script writes, so a wait of 0.5 and
// twenty-nine of 0.3 end on the float nearest 9.2, which frame 276 samples at 30 frames a second,
// and what starts there shows on that frame. Added up as floats they ended three ulps late, and it
// showed a frame late. A play of items that begin a lag apart moves the cursor as far.
static void cursorDecimals(SwRenderer* renderer)
{
    Config config;
    config.width = 40;
    config.height = 20;
    config.fps = 30;
    config.camera.orthoHeight = 2.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto card = Rectangle::gen({}, {8.0f, 4.0f});
    Object* dots[] = {Circle::gen({-1.0f, 0.6f, 0.0f}, 0.1f), Circle::gen({0.0f, 0.6f, 0.0f}, 0.1f),
                      Circle::gen({1.0f, 0.6f, 0.0f}, 0.1f)};
    CHECK(scene && card && dots[0] && dots[1] && dots[2]);
    if (!scene || !card || !dots[0] || !dots[1] || !dots[2]) {
        for (auto dot : dots) delete dot;
        delete card;
        delete scene;
        return;
    }
    card->style.stroke.a = 0;
    card->fill(Color::hex("#ff0000"));
    CHECK(scene->add(card) == Result::Success);
    for (auto dot : dots) CHECK(scene->add(dot) == Result::Success);
    CHECK(scene->wait(0.5f) == Result::Success);
    for (auto i = 0; i < 29; i++) CHECK(scene->wait(0.3f) == Result::Success);
    CHECK(scene->cursor() == 9.2f);
    CHECK(scene->update(AnimationTarget::from(card).fill(Color::hex("#ffffff"))) == Result::Success);
    Surface surface;
    auto middle = [&](uint32_t frame) {
        auto time = static_cast<float>(frame) / static_cast<float>(config.fps);
        if (renderer->render(scene, time, surface) != Result::Success || !surface.data()) return 0u;
        return surface.data()[10u * surface.stride() + 20u];
    };
    CHECK(middle(275u) == pixel(Color::hex("#ff0000")));
    CHECK(middle(276u) == pixel(Color::hex("#ffffff")));
    // Three fades of 0.3 that begin 0.4 of it apart end 0.54 on.
    Animation fades[] = {Animation::fade(dots[0], 0.5f), Animation::fade(dots[1], 0.5f), Animation::fade(dots[2], 0.5f)};
    CHECK(scene->play(fades, 3u, 0.3f, Easing::Linear, 0.4f) == Result::Success);
    CHECK(scene->cursor() == 9.74f);
    delete scene;
}

static void morphAnimations(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;

    Vec3 sourcePoints[] = {
        {-1.8f, -0.5f, 0.0f}, {-0.8f, -0.5f, 0.0f},
        {-0.8f, 0.5f, 0.0f}, {-1.8f, 0.5f, 0.0f}};
    Vec3 targetPoints[] = {
        {1.0f, -0.9f, 0.0f}, {1.8f, 0.0f, 0.0f},
        {1.0f, 0.9f, 0.0f}, {0.2f, 0.0f, 0.0f}};
    auto scene = Scene::gen(config);
    auto source = Polygon::gen(sourcePoints, 4u);
    auto target = Polygon::gen(targetPoints, 4u);
    CHECK(scene && source && target);
    if (scene && source && target) {
        auto sourceColor = Color::hex("#ef5350");
        auto targetColor = Color::hex("#42a5f5");
        source->style.stroke.a = 0;
        source->fill(sourceColor);
        target->style.stroke.a = 0;
        target->fill(targetColor);
        auto sourceResult = scene->add(source);
        auto targetResult = scene->add(target);
        CHECK(sourceResult == Result::Success && targetResult == Result::Success);
        if (sourceResult == Result::Success && targetResult == Result::Success) {
            CHECK(scene->play(Animation::replacementTransform(source, target), 1.0f,
                              Easing::Linear) == Result::Success);
            CHECK(near(scene->duration(), 1.0f));
            auto targetModel = target->model;
            auto targetFill = target->style.fill;
            target->model = Mat4::translate({0.25f, 0.0f, 0.0f}) * target->model;
            target->style.fill = Color::hex("#ffffff");
            Surface invalidSurface;
            CHECK(renderer->render(scene, 0.5f, invalidSurface) == Result::InvalidArguments);
            target->model = targetModel;
            target->style.fill = targetFill;
            CHECK(renderer->render(scene, 0.5f, invalidSurface) == Result::Success);
            CHECK(scene->play(Animation::fadeOut(source), 0.25f, Easing::Linear)
                  == Result::InvalidArguments);
            CHECK(near(scene->duration(), 1.0f));
            CHECK(scene->play(Animation::indicate(target), 0.5f, Easing::Linear)
                  == Result::Success);
            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            auto startHash = checksum(surface);
            auto start = extent(surface, config.background);
            auto startRed = 0u;
            for (auto i = 0u; i < surface.width() * surface.height(); i++) {
                if (surface.data()[i] == pixel(sourceColor)) startRed++;
            }
            CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
            auto middleHash = checksum(surface);
            auto middle = extent(surface, config.background);
            CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
            auto endHash = checksum(surface);
            auto end = extent(surface, config.background);
            auto endBlue = 0u;
            auto endRed = 0u;
            for (auto i = 0u; i < surface.width() * surface.height(); i++) {
                if (surface.data()[i] == pixel(targetColor)) endBlue++;
                if (surface.data()[i] == pixel(sourceColor)) endRed++;
            }
            CHECK(startHash != middleHash && middleHash != endHash && startHash != endHash);
            CHECK(startRed > 0u && endBlue > 0u && endRed == 0u);
            CHECK(start.valid && middle.valid && end.valid);
            if (start.valid && middle.valid && end.valid) {
                auto startCenter = (start.minX + start.maxX) / 2u;
                auto middleCenter = (middle.minX + middle.maxX) / 2u;
                auto endCenter = (end.minX + end.maxX) / 2u;
                CHECK(startCenter < middleCenter && middleCenter < endCenter);
            }
        } else {
            if (sourceResult != Result::Success) delete source;
            if (targetResult != Result::Success) delete target;
        }
    } else {
        delete source;
        delete target;
    }
    delete scene;

    Vec3 mismatchPoints[] = {
        {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.0f, 0.5f, 0.0f}};
    auto atomic = Scene::gen(config);
    auto atomicSource = Polygon::gen(sourcePoints, 4u);
    auto atomicTarget = Polygon::gen(targetPoints, 4u);
    auto mismatch = Plot::gen(mismatchPoints, 3u);
    auto unsupportedSource = Point::gen({-2.5f, 0.0f, 0.0f});
    auto unsupportedTarget = Point::gen({2.5f, 0.0f, 0.0f});
    auto foreignScene = Scene::gen(config);
    auto foreign = Polygon::gen(targetPoints, 4u);
    CHECK(atomic && atomicSource && atomicTarget && mismatch && unsupportedSource
          && unsupportedTarget && foreignScene && foreign);
    if (atomic && atomicSource && atomicTarget && mismatch && unsupportedSource
        && unsupportedTarget && foreignScene && foreign) {
        auto sourceResult = atomic->add(atomicSource);
        auto targetResult = atomic->add(atomicTarget);
        auto mismatchResult = atomic->add(mismatch);
        auto unsupportedSourceResult = atomic->add(unsupportedSource);
        auto unsupportedTargetResult = atomic->add(unsupportedTarget);
        auto foreignResult = foreignScene->add(foreign);
        CHECK(sourceResult == Result::Success && targetResult == Result::Success
              && mismatchResult == Result::Success
              && unsupportedSourceResult == Result::Success
              && unsupportedTargetResult == Result::Success
              && foreignResult == Result::Success);
        if (sourceResult == Result::Success && targetResult == Result::Success
            && mismatchResult == Result::Success
            && unsupportedSourceResult == Result::Success
            && unsupportedTargetResult == Result::Success
            && foreignResult == Result::Success) {
            Surface surface;
            CHECK(renderer->render(atomic, 0.0f, surface) == Result::Success);
            auto initial = checksum(surface);
            Animation unsupported[] = {
                Animation::fade(atomicTarget, 0.25f),
                Animation::morph(unsupportedSource, unsupportedTarget)};
            CHECK(atomic->play(unsupported, 2u, 1.0f, Easing::Linear)
                  == Result::NonSupport);
            CHECK(near(atomic->duration(), 0.0f));
            CHECK(near(atomicTarget->opacity, 1.0f));
            CHECK(renderer->render(atomic, 0.0f, surface) == Result::Success);
            CHECK(checksum(surface) == initial);
            CHECK(atomic->play(Animation::morph(atomicSource, atomicSource))
                  == Result::InvalidArguments);
            CHECK(atomic->play(Animation::morph(atomicSource, mismatch))
                  == Result::NonSupport);
            CHECK(atomic->play(Animation::morph(atomicSource, foreign))
                  == Result::InvalidArguments);
            CHECK(near(atomic->duration(), 0.0f));
            CHECK(renderer->render(atomic, 0.0f, surface) == Result::Success);
            CHECK(checksum(surface) == initial);
            CHECK(atomic->play(Animation::morph(atomicSource, atomicTarget), 1.0f,
                               Easing::Linear) == Result::Success);
        } else {
            if (sourceResult != Result::Success) delete atomicSource;
            if (targetResult != Result::Success) delete atomicTarget;
            if (mismatchResult != Result::Success) delete mismatch;
            if (unsupportedSourceResult != Result::Success) delete unsupportedSource;
            if (unsupportedTargetResult != Result::Success) delete unsupportedTarget;
            if (foreignResult != Result::Success) delete foreign;
        }
    } else {
        delete atomicSource;
        delete atomicTarget;
        delete mismatch;
        delete unsupportedSource;
        delete unsupportedTarget;
        delete foreign;
    }
    delete foreignScene;
    delete atomic;
}

static void springResponses()
{
    constexpr auto pi = 3.14159265358979323846;
    auto close = [](double actual, double expected) {
        return std::isfinite(actual) && std::fabs(actual - expected) < 1.0e-10 * (1.0 + std::fabs(expected));
    };
    // Physical seconds preserve the response at the observation boundary.
    for (auto frame = 0; frame <= 240; frame++) {
        auto time = std::fmax(0.0, frame / 30.0 - 0.5);
        auto omega = 2.0 * pi;
        auto damped = omega * std::sqrt(1.0 - 0.15 * 0.15);
        auto expected = 200.0 * std::exp(-0.15 * omega * time)
                      * (std::cos(damped * time) + 0.15 * omega / damped * std::sin(damped * time));
        CHECK(close(springResponse(time, 1.0, omega * omega, 0.3 * omega, 200.0), expected));
        CHECK(close(springResponse(time, 1.0, omega * omega, 2.0 * omega, 200.0),
                    200.0 * (1.0 + omega * time) * std::exp(-omega * time)));
    }
    CHECK(close(springResponse(7.5, 1.0, 4.0 * pi * pi, 0.6 * pi, 200.0), -0.13352159900600497));
    CHECK(springResponse(0.0, 2.0, 8.0, 8.0, -3.25, 5.0) == -3.25);
    for (auto time : {0.01, 0.25, 1.0, 4.0}) {
        CHECK(close(springResponse(time, 2.0, 8.0, 0.0, 3.0, 5.0),
                    3.0 * std::cos(2.0 * time) + 2.5 * std::sin(2.0 * time)));
        CHECK(close(springResponse(time, 2.0, 8.0, 8.0, 3.0, 5.0),
                    (3.0 + 11.0 * time) * std::exp(-2.0 * time)));
        CHECK(close(springResponse(time, 1.0, 16.0, 10.0, 3.0, 5.0),
                    (29.0 * std::exp(-2.0 * time) - 11.0 * std::exp(-8.0 * time)) / 6.0));
        auto critical = springResponse(time, 2.0, 8.0, 8.0, 3.0, 5.0);
        CHECK(std::fabs(springResponse(time, 2.0, 8.0, 8.0 - 1.0e-10, 3.0, 5.0) - critical) < 1.0e-9);
        CHECK(std::fabs(springResponse(time, 2.0, 8.0, 8.0 + 1.0e-10, 3.0, 5.0) - critical) < 1.0e-9);
    }
    CHECK(close(springResponse(1.0e300, 1.0, 1.0, 1.0e300, 1.0), std::exp(-1.0)));
    CHECK(springResponse(1.0e300, 1.0, 1.0, 2.0, 1.0) == 0.0);
    // Large initial states can restore a finite response after decay or cancellation.
    auto residual = std::exp(std::log(1.0e308) + std::log(751.0) - 750.0);
    CHECK(std::fabs(springResponse(750.0, 1.0, 1.0, 2.0, 1.0e308) / residual - 1.0) < 1.0e-12);
    auto underdamped = std::exp(std::log(1.0e308) - 750.0) * std::cos(750.0);
    CHECK(std::fabs(springResponse(750.0, 1.0, 2.0, 2.0, 1.0e308, -1.0e308) / underdamped - 1.0) < 1.0e-12);
    auto overdamped = std::exp(std::log(1.0e308) + std::log(4.0 / 3.0) - 750.0);
    CHECK(std::fabs(springResponse(750.0, 1.0, 4.0, 5.0, 1.0e308) / overdamped - 1.0) < 1.0e-12);
    auto tiny = std::exp(std::log(1.0e308) + std::log(1431.0) - 1430.0);
    CHECK(tiny > 0.0 && std::fabs(springResponse(1430.0, 1.0, 1.0, 2.0, 1.0e308) / tiny - 1.0) < 1.0e-12);
    CHECK(std::fabs(springResponse(2.0, 1.0, 1.0e-308, 0.0, -1.0e308, 1.0e308) / 1.0e308 - 1.0) < 1.0e-12);
    CHECK(std::fabs(springResponse(2.0, 1.0, 1.0e-308, 0.0, 1.0e308, -1.0e308) / 1.0e308 + 1.0) < 1.0e-12);
    CHECK(springResponse(1.0, 1.0, 1.0e-308, 0.0, -1.0e308, 1.0e308) == 0.0);
    CHECK(std::isnan(springResponse(2.0, 1.0, 1.0e-308, 0.0, 1.0e308, 1.0e308)));
    auto nan = std::numeric_limits<double>::quiet_NaN();
    auto inf = std::numeric_limits<double>::infinity();
    CHECK(std::isnan(springResponse(-1.0, 1.0, 1.0, 1.0, 1.0)));
    CHECK(std::isnan(springResponse(inf, 1.0, 1.0, 1.0, 1.0)));
    CHECK(std::isnan(springResponse(1.0, 0.0, 1.0, 1.0, 1.0)));
    CHECK(std::isnan(springResponse(1.0, 1.0, -1.0, 1.0, 1.0)));
    CHECK(std::isnan(springResponse(1.0, 1.0, 1.0, -1.0, 1.0)));
    CHECK(std::isnan(springResponse(0.0, 1.0, 1.0, 1.0, nan)));
    CHECK(std::isnan(springResponse(0.0, 1.0, 1.0, 1.0, 1.0, inf)));
    CHECK(std::isnan(springResponse(1.0, 1.0e-300, 1.0e300, 1.0, 1.0)));
    CHECK(std::isnan(springResponse(1.0, 1.0e300, 1.0e-300, 1.0, 1.0)));
    CHECK(std::isnan(springResponse(1.0, 1.0e-300, 1.0, 1.0e300, 1.0)));
    CHECK(std::isnan(springResponse(1.0e300, 1.0, 1.0e300, 0.0, 1.0)));
    CHECK(std::isnan(springResponse(1.0e10, 1.0, 1.0e-300, 0.0, 1.0e300, 1.0e300)));
}

static void boundaries(SwRenderer* renderer)
{
    auto invalid = Point::gen({0.0f, 0.0f});
    CHECK(invalid != nullptr);
    if (invalid) {
        invalid->opacity = std::numeric_limits<float>::quiet_NaN();
        auto scene = Scene::gen();
        CHECK(scene != nullptr);
        auto root = _space();
        CHECK(root != nullptr);
        if (root) CHECK(root->add(invalid) == Result::InvalidArguments);
        delete root;
        delete scene;
        delete invalid;
    }

    auto first = Scene::gen();
    auto second = Scene::gen();
    auto shared = Point::gen({0.0f, 0.0f});
    auto sharedSpace = _space();
    CHECK(first && second && shared && sharedSpace);
    if (first && second && shared && sharedSpace) {
        CHECK(sharedSpace->add(shared) == Result::Success);
        CHECK(first->add(sharedSpace) == Result::Success);
        CHECK(second->add(sharedSpace) == Result::InvalidArguments);
        auto animation = Animation::create(shared);
        CHECK(first->play(animation, 1.0f, static_cast<Easing>(255)) == Result::InvalidArguments);
        auto custom = AnimCurve::cubicBezier({0.2f, 0.8f}, {0.3f, 1.0f}, 0.9f);
        CHECK(first->play(Animation::shift(shared, {1.0f, 0.0f}), 1.0f, custom)
              == Result::Success);
        auto invalidCurve = AnimCurve::preset(AnimCurvePreset::Back);
        invalidCurve.strength = std::numeric_limits<float>::quiet_NaN();
        CHECK(first->play(Animation::shift(shared, {1.0f, 0.0f}), 1.0f, invalidCurve)
              == Result::InvalidArguments);
        animation = Animation::fade(shared, 0.5f);
        animation.value = std::numeric_limits<float>::quiet_NaN();
        CHECK(first->play(animation) == Result::InvalidArguments);
        shared->opacity = std::numeric_limits<float>::quiet_NaN();
        Surface surface;
        CHECK(renderer->render(first, 0.0f, surface) == Result::InvalidArguments);
    }
    delete second;
    delete first;

    auto frozenScene = Scene::gen();
    auto frozen = Line::gen({-1.0f, 0.0f}, {1.0f, 0.0f});
    auto frozenSpace = _space();
    CHECK(frozenScene && frozen && frozenSpace);
    if (frozenScene && frozen && frozenSpace) {
        auto red = Color::hex("#ff0000");
        auto green = Color::hex("#00ff00");
        frozen->stroke(red, 3.0f);
        CHECK(frozenSpace->add(frozen) == Result::Success);
        CHECK(frozenScene->add(frozenSpace) == Result::Success);
        frozen->stroke(green, 7.0f);
        frozen->shift({2.0f, 0.0f});
        CHECK(frozen->style.stroke.r == red.r && frozen->style.width == 3.0f);
        CHECK(frozen->model.e[3] == 0.0f);
        frozen->layer = 1;
        Surface surface;
        CHECK(renderer->render(frozenScene, 0.0f, surface) == Result::InvalidArguments);
        frozen->layer = 0;
        CHECK(renderer->render(frozenScene, 0.0f, surface) == Result::Success);
    }
    delete frozenScene;

    auto textScene = Scene::gen();
    auto frozenText = Text::gen("before");
    auto textSpace = _space();
    CHECK(textScene && frozenText && textSpace);
    if (textScene && frozenText && textSpace) {
        CHECK(textSpace->add(frozenText) == Result::Success);
        CHECK(textScene->add(textSpace) == Result::Success);
        CHECK(frozenText->text("after") == Result::Success);
        CHECK(std::strcmp(frozenText->text(), "after") == 0);
        CHECK(textScene->wait(0.0f) == Result::Success);
        CHECK(frozenText->text("sealed") == Result::InsufficientCondition);
        CHECK(std::strcmp(frozenText->text(), "after") == 0);
    }
    delete textScene;

    auto limits = Scene::gen();
    CHECK(limits != nullptr);
    if (limits) {
        CHECK(limits->wait(std::numeric_limits<float>::max()) == Result::Success);
        CHECK(limits->wait(1.0f) == Result::InvalidArguments);
        delete limits;
    }

    auto precisionTimeline = Scene::gen();
    auto precisionPoint = Point::gen({0.0f, 0.0f});
    CHECK(precisionTimeline && precisionPoint);
    if (precisionTimeline && precisionPoint) {
        auto addResult = precisionTimeline->add(precisionPoint);
        CHECK(addResult == Result::Success);
        if (addResult == Result::Success) {
            CHECK(precisionTimeline->wait(1.0e30f) == Result::Success);
            auto duration = precisionTimeline->duration();
            CHECK(precisionTimeline->wait(1.0f) == Result::InvalidArguments);
            CHECK(precisionTimeline->duration() == duration);
            CHECK(precisionTimeline->play(Animation::fade(precisionPoint, 0.5f), 1.0f)
                  == Result::InvalidArguments);
            auto target = AnimationTarget::from(precisionPoint).opacity(0.5f);
            CHECK(precisionTimeline->play(target, 1.0f) == Result::InvalidArguments);
            CHECK(precisionTimeline->look(precisionTimeline->config().camera, CameraView::TwoD, 1.0f)
                  == Result::InvalidArguments);
            CHECK(precisionTimeline->duration() == duration);
        } else {
            delete precisionPoint;
        }
    } else {
        delete precisionPoint;
    }
    delete precisionTimeline;

    auto interpolation = Scene::gen();
    auto line = Line::gen({-1.0f, 0.0f}, {1.0f, 0.0f});
    auto interpolationSpace = _space();
    CHECK(interpolation && line && interpolationSpace);
    if (interpolation && line && interpolationSpace) {
        line->model = Mat4::translate({-std::numeric_limits<float>::max(), 0.0f, 0.0f});
        CHECK(interpolationSpace->add(line) == Result::Success);
        CHECK(interpolation->add(interpolationSpace) == Result::Success);
        auto target = Mat4::translate({std::numeric_limits<float>::max(), 0.0f, 0.0f});
        CHECK(interpolation->play(Animation::transform(line, target), 1.0f, Easing::Linear) == Result::Success);
        Surface surface;
        CHECK(renderer->render(interpolation, 0.5f, surface) == Result::Success);
    }
    delete interpolation;

    auto projectiveScene = Scene::gen();
    auto projectiveLine = Line::gen({-1.0f, 1.0f}, {1.0f, 1.0f});
    auto projectiveSpace = _space();
    CHECK(projectiveScene && projectiveLine && projectiveSpace);
    if (projectiveScene && projectiveLine && projectiveSpace) {
        projectiveLine->model.e[12] = 1.0f;
        CHECK(projectiveSpace->add(projectiveLine) == Result::InvalidArguments);
        projectiveLine->model = Mat4::identity();
        CHECK(projectiveSpace->add(projectiveLine) == Result::Success);
        CHECK(projectiveScene->add(projectiveSpace) == Result::Success);
        auto projective = Mat4::identity();
        projective.e[12] = 1.0f;
        CHECK(projectiveScene->play(Animation::transform(projectiveLine, projective)) == Result::InvalidArguments);
    }
    delete projectiveScene;

    Config subnormalConfig;
    subnormalConfig.camera.orthoHeight = 1.0e-40f;
    auto subnormalScene = Scene::gen(subnormalConfig);
    auto subnormalPoint = Point::gen({});
    auto subnormalSpace = _space();
    CHECK(subnormalScene && subnormalPoint && subnormalSpace);
    if (subnormalScene && subnormalPoint && subnormalSpace) {
        CHECK(subnormalSpace->add(subnormalPoint) == Result::Success);
        CHECK(subnormalScene->add(subnormalSpace) == Result::Success);
        Surface surface;
        CHECK(renderer->render(subnormalScene, 0.0f, surface) == Result::Success);
    }
    delete subnormalScene;

    Config scaledCameraConfig;
    scaledCameraConfig.camera.target = {0.0f, 0.0f, -1.0e-7f};
    scaledCameraConfig.camera.up = {0.0f, 1.0e-7f, 0.0f};
    scaledCameraConfig.camera.near = 1.0e-8f;
    auto scaledCameraScene = Scene::gen(scaledCameraConfig);
    CHECK(scaledCameraScene != nullptr);
    delete scaledCameraScene;

    Config invalidPlanar;
    invalidPlanar.camera.projection = Projection::Perspective;
    CHECK(Scene::gen(invalidPlanar) == nullptr);

    Config oddConfig;
    oddConfig.width = 321;
    oddConfig.height = 181;
    auto odd = Scene::gen(oddConfig);
    CHECK(odd != nullptr);
    if (odd) {
#ifdef VIS_SAVER_VIDEO
        CHECK(Saver::video(odd, renderer, "invalid.mp4") == Result::InvalidArguments);
#else
        CHECK(Saver::video(odd, renderer, "invalid.mp4") == Result::NonSupport);
#endif
        CHECK(Saver::video(odd, renderer, "invalid.webm") == Result::InvalidArguments);
        delete odd;
    }

#ifndef VIS_EXPECT_GL
    CHECK(GlRenderer::gen() == nullptr);
#endif
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && defined(VIS_SAVER_GIF) \
    && defined(VIS_SAVER_VIDEO)
// Whether the file at path, which holds one ffmpeg argument per line, contains argument.
static bool _argument(const char* path, const char* argument)
{
    char text[4096] = {};
    auto file = std::fopen(path, "rb");
    if (!file) return false;
    auto size = std::fread(text, 1, sizeof(text) - 1u, file);
    std::fclose(file);
    text[size] = '\0';
    auto length = std::strlen(argument);
    for (auto line = text; *line;) {
        auto end = std::strchr(line, '\n');
        auto lineLength = end ? static_cast<size_t>(end - line) : std::strlen(line);
        if (lineLength == length && std::strncmp(line, argument, length) == 0) return true;
        if (!end) break;
        line = end + 1;
    }
    return false;
}

static void videoCadence(SwRenderer* renderer)
{
    char directory[64];
    std::snprintf(directory, sizeof(directory), "vis-video-%ld", static_cast<long>(getpid()));
    auto created = mkdir(directory, 0700);
    CHECK(created == 0);
    if (created != 0) return;
    char executable[96];
    char countPath[96];
    char argumentPath[96];
    std::snprintf(executable, sizeof(executable), "%s/ffmpeg", directory);
    std::snprintf(countPath, sizeof(countPath), "%s/bytes", directory);
    std::snprintf(argumentPath, sizeof(argumentPath), "%s/arguments", directory);
    auto script = std::fopen(executable, "wb");
    CHECK(script != nullptr);
    if (!script) return;
    CHECK(std::fputs("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$VIS_FRAME_ARGUMENTS\"\n"
                     "wc -c > \"$VIS_FRAME_BYTES\"\n", script) >= 0);
    CHECK(std::fclose(script) == 0);
    CHECK(chmod(executable, 0700) == 0);

    auto oldPath = std::getenv("PATH");
    auto oldSize = oldPath ? std::strlen(oldPath) : 0u;
    auto savedPath = static_cast<char*>(std::malloc(oldSize + 1u));
    CHECK(savedPath != nullptr);
    if (!savedPath) return;
    if (oldPath) std::memcpy(savedPath, oldPath, oldSize);
    savedPath[oldSize] = '\0';
    auto directorySize = std::strlen(directory);
    auto joinedPath = static_cast<char*>(std::malloc(directorySize + (oldSize ? oldSize + 1u : 0u) + 1u));
    CHECK(joinedPath != nullptr);
    if (!joinedPath) {
        std::free(savedPath);
        return;
    }
    std::memcpy(joinedPath, directory, directorySize);
    auto joinedSize = directorySize;
    if (oldSize) {
        joinedPath[joinedSize++] = ':';
        std::memcpy(joinedPath + joinedSize, savedPath, oldSize);
        joinedSize += oldSize;
    }
    joinedPath[joinedSize] = '\0';
    CHECK(setenv("PATH", joinedPath, 1) == 0);
    std::free(joinedPath);
    CHECK(setenv("VIS_FRAME_BYTES", countPath, 1) == 0);
    CHECK(setenv("VIS_FRAME_ARGUMENTS", argumentPath, 1) == 0);

    Config config;
    config.width = 16;
    config.height = 16;
    auto scene = Scene::gen(config);
    CHECK(scene != nullptr);
    if (scene) {
        CHECK(scene->wait(0.1f) == Result::Success);
        CHECK(Saver::video(scene, renderer, "cadence.mp4", 30) == Result::Success);
        CHECK(Saver::video(scene, renderer, "cadence.gif", 30) == Result::Success);
        delete scene;
    }

    auto count = std::fopen(countPath, "rb");
    CHECK(count != nullptr);
    unsigned long long bytes = 0;
    if (count) {
        CHECK(std::fscanf(count, "%llu", &bytes) == 1);
        CHECK(std::fclose(count) == 0);
    }
    CHECK(bytes == 3ull * config.width * config.height * 4ull);

    auto preserved = std::fopen("preserved.mp4", "wb");
    CHECK(preserved != nullptr);
    if (preserved) {
        CHECK(std::fputs("KEEP-ME", preserved) >= 0);
        CHECK(std::fclose(preserved) == 0);
    }
    auto invalidScene = Scene::gen(config);
    auto invalidPoint = Point::gen({});
    auto invalidSpace = _space();
    CHECK(invalidScene && invalidPoint && invalidSpace);
    if (invalidScene && invalidPoint && invalidSpace) {
        CHECK(invalidSpace->add(invalidPoint) == Result::Success);
        CHECK(invalidScene->add(invalidSpace) == Result::Success);
        invalidPoint->point.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(Saver::video(invalidScene, renderer, "preserved.mp4", 30) == Result::InvalidArguments);
    }
    delete invalidScene;
    preserved = std::fopen("preserved.mp4", "rb");
    CHECK(preserved != nullptr);
    char contents[8] = {};
    if (preserved) {
        CHECK(std::fread(contents, 1, 7, preserved) == 7);
        CHECK(std::fclose(preserved) == 0);
    }
    CHECK(std::strcmp(contents, "KEEP-ME") == 0);

    // A soundtrack becomes the second input, read for as long as the 0.1 s video lasts and
    // encoded as AAC; its path is quoted for the shell.
    char trackPath[96];
    char missingPath[96];
    std::snprintf(trackPath, sizeof(trackPath), "%s/voice 'one'.wav", directory);
    std::snprintf(missingPath, sizeof(missingPath), "%s/missing.wav", directory);
    auto track = std::fopen(trackPath, "wb");
    CHECK(track != nullptr);
    if (track) {
        CHECK(std::fputs("RIFF", track) >= 0);
        CHECK(std::fclose(track) == 0);
    }
    auto cpu = Renderer::gen(RenderEngine::Cpu);
    auto voiced = Scene::gen(config);
    CHECK(cpu && voiced);
    if (cpu && voiced) {
        CHECK(voiced->wait(0.1f) == Result::Success);
        CHECK(Saver::save(voiced, *cpu, "voiced.mp4", 30, trackPath) == Result::Success);
        CHECK(_argument(argumentPath, "-t") && _argument(argumentPath, "0.100000"));
        CHECK(_argument(argumentPath, trackPath));
        CHECK(_argument(argumentPath, "0:v:0") && _argument(argumentPath, "1:a:0"));
        CHECK(_argument(argumentPath, "-c:a") && _argument(argumentPath, "aac"));
        CHECK(!_argument(argumentPath, "-an"));
        CHECK(Saver::save(voiced, *cpu, "silent.mp4", 30, nullptr) == Result::Success);
        CHECK(_argument(argumentPath, "-an") && !_argument(argumentPath, trackPath));
        CHECK(Saver::save(voiced, *cpu, "voiced.gif", 30, trackPath) == Result::InvalidArguments);
        CHECK(Saver::save(voiced, *cpu, "voiced.mp4", 30, "") == Result::InvalidArguments);
        CHECK(Saver::save(voiced, *cpu, "unvoiced.mp4", 30, missingPath) == Result::IoError);
        CHECK(std::fopen("unvoiced.mp4", "rb") == nullptr);
    }
    delete voiced;
    delete cpu;

    if (oldPath) CHECK(setenv("PATH", savedPath, 1) == 0);
    else CHECK(unsetenv("PATH") == 0);
    CHECK(unsetenv("VIS_FRAME_BYTES") == 0);
    CHECK(unsetenv("VIS_FRAME_ARGUMENTS") == 0);
    std::free(savedPath);

    CHECK(std::remove("cadence.mp4") == 0);
    CHECK(std::remove("cadence.gif") == 0);
    CHECK(std::remove("preserved.mp4") == 0);
    CHECK(std::remove("voiced.mp4") == 0);
    CHECK(std::remove("silent.mp4") == 0);
    CHECK(std::remove(trackPath) == 0);
    CHECK(std::remove(argumentPath) == 0);
    CHECK(std::remove(countPath) == 0);
    CHECK(std::remove(executable) == 0);
    CHECK(rmdir(directory) == 0);
}
#endif

static void scene3D(SwRenderer* renderer)
{
    Config config;
    config.width = 640;
    config.height = 360;
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {6.0f, 5.0f, 7.0f};
    auto scene = Scene::gen(config);
    auto space = Space::gen({-3.0f, 3.0f, 1.0f}, {-3.0f, 3.0f, 1.0f}, {-3.0f, 3.0f, 1.0f});
    auto x = Vector::gen({2.5f, 0.0f, 0.0f});
    auto y = Vector::gen({0.0f, 2.5f, 0.0f});
    auto z = Vector::gen({0.0f, 0.0f, 2.5f});
    CHECK(scene && space && x && y && z);
    x->tail = 12.0f;
    z->tip = 0.0f;
    x->stroke(Color::hex("#ef5350"), 4.0f);
    y->stroke(Color::hex("#66bb6a"), 4.0f);
    z->stroke(Color::hex("#42a5f5"), 4.0f);
    CHECK(space->add(x) == Result::Success);
    CHECK(space->add(y) == Result::Success);
    CHECK(space->add(z) == Result::Success);
    CHECK(scene->add(space) == Result::Success);
    Animation create[] = {Animation::create(space), Animation::create(x), Animation::create(y), Animation::create(z)};
    CHECK(scene->play(create, 4, 1.2f, Easing::Smooth, 0.08f) == Result::Success);

    Surface surface;
    CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
    CHECK(changed(surface, config.background) > 3500);
    CHECK(Saver::png(surface, "test-sw-3d.png") == Result::Success);
    delete scene;
}

// Count pixels on the middle row of a red|blue picture that are neither pure red nor pure blue.
static uint32_t pictureBlend(SwRenderer* renderer, const Asset* asset)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    auto space = _space();
    auto picture = Picture::gen(asset, {}, 4.0f);
    CHECK(scene && space && picture);
    if (!scene || !space || !picture) {
        delete scene;
        delete space;
        delete picture;
        return UINT32_MAX;
    }
    CHECK(space->add(picture) == Result::Success);
    CHECK(scene->add(space) == Result::Success);
    Surface surface;
    auto blended = UINT32_MAX;
    if (renderer->render(scene, 0.0f, surface) == Result::Success) {
        blended = 0u;
        auto row = surface.data() + static_cast<size_t>(surface.height() / 2u) * surface.stride();
        for (auto x = 110u; x < 210u; x++) {
            auto r = row[x] & 0xffu;
            auto b = (row[x] >> 16) & 0xffu;
            auto red = r >= 240u && b <= 100u;
            auto blue = b >= 240u && r <= 80u;
            if (!red && !blue) blended++;
        }
    }
    delete scene;
    return blended;
}

static void pictureSharpness(SwRenderer* renderer, const Asset* svg)
{
    // A 4x2 SVG scaled 30x stays vector: the red/blue edge is crisp.
    auto vector = pictureBlend(renderer, svg);
    CHECK(vector <= 2u);
    // The same content as a 4x2 raster blurs across the scaled edge (the former eager-raster path).
    Asset* raster = nullptr;
    CHECK(svg->data() && AssetLoader::load(svg->data(), svg->width(), svg->height(), raster) == Result::Success);
    if (!raster) return;
    CHECK(!raster->encoded());
    auto blurred = pictureBlend(renderer, raster);
    CHECK(blurred != UINT32_MAX && blurred > 10u && blurred > vector);
    delete raster;
}

static void assetsAndCells(SwRenderer* renderer)
{
    static constexpr char svg[] = "<svg xmlns='http://www.w3.org/2000/svg' width='4' height='2'>"
                                  "<rect width='2' height='2' fill='#ff4050'/><rect x='2' width='2' height='2' fill='#40a0ff'/>"
                                  "</svg>";
    Asset* decoded = nullptr;
    CHECK(AssetLoader::load(svg, sizeof(svg) - 1u, "svg", decoded) == Result::Success);
    CHECK(decoded && decoded->width() == 4 && decoded->height() == 2);
    CHECK(decoded && decoded->encoded() && decoded->encodedSize() == sizeof(svg) - 1u);
    CHECK(decoded && decoded->mime() && std::strcmp(decoded->mime(), "svg") == 0);
    CHECK(decoded && decoded->data() && decoded->data()[0] == 0xff5040ffu);
    if (decoded) pictureSharpness(renderer, decoded);
    delete decoded;
    Asset* invalid = nullptr;
    CHECK(AssetLoader::load("<svg", 4u, "svg", invalid) != Result::Success && !invalid);
    CHECK(AssetLoader::load("missing-asset.svg", invalid) != Result::Success && !invalid);
    CHECK(AssetLoader::load("asset.unknown", invalid) == Result::NonSupport && !invalid);

    uint32_t pixels[] = {0xff5040ff, 0xffffa040, 0xff66bb66, 0xffffd166};
    Asset* asset = nullptr;
    CHECK(AssetLoader::load(pixels, 2, 2, asset) == Result::Success);
    CHECK(asset && asset->width() == 2 && asset->height() == 2);
    if (!asset) return;

    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    auto space = _space();
    auto image = Picture::gen(asset, {1.5f, 0.0f, 0.0f}, 2.0f);
    auto cells = Cell::gen({-2.5f, -1.0f, 0.0f}, 2, 2, {}, CellMode::Padd);
    CHECK(scene && space && image && cells);
    if (scene && space && image && cells) {
        image->filter = ImageFilter::Nearest;
        CHECK(cells->texture(asset) == Result::Success);
        CHECK(cells->fill({0, 0, 1, 1}, Color::hex("#ffffff")) == Result::Success);
        CHECK(space->add(image) == Result::Success);
        CHECK(space->add(cells) == Result::Success);
        CHECK(scene->add(space) == Result::Success);
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, config.background) > 3000);
        scene->config().antialiasing = false;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, config.background) > 3000);
        CHECK(Saver::png(surface, "test-sw-assets-cells.png") == Result::Success);
    } else {
        delete space;
        delete image;
        delete cells;
    }
    delete scene;

    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {5.0f, 4.0f, 6.0f};
    config.antialiasing = true;
    auto voxelScene = Scene::gen(config);
    auto voxelSpace = _space();
    auto voxels = Cell::gen({-1.0f, -1.0f, -0.5f}, 2, 2, {}, CellMode::Padd);
    CHECK(voxelScene && voxelSpace && voxels);
    if (voxelScene && voxelSpace && voxels) {
        CHECK(voxels->texture(asset) == Result::Success);
        CHECK(voxelSpace->add(voxels) == Result::Success);
        CHECK(voxelScene->add(voxelSpace) == Result::Success);
        Surface surface;
        CHECK(renderer->render(voxelScene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, config.background) > 1500);
        CHECK(Saver::png(surface, "test-sw-voxels.png") == Result::Success);
    } else {
        delete voxelSpace;
        delete voxels;
    }
    delete voxelScene;
    delete asset;
}

struct SampleContext
{
    uint32_t calls = 0;
    float firstTime = -1.0f;
    float lastTime = -1.0f;
};

static bool _cellSample(float x, float y, float time, Color& color, void* data) noexcept
{
    auto context = static_cast<SampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = {
        static_cast<uint8_t>((x + 1.0f) * 80.0f),
        static_cast<uint8_t>(y * 80.0f),
        40,
        255};
    return true;
}

static bool _voxelSample(float x, float y, float z, float time, Color& color,
                         void* data) noexcept
{
    auto context = static_cast<SampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = {
        static_cast<uint8_t>((x + 1.0f) * 80.0f),
        static_cast<uint8_t>(y * 80.0f),
        static_cast<uint8_t>((z + 1.0f) * 80.0f),
        255};
    return true;
}

static bool _rejectVoxel(float, float, float, float, Color&, void*) noexcept
{
    return false;
}

static bool _solidCell(float, float, float, Color& color, void* data) noexcept
{
    color = *static_cast<Color*>(data);
    return true;
}

struct ReentrantVoxelContext
{
    Space* space = nullptr;
    Result nested = Result::Success;
    Result added = Result::Success;
};

static bool _reentrantVoxel(float, float, float, float, Color& color,
                            void* data) noexcept
{
    auto context = static_cast<ReentrantVoxelContext*>(data);
    Group* nested = nullptr;
    context->nested = context->space->voxel(_rejectVoxel, nested);
    auto child = Group::gen();
    context->added = context->space->add(child);
    if (context->added != Result::Success) delete child;
    color = Color::hex("#2563eb");
    return true;
}

static void voxelSamples(SwRenderer* renderer)
{
    auto space = Space::gen({-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f},
                            {-1.0f, 1.0f, 1.0f});
    SampleContext context;
    Group* voxels = nullptr;
    CHECK(space != nullptr);
    if (!space) return;
    CHECK(space->voxel(_voxelSample, voxels, &context, CellMode::Padd, 0.1f)
          == Result::Success);
    CHECK(voxels != nullptr);
    CHECK(context.calls == 18u);
    CHECK(context.firstTime == 0.0f && context.lastTime == 0.0f);
    CHECK(space->count() == 1u);
    CHECK(voxels && voxels->parent() == space);
    CHECK(voxels && voxels->childCount() == 3u);
    if (voxels && voxels->childCount() == 3u) {
        auto first = static_cast<Cell*>(voxels->childAt(0));
        auto last = static_cast<Cell*>(voxels->childAt(2));
        CHECK(first->type() == Type::Cell);
        CHECK(first->columns() == 3u);
        CHECK(first->rows() == 2u);
        CHECK(same(first->colors()[0], Color{0, 0, 0, 255}));
        CHECK(same(first->colors()[5], Color{160, 80, 0, 255}));
        CHECK(same(last->colors()[0], Color{0, 0, 160, 255}));
        auto center = first->model * Vec3{0.5f, 0.5f, 0.5f};
        CHECK(near(center.x, -1.0f));
        CHECK(near(center.y, 0.0f));
        CHECK(near(center.z, -1.0f));
    }

    Group* rejected = nullptr;
    CHECK(space->voxel(_rejectVoxel, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    CHECK(space->count() == 1u);
    CHECK(space->voxel(nullptr, rejected) == Result::InvalidArguments);
    CHECK(space->voxel(_voxelSample, rejected, &context, CellMode::Padd, 0.5f)
          == Result::InvalidArguments);

    Config config;
    config.width = 320u;
    config.height = 180u;
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {5.0f, 4.0f, 6.0f};
    auto scene = Scene::gen(config);
    CHECK(scene != nullptr);
    if (scene) {
        CHECK(scene->add(space) == Result::Success);
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(changed(surface, config.background) > 1000u);
    } else {
        delete space;
    }
    delete scene;

    auto oversized = Space::gen({-100.0f, 100.0f, 1.0f},
                                {-100.0f, 100.0f, 1.0f}, {0.0f, 0.0f, 1.0f});
    Group* output = nullptr;
    CHECK(oversized != nullptr);
    if (oversized) {
        CHECK(oversized->voxel(_voxelSample, output, &context)
              == Result::InsufficientCondition);
    }
    CHECK(output == nullptr);
    delete oversized;

    auto guarded = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
                              {0.0f, 0.0f, 1.0f});
    auto reentrant = ReentrantVoxelContext{guarded};
    output = nullptr;
    CHECK(guarded != nullptr);
    if (guarded) {
        CHECK(guarded->voxel(_reentrantVoxel, output, &reentrant)
              == Result::Success);
        CHECK(reentrant.nested == Result::InsufficientCondition);
        CHECK(reentrant.added == Result::InsufficientCondition);
        CHECK(output && guarded->count() == 1u);
    }
    delete guarded;
}

static void cellSamples()
{
    auto space = Space::gen({-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f},
                            {-100.0f, 100.0f, 1.0f});
    SampleContext context;
    Group* cells = nullptr;
    CHECK(space != nullptr);
    if (!space) return;
    CHECK(space->cell(_cellSample, cells, &context, CellMode::Padd, 0.1f)
          == Result::Success);
    CHECK(cells != nullptr);
    CHECK(context.calls == 6u);
    CHECK(context.firstTime == 0.0f && context.lastTime == 0.0f);
    CHECK(cells && cells->childCount() == 1u);
    if (cells && cells->childCount() == 1u) {
        auto cell = static_cast<Cell*>(cells->childAt(0));
        CHECK(cell->columns() == 3u);
        CHECK(cell->rows() == 2u);
        CHECK(same(cell->colors()[0], Color{0, 0, 40, 255}));
        CHECK(same(cell->colors()[5], Color{160, 80, 40, 255}));
        auto center = cell->model * Vec3{0.5f, 0.5f, 0.5f};
        CHECK(near(center.x, -1.0f));
        CHECK(near(center.y, 0.0f));
        CHECK(near(center.z, 0.0f));
    }
    Group* rejected = nullptr;
    CHECK(space->cell(nullptr, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    delete space;
}

struct RiemannProbe
{
    float xs[8] = {};
    uint32_t calls = 0u;
    // The call that returns false, and whether every value is NaN.
    uint32_t stop = UINT32_MAX;
    bool refuse = false;
};

// f(x) = x - 1, recording where it is read.
static bool _riemannLine(float x, float& value, void* data) noexcept
{
    auto probe = static_cast<RiemannProbe*>(data);
    auto call = probe->calls++;
    if (call < 8u) probe->xs[call] = x;
    if (call == probe->stop) return false;
    value = probe->refuse ? std::numeric_limits<float>::quiet_NaN() : x - 1.0f;
    return true;
}

static bool _riemannBar(const Object* object, float x, float y, float width, float height)
{
    if (!object || object->type() != Type::Rectangle) return false;
    auto bar = static_cast<const Rectangle*>(object);
    return near(bar->center.x, x) && near(bar->center.y, y) && near(bar->size.x, width)
           && near(bar->size.y, height);
}

static void riemannSums()
{
    // Whole steps from min, the last one ending at max.
    auto whole = Range{0.0f, 2.0f, 0.5f};
    auto shortened = Range{0.0f, 1.2f, 0.5f};
    CHECK(whole.steps() == 4u && shortened.steps() == 3u);
    CHECK(near(shortened.stepAt(2u), 1.0f) && shortened.stepAt(3u) == 1.2f && shortened.stepAt(9u) == 1.2f);
    // Rounding leaves no sliver of a step at the end; the last step reaches max instead.
    auto sliver = Range{0.0f, 1.00004f, 0.5f};
    CHECK(sliver.steps() == 2u && sliver.stepAt(2u) == 1.00004f);
    auto empty = Range{1.0f, 1.0f, 0.5f};
    auto reversed = Range{2.0f, 1.0f, 0.5f};
    auto still = Range{0.0f, 1.0f, 0.0f};
    auto endless = Range{0.0f, std::numeric_limits<float>::infinity(), 1.0f};
    auto countless = Range{0.0f, 1.0e30f, 1.0e-30f};
    CHECK(empty.steps() == 0u && reversed.steps() == 0u && still.steps() == 0u);
    CHECK(endless.steps() == 0u && countless.steps() == 0u);

    // One rectangle for each height from the left, standing on y = 0.
    const float heights[] = {-1.0f, -0.5f, 0.0f, 0.5f};
    Group* sum = nullptr;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, heights, 4u, sum) == Result::Success);
    CHECK(sum && sum->type() == Type::Group && sum->childCount() == 4u);
    if (sum && sum->childCount() == 4u) {
        CHECK(_riemannBar(sum->childAt(0), 0.25f, -0.5f, 0.5f, 1.0f));
        CHECK(_riemannBar(sum->childAt(1), 0.75f, -0.25f, 0.5f, 0.5f));
        CHECK(_riemannBar(sum->childAt(3), 1.75f, 0.25f, 0.5f, 0.5f));
        // A height of zero leaves a flat rectangle in its place.
        auto flat = static_cast<const Rectangle*>(sum->childAt(2));
        CHECK(near(flat->center.x, 1.25f) && flat->size.y > 0.0f && flat->size.y < 1.0e-5f);
    }
    // The group is not attached yet: a scene takes it whole.
    auto scene = Scene::gen();
    CHECK(scene != nullptr);
    if (scene && sum) {
        CHECK(scene->add(sum) == Result::Success);
        CHECK(scene->object(sum->id()) == sum && sum->childAt(0)->id() == sum->id() + 1u);
        if (scene->object(sum->id()) != sum) delete sum;
    } else {
        delete sum;
    }
    delete scene;

    auto dummy = Group::gen();
    auto rejected = dummy;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, heights, 3u, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, nullptr, 4u, rejected) == Result::InvalidArguments);
    const float infinite[] = {1.0f, std::numeric_limits<float>::infinity()};
    CHECK(Group::riemann({0.0f, 1.0f, 0.5f}, infinite, 2u, rejected) == Result::InvalidArguments);
    CHECK(Group::riemann({1.0f, 0.0f, 0.5f}, heights, 4u, rejected) == Result::InvalidArguments);
    CHECK(Group::riemann({1.0f, 1.0f, 0.5f}, heights, 4u, rejected) == Result::InsufficientCondition);
    CHECK(Group::riemann({0.0f, 4097.0f, 1.0f}, heights, 4u, rejected) == Result::InsufficientCondition);
    // Past 1e7 a tenth of a step no longer moves a float, so the first step has no width.
    float ones[10];
    for (auto& one : ones)
        one = 1.0f;
    CHECK(Group::riemann({1.0e7f, 1.0e7f + 1.0f, 0.1f}, ones, 10u, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    delete dummy;

    // fn is read once for each step, from the left, where sample says.
    RiemannProbe probe;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Left, sum)
          == Result::Success);
    CHECK(probe.calls == 4u && probe.xs[0] == 0.0f && probe.xs[3] == 1.5f);
    CHECK(sum && _riemannBar(sum->childAt(0), 0.25f, -0.5f, 0.5f, 1.0f));
    delete sum;
    probe = {};
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Right, sum)
          == Result::Success);
    CHECK(probe.calls == 4u && probe.xs[0] == 0.5f && probe.xs[3] == 2.0f);
    CHECK(sum && _riemannBar(sum->childAt(3), 1.75f, 0.5f, 0.5f, 1.0f));
    delete sum;
    // The last step ends short at the end of the range and is read at its own midpoint.
    probe = {};
    CHECK(Group::riemann({0.0f, 1.2f, 0.5f}, _riemannLine, &probe, RiemannSample::Midpoint, sum)
          == Result::Success);
    CHECK(probe.calls == 3u && near(probe.xs[0], 0.25f) && near(probe.xs[2], 1.1f));
    CHECK(sum && _riemannBar(sum->childAt(2), 1.1f, 0.05f, 0.2f, 0.1f));
    delete sum;
    // The point lies a share of the step from its left end, in float: across zero that can be a
    // rounding away from the midpoint of the ends.
    probe = {};
    auto crossing = Range{-0.09177006f, 4.873922f, 0.37f};
    CHECK(Group::riemann(crossing, _riemannLine, &probe, RiemannSample::Midpoint, sum) == Result::Success);
    auto first = crossing.stepAt(0u);
    auto second = crossing.stepAt(1u);
    CHECK(probe.xs[0] == first + (second - first) * 0.5f);
    CHECK(probe.xs[0] != static_cast<float>((static_cast<double>(first) + second) * 0.5));
    delete sum;

    // fn stops the call by returning false or writing a value that is not finite.
    rejected = nullptr;
    probe = {};
    probe.stop = 2u;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Left, rejected)
          == Result::InvalidArguments);
    CHECK(probe.calls == 3u && rejected == nullptr);
    probe = {};
    probe.refuse = true;
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, RiemannSample::Left, rejected)
          == Result::InvalidArguments);
    CHECK(probe.calls == 1u && rejected == nullptr);
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, nullptr, nullptr, RiemannSample::Left, rejected)
          == Result::InvalidArguments);
    CHECK(Group::riemann({0.0f, 2.0f, 0.5f}, _riemannLine, &probe, static_cast<RiemannSample>(3), rejected)
          == Result::InvalidArguments);
    CHECK(Group::riemann({0.0f, 5000.0f, 1.0f}, _riemannLine, &probe, RiemannSample::Left, rejected)
          == Result::InsufficientCondition);
    CHECK(rejected == nullptr);
}

struct FieldProbe
{
    uint32_t calls = 0u;
    // The call that returns false, and the x past which the field is NaN.
    uint32_t stop = UINT32_MAX;
    double edge = std::numeric_limits<double>::infinity();
};

// The field that turns about the origin, (-y, x).
static bool _fieldTurn(double x, double y, double& u, double& v, void* data) noexcept
{
    auto probe = static_cast<FieldProbe*>(data);
    if (probe->calls++ == probe->stop) return false;
    u = -y;
    v = x;
    return true;
}

// The field that runs right at unit speed, NaN past the probe's edge.
static bool _fieldFlow(double x, double, double& u, double& v, void* data) noexcept
{
    auto probe = static_cast<FieldProbe*>(data);
    if (probe->calls++ == probe->stop) return false;
    u = x > probe->edge ? std::numeric_limits<double>::quiet_NaN() : 1.0;
    v = 0.0;
    return true;
}

// The field that carries a point away from x = 0, faster the farther it is.
static bool _fieldSpread(double x, double, double& u, double& v, void*) noexcept
{
    u = x;
    v = 0.0;
    return true;
}

static bool _vectorAt(const Object* object, const Vec3& origin, const Vec3& value)
{
    if (!object || object->type() != Type::Vector) return false;
    auto arrow = static_cast<const Vector*>(object);
    return near(arrow->origin.x, origin.x) && near(arrow->origin.y, origin.y) && near(arrow->value.x, value.x)
           && near(arrow->value.y, value.y);
}

static void vectorFields()
{
    // The samples from min, one every step up to max.
    auto grid = Range{-1.0f, 1.0f, 0.5f};
    auto uneven = Range{0.0f, 1.2f, 0.5f};
    auto rounded = Range{0.0f, 0.99996f, 0.5f};
    auto single = Range{2.0f, 2.0f, 1.0f};
    auto reversed = Range{1.0f, 0.0f, 1.0f};
    CHECK(grid.samples() == 5u && grid.sampleAt(4u) == 1.0f && grid.sampleAt(1u) == -0.5f);
    CHECK(uneven.samples() == 3u && uneven.sampleAt(2u) == 1.0f);
    // Rounding never drops a sample that max should have been.
    CHECK(rounded.samples() == 3u && rounded.sampleAt(2u) == 1.0f);
    CHECK(single.samples() == 1u && reversed.samples() == 0u);

    // An arrow at every sample but the origin, where the field vanishes, row by row from the
    // bottom left, approaching length as the field grows.
    FieldProbe probe;
    Group* field = nullptr;
    uint32_t indices[9] = {};
    auto axis = Range{-1.0f, 1.0f, 1.0f};
    VectorFieldOptions options;
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, field, options, indices) == Result::Success);
    CHECK(probe.calls == 9u && field && field->childCount() == 8u);
    CHECK(indices[0] == 0u && indices[3] == 3u && indices[4] == 5u && indices[7] == 8u);
    if (field && field->childCount() == 8u) {
        // The default length is 0.8 of the smaller step: tanh(1 / 0.8) * 0.8 along (0, 1) at (1, 0).
        auto edge = 0.8f * std::tanh(1.0f / 0.8f);
        CHECK(_vectorAt(field->childAt(4), {1.0f, 0.0f, 0.0f}, {0.0f, edge, 0.0f}));
        auto arrow = static_cast<const Vector*>(field->childAt(0));
        CHECK(arrow->tip == 10.0f && near(std::hypot(arrow->value.x, arrow->value.y), 0.8f * std::tanh(std::sqrt(2.0f) / 0.8f)));
    }
    delete field;

    // Scaled and centered arrows, colored by magnitude from the least to the greatest.
    const Color ramp[] = {{0, 0, 0, 255}, {255, 255, 255, 255}};
    options.scale = 0.5f;
    options.centered = true;
    options.tip = 4.0f;
    options.colors = ramp;
    options.colorCount = 2u;
    probe = {};
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, field, options) == Result::Success);
    CHECK(field && field->childCount() == 8u);
    if (field && field->childCount() == 8u) {
        CHECK(_vectorAt(field->childAt(4), {1.0f, -0.25f, 0.0f}, {0.0f, 0.5f, 0.0f}));
        auto corner = static_cast<const Vector*>(field->childAt(0));
        auto side = static_cast<const Vector*>(field->childAt(1));
        CHECK(corner->tip == 4.0f && same(corner->style.stroke, {255, 255, 255, 255}));
        // A side has magnitude 1 of the corners' sqrt(2).
        auto gray = static_cast<uint8_t>(std::lround(255.0 / std::sqrt(2.0)));
        CHECK(same(side->style.stroke, {gray, gray, gray, 255}));
    }
    delete field;
    // low and high set the magnitudes the colors spread over.
    options.low = 1.0f;
    options.high = 2.0f;
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, field, options) == Result::Success);
    CHECK(field && field->childCount() == 8u && same(field->childAt(1)->style.stroke, {0, 0, 0, 255}));
    delete field;

    // The vectors given instead, one for each sample.
    const Vec2 given[] = {{1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 2.0f}};
    auto row = Range{0.0f, 2.0f, 1.0f};
    auto line = Range{0.0f, 0.0f, 1.0f};
    CHECK(Group::vectorField(row, line, given, 3u, field, {}, indices) == Result::Success);
    CHECK(field && field->childCount() == 2u && indices[0] == 0u && indices[1] == 2u);
    delete field;

    auto dummy = Group::gen();
    auto rejected = dummy;
    CHECK(Group::vectorField(row, line, given, 2u, rejected) == Result::InvalidArguments);
    CHECK(rejected == nullptr);
    const Vec2 infinite[] = {{1.0f, 0.0f}, {0.0f, std::numeric_limits<float>::infinity()}, {0.0f, 2.0f}};
    CHECK(Group::vectorField(row, line, infinite, 3u, rejected) == Result::InvalidArguments);
    CHECK(Group::vectorField(row, line, static_cast<const Vec2*>(nullptr), 3u, rejected)
          == Result::InvalidArguments);
    CHECK(Group::vectorField(row, line, static_cast<FieldSampler>(nullptr), nullptr, rejected)
          == Result::InvalidArguments);
    VectorFieldOptions both;
    both.length = 1.0f;
    both.scale = 1.0f;
    CHECK(Group::vectorField(row, line, given, 3u, rejected, both) == Result::InvalidArguments);
    VectorFieldOptions lone;
    lone.colors = ramp;
    lone.colorCount = 1u;
    CHECK(Group::vectorField(row, line, given, 3u, rejected, lone) == Result::InvalidArguments);
    VectorFieldOptions tipless;
    tipless.tip = -1.0f;
    CHECK(Group::vectorField(row, line, given, 3u, rejected, tipless) == Result::InvalidArguments);
    CHECK(Group::vectorField(reversed, line, given, 3u, rejected) == Result::InvalidArguments);
    auto wide = Range{0.0f, 5000.0f, 1.0f};
    auto square = Range{0.0f, 200.0f, 1.0f};
    CHECK(Group::vectorField(wide, line, _fieldTurn, &probe, rejected) == Result::InsufficientCondition);
    CHECK(Group::vectorField(square, square, _fieldTurn, &probe, rejected) == Result::InsufficientCondition);
    // fn stops the call by returning false or writing a vector that is not finite.
    probe = {};
    probe.stop = 3u;
    CHECK(Group::vectorField(axis, axis, _fieldTurn, &probe, rejected) == Result::InvalidArguments);
    CHECK(probe.calls == 4u && rejected == nullptr);
    probe = {};
    probe.edge = 0.5;
    CHECK(Group::vectorField(axis, axis, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    CHECK(probe.calls == 3u && rejected == nullptr);
    delete dummy;
}

static bool _plotFrom(const Object* object, uint32_t count, float first, float last)
{
    if (!object || object->type() != Type::Plot) return false;
    auto plot = static_cast<const Plot*>(object);
    return plot->count() == count && near(plot->points()[0].x, first) && near(plot->points()[count - 1u].x, last);
}

static void streamLines()
{
    StreamLineOptions options;
    CHECK(options.steps() == 60u);
    options.time = 1.0f;
    options.dt = 0.3f;
    CHECK(options.steps() == 4u);
    options.time = 1.0e-9f;
    options.dt = 1.0f;
    CHECK(options.steps() == 1u);
    options.time = 0.0f;
    CHECK(options.steps() == 0u);

    // A line from each sample of the grid, which also bounds them: the last seed has nowhere
    // to go and draws no line.
    FieldProbe probe;
    Group* lines = nullptr;
    uint32_t indices[4] = {};
    options.time = 1.0f;
    options.dt = 0.5f;
    auto row = Range{0.0f, 2.0f, 1.0f};
    auto line = Range{0.0f, 0.0f, 1.0f};
    CHECK(Group::streamLines(row, line, _fieldFlow, &probe, lines, options, indices) == Result::Success);
    CHECK(lines && lines->childCount() == 2u && indices[0] == 0u && indices[1] == 1u);
    if (lines && lines->childCount() == 2u) {
        CHECK(_plotFrom(lines->childAt(0), 3u, 0.0f, 1.0f) && _plotFrom(lines->childAt(1), 3u, 1.0f, 2.0f));
    }
    delete lines;

    // Both ways from a seed, the backward part toward it and on past it, inside the region.
    const Vec2 seed[] = {{0.0f, 0.0f}};
    Bounds region;
    region.min = {-0.75f, -1.0f, 0.0f};
    region.max = {10.0f, 1.0f, 0.0f};
    options.direction = StreamDirection::Both;
    probe = {};
    CHECK(Group::streamLines(seed, 1u, &region, _fieldFlow, &probe, lines, options) == Result::Success);
    CHECK(lines && lines->childCount() == 1u && _plotFrom(lines->childAt(0), 4u, -0.5f, 1.0f));
    delete lines;
    // Without a region nothing bounds it; backward runs against the field only.
    options.direction = StreamDirection::Backward;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, lines, options) == Result::Success);
    CHECK(lines && lines->childCount() == 1u && _plotFrom(lines->childAt(0), 3u, -1.0f, 0.0f));
    delete lines;

    // A line ends where the field stops being finite, without failing the call.
    options.direction = StreamDirection::Forward;
    options.time = 2.0f;
    options.dt = 0.25f;
    probe = {};
    probe.edge = 0.6;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, lines, options) == Result::Success);
    CHECK(lines && lines->childCount() == 1u && _plotFrom(lines->childAt(0), 3u, 0.0f, 0.5f));
    delete lines;

    // A line under a thousandth of the longest is left out, and the colors spread over the mean
    // speeds of the lines kept.
    const Vec2 spread[] = {{1.0f, 0.0f}, {1.0e-6f, 0.0f}, {0.0f, 0.0f}};
    const Color ramp[] = {{0, 0, 0, 255}, {255, 255, 255, 255}};
    options.time = 1.0f;
    options.dt = 0.1f;
    options.colors = ramp;
    options.colorCount = 2u;
    CHECK(Group::streamLines(spread, 3u, nullptr, _fieldSpread, nullptr, lines, options, indices)
          == Result::Success);
    CHECK(lines && lines->childCount() == 1u && indices[0] == 0u);
    CHECK(lines && same(lines->childAt(0)->style.stroke, {255, 255, 255, 255}));
    delete lines;
    options.colors = nullptr;
    options.colorCount = 0u;

    auto dummy = Group::gen();
    auto rejected = dummy;
    probe = {};
    probe.stop = 5u;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, options)
          == Result::InvalidArguments);
    CHECK(probe.calls == 6u && rejected == nullptr);
    CHECK(Group::streamLines(nullptr, 1u, nullptr, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    CHECK(Group::streamLines(seed, 0u, nullptr, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    CHECK(Group::streamLines(seed, 1u, nullptr, nullptr, nullptr, rejected) == Result::InvalidArguments);
    const Vec2 lost[] = {{std::numeric_limits<float>::quiet_NaN(), 0.0f}};
    CHECK(Group::streamLines(lost, 1u, nullptr, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    Bounds inverted;
    inverted.min = {1.0f, 0.0f, 0.0f};
    inverted.max = {0.0f, 1.0f, 0.0f};
    CHECK(Group::streamLines(seed, 1u, &inverted, _fieldFlow, &probe, rejected) == Result::InvalidArguments);
    StreamLineOptions still;
    still.dt = 0.0f;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, still) == Result::InvalidArguments);
    StreamLineOptions astray;
    astray.direction = static_cast<StreamDirection>(3);
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, astray)
          == Result::InvalidArguments);
    StreamLineOptions endless;
    endless.time = 1000.0f;
    endless.dt = 0.01f;
    CHECK(Group::streamLines(seed, 1u, nullptr, _fieldFlow, &probe, rejected, endless)
          == Result::InsufficientCondition);
    auto wide = Range{0.0f, 5000.0f, 1.0f};
    CHECK(Group::streamLines(wide, line, _fieldFlow, &probe, rejected) == Result::InsufficientCondition);
    CHECK(rejected == nullptr);
    delete dummy;
}

struct TemporalSampleContext
{
    uint32_t calls = 0u;
    float firstTime = -1.0f;
    float lastTime = -1.0f;
};

static bool _temporalCell(float, float, float time, Color& color,
                          void* data) noexcept
{
    auto context = static_cast<TemporalSampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = {
        static_cast<uint8_t>(255.0f * (1.0f - time) + 0.5f),
        0u,
        static_cast<uint8_t>(255.0f * time + 0.5f),
        255u};
    return true;
}

static bool _temporalVoxel(float, float, float, float time, Color& color,
                           void* data) noexcept
{
    auto context = static_cast<TemporalSampleContext*>(data);
    if (!context->calls) context->firstTime = time;
    context->lastTime = time;
    context->calls++;
    color = time < 0.5f ? Color::hex("#2563eb") : Color::hex("#7c3aed");
    return true;
}

static void sampleTime(SwRenderer* renderer)
{
    Config config;
    config.width = 80u;
    config.height = 80u;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto space = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
                            {0.0f, 0.0f, 1.0f});
    CHECK(scene && space);
    if (!scene || !space) {
        delete scene;
        delete space;
        return;
    }
    space->style.stroke.a = 0u;
    space->axisX.a = 0u;
    space->axisY.a = 0u;
    space->axisZ.a = 0u;
    TemporalSampleContext context;
    Group* field = nullptr;
    CHECK(space->cell(_temporalCell, field, &context, CellMode::Full,
                      0.0f, {1.0f, 2u}) == Result::Success);
    CHECK(field != nullptr);
    CHECK(context.calls == 3u);
    CHECK(near(context.firstTime, 0.0f) && near(context.lastTime, 1.0f));
    CHECK(scene->add(space) == Result::Success);
    CHECK(near(scene->duration(), 1.0f));

    Surface surface;
    auto center = static_cast<size_t>(config.height / 2u) * config.width
                + config.width / 2u;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#ff0000")));
    CHECK(renderer->render(scene, 0.25f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#c00040")));
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#800080")));
    BBox bounds;
    CHECK(renderer->bounds(scene, field, 0.5f, bounds) == Result::Success);
    CHECK(bounds.width > 0.0f && bounds.height > 0.0f);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(surface.data()[center] == pixel(Color::hex("#0000ff")));
    CHECK(context.calls == 3u);

    auto blue = Color::hex("#2563eb");
    Group* target = nullptr;
    CHECK(space->cell(_solidCell, target, &blue, CellMode::Full)
          == Result::Success);
    CHECK(scene->play(Animation::morph(field, target), 1.0f, Easing::Linear)
          == Result::NonSupport);
    delete scene;

    auto volume = Space::gen({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
                             {0.0f, 0.0f, 1.0f});
    context = {};
    field = nullptr;
    CHECK(volume != nullptr);
    if (volume) {
        CHECK(volume->voxel(_temporalVoxel, field, &context, CellMode::Padd,
                            0.05f, {0.5f, 2u}) == Result::Success);
        CHECK(field != nullptr && context.calls == 2u);
        CHECK(near(context.firstTime, 0.0f) && near(context.lastTime, 0.5f));
        Group* invalid = nullptr;
        CHECK(volume->voxel(_temporalVoxel, invalid, &context, CellMode::Padd,
                            0.05f, {-1.0f, 30u}) == Result::InvalidArguments);
        CHECK(volume->voxel(_temporalVoxel, invalid, &context, CellMode::Padd,
                            0.05f, {1.0f, 0u}) == Result::InvalidArguments);
        CHECK(volume->voxel(_temporalVoxel, invalid, &context, CellMode::Padd,
                            0.05f, {200.0f, 30u}) == Result::InsufficientCondition);
        CHECK(invalid == nullptr);
    }
    delete volume;
}

static void dimensionTransition(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.cameraMode = CameraMode::Fixed;
    auto scene = Scene::gen(config);
    auto space = Space::gen({-4.0f, 4.0f, 1.0f}, {-3.0f, 3.0f, 1.0f}, {-3.0f, 3.0f, 1.0f});
    auto vector = Vector::gen({2.0f, 1.5f, 1.0f});
    CHECK(scene && space && vector);
    if (!scene || !space || !vector) {
        delete scene;
        delete space;
        delete vector;
        return;
    }
    space->model = Mat4::scale({1.0f, 2.0f, 0.5f});
    CHECK(space->add(vector) == Result::Success);
    CHECK(scene->add(space) == Result::Success);
    CHECK(space->count() == 1);
    CHECK(space->objectAt(0) == vector);
    CHECK(space->c2w({1.0f, 1.0f, 1.0f}).x == 1.0f);
    CHECK(space->c2w({1.0f, 1.0f, 1.0f}).y == 2.0f);
    CHECK(space->c2w({1.0f, 1.0f, 1.0f}).z == 0.5f);
    Vec3 coordinate;
    CHECK(space->w2c({1.0f, 2.0f, 0.5f}, coordinate));
    CHECK(coordinate.x == 1.0f && coordinate.y == 1.0f && coordinate.z == 1.0f);
    auto child = Space::gen({-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    auto childPoint = Point::gen({1.0f, 1.0f, 1.0f});
    CHECK(child && childPoint);
    if (child && childPoint) {
        child->model = Mat4::translate({1.0f, 0.0f, 0.0f});
        CHECK(child->add(childPoint) == Result::Success);
        CHECK(space->add(child) == Result::Success);
        CHECK(child->space() == space && childPoint->space() == child);
        auto world = child->c2w({1.0f, 1.0f, 1.0f});
        CHECK(world.x == 2.0f && world.y == 2.0f && world.z == 0.5f);
    }
    auto second = _space();
    CHECK(second != nullptr);
    if (second) {
        second->model = Mat4::translate({-3.0f, 0.0f, 0.0f});
        CHECK(scene->add(second) == Result::Success);
        CHECK(second->space() == nullptr);
    }
    auto singular = Space::gen();
    CHECK(singular != nullptr);
    if (singular) {
        singular->model = Mat4::scale({1.0f, 0.0f, 1.0f});
        CHECK(!singular->w2c({}, coordinate));
    }
    delete singular;

    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(changed(surface, config.background) > 1000);
    auto planar = checksum(surface);
    auto planarCamera = scene->config().camera;
    auto spatialCamera = planarCamera;
    spatialCamera.eye = {6.0f, 4.0f, 7.0f};
    spatialCamera.projection = Projection::Perspective;
    CHECK(scene->look(spatialCamera, CameraView::ThreeD, 1.0f, Easing::Smooth) == Result::Success);
    CHECK(scene->view(0.0f) == CameraView::TwoD);
    CHECK(scene->view(0.5f) == CameraView::ThreeD);
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(changed(surface, config.background) > 1000);
    CHECK(checksum(surface) != planar);
    CHECK(scene->look(planarCamera, CameraView::TwoD, 1.0f, Easing::Smooth) == Result::Success);
    CHECK(scene->view(scene->duration()) == CameraView::TwoD);
    CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
    CHECK(changed(surface, config.background) > 1000);
    CHECK(checksum(surface) == planar);
    delete scene;

    Config cameraPathConfig;
    cameraPathConfig.width = 64;
    cameraPathConfig.height = 64;
    cameraPathConfig.cameraView = CameraView::ThreeD;
    cameraPathConfig.camera.projection = Projection::Perspective;
    auto cameraPath = Scene::gen(cameraPathConfig);
    CHECK(cameraPath != nullptr);
    if (cameraPath) {
        auto opposite = cameraPathConfig.camera;
        opposite.eye = {0.0f, 0.0f, -10.0f};
        CHECK(cameraPath->look(opposite, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
        Surface pathSurface;
        CHECK(renderer->render(cameraPath, 0.5f, pathSurface) == Result::Success);

        auto rolled = opposite;
        rolled.up = {0.0f, -1.0f, 0.0f};
        CHECK(cameraPath->look(rolled, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
        CHECK(renderer->render(cameraPath, 1.5f, pathSurface) == Result::Success);
    }
    delete cameraPath;

    Config extremeCameraConfig;
    extremeCameraConfig.width = 32;
    extremeCameraConfig.height = 32;
    extremeCameraConfig.cameraView = CameraView::ThreeD;
    extremeCameraConfig.camera.projection = Projection::Perspective;
    auto limit = std::numeric_limits<float>::max();
    extremeCameraConfig.camera.eye = {limit, 0.0f, 0.0f};
    extremeCameraConfig.camera.target = {-limit, 0.0f, 0.0f};
    auto extremeCamera = Scene::gen(extremeCameraConfig);
    CHECK(extremeCamera != nullptr);
    if (extremeCamera) {
        auto ordinary = Camera{};
        ordinary.projection = Projection::Perspective;
        CHECK(extremeCamera->look(ordinary, CameraView::ThreeD, 1.0f, Easing::Linear)
              == Result::InvalidArguments);
        CHECK(near(extremeCamera->duration(), 0.0f));
        Surface extremeSurface;
        CHECK(renderer->render(extremeCamera, 0.0f, extremeSurface) == Result::Success);
        auto extremeLens = extremeCameraConfig.camera;
        extremeLens.fov = 0.9f;
        CHECK(extremeCamera->look(extremeLens, CameraView::ThreeD, 1.0f, Easing::Linear)
              == Result::Success);
        CHECK(renderer->render(extremeCamera, 0.5f, extremeSurface) == Result::Success);
    }
    delete extremeCamera;

    Config boundaryCameraConfig;
    boundaryCameraConfig.width = 32;
    boundaryCameraConfig.height = 32;
    boundaryCameraConfig.cameraView = CameraView::ThreeD;
    boundaryCameraConfig.camera.projection = Projection::Perspective;
    boundaryCameraConfig.camera.eye = {limit, 0.0f, 0.0f};
    auto boundaryTargetX = limit;
    for (auto i = 0u; i < 7u; i++) boundaryTargetX = std::nextafter(boundaryTargetX, 0.0f);
    boundaryCameraConfig.camera.target = {boundaryTargetX, 0.0f, 0.0f};
    auto boundaryCamera = Scene::gen(boundaryCameraConfig);
    CHECK(boundaryCamera != nullptr);
    if (boundaryCamera) {
        auto boundaryTarget = boundaryCameraConfig.camera;
        boundaryTarget.target.x = std::nextafter(boundaryCameraConfig.camera.target.x, 0.0f);
        CHECK(boundaryCamera->look(boundaryTarget, CameraView::ThreeD, 1.0f, Easing::Linear)
              == Result::Success);
        Surface boundarySurface;
        CHECK(renderer->render(boundaryCamera, 0.5f, boundarySurface) == Result::Success);
    }
    delete boundaryCamera;

    Config clipPlaneConfig;
    clipPlaneConfig.width = 32;
    clipPlaneConfig.height = 32;
    clipPlaneConfig.camera.near = 1.0f;
    clipPlaneConfig.camera.far = std::nextafter(1.0f, std::numeric_limits<float>::infinity());
    auto clipPlaneScene = Scene::gen(clipPlaneConfig);
    CHECK(clipPlaneScene != nullptr);
    if (clipPlaneScene) {
        auto clipPlaneTarget = clipPlaneConfig.camera;
        clipPlaneTarget.near = std::nextafter(2.0f, 0.0f);
        clipPlaneTarget.far = 2.0f;
        CHECK(clipPlaneScene->look(clipPlaneTarget, CameraView::TwoD, 1.0f, Easing::Linear)
              == Result::InvalidArguments);
        CHECK(near(clipPlaneScene->duration(), 0.0f));
        Surface clipPlaneSurface;
        CHECK(renderer->render(clipPlaneScene, 0.0f, clipPlaneSurface) == Result::Success);
    }
    delete clipPlaneScene;

    Config precisionCameraConfig;
    precisionCameraConfig.width = 32;
    precisionCameraConfig.height = 32;
    precisionCameraConfig.cameraView = CameraView::ThreeD;
    precisionCameraConfig.camera.projection = Projection::Perspective;
    precisionCameraConfig.camera.target = {1073741824.0f, 0.0f, 0.0f};
    precisionCameraConfig.camera.eye = {1073741824.0f, 1.0f, 0.0f};
    precisionCameraConfig.camera.up = {0.0f, 0.0f, 1.0f};
    auto precisionCamera = Scene::gen(precisionCameraConfig);
    CHECK(precisionCamera != nullptr);
    if (precisionCamera) {
        auto collapsed = precisionCameraConfig.camera;
        collapsed.target = {0.0f, 1073741824.0f, 0.0f};
        collapsed.eye = {1.0f, 1073741824.0f, 0.0f};
        CHECK(precisionCamera->look(collapsed, CameraView::ThreeD, 1.0f, Easing::Linear)
              == Result::InvalidArguments);
        CHECK(near(precisionCamera->duration(), 0.0f));
        auto fixedPose = precisionCameraConfig.camera;
        fixedPose.fov = 0.9f;
        CHECK(precisionCamera->look(fixedPose, CameraView::ThreeD, 1.0f, Easing::Linear)
              == Result::Success);
        Surface precisionSurface;
        CHECK(renderer->render(precisionCamera, 0.5f, precisionSurface) == Result::Success);
    }
    delete precisionCamera;

    auto invalidCurrentCamera = Scene::gen();
    CHECK(invalidCurrentCamera != nullptr);
    if (invalidCurrentCamera) {
        invalidCurrentCamera->config().camera.near = -1.0f;
        CHECK(invalidCurrentCamera->look(Camera{}, CameraView::TwoD, 1.0f, Easing::Linear)
              == Result::InvalidArguments);
        CHECK(near(invalidCurrentCamera->duration(), 0.0f));
    }
    delete invalidCurrentCamera;

    Config authoredCameraConfig;
    authoredCameraConfig.width = 64;
    authoredCameraConfig.height = 64;
    auto authoredCamera = Scene::gen(authoredCameraConfig);
    auto authoredRectangle = Rectangle::gen({}, {2.0f, 2.0f});
    CHECK(authoredCamera && authoredRectangle);
    if (authoredCamera && authoredRectangle) {
        authoredRectangle->style.stroke.a = 0;
        authoredRectangle->fill(Color::hex("#4cc9f0"));
        auto rectangleResult = authoredCamera->add(authoredRectangle);
        CHECK(rectangleResult == Result::Success);
        if (rectangleResult == Result::Success) {
            authoredCamera->config().camera.orthoHeight = 4.0f;
            CHECK(authoredCamera->wait(1.0f) == Result::Success);
            Surface authoredSurface;
            CHECK(renderer->render(authoredCamera, 0.5f, authoredSurface) == Result::Success);
            auto authoredFrame = checksum(authoredSurface);
            auto wider = authoredCamera->config().camera;
            wider.orthoHeight = 8.0f;
            CHECK(authoredCamera->look(wider, CameraView::TwoD, 1.0f, Easing::Linear)
                  == Result::Success);
            CHECK(renderer->render(authoredCamera, 0.5f, authoredSurface) == Result::Success);
            CHECK(checksum(authoredSurface) == authoredFrame);
            CHECK(renderer->render(authoredCamera, 1.5f, authoredSurface) == Result::Success);
            CHECK(checksum(authoredSurface) != authoredFrame);
        } else {
            delete authoredRectangle;
        }
    } else {
        delete authoredRectangle;
    }
    delete authoredCamera;

    auto authoredView = Scene::gen();
    CHECK(authoredView != nullptr);
    if (authoredView) {
        authoredView->config().cameraView = CameraView::ThreeD;
        authoredView->config().camera.projection = Projection::Perspective;
        authoredView->config().camera.eye = {6.0f, 4.0f, 7.0f};
        CHECK(authoredView->wait(1.0f) == Result::Success);
        CHECK(authoredView->view(0.0f) == CameraView::ThreeD);
        auto lens = authoredView->config().camera;
        lens.fov = 0.9f;
        CHECK(authoredView->look(lens, CameraView::ThreeD, 1.0f, Easing::Linear)
              == Result::Success);
        CHECK(authoredView->view(0.0f) == CameraView::ThreeD);
    }
    delete authoredView;

    config.cameraMode = CameraMode::Interactive;
    auto interactive = Scene::gen(config);
    CHECK(interactive != nullptr);
    if (interactive) {
        CHECK(interactive->camera({CameraAction::View3D, {}}) == Result::NonSupport);
        CHECK(interactive->camera({CameraAction::Orbit, {0.1f, -0.05f}})
              == Result::NonSupport);
        CHECK(interactive->look(spatialCamera, CameraView::ThreeD) == Result::InsufficientCondition);
    }
    delete interactive;
}

static void viewports(SwRenderer* renderer)
{
    auto parent = Scene::gen();
    auto child = Scene::gen();
    auto other = Scene::gen();
    auto childObject = Rectangle::gen();
    CHECK(parent && child && other && childObject);
    if (parent && child && other && childObject) {
        CHECK(child->add(childObject) == Result::Success);
        Viewport viewport;
        viewport.width = 0.0f;
        CHECK(parent->viewport(child, viewport) == Result::InvalidArguments);
        viewport.width = 0.5f;
        viewport.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(parent->viewport(child, viewport) == Result::InvalidArguments);
        viewport.x = 0.75f;
        CHECK(parent->viewport(child, viewport) == Result::InvalidArguments);
        viewport = {0.25f, 0.25f, 0.5f, 0.5f};
        CHECK(parent->viewport(parent, viewport) == Result::InvalidArguments);
        CHECK(parent->viewport(child, viewport) == Result::Success);
        CHECK(parent->viewportCount() == 1);
        CHECK(parent->sceneAt(0) == child);
        CHECK(child->update(childObject) == Result::InsufficientCondition);
        CHECK(child->wait(0.1f) == Result::InsufficientCondition);
        CHECK(child->play(Animation::create(childObject)) == Result::InsufficientCondition);
        auto lateObject = Rectangle::gen();
        CHECK(lateObject != nullptr);
        if (lateObject) {
            auto result = child->add(lateObject);
            CHECK(result == Result::InsufficientCondition);
            if (result != Result::Success) delete lateObject;
        }
        auto lateScene = Scene::gen();
        CHECK(lateScene != nullptr);
        if (lateScene) {
            auto result = child->viewport(lateScene, {});
            CHECK(result == Result::InsufficientCondition);
            if (result != Result::Success) delete lateScene;
        }
        Viewport actual;
        CHECK(parent->viewportAt(0, actual));
        CHECK(actual.x == 0.25f);
        CHECK(!parent->viewportAt(1, actual));
        auto boundary = Scene::gen();
        CHECK(boundary != nullptr);
        if (boundary) {
            auto result = parent->viewport(boundary, {0.8f, 0.0f, 0.2f, 1.0f});
            CHECK(result == Result::Success);
            if (result != Result::Success) delete boundary;
        }
        auto thirds = Scene::gen();
        CHECK(thirds != nullptr);
        if (thirds) {
            auto result = parent->viewport(thirds, {2.0f / 3.0f, 0.0f, 1.0f / 3.0f, 1.0f});
            CHECK(result == Result::Success);
            if (result != Result::Success) delete thirds;
        }
        CHECK(other->viewport(child, viewport) == Result::InvalidArguments);
        CHECK(child->viewport(parent, viewport) == Result::InsufficientCondition);
    }
    delete other;
    delete parent;
    if (!parent || !child || !other || !childObject) {
        delete child;
        delete childObject;
        return;
    }

    Config rootConfig;
    rootConfig.width = 240;
    rootConfig.height = 120;
    rootConfig.background = Color::hex("#111820");
    auto root = Scene::gen(rootConfig);
    Config leftConfig;
    leftConfig.width = 120;
    leftConfig.height = 120;
    leftConfig.background = Color::hex("#d84040");
    leftConfig.camera.orthoHeight = 4.0f;
    auto left = Scene::gen(leftConfig);
    Config rightConfig = leftConfig;
    rightConfig.background = Color::hex("#4080d8");
    rightConfig.camera.orthoHeight = 8.0f;
    auto right = Scene::gen(rightConfig);
    CHECK(root && left && right);
    if (root && left && right) {
        CHECK(root->wait(0.2f) == Result::Success);
        CHECK(left->wait(0.8f) == Result::Success);
        CHECK(right->wait(1.4f) == Result::Success);
        CHECK(root->viewport(left, {0.0f, 0.0f, 0.5f, 1.0f}) == Result::Success);
        CHECK(root->viewport(right, {0.5f, 0.0f, 0.5f, 1.0f}) == Result::Success);
        CHECK(root->duration() == 1.4f);
        CHECK(root->sceneAt(0)->config().camera.orthoHeight == 4.0f);
        CHECK(root->sceneAt(1)->config().camera.orthoHeight == 8.0f);
        Surface surface;
        CHECK(renderer->render(root, root->duration(), surface) == Result::Success);
        if (surface.data()) {
            CHECK(surface.data()[60u * surface.stride() + 30u] == pixel(leftConfig.background));
            CHECK(surface.data()[60u * surface.stride() + 210u] == pixel(rightConfig.background));
        }
        root->config().width = 120;
        root->config().height = 60;
        CHECK(renderer->render(root, root->duration(), surface) == Result::Success);
        CHECK(surface.width() == 120 && surface.height() == 60);
        if (surface.data()) {
            CHECK(surface.data()[30u * surface.stride() + 15u] == pixel(leftConfig.background));
            CHECK(surface.data()[30u * surface.stride() + 105u] == pixel(rightConfig.background));
        }
    } else {
        delete left;
        delete right;
    }
    delete root;

    Config clipConfig;
    clipConfig.width = 100;
    clipConfig.height = 100;
    clipConfig.background = Color::hex("#101010");
    auto clipRoot = Scene::gen(clipConfig);
    auto clipChild = Scene::gen(clipConfig);
    auto clipSpace = _space();
    auto circle = Circle::gen({}, 10.0f);
    CHECK(clipRoot && clipChild && clipSpace && circle);
    if (clipRoot && clipChild && clipSpace && circle) {
        clipChild->config().background.a = 0;
        circle->style.stroke.a = 0;
        circle->fill(Color::hex("#40d8a0"));
        CHECK(clipSpace->add(circle) == Result::Success);
        CHECK(clipChild->add(clipSpace) == Result::Success);
        CHECK(clipRoot->viewport(clipChild, {0.25f, 0.25f, 0.5f, 0.5f}) == Result::Success);
        Surface surface;
        CHECK(renderer->render(clipRoot, 0.0f, surface) == Result::Success);
        if (surface.data()) {
            CHECK(surface.data()[50u * surface.stride() + 10u] == pixel(clipConfig.background));
            CHECK(surface.data()[50u * surface.stride() + 50u] == pixel(Color::hex("#40d8a0")));
            CHECK(surface.data()[50u * surface.stride() + 90u] == pixel(clipConfig.background));
        }
    } else {
        delete clipChild;
        delete clipSpace;
        delete circle;
    }
    delete clipRoot;

    Config groupConfig;
    groupConfig.width = 64;
    groupConfig.height = 64;
    groupConfig.background = Color::hex("#101010");
    groupConfig.camera.orthoHeight = 4.0f;
    auto groupRoot = Scene::gen(groupConfig);
    auto first = Scene::gen(groupConfig);
    auto second = Scene::gen(groupConfig);
    auto firstSpace = _space();
    auto secondSpace = _space();
    auto firstCircle = Circle::gen({}, 1.5f);
    auto secondCircle = Circle::gen({}, 1.0f);
    CHECK(groupRoot && first && second && firstSpace && secondSpace && firstCircle && secondCircle);
    if (groupRoot && first && second && firstSpace && secondSpace && firstCircle && secondCircle) {
        first->config().background.a = 0;
        second->config().background.a = 0;
        firstCircle->style.stroke.a = 0;
        secondCircle->style.stroke.a = 0;
        firstCircle->fill(Color::hex("#d84040"));
        secondCircle->fill(Color::hex("#4080d8"));
        firstCircle->layer = 100;
        secondCircle->layer = -100;
        CHECK(firstSpace->add(firstCircle) == Result::Success);
        CHECK(secondSpace->add(secondCircle) == Result::Success);
        CHECK(first->add(firstSpace) == Result::Success);
        CHECK(second->add(secondSpace) == Result::Success);
        CHECK(groupRoot->viewport(first, {}) == Result::Success);
        CHECK(groupRoot->viewport(second, {}) == Result::Success);
        Surface surface;
        CHECK(renderer->render(groupRoot, 0.0f, surface) == Result::Success);
        if (surface.data()) {
            CHECK(surface.data()[32u * surface.stride() + 32u] == pixel(Color::hex("#4080d8")));
            CHECK(surface.data()[2u * surface.stride() + 2u] == pixel(groupConfig.background));
        }
    } else {
        delete first;
        delete second;
        delete firstSpace;
        delete secondSpace;
        delete firstCircle;
        delete secondCircle;
    }
    delete groupRoot;

    auto nestedRoot = Scene::gen(clipConfig);
    auto middle = Scene::gen(clipConfig);
    auto leaf = Scene::gen(clipConfig);
    CHECK(nestedRoot && middle && leaf);
    if (nestedRoot && middle && leaf) {
        middle->config().background.a = 0;
        leaf->config().background = Color::hex("#7bd88f");
        CHECK(middle->viewport(leaf, {0.5f, 0.5f, 0.5f, 0.5f}) == Result::Success);
        CHECK(nestedRoot->viewport(middle, {0.0f, 0.0f, 0.5f, 0.5f}) == Result::Success);
        Surface surface;
        CHECK(renderer->render(nestedRoot, 0.0f, surface) == Result::Success);
        if (surface.data()) {
            CHECK(surface.data()[37u * surface.stride() + 37u] == pixel(Color::hex("#7bd88f")));
            CHECK(surface.data()[10u * surface.stride() + 10u] == pixel(clipConfig.background));
            CHECK(surface.data()[60u * surface.stride() + 60u] == pixel(clipConfig.background));
        }
    } else {
        delete middle;
        delete leaf;
    }
    delete nestedRoot;

    auto timedRoot = Scene::gen(groupConfig);
    auto timedChild = Scene::gen(groupConfig);
    auto timedSpace = _space();
    auto timedCircle = Circle::gen({}, 1.25f);
    CHECK(timedRoot && timedChild && timedSpace && timedCircle);
    if (timedRoot && timedChild && timedSpace && timedCircle) {
        timedChild->config().background.a = 0;
        timedCircle->style.stroke.a = 0;
        timedCircle->fill(Color::hex("#ffd166"));
        CHECK(timedSpace->add(timedCircle) == Result::Success);
        CHECK(timedChild->add(timedSpace) == Result::Success);
        CHECK(timedChild->play(Animation::create(timedCircle), 1.0f, Easing::Linear) == Result::Success);
        CHECK(timedRoot->viewport(timedChild, {}) == Result::Success);
        CHECK(timedRoot->duration() == 1.0f);
        Surface surface;
        CHECK(renderer->render(timedRoot, 0.0f, surface) == Result::Success);
        auto begin = checksum(surface);
        CHECK(renderer->render(timedRoot, 1.0f, surface) == Result::Success);
        CHECK(checksum(surface) != begin);
    } else {
        delete timedChild;
        delete timedSpace;
        delete timedCircle;
    }
    delete timedRoot;

    auto capacityRoot = Scene::gen();
    CHECK(capacityRoot != nullptr);
    if (capacityRoot) {
        for (auto i = 0u; i < 64u; i++) {
            auto item = Scene::gen();
            CHECK(item != nullptr);
            if (!item) break;
            auto result = capacityRoot->viewport(item, {});
            CHECK(result == Result::Success);
            if (result != Result::Success) delete item;
        }
        auto overflow = Scene::gen();
        CHECK(overflow != nullptr);
        if (overflow) {
            auto result = capacityRoot->viewport(overflow, {});
            CHECK(result == Result::InsufficientCondition);
            if (result != Result::Success) delete overflow;
        }
    }
    delete capacityRoot;

    auto chain = Scene::gen();
    CHECK(chain != nullptr);
    if (chain) {
        for (auto i = 0u; i < 15u; i++) {
            auto wrapper = Scene::gen();
            CHECK(wrapper != nullptr);
            if (!wrapper) break;
            auto result = wrapper->viewport(chain, {});
            CHECK(result == Result::Success);
            if (result != Result::Success) {
                delete wrapper;
                break;
            }
            chain = wrapper;
        }
        auto tooDeep = Scene::gen();
        CHECK(tooDeep != nullptr);
        if (tooDeep) {
            auto result = tooDeep->viewport(chain, {});
            CHECK(result == Result::InsufficientCondition);
            if (result == Result::Success) chain = nullptr;
            delete tooDeep;
        }
    }
    delete chain;

    auto budgetRoot = Scene::gen(groupConfig);
    auto sparse = Scene::gen(groupConfig);
    auto dense = Scene::gen(groupConfig);
    auto sparseSpace = _space();
    auto denseSpace = _space();
    auto sparseCircle = Circle::gen({}, 1.0f);
    auto cellA = Cell::gen({}, 128, 128, Color::hex("#ffffff"));
    auto cellB = Cell::gen({}, 128, 128, Color::hex("#ffffff"));
    auto cellC = Cell::gen({}, 128, 128, Color::hex("#ffffff"));
    auto cellD = Cell::gen({}, 128, 128, Color::hex("#ffffff"));
    CHECK(budgetRoot && sparse && dense && sparseSpace && denseSpace && sparseCircle
          && cellA && cellB && cellC && cellD);
    if (budgetRoot && sparse && dense && sparseSpace && denseSpace && sparseCircle
        && cellA && cellB && cellC && cellD) {
        sparseSpace->progress = 0.0f;
        denseSpace->progress = 0.0f;
        CHECK(sparseSpace->add(sparseCircle) == Result::Success);
        CHECK(denseSpace->add(cellA) == Result::Success);
        CHECK(denseSpace->add(cellB) == Result::Success);
        CHECK(denseSpace->add(cellC) == Result::Success);
        CHECK(denseSpace->add(cellD) == Result::Success);
        CHECK(sparse->add(sparseSpace) == Result::Success);
        CHECK(dense->add(denseSpace) == Result::Success);
        CHECK(budgetRoot->viewport(sparse, {}) == Result::Success);
        CHECK(budgetRoot->viewport(dense, {}) == Result::Success);
        Surface surface;
        CHECK(renderer->render(budgetRoot, 0.0f, surface) == Result::InsufficientCondition);
    } else {
        delete sparse;
        delete dense;
        delete sparseSpace;
        delete denseSpace;
        delete sparseCircle;
        delete cellA;
        delete cellB;
        delete cellC;
        delete cellD;
    }
    delete budgetRoot;
}

static void liveSceneTransitions(SwRenderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.cameraMode = CameraMode::Fixed;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    TransitionKind kinds[] = {TransitionKind::Match, TransitionKind::Fade, TransitionKind::Slide,
                              TransitionKind::ZoomIn, TransitionKind::ZoomOut,
                              TransitionKind::Slide, TransitionKind::ZoomIn};
    auto variant = 0u;
    for (auto kind : kinds) {
        auto root = Scene::gen(config);
        Scene* stages[] = {Scene::gen(config), Scene::gen(config), Scene::gen(config)};
        for (auto i = 0u; i < 3u; i++) {
            auto dot = Circle::gen({-1.0f, 0.0f}, 0.2f);
            dot->tag("dot");
            dot->fill(Color::hex("#ffffff")).stroke(Color::hex("#00000000"), 0);
            CHECK(stages[i]->add(dot) == Result::Success);
            CHECK(stages[i]->play(Animation::shift(dot, {i ? 1.0f : 4.0f, 0.0f}),
                                  i ? 0.5f : 4.0f, Easing::Linear) == Result::Success);
        }
        float holds[] = {2.0f, 1.0f, 1.0f};
        TransitionStyle style;
        style.kind = kind;
        style.match = variant++ >= 5u;
        style.area = {0.5f, 0.25f, 0.25f, 0.25f};
        auto curve = AnimCurve::preset(AnimCurvePreset::Linear);
        CHECK(root->transition(stages, 3u, {}, 1, holds, 2u, curve, &style, 1u)
              == Result::InvalidArguments);
        holds[1] = -1;
        CHECK(root->transition(stages, 3u, {}, 1, holds, 3u, curve, &style, 1u)
              == Result::InvalidArguments);
        holds[1] = 1;
        CHECK(root->transition(stages, 3u, {}, 1, holds, 3u, curve, &style, 1u)
              == Result::Success);
        CHECK(near(root->duration(), 6.0f));
        holds[0] = 99;
        CHECK(stages[0]->wait(1) == Result::InsufficientCondition);
        LayoutReport report;
        Surface surface;
        CHECK(renderer->render(root, 1, surface) == Result::Success);
        auto opening = checksum(surface);
        CHECK((surface.data()[60u * surface.stride() + 120u] & 0x00ffffffu) == 0x00ffffffu);
        CHECK(renderer->layout(root, 1, report) == Result::Success);
        CHECK(report.sceneCount() == 2u && near(report.sceneAt(1)->time, 1));
        CHECK(report.objectCount() == 1u && near(report.objectAt(0)->paintBounds.x, 114, 0.5f));
        CHECK(renderer->render(root, 2.25f, surface) == Result::Success);
        CHECK(checksum(surface) != opening);
        CHECK(renderer->layout(root, 2.25f, report) == Result::Success);
        CHECK(report.sceneCount() == 3u);
        for (auto i = 1u; i < report.sceneCount(); i++) {
            auto item = report.sceneAt(i);
            CHECK(near(item->time, item->scene == stages[0] ? 2.25f : 0.25f));
        }
        if (style.match || kind == TransitionKind::Match) {
            CHECK(report.visualCount() == 1u);
            CHECK(report.visualAt(0)->kind == LayoutVisualKind::Morph);
            CHECK(near(report.visualAt(0)->paintBounds.x + report.visualAt(0)->paintBounds.width / 2,
                       144.375f, 0.5f));
        }
        CHECK(renderer->layout(root, 3.5f, report) == Result::Success);
        CHECK(report.sceneCount() == 2u && near(report.sceneAt(1)->time, 0.5f));
        CHECK(renderer->render(root, 5.5f, surface) == Result::Success);
        auto ending = checksum(surface);
        CHECK(renderer->render(root, 6, surface) == Result::Success);
        CHECK(checksum(surface) == ending);
        CHECK(renderer->render(root, 1, surface) == Result::Success);
        CHECK(checksum(surface) == opening);
        delete root;
    }
    auto root = Scene::gen(config);
    Scene* stages[] = {Scene::gen(config), Scene::gen(config)};
    auto dot = Circle::gen({-1, 0}, 0.2f);
    dot->fill(Color::hex("#ffffff"));
    dot->stroke(Color::hex("#00000000"), 0);
    CHECK(stages[0]->add(dot) == Result::Success);
    CHECK(stages[0]->play(Animation::shift(dot, {2, 0}), 3, Easing::Linear) == Result::Success);
    CHECK(root->transition(stages, 2u, {}, 1, 2, AnimCurve::preset(AnimCurvePreset::Linear),
                           nullptr, 0u, false) == Result::Success);
    LayoutReport report;
    CHECK(renderer->layout(root, 1, report) == Result::Success);
    CHECK(near(report.sceneAt(1)->time, 3));
    CHECK(near(report.objectAt(0)->paintBounds.x, 144, 0.5f));
    delete root;
}

static void sceneTransitions(SwRenderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto root = Scene::gen(config);
    auto first = Scene::gen(config);
    auto second = Scene::gen(config);
    auto circle = Circle::gen({-1.4f, 0.0f, 0.0f}, 0.7f);
    auto rectangle = Rectangle::gen({1.25f, 0.0f, 0.0f}, {1.6f, 1.2f}, 0.18f);
    auto oldLabel = Text::gen("SCENE", {-1.4f, -1.25f, 0.0f});
    auto newLabel = Text::gen("SCENE", {1.25f, -1.25f, 0.0f});
    CHECK(root && first && second && circle && rectangle && oldLabel && newLabel);
    if (root && first && second && circle && rectangle && oldLabel && newLabel) {
        first->config().background = Color::hex("#111827");
        second->config().background = Color::hex("#172033");
        circle->fill(Color::hex("#4cc9f0")).stroke(Color::hex("#4cc9f0"), 3.0f).tag("hero");
        rectangle->fill(Color::hex("#f72585")).stroke(Color::hex("#f72585"), 3.0f).tag("hero");
        oldLabel->fill(Color::hex("#f4f7fb")).tag("label");
        newLabel->fill(Color::hex("#f4f7fb")).tag("label");
        CHECK(first->add(circle) == Result::Success);
        CHECK(first->add(oldLabel) == Result::Success);
        CHECK(second->add(rectangle) == Result::Success);
        CHECK(second->add(newLabel) == Result::Success);
        Scene* stages[] = {first, second};
        Scene* duplicate[] = {first, first};
        CHECK(root->transition(nullptr, 2u, {}, 1.0f, 0.25f, Easing::Linear)
              == Result::InvalidArguments);
        CHECK(root->transition(stages, 1u, {}, 1.0f, 0.25f, Easing::Linear)
              == Result::InvalidArguments);
        CHECK(root->transition(duplicate, 2u, {}, 1.0f, 0.25f, Easing::Linear)
              == Result::InvalidArguments);
        CHECK(root->transition(stages, 2u, {}, 1.0f, 0.25f, Easing::Linear)
              == Result::Success);
        CHECK(near(root->duration(), 1.5f));
        CHECK(root->transitionCount() == 2u);
        CHECK(root->transitionAt(0u) == first);
        CHECK(root->transitionAt(1u) == second);
        CHECK(root->transitionAt(2u) == nullptr);
        CHECK(first->wait(0.1f) == Result::InsufficientCondition);
        Surface surface;
        CHECK(renderer->render(root, 0.1f, surface) == Result::Success);
        auto from = checksum(surface);
        CHECK(renderer->render(root, 0.75f, surface) == Result::Success);
        auto middle = checksum(surface);
        CHECK(renderer->render(root, 1.4f, surface) == Result::Success);
        auto to = checksum(surface);
        CHECK(from != middle && middle != to && from != to);

        LayoutReport report;
        CHECK(renderer->layout(root, 0.75f, report) == Result::Success);
        CHECK(report.sceneCount() == 3u && report.objectCount() == 4u
              && report.visualCount() == 2u);
        if (report.sceneCount() == 3u) {
            CHECK(std::strcmp(report.sceneAt(1u)->path, "root/transition:0") == 0);
            CHECK(std::strcmp(report.sceneAt(2u)->path, "root/transition:1") == 0);
        }
        auto visibleHeroes = 0u;
        auto visibleLabels = 0u;
        auto morphs = 0u;
        auto fadeOuts = 0u;
        auto fadeIns = 0u;
        for (auto i = 0u; i < report.objectCount(); i++) {
            auto item = report.objectAt(i);
            auto tag = item && item->object ? item->object->tag() : nullptr;
            if (!item || !item->visible || !tag) continue;
            if (std::strcmp(tag, "hero") == 0) visibleHeroes++;
            else if (std::strcmp(tag, "label") == 0) visibleLabels++;
        }
        CHECK(visibleHeroes == 2u);
        CHECK(visibleLabels == 2u);
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            CHECK(visual && visual->visible && visual->object < report.objectCount());
            if (!visual) continue;
            if (visual->kind == LayoutVisualKind::Morph) {
                morphs++;
                CHECK(visual->counterpart < report.objectCount());
                auto source = report.objectAt(visual->object);
                auto target = report.objectAt(visual->counterpart);
                CHECK(source && target);
                if (source && source->object == oldLabel) {
                    CHECK(target->object == newLabel);
                } else if (source) {
                    CHECK(source->object == circle && target->object == rectangle);
                }
            } else if (visual->kind == LayoutVisualKind::FadeOut) {
                fadeOuts++;
                CHECK(visual->counterpart < report.objectCount());
            } else if (visual->kind == LayoutVisualKind::FadeIn) {
                fadeIns++;
                CHECK(visual->counterpart < report.objectCount());
            }
        }
        CHECK(morphs == 2u && fadeOuts == 0u && fadeIns == 0u);
        CHECK(report.visualAt(report.visualCount()) == nullptr);

        CHECK(renderer->layout(root, 0.25f, report) == Result::Success);
        CHECK(report.sceneCount() == 2u && report.objectCount() == 2u
              && report.visualCount() == 2u);
        if (report.sceneCount() == 2u) {
            CHECK(std::strcmp(report.sceneAt(1u)->path, "root/transition:0") == 0);
        }
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            CHECK(visual && visual->kind == LayoutVisualKind::Stable
                  && visual->counterpart == UINT32_MAX);
        }

        CHECK(renderer->layout(root, 1.25f, report) == Result::Success);
        CHECK(report.sceneCount() == 2u && report.objectCount() == 2u
              && report.visualCount() == 2u);
        if (report.sceneCount() == 2u) {
            CHECK(std::strcmp(report.sceneAt(1u)->path, "root/transition:1") == 0);
        }
    } else {
        delete first;
        delete second;
        delete circle;
        delete rectangle;
        delete oldLabel;
        delete newLabel;
    }
    delete root;
}

static void transitionLayoutVisuals(SwRenderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;

    auto dissolveRoot = Scene::gen(config);
    auto dissolveFrom = Scene::gen(config);
    auto dissolveTo = Scene::gen(config);
    auto oldText = Text::gen("OLD", {});
    auto newText = Text::gen("NEW", {});
    CHECK(dissolveRoot && dissolveFrom && dissolveTo && oldText && newText);
    if (dissolveRoot && dissolveFrom && dissolveTo && oldText && newText) {
        oldText->tag("copy").fill(Color::hex("#ffffff"));
        newText->tag("copy").fill(Color::hex("#ffffff"));
        oldText->size = 24.0f;
        newText->size = 24.0f;
        CHECK(oldText->font("Pretendard") == Result::Success);
        CHECK(newText->font("Pretendard") == Result::Success);
        CHECK(dissolveFrom->add(oldText) == Result::Success);
        CHECK(dissolveTo->add(newText) == Result::Success);
        Scene* stages[] = {dissolveFrom, dissolveTo};
        CHECK(dissolveRoot->transition(stages, 2u, {}, 1.0f, 0.0f,
                                       Easing::Linear) == Result::Success);

        LayoutReport report;
        CHECK(renderer->layout(dissolveRoot, 0.25f, report) == Result::Success);
        CHECK(report.sceneCount() == 3u && report.objectCount() == 2u
              && report.visualCount() == 2u);
        auto visibleFadeOut = 0u;
        auto visibleFadeIn = 0u;
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            CHECK(visual && visual->counterpart < report.objectCount());
            if (!visual) continue;
            if (visual->kind == LayoutVisualKind::FadeOut && visual->visible) {
                visibleFadeOut++;
                CHECK(near(visual->opacity, 128.0f / 255.0f));
            }
            if (visual->kind == LayoutVisualKind::FadeIn && visual->visible) {
                visibleFadeIn++;
            }
        }
        CHECK(visibleFadeOut == 1u && visibleFadeIn == 0u);

        CHECK(renderer->layout(dissolveRoot, 0.5f, report) == Result::Success);
        CHECK(report.visualCount() == 2u);
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            CHECK(visual && !visual->visible && near(visual->opacity, 0.0f));
        }
        for (auto i = 0u; i < report.objectCount(); i++) {
            auto object = report.objectAt(i);
            CHECK(object && !object->visible);
        }

        CHECK(renderer->layout(dissolveRoot, 0.75f, report) == Result::Success);
        visibleFadeOut = 0u;
        visibleFadeIn = 0u;
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            if (!visual || !visual->visible) continue;
            if (visual->kind == LayoutVisualKind::FadeOut) visibleFadeOut++;
            if (visual->kind == LayoutVisualKind::FadeIn) {
                visibleFadeIn++;
                CHECK(near(visual->opacity, 128.0f / 255.0f));
            }
        }
        CHECK(visibleFadeOut == 0u && visibleFadeIn == 1u);
    } else {
        delete dissolveFrom;
        delete dissolveTo;
        delete oldText;
        delete newText;
    }
    delete dissolveRoot;

    auto fadeRoot = Scene::gen(config);
    auto fadeFrom = Scene::gen(config);
    auto fadeTo = Scene::gen(config);
    auto incompatibleFrom = Text::gen("TYPE", {-1.0f, 0.0f, 0.0f});
    auto incompatibleTo = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto unmatchedFrom = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    auto unmatchedTo = Circle::gen({2.0f, 0.0f, 0.0f}, 0.25f);
    CHECK(fadeRoot && fadeFrom && fadeTo && incompatibleFrom && incompatibleTo
          && unmatchedFrom && unmatchedTo);
    if (fadeRoot && fadeFrom && fadeTo && incompatibleFrom && incompatibleTo
        && unmatchedFrom && unmatchedTo) {
        incompatibleFrom->tag("incompatible").fill(Color::hex("#ffffff"));
        incompatibleTo->tag("incompatible").fill(Color::hex("#ffffff"));
        unmatchedFrom->tag("outgoing").fill(Color::hex("#ffffff"));
        unmatchedTo->tag("incoming").fill(Color::hex("#ffffff"));
        incompatibleFrom->size = 24.0f;
        CHECK(incompatibleFrom->font("Pretendard") == Result::Success);
        incompatibleTo->style.stroke.a = 0u;
        unmatchedFrom->style.stroke.a = 0u;
        unmatchedTo->style.stroke.a = 0u;
        CHECK(fadeFrom->add(incompatibleFrom) == Result::Success);
        CHECK(fadeFrom->add(unmatchedFrom) == Result::Success);
        CHECK(fadeTo->add(incompatibleTo) == Result::Success);
        CHECK(fadeTo->add(unmatchedTo) == Result::Success);
        Scene* stages[] = {fadeFrom, fadeTo};
        CHECK(fadeRoot->transition(stages, 2u, {}, 1.0f, 0.0f,
                                   Easing::Linear) == Result::Success);

        LayoutReport report;
        CHECK(renderer->layout(fadeRoot, 0.5f, report) == Result::Success);
        CHECK(report.objectCount() == 4u && report.visualCount() == 4u);
        auto paired = 0u;
        auto oneSided = 0u;
        auto fadeOut = 0u;
        auto fadeIn = 0u;
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            CHECK(visual && visual->visible && near(visual->opacity, 128.0f / 255.0f));
            if (!visual) continue;
            if (visual->counterpart == UINT32_MAX) oneSided++;
            else paired++;
            if (visual->kind == LayoutVisualKind::FadeOut) fadeOut++;
            if (visual->kind == LayoutVisualKind::FadeIn) fadeIn++;
        }
        CHECK(paired == 2u && oneSided == 2u && fadeOut == 2u && fadeIn == 2u);
    } else {
        delete fadeFrom;
        delete fadeTo;
        delete incompatibleFrom;
        delete incompatibleTo;
        delete unmatchedFrom;
        delete unmatchedTo;
    }
    delete fadeRoot;

    Config rootConfig;
    rootConfig.width = 400;
    rootConfig.height = 200;
    rootConfig.camera.orthoHeight = 4.0f;
    rootConfig.antialiasing = false;
    Config fromConfig;
    fromConfig.width = 100;
    fromConfig.height = 100;
    fromConfig.camera.orthoHeight = 4.0f;
    fromConfig.antialiasing = false;
    Config toConfig = fromConfig;
    toConfig.width = 200;

    auto dimensionRoot = Scene::gen(rootConfig);
    auto dimensionFrom = Scene::gen(fromConfig);
    auto dimensionTo = Scene::gen(toConfig);
    auto fromCircle = Circle::gen({}, 0.5f);
    auto toCircle = Circle::gen({}, 0.5f);
    CHECK(dimensionRoot && dimensionFrom && dimensionTo && fromCircle && toCircle);
    if (dimensionRoot && dimensionFrom && dimensionTo && fromCircle && toCircle) {
        fromCircle->tag("shape").fill(Color::hex("#ffffff"));
        toCircle->tag("shape").fill(Color::hex("#ffffff"));
        fromCircle->style.stroke.a = 0u;
        toCircle->style.stroke.a = 0u;
        CHECK(dimensionFrom->add(fromCircle) == Result::Success);
        CHECK(dimensionTo->add(toCircle) == Result::Success);
        Scene* stages[] = {dimensionFrom, dimensionTo};
        CHECK(dimensionRoot->transition(stages, 2u, {}, 1.0f, 0.0f,
                                        Easing::Linear) == Result::Success);

        LayoutReport report;
        CHECK(renderer->layout(dimensionRoot, 0.5f, report) == Result::Success);
        CHECK(report.sceneCount() == 3u && report.objectCount() == 2u
              && report.visualCount() == 1u);
        auto sourceScene = report.sceneAt(1u);
        auto targetScene = report.sceneAt(2u);
        CHECK(sourceScene && targetScene);
        if (sourceScene && targetScene) {
            CHECK(near(sourceScene->scaleX, 4.0f) && near(sourceScene->scaleY, 2.0f));
            CHECK(near(targetScene->scaleX, 2.0f) && near(targetScene->scaleY, 2.0f));
            CHECK(sourceScene->stretched && !targetScene->stretched);
        }
        auto visual = report.visualAt(0u);
        CHECK(visual && visual->kind == LayoutVisualKind::Morph
              && visual->counterpart < report.objectCount() && visual->visible);
        if (visual) {
            CHECK(near(visual->paintBounds.center().x, 200.0f, 0.1f));
            CHECK(near(visual->paintBounds.center().y, 100.0f, 0.1f));
            CHECK(near(visual->paintBounds.width, 75.0f, 0.2f));
            CHECK(near(visual->paintBounds.height, 50.0f, 0.2f));
            auto sourceObject = report.objectAt(visual->object);
            auto targetObject = report.objectAt(visual->counterpart);
            CHECK(sourceObject && targetObject && sourceObject->visible
                  && targetObject->visible);
            if (sourceObject && targetObject) {
                CHECK(near(sourceObject->paintBounds.width, visual->paintBounds.width));
                CHECK(near(targetObject->paintBounds.width, visual->paintBounds.width));
            }
        }
    } else {
        delete dimensionFrom;
        delete dimensionTo;
        delete fromCircle;
        delete toCircle;
    }
    delete dimensionRoot;

    auto nestedRoot = Scene::gen(config);
    auto nestedFrom = Scene::gen(config);
    auto nestedTo = Scene::gen(config);
    Config childConfig;
    childConfig.width = 120;
    childConfig.height = 120;
    childConfig.camera.orthoHeight = 4.0f;
    childConfig.antialiasing = false;
    auto fromChild = Scene::gen(childConfig);
    auto toChild = Scene::gen(childConfig);
    auto fromBox = Rectangle::gen({1.8f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto toBox = Rectangle::gen({-1.8f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(nestedRoot && nestedFrom && nestedTo && fromChild && toChild
          && fromBox && toBox);
    if (nestedRoot && nestedFrom && nestedTo && fromChild && toChild
        && fromBox && toBox) {
        fromBox->tag("nested").fill(Color::hex("#ffffff"));
        toBox->tag("nested").fill(Color::hex("#ffffff"));
        fromBox->style.stroke.a = 0u;
        toBox->style.stroke.a = 0u;
        CHECK(fromChild->add(fromBox) == Result::Success);
        CHECK(toChild->add(toBox) == Result::Success);
        CHECK(nestedFrom->viewport(fromChild, {0.0f, 0.0f, 0.5f, 1.0f})
              == Result::Success);
        CHECK(nestedTo->viewport(toChild, {0.5f, 0.0f, 0.5f, 1.0f})
              == Result::Success);
        Scene* stages[] = {nestedFrom, nestedTo};
        CHECK(nestedRoot->transition(stages, 2u, {}, 1.0f, 0.0f,
                                     Easing::Linear) == Result::Success);

        LayoutReport report;
        CHECK(renderer->layout(nestedRoot, 0.5f, report) == Result::Success);
        CHECK(report.sceneCount() == 5u && report.objectCount() == 2u
              && report.visualCount() == 2u);
        if (report.sceneCount() == 5u) {
            CHECK(std::strcmp(report.sceneAt(2u)->path,
                              "root/transition:0/viewport:0") == 0);
            CHECK(std::strcmp(report.sceneAt(4u)->path,
                              "root/transition:1/viewport:0") == 0);
        }
        auto fadeOut = 0u;
        auto fadeIn = 0u;
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            CHECK(visual && visual->counterpart == UINT32_MAX && visual->visible
                  && visual->clipped && near(visual->opacity, 128.0f / 255.0f));
            if (!visual) continue;
            if (visual->kind == LayoutVisualKind::FadeOut) fadeOut++;
            if (visual->kind == LayoutVisualKind::FadeIn) fadeIn++;
        }
        CHECK(fadeOut == 1u && fadeIn == 1u);
    } else {
        delete nestedFrom;
        delete nestedTo;
        delete fromChild;
        delete toChild;
        delete fromBox;
        delete toBox;
    }
    delete nestedRoot;

    auto backRoot = Scene::gen(config);
    auto backFrom = Scene::gen(config);
    auto backTo = Scene::gen(config);
    auto backFirst = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.2f);
    auto backSecond = Circle::gen({1.0f, 0.0f, 0.0f}, 0.2f);
    CHECK(backRoot && backFrom && backTo && backFirst && backSecond);
    if (backRoot && backFrom && backTo && backFirst && backSecond) {
        backFirst->tag("back").fill(Color::hex("#ffffff"));
        backSecond->tag("back").fill(Color::hex("#ffffff"));
        backFirst->style.stroke.a = 0u;
        backSecond->style.stroke.a = 0u;
        CHECK(backFrom->add(backFirst) == Result::Success);
        CHECK(backTo->add(backSecond) == Result::Success);
        Scene* stages[] = {backFrom, backTo};
        auto curve = AnimCurve::preset(AnimCurvePreset::Back);
        CHECK(backRoot->transition(stages, 2u, {}, 1.0f, 0.0f, curve)
              == Result::Success);
        LayoutReport report;
        CHECK(renderer->layout(backRoot, 0.7f, report) == Result::Success);
        auto visual = report.visualAt(0u);
        auto progress = ease(curve, 0.7f);
        CHECK(progress > 1.0f);
        CHECK(visual && visual->kind == LayoutVisualKind::Morph);
        if (visual) {
            auto expected = 90.0f + (150.0f - 90.0f) * progress;
            CHECK(near(visual->paintBounds.center().x, expected, 0.2f));
        }
    } else {
        delete backFrom;
        delete backTo;
        delete backFirst;
        delete backSecond;
    }
    delete backRoot;

    auto reparentRoot = Scene::gen(config);
    auto reparentFrom = Scene::gen(config);
    auto reparentTo = Scene::gen(config);
    auto sourceParent = Rectangle::gen({}, {1.0f, 1.0f});
    auto sourceChild = Rectangle::gen({}, {1.0f, 1.0f});
    auto targetParent = Rectangle::gen({}, {1.0f, 1.0f});
    auto targetChild = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(reparentRoot && reparentFrom && reparentTo && sourceParent
          && sourceChild && targetParent && targetChild);
    if (reparentRoot && reparentFrom && reparentTo && sourceParent
        && sourceChild && targetParent && targetChild) {
        sourceParent->tag("parent").fill(Color::hex("#ffffff"));
        sourceChild->tag("child").fill(Color::hex("#ffffff"));
        targetParent->tag("parent").fill(Color::hex("#ffffff"));
        targetChild->tag("child").fill(Color::hex("#ffffff"));
        sourceParent->style.stroke.a = 0u;
        sourceChild->style.stroke.a = 0u;
        targetParent->style.stroke.a = 0u;
        targetChild->style.stroke.a = 0u;
        CHECK(sourceParent->add(sourceChild) == Result::Success);
        CHECK(reparentFrom->add(sourceParent) == Result::Success);
        CHECK(reparentTo->add(targetParent) == Result::Success);
        CHECK(reparentTo->add(targetChild) == Result::Success);
        Scene* stages[] = {reparentFrom, reparentTo};
        CHECK(reparentRoot->transition(stages, 2u, {}, 1.0f, 0.0f,
                                       Easing::Linear) == Result::Success);

        LayoutReport report;
        CHECK(renderer->layout(reparentRoot, 0.5f, report) == Result::Success);
        CHECK(report.visualCount() == 2u && report.collisionCount() == 1u);
        auto collision = report.collisionAt(0u);
        CHECK(collision && collision->overlapping);
        if (collision) {
            auto first = report.objectAt(collision->first);
            auto second = report.objectAt(collision->second);
            CHECK(first && second);
            if (first && second) {
                CHECK((first->object == sourceParent && second->object == sourceChild)
                      || (first->object == sourceChild
                          && second->object == sourceParent));
            }
        }
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            CHECK(visual && visual->kind == LayoutVisualKind::Morph
                  && visual->counterpart < report.objectCount());
        }
    } else {
        delete reparentFrom;
        delete reparentTo;
        delete sourceParent;
        delete sourceChild;
        delete targetParent;
        delete targetChild;
    }
    delete reparentRoot;
}

// A stage with a background and a borderless 1 x 1 card tagged card at x, in color.
static Scene* _transitionStage(const Config& config, Color background, float x, Color color,
                               Rectangle** card = nullptr)
{
    auto scene = Scene::gen(config);
    auto rectangle = Rectangle::gen({x, 0.0f, 0.0f}, {1.0f, 1.0f});
    if (rectangle) {
        rectangle->tag("card").fill(color);
        rectangle->style.stroke.a = 0u;
    }
    if (!scene || !rectangle || scene->add(rectangle) != Result::Success) {
        delete rectangle;
        delete scene;
        return nullptr;
    }
    scene->config().background = background;
    if (card) *card = rectangle;
    return scene;
}

// A red root of the size of config showing the scenes in viewport, one second per transition
// on a linear curve, or nullptr after deleting the scenes.
static Scene* _styledRoot(const Config& config, Scene** scenes, uint32_t count,
                          const Viewport& viewport, const TransitionStyle* styles,
                          uint32_t styleCount)
{
    auto root = Scene::gen(config);
    auto complete = root != nullptr;
    for (auto i = 0u; i < count; i++) complete = complete && scenes[i];
    if (complete) {
        root->config().background = Color::hex("#ff0000");
        complete = root->transition(scenes, count, viewport, 1.0f, 0.0f,
                                    AnimCurve::preset(AnimCurvePreset::Linear), styles,
                                    styleCount) == Result::Success;
    }
    if (complete) return root;
    for (auto i = 0u; i < count; i++) delete scenes[i];
    delete root;
    return nullptr;
}

static const LayoutScene* _layoutScene(const LayoutReport& report, const Scene* scene)
{
    for (auto i = 0u; i < report.sceneCount(); i++) {
        auto entry = report.sceneAt(i);
        if (entry && entry->scene == scene) return entry;
    }
    return nullptr;
}

static bool _near(const BBox& box, float x, float y, float width, float height)
{
    return near(box.x, x, 0.01f) && near(box.y, y, 0.01f) && near(box.width, width, 0.01f)
           && near(box.height, height, 0.01f);
}

// Whether a pixel of surface is within two steps of red, green and blue on each channel.
static bool _shade(const Surface& surface, uint32_t x, uint32_t y, int red, int green, int blue)
{
    if (!surface.data() || x >= surface.width() || y >= surface.height()) return false;
    auto value = surface.data()[y * surface.stride() + x];
    auto close = [](uint32_t channel, int expected) {
        return std::abs(static_cast<int>(channel & 0xffu) - expected) <= 2;
    };
    return close(value, red) && close(value >> 8u, green) && close(value >> 16u, blue);
}

// Styles of scene transitions: where each stage stands, at what scale and how opaque, while it
// hands over to the next.
static void transitionStyles(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    // The sequence covers x 20 to 180 and y 20 to 80 of the root, at scale 0.8 x 0.6.
    Viewport viewport = {0.1f, 0.2f, 0.8f, 0.6f};
    auto white = Color::hex("#ffffff");
    auto black = Color::hex("#000000");
    TransitionStyle fade;
    fade.kind = TransitionKind::Fade;

    // Fade: the incoming scene comes in over the outgoing one, which stays whole beneath an
    // opaque background, and cards that share a tag do not pair.
    Rectangle* fromCard = nullptr;
    Rectangle* toCard = nullptr;
    Scene* fading[] = {_transitionStage(config, black, -2.0f, white, &fromCard),
                       _transitionStage(config, white, 2.0f, black, &toCard)};
    auto root = _styledRoot(config, fading, 2u, viewport, &fade, 1u);
    CHECK(root != nullptr);
    if (root) {
        LayoutReport report;
        CHECK(renderer->layout(root, 0.25f, report) == Result::Success);
        CHECK(report.sceneCount() == 3u);
        for (auto stage : fading) {
            auto entry = _layoutScene(report, stage);
            CHECK(entry && _near(entry->bounds, 20.0f, 20.0f, 160.0f, 60.0f));
            CHECK(entry && near(entry->scaleX, 0.8f) && near(entry->scaleY, 0.6f));
        }
        auto leaving = _layoutVisual(report, fromCard);
        auto arriving = _layoutVisual(report, toCard);
        CHECK(leaving && leaving->kind == LayoutVisualKind::FadeOut
              && leaving->counterpart == UINT32_MAX && near(leaving->opacity, 1.0f));
        CHECK(arriving && arriving->kind == LayoutVisualKind::FadeIn
              && arriving->counterpart == UINT32_MAX && near(arriving->opacity, 64.0f / 255.0f));
        // Halfway, white over black dissolves to middle gray, red outside the sequence.
        Surface surface;
        CHECK(renderer->render(root, 0.5f, surface) == Result::Success);
        CHECK(_shade(surface, 100u, 25u, 128, 128, 128));
        CHECK(_shade(surface, 100u, 10u, 255, 0, 0));
        CHECK(renderer->render(root, 0.75f, surface) == Result::Success);
        CHECK(_shade(surface, 100u, 25u, 191, 191, 191));
    }
    delete root;

    // An incoming background that lets the outgoing scene show fades the outgoing one out.
    Scene* clear[] = {_transitionStage(config, black, -2.0f, white, &fromCard),
                      _transitionStage(config, {255, 255, 255, 0}, 2.0f, black)};
    root = _styledRoot(config, clear, 2u, viewport, &fade, 1u);
    CHECK(root != nullptr);
    if (root) {
        LayoutReport report;
        CHECK(renderer->layout(root, 0.25f, report) == Result::Success);
        auto leaving = _layoutVisual(report, fromCard);
        CHECK(leaving && near(leaving->opacity, 191.0f / 255.0f));
    }
    delete root;

    // Slide: a quarter in, the incoming scene has come a quarter of the viewport in from its
    // side and pushed the outgoing one as far out by the other, both opaque and clipped to the
    // viewport. At each side, a pixel of each scene and one past the viewport.
    struct Slide { TransitionSide from; float x; float y; uint32_t in[2]; uint32_t out[2]; uint32_t past[2]; };
    Slide slides[] = {{TransitionSide::Right, 1.0f, 0.0f, {160u, 25u}, {100u, 25u}, {190u, 25u}},
                      {TransitionSide::Left, -1.0f, 0.0f, {40u, 25u}, {100u, 25u}, {10u, 25u}},
                      {TransitionSide::Bottom, 0.0f, 1.0f, {100u, 72u}, {100u, 40u}, {100u, 90u}},
                      {TransitionSide::Top, 0.0f, -1.0f, {100u, 27u}, {100u, 45u}, {100u, 10u}}};
    for (auto& slide : slides) {
        TransitionStyle style;
        style.kind = TransitionKind::Slide;
        style.from = slide.from;
        Scene* sliding[] = {_transitionStage(config, black, -2.0f, white, &fromCard),
                            _transitionStage(config, white, 2.0f, black, &toCard)};
        root = _styledRoot(config, sliding, 2u, viewport, &style, 1u);
        CHECK(root != nullptr);
        if (!root) continue;
        LayoutReport report;
        CHECK(renderer->layout(root, 0.25f, report) == Result::Success);
        auto outgoing = _layoutScene(report, sliding[0]);
        auto incoming = _layoutScene(report, sliding[1]);
        BBox frame = {20.0f, 20.0f, 160.0f, 60.0f};
        BBox from = {20.0f - slide.x * 40.0f, 20.0f - slide.y * 15.0f, 160.0f, 60.0f};
        BBox to = {20.0f + slide.x * 120.0f, 20.0f + slide.y * 45.0f, 160.0f, 60.0f};
        auto clipped = [&](const LayoutScene* entry, const BBox& bounds) {
            auto left = std::fmax(bounds.x, frame.x);
            auto top = std::fmax(bounds.y, frame.y);
            auto right = std::fmin(bounds.x + bounds.width, frame.x + frame.width);
            auto bottom = std::fmin(bounds.y + bounds.height, frame.y + frame.height);
            return entry && _near(entry->bounds, bounds.x, bounds.y, bounds.width, bounds.height)
                   && _near(entry->clipBounds, left, top, right - left, bottom - top)
                   && near(entry->scaleX, 0.8f) && near(entry->scaleY, 0.6f);
        };
        CHECK(clipped(outgoing, from));
        CHECK(clipped(incoming, to));
        auto leaving = _layoutVisual(report, fromCard);
        auto arriving = _layoutVisual(report, toCard);
        CHECK(leaving && leaving->kind == LayoutVisualKind::FadeOut && near(leaving->opacity, 1.0f));
        CHECK(arriving && arriving->kind == LayoutVisualKind::FadeIn && near(arriving->opacity, 1.0f));
        Surface surface;
        CHECK(renderer->render(root, 0.25f, surface) == Result::Success);
        CHECK(_shade(surface, slide.in[0], slide.in[1], 255, 255, 255));
        CHECK(_shade(surface, slide.out[0], slide.out[1], 0, 0, 0));
        CHECK(_shade(surface, slide.past[0], slide.past[1], 255, 0, 0));
        delete root;
    }

    // Zoom into the area at x 0.5 to 0.75 and y 0.25 to 0.5 of the viewport, square in its
    // fractions. Halfway, the outer scene has grown twice about the point that brings the area
    // to the whole viewport, a third of the way there, and the inner scene fills the area at
    // twice its first size. Zooming in, the outgoing scene is outer and the incoming one comes
    // in through the area, fading in from a tenth to half of the way; zooming out swaps them,
    // and the outgoing scene fades out from half to nine tenths of the way.
    auto layoutOf = [&](Scene* scene, float time, const Scene* stage) {
        LayoutReport report;
        auto entry = renderer->layout(scene, time, report) == Result::Success
                         ? _layoutScene(report, stage) : nullptr;
        return entry ? *entry : LayoutScene{};
    };
    auto outer = [&](const LayoutScene& entry) {
        return _near(entry.bounds, 20.0f - 320.0f / 3.0f, 0.0f, 320.0f, 120.0f)
               && _near(entry.clipBounds, 20.0f, 20.0f, 160.0f, 60.0f)
               && near(entry.scaleX, 1.6f) && near(entry.scaleY, 1.2f);
    };
    auto inner = [&](const LayoutScene& entry) {
        return _near(entry.bounds, 20.0f + 160.0f / 3.0f, 30.0f, 80.0f, 30.0f)
               && _near(entry.clipBounds, 20.0f + 160.0f / 3.0f, 30.0f, 80.0f, 30.0f)
               && near(entry.scaleX, 0.4f) && near(entry.scaleY, 0.3f);
    };
    TransitionKind zooms[] = {TransitionKind::ZoomIn, TransitionKind::ZoomOut};
    for (auto kind : zooms) {
        TransitionStyle zoom;
        zoom.kind = kind;
        zoom.area = {0.5f, 0.25f, 0.25f, 0.25f};
        auto in = kind == TransitionKind::ZoomIn;
        Scene* zooming[] = {_transitionStage(config, black, -2.0f, white, &fromCard),
                            _transitionStage(config, white, 2.0f, black, &toCard)};
        root = _styledRoot(config, zooming, 2u, viewport, &zoom, 1u);
        CHECK(root != nullptr);
        if (!root) continue;
        auto outgoing = layoutOf(root, 0.5f, zooming[0]);
        auto incoming = layoutOf(root, 0.5f, zooming[1]);
        CHECK(in ? outer(outgoing) && inner(incoming) : inner(outgoing) && outer(incoming));
        LayoutReport report;
        CHECK(renderer->layout(root, in ? 0.3f : 0.7f, report) == Result::Success);
        auto fading = _layoutVisual(report, in ? toCard : fromCard);
        CHECK(fading && near(fading->opacity, 128.0f / 255.0f));
        // The inner scene lies above: inside the area, then outside it in the viewport.
        Surface surface;
        CHECK(renderer->render(root, 0.5f, surface) == Result::Success);
        CHECK(_shade(surface, 113u, 45u, in ? 255 : 0, in ? 255 : 0, in ? 255 : 0));
        CHECK(_shade(surface, 40u, 70u, in ? 0 : 255, in ? 0 : 255, in ? 0 : 255));
        delete root;
    }

    // An object gives the area it covers at the end of its scene, with a margin of a twentieth
    // of its larger side: the card at x 0.1875 to 0.3125 and y 0.375 to 0.625 of the frame
    // zooms like the square at 0.1125, 0.3625 with sides of 0.275.
    {
        TransitionStyle byObject;
        byObject.kind = TransitionKind::ZoomIn;
        TransitionStyle byArea = byObject;
        byArea.area = {0.1125f, 0.3625f, 0.275f, 0.275f};
        Scene* first[] = {_transitionStage(config, black, -2.0f, white, &fromCard),
                          _transitionStage(config, white, 2.0f, black)};
        byObject.object = fromCard;
        Scene* second[] = {_transitionStage(config, black, -2.0f, white),
                           _transitionStage(config, white, 2.0f, black)};
        auto objectRoot = _styledRoot(config, first, 2u, viewport, &byObject, 1u);
        auto areaRoot = _styledRoot(config, second, 2u, viewport, &byArea, 1u);
        CHECK(objectRoot && areaRoot);
        if (objectRoot && areaRoot) {
            for (auto i = 0u; i < 2u; i++) {
                auto object = layoutOf(objectRoot, 0.5f, first[i]);
                auto area = layoutOf(areaRoot, 0.5f, second[i]);
                CHECK(object.bounds.width > 0.0f
                      && _near(object.bounds, area.bounds.x, area.bounds.y, area.bounds.width,
                               area.bounds.height));
            }
        }
        delete objectRoot;
        delete areaRoot;
    }

    // Zero margin uses geometry, independent of stroke width. Resolve the moving card and
    // camera at the stage's end, then widen the non-square region and clamp it as usual.
    for (auto kind : zooms) {
        for (auto width : {0.0f, 3.0f, 18.0f}) {
            for (auto margin : {0.0f, 0.1f, 100.0f}) {
                auto stage = [&](Rectangle** card = nullptr) {
                    Rectangle* object = nullptr;
                    auto scene = _transitionStage(config, black, 0.0f, white, &object);
                    if (!scene) return scene;
                    object->style.stroke = Color::hex("#ff0000");
                    object->style.width = width;
                    CHECK(scene->update(object) == Result::Success);
                    CHECK(scene->play(Animation::shift(object, {1.0f, 1.0f, 0.0f}), 1.0f, Easing::Linear)
                          == Result::Success);
                    auto camera = config.camera;
                    camera.eye.x += 0.5f;
                    camera.target.x += 0.5f;
                    camera.eye.y += 0.5f;
                    camera.target.y += 0.5f;
                    camera.orthoHeight = 2.0f;
                    CHECK(scene->look(camera, CameraView::TwoD, 1.0f, Easing::Linear)
                          == Result::Success);
                    if (card) *card = object;
                    return scene;
                };
                Rectangle* cards[2] = {};
                Scene* first[] = {stage(&cards[0]), stage(&cards[1])};
                Scene* second[] = {stage(), stage()};
                TransitionStyle byObject;
                byObject.kind = kind;
                byObject.object = cards[kind == TransitionKind::ZoomIn ? 0u : 1u];
                byObject.areaMargin = margin;
                auto pad = std::fmin(0.5f * margin, 0.25f);
                TransitionStyle byArea;
                byArea.kind = kind;
                byArea.area = {0.5f - pad, -pad, 0.25f + 2.0f * pad, 0.5f + 2.0f * pad};
                auto objectRoot = _styledRoot(config, first, 2u, viewport, &byObject, 1u);
                auto areaRoot = _styledRoot(config, second, 2u, viewport, &byArea, 1u);
                CHECK(objectRoot && areaRoot);
                if (objectRoot && areaRoot) {
                    for (auto time : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
                        for (auto i = 0u; i < 2u; i++) {
                            auto object = layoutOf(objectRoot, time, first[i]);
                            auto area = layoutOf(areaRoot, time, second[i]);
                            CHECK(_near(object.bounds, area.bounds.x, area.bounds.y,
                                        area.bounds.width, area.bounds.height));
                            CHECK(_near(object.clipBounds, area.clipBounds.x, area.clipBounds.y,
                                        area.clipBounds.width, area.clipBounds.height));
                            CHECK(near(object.scaleX, area.scaleX) && near(object.scaleY, area.scaleY));
                        }
                    }
                    Surface objectPixels, areaPixels;
                    CHECK(renderer->render(objectRoot, 0.5f, objectPixels) == Result::Success);
                    CHECK(renderer->render(areaRoot, 0.5f, areaPixels) == Result::Success);
                    CHECK(objectPixels.data() && areaPixels.data()
                          && objectPixels.stride() == areaPixels.stride()
                          && std::memcmp(objectPixels.data(), areaPixels.data(),
                                         objectPixels.height() * objectPixels.stride() * sizeof(uint32_t)) == 0);
                }
                delete objectRoot;
                delete areaRoot;
            }
        }
    }

    // A slide or a zoom that also matches: the cards that share a tag travel above both scenes
    // in the viewport itself, halfway from white to black at x 90 to 110 and y 42.5 to 57.5,
    // and neither scene draws its own card as it moves.
    TransitionStyle matching[2];
    matching[0].kind = TransitionKind::Slide;
    matching[1].kind = TransitionKind::ZoomIn;
    matching[1].area = {0.5f, 0.25f, 0.25f, 0.25f};
    for (auto& style : matching) {
        style.match = true;
        Scene* pairing[] = {_transitionStage(config, black, 0.0f, white, &fromCard),
                            _transitionStage(config, white, 0.0f, black, &toCard)};
        root = _styledRoot(config, pairing, 2u, viewport, &style, 1u);
        CHECK(root != nullptr);
        if (!root) continue;
        LayoutReport report;
        CHECK(renderer->layout(root, 0.5f, report) == Result::Success);
        auto cards = 0u;
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            auto primary = visual ? report.objectAt(visual->object) : nullptr;
            if (primary && (primary->object == fromCard || primary->object == toCard)) cards++;
        }
        auto pair = _layoutVisual(report, fromCard);
        CHECK(cards == 1u);
        CHECK(pair && pair->kind == LayoutVisualKind::Morph
              && _near(pair->paintBounds, 90.0f, 42.5f, 20.0f, 15.0f));
        Surface surface;
        CHECK(renderer->render(root, 0.5f, surface) == Result::Success);
        CHECK(_shade(surface, 95u, 50u, 128, 128, 128));
        CHECK(_shade(surface, 105u, 50u, 128, 128, 128));
        if (style.kind == TransitionKind::Slide) {
            // The outgoing card would have slid to x 10 to 30.
            CHECK(_shade(surface, 25u, 50u, 0, 0, 0));
        }
        delete root;
    }

    // One style for each transition: fade, slide from the bottom, zoom in and match, each
    // sampled halfway.
    Scene* mixed[] = {_transitionStage(config, black, -2.0f, white),
                      _transitionStage(config, black, 0.0f, white),
                      _transitionStage(config, black, 2.0f, white),
                      _transitionStage(config, black, -1.0f, white),
                      _transitionStage(config, black, 1.0f, white)};
    TransitionStyle slideUp;
    slideUp.kind = TransitionKind::Slide;
    slideUp.from = TransitionSide::Bottom;
    TransitionStyle zoomIn;
    zoomIn.kind = TransitionKind::ZoomIn;
    zoomIn.area = {0.5f, 0.25f, 0.25f, 0.25f};
    TransitionStyle styles[] = {fade, slideUp, zoomIn, {}};
    root = _styledRoot(config, mixed, 5u, viewport, styles, 4u);
    CHECK(root != nullptr);
    if (root) {
        auto morphs = [&](float time) {
            LayoutReport report;
            CHECK(renderer->layout(root, time, report) == Result::Success);
            auto count = 0u;
            for (auto i = 0u; i < report.visualCount(); i++) {
                auto visual = report.visualAt(i);
                if (visual && visual->kind == LayoutVisualKind::Morph) count++;
            }
            return count;
        };
        auto fadeIn = layoutOf(root, 0.5f, mixed[1]);
        CHECK(_near(fadeIn.bounds, 20.0f, 20.0f, 160.0f, 60.0f));
        auto pushed = layoutOf(root, 1.5f, mixed[1]);
        auto rising = layoutOf(root, 1.5f, mixed[2]);
        CHECK(_near(pushed.bounds, 20.0f, -10.0f, 160.0f, 60.0f));
        CHECK(_near(rising.bounds, 20.0f, 50.0f, 160.0f, 60.0f));
        CHECK(outer(layoutOf(root, 2.5f, mixed[2])) && inner(layoutOf(root, 2.5f, mixed[3])));
        CHECK(morphs(0.5f) == 0u && morphs(1.5f) == 0u && morphs(2.5f) == 0u);
        CHECK(morphs(3.5f) == 1u);
    }
    delete root;

    // Styles must come one for all transitions or one for each, and be known.
    Rectangle* lastCard = nullptr;
    Scene* invalid[] = {_transitionStage(config, black, 0.0f, white),
                        _transitionStage(config, white, 0.0f, black),
                        _transitionStage(config, black, 0.0f, white, &lastCard)};
    root = Scene::gen(config);
    CHECK(root && invalid[0] && invalid[1] && invalid[2]);
    if (root && invalid[0] && invalid[1] && invalid[2]) {
        auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
        TransitionStyle three[] = {fade, fade, fade};
        CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, three, 3u)
              == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "one for each transition"));
        CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, nullptr, 1u)
              == Result::InvalidArguments);
        TransitionStyle unknown;
        unknown.kind = static_cast<TransitionKind>(200);
        CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &unknown, 1u)
              == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "transition style must be"));
        TransitionStyle sideways;
        sideways.kind = TransitionKind::Slide;
        sideways.from = static_cast<TransitionSide>(9);
        CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &sideways, 1u)
              == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "a slide comes from"));
        // A zoom needs an area that overlaps the viewport and fits in it, or an object of one
        // of the two scenes of its transition.
        TransitionStyle zoom;
        zoom.kind = TransitionKind::ZoomIn;
        Viewport areas[] = {zoom.area, {0.2f, 0.2f, 0.5f, 1.5f}, {1.5f, 0.2f, 0.1f, 0.1f}};
        for (auto& area : areas) {
            zoom.area = area;
            CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &zoom, 1u)
                  == Result::InvalidArguments);
            CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "a zoom needs an area"));
        }
        zoom.area = {0.25f, 0.25f, 0.5f, 0.5f};
        zoom.object = lastCard;
        CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &zoom, 1u)
              == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "a zoom object must be in one"));
        zoom.object = invalid[0]->object("card");
        for (auto margin : {-1.0f, std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()}) {
            zoom.areaMargin = margin;
            CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &zoom, 1u)
                  == Result::InvalidArguments);
            CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "area margin must be"));
        }
        zoom.areaMargin = 0.0f;
        zoom.object = nullptr;
        CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &zoom, 1u)
              == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "only to an object zoom area"));
        zoom.object = invalid[0]->object("card");
        zoom.kind = TransitionKind::Fade;
        CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &zoom, 1u)
              == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "only to an object zoom area"));
        CHECK(near(root->duration(), 0.0f));
        CHECK(invalid[0]->wait(0.25f) == Result::Success);
        // A fade never matches, and a match already does.
        TransitionStyle pairing[2];
        pairing[0].kind = TransitionKind::Fade;
        for (auto& style : pairing) {
            style.match = true;
            CHECK(root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, &style, 1u)
                  == Result::InvalidArguments);
            CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "only a slide or a zoom"));
        }
        CHECK(root->transitionCount() == 0u);
        auto owned = root->transition(invalid, 3u, {}, 1.0f, 0.0f, linear, three, 2u) == Result::Success;
        CHECK(owned && root->transitionCount() == 3u);
        if (!owned) for (auto stage : invalid) delete stage;
    } else {
        for (auto stage : invalid) delete stage;
    }
    delete root;
}

static void objectTree(SwRenderer* renderer)
{
    auto labelScene = Scene::gen();
    auto labelParent = Rectangle::gen({}, {2.0f, 1.0f});
    auto label = Text::gen("semantic label", {0.0f, 0.75f, 0.0f});
    auto labelOtherParent = Group::gen();
    CHECK(labelScene && labelParent && label && labelOtherParent);
    if (labelScene && labelParent && label && labelOtherParent) {
        label->tag("semantic-label");
        label->role = TextRole::H3;
        label->align = {0.5f, 1.0f};
        CHECK(labelParent->label(nullptr) == Result::InvalidArguments);
        auto parentResult = labelScene->add(labelParent);
        CHECK(parentResult == Result::Success);
        if (parentResult == Result::Success) {
            auto result = labelParent->label(label);
            CHECK(result == Result::Success);
            if (result == Result::Success) {
                CHECK(labelParent->childCount() == 1u && labelParent->childAt(0u) == label);
                CHECK(label->parent() == labelParent && labelScene->object("semantic-label") == label);
                CHECK(labelOtherParent->label(label) == Result::InvalidArguments);
            } else {
                delete label;
            }
        } else {
            delete labelParent;
            delete label;
        }
    } else {
        delete labelParent;
        delete label;
    }
    delete labelOtherParent;
    delete labelScene;

    Config config;
    config.width = 160;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto rectangle = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto circle = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    CHECK(scene && rectangle && circle);
    if (scene && rectangle && circle) {
        rectangle->style.stroke.a = 0;
        rectangle->fill(Color::hex("#ef5350"));
        circle->style.stroke.a = 0;
        circle->fill(Color::hex("#42a5f5"));
        auto rectangleResult = scene->add(rectangle);
        auto circleResult = scene->add(circle);
        CHECK(rectangleResult == Result::Success);
        CHECK(circleResult == Result::Success);
        if (rectangleResult == Result::Success && circleResult == Result::Success) {
            CHECK(scene->count() == 2);
            CHECK(scene->objectAt(0) == rectangle);
            CHECK(scene->objectAt(1) == circle);
            CHECK(rectangle->parent() == nullptr && rectangle->space() == nullptr);
            CHECK(circle->parent() == nullptr && circle->space() == nullptr);
            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) > 1000);
        }
        if (rectangleResult != Result::Success) delete rectangle;
        if (circleResult != Result::Success) delete circle;
    } else {
        delete rectangle;
        delete circle;
    }
    delete scene;

    auto outer = Group::gen();
    auto coordinates = Space::gen({0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f});
    auto inner = Group::gen();
    auto item = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    CHECK(outer && coordinates && inner && item);
    if (outer && coordinates && inner && item) {
        outer->model = Mat4::translate({1.0f, 0.0f, 0.0f});
        coordinates->model = Mat4::scale({2.0f, 2.0f, 1.0f});
        inner->model = Mat4::translate({0.0f, 1.0f, 0.0f});
        coordinates->style.stroke.a = 0;
        coordinates->axisX.a = 0;
        coordinates->axisY.a = 0;
        coordinates->axisZ.a = 0;
        item->style.stroke.a = 0;
        item->fill(Color::hex("#ffd166"));
        auto itemResult = inner->add(item);
        CHECK(itemResult == Result::Success);
        if (itemResult == Result::Success) {
            auto innerResult = coordinates->add(inner);
            CHECK(innerResult == Result::Success);
            if (innerResult == Result::Success) {
                auto coordinatesResult = outer->add(coordinates);
                CHECK(coordinatesResult == Result::Success);
                if (coordinatesResult == Result::Success) {
                    CHECK(coordinates->parent() == outer);
                    CHECK(coordinates->space() == nullptr);
                    CHECK(inner->parent() == coordinates && inner->space() == coordinates);
                    CHECK(item->parent() == inner && item->space() == coordinates);
                    CHECK(outer->childCount() == 1 && outer->childAt(0) == coordinates);
                    CHECK(coordinates->count() == 1 && coordinates->objectAt(0) == inner);
                    auto world = coordinates->c2w({1.0f, 1.0f, 0.0f});
                    CHECK(near(world.x, 3.0f) && near(world.y, 2.0f));
                    Bounds bounds;
                    CHECK(item->bounds(bounds));
                    CHECK(near(bounds.center().x, 1.0f) && near(bounds.center().y, 0.0f));
                    CHECK(near(bounds.size().x, 0.5f) && near(bounds.size().y, 0.5f));
                    CHECK(inner->bounds(bounds));
                    CHECK(near(bounds.center().x, 1.0f) && near(bounds.center().y, 1.0f));
                    CHECK(outer->bounds(bounds));
                    CHECK(near(bounds.min.x, 1.0f) && near(bounds.min.y, 0.0f));
                    CHECK(near(bounds.max.x, 3.5f) && near(bounds.max.y, 2.5f));
                    auto mixedScene = Scene::gen(config);
                    CHECK(mixedScene != nullptr);
                    if (mixedScene) {
                        auto result = mixedScene->add(outer);
                        CHECK(result == Result::Success);
                        if (result == Result::Success) {
                            CHECK(mixedScene->count() == 4);
                            Surface surface;
                            CHECK(renderer->render(mixedScene, 0.0f, surface) == Result::Success);
                            CHECK(changed(surface, config.background) > 0);
                        } else {
                            delete outer;
                        }
                        delete mixedScene;
                        outer = nullptr;
                    }
                }
            }
        }
    }
    delete outer;

    auto self = Group::gen();
    CHECK(self != nullptr);
    if (self) {
        CHECK(self->add(self) == Result::InvalidArguments);
        CHECK(self->childCount() == 0);
    }
    delete self;

    auto ancestor = Group::gen();
    auto descendant = Group::gen();
    CHECK(ancestor && descendant);
    if (ancestor && descendant) {
        auto result = ancestor->add(descendant);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(descendant->add(ancestor) == Result::InvalidArguments);
            CHECK(descendant->childCount() == 0);
        } else {
            delete descendant;
        }
    }
    delete ancestor;

    auto firstParent = Group::gen();
    auto secondParent = Group::gen();
    auto reused = Circle::gen({}, 0.5f);
    CHECK(firstParent && secondParent && reused);
    if (firstParent && secondParent && reused) {
        auto result = firstParent->add(reused);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(secondParent->add(reused) == Result::InvalidArguments);
            CHECK(reused->parent() == firstParent);
        } else {
            delete reused;
        }
    }
    delete firstParent;
    delete secondParent;

    auto firstScene = Scene::gen();
    auto secondScene = Scene::gen();
    auto crossScene = Rectangle::gen();
    CHECK(firstScene && secondScene && crossScene);
    if (firstScene && secondScene && crossScene) {
        auto result = firstScene->add(crossScene);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(secondScene->add(crossScene) == Result::InvalidArguments);
        } else {
            delete crossScene;
        }
    }
    delete secondScene;
    delete firstScene;

    auto deadScene = Scene::gen();
    auto deadParent = Group::gen();
    CHECK(deadScene && deadParent);
    if (deadScene && deadParent) {
        auto result = deadScene->add(deadParent);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(deadScene->remove(deadParent) == Result::Success);
            auto late = Point::gen({});
            CHECK(late != nullptr);
            if (late) {
                CHECK(deadParent->add(late) == Result::InvalidArguments);
                delete late;
            }
        } else {
            delete deadParent;
        }
    }
    delete deadScene;

    auto depthScene = Scene::gen();
    auto depthRoot = Group::gen();
    CHECK(depthScene && depthRoot);
    if (depthScene && depthRoot) {
        auto result = depthScene->add(depthRoot);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            auto cursor = depthRoot;
            auto complete = true;
            for (auto i = 0u; i < 64u; i++) {
                auto next = Group::gen();
                CHECK(next != nullptr);
                if (!next) {
                    complete = false;
                    break;
                }
                auto added = cursor->add(next);
                CHECK(added == Result::Success);
                if (added != Result::Success) {
                    delete next;
                    complete = false;
                    break;
                }
                cursor = next;
            }
            if (complete) {
                auto overflow = Point::gen({});
                CHECK(overflow != nullptr);
                if (overflow) {
                    auto count = depthScene->count();
                    CHECK(cursor->add(overflow) == Result::InsufficientCondition);
                    CHECK(depthScene->count() == count);
                    CHECK(cursor->childCount() == 0);
                    delete overflow;
                }
            }
        } else {
            delete depthRoot;
        }
    }
    delete depthScene;
}

// A scene numbers its objects from 1 in the order it stores them, a group before the objects
// below it, and finds each by its id at once; an object of another scene that carries the same
// id stays foreign.
static void objectIds()
{
    auto first = Scene::gen();
    auto second = Scene::gen();
    auto group = Group::gen();
    auto inner = Circle::gen({}, 0.5f);
    auto outer = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto later = Circle::gen({0.0f, 1.0f, 0.0f}, 0.25f);
    auto stranger = Circle::gen({}, 0.5f);
    CHECK(first && second && group && inner && outer && later && stranger);
    if (!first || !second || !group || !inner || !outer || !later || !stranger) {
        delete first;
        delete second;
        delete group;
        delete inner;
        delete outer;
        delete later;
        delete stranger;
        return;
    }
    CHECK(group->add(inner) == Result::Success);
    CHECK(first->add(group) == Result::Success);
    CHECK(first->add(outer) == Result::Success);
    CHECK(group->add(later) == Result::Success);
    CHECK(second->add(stranger) == Result::Success);
    CHECK(group->id() == 1u && inner->id() == 2u && outer->id() == 3u && later->id() == 4u);
    CHECK(first->count() == 4u);
    for (auto id = 1u; id <= first->count(); id++) {
        auto object = first->objectAt(id - 1u);
        CHECK(object && object->id() == id && first->object(id) == object);
    }
    CHECK(first->object(0u) == nullptr && first->object(5u) == nullptr);
    CHECK(stranger->id() == 1u && first->object(stranger->id()) == group);
    CHECK(second->object(1u) == stranger && second->object(2u) == nullptr);
    CHECK(first->play(Animation::fade(stranger, 0.5f), 1.0f, Easing::Linear) == Result::InvalidArguments);
    CHECK(first->diagnostic() && std::strcmp(first->diagnostic(), "the object is not part of this scene") == 0);
    CHECK(first->remove(stranger) != Result::Success);
    CHECK(first->play(Animation::fade(later, 0.5f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(second->play(Animation::fade(stranger, 0.25f), 1.0f, Easing::Linear) == Result::Success);
    delete first;
    delete second;
}

// A text's box moves off where align pins it by boxOffset, and bounds and the layout calls see
// it there, while the glyphs stay where align pins them.
static void offsetBoxes(SwRenderer* renderer)
{
    auto scene = Scene::gen();
    auto row = Group::gen();
    auto word = Text::gen("word", {1.0f, 2.0f, 0.0f});
    auto plain = Text::gen("word", {1.0f, 2.0f, 0.0f});
    auto mark = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && row && word && plain && mark);
    if (!scene || !row || !word || !plain || !mark) {
        delete scene;
        delete row;
        delete word;
        delete plain;
        delete mark;
        return;
    }
    word->box = {2.0f, 1.0f};
    plain->box = {2.0f, 1.0f};
    plain->opacity = 0.0f;
    Bounds bounds;
    CHECK(word->bounds(bounds) && near(bounds.min.x, 0.0f) && near(bounds.max.x, 2.0f) && near(bounds.min.y, 1.5f)
          && near(bounds.max.y, 2.5f));
    word->boxOffset = {0.25f, -0.5f};
    CHECK(word->bounds(bounds) && near(bounds.min.x, 0.25f) && near(bounds.max.x, 2.25f) && near(bounds.min.y, 1.0f)
          && near(bounds.max.y, 2.0f));
    CHECK(row->add(word) == Result::Success && row->add(plain) == Result::Success && row->add(mark) == Result::Success);
    CHECK(scene->add(row) == Result::Success);
    CHECK(mark->nextTo(word, {1.0f, 0.0f, 0.0f}, 0.5f) == Result::Success);
    Bounds placed;
    CHECK(mark->bounds(placed) && near(placed.min.x, 2.75f) && near(placed.center().y, 1.5f));
    // The glyphs draw where the text without an offset draws them.
#ifdef VIS_TEST_FONT
    CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
#ifdef VIS_TEST_MONO_FONT
    CHECK(renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
#endif
    BBox drawn;
    BBox kept;
    CHECK(renderer->bounds(scene, word, 0.0f, drawn) == Result::Success);
    plain->opacity = 1.0f;
    word->opacity = 0.0f;
    CHECK(scene->update(plain) == Result::Success && scene->update(word) == Result::Success);
    CHECK(renderer->bounds(scene, plain, 0.0f, kept) == Result::Success);
    CHECK(near(drawn.x, kept.x) && near(drawn.y, kept.y) && near(drawn.width, kept.width)
          && near(drawn.height, kept.height));
#else
    (void)renderer;
#endif
    delete scene;
    // An offset must be finite.
    auto broken = Text::gen("x");
    auto other = Scene::gen();
    if (broken) broken->boxOffset = {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    CHECK(broken && other && other->add(broken) == Result::InvalidArguments);
    delete broken;
    delete other;
}

static void boundsAndLayout()
{
    auto group = Group::gen();
    auto anchor = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {2.0f, 1.0f});
    auto moving = Rectangle::gen({3.0f, 2.0f, 0.0f}, {1.0f, 2.0f});
    CHECK(group && anchor && moving);
    if (group && anchor && moving) {
        auto anchorResult = group->add(anchor);
        auto movingResult = group->add(moving);
        CHECK(anchorResult == Result::Success);
        CHECK(movingResult == Result::Success);
        if (anchorResult == Result::Success && movingResult == Result::Success) {
            Bounds anchorBounds;
            Bounds movingBounds;
            CHECK(anchor->bounds(anchorBounds));
            CHECK(near(anchorBounds.min.x, -2.0f) && near(anchorBounds.max.x, 0.0f));
            CHECK(near(anchorBounds.min.y, -0.5f) && near(anchorBounds.max.y, 0.5f));
            CHECK(near(anchorBounds.center().x, -1.0f));
            CHECK(near(anchorBounds.size().x, 2.0f) && near(anchorBounds.size().y, 1.0f));

            CHECK(moving->moveTo({0.0f, 1.0f, 0.0f}) == Result::Success);
            CHECK(moving->bounds(movingBounds));
            CHECK(near(movingBounds.center().x, 0.0f) && near(movingBounds.center().y, 1.0f));

            CHECK(moving->nextTo(anchor, {1.0f, 0.0f, 0.0f}, 0.5f) == Result::Success);
            CHECK(moving->bounds(movingBounds));
            CHECK(near(movingBounds.min.x, anchorBounds.max.x + 0.5f));
            CHECK(near(movingBounds.center().y, anchorBounds.center().y));

            CHECK(moving->alignTo(anchor, {0.0f, 1.0f, 0.0f}) == Result::Success);
            CHECK(moving->bounds(movingBounds));
            CHECK(near(movingBounds.max.y, anchorBounds.max.y));
            CHECK(moving->nextTo(anchor, {}, 0.5f) == Result::InvalidArguments);
            CHECK(moving->nextTo(anchor, {1.0f, 0.0f, 0.0f}, -0.1f) == Result::InvalidArguments);
            CHECK(moving->alignTo(anchor, {}) == Result::InvalidArguments);

            auto outsider = Rectangle::gen();
            CHECK(outsider != nullptr);
            if (outsider) {
                CHECK(moving->nextTo(outsider, {1.0f, 0.0f, 0.0f}) == Result::InvalidArguments);
                CHECK(moving->alignTo(outsider, {1.0f, 0.0f, 0.0f}) == Result::InvalidArguments);
            }
            delete outsider;

            Bounds groupBounds;
            CHECK(group->bounds(groupBounds));
            CHECK(groupBounds.min.x <= anchorBounds.min.x);
            CHECK(groupBounds.max.x >= movingBounds.max.x);
        }
        if (anchorResult != Result::Success) delete anchor;
        if (movingResult != Result::Success) delete moving;
    } else {
        delete anchor;
        delete moving;
    }
    delete group;

    auto row = Group::gen();
    auto rowA = Rectangle::gen({}, {1.0f, 1.0f});
    auto rowB = Rectangle::gen({}, {2.0f, 1.0f});
    auto rowC = Rectangle::gen({}, {0.5f, 1.0f});
    CHECK(row && rowA && rowB && rowC);
    if (row && rowA && rowB && rowC) {
        auto addedA = row->add(rowA);
        auto addedB = row->add(rowB);
        auto addedC = row->add(rowC);
        CHECK(addedA == Result::Success && addedB == Result::Success && addedC == Result::Success);
        if (addedA == Result::Success && addedB == Result::Success && addedC == Result::Success) {
            CHECK(row->arrange({1.0f, 0.0f, 0.0f}, 0.4f) == Result::Success);
            Bounds a;
            Bounds b;
            Bounds c;
            CHECK(rowA->bounds(a) && rowB->bounds(b) && rowC->bounds(c));
            CHECK(near(b.min.x - a.max.x, 0.4f));
            CHECK(near(c.min.x - b.max.x, 0.4f));
            CHECK(near(a.center().y, b.center().y) && near(b.center().y, c.center().y));
            CHECK(row->arrange({}, 0.4f) == Result::InvalidArguments);
            CHECK(row->arrange({1.0f, 0.0f, 0.0f}, -1.0f) == Result::InvalidArguments);
        }
        if (addedA != Result::Success) delete rowA;
        if (addedB != Result::Success) delete rowB;
        if (addedC != Result::Success) delete rowC;
    } else {
        delete rowA;
        delete rowB;
        delete rowC;
    }
    delete row;

    auto grid = Group::gen();
    auto gridA = Rectangle::gen();
    auto gridB = Rectangle::gen();
    auto gridC = Rectangle::gen();
    auto gridD = Rectangle::gen();
    CHECK(grid && gridA && gridB && gridC && gridD);
    if (grid && gridA && gridB && gridC && gridD) {
        auto addedA = grid->add(gridA);
        auto addedB = grid->add(gridB);
        auto addedC = grid->add(gridC);
        auto addedD = grid->add(gridD);
        CHECK(addedA == Result::Success && addedB == Result::Success
              && addedC == Result::Success && addedD == Result::Success);
        if (addedA == Result::Success && addedB == Result::Success
            && addedC == Result::Success && addedD == Result::Success) {
            CHECK(grid->arrangeGrid(0) == Result::InvalidArguments);
            CHECK(grid->arrangeGrid(2, -0.1f, 0.5f) == Result::InvalidArguments);
            CHECK(grid->arrangeGrid(2, 0.25f, 0.5f) == Result::Success);
            Bounds a;
            Bounds b;
            Bounds c;
            Bounds d;
            CHECK(gridA->bounds(a) && gridB->bounds(b) && gridC->bounds(c) && gridD->bounds(d));
            CHECK(near(b.min.x - a.max.x, 0.25f));
            CHECK(near(d.min.x - c.max.x, 0.25f));
            CHECK(near(a.min.y - c.max.y, 0.5f));
            CHECK(near(b.min.y - d.max.y, 0.5f));
            CHECK(near(a.center().x, c.center().x));
            CHECK(near(b.center().x, d.center().x));
        }
        if (addedA != Result::Success) delete gridA;
        if (addedB != Result::Success) delete gridB;
        if (addedC != Result::Success) delete gridC;
        if (addedD != Result::Success) delete gridD;
    } else {
        delete gridA;
        delete gridB;
        delete gridC;
        delete gridD;
    }
    delete grid;

    auto empty = Group::gen();
    CHECK(empty != nullptr);
    if (empty) {
        Bounds bounds;
        CHECK(!empty->bounds(bounds));
    }
    delete empty;
}

static void sceneUpdate(SwRenderer* renderer)
{
    Config config;
    config.width = 100;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto rectangle = Rectangle::gen({}, {1.0f, 1.0f});
    auto foreign = Rectangle::gen();
    CHECK(scene && rectangle && foreign);
    if (scene && rectangle && foreign) {
        rectangle->style.stroke.a = 0;
        rectangle->fill(Color::hex("#7bd88f"));
        auto result = scene->add(rectangle);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            auto original = checksum(surface);
            rectangle->center.x = 1.0f;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::InvalidArguments);
            CHECK(scene->update(nullptr) == Result::InvalidArguments);
            CHECK(scene->update(foreign) == Result::InvalidArguments);
            CHECK(scene->update(rectangle) == Result::Success);
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            CHECK(checksum(surface) != original);

            rectangle->layer = 4;
            CHECK(scene->update(rectangle) == Result::Success);
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);

            CHECK(scene->wait(0.0f) == Result::Success);
            rectangle->center.x = 1.5f;
            CHECK(scene->update(rectangle) == Result::InsufficientCondition);
            CHECK(renderer->render(scene, scene->duration(), surface) == Result::InvalidArguments);
            rectangle->center.x = 1.0f;
            CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
        } else {
            delete rectangle;
        }
    } else {
        delete rectangle;
    }
    delete foreign;
    delete scene;

    auto historical = Scene::gen(config);
    auto historicalRectangle = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(historical && historicalRectangle);
    if (historical && historicalRectangle) {
        historicalRectangle->style.stroke.a = 0;
        historicalRectangle->fill(Color::hex("#7bd88f"));
        auto result = historical->add(historicalRectangle);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            Surface surface;
            CHECK(renderer->render(historical, 0.0f, surface) == Result::Success);
            auto visible = checksum(surface);
            CHECK(historical->wait(1.0f) == Result::Success);
            CHECK(historical->play(Animation::create(historicalRectangle), 1.0f, Easing::Linear)
                  == Result::Success);
            CHECK(renderer->render(historical, 0.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(historical, 1.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            CHECK(renderer->render(historical, 2.0f, surface) == Result::Success);
            CHECK(checksum(surface) == visible);
        } else {
            delete historicalRectangle;
        }
    } else {
        delete historicalRectangle;
    }
    delete historical;

    auto sealed = Scene::gen();
    auto removed = Rectangle::gen();
    auto retained = Rectangle::gen();
    CHECK(sealed && removed && retained);
    if (sealed && removed && retained) {
        CHECK(sealed->add(removed) == Result::Success);
        CHECK(sealed->add(retained) == Result::Success);
        CHECK(sealed->remove(removed) == Result::Success);
        retained->center.x = 1.0f;
        CHECK(sealed->update(retained) == Result::InsufficientCondition);
    } else {
        delete removed;
        delete retained;
    }
    delete sealed;
}

static void groupAndRectangleRendering(SwRenderer* renderer)
{
    Config config;
    config.width = 120;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto visible = Group::gen();
    auto hidden = Group::gen();
    auto visibleRectangle = Rectangle::gen({}, {1.0f, 1.0f});
    auto hiddenRectangle = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(scene && visible && hidden && visibleRectangle && hiddenRectangle);
    if (scene && visible && hidden && visibleRectangle && hiddenRectangle) {
        visible->model = Mat4::translate({1.5f, 0.0f, 0.0f});
        visible->opacity = 0.5f;
        hidden->model = Mat4::translate({-1.5f, 0.0f, 0.0f});
        hidden->progress = 0.0f;
        visibleRectangle->style.stroke.a = 0;
        hiddenRectangle->style.stroke.a = 0;
        visibleRectangle->fill(Color::hex("#f04040"));
        hiddenRectangle->fill(Color::hex("#40f040"));
        auto visibleChild = visible->add(visibleRectangle);
        auto hiddenChild = hidden->add(hiddenRectangle);
        CHECK(visibleChild == Result::Success && hiddenChild == Result::Success);
        if (visibleChild == Result::Success && hiddenChild == Result::Success) {
            auto visibleRoot = scene->add(visible);
            auto hiddenRoot = scene->add(hidden);
            CHECK(visibleRoot == Result::Success && hiddenRoot == Result::Success);
            if (visibleRoot == Result::Success && hiddenRoot == Result::Success) {
                Surface surface;
                CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
                if (surface.data()) {
                    auto background = pixel(config.background);
                    auto visiblePixel = surface.data()[40u * surface.stride() + 90u];
                    auto red = static_cast<uint8_t>(visiblePixel);
                    auto green = static_cast<uint8_t>(visiblePixel >> 8u);
                    CHECK(surface.data()[40u * surface.stride() + 30u] == background);
                    CHECK(surface.data()[40u * surface.stride() + 60u] == background);
                    CHECK(visiblePixel != background);
                    CHECK(red > 80u && red < 230u && red > green + 40u);
                }
            } else {
                if (visibleRoot != Result::Success) delete visible;
                if (hiddenRoot != Result::Success) delete hidden;
            }
        } else {
            if (visibleChild != Result::Success) delete visibleRectangle;
            if (hiddenChild != Result::Success) delete hiddenRectangle;
            delete visible;
            delete hidden;
        }
    } else {
        delete visible;
        delete hidden;
        delete visibleRectangle;
        delete hiddenRectangle;
    }
    delete scene;

    config.width = 100;
    config.height = 100;
    auto roundedScene = Scene::gen(config);
    auto rounded = Rectangle::gen({}, {2.0f, 2.0f}, 0.6f);
    CHECK(roundedScene && rounded);
    if (roundedScene && rounded) {
        rounded->style.stroke.a = 0;
        auto fill = Color::hex("#40d8a0");
        rounded->fill(fill);
        auto result = roundedScene->add(rounded);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            Surface surface;
            CHECK(renderer->render(roundedScene, 0.0f, surface) == Result::Success);
            if (surface.data()) {
                CHECK(surface.data()[50u * surface.stride() + 50u] == pixel(fill));
                CHECK(surface.data()[27u * surface.stride() + 73u] == pixel(config.background));
                CHECK(surface.data()[27u * surface.stride() + 50u] == pixel(fill));
            }
        } else {
            delete rounded;
        }
    } else {
        delete rounded;
    }
    delete roundedScene;
}

static void connectorOutlines(SwRenderer* renderer)
{
    Config config;
    config.width = 960;
    config.height = 540;
    config.camera.orthoHeight = 540.0f;
    auto ends = [&](Scene* scene, Connector* edge, float time, Vec3& a, Vec3& b) {
        LayoutReport report;
        CHECK(renderer->layout(scene, time, report) == Result::Success);
        for (auto i = 0u; i < report.pathCount(); i++) {
            auto path = report.pathAt(i);
            auto visual = report.visualAt(path->visual);
            if (report.objectAt(visual->object)->object != edge) continue;
            CHECK(path->count == 2u);
            if (path->count != 2u) return false;
            a = {path->points[0].x - 480.0f, 270.0f - path->points[0].y, 0.0f};
            b = {path->points[1].x - 480.0f, 270.0f - path->points[1].y, 0.0f};
            return true;
        }
        CHECK(false);
        return false;
    };
    auto check = [&](Object* source, Vec3 expected) {
        auto scene = Scene::gen(config);
        auto sink = Circle::gen({100, 100, 0}, 10);
        auto edge = Connector::gen(source, sink);
        CHECK(scene && source && sink && edge);
        CHECK(scene->add(source) == Result::Success);
        CHECK(scene->add(sink) == Result::Success);
        CHECK(scene->add(edge) == Result::Success);
        Vec3 a;
        Vec3 b;
        CHECK(ends(scene, edge, 0.0f, a, b));
        CHECK((a - expected).length() < 0.001f);
        delete scene;
    };
    auto diagonal = 26.0f / std::sqrt(2.0f);
    check(Circle::gen({}, 26), {diagonal, diagonal, 0});
    auto rounded = 16.0f + 10.0f / std::sqrt(2.0f);
    check(Rectangle::gen({}, {52, 52}, 10), {rounded, rounded, 0});
    auto group = Group::gen();
    CHECK(group->add(Circle::gen({}, 26)) == Result::Success);
    CHECK(group->add(Text::gen("node")) == Result::Success);
    check(group, {diagonal, diagonal, 0});
    auto ellipse = Circle::gen({}, 26);
    ellipse->model = Mat4::scale({2, 1, 1});
    auto elliptical = 26.0f / std::sqrt(1.25f);
    check(ellipse, {elliptical, elliptical, 0});
    Vec3 triangle[] = {{-26, -26, 0}, {26, -26, 0}, {0, 26, 0}};
    check(Polygon::gen(triangle, 3), {26.0f / 3.0f, 26.0f / 3.0f, 0});

    PathCommand commands[] = {
        PathCommand::move(triangle[0]), PathCommand::line(triangle[1]),
        PathCommand::line(triangle[2]), PathCommand::close(),
        PathCommand::move({-2, -2, 0}), PathCommand::line({2, -2, 0}),
        PathCommand::line({0, 2, 0}), PathCommand::close()};
    check(Path::gen(commands, 8), {26.0f / 3.0f, 26.0f / 3.0f, 0});
    auto nested = Group::gen();
    auto inner = Group::gen();
    CHECK(nested->add(Circle::gen({}, 10)) == Result::Success);
    CHECK(inner->add(Circle::gen({}, 26)) == Result::Success);
    CHECK(nested->add(inner) == Result::Success);
    check(nested, {diagonal, diagonal, 0});

    auto corners = Scene::gen(config);
    auto box = Rectangle::gen({}, {52, 52}, 10);
    auto cornerSink = Circle::gen({100, 100, 0}, 10);
    auto cornerEdge = Connector::gen(box, cornerSink);
    CHECK(corners->add(box) == Result::Success);
    CHECK(corners->add(cornerSink) == Result::Success);
    CHECK(corners->add(cornerEdge) == Result::Success);
    CHECK(corners->play(AnimationTarget::from(box).size(Vec2{72, 72}).corner(20), 1, Easing::Linear) == Result::Success);
    const float cornerTimes[] = {0, 0.25f, 0.5f, 1};
    for (auto time : cornerTimes) {
        Vec3 a;
        Vec3 b;
        CHECK(ends(corners, cornerEdge, time, a, b));
        auto expected = 16 + (10 + 10 * time) / std::sqrt(2.0f);
        CHECK(near(a.x, expected, 0.001f) && near(a.y, expected, 0.001f));
    }
    delete corners;

    auto scene = Scene::gen(config);
    auto source = Circle::gen({}, 26);
    auto sink = Circle::gen({100, 100, 0}, 10);
    auto edge = Connector::gen(source, sink);
    auto token = Rectangle::gen({}, {8, 2});
    token->stroke(Color::hex("#00000000")).fill(Color::hex("#ffffff"));
    CHECK(scene->add(source) == Result::Success);
    CHECK(scene->add(sink) == Result::Success);
    CHECK(scene->add(edge) == Result::Success);
    CHECK(scene->add(token) == Result::Success);
    edge->padding = 3;
    CHECK(scene->update(edge) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(source).radius(36).shift({20, 0, 0}), 1, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0) == Result::Success);
    auto ride = Animation::moveAlong(token, edge, true);
    ride.from = 0.8f;
    ride.to = 0.2f;
    CHECK(scene->play(ride, 1, Easing::Linear) == Result::Success);
    const float times[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.25f};
    for (auto time : times) {
        Vec3 a;
        Vec3 b;
        CHECK(ends(scene, edge, time, a, b));
        auto center = Vec3{20 * time, 0, 0};
        CHECK(near((a - center).length(), 29 + 10 * time, 0.001f));
        auto expected = a + (b - a) * (0.8f - 0.6f * time);
        BBox bounds;
        CHECK(renderer->bounds(scene, token, time, bounds) == Result::Success);
        CHECK(near(bounds.center().x - 480, expected.x, 0.001f));
        CHECK(near(270 - bounds.center().y, expected.y, 0.001f));
        auto angle = std::atan2(100.0f, 100.0f - 20.0f * time) - std::atan2(100.0f, 100.0f);
        CHECK(near(bounds.width, 8 * std::cos(angle) + 2 * std::sin(angle), 0.001f));
        CHECK(near(bounds.height, 2 * std::cos(angle) + 8 * std::sin(angle), 0.001f));
    }
    Vec3 finalStart;
    Vec3 finalEnd;
    CHECK(ends(scene, edge, 1, finalStart, finalEnd));
    auto finalPosition = finalStart + (finalEnd - finalStart) * 0.2f;
    CHECK((token->model * token->center - finalPosition).length() < 0.001f);
    BBox held;
    CHECK(scene->play(Animation::shift(sink, {50, 0, 0}), 1, Easing::Linear) == Result::Success);
    CHECK(renderer->bounds(scene, token, 1.5f, held) == Result::Success);
    CHECK(near(held.center().x - 480, finalPosition.x, 0.001f));
    CHECK(near(270 - held.center().y, finalPosition.y, 0.001f));
    CHECK(scene->play(Animation::shift(token, {5, 0, 0}), 1, Easing::Linear) == Result::Success);
    CHECK(renderer->bounds(scene, token, 3, held) == Result::Success);
    CHECK(near(held.center().x - 480, finalPosition.x + 5, 0.001f));
    CHECK(near(270 - held.center().y, finalPosition.y, 0.001f));
    auto chain = Connector::gen(token, sink);
    CHECK(scene->add(chain) == Result::Success);
    CHECK(scene->play(Animation::moveAlong(source, chain)) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "cannot depend on the target"));

    CHECK(scene->play(Animation::moveAlong(source, edge)) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "endpoint cannot move along"));
    delete scene;
    auto parentScene = Scene::gen(config);
    auto parent = Group::gen();
    auto child = Circle::gen({}, 2);
    auto left = Circle::gen({}, 26);
    auto right = Circle::gen({100, 100, 0}, 10);
    auto parentEdge = Connector::gen(left, right);
    CHECK(parent->add(child) == Result::Success);
    CHECK(parentScene->add(parent) == Result::Success);
    CHECK(parentScene->add(left) == Result::Success);
    CHECK(parentScene->add(right) == Result::Success);
    CHECK(parentScene->add(parentEdge) == Result::Success);
    CHECK(parentScene->play(Animation::shift(parent, {20, 0, 0}), 1, Easing::Linear) == Result::Success);
    CHECK(parentScene->seek(0) == Result::Success);
    CHECK(parentScene->play(Animation::moveAlong(child, parentEdge), 1, Easing::Linear) == Result::Success);
    const float parentTimes[] = {0, 0.25f, 0.5f, 1};
    for (auto time : parentTimes) {
        Vec3 a;
        Vec3 b;
        CHECK(ends(parentScene, parentEdge, time, a, b));
        auto expected = a + (b - a) * time;
        BBox bounds;
        CHECK(renderer->bounds(parentScene, child, time, bounds) == Result::Success);
        CHECK(near(bounds.center().x - 480, expected.x, 0.001f));
        CHECK(near(270 - bounds.center().y, expected.y, 0.001f));
    }
    delete parentScene;


}

static void connectorAnimation(SwRenderer* renderer)
{
    Config config;
    config.width = 100;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto source = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    auto destination = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    CHECK(scene && source && destination);
    if (scene && source && destination) {
        source->style.stroke.a = 0;
        source->style.fill.a = 0;
        destination->style.stroke.a = 0;
        destination->style.fill.a = 0;
        auto sourceResult = scene->add(source);
        auto destinationResult = scene->add(destination);
        CHECK(sourceResult == Result::Success && destinationResult == Result::Success);
        if (sourceResult == Result::Success && destinationResult == Result::Success) {
            auto transformedConnector = Connector::gen(source, destination);
            CHECK(transformedConnector != nullptr);
            if (transformedConnector) {
                transformedConnector->model = Mat4::translate({1.0f, 0.0f, 0.0f});
                CHECK(scene->add(transformedConnector) == Result::InvalidArguments);
                delete transformedConnector;
            }
            auto transformedGroup = Group::gen();
            auto transformedChild = Connector::gen(source, destination);
            CHECK(transformedGroup && transformedChild);
            if (transformedGroup && transformedChild) {
                transformedGroup->model = Mat4::translate({1.0f, 0.0f, 0.0f});
                auto childResult = transformedGroup->add(transformedChild);
                CHECK(childResult == Result::Success);
                if (childResult == Result::Success) {
                    CHECK(scene->add(transformedGroup) == Result::InvalidArguments);
                    delete transformedGroup;
                } else {
                    delete transformedChild;
                    delete transformedGroup;
                }
            } else {
                delete transformedChild;
                delete transformedGroup;
            }
            auto attachedEdgeGroup = Group::gen();
            CHECK(attachedEdgeGroup != nullptr);
            if (attachedEdgeGroup) {
                attachedEdgeGroup->model = Mat4::translate({1.0f, 0.0f, 0.0f});
                auto attachedResult = scene->add(attachedEdgeGroup);
                CHECK(attachedResult == Result::Success);
                if (attachedResult == Result::Success) {
                    auto rejected = Connector::gen(source, destination);
                    CHECK(rejected != nullptr);
                    if (rejected) {
                        CHECK(attachedEdgeGroup->add(rejected) == Result::InvalidArguments);
                        delete rejected;
                    }
                    auto originalModel = attachedEdgeGroup->model;
                    attachedEdgeGroup->model = Mat4::identity();
                    auto stale = Connector::gen(source, destination);
                    CHECK(stale != nullptr);
                    if (stale) {
                        CHECK(attachedEdgeGroup->add(stale) == Result::InvalidArguments);
                        delete stale;
                    }
                    attachedEdgeGroup->model = originalModel;
                } else {
                    delete attachedEdgeGroup;
                }
            }
            auto attachedNode = Group::gen();
            auto attachedEndpoint = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
            CHECK(attachedNode && attachedEndpoint);
            if (attachedNode && attachedEndpoint) {
                attachedNode->model = Mat4::translate({0.25f, 0.0f, 0.0f});
                attachedEndpoint->opacity = 0.0f;
                auto endpointResult = attachedNode->add(attachedEndpoint);
                CHECK(endpointResult == Result::Success);
                auto nodeResult = endpointResult == Result::Success ? scene->add(attachedNode) : Result::InvalidArguments;
                CHECK(nodeResult == Result::Success);
                if (nodeResult == Result::Success) {
                    auto outgoing = Connector::gen(attachedEndpoint, destination);
                    CHECK(outgoing != nullptr);
                    if (outgoing) {
                        outgoing->opacity = 0.0f;
                        auto outgoingResult = attachedNode->add(outgoing);
                        CHECK(outgoingResult == Result::Success);
                        if (outgoingResult != Result::Success) delete outgoing;
                    }
                } else {
                    if (endpointResult != Result::Success) delete attachedEndpoint;
                    delete attachedNode;
                }
            } else {
                delete attachedEndpoint;
                delete attachedNode;
            }
            auto edgeGroup = Group::gen();
            auto edgeChild = Connector::gen(source, destination);
            CHECK(edgeGroup && edgeChild);
            if (edgeGroup && edgeChild) {
                auto childResult = edgeGroup->add(edgeChild);
                CHECK(childResult == Result::Success);
                auto groupResult = childResult == Result::Success ? scene->add(edgeGroup) : Result::InvalidArguments;
                CHECK(groupResult == Result::Success);
                if (groupResult == Result::Success) {
                    auto originalModel = edgeGroup->model;
                    edgeGroup->model = Mat4::translate({1.0f, 0.0f, 0.0f});
                    CHECK(scene->update(edgeGroup) == Result::InvalidArguments);
                    edgeGroup->model = originalModel;
                    CHECK(scene->update(edgeGroup) == Result::Success);
                    CHECK(scene->play(Animation::shift(edgeGroup, {0.5f, 0.0f, 0.0f}))
                          == Result::InvalidArguments);
                    auto edgeTarget = AnimationTarget::from(edgeGroup);
                    edgeTarget.shift({0.5f, 0.0f, 0.0f});
                    CHECK(scene->play(edgeTarget) == Result::InvalidArguments);
                    CHECK(near(scene->duration(), 0.0f));
                } else {
                    if (childResult != Result::Success) delete edgeChild;
                    delete edgeGroup;
                }
            } else {
                delete edgeChild;
                delete edgeGroup;
            }
            auto connector = Connector::gen(source, destination);
            CHECK(connector != nullptr);
            if (connector) {
                connector->stroke(Color::hex("#ffd166"), 4.0f);
                connector->tail = 0.0f;
                connector->tip = 0.0f;
                CHECK(connector->from() == source && connector->to() == destination);
                auto connectorResult = scene->add(connector);
                CHECK(connectorResult == Result::Success);
                if (connectorResult == Result::Success) {
                    CHECK(scene->play(Animation::shift(connector, {0.5f, 0.0f, 0.0f}))
                          == Result::InvalidArguments);
                    auto connectorTarget = AnimationTarget::from(connector);
                    connectorTarget.shift({0.5f, 0.0f, 0.0f});
                    CHECK(scene->play(connectorTarget) == Result::InvalidArguments);
                    CHECK(near(scene->duration(), 0.0f));
                    auto target = AnimationTarget::from(destination);
                    target.shift({0.0f, 1.5f, 0.0f});
                    CHECK(scene->play(target, 1.0f, Easing::Linear) == Result::Success);
                    Surface surface;
                    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
                    auto start = extent(surface, config.background);
                    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
                    auto middle = extent(surface, config.background);
                    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
                    auto end = extent(surface, config.background);
                    CHECK(start.valid && middle.valid && end.valid);
                    if (start.valid && middle.valid && end.valid) {
                        auto startHeight = start.maxY - start.minY;
                        auto middleHeight = middle.maxY - middle.minY;
                        auto endHeight = end.maxY - end.minY;
                        CHECK(start.minY > middle.minY && middle.minY > end.minY);
                        CHECK(startHeight + 4u < middleHeight);
                        CHECK(middleHeight + 4u < endHeight);
                    }
                } else {
                    delete connector;
                }
            }
        } else {
            if (sourceResult != Result::Success) delete source;
            if (destinationResult != Result::Success) delete destination;
        }
    } else {
        delete source;
        delete destination;
    }
    delete scene;

    auto graphScene = Scene::gen(config);
    auto graph = Group::gen();
    auto graphSource = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    auto graphDestination = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    auto graphConnector = Connector::gen(graphSource, graphDestination);
    CHECK(graphScene && graph && graphSource && graphDestination && graphConnector);
    if (graphScene && graph && graphSource && graphDestination && graphConnector) {
        CHECK(graph->add(graphSource) == Result::Success);
        CHECK(graph->add(graphDestination) == Result::Success);
        CHECK(graph->add(graphConnector) == Result::Success);
        graph->model = Mat4::translate({0.25f, 0.0f, 0.0f});
        auto graphResult = graphScene->add(graph);
        CHECK(graphResult == Result::Success);
        if (graphResult == Result::Success) {
            CHECK(graphScene->play(Animation::shift(graph, {0.5f, 0.0f, 0.0f}), 1.0f,
                                   Easing::Linear) == Result::Success);
            Surface surface;
            CHECK(renderer->render(graphScene, 0.5f, surface) == Result::Success);
        } else {
            delete graph;
        }
    } else {
        delete graphConnector;
        delete graphDestination;
        delete graphSource;
        delete graph;
    }
    delete graphScene;

    auto outgoingScene = Scene::gen(config);
    auto outside = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    auto node = Group::gen();
    auto inside = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {0.5f, 0.5f});
    auto outgoing = Connector::gen(inside, outside);
    CHECK(outgoingScene && outside && node && inside && outgoing);
    if (outgoingScene && outside && node && inside && outgoing) {
        auto outsideResult = outgoingScene->add(outside);
        CHECK(outsideResult == Result::Success);
        CHECK(node->add(inside) == Result::Success);
        CHECK(node->add(outgoing) == Result::Success);
        node->model = Mat4::translate({0.25f, 0.0f, 0.0f});
        auto nodeResult = outsideResult == Result::Success ? outgoingScene->add(node) : Result::InvalidArguments;
        CHECK(nodeResult == Result::Success);
        if (nodeResult == Result::Success) {
            CHECK(outgoingScene->play(Animation::shift(node, {0.5f, 0.0f, 0.0f}), 1.0f,
                                      Easing::Linear) == Result::Success);
        } else {
            delete node;
        }
        if (outsideResult != Result::Success) delete outside;
    } else {
        delete outgoing;
        delete inside;
        delete node;
        delete outside;
    }
    delete outgoingScene;
}

static void animationTargets(SwRenderer* renderer)
{
    Config config;
    config.width = 120;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto first = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {0.8f, 0.8f});
    auto second = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.8f, 0.8f});
    auto foreign = Rectangle::gen();
    CHECK(scene && first && second && foreign);
    if (scene && first && second && foreign) {
        first->fill(Color::hex("#ef5350"));
        second->fill(Color::hex("#42a5f5"));
        const float dash[] = {6.0f, 4.0f};
        CHECK(second->dash(dash, 2u) == Result::Success);
        auto firstResult = scene->add(first);
        auto secondResult = scene->add(second);
        CHECK(firstResult == Result::Success && secondResult == Result::Success);
        if (firstResult == Result::Success && secondResult == Result::Success) {
            auto firstModel = first->model;
            auto firstFill = first->style.fill;
            auto firstOpacity = first->opacity;

            auto atomicValid = AnimationTarget::from(first);
            atomicValid.shift({3.0f, 0.0f, 0.0f});
            auto atomicInvalid = AnimationTarget::from(second);
            atomicInvalid.opacity(std::numeric_limits<float>::quiet_NaN());
            AnimationTarget atomic[] = {atomicValid, atomicInvalid};
            CHECK(scene->play(atomic, 2, 1.0f, Easing::Linear, 0.25f) == Result::InvalidArguments);
            CHECK(near(scene->duration(), 0.0f));
            CHECK(near(first->model.e[3], firstModel.e[3]));
            CHECK(near(first->opacity, firstOpacity));
            CHECK(first->style.fill.r == firstFill.r && first->style.fill.g == firstFill.g
                  && first->style.fill.b == firstFill.b && first->style.fill.a == firstFill.a);

            auto duplicateShift = AnimationTarget::from(first);
            duplicateShift.shift({1.0f, 0.0f, 0.0f});
            auto duplicateOpacity = AnimationTarget::from(first);
            duplicateOpacity.opacity(0.5f);
            AnimationTarget duplicate[] = {duplicateShift, duplicateOpacity};
            CHECK(scene->play(duplicate, 2, 1.0f, Easing::Linear, 0.0f) == Result::InvalidArguments);
            CHECK(near(scene->duration(), 0.0f));
            CHECK(near(first->model.e[3], firstModel.e[3]) && near(first->opacity, firstOpacity));

            auto noProperties = AnimationTarget::from(first);
            CHECK(scene->play(noProperties) == Result::InvalidArguments);
            auto foreignTarget = AnimationTarget::from(foreign);
            foreignTarget.fill(Color::hex("#ffffff"));
            CHECK(scene->play(foreignTarget) == Result::InvalidArguments);
            auto nullTarget = AnimationTarget::from(nullptr);
            nullTarget.shift({1.0f, 0.0f, 0.0f});
            CHECK(scene->play(nullTarget) == Result::InvalidArguments);
            auto projective = Mat4::identity();
            projective.e[12] = 1.0f;
            auto invalidTransform = AnimationTarget::from(second);
            invalidTransform.transform(projective);
            CHECK(scene->play(invalidTransform) == Result::InvalidArguments);
            auto invalidShift = AnimationTarget::from(second);
            invalidShift.shift({std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f});
            CHECK(scene->play(invalidShift) == Result::InvalidArguments);
            auto invalidDashTarget = AnimationTarget::from(first);
            invalidDashTarget.dashOffset(4.0f);
            CHECK(scene->play(invalidDashTarget) == Result::InvalidArguments);
            auto invalidDashValue = AnimationTarget::from(second);
            invalidDashValue.dashOffset(std::numeric_limits<float>::quiet_NaN());
            CHECK(scene->play(invalidDashValue) == Result::InvalidArguments);
            auto invalidMarkerTarget = AnimationTarget::from(first);
            invalidMarkerTarget.tip(8.0f);
            CHECK(scene->play(invalidMarkerTarget) == Result::InvalidArguments);
            CHECK(scene->play(static_cast<const AnimationTarget*>(nullptr), 1, 1.0f,
                              Easing::Linear, 0.0f) == Result::InvalidArguments);
            CHECK(scene->play(&noProperties, 0, 1.0f, Easing::Linear, 0.0f) == Result::InvalidArguments);
            CHECK(near(scene->duration(), 0.0f));

            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            auto start = checksum(surface);

            auto firstTarget = AnimationTarget::from(first);
            firstTarget.shift({2.0f, 0.0f, 0.0f})
                .opacity(0.4f)
                .stroke(Color::hex("#ffd166"))
                .fill(Color::hex("#40d890"));
            auto secondTarget = AnimationTarget::from(second);
            secondTarget.shift({-1.0f, 1.0f, 0.0f})
                .fill(Color::hex("#b060e8"))
                .dashOffset(-10.0f);
            AnimationTarget targets[] = {firstTarget, secondTarget};
            CHECK(scene->play(targets, 2, 2.0f, Easing::Linear, 0.25f) == Result::Success);
            CHECK(near(scene->duration(), 2.5f));
            CHECK(near(first->model.e[3], 2.0f) && near(first->model.e[7], 0.0f));
            CHECK(near(first->opacity, 0.4f));
            CHECK(first->style.stroke.r == Color::hex("#ffd166").r);
            CHECK(first->style.fill.g == Color::hex("#40d890").g);
            CHECK(near(second->model.e[3], -1.0f) && near(second->model.e[7], 1.0f));
            CHECK(second->style.fill.b == Color::hex("#b060e8").b);
            CHECK(near(second->style.dashOffset, -10.0f));

            CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
            auto middle = checksum(surface);
            CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
            auto staggered = checksum(surface);
            CHECK(renderer->render(scene, 2.5f, surface) == Result::Success);
            auto end = checksum(surface);
            CHECK(start != middle);
            CHECK(middle != end);
            CHECK(staggered != end);
        } else {
            if (firstResult != Result::Success) delete first;
            if (secondResult != Result::Success) delete second;
        }
    } else {
        delete first;
        delete second;
    }
    delete foreign;
    delete scene;
}

static void pathAndCurveShapes(SwRenderer* renderer)
{
    Config config;
    config.width = 240;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    PathCommand commands[] = {
        PathCommand::move({-2.4f, -0.6f, 0.0f}),
        PathCommand::line({-1.7f, 0.7f, 0.0f}),
        PathCommand::quadratic({-1.1f, 1.5f, 0.0f}, {-0.5f, 0.7f, 0.0f}),
        PathCommand::cubic({-0.1f, -0.2f, 0.0f}, {0.2f, -1.2f, 0.0f},
                           {0.7f, -0.6f, 0.0f}),
        PathCommand::close()};
    PathCommand invalid[] = {
        PathCommand::line({0.0f, 0.0f, 0.0f}),
        PathCommand::line({1.0f, 1.0f, 0.0f})};
    CHECK(Path::gen(invalid, 2u, 8u) == nullptr);
    CHECK(Path::gen(commands, 5u, 0u) == nullptr);
    CHECK(Curve::gen({}, {}, {}, {}, 0u) == nullptr);

    auto scene = Scene::gen(config);
    auto path = Path::gen(commands, 5u, 16u);
    auto curve = Curve::gen({0.9f, -0.7f, 0.0f}, {1.2f, 1.3f, 0.0f},
                            {2.0f, -1.3f, 0.0f}, {2.5f, 0.7f, 0.0f}, 24u);
    CHECK(scene && path && curve);
    if (scene && path && curve) {
        CHECK(path->type() == Type::Path && path->closed() && path->count() == 34u);
        CHECK(curve->type() == Type::Curve && curve->count() == 25u);
        Bounds bounds;
        CHECK(path->bounds(bounds));
        CHECK(bounds.min.x <= -2.4f && bounds.max.x >= 0.7f);
        path->stroke(Color::hex("#4cc9f0"), 3.0f).fill(Color::hex("#4cc9f044"));
        curve->stroke(Color::hex("#ffd166"), 4.0f);
        auto pathResult = scene->add(path);
        auto curveResult = scene->add(curve);
        CHECK(pathResult == Result::Success && curveResult == Result::Success);
        if (pathResult == Result::Success && curveResult == Result::Success) {
            Animation create[] = {Animation::create(path), Animation::create(curve)};
            CHECK(scene->play(create, 2u, 0.8f, Easing::Linear, 0.1f) == Result::Success);
            auto curveTarget = Curve::gen({0.9f, 0.7f, 0.0f}, {1.4f, -1.3f, 0.0f},
                                          {2.0f, 1.3f, 0.0f}, {2.5f, -0.7f, 0.0f}, 24u);
            PathCommand targetCommands[] = {
                PathCommand::move({-2.4f, 0.6f, 0.0f}),
                PathCommand::line({-1.7f, -0.7f, 0.0f}),
                PathCommand::quadratic({-1.1f, -1.5f, 0.0f}, {-0.5f, -0.7f, 0.0f}),
                PathCommand::cubic({-0.1f, 0.2f, 0.0f}, {0.2f, 1.2f, 0.0f},
                                   {0.7f, 0.6f, 0.0f}),
                PathCommand::close()};
            auto pathTarget = Path::gen(targetCommands, 5u, 16u);
            CHECK(curveTarget && pathTarget);
            if (curveTarget && pathTarget) {
                curveTarget->stroke(Color::hex("#7bd88f"), 4.0f);
                pathTarget->stroke(Color::hex("#f72585"), 3.0f).fill(Color::hex("#f7258544"));
                auto curveTargetResult = scene->add(curveTarget);
                CHECK(curveTargetResult == Result::Success);
                if (curveTargetResult == Result::Success) {
                    CHECK(scene->play(Animation::morph(curve, curveTarget), 0.5f,
                                      Easing::Linear) == Result::Success);
                } else {
                    delete curveTarget;
                }
                auto pathTargetResult = scene->add(pathTarget);
                CHECK(pathTargetResult == Result::Success);
                if (pathTargetResult == Result::Success) {
                    CHECK(scene->play(Animation::morph(path, pathTarget), 0.5f,
                                      Easing::Linear) == Result::Success);
                } else {
                    delete pathTarget;
                }
            } else {
                delete curveTarget;
                delete pathTarget;
            }
            Surface surface;
            CHECK(renderer->render(scene, scene->duration(), surface) == Result::Success);
            CHECK(changed(surface, config.background) > 300u);
        } else {
            if (pathResult != Result::Success) delete path;
            if (curveResult != Result::Success) delete curve;
        }
    } else {
        delete path;
        delete curve;
    }
    delete scene;
}

static void pathFillAnimations(SwRenderer* renderer)
{
    Config config;
    config.width = 120;
    config.height = 120;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#101010");
    PathCommand commands[] = {
        PathCommand::move({-1.0f, -1.0f, 0.0f}),
        PathCommand::line({1.0f, -1.0f, 0.0f}),
        PathCommand::line({1.0f, 1.0f, 0.0f}),
        PathCommand::line({-1.0f, 1.0f, 0.0f}),
        PathCommand::close()};
    auto scene = Scene::gen(config);
    auto path = Path::gen(commands, 5u, 8u);
    CHECK(scene && path);
    if (scene && path) {
        auto fill = Color::hex("#f72585");
        path->stroke(Color::hex("#f4f7fb"), 3.0f).fill(fill);
        auto result = scene->add(path);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(scene->play(Animation::drawBorderThenFill(path), 1.0f, Easing::Linear)
                  == Result::Success);
            Surface surface;
            auto center = static_cast<size_t>(60u) * 120u + 60u;
            CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
            CHECK(surface.data()[center] == pixel(config.background));
            CHECK(renderer->render(scene, 0.84f, surface) == Result::Success);
            auto partial = surface.data()[center];
            CHECK(partial != pixel(config.background));
            CHECK(partial != pixel(fill));
            CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
            CHECK(surface.data()[center] == pixel(fill));
        } else {
            delete path;
        }
    } else {
        delete path;
    }
    delete scene;

    scene = Scene::gen(config);
    path = Path::gen(commands, 5u, 8u);
    CHECK(scene && path);
    if (scene && path) {
        auto fill = Color::hex("#4cc9f0");
        path->stroke(Color::hex("#f4f7fb"), 3.0f).fill(fill);
        auto result = scene->add(path);
        CHECK(result == Result::Success);
        if (result == Result::Success) {
            CHECK(scene->play(Animation::create(path), 0.6f, Easing::Linear)
                  == Result::Success);
            CHECK(scene->play(Animation::fillReveal(path), 0.4f, Easing::Linear)
                  == Result::Success);
            Surface surface;
            auto center = static_cast<size_t>(60u) * 120u + 60u;
            CHECK(renderer->render(scene, 0.6f, surface) == Result::Success);
            CHECK(surface.data()[center] == pixel(config.background));
            CHECK(renderer->render(scene, 0.8f, surface) == Result::Success);
            auto partial = surface.data()[center];
            CHECK(partial != pixel(config.background));
            CHECK(partial != pixel(fill));
            CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
            CHECK(surface.data()[center] == pixel(fill));
        } else {
            delete path;
        }
    } else {
        delete path;
    }
    delete scene;
}

// The pixel of a frame at x and y, as a color.
static Color framePixel(const Surface& frame, uint32_t x, uint32_t y)
{
    auto value = frame.data()[static_cast<size_t>(y) * frame.stride() + x];
    return {static_cast<uint8_t>(value & 0xffu), static_cast<uint8_t>((value >> 8) & 0xffu),
            static_cast<uint8_t>((value >> 16) & 0xffu), static_cast<uint8_t>(value >> 24)};
}

// Whether the color channels of first and second differ by at most tolerance.
static bool close(Color first, Color second, int tolerance)
{
    return std::abs(first.r - second.r) <= tolerance && std::abs(first.g - second.g) <= tolerance
           && std::abs(first.b - second.b) <= tolerance;
}

// The bowl z = (x^2 + y^2) / 4 over [-2, 2]^2 on an n by n grid, row by row from the bottom left.
static void bowlPoints(Vec3* points, uint32_t n)
{
    for (auto j = 0u; j < n; j++) {
        for (auto i = 0u; i < n; i++) {
            auto x = -2.0f + 4.0f * static_cast<float>(i) / static_cast<float>(n - 1u);
            auto y = -2.0f + 4.0f * static_cast<float>(j) / static_cast<float>(n - 1u);
            points[j * n + i] = {x, y, (x * x + y * y) / 4.0f};
        }
    }
}

// A 200 by 200 frame looking straight down at the origin, 40 pixels to a world unit.
static Config topView()
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

// A colormap colors each face of a surface at the value of its middle, so the bottom of a bowl
// takes the first color of viridis and the face at a corner the color near its last; colors
// given per point average over each face, and both follow the opacity. A map that cannot run
// and a count that is not one color per point fail.
static void surfaceColors(SwRenderer* renderer)
{
    static constexpr auto N = 41u;
    static Vec3 points[N * N];
    bowlPoints(points, N);
    auto config = topView();
    // Renders the bowl as configure leaves it, alone in a scene, into frame.
    auto render = [&](auto configure, Surface& frame) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(points, N, N);
        CHECK(scene && surface);
        if (scene && surface) {
            surface->shading = false;
            configure(surface);
            CHECK(scene->add(surface) == Result::Success);
            CHECK(renderer->render(scene, 0.0f, frame) == Result::Success);
        } else {
            delete surface;
        }
        delete scene;
        return frame.data() != nullptr;
    };

    Surface frame;
    auto viridis = [](SurfaceMesh* surface) {
        surface->colormap = {Colormap::Viridis, {0.0f, 0.0f, 1.0f}, 0.0f, 2.0f};
    };
    if (render(viridis, frame)) {
        // The face beside the bottom, z from 0 to 0.005: share 0.00125 of the map.
        CHECK(close(framePixel(frame, 102u, 98u), {68, 1, 84, 255}, 1));
        // The faces at the four corners, z from 1.805 to 2: share 0.95125, 0.61 of the way from
        // the eighth stop (171, 220, 50) to the last (253, 231, 37).
        static constexpr uint32_t corners[4][2] = {{178, 22}, {22, 22}, {22, 178}, {178, 178}};
        for (auto& corner : corners) {
            CHECK(close(framePixel(frame, corner[0], corner[1]), {221, 227, 42, 255}, 1));
        }
        // Halfway up the side, x = 1.45 and y = 0.05: z about 0.53, a teal near the third stop.
        auto side = framePixel(frame, 158u, 98u);
        CHECK(side.g > side.r && side.b > side.r && side.g > 80u);
    }

    // The range runs the other way when min is above max.
    auto reversed = [](SurfaceMesh* surface) {
        surface->colormap = {Colormap::Viridis, {0.0f, 0.0f, 1.0f}, 2.0f, 0.0f};
    };
    if (render(reversed, frame)) {
        CHECK(close(framePixel(frame, 102u, 98u), {253, 231, 37, 255}, 1));
        CHECK(close(framePixel(frame, 178u, 22u), {69, 18, 99, 255}, 1));
    }

    // A color for every point: each face takes the mean of its corners, and opacity scales it.
    static Color colors[N * N];
    for (auto j = 0u; j < N; j++) {
        for (auto i = 0u; i < N; i++) {
            colors[j * N + i] = {static_cast<uint8_t>(i * 6u), static_cast<uint8_t>(j * 6u), 128, 255};
        }
    }
    auto pointColors = [&](SurfaceMesh* surface) {
        CHECK(surface->colors(colors, N * N - 1u) == Result::InvalidArguments);
        CHECK(surface->colors() == nullptr);
        CHECK(surface->colors(colors, N * N) == Result::Success);
        CHECK(surface->colors() != nullptr && surface->colors() != colors);
    };
    if (render(pointColors, frame)) {
        CHECK(close(framePixel(frame, 102u, 98u), {123, 123, 128, 255}, 0));
        CHECK(close(framePixel(frame, 22u, 178u), {3, 3, 128, 255}, 0));
    }
    auto faded = [&](SurfaceMesh* surface) {
        pointColors(surface);
        surface->opacity = 0.5f;
    };
    if (render(faded, frame)) CHECK(close(framePixel(frame, 102u, 98u), {70, 70, 72, 255}, 2));
    // A colormap wins over the colors of the points, and nullptr takes the colors away.
    auto both = [&](SurfaceMesh* surface) {
        pointColors(surface);
        viridis(surface);
    };
    if (render(both, frame)) CHECK(close(framePixel(frame, 102u, 98u), {68, 1, 84, 255}, 1));
    auto cleared = [&](SurfaceMesh* surface) {
        pointColors(surface);
        CHECK(surface->colors(nullptr, 0u) == Result::Success);
        CHECK(surface->colors() == nullptr);
        surface->fill(Color::hex("#ff8000"));
    };
    if (render(cleared, frame)) CHECK(close(framePixel(frame, 102u, 98u), {255, 128, 0, 255}, 0));

    // A map past the last, a zero axis, a range of no length and a range past float fail to add.
    auto invalid = [&](SurfaceColormap colormap) {
        auto other = Scene::gen(config);
        auto mesh = SurfaceMesh::gen(points, N, N);
        CHECK(other && mesh);
        if (other && mesh) {
            mesh->colormap = colormap;
            CHECK(other->add(mesh) == Result::InvalidArguments);
        }
        delete mesh;
        delete other;
    };
    invalid({static_cast<Colormap>(6), {0.0f, 0.0f, 1.0f}, 0.0f, 1.0f});
    invalid({Colormap::Magma, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f});
    invalid({Colormap::Magma, {0.0f, 0.0f, 1.0f}, 1.0f, 1.0f});
    auto largest = std::numeric_limits<float>::max();
    invalid({Colormap::Magma, {0.0f, 0.0f, 1.0f}, -largest, largest});
    invalid({Colormap::Magma, {0.0f, 0.0f, 1.0f}, 0.0f, std::numeric_limits<float>::quiet_NaN()});
}

// Mesh lines at a resolution of their own: a 41 by 41 plane over [-2, 2]^2 seen from above,
// 40 pixels to a unit, draws lines only where the counts put them, between the samples where
// they fall there; a Mesh surface draws nothing but them; a nearer surface hides the pieces
// below it while the rest stay; and counts of 1 or past the points fail.
static void surfaceMeshLines(SwRenderer* renderer)
{
    static constexpr auto N = 41u;
    static Vec3 plane[N * N];
    for (auto j = 0u; j < N; j++) {
        for (auto i = 0u; i < N; i++) {
            plane[j * N + i] = {-2.0f + 0.1f * static_cast<float>(i), -2.0f + 0.1f * static_cast<float>(j), 0.0f};
        }
    }
    auto config = topView();
    auto fill = Color::hex("#2050c0");
    auto stroke = Color::hex("#ffffff");
    auto background = config.background;
    // Renders the plane as configure leaves it, with a cover added if given, into frame.
    auto render = [&](auto configure, Object* cover, Surface& frame) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, N, N);
        CHECK(scene && surface);
        auto drawn = false;
        if (scene && surface) {
            surface->shading = false;
            surface->mode = SurfaceMode::SolidMesh;
            surface->fill(fill);
            surface->stroke(stroke, 2.0f);
            configure(surface);
            CHECK(scene->add(surface) == Result::Success);
            if (cover) CHECK(scene->add(cover) == Result::Success);
            drawn = renderer->render(scene, 0.0f, frame) == Result::Success && frame.data();
            CHECK(drawn);
            cover = nullptr;
        } else {
            delete surface;
        }
        delete cover;
        delete scene;
        return drawn;
    };

    Surface frame;
    // Five lines each way fall on the samples at -2, -1, 0, 1 and 2: x = 0 is pixel column 100,
    // while x = 0.5, where every face drew its outline before, keeps the fill.
    auto five = [](SurfaceMesh* surface) {
        surface->meshColumns = 5u;
        surface->meshRows = 5u;
    };
    if (render(five, nullptr, frame)) {
        CHECK(close(framePixel(frame, 100u, 130u), stroke, 8));
        CHECK(close(framePixel(frame, 130u, 100u), stroke, 8));
        CHECK(close(framePixel(frame, 120u, 130u), fill, 0));
        CHECK(close(framePixel(frame, 122u, 122u), fill, 0));
    }
    auto outlines = [](SurfaceMesh*) {};
    if (render(outlines, nullptr, frame)) CHECK(close(framePixel(frame, 120u, 130u), stroke, 8));
    // Four lines across 40 faces fall at 13.33 faces apart, between the samples: x = -0.667 is
    // pixel 73.3 and x = 0.667 pixel 126.7, and only one column, the columns, takes lines.
    auto four = [](SurfaceMesh* surface) { surface->meshColumns = 4u; };
    if (render(four, nullptr, frame)) {
        CHECK(framePixel(frame, 126u, 150u).r > 160u && framePixel(frame, 126u, 150u).g > 160u);
        CHECK(framePixel(frame, 73u, 150u).r > 160u);
        CHECK(close(framePixel(frame, 100u, 150u), fill, 0));
        CHECK(close(framePixel(frame, 150u, 100u), fill, 0));
    }
    // A Mesh surface draws only its lines.
    auto wire = [](SurfaceMesh* surface) {
        surface->mode = SurfaceMode::Mesh;
        surface->meshColumns = 5u;
        surface->meshRows = 5u;
    };
    if (render(wire, nullptr, frame)) {
        CHECK(close(framePixel(frame, 100u, 130u), stroke, 8));
        CHECK(close(framePixel(frame, 120u, 130u), background, 0));
    }
    // The elevated plane hides only the covered part of the line.
    Vec3 above[] = {{0.5f, -1.0f, 1.0f}, {1.5f, -1.0f, 1.0f}, {0.5f, 1.0f, 1.0f}, {1.5f, 1.0f, 1.0f}};
    auto cover = SurfaceMesh::gen(above, 2u, 2u);
    CHECK(cover != nullptr);
    if (cover) {
        cover->shading = false;
        cover->fill(Color::hex("#c03020"));
    }
    if (cover && render(five, cover, frame)) {
        CHECK(close(framePixel(frame, 140u, 100u), Color::hex("#c03020"), 0));
        CHECK(close(framePixel(frame, 140u, 160u), stroke, 8));
        CHECK(close(framePixel(frame, 140u, 40u), stroke, 8));
    }

    auto invalid = [&](uint32_t columns, uint32_t rows) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, N, N);
        CHECK(scene && surface);
        if (scene && surface) {
            surface->mode = SurfaceMode::Mesh;
            surface->meshColumns = columns;
            surface->meshRows = rows;
            CHECK(scene->add(surface) == Result::InvalidArguments);
        }
        delete surface;
        delete scene;
    };
    invalid(1u, 0u);
    invalid(0u, 1u);
    invalid(N + 1u, 2u);
    invalid(2u, N + 1u);
}

// A surface morphs into another on its grid point by point. Its colormap is read again at the
// points between the two, so halfway from the bowl to the saddle (x^2 - y^2) / 4, where the
// surface is x^2 / 4, the face over (0.1, 1.5) takes the middle of viridis rather than a blend
// of the bowl's color there with the saddle's. Colors sampled between the two blend between
// steps and mix into the target's by the progress. Surfaces on different grids, a surface and a
// circle, and sampled colors that do not fit fail to play.
static void surfaceMorph(SwRenderer* renderer)
{
    static constexpr auto N = 21u;
    static Vec3 bowl[N * N];
    static Vec3 saddle[N * N];
    bowlPoints(bowl, N);
    for (auto i = 0u; i < N * N; i++) {
        auto& p = bowl[i];
        saddle[i] = {p.x, p.y, (p.x * p.x - p.y * p.y) / 4.0f};
    }
    auto config = topView();
    auto colormap = [](SurfaceMesh* surface) {
        surface->shading = false;
        surface->colormap = {Colormap::Viridis, {0.0f, 0.0f, 1.0f}, -1.0f, 1.0f};
    };
    {
        auto scene = Scene::gen(config);
        auto source = SurfaceMesh::gen(bowl, N, N);
        auto target = SurfaceMesh::gen(saddle, N, N);
        CHECK(scene && source && target);
        if (scene && source && target) {
            colormap(source);
            colormap(target);
            CHECK(scene->add(source) == Result::Success);
            CHECK(scene->add(target) == Result::Success);
            CHECK(scene->play(Animation::morph(source, target), 1.0f, Easing::Linear) == Result::Success);
            Surface frame;
            Color colors[3];
            auto drawn = 0u;
            float times[3] = {0.0f, 0.5f, 1.0f};
            for (auto i = 0u; i < 3u; i++) {
                if (renderer->render(scene, times[i], frame) == Result::Success && frame.data()) {
                    colors[i] = framePixel(frame, 104u, 40u);
                    drawn++;
                }
            }
            CHECK(drawn == 3u);
            if (drawn == 3u) {
                // The bowl's face at a mean height of 0.57, the share 0.785.
                CHECK(close(colors[0], {115, 206, 85, 255}, 2));
                // A mean height of 0.005, against the blend (87, 139, 111) of the two ends.
                CHECK(close(colors[1], {33, 144, 140, 255}, 2));
                CHECK(!close(colors[1], {87, 139, 111, 255}, 20));
                // The saddle's face at a mean height of -0.56.
                CHECK(close(colors[2], {62, 73, 135, 255}, 2));
            }
        } else {
            delete source;
            delete target;
        }
        delete scene;
    }
    // Colors sampled at three steps, red, green and blue, mix into the target's white.
    Vec3 low[] = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    Vec3 high[] = {{-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
    Color steps[12];
    for (auto i = 0u; i < 4u; i++) {
        steps[i] = Color::hex("#ff0000");
        steps[4u + i] = Color::hex("#00ff00");
        steps[8u + i] = Color::hex("#0000ff");
    }
    Color white[4] = {Color::hex("#ffffff"), Color::hex("#ffffff"), Color::hex("#ffffff"), Color::hex("#ffffff")};
    // Plays a morph of the two squares with sampled colors set up by configure, returning its
    // result and, when it plays, the middle of the frame at times 0.25 and 0.5.
    auto sampled = [&](auto configure, Color* middle) {
        auto scene = Scene::gen(config);
        auto source = SurfaceMesh::gen(low, 2u, 2u);
        auto target = SurfaceMesh::gen(high, 2u, 2u);
        auto result = Result::Unknown;
        if (scene && source && target) {
            source->shading = false;
            target->shading = false;
            target->colors(white, 4u);
            scene->add(source);
            scene->add(target);
            auto animation = Animation::morph(source, target);
            animation.sourceColors = steps;
            animation.colorSteps = 2u;
            animation.colorCount = 12u;
            configure(animation);
            result = scene->play(animation, 1.0f, Easing::Linear);
            Surface frame;
            if (result == Result::Success && middle) {
                if (renderer->render(scene, 0.25f, frame) == Result::Success && frame.data()) {
                    middle[0] = framePixel(frame, 100u, 100u);
                }
                if (renderer->render(scene, 0.5f, frame) == Result::Success && frame.data()) {
                    middle[1] = framePixel(frame, 100u, 100u);
                }
            }
        } else {
            delete source;
            delete target;
        }
        delete scene;
        return result;
    };
    Color middle[2];
    CHECK(sampled([](Animation&) {}, middle) == Result::Success);
    // A quarter: halfway from red to green, a quarter of the way to white.
    CHECK(close(middle[0], {159, 159, 64, 255}, 1));
    // A half: green, halfway to white.
    CHECK(close(middle[1], {128, 255, 128, 255}, 1));
    CHECK(sampled([](Animation& a) { a.colorSteps = 0u; }, nullptr) == Result::InvalidArguments);
    CHECK(sampled([](Animation& a) { a.colorSteps = 65u; a.colorCount = 66u * 4u; }, nullptr)
          == Result::InvalidArguments);
    CHECK(sampled([](Animation& a) { a.colorCount = 8u; }, nullptr) == Result::InvalidArguments);
    CHECK(sampled([](Animation& a) { a.sourceColors = nullptr; }, nullptr) == Result::InvalidArguments);
    CHECK(sampled([&](Animation& a) { a.targetColors = steps; }, nullptr) == Result::Success);
    // A surface cannot morph into another grid or into a circle, and only surfaces take colors.
    auto refused = [&](Object* source, Object* target, bool colored) {
        auto scene = Scene::gen(config);
        auto result = Result::Unknown;
        if (scene && source && target) {
            scene->add(source);
            scene->add(target);
            auto animation = Animation::morph(source, target);
            if (colored) {
                animation.sourceColors = steps;
                animation.colorSteps = 2u;
                animation.colorCount = 12u;
            }
            result = scene->play(animation, 1.0f, Easing::Linear);
        } else {
            delete source;
            delete target;
        }
        delete scene;
        return result;
    };
    Vec3 wide[] = {{-1.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
                   {-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    CHECK(refused(SurfaceMesh::gen(low, 2u, 2u), SurfaceMesh::gen(wide, 3u, 2u), false) == Result::NonSupport);
    CHECK(refused(SurfaceMesh::gen(low, 2u, 2u), Circle::gen({}, 1.0f), false) == Result::NonSupport);
    CHECK(refused(Circle::gen({}, 1.0f), SurfaceMesh::gen(low, 2u, 2u), false) == Result::NonSupport);
    CHECK(refused(Circle::gen({}, 1.0f), Circle::gen({}, 2.0f), true) == Result::InvalidArguments);
}

// A light fixed in the world shades a tilted plane the same before and after the camera orbits
// a quarter turn around it, where the headlight shades it by the view; the side the camera
// sees takes the light, a light behind it leaves the ambient share, no shading leaves the fill,
// and a light that cannot shine fails to add.
static void surfaceLight(SwRenderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    // z = x / 2 over [-1, 1]^2: its normal is (-1, 0, 2) / sqrt(5).
    Vec3 plane[] = {{-1.0f, -1.0f, -0.5f}, {1.0f, -1.0f, 0.5f}, {-1.0f, 1.0f, -0.5f}, {1.0f, 1.0f, 0.5f}};
    Config config;
    config.width = 120;
    config.height = 120;
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {0.0f, -6.0f, 6.0f};
    config.camera.target = {};
    config.camera.up = {0.0f, 0.0f, 1.0f};
    config.background = Color::hex("#101010");
    // The middle of the plane at the start of a linear quarter orbit about +z and at its end,
    // where the eye stands at (6, 0, 6).
    auto sample = [&](auto configure, Color& start, Color& end) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, 2u, 2u);
        CHECK(scene && surface);
        auto drawn = false;
        if (scene && surface) {
            surface->fill(Color::hex("#c86432"));
            configure(surface);
            CHECK(scene->add(surface) == Result::Success);
            CHECK(scene->orbit(PI / 2.0f, {0.0f, 0.0f, 1.0f}, 1.0f, Easing::Linear) == Result::Success);
            Surface frame;
            if (renderer->render(scene, 0.0f, frame) == Result::Success && frame.data()) {
                start = framePixel(frame, 60u, 60u);
                if (renderer->render(scene, 1.0f, frame) == Result::Success && frame.data()) {
                    end = framePixel(frame, 60u, 60u);
                    drawn = true;
                }
            }
            CHECK(drawn);
        } else {
            delete surface;
        }
        delete scene;
        return drawn;
    };
    Color start;
    Color end;
    // Lit from above: 0.5 + 0.5 * 2 / sqrt(5) = 0.947 of (200, 100, 50) from either eye.
    auto above = [](SurfaceMesh* surface) { surface->light = {{0.0f, 0.0f, 3.0f}, 0.5f}; };
    if (sample(above, start, end)) {
        CHECK(close(start, {189, 95, 47, 255}, 1));
        CHECK(close(end, start, 0));
    }
    // The headlight follows the eye: 0.58 + 0.42 cos of 0.632 at the start and of 0.316 at the
    // end.
    auto headlight = [](SurfaceMesh*) {};
    if (sample(headlight, start, end)) {
        CHECK(close(start, {169, 85, 42, 255}, 1));
        CHECK(close(end, {143, 71, 36, 255}, 1));
    }
    // From below, the light does not reach the side the camera sees, which keeps the ambient
    // share; the default ambient is a half.
    auto below = [](SurfaceMesh* surface) { surface->light.direction = {0.0f, 0.0f, -1.0f}; };
    if (sample(below, start, end)) {
        CHECK(close(start, {100, 50, 25, 255}, 1));
        CHECK(close(end, start, 0));
    }
    auto flat = [](SurfaceMesh* surface) {
        surface->light.direction = {0.0f, 0.0f, 1.0f};
        surface->shading = false;
    };
    if (sample(flat, start, end)) CHECK(close(start, {200, 100, 50, 255}, 0) && close(end, start, 0));

    auto invalid = [&](SurfaceLight light) {
        auto scene = Scene::gen(config);
        auto surface = SurfaceMesh::gen(plane, 2u, 2u);
        CHECK(scene && surface);
        if (scene && surface) {
            surface->light = light;
            CHECK(scene->add(surface) == Result::InvalidArguments);
        }
        delete surface;
        delete scene;
    };
    invalid({{0.0f, 0.0f, 1.0f}, -0.1f});
    invalid({{0.0f, 0.0f, 1.0f}, 1.5f});
    invalid({{0.0f, 0.0f, 1.0f}, std::numeric_limits<float>::quiet_NaN()});
    invalid({{0.0f, std::numeric_limits<float>::infinity(), 1.0f}, 0.5f});
}

// The plane z = height over [-1, 1]^2 in one face of color, seen from above in pixels 60 to 140
// each way.
static SurfaceMesh* depthPlate(float height, Color color)
{
    Vec3 corners[] = {{-1.0f, -1.0f, height}, {1.0f, -1.0f, height}, {-1.0f, 1.0f, height}, {1.0f, 1.0f, height}};
    auto surface = SurfaceMesh::gen(corners, 2u, 2u);
    if (surface) {
        surface->shading = false;
        surface->fill(color);
    }
    return surface;
}

// Adds count objects to a scene of config in order, orbits its camera by angle about +z over a
// second, and renders time into frame; the scene owns what it takes and the rest is deleted.
static bool depthRender(SwRenderer* renderer, const Config& config, Object* const* objects, uint32_t count,
                        float angle, float time, Surface& frame)
{
    auto scene = Scene::gen(config);
    auto complete = scene != nullptr;
    for (auto i = 0u; i < count; i++) complete = complete && objects[i];
    auto drawn = false;
    if (complete) {
        for (auto i = 0u; i < count; i++) CHECK(scene->add(objects[i]) == Result::Success);
        if (angle != 0.0f) CHECK(scene->orbit(angle, {0.0f, 0.0f, 1.0f}, 1.0f, Easing::Linear) == Result::Success);
        drawn = renderer->render(scene, time, frame) == Result::Success && frame.data();
    } else {
        for (auto i = 0u; i < count; i++) delete objects[i];
    }
    CHECK(drawn);
    delete scene;
    return drawn;
}

// The raw pixel of a frame at x and y.
static uint32_t depthPixel(const Surface& frame, uint32_t x, uint32_t y)
{
    return frame.data()[static_cast<size_t>(y) * frame.stride() + x];
}

// Split strokes only at same-layer depth crossings, preserving dashes, caps and drawing progress.
static void depthSplitStrokes(SwRenderer* renderer)
{
    auto config = topView();
    auto plate = Color::hex("#2050c0");
    auto white = Color::hex("#ffffff");
    auto background = config.background;
    // The line z = x from x = -2 to 2 along y = 0, pixels 20 to 180 of row 100.
    auto line = [&](bool reversed, Color color, float width) {
        Vec3 from = {-2.0f, 0.0f, -2.0f};
        Vec3 to = {2.0f, 0.0f, 2.0f};
        auto made = reversed ? Line::gen(to, from) : Line::gen(from, to);
        if (made) made->stroke(color, width);
        return made;
    };
    Surface frame;
    Surface reference;
    // Below the plane from x = -1 to a little short of 0, where the line comes within the plane's
    // tolerance, and in front of it from there to 1.
    auto through = [&](const Surface& image) {
        CHECK(close(framePixel(image, 80u, 100u), plate, 8));
        CHECK(close(framePixel(image, 86u, 100u), plate, 8));
        CHECK(close(framePixel(image, 120u, 100u), white, 8));
        CHECK(close(framePixel(image, 40u, 100u), white, 8));
        CHECK(close(framePixel(image, 160u, 100u), white, 8));
        CHECK(close(framePixel(image, 80u, 70u), plate, 0));
    };
    {
        Object* objects[] = {depthPlate(0.0f, plate), line(false, white, 4.0f)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) through(frame);
    }
    {
        Object* objects[] = {line(false, white, 4.0f), depthPlate(0.0f, plate)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) through(frame);
    }
    {
        Object* objects[] = {depthPlate(0.0f, plate), line(true, white, 4.0f)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) through(frame);
    }
    // A layer of its own keeps the line whole: over the plane from above, under it from below.
    {
        auto over = line(false, white, 4.0f);
        if (over) over->layer = 1;
        Object* objects[] = {depthPlate(0.0f, plate), over};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 80u, 100u), white, 8));
        }
        auto under = line(false, white, 4.0f);
        if (under) under->layer = -1;
        Object* others[] = {depthPlate(0.0f, plate), under};
        if (depthRender(renderer, config, others, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 120u, 100u), plate, 0));
            CHECK(close(framePixel(frame, 40u, 100u), white, 8));
        }
    }
    // A dashed line draws the same dashes over the plane and beside it as with no plane, where
    // pieces that started the pattern again would shift them.
    {
        float pattern[] = {10.0f, 6.0f};
        auto dashed = line(false, white, 4.0f);
        auto alone = line(false, white, 4.0f);
        if (dashed) CHECK(dashed->dash(pattern, 2u, 3.0f) == Result::Success);
        if (alone) CHECK(alone->dash(pattern, 2u, 3.0f) == Result::Success);
        Object* objects[] = {depthPlate(0.0f, plate), dashed};
        Object* others[] = {alone};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            auto shown = 0u;
            for (auto x = 18u; x < 183u; x++) {
                for (auto y = 97u; y <= 103u; y++) {
                    if (x < 58u || x > 142u) {
                        same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                    } else if (x > 104u && x < 138u) {
                        auto drawn = framePixel(reference, x, y);
                        if (close(drawn, white, 0)) {
                            same = same && close(framePixel(frame, x, y), white, 0);
                            shown++;
                        } else if (close(drawn, background, 0)) {
                            same = same && close(framePixel(frame, x, y), plate, 0);
                        }
                    } else if (x > 62u && x < 90u) {
                        same = same && close(framePixel(frame, x, y), plate, 0);
                    }
                }
            }
            CHECK(same);
            CHECK(shown > 40u);
        }
    }
    // Seven eighths drawn, the line ends at x = 1.5 with or without the plane.
    {
        auto partial = line(false, white, 4.0f);
        auto alone = line(false, white, 4.0f);
        if (partial) partial->progress = 0.875f;
        if (alone) alone->progress = 0.875f;
        Object* objects[] = {depthPlate(0.0f, plate), partial};
        Object* others[] = {alone};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            for (auto x = 142u; x < 200u; x++) {
                for (auto y = 96u; y <= 104u; y++) {
                    same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                }
            }
            CHECK(same);
            CHECK(close(framePixel(frame, 159u, 100u), white, 8));
            CHECK(close(framePixel(frame, 165u, 100u), background, 0));
            CHECK(close(framePixel(frame, 80u, 100u), plate, 8));
        }
    }
    // A translucent line keeps its round ends, and over the plane it is even from where it comes
    // out from under it. Its pieces meet without overlapping, where the line would draw twice and
    // lighten: seen through a translucent plane, no pixel of it over the plane is lighter than
    // where it lies in front.
    {
        auto pale = Color::hex("#ffffff80");
        Object* objects[] = {depthPlate(0.0f, plate), line(false, pale, 6.0f)};
        Object* others[] = {line(false, pale, 6.0f)};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            for (auto y = 95u; y <= 105u; y++) {
                for (auto x = 14u; x < 50u; x++) {
                    same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                }
                for (auto x = 150u; x < 187u; x++) {
                    same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
                }
            }
            CHECK(same);
            auto out = 60u;
            while (out < 140u && close(framePixel(frame, out, 100u), plate, 0)) out++;
            CHECK(out > 80u && out < 100u);
            auto even = true;
            for (auto x = out + 2u; x < 138u; x++) {
                even = even && close(framePixel(frame, x, 100u), framePixel(frame, 125u, 100u), 2);
            }
            CHECK(even);
            CHECK(!close(framePixel(frame, 125u, 100u), plate, 8));
        }
        Object* veiled[] = {depthPlate(0.0f, Color::hex("#2050c080")), line(false, pale, 6.0f)};
        if (depthRender(renderer, config, veiled, 2u, 0.0f, 0.0f, frame)) {
            auto front = static_cast<int>(framePixel(frame, 125u, 100u).r);
            auto lightest = 0;
            for (auto x = 62u; x < 138u; x++) {
                auto red = static_cast<int>(framePixel(frame, x, 100u).r);
                if (red > lightest) lightest = red;
            }
            CHECK(lightest <= front + 2);
            CHECK(static_cast<int>(framePixel(frame, 75u, 100u).r) + 20 < front);
        }
    }
    // An arrow's head draws whole with its tip: over the plane where the tip is above it, and
    // under it where the tip is below.
    {
        auto up = Arrow::gen({-0.8f, 0.0f, -1.0f}, {0.8f, 0.0f, 1.0f});
        if (up) up->stroke(white, 4.0f);
        Object* objects[] = {depthPlate(0.0f, plate), up};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 128u, 100u), white, 8));
            CHECK(close(framePixel(frame, 76u, 100u), plate, 0));
        }
        auto down = Arrow::gen({0.8f, 0.0f, 1.0f}, {-0.8f, 0.0f, -1.0f});
        if (down) down->stroke(white, 4.0f);
        Object* others[] = {depthPlate(0.0f, plate), down};
        if (depthRender(renderer, config, others, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 72u, 100u), plate, 0));
            CHECK(close(framePixel(frame, 76u, 98u), plate, 0));
            CHECK(close(framePixel(frame, 124u, 100u), white, 8));
        }
    }
    // A point on the plane stays on top although added first; one below it hides and one above
    // shows.
    {
        auto red = Color::hex("#e02020");
        auto point = [&](const Vec3& at) {
            auto made = Point::gen(at);
            if (made) {
                made->fill(red);
                made->stroke(red, 1.0f);
            }
            return made;
        };
        Object* objects[] = {point({0.5f, 0.5f, 0.0f}), point({-0.5f, 0.5f, -0.5f}), point({-0.5f, -0.5f, 0.3f}),
                             depthPlate(0.0f, plate)};
        if (depthRender(renderer, config, objects, 4u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 120u, 80u), red, 4));
            CHECK(close(framePixel(frame, 80u, 80u), plate, 0));
            CHECK(close(framePixel(frame, 80u, 120u), red, 4));
        }
    }
    // A stroke away from the plane draws as with no plane while another is cut beside it.
    {
        auto far = [&]() {
            auto made = Line::gen({-2.0f, 1.8f, 0.3f}, {2.0f, 1.8f, -0.3f});
            if (made) {
                float pattern[] = {7.0f, 5.0f};
                made->stroke(Color::hex("#ffcc0080"), 3.0f);
                CHECK(made->dash(pattern, 2u) == Result::Success);
            }
            return made;
        };
        Object* objects[] = {depthPlate(0.0f, plate), line(false, white, 4.0f), far()};
        Object* others[] = {far()};
        if (depthRender(renderer, config, objects, 3u, 0.0f, 0.0f, frame)
            && depthRender(renderer, config, others, 1u, 0.0f, 0.0f, reference)) {
            auto same = true;
            for (auto y = 20u; y <= 36u; y++) {
                for (auto x = 0u; x < 200u; x++) same = same && depthPixel(frame, x, y) == depthPixel(reference, x, y);
            }
            CHECK(same);
        }
    }
    // A 2D scene keeps the painter's order: the line added after the plane draws over all of it.
    {
        auto flat = config;
        flat.cameraView = CameraView::TwoD;
        Object* objects[] = {depthPlate(0.0f, plate), line(false, white, 4.0f)};
        if (depthRender(renderer, flat, objects, 2u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 80u, 100u), white, 8));
            CHECK(close(framePixel(frame, 80u, 70u), plate, 0));
        }
    }
}

// A space in the layer of a surface lowers only the parts of its axes the surface hides, so
// they draw behind the surface there and over the space's own layer elsewhere; while the camera
// orbits a dome, the axis hides inside and behind the dome in every frame and shows in front of
// it; and a stroke that crosses more folds than it can be cut into still draws.
static void depthSplitSpace(SwRenderer* renderer)
{
    auto plate = Color::hex("#2050c0");
    // Colors apart from the theme's axis colors, which a theme would replace.
    auto red = Color::hex("#f02828");
    auto green = Color::hex("#28d028");
    auto clear = Color{0, 0, 0, 0};
    auto space = [&](float z) {
        auto made = Space::gen({-2.0f, 2.0f, 1.0f}, {-2.0f, 2.0f, 1.0f}, {-z, z, 1.0f});
        if (made) {
            made->layer = 0;
            // Axes 4 pixels wide cover the pixels the checks read whole.
            made->stroke(clear, 4.0f);
            made->axisX = red;
            made->axisY = green;
        }
        return made;
    };
    Surface frame;
    // Seen from above, the plane z = 0.5 hides the axes below it; past it they show, and over
    // a line of the space's layer nearer the camera, which they cover there as their own layer.
    {
        auto config = topView();
        auto cross = Line::gen({1.5f, -0.5f, 1.0f}, {1.5f, 0.5f, 1.0f});
        if (cross) cross->stroke(Color::hex("#ffffff"), 4.0f);
        Object* objects[] = {space(1.0f), depthPlate(0.5f, plate), cross};
        if (depthRender(renderer, config, objects, 3u, 0.0f, 0.0f, frame)) {
            CHECK(close(framePixel(frame, 80u, 100u), plate, 0));
            CHECK(close(framePixel(frame, 100u, 80u), plate, 0));
            CHECK(close(framePixel(frame, 40u, 100u), red, 8));
            CHECK(close(framePixel(frame, 100u, 40u), green, 8));
            CHECK(close(framePixel(frame, 160u, 100u), red, 8));
            CHECK(close(framePixel(frame, 160u, 85u), Color::hex("#ffffff"), 8));
        }
    }
    // The dome z = 1 - (x^2 + y^2) / 2 over [-1, 1]^2 from (0, -6, 4), orbiting to (6, 0, 4).
    {
        static constexpr auto N = 21u;
        static Vec3 dome[N * N];
        for (auto j = 0u; j < N; j++) {
            for (auto i = 0u; i < N; i++) {
                auto x = -1.0f + 0.1f * static_cast<float>(i);
                auto y = -1.0f + 0.1f * static_cast<float>(j);
                dome[j * N + i] = {x, y, 1.0f - (x * x + y * y) / 2.0f};
            }
        }
        Config config;
        config.width = 200;
        config.height = 200;
        config.cameraView = CameraView::ThreeD;
        config.camera.projection = Projection::Perspective;
        config.camera.eye = {0.0f, -6.0f, 4.0f};
        config.camera.target = {};
        config.camera.up = {0.0f, 0.0f, 1.0f};
        config.background = Color::hex("#101010");
        constexpr auto PI = 3.14159265358979f;
        // The pixel of world at time, where the eye has turned a quarter of time about +z.
        auto pixel = [&](float time, const Vec3& world, uint32_t& x, uint32_t& y) {
            auto camera = config.camera;
            auto angle = PI / 2.0f * time;
            camera.eye = {6.0f * std::sin(angle), -6.0f * std::cos(angle), 4.0f};
            Vec3 ndc;
            if (!camera.project(world, 1.0f, ndc)) return false;
            x = static_cast<uint32_t>((ndc.x + 1.0f) * 100.0f);
            y = static_cast<uint32_t>((1.0f - ndc.y) * 100.0f);
            return x < 200u && y < 200u;
        };
        float times[] = {0.0f, 0.5f, 1.0f};
        for (auto time : times) {
            auto surface = SurfaceMesh::gen(dome, N, N);
            if (surface) {
                surface->shading = false;
                surface->fill(plate);
            }
            Object* objects[] = {space(0.5f), surface};
            if (!depthRender(renderer, config, objects, 2u, PI / 2.0f, time, frame)) continue;
            uint32_t x;
            uint32_t y;
            // The middle of the axis lies inside the dome. The dome's color is read within 6, as
            // a pixel on a seam between its faces is a little darker.
            CHECK(pixel(time, {0.0f, 0.0f, 0.0f}, x, y) && close(framePixel(frame, x, y), plate, 6));
            // The end at x = 1.7 lies beside the dome, then in front of it.
            CHECK(pixel(time, {1.7f, 0.0f, 0.0f}, x, y) && close(framePixel(frame, x, y), red, 12));
            // The end at x = -1.7 lies beside the dome, then behind it.
            CHECK(pixel(time, {-1.7f, 0.0f, 0.0f}, x, y));
            if (time < 0.25f) CHECK(close(framePixel(frame, x, y), red, 12));
            if (time > 0.75f) CHECK(close(framePixel(frame, x, y), plate, 6));
        }
    }
    // Folds 6 pixels apart under a 1200 pixel line cut it into more pieces than a stroke takes;
    // the last piece takes the rest, and the frame still draws.
    {
        static constexpr auto COLUMNS = 2401u;
        static Vec3 folds[COLUMNS * 2u];
        for (auto j = 0u; j < 2u; j++) {
            for (auto i = 0u; i < COLUMNS; i++) {
                auto x = -6.0f + 0.005f * static_cast<float>(i);
                folds[j * COLUMNS + i] = {x, -0.2f + 0.4f * static_cast<float>(j), 0.2f * std::sin(x * 104.72f)};
            }
        }
        Config config;
        config.width = 1200;
        config.height = 100;
        config.cameraView = CameraView::ThreeD;
        config.camera.eye = {0.0f, 0.0f, 10.0f};
        config.camera.target = {};
        config.camera.up = {0.0f, 1.0f, 0.0f};
        config.camera.projection = Projection::Orthographic;
        config.camera.orthoHeight = 1.0f;
        config.background = Color::hex("#101010");
        auto surface = SurfaceMesh::gen(folds, COLUMNS, 2u);
        if (surface) {
            surface->shading = false;
            surface->fill(plate);
        }
        auto across = Line::gen({-6.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f});
        if (across) across->stroke(Color::hex("#ffffff"), 2.0f);
        Object* objects[] = {surface, across};
        if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
            auto lit = 0u;
            for (auto x = 0u; x < 1200u; x++) lit += close(framePixel(frame, x, 50u), Color::hex("#ffffff"), 40);
            CHECK(lit > 100u && lit < 1100u);
        }
    }
}

// Each piece of a plot hides behind a surface where it lies behind it, as a stroke of one
// contour does, where a stroke of several contours would draw whole. Seen from above, the piece
// z = x along y = 0.5 hides under the plane z = 0 left of x = 0 and the piece z = -x along
// y = -0.5 right of it, also while the plot draws; drawn to three quarters, the second piece
// ends at x = 0.
static void depthSplitPlotPieces(SwRenderer* renderer)
{
    auto config = topView();
    auto plate = Color::hex("#2050c0");
    auto white = Color::hex("#ffffff");
    auto plot = [&](float progress) {
        Vec3 points[] = {{-2.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 0.0f}, {2.0f, 0.5f, 2.0f},
                         {-2.0f, -0.5f, 2.0f}, {0.0f, -0.5f, 0.0f}, {2.0f, -0.5f, -2.0f}};
        uint32_t ends[] = {3u, 6u};
        auto made = Plot::gen(points, 6u, ends, 2u);
        if (made) {
            made->stroke(white, 4.0f);
            made->progress = progress;
        }
        return made;
    };
    Surface frame;
    Object* objects[] = {depthPlate(0.0f, plate), plot(1.0f)};
    if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
        CHECK(close(framePixel(frame, 80u, 80u), plate, 8));
        CHECK(close(framePixel(frame, 120u, 80u), white, 8));
        CHECK(close(framePixel(frame, 80u, 120u), white, 8));
        CHECK(close(framePixel(frame, 120u, 120u), plate, 8));
        CHECK(close(framePixel(frame, 40u, 80u), white, 8));
        CHECK(close(framePixel(frame, 160u, 120u), white, 8));
    }
    Object* drawing[] = {depthPlate(0.0f, plate), plot(0.75f)};
    if (depthRender(renderer, config, drawing, 2u, 0.0f, 0.0f, frame)) {
        CHECK(close(framePixel(frame, 80u, 80u), plate, 8));
        CHECK(close(framePixel(frame, 120u, 80u), white, 8));
        CHECK(close(framePixel(frame, 80u, 120u), white, 8));
        CHECK(close(framePixel(frame, 160u, 120u), config.background, 0));
    }
}

// The ticks of a space draw over its grid with its axes and hide where a surface in the
// grid's layer covers them, as the axes do. Seen from above, the plane z = 0.5 over
// [-1, 1]^2 hides the ticks 20 pixels long at x = 0.5 and y = 0.5 below it, while those at
// 1.5 beside it show.
static void depthSplitSpaceTicks(SwRenderer* renderer)
{
    auto config = topView();
    auto plate = Color::hex("#2050c0");
    auto red = Color::hex("#f02828");
    auto green = Color::hex("#28d028");
    auto space = Space::gen({-2.0f, 2.0f, 1.0f}, {-2.0f, 2.0f, 1.0f}, {-1.0f, 1.0f, 1.0f});
    if (space) {
        space->layer = 0;
        space->stroke(Color{0, 0, 0, 0}, 4.0f);
        space->axisX = red;
        space->axisY = green;
        space->ticks = {0.5f, 0.5f, 0.0f};
        space->tickLength = 20.0f;
    }
    Surface frame;
    Object* objects[] = {space, depthPlate(0.5f, plate)};
    if (depthRender(renderer, config, objects, 2u, 0.0f, 0.0f, frame)) {
        // Rows 90 to 110 across x = 0.5 and 1.5, beside the axis, which covers rows 98 to 102.
        CHECK(close(framePixel(frame, 120u, 92u), plate, 0));
        CHECK(close(framePixel(frame, 160u, 92u), red, 8));
        // Columns 90 to 110 across y = 0.5 and 1.5.
        CHECK(close(framePixel(frame, 92u, 80u), plate, 0));
        CHECK(close(framePixel(frame, 92u, 40u), green, 8));
    }
}

static void surfacePlaneTextAndWrite(SwRenderer* renderer)
{
    Vec3 points[] = {
        {-1.5f, 0.0f, -1.5f}, {0.0f, 0.4f, -1.5f}, {1.5f, 0.0f, -1.5f},
        {-1.5f, 0.2f, 0.0f}, {0.0f, 0.9f, 0.0f}, {1.5f, 0.2f, 0.0f},
        {-1.5f, 0.0f, 1.5f}, {0.0f, 0.4f, 1.5f}, {1.5f, 0.0f, 1.5f}};
    CHECK(SurfaceMesh::gen(nullptr, 3u, 3u) == nullptr);
    CHECK(SurfaceMesh::gen(points, 1u, 3u) == nullptr);
    auto surface = SurfaceMesh::gen(points, 3u, 3u);
    CHECK(surface != nullptr);
    if (surface) {
        CHECK(surface->type() == Type::SurfaceMesh);
        CHECK(surface->columns() == 3u && surface->rows() == 3u);
        CHECK(std::strcmp(type(surface->type()), "surface") == 0);
        surface->mode = SurfaceMode::Solid;
        Bounds bounds;
        CHECK(surface->bounds(bounds));
        CHECK(bounds.min.x == -1.5f && bounds.max.y == 0.9f && bounds.max.z == 1.5f);
    }

    Config config;
    config.width = 180;
    config.height = 140;
    config.cameraView = CameraView::ThreeD;
    config.camera.eye = {4.0f, 3.4f, 5.0f};
    config.camera.target = {};
    config.camera.projection = Projection::Perspective;
    config.camera.near = 0.1f;
    config.camera.far = 100.0f;
    config.background = Color::hex("#f7f8fc");
    auto scene = Scene::gen(config);
    auto text = Text::gen("plane", {0.0f, -0.1f, 1.8f});
    CHECK(scene && surface && text);
    if (scene && surface && text) {
        text->orientation = TextOrientation::Plane;
        text->size = 0.45f;
        text->fill(Color::hex("#172033"));
        CHECK(scene->add(surface) == Result::Success);
        CHECK(scene->add(text) == Result::Success);
        CHECK(scene->play(Animation::create(surface), 0.6f, Easing::Linear)
              == Result::Success);
        CHECK(scene->play(Animation::fadeIn(text), 0.2f, Easing::Linear)
              == Result::Success);
        Surface frame;
        CHECK(renderer->render(scene, scene->duration(), frame) == Result::Success);
        CHECK(changed(frame, config.background) > 400u);
        BBox textBounds;
        CHECK(renderer->bounds(scene, text, scene->duration(), textBounds) == Result::Success);
        CHECK(textBounds.width > 0.0f && textBounds.height > 0.0f);
    } else {
        delete surface;
        delete text;
    }
    delete scene;

    config.cameraView = CameraView::TwoD;
    config.camera = {};
    config.camera.orthoHeight = 4.0f;
    config.width = 120;
    config.height = 120;
    config.background = Color::hex("#101010");
    PathCommand commands[] = {
        PathCommand::move({-1.0f, -1.0f, 0.0f}),
        PathCommand::line({1.0f, -1.0f, 0.0f}),
        PathCommand::line({1.0f, 1.0f, 0.0f}),
        PathCommand::line({-1.0f, 1.0f, 0.0f}),
        PathCommand::close()};
    scene = Scene::gen(config);
    auto path = Path::gen(commands, 5u, 8u);
    CHECK(scene && path);
    if (scene && path) {
        auto fill = Color::hex("#ef6461");
        path->stroke(Color::hex("#ef6461"), 3.0f).fill(fill);
        CHECK(scene->add(path) == Result::Success);
        CHECK(scene->play(Animation::write(path), 1.0f, Easing::Linear)
              == Result::Success);
        Surface frame;
        auto center = static_cast<size_t>(60u) * 120u + 60u;
        CHECK(renderer->render(scene, 0.20f, frame) == Result::Success);
        CHECK(frame.data()[center] == pixel(config.background));
        CHECK(renderer->render(scene, 0.65f, frame) == Result::Success);
        CHECK(frame.data()[center] != pixel(config.background));
        CHECK(renderer->render(scene, 1.0f, frame) == Result::Success);
        CHECK(frame.data()[center] == pixel(fill));
    } else {
        delete path;
    }
    delete scene;
}

static void countingText(SwRenderer* renderer)
{
    // A counting text writes its value with the format between its prefix and suffix.
    auto text = Text::gen("plain");
    CHECK(text && !text->counting() && text->value() == 0.0f);
    if (text) {
        NumberFormat grouped;
        grouped.separator = ',';
        CHECK(text->count(1234567.0f, grouped) == Result::Success);
        CHECK(text->counting() && text->value() == 1234567.0f && std::strcmp(text->text(), "1,234,567") == 0);
        NumberFormat signedTwo;
        signedTwo.decimals = 2;
        signedTwo.sign = true;
        CHECK(text->count(3.14159f, signedTwo, "x = ", " m") == Result::Success);
        CHECK(std::strcmp(text->text(), "x = +3.14 m") == 0);
        // A value that rounds to zero has no sign.
        CHECK(text->count(-0.004f, signedTwo) == Result::Success && std::strcmp(text->text(), "0.00") == 0);
        NumberFormat spaced;
        spaced.decimals = 1;
        spaced.separator = ' ';
        CHECK(text->count(-1234.56f, spaced) == Result::Success && std::strcmp(text->text(), "-1 234.6") == 0);
        CHECK(text->count(999.0f, grouped) == Result::Success && std::strcmp(text->text(), "999") == 0);
        CHECK(text->format().separator == ',' && text->format().decimals == 0u);
        char small[4];
        char room[32];
        CHECK(text->write(12345.0f, small, sizeof(small)) == 0u);
        CHECK(text->write(12345.0f, room, sizeof(room)) == 6u && std::strcmp(room, "12,345") == 0);
        CHECK(text->write(std::numeric_limits<float>::quiet_NaN(), room, sizeof(room)) == 0u);
        CHECK(text->write(-std::numeric_limits<float>::max(), room, sizeof(room)) == 0u);
        char huge[96];
        CHECK(text->write(-std::numeric_limits<float>::max(), huge, sizeof(huge)) > 50u && huge[0] == '-');
        // A refused format keeps the previous count.
        NumberFormat tooFine;
        tooFine.decimals = 7;
        NumberFormat control;
        control.separator = '\n';
        CHECK(text->count(1.0f, tooFine) == Result::InvalidArguments);
        CHECK(text->count(1.0f, control) == Result::InvalidArguments);
        CHECK(text->count(std::numeric_limits<float>::infinity()) == Result::InvalidArguments);
        CHECK(text->counting() && std::strcmp(text->text(), "999") == 0);
        // A plain string ends counting.
        CHECK(text->text("plain") == Result::Success && !text->counting() && text->value() == 0.0f);
        CHECK(text->write(1.0f, room, sizeof(room)) == 0u);
    }
    delete text;

#ifdef VIS_TEST_FONT
    // Tabular text uses the widest digit advance, including spaces used to pad a counter.
    {
        Config cfg;
        cfg.width = 320;
        cfg.height = 180;
        cfg.camera.orthoHeight = 180.0f;
        auto owner = Scene::gen(cfg);
        auto label = Text::gen("1");
        label->size = 24.0f;
        label->align.x = 0.0f;
        TextExtent proportional[10];
        auto widest = 0.0f;
        for (auto digit = 0u; digit < 10u; digit++) {
            char value[] = {static_cast<char>('0' + digit), 0};
            CHECK(owner->measure(label, value, proportional[digit]) == Result::Success);
            widest = std::fmax(widest, proportional[digit].width);
        }
        CHECK(widest > proportional[1].width + 0.1f);
        label->tabular = true;
        for (auto digit = 0u; digit < 10u; digit++) {
            char value[] = {static_cast<char>('0' + digit), 0};
            TextExtent cell;
            CHECK(owner->measure(label, value, cell) == Result::Success);
            CHECK(near(cell.width, widest));
            CHECK(near(cell.left, proportional[digit].left + (widest - proportional[digit].width) * 0.5f));
            CHECK(near(cell.right, proportional[digit].right + (widest - proportional[digit].width) * 0.5f));
        }
        TextExtent sample, letter;
        CHECK(owner->measure(label, "a", letter) == Result::Success);
        CHECK(owner->measure(label, "a1", sample) == Result::Success && near(sample.width, letter.width + widest));
        CHECK(owner->measure(label, "- ", sample) == Result::Success && near(sample.width, 2.0f * widest));
        for (auto frame = 0u; frame < 30u; frame++) {
            char value[8];
            std::snprintf(value, sizeof(value), "%4u", (1000u * frame + 14u) / 29u);
            CHECK(owner->measure(label, value, sample) == Result::Success);
            CHECK(std::fabs(sample.width - 4.0f * widest) < 0.01f);
        }
        CHECK(owner->reserve(label, "0000") == Result::Success);
        CHECK(near(label->box.x, 4.0f * widest));
        auto leaned = Text::gen("1\n9", {70.0f, 0.0f});
        leaned->tabular = true;
        leaned->slant = -0.25f;
        leaned->size = label->size;
        leaned->align = {0.0f, 0.0f};
        leaned->orientation = TextOrientation::Plane;
        CHECK(owner->measure(leaned, sample) == Result::Success && sample.lines == 2u);
        CHECK(owner->add(leaned) == Result::Success);
        BBox ink;
        CHECK(renderer->bounds(owner, leaned, 0.0f, ink) == Result::Success);
        CHECK(std::fabs(ink.width - (sample.right - sample.left)) < 0.05f);
        CHECK(label->count(0.0f) == Result::Success && owner->add(label) == Result::Success);
        auto target = AnimationTarget::from(label);
        target.value(1000.0f);
        CHECK(owner->play(target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
        Surface kept, fresh;
        auto other = SwRenderer::gen();
        CHECK(renderer->render(owner, 1.0f, kept) == Result::Success);
        CHECK(renderer->render(owner, 0.25f, kept) == Result::Success);
        CHECK(other->render(owner, 0.25f, fresh) == Result::Success);
        CHECK(checksum(kept) == checksum(fresh));
        Object* copy = nullptr;
        CHECK(owner->clone(label, copy) == Result::Success);
        CHECK(copy && static_cast<Text*>(copy)->tabular);
        delete other;
        delete owner;
    }
    // Reservation measures a sample without changing the value and never follows later values.
    {
        auto reservedScene = Scene::gen();
        auto label = Text::gen("1");
        CHECK(reservedScene && label);
        TextExtent sample;
        TextExtent initial;
        CHECK(reservedScene->measure(label, "-000.00", sample) == Result::Success);
        CHECK(reservedScene->measure(label, initial) == Result::Success);
        CHECK(sample.width > initial.width);
        CHECK(reservedScene->reserve(label, "-000.00") == Result::Success);
        CHECK(near(label->box.x, sample.width) && std::strcmp(label->text(), "1") == 0);
        auto box = label->box;
        CHECK(reservedScene->reserve(label, nullptr) == Result::InvalidArguments);
        CHECK(near(label->box.x, box.x));
        CHECK(label->text("123456789012345") == Result::Success);
        CHECK(reservedScene->measure(label, initial) == Result::Success && initial.width > box.x);
        CHECK(near(label->box.x, box.x));
        CHECK(reservedScene->reserve(label, "1") == Result::Success && near(label->box.x, initial.width));
        CHECK(reservedScene->add(label) == Result::Success);
        CHECK(reservedScene->reserve(label, "000") == Result::InvalidArguments);
        delete reservedScene;
    }
    // The value animates, and every frame writes the number it has reached.
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    auto counter = Text::gen("", {0.0f, 1.5f, 0.0f});
    auto half = Text::gen("500", {0.0f, 0.0f, 0.0f});
    auto whole = Text::gen("1,000", {0.0f, -1.5f, 0.0f});
    CHECK(scene && counter && half && whole);
    if (scene && counter && half && whole) {
        NumberFormat grouped;
        grouped.separator = ',';
        CHECK(counter->count(0.0f, grouped) == Result::Success);
        Text* texts[] = {counter, half, whole};
        for (auto item : texts) {
            CHECK(item->font("Pretendard") == Result::Success);
            item->size = 28.0f;
        }
        CHECK(scene->add(counter) == Result::Success && scene->add(half) == Result::Success
              && scene->add(whole) == Result::Success);
        auto count = AnimationTarget::from(counter);
        count.value(1000.0f);
        CHECK(scene->play(count, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
        BBox counted;
        BBox expected;
        CHECK(renderer->bounds(scene, counter, 1.0f, counted) == Result::Success);
        CHECK(renderer->bounds(scene, half, 1.0f, expected) == Result::Success);
        CHECK(near(counted.width, expected.width, 0.5f) && near(counted.height, expected.height, 0.5f));
        CHECK(renderer->bounds(scene, counter, 2.0f, counted) == Result::Success);
        CHECK(renderer->bounds(scene, whole, 2.0f, expected) == Result::Success);
        CHECK(near(counted.width, expected.width, 0.5f));
        // The text keeps its latest authored count, like every other property.
        CHECK(counter->value() == 1000.0f && std::strcmp(counter->text(), "1,000") == 0);
        // Only a counting text takes a value.
        auto wrong = AnimationTarget::from(half);
        wrong.value(3.0f);
        CHECK(scene->play(wrong) == Result::InvalidArguments);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "counting text"));
        auto endless = AnimationTarget::from(counter);
        endless.value(std::numeric_limits<float>::infinity());
        CHECK(scene->play(endless) == Result::InvalidArguments);
        // A count and a move run side by side on their own channels.
        auto down = AnimationTarget::from(counter);
        down.value(0.0f);
        auto move = AnimationTarget::from(counter);
        move.shift({1.0f, 0.0f, 0.0f});
        CHECK(scene->play(down, 1.0f) == Result::Success);
        CHECK(scene->seek(2.0f) == Result::Success);
        CHECK(scene->play(move, 1.0f) == Result::Success);
        CHECK(counter->value() == 0.0f && std::strcmp(counter->text(), "0") == 0);
    }
    delete scene;
#else
    (void)renderer;
#endif
}

// What text shows at time, or "?" when it does not fit.
static const char* _shown(const Scene* scene, const Text* text, float time)
{
    static char output[64];
    return scene->text(text, time, output, sizeof(output)) < sizeof(output) ? output : "?";
}

// Plays a linear text change of one second on a new text and checks what it shows a quarter,
// a half and three quarters of the way.
static void _textChange(const char* from, const char* to, TextChange change, const char* quarter,
                        const char* half, const char* threeQuarters)
{
    auto scene = Scene::gen({});
    auto text = Text::gen(from);
    auto added = scene && text && scene->add(text) == Result::Success;
    CHECK(added);
    if (!added) {
        delete text;
        delete scene;
        return;
    }
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    CHECK(scene->play(AnimationTarget::from(text).text(to, change), 1.0f, linear) == Result::Success);
    CHECK(std::strcmp(_shown(scene, text, 0.0f), from) == 0);
    CHECK(std::strcmp(_shown(scene, text, 0.25f), quarter) == 0);
    CHECK(std::strcmp(_shown(scene, text, 0.5f), half) == 0);
    CHECK(std::strcmp(_shown(scene, text, 0.75f), threeQuarters) == 0);
    CHECK(std::strcmp(_shown(scene, text, 1.0f), to) == 0);
    CHECK(std::strcmp(text->text(), to) == 0);
    delete scene;
}

static void textChanges(SwRenderer* renderer)
{
    // Sweep replaces characters from the left; shortening removes them from the right.
    _textChange("cat", "horse", TextChange::Sweep, "hat", "hots", "horse");
    _textChange("horse", "cat", TextChange::Sweep, "horse", "hors", "hat");
    _textChange("abcd", "wxyz", TextChange::Sweep, "wbcd", "wxcd", "wxyd");
    _textChange("", "abc", TextChange::Sweep, "a", "ab", "abc");
    _textChange("abc", "", TextChange::Sweep, "abc", "ab", "a");
    // Characters, not bytes: syllables of three bytes, an emoji with its skin tone, a family
    // of joined emoji, a flag and a letter with its combining accent each change whole.
    _textChange("\xea\xb0\x80", "\xeb\x82\x98\xeb\x8b\xa4\xeb\x9d\xbc", TextChange::Sweep, "\xea\xb0\x80",
                "\xeb\x82\x98\xeb\x8b\xa4", "\xeb\x82\x98\xeb\x8b\xa4\xeb\x9d\xbc");
    _textChange("\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd!", "ok", TextChange::Sweep, "\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd!",
                "o!", "o!");
    _textChange("\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7\xf0\x9f\x87\xb0\xf0\x9f\x87\xb7",
                "xyz", TextChange::Sweep,
                "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7\xf0\x9f\x87\xb0\xf0\x9f\x87\xb7",
                "x\xf0\x9f\x87\xb0\xf0\x9f\x87\xb7z", "xyz");
    _textChange("e\xcc\x81" "a", "xyz", TextChange::Sweep, "e\xcc\x81" "a", "xaz", "xyz");
    // Retyping erases back to what both strings begin with, then types the rest.
    _textChange("Hello world", "Hello there", TextChange::Retype, "Hello wor", "Hello ", "Hello th");
    _textChange("", "abc", TextChange::Retype, "", "a", "ab");
    _textChange("\xea\xb0\x80\xeb\x82\x98", "\xea\xb0\x80\xeb\x8b\xa4\xeb\x9d\xbc", TextChange::Retype,
                "\xea\xb0\x80\xeb\x82\x98", "\xea\xb0\x80", "\xea\xb0\x80\xeb\x8b\xa4");

    auto scene = Scene::gen({});
    auto label = Text::gen("draft", {0.0f, 1.0f, 0.0f});
    auto left = Text::gen("cat", {-2.0f, -1.0f, 0.0f});
    auto counter = Text::gen("");
    auto circle = Circle::gen({0.0f, -1.0f, 0.0f}, 0.5f);
    CHECK(scene && label && left && counter && circle);
    if (!scene || !label || !left || !counter || !circle) {
        delete scene;
        delete label;
        delete left;
        delete counter;
        delete circle;
        return;
    }
    left->align = {0.0f, 0.5f};
    CHECK(counter->count(0.0f) == Result::Success);
    CHECK(scene->add(label) == Result::Success && scene->add(left) == Result::Success
          && scene->add(counter) == Result::Success && scene->add(circle) == Result::Success);
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto says = [scene](const char* words) { return scene->diagnostic() && std::strstr(scene->diagnostic(), words); };
    // Before the timeline a new string becomes the text's own, as Text::text() does.
    CHECK(scene->update(AnimationTarget::from(label).text("cat")) == Result::Success);
    CHECK(std::strcmp(label->text(), "cat") == 0 && std::strcmp(_shown(scene, label, 0.0f), "cat") == 0);
    CHECK(label->texts(nullptr, 0u) == 1u);
    // Text content is a channel of its own: a move, a fade and a recolor run beside it.
    AnimationTarget beside[] = {AnimationTarget::from(label).text("horse"),
                                AnimationTarget::from(left).text("horse"),
                                AnimationTarget::from(circle).fill({255, 0, 0, 255})};
    CHECK(scene->play(beside, 3u, 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(label).shift({1.0f, 0.0f, 0.0f}).opacity(0.5f), 1.0f, linear)
          == Result::Success);
    CHECK(scene->play(AnimationTarget::from(label).fill({0, 0, 255, 255}).text("cat"), 1.0f, linear)
          == Result::Success);
    CHECK(std::strcmp(_shown(scene, label, 0.5f), "hots") == 0);
    CHECK(std::strcmp(_shown(scene, label, 1.5f), "hors") == 0);
    // Sampling is a pure function of time, in any order.
    CHECK(std::strcmp(_shown(scene, label, 0.25f), "hat") == 0);
    CHECK(std::strcmp(_shown(scene, label, 1.75f), "hat") == 0);
    CHECK(std::strcmp(_shown(scene, label, 0.5f), "hots") == 0);
    // The text keeps the string its latest change ends at, and the strings come back.
    CHECK(std::strcmp(label->text(), "cat") == 0);
    const char* strings[3] = {};
    CHECK(label->texts(strings, 3u) == 2u && std::strcmp(strings[0], "cat") == 0
          && std::strcmp(strings[1], "horse") == 0);
    // A change may not begin while another runs, and a plain string waits for none.
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(label).text("dog")) == Result::InsufficientCondition);
    CHECK(says("already animates its text until 2.00 s"));
    CHECK(label->text("dog") == Result::InsufficientCondition && std::strcmp(label->text(), "cat") == 0);
    // After the timeline starts, a set switches the string at the cursor.
    CHECK(scene->seek(2.5f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(label).text("dog")) == Result::Success);
    CHECK(std::strcmp(_shown(scene, label, 2.4f), "cat") == 0);
    CHECK(std::strcmp(_shown(scene, label, 2.5f), "dog") == 0);
    // Only a text that is not a number changes its string, and only to a string.
    CHECK(scene->play(AnimationTarget::from(circle).text("x")) == Result::InvalidArguments);
    CHECK(says("text needs a text object"));
    CHECK(scene->play(AnimationTarget::from(counter).text("x")) == Result::InvalidArguments);
    CHECK(says("counting text changes through its value"));
    CHECK(scene->play(AnimationTarget::from(label).text(nullptr)) == Result::InvalidArguments);
    CHECK(scene->play(AnimationTarget::from(label).text("x", static_cast<TextChange>(7))) == Result::InvalidArguments);
    CHECK(says("the text change is unknown"));
    char room[4];
    CHECK(scene->text(label, 1.0f, room, sizeof(room)) == 5u && room[0] == '\0');
    CHECK(scene->text(counter, 0.0f, room, sizeof(room)) == 1u && std::strcmp(room, "0") == 0);
    CHECK(scene->text(label, -1.0f, room, sizeof(room)) == 0u);
    CHECK(scene->text(nullptr, 0.0f, room, sizeof(room)) == 0u);
#ifdef VIS_TEST_FONT
    // The renderer draws the string the time shows, pinned to the text's point by its align as
    // the width changes: about the center by default, from the left edge here.
    BBox shorter;
    BBox longer;
    CHECK(renderer->bounds(scene, label, 1.75f, shorter) == Result::Success);
    CHECK(renderer->bounds(scene, label, 1.0f, longer) == Result::Success);
    CHECK(longer.width > shorter.width * 1.3f);
    CHECK(near(shorter.x + shorter.width * 0.5f, longer.x + longer.width * 0.5f, 3.0f));
    CHECK(renderer->bounds(scene, left, 0.25f, shorter) == Result::Success);
    CHECK(renderer->bounds(scene, left, 1.0f, longer) == Result::Success);
    CHECK(longer.width > shorter.width * 1.3f && near(shorter.x, longer.x, 1.0f));
    TextExtent hat;
    TextExtent horse;
    CHECK(label->measure("hat", hat) == Result::Success && label->measure("horse", horse) == Result::Success);
    CHECK(hat.width < horse.width && label->measure(nullptr, hat) == Result::InvalidArguments);
#else
    (void)renderer;
#endif
    delete scene;
}

static void matchedPieces(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;

    // Mistakes leave the timeline as it was.
    auto checks = Scene::gen(config);
    auto kept = Group::gen();
    auto inside = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto busy = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto early = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    CHECK(checks && kept && inside && busy && early);
    if (checks && kept && inside && busy && early) {
        CHECK(kept->add(inside) == Result::Success);
        CHECK(checks->add(kept) == Result::Success && checks->add(busy) == Result::Success
              && checks->add(early) == Result::Success);
        CHECK(checks->matchTransform(kept, kept) == Result::InvalidArguments);
        CHECK(checks->matchTransform(kept, inside) == Result::InvalidArguments);
        CHECK(checks->play(Animation::shift(busy, {0.0f, 1.0f, 0.0f}), 2.0f) == Result::Success);
        CHECK(checks->seek(1.0f) == Result::Success);
        auto late = Circle::gen({0.0f, 2.0f, 0.0f}, 0.5f);
        CHECK(late && checks->add(late) == Result::Success);
        auto clips = checks->clipCount();
        CHECK(checks->matchTransform(busy, late) == Result::InsufficientCondition);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "at rest"));
        CHECK(checks->matchTransform(kept, early) == Result::InsufficientCondition);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "right before the match"));
        CHECK(checks->matchTransform(kept, late, 0.0f) == Result::InvalidArguments);
        MatchOptions turn;
        turn.arc = 7.0f;
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, turn) == Result::InvalidArguments);
        MatchOptions faded;
        faded.fade = 0.0f;
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, faded) == Result::InvalidArguments);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "fade share"));
        faded.fade = std::numeric_limits<float>::quiet_NaN();
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, faded) == Result::InvalidArguments);
        Object* foreign[] = {busy, late};
        MatchOptions stray;
        stray.pairs = foreign;
        stray.pairCount = 1;
        CHECK(checks->matchTransform(kept, late, 1.0f, {}, stray) == Result::InvalidArguments);
        CHECK(checks->diagnostic() && std::strstr(checks->diagnostic(), "each pair"));
        CHECK(checks->clipCount() == clips && near(checks->cursor(), 1.0f) && near(checks->duration(), 2.0f));
        // A plain shape pairs with a plain shape only when asked; unpaired, they cross over.
        CHECK(checks->matchTransform(kept, late, 1.0f) == Result::Success);
        CHECK(near(checks->cursor(), 2.0f) && checks->clipCount() == clips + 2u);
        CHECK(checks->play(Animation::fadeOut(kept)) == Result::InvalidArguments);
    } else {
        delete kept;
        delete inside;
        delete busy;
        delete early;
    }
    delete checks;

    // Texts that read the same pair by themselves unless the caller pairs everything: one
    // clip carries a pair, and two fade an unpaired text out and its twin in.
    const bool modes[] = {true, false};
    for (auto strings : modes) {
        auto twins = Scene::gen(config);
        auto from = Group::gen();
        auto to = Group::gen();
        auto first = Text::gen("x", {-1.0f, 0.0f, 0.0f});
        auto second = Text::gen("x", {1.0f, 0.0f, 0.0f});
        CHECK(twins && from && to && first && second);
        if (!twins || !from || !to || !first || !second) {
            delete twins;
            delete from;
            delete to;
            delete first;
            delete second;
            continue;
        }
        CHECK(from->add(first) == Result::Success && to->add(second) == Result::Success);
        CHECK(twins->add(from) == Result::Success && twins->wait(1.0f) == Result::Success);
        CHECK(twins->add(to) == Result::Success);
        MatchOptions options;
        options.strings = strings;
        CHECK(twins->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
        CHECK(twins->clipCount() == (strings ? 1u : 2u));
        delete twins;
    }

#ifdef VIS_TEST_FONT
    // Texts that read the same travel; a given pair cross-fades; the rest fades.
    auto scene = Scene::gen(config);
    auto source = Group::gen();
    auto target = Group::gen();
    auto nested = Group::gen();
    auto a = Text::gen("a", {-2.0f, 0.0f, 0.0f});
    auto plus = Text::gen("+", {-1.0f, 0.0f, 0.0f});
    auto square = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.6f, 0.6f});
    auto gone = Text::gen("z", {2.0f, 0.0f, 0.0f});
    auto plusTo = Text::gen("+", {-1.0f, 1.5f, 0.0f});
    auto aTo = Text::gen("a", {1.5f, 1.5f, 0.0f});
    auto round = Circle::gen({1.0f, -1.5f, 0.0f}, 0.4f);
    auto fresh = Text::gen("c", {-2.0f, -1.5f, 0.0f});
    CHECK(scene && source && target && nested && a && plus && square && gone && plusTo && aTo && round && fresh);
    if (scene && source && target && nested && a && plus && square && gone && plusTo && aTo && round && fresh) {
        Text* texts[] = {a, plus, gone, plusTo, aTo, fresh};
        for (auto item : texts) {
            CHECK(item->font("Pretendard") == Result::Success);
            item->size = 28.0f;
        }
        aTo->size = 56.0f;
        a->fill(Color::hex("#ef5350"));
        aTo->fill(Color::hex("#42a5f5"));
        CHECK(source->add(a) == Result::Success && source->add(plus) == Result::Success
              && source->add(square) == Result::Success && source->add(gone) == Result::Success);
        CHECK(nested->add(aTo) == Result::Success);
        CHECK(target->add(plusTo) == Result::Success && target->add(nested) == Result::Success
              && target->add(round) == Result::Success && target->add(fresh) == Result::Success);
        CHECK(scene->add(source) == Result::Success);
        CHECK(scene->wait(1.0f) == Result::Success);
        CHECK(scene->add(target) == Result::Success);
        Object* pairs[] = {square, round};
        MatchOptions options;
        options.arc = 1.5f;
        options.pairs = pairs;
        options.pairCount = 1;
        CHECK(scene->matchTransform(source, target, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
              == Result::Success);
        CHECK(near(scene->cursor(), 3.0f) && near(scene->duration(), 3.0f));

        // The source text leaves as its pair takes its place, size and color.
        BBox before;
        BBox start;
        BBox middle;
        BBox end;
        CHECK(renderer->bounds(scene, a, 0.9f, before) == Result::Success);
        CHECK(renderer->bounds(scene, a, 1.0f, start) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, aTo, 1.0f, start) == Result::Success);
        CHECK(near(start.x, before.x, 0.5f) && near(start.y, before.y, 0.5f));
        CHECK(near(start.width, before.width, 0.5f) && near(start.height, before.height, 0.5f));
        Surface surface;
        CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
        auto inked = [&](const BBox& box, Color color) {
            auto wanted = pixel(color);
            for (auto y = static_cast<uint32_t>(box.y); y < static_cast<uint32_t>(box.y + box.height); y++) {
                for (auto x = static_cast<uint32_t>(box.x); x < static_cast<uint32_t>(box.x + box.width); x++) {
                    if (surface.data()[static_cast<size_t>(y) * surface.stride() + x] == wanted) return true;
                }
            }
            return false;
        };
        CHECK(inked(start, Color::hex("#ef5350")));
        // It grows and turns blue on the way, along an arc, and lands in its own place.
        CHECK(renderer->bounds(scene, aTo, 2.0f, middle) == Result::Success);
        CHECK(renderer->bounds(scene, aTo, 3.0f, end) == Result::Success);
        CHECK(end.width > start.width * 1.6f && end.center().x > start.center().x + 80.0f);
        CHECK(std::fabs(middle.center().y - (start.center().y + end.center().y) * 0.5f) > 10.0f);
        CHECK(renderer->render(scene, 3.0f, surface) == Result::Success);
        CHECK(inked(end, Color::hex("#42a5f5")) && !inked(end, Color::hex("#ef5350")));
        // Unpaired pieces fade: one in, one out with its object.
        BBox unseen;
        CHECK(renderer->bounds(scene, fresh, 1.0f, unseen) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, fresh, 3.0f, unseen) == Result::Success);
        CHECK(renderer->bounds(scene, gone, 2.0f, unseen) == Result::Success);
        CHECK(renderer->bounds(scene, gone, 3.0f, unseen) == Result::InsufficientCondition);
        // The given pair cross-fades while each travels to the other's place.
        BBox squareStart;
        BBox roundEnd;
        BBox roundMiddle;
        BBox squareMiddle;
        CHECK(renderer->bounds(scene, square, 0.9f, squareStart) == Result::Success);
        CHECK(renderer->bounds(scene, round, 1.0f, unseen) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, round, 2.0f, roundMiddle) == Result::Success);
        CHECK(renderer->bounds(scene, square, 2.0f, squareMiddle) == Result::Success);
        CHECK(near(roundMiddle.center().x, squareMiddle.center().x, 1.0f)
              && near(roundMiddle.center().y, squareMiddle.center().y, 1.0f));
        CHECK(renderer->bounds(scene, round, 3.0f, roundEnd) == Result::Success);
        CHECK(renderer->bounds(scene, square, 3.0f, unseen) == Result::InsufficientCondition);
        CHECK(roundEnd.center().y > squareStart.center().y + 30.0f);
        // The source is gone; the target plays on.
        CHECK(scene->play(Animation::fadeOut(source)) == Result::InvalidArguments);
        CHECK(scene->play(Animation::indicate(aTo)) == Result::Success);

        // Lag starts each later piece later, counting the pieces of each object in order.
        auto next = Group::gen();
        auto plusNext = Text::gen("+", {-1.0f, -1.0f, 0.0f});
        auto aNext = Text::gen("a", {1.0f, -1.0f, 0.0f});
        CHECK(next && plusNext && aNext);
        if (next && plusNext && aNext) {
            Text* more[] = {plusNext, aNext};
            for (auto item : more) {
                CHECK(item->font("Pretendard") == Result::Success);
                item->size = 28.0f;
                CHECK(next->add(item) == Result::Success);
            }
            CHECK(scene->add(next) == Result::Success);
            auto begin = scene->cursor();
            MatchOptions staggered;
            staggered.lag = 0.5f;
            CHECK(scene->matchTransform(target, next, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), staggered)
                  == Result::Success);
            // The target had four pieces, so the last one starts three lags in.
            CHECK(near(scene->cursor(), begin + 2.5f));
            BBox waiting;
            BBox moving;
            BBox first;
            CHECK(renderer->bounds(scene, aNext, begin, first) == Result::Success);
            CHECK(renderer->bounds(scene, aNext, begin + 0.4f, waiting) == Result::Success);
            CHECK(near(waiting.x, first.x, 0.5f) && near(waiting.y, first.y, 0.5f));
            CHECK(renderer->bounds(scene, plusNext, begin + 0.4f, moving) == Result::Success);
            BBox plusFirst;
            CHECK(renderer->bounds(scene, plusNext, begin, plusFirst) == Result::Success);
            CHECK(std::fabs(moving.y - plusFirst.y) > 5.0f);
        } else {
            delete next;
            delete plusNext;
            delete aNext;
        }
    }
    delete scene;
#else
    (void)renderer;
#endif
}

// A group of lines, each a group of the texts of one string split at spaces, set at one unit
// per text and per line from the top left.
static Group* _lines(const char* const* lines, uint32_t count, Text** texts = nullptr)
{
    auto block = Group::gen();
    if (!block) return nullptr;
    auto made = 0u;
    for (auto row = 0u; row < count; row++) {
        auto line = Group::gen();
        if (!line || block->add(line) != Result::Success) {
            delete line;
            delete block;
            return nullptr;
        }
        char word[16];
        auto column = 0u;
        for (auto at = lines[row]; *at;) {
            auto length = 0u;
            while (at[length] && at[length] != ' ' && length < 15u) length++;
            std::memcpy(word, at, length);
            word[length] = '\0';
            auto text = Text::gen(word, {-3.0f + static_cast<float>(column++), 2.0f - static_cast<float>(row), 0.0f});
            if (!text || line->add(text) != Result::Success) {
                delete text;
                delete block;
                return nullptr;
            }
            text->font("Pretendard");
            text->size = 20.0f;
            if (texts) texts[made] = text;
            made++;
            at += length;
            while (*at == ' ') at++;
        }
    }
    return block;
}

static void diffedLines(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;

    // A diff keeps order: words that cross a line kept in place fade, where strings would
    // carry them; one clip carries a pair, and two fade an unpaired text out and its twin in.
    const char* const crossed[] = {"a b", "c"};
    const char* const across[] = {"c", "b a"};
    const bool modes[] = {false, true};
    for (auto diff : modes) {
        auto scene = Scene::gen(config);
        auto from = _lines(crossed, 2u);
        auto to = _lines(across, 2u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            CHECK(scene->add(from) == Result::Success && scene->wait(1.0f) == Result::Success);
            CHECK(scene->add(to) == Result::Success);
            MatchOptions options;
            options.diff = diff;
            CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
            CHECK(scene->clipCount() == (diff ? 5u : 3u));
        } else {
            delete from;
            delete to;
        }
        delete scene;
    }

    // A line that reads as one left over elsewhere moves there whole.
    const char* const pair[] = {"a b", "c d"};
    const char* const swapped[] = {"c d", "a b"};
    auto moved = Scene::gen(config);
    auto from = _lines(pair, 2u);
    auto to = _lines(swapped, 2u);
    CHECK(moved && from && to);
    if (moved && from && to) {
        CHECK(moved->add(from) == Result::Success && moved->wait(1.0f) == Result::Success);
        CHECK(moved->add(to) == Result::Success);
        MatchOptions options;
        options.diff = true;
        CHECK(moved->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
        CHECK(moved->clipCount() == 4u);
    } else {
        delete from;
        delete to;
    }
    delete moved;

    // More lines than a diff takes leave the timeline as it was.
    auto crowded = Scene::gen(config);
    auto many = Group::gen();
    auto one = Text::gen("x", {0.0f, 0.0f, 0.0f});
    CHECK(crowded && many && one);
    if (crowded && many && one) {
        for (auto i = 0u; i < 4097u; i++) {
            auto line = Group::gen();
            if (!line || many->add(line) != Result::Success) delete line;
        }
        CHECK(many->childCount() == 4097u);
        CHECK(crowded->add(many) == Result::Success && crowded->wait(1.0f) == Result::Success);
        CHECK(crowded->add(one) == Result::Success);
        MatchOptions options;
        options.diff = true;
        CHECK(crowded->matchTransform(many, one, 1.0f, {}, options) == Result::InvalidArguments);
        CHECK(crowded->diagnostic() && std::strstr(crowded->diagnostic(), "4096 lines"));
        CHECK(crowded->clipCount() == 0u && near(crowded->cursor(), 1.0f));
    } else {
        delete many;
        delete one;
    }
    delete crowded;

#ifdef VIS_TEST_FONT
    // An edit: a line comes in above, the kept lines move down, and a changed line keeps the
    // words it shares while the rest fades.
    const char* const before[] = {"x = 1", "y = 2", "end"};
    const char* const after[] = {"z = 0", "x = 1", "y = 3", "end"};
    Text* old[7] = {};
    Text* edited[10] = {};
    auto scene = Scene::gen(config);
    auto source = _lines(before, 3u, old);
    auto target = _lines(after, 4u, edited);
    CHECK(scene && source && target);
    if (scene && source && target) {
        CHECK(scene->add(source) == Result::Success && scene->wait(1.0f) == Result::Success);
        CHECK(scene->add(target) == Result::Success);
        MatchOptions options;
        options.diff = true;
        CHECK(scene->matchTransform(source, target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
              == Result::Success);
        auto startsAt = [&](Text* piece, Text* place) {
            BBox start;
            BBox there;
            return renderer->bounds(scene, piece, 1.0f, start) == Result::Success
                   && renderer->bounds(scene, place, 0.9f, there) == Result::Success && near(start.x, there.x, 0.5f)
                   && near(start.y, there.y, 0.5f);
        };
        auto unseen = [&](Text* piece) {
            BBox box;
            return renderer->bounds(scene, piece, 1.0f, box) == Result::InsufficientCondition;
        };
        // x = 1 and end kept their lines; y and = kept their places in the changed line.
        CHECK(startsAt(edited[3], old[0]) && startsAt(edited[4], old[1]) && startsAt(edited[5], old[2]));
        CHECK(startsAt(edited[6], old[3]) && startsAt(edited[7], old[4]) && startsAt(edited[9], old[6]));
        // The new line comes in whole, = too, and 3 replaces 2.
        CHECK(unseen(edited[0]) && unseen(edited[1]) && unseen(edited[2]) && unseen(edited[8]));
        BBox leaving;
        CHECK(renderer->bounds(scene, old[5], 1.5f, leaving) == Result::Success);
        CHECK(renderer->bounds(scene, old[5], 2.0f, leaving) == Result::InsufficientCondition);
    } else {
        delete source;
        delete target;
    }
    delete scene;

    // With a share of 0.4, what leaves is gone before what arrives shows, while a pair
    // travels the whole time.
    const char* const kept[] = {"a b"};
    const char* const changed[] = {"c a"};
    Text* first[2] = {};
    Text* second[2] = {};
    scene = Scene::gen(config);
    source = _lines(kept, 1u, first);
    target = _lines(changed, 1u, second);
    CHECK(scene && source && target);
    if (scene && source && target) {
        CHECK(scene->add(source) == Result::Success && scene->wait(1.0f) == Result::Success);
        CHECK(scene->add(target) == Result::Success);
        MatchOptions options;
        options.fade = 0.4f;
        CHECK(scene->matchTransform(source, target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
              == Result::Success);
        CHECK(near(scene->cursor(), 2.0f));
        BBox box;
        CHECK(renderer->bounds(scene, first[1], 1.2f, box) == Result::Success);
        CHECK(renderer->bounds(scene, first[1], 1.45f, box) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, second[0], 1.5f, box) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, second[0], 1.8f, box) == Result::Success);
        BBox start;
        BBox middle;
        BBox end;
        CHECK(renderer->bounds(scene, second[1], 1.0f, start) == Result::Success);
        CHECK(renderer->bounds(scene, second[1], 1.5f, middle) == Result::Success);
        CHECK(renderer->bounds(scene, second[1], 2.0f, end) == Result::Success);
        CHECK(near(middle.x, (start.x + end.x) * 0.5f, 1.0f) && end.x > start.x + 20.0f);
    } else {
        delete source;
        delete target;
    }
    delete scene;
#else
    (void)renderer;
#endif
}

// A match and a cross-fade record that their objects hand over while they play.
// The pixel center of object at time along x, or -1 when it is not drawn.
static float _shownX(SwRenderer* renderer, Scene* scene, const Object* object, float time)
{
    BBox box;
    if (renderer->bounds(scene, object, time, box) != Result::Success) return -1.0f;
    return box.center().x;
}

static void arrangedRows(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto box = [](float x) {
        auto shape = Rectangle::gen({x, 0.0f, 0.0f}, {1.0f, 1.0f});
        if (shape) shape->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        return shape;
    };

    // A box joins a row of five at the third place: the others slide apart and it grows in.
    auto scene = Scene::gen(config);
    Object* items[6] = {box(-2.5f), box(-1.25f), nullptr, box(0.0f), box(1.25f), box(2.5f)};
    CHECK(scene && items[0] && items[1] && items[3] && items[4] && items[5]);
    if (scene && items[0] && items[1] && items[3] && items[4] && items[5]) {
        for (auto i = 0u; i < 6u; i++) CHECK(i == 2u || scene->add(items[i]) == Result::Success);
        CHECK(scene->wait(0.5f) == Result::Success);
        items[2] = box(4.0f);
        CHECK(items[2] && scene->add(items[2]) == Result::Success);
        CHECK(scene->arrange(items, 6u, {1.0f, 0.0f, 0.0f}, 0.25f, {}, 1.0f, linear) == Result::Success);
        CHECK(near(scene->duration(), 1.5f));
        // Six boxes a quarter apart span 7.25 units, 30 pixels each, around the middle.
        float slots[] = {-3.125f, -1.875f, -0.625f, 0.625f, 1.875f, 3.125f};
        float starts[] = {-2.5f, -1.25f, -0.625f, 0.0f, 1.25f, 2.5f};
        for (auto i = 0u; i < 6u; i++) {
            CHECK(near(_shownX(renderer, scene, items[i], 1.5f), 160.0f + 30.0f * slots[i], 0.6f));
            if (i != 2u) CHECK(near(_shownX(renderer, scene, items[i], 0.5f), 160.0f + 30.0f * starts[i], 0.6f));
        }
        CHECK(near(_shownX(renderer, scene, items[0], 1.0f), 160.0f + 30.0f * -2.8125f, 0.6f));
        // The new box is unseen before and half grown halfway, in its own place.
        BBox grown;
        CHECK(renderer->bounds(scene, items[2], 0.25f, grown) == Result::InsufficientCondition);
        CHECK(renderer->bounds(scene, items[2], 1.0f, grown) == Result::Success);
        CHECK(near(grown.width, 15.0f, 0.6f) && near(grown.center().x, 160.0f + 30.0f * -0.625f, 0.6f));
        // Arranged again in place, nothing moves and the call waits.
        auto clips = scene->clipCount();
        CHECK(scene->arrange(items, 6u, {1.0f, 0.0f, 0.0f}, 0.25f, {}, 0.5f, linear) == Result::Success);
        CHECK(scene->clipCount() == clips && near(scene->duration(), 2.0f));
        // A column runs down from its top item, centered where it is told.
        CHECK(scene->arrange(items, 3u, {0.0f, -1.0f, 0.0f}, 0.5f, {-4.0f, 0.0f, 0.0f}, 1.0f, linear)
              == Result::Success);
        BBox top;
        CHECK(renderer->bounds(scene, items[0], 3.0f, top) == Result::Success);
        CHECK(near(top.center().x, 160.0f - 120.0f, 0.6f) && near(top.center().y, 90.0f - 45.0f, 0.6f));
        // Refusals say what is wrong.
        auto group = Group::gen();
        auto stranger = box(0.0f);
        CHECK(group && stranger && group->add(stranger) == Result::Success && scene->add(group) == Result::Success);
        Object* mixed[] = {items[0], stranger};
        CHECK(scene->arrange(mixed, 2u) == Result::InvalidArguments);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "share one parent"));
        Object* twice[] = {items[0], items[0]};
        CHECK(scene->arrange(twice, 2u) == Result::InvalidArguments);
        CHECK(scene->arrange(items, 6u, {0.0f, 0.0f, 0.0f}) == Result::InvalidArguments);
        CHECK(scene->arrange(items, 6u, {1.0f, 0.0f, 0.0f}, -1.0f) == Result::InvalidArguments);
        auto empty = Group::gen();
        CHECK(empty && scene->add(empty) == Result::Success);
        Object* hollow[] = {items[0], empty};
        CHECK(scene->arrange(hollow, 2u) == Result::InvalidArguments);
        CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "with bounds"));
    } else {
        for (auto item : items) delete item;
    }
    delete scene;
}

static void handedOver(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    auto from = Group::gen();
    auto to = Group::gen();
    auto a = Text::gen("a", {0.0f, 0.0f, 0.0f});
    auto b = Text::gen("b", {0.0f, 0.0f, 0.0f});
    auto other = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    CHECK(scene && from && to && a && b && other);
    if (!scene || !from || !to || !a || !b || !other) {
        delete scene;
        delete from;
        delete to;
        delete a;
        delete b;
        delete other;
        return;
    }
    CHECK(from->add(a) == Result::Success && to->add(b) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->add(other) == Result::Success);
    CHECK(scene->wait(1.0f) == Result::Success && scene->add(to) == Result::Success);
    CHECK(!scene->handoff(a, b, 1.5f));
    auto unrelated = Text::gen("z", {1.0f, 0.0f, 0.0f});
    CHECK(unrelated && from->add(unrelated) == Result::Success);
    CHECK(scene->matchTransform(from, to, 1.0f) == Result::Success);
    CHECK(!scene->handoff(unrelated, b, 1.5f));
    // An unmatched fade at the same authored slot meets either way round.
    CHECK(scene->handoff(a, b, 1.5f) && scene->handoff(b, a, 1.5f) && scene->handoff(from, to, 1.0f));
    CHECK(!scene->handoff(a, b, 0.5f) && !scene->handoff(a, b, 2.5f));
    CHECK(!scene->handoff(a, other, 1.5f) && !scene->handoff(a, a, 1.5f) && !scene->handoff(nullptr, b, 1.5f));
    // A cross-fade hands over too.
    auto next = Circle::gen({2.0f, 1.0f, 0.0f}, 0.5f);
    CHECK(next && scene->add(next) == Result::Success);
    CHECK(scene->fadeTransform(other, next, 1.0f) == Result::Success);
    CHECK(scene->handoff(next, other, 2.5f) && !scene->handoff(next, other, 1.5f) && !scene->handoff(b, next, 2.5f));
    // The layout report gives a scene the time it shows.
    LayoutReport report;
    CHECK(renderer->layout(scene, 1.5f, report) == Result::Success);
    CHECK(report.sceneCount() == 1u && report.sceneAt(0) && near(report.sceneAt(0)->time, 1.5f));
    delete scene;
}

// A departing unpaired token yields its final slot to a moved token only while it fades.
// With copy, the original stays and is not a replacement slot.
static void replacementContacts()
{
    for (auto copying : {false, true}) {
        auto scene = Scene::gen({});
        auto from = Group::gen();
        auto to = Group::gen();
        auto kept = Text::gen("a", {-1.0f, 0.0f, 0.0f});
        auto replaced = Text::gen("b", {1.0f, 0.0f, 0.0f});
        auto arriving = Text::gen("a", {1.0f, 0.0f, 0.0f});
        CHECK(scene && from && to && kept && replaced && arriving);
        if (!scene || !from || !to || !kept || !replaced || !arriving) {
            delete scene; delete from; delete to;
            delete kept; delete replaced; delete arriving;
            continue;
        }
        from->tag("slots");
        CHECK(std::strcmp(from->tag(), "slots") == 0);
        CHECK(from->add(kept) == Result::Success && from->add(replaced) == Result::Success);
        CHECK(to->add(arriving) == Result::Success);
        CHECK(scene->add(from) == Result::Success && scene->wait(0.13f) == Result::Success
              && scene->add(to) == Result::Success);
        MatchOptions options;
        options.copy = copying;
        options.fade = 0.4f;
        options.lag = 0.3f;
        CHECK(scene->matchTransform(from, to, 0.7f, {}, options) == Result::Success);
        CHECK(scene->handoff(kept, arriving, 0.4f));
        CHECK(scene->handoff(replaced, arriving, 0.4f) == !copying);
        CHECK(!scene->handoff(replaced, arriving, 0.7f));
        if (copying) {
            auto copy = scene->object("slots:copy");
            CHECK(copy && scene->handoff(copy->childAt(1), arriving, 0.4f));
            CHECK(copy && !scene->handoff(copy->childAt(1), arriving, 0.7f));
        }
        auto begin = static_cast<float>(static_cast<double>(0.13f) + static_cast<double>(0.7f) * 0.3f);
        auto end = begin + static_cast<float>(static_cast<double>(0.7f) * 0.4f);
        auto fading = copying ? scene->object("slots:copy")->childAt(1) : replaced;
        CHECK(!scene->handoff(fading, arriving, std::nextafter(begin, -1.0f)));
        CHECK(scene->handoff(fading, arriving, begin) && scene->handoff(fading, arriving, end));
        CHECK(!scene->handoff(fading, arriving, std::nextafter(end, 2.0f)));
        delete scene;
    }
}

// The opacity a black shape shows over the white background of a 320x180 frame, six units
// high, at a world point inside it.
static float _shownOpacity(SwRenderer* renderer, Scene* scene, float time, const Vec3& point)
{
    Surface surface;
    if (renderer->render(scene, time, surface) != Result::Success || !surface.data()) return -1.0f;
    auto x = static_cast<uint32_t>(160.0f + 30.0f * point.x);
    auto y = static_cast<uint32_t>(90.0f - 30.0f * point.y);
    auto red = surface.data()[static_cast<size_t>(y) * surface.stride() + x] & 0xffu;
    return 1.0f - static_cast<float>(red) / 255.0f;
}

static void matchedShapes(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};

    // A shape paired with one of its kind becomes it: the source leaves at once, and its pair
    // starts in its place, size, and colors, then travels to its own.
    auto scene = Scene::gen(config);
    auto from = Group::gen();
    auto to = Group::gen();
    auto wide = Rectangle::gen({-2.0f, 0.5f, 0.0f}, {2.0f, 1.0f}, 0.2f);
    auto tall = Rectangle::gen({2.0f, 0.5f, 0.0f}, {0.6f, 2.0f});
    auto small = Circle::gen({-2.0f, -1.5f, 0.0f}, 0.3f);
    auto large = Circle::gen({2.0f, -1.5f, 0.0f}, 0.6f);
    CHECK(scene && from && to && wide && tall && small && large);
    if (!scene || !from || !to || !wide || !tall || !small || !large) {
        delete scene;
        delete from;
        delete to;
        delete wide;
        delete tall;
        delete small;
        delete large;
        return;
    }
    Object* shapes[] = {wide, tall, small, large};
    for (auto shape : shapes) shape->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
    tall->fill({0, 0, 0, 128});
    CHECK(from->add(wide) == Result::Success && from->add(small) == Result::Success);
    CHECK(to->add(tall) == Result::Success && to->add(large) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->wait(1.0f) == Result::Success);
    CHECK(scene->add(to) == Result::Success);
    MatchOptions options;
    options.pairs = shapes;
    options.pairCount = 2;
    CHECK(scene->matchTransform(from, to, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
          == Result::Success);
    CHECK(scene->clipCount() == 2u);
    BBox before;
    BBox start;
    BBox middle;
    BBox end;
    CHECK(renderer->bounds(scene, wide, 0.9f, before) == Result::Success);
    CHECK(renderer->bounds(scene, wide, 1.0f, start) == Result::InsufficientCondition);
    CHECK(renderer->bounds(scene, tall, 1.0f, start) == Result::Success);
    CHECK(near(start.x, before.x, 0.5f) && near(start.y, before.y, 0.5f));
    CHECK(near(start.width, before.width, 0.5f) && near(start.height, before.height, 0.5f));
    CHECK(near(_shownOpacity(renderer, scene, 1.0f, {-2.0f, 0.5f, 0.0f}), 1.0f, 0.02f));
    // Halfway it is halfway in place, size, and fill.
    CHECK(renderer->bounds(scene, tall, 2.0f, middle) == Result::Success);
    CHECK(renderer->bounds(scene, tall, 3.0f, end) == Result::Success);
    CHECK(near(middle.center().x, (start.center().x + end.center().x) * 0.5f, 0.5f));
    CHECK(near(middle.width, (start.width + end.width) * 0.5f, 0.5f));
    CHECK(near(middle.height, (start.height + end.height) * 0.5f, 0.5f));
    CHECK(near(_shownOpacity(renderer, scene, 2.0f, {0.0f, 0.5f, 0.0f}), 0.75f, 0.03f));
    CHECK(near(end.width, 18.0f, 0.5f) && near(end.height, 60.0f, 0.5f));
    CHECK(near(_shownOpacity(renderer, scene, 3.0f, {2.0f, 0.5f, 0.0f}), 0.5f, 0.02f));
    // A circle grows into the circle it pairs with.
    CHECK(renderer->bounds(scene, small, 0.9f, before) == Result::Success);
    CHECK(renderer->bounds(scene, large, 1.0f, start) == Result::Success);
    CHECK(near(start.x, before.x, 0.5f) && near(start.width, before.width, 0.5f));
    CHECK(renderer->bounds(scene, large, 3.0f, end) == Result::Success);
    CHECK(near(end.width, 36.0f, 0.5f));
    delete scene;

    // Shapes pair in order, kind by kind, when asked. Two bars split into four: the first of
    // each run becomes its bar, and the second starts unseen in its place and fades in on the
    // way to its own; four merging into two fade the second of each run out into its bar.
    const bool splits[] = {true, false};
    for (auto split : splits) {
        auto bars = Scene::gen(config);
        auto two = Group::gen();
        auto four = Group::gen();
        auto dot = Circle::gen({0.0f, 2.0f, 0.0f}, 0.2f);
        Rectangle* halves[4] = {};
        Rectangle* wholes[2] = {};
        auto made = bars && two && four && dot;
        for (auto i = 0u; i < 4u; i++) {
            halves[i] = Rectangle::gen({(i < 2u ? -2.5f : 1.5f) + static_cast<float>(i % 2u), 0.0f, 0.0f}, {0.8f, 1.0f});
            made = made && halves[i];
        }
        for (auto i = 0u; i < 2u; i++) {
            wholes[i] = Rectangle::gen({i ? 2.0f : -2.0f, 0.0f, 0.0f}, {2.0f, 1.0f});
            made = made && wholes[i];
        }
        CHECK(made);
        if (!made) {
            delete bars;
            delete two;
            delete four;
            delete dot;
            for (auto half : halves) delete half;
            for (auto whole : wholes) delete whole;
            continue;
        }
        for (auto whole : wholes) {
            whole->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(two->add(whole) == Result::Success);
        }
        for (auto half : halves) {
            half->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(four->add(half) == Result::Success);
        }
        CHECK((split ? two : four)->add(dot) == Result::Success);
        auto from = split ? two : four;
        auto to = split ? four : two;
        CHECK(bars->add(from) == Result::Success && bars->wait(1.0f) == Result::Success);
        CHECK(bars->add(to) == Result::Success);
        MatchOptions shaped;
        shaped.shapes = true;
        CHECK(bars->matchTransform(from, to, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear), shaped)
              == Result::Success);
        // Four bars play, and the lone circle of the source fades out.
        CHECK(bars->clipCount() == 5u);
        BBox first;
        BBox second;
        BBox whole;
        if (split) {
            CHECK(renderer->bounds(bars, wholes[0], 0.9f, whole) == Result::Success);
            CHECK(renderer->bounds(bars, halves[0], 1.0f, first) == Result::Success);
            CHECK(near(first.x, whole.x, 0.5f) && near(first.width, whole.width, 0.5f));
            CHECK(renderer->bounds(bars, halves[1], 1.0f, second) == Result::InsufficientCondition);
            CHECK(renderer->bounds(bars, halves[1], 2.0f, second) == Result::Success);
            CHECK(near(second.center().x, 107.5f, 0.5f) && near(second.width, 42.0f, 0.5f));
            CHECK(renderer->bounds(bars, halves[3], 3.0f, second) == Result::Success);
            CHECK(near(second.center().x, 235.0f, 0.5f) && near(second.width, 24.0f, 0.5f));
        } else {
            CHECK(renderer->bounds(bars, halves[0], 1.0f, first) == Result::InsufficientCondition);
            CHECK(renderer->bounds(bars, wholes[0], 1.0f, whole) == Result::Success);
            CHECK(near(whole.center().x, 85.0f, 0.5f) && near(whole.width, 24.0f, 0.5f));
            CHECK(renderer->bounds(bars, halves[1], 2.0f, second) == Result::Success);
            CHECK(near(second.center().x, 107.5f, 0.5f) && near(second.width, 42.0f, 0.5f));
            CHECK(renderer->bounds(bars, halves[3], 2.99f, second) == Result::Success);
            CHECK(near(second.center().x, 220.0f, 1.0f) && near(second.width, 60.0f, 1.0f));
            CHECK(renderer->bounds(bars, halves[3], 3.0f, second) == Result::InsufficientCondition);
        }
        delete bars;
    }
}

// Pixels at least half dark on a white background.
static uint32_t _inked(SwRenderer* renderer, Scene* scene, float time)
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

static void morphedOutlines(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);

    // A circle becomes a larger square: each side bulges out of the circle and its corners
    // come from the points at 45 degrees, so halfway the shape reaches 1.25 units out.
    {
        auto scene = Scene::gen(config);
        auto circle = Circle::gen({}, 1.0f);
        auto square = Rectangle::gen({}, {3.0f, 3.0f});
        CHECK(scene && circle && square);
        if (scene && circle && square) {
            circle->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            square->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(circle) == Result::Success && scene->wait(1.0f) == Result::Success);
            CHECK(scene->add(square) == Result::Success);
            CHECK(scene->play(Animation::morph(circle, square), 2.0f, linear) == Result::Success);
            BBox middle;
            CHECK(renderer->bounds(scene, circle, 2.0f, middle) == Result::Success);
            CHECK(near(middle.width, 75.0f, 1.0f) && near(middle.height, 75.0f, 1.0f));
            CHECK(near(_shownOpacity(renderer, scene, 0.9f, {1.2f, 0.0f, 0.0f}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 2.0f, {1.2f, 0.0f, 0.0f}), 1.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 2.0f, {0.0f, 0.0f, 0.0f}), 1.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 2.0f, {1.2f, 1.2f, 0.0f}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 3.0f, {1.4f, 1.4f, 0.0f}), 1.0f, 0.02f));
            CHECK(renderer->bounds(scene, circle, 3.0f, middle) == Result::InsufficientCondition);
        } else {
            delete circle;
            delete square;
        }
        delete scene;
    }

    // A star listed clockwise runs against the square's outline; the morph turns it around
    // instead of pulling the shape through its center.
    {
        auto scene = Scene::gen(config);
        auto square = Rectangle::gen({}, {3.0f, 3.0f});
        Vec3 points[10];
        for (auto i = 0u; i < 10u; i++) {
            auto angle = 1.5707963f - 0.62831853f * static_cast<float>(i);
            auto radius = i % 2u ? 0.7f : 1.6f;
            points[i] = {radius * std::cos(angle), radius * std::sin(angle), 0.0f};
        }
        auto star = Polygon::gen(points, 10u);
        CHECK(scene && square && star);
        if (scene && square && star) {
            square->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            star->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(square) == Result::Success && scene->add(star) == Result::Success);
            CHECK(scene->play(Animation::morph(square, star), 2.0f, linear) == Result::Success);
            auto start = _inked(renderer, scene, 0.0f);
            auto middle = _inked(renderer, scene, 1.0f);
            auto end = _inked(renderer, scene, 2.0f);
            CHECK(near(static_cast<float>(start), 8100.0f, 200.0f));
            CHECK(near(static_cast<float>(end), 2962.0f, 150.0f));
            CHECK(middle > end && middle < start);
        } else {
            delete square;
            delete star;
        }
        delete scene;
    }

    // An open outline starts at either end of the other: a line becomes a plot drawn from right
    // to left without folding through its middle.
    {
        auto scene = Scene::gen(config);
        auto line = Line::gen({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
        Vec3 points[] = {{2.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f}};
        auto plot = Plot::gen(points, 3u);
        auto circle = Circle::gen({}, 0.5f);
        CHECK(scene && line && plot && circle);
        if (scene && line && plot && circle) {
            CHECK(scene->add(line) == Result::Success && scene->add(plot) == Result::Success);
            CHECK(scene->add(circle) == Result::Success);
            CHECK(scene->play(Animation::morph(circle, plot), 1.0f) == Result::NonSupport);
            CHECK(scene->play(Animation::morph(line, plot), 2.0f, linear) == Result::Success);
            BBox middle;
            CHECK(renderer->bounds(scene, line, 1.0f, middle) == Result::Success);
            CHECK(middle.width >= 120.0f && middle.width < 126.0f);
            CHECK(near(middle.center().y, 75.0f, 1.0f));
        } else {
            delete line;
            delete plot;
            delete circle;
        }
        delete scene;
    }

    // Two polygons with the same number of points keep the pairing they were given: corner i
    // goes to corner i, so a square whose corners are listed one place on turns into a diamond
    // halfway.
    {
        auto scene = Scene::gen(config);
        Vec3 corners[] = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
        Vec3 turned[] = {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f}};
        auto from = Polygon::gen(corners, 4u);
        auto to = Polygon::gen(turned, 4u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            from->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            to->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            CHECK(scene->play(Animation::morph(from, to), 2.0f, linear) == Result::Success);
            CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.9f, 0.9f, 0.0f}), 1.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.9f, 0.9f, 0.0f}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.0f, 0.0f, 0.0f}), 1.0f, 0.02f));
        } else {
            delete from;
            delete to;
        }
        delete scene;
    }
}

static double _morphArea(const LayoutPath* path)
{
    auto area = 0.0;
    for (auto i = 0u; i < path->count; i++) {
        auto a = path->points[i];
        auto b = path->points[(i + 1u) % path->count];
        area += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
    }
    return area * 0.5;
}

static void pieceMorphs(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto move = [](float x, float y) { return PathCommand::move({x, y, 0.0f}); };
    auto line = [](float x, float y) { return PathCommand::line({x, y, 0.0f}); };

    // Piece order differs across kinds. Automatic pairs stay on their own side; explicit
    // pairs cross over. A rejected pair leaves both objects and the timeline untouched.
    for (auto explicitPairs : {false, true}) {
        PathCommand commands[] = {move(-2, 0), line(-1, 0), move(1, 0), line(3, 0)};
        Vec3 points[] = {{3, 2, 0}, {1, 2, 0}, {-1, 2, 0}, {-2, 2, 0}};
        uint32_t ends[] = {2, 4};
        auto scene = Scene::gen(config);
        auto from = Path::gen(commands, 4u);
        auto to = Plot::gen(points, 4u, ends, 2u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            uint32_t duplicate[] = {0, 0, 0, 1};
            auto animation = Animation::morph(from, to);
            animation.pairs = duplicate;
            animation.pairCount = 2;
            CHECK(scene->play(animation, 1.0f, linear) == Result::InvalidArguments);
            CHECK(near(scene->duration(), 0.0f) && scene->clipCount() == 0u);
            uint32_t pairs[] = {0, 0, 1, 1};
            animation.pairs = explicitPairs ? pairs : nullptr;
            animation.pairCount = explicitPairs ? 2u : 0u;
            CHECK(scene->play(animation, 1.0f, linear) == Result::Success);
            // The caller's pair array is no longer needed after play.
            pairs[0] = 100u;
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.5f, report) == Result::Success && report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                auto right = report.pathAt(0)->paintBounds.center().x;
                auto left = report.pathAt(1)->paintBounds.center().x;
                CHECK(explicitPairs ? near(right, 167.5f, 0.1f) && near(left, 167.5f, 0.1f)
                                    : near(right, 220.0f, 0.1f) && near(left, 115.0f, 0.1f));
            }
        } else { delete from; delete to; }
        delete scene;
    }

    // Keep centroid distances in double precision: a float-rounded best distance could
    // select the farther, larger target even after the exact-size target was found.
    {
        Vec3 a[] = {{-1, 0, 0}, {1, 0, 0}, {-6, 0, 0}, {6, 0, 0}};
        Vec3 b[] = {{1, 0, 0}, {3, 0, 0}, {-4, 0, 0}, {0, 0, 0}, {-6, 0, 0}, {6, 0, 0}};
        uint32_t fromEnds[] = {2, 4};
        uint32_t toEnds[] = {2, 4, 6};
        auto scene = Scene::gen(config);
        auto from = Plot::gen(a, 4u, fromEnds, 2u);
        auto to = Plot::gen(b, 6u, toEnds, 3u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            auto animation = Animation::morph(from, to);
            uint32_t pairs[] = {1, 2};
            animation.pairs = pairs;
            animation.pairCount = 1;
            CHECK(scene->play(animation, 1.0f, linear) == Result::Success);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.5f, report) == Result::Success && report.pathCount() == 3u);
            if (report.pathCount() == 3u) CHECK(near(report.pathAt(1)->paintBounds.center().x, 190.0f, 0.1f));
        } else { delete from; delete to; }
        delete scene;
    }

    // Reserve open targets for open sources before a closed source takes a mixed pair.
    {
        Vec3 points[] = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}, {-1, -1, 0},
                         {3, 2, 0}, {5, 2, 0}};
        uint32_t ends[] = {5, 7};
        auto scene = Scene::gen(config);
        auto source = Plot::gen(points, 7u, ends, 2u);
        auto target = Line::gen({-1, 2, 0}, {1, 2, 0});
        CHECK(scene && source && target);
        if (scene && source && target) {
            CHECK(scene->add(source) == Result::Success && scene->add(target) == Result::Success);
            CHECK(scene->play(Animation::morph(source, target), 1.0f, linear) == Result::Success);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.5f, report) == Result::Success && report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                CHECK(report.pathAt(0)->closed && !report.pathAt(1)->closed);
                CHECK(near(static_cast<float>(std::fabs(_morphArea(report.pathAt(0)))), 900.0f, 0.1f));
                CHECK(near(report.pathAt(1)->paintBounds.center().x, 220.0f, 0.1f));
            }
        } else { delete source; delete target; }
        delete scene;
    }

    PathCommand ring[] = {move(-2, -2), line(2, -2), line(2, 2), line(-2, 2), PathCommand::close(),
                          move(-1, -1), line(-1, 1), line(1, 1), line(1, -1), PathCommand::close()};
    // A hole shrinks to its own centroid, or grows from it, quadratically in area and without
    // reversing its winding. Endpoint pixels are exactly those of the target on its own.
    for (auto grow : {false, true}) {
        auto scene = Scene::gen(config);
        auto frame = Path::gen(ring, 10u);
        auto square = Rectangle::gen({}, {4, 4});
        auto alone = Scene::gen(config);
        auto final = grow ? static_cast<Object*>(Path::gen(ring, 10u))
                          : static_cast<Object*>(Rectangle::gen({}, {4, 4}));
        CHECK(scene && frame && square && alone && final);
        if (scene && frame && square && alone && final) {
            for (auto object : {static_cast<Object*>(frame), static_cast<Object*>(square), final})
                object->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            auto from = grow ? static_cast<Object*>(square) : static_cast<Object*>(frame);
            auto to = grow ? static_cast<Object*>(frame) : static_cast<Object*>(square);
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            CHECK(alone->add(final) == Result::Success);
            CHECK(scene->play(Animation::morph(from, to), 1.0f, linear) == Result::Success);
            for (auto t : {0.0f, 0.25f, 0.5f, 0.75f, 0.999f}) {
                LayoutReport report;
                CHECK(renderer->layout(scene, t, report) == Result::Success && report.pathCount() == 2u);
                if (report.pathCount() != 2u) continue;
                auto outer = _morphArea(report.pathAt(0));
                auto hole = _morphArea(report.pathAt(1));
                auto scale = grow ? t : 1.0f - t;
                CHECK(near(static_cast<float>(std::fabs(hole)), 3600.0f * scale * scale, 0.1f));
                CHECK(hole == 0.0 || outer * hole < 0.0);
                auto center = report.pathAt(1)->paintBounds.center();
                CHECK(near(center.x, 160.0f, 0.1f) && near(center.y, 90.0f, 0.1f));
            }
            CHECK(near(_shownOpacity(renderer, scene, 0.5f, {}), 0.0f, 0.02f));
            Surface actual, expected;
            CHECK(renderer->render(scene, 1.0f, actual) == Result::Success);
            CHECK(renderer->render(alone, 0.0f, expected) == Result::Success);
            CHECK(checksum(actual) == checksum(expected));
            CHECK(renderer->render(scene, 0.25f, actual) == Result::Success);
            auto hash = checksum(actual);
            CHECK(renderer->render(scene, 0.75f, actual) == Result::Success);
            CHECK(renderer->render(scene, 0.25f, actual) == Result::Success && checksum(actual) == hash);
        } else { delete frame; delete square; delete final; }
        delete scene;
        delete alone;
    }

    // Filled open contours share even-odd coverage too; implicit closing edges still cut holes.
    {
        PathCommand open[] = {move(-2, -2), line(2, -2), line(2, 2), line(-2, 2),
                              move(-1, -1), line(-1, 1), line(1, 1), line(1, -1)};
        auto scene = Scene::gen(config);
        auto from = Path::gen(open, 8u);
        auto to = Path::gen(open, 8u);
        CHECK(scene && from && to);
        if (scene && from && to) {
            from->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            to->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
            CHECK(scene->add(from) == Result::Success && scene->add(to) == Result::Success);
            CHECK(scene->play(Animation::morph(from, to), 1.0f, linear) == Result::Success);
            CHECK(near(_shownOpacity(renderer, scene, 0.5f, {}), 0.0f, 0.02f));
            CHECK(near(_shownOpacity(renderer, scene, 0.5f, {1.5f, 0, 0}), 1.0f, 0.02f));
        } else { delete from; delete to; }
        delete scene;
    }

    // An incoming stroke grows with its points rather than showing a round cap at time zero.
    {
        auto scene = Scene::gen(config);
        auto source = Line::gen({-2, 0, 0}, {-1, 0, 0});
        Vec3 points[] = {{-2, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {2, 0, 0}};
        uint32_t ends[] = {2, 4};
        auto target = Plot::gen(points, 4u, ends, 2u);
        CHECK(scene && source && target);
        if (scene && source && target) {
            source->stroke({0, 0, 0, 255});
            target->stroke({0, 0, 0, 255});
            CHECK(scene->add(source) == Result::Success);
            Surface before, after;
            CHECK(renderer->render(scene, 0.0f, before) == Result::Success);
            CHECK(scene->add(target) == Result::Success);
            CHECK(scene->play(Animation::morph(source, target), 1.0f, linear) == Result::Success);
            CHECK(renderer->render(scene, 0.0f, after) == Result::Success && checksum(before) == checksum(after));
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.25f, report) == Result::Success && report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                CHECK(near(report.pathAt(1)->strokeWidth, report.pathAt(0)->strokeWidth * 0.25f));
                CHECK(near(report.pathAt(1)->points[1].x - report.pathAt(1)->points[0].x, 7.5f));
            }
        } else { delete source; delete target; }
        delete scene;
    }

    // Copies keep all contours and array lag still delays each object pair.
    auto scene = Scene::gen(config);
    Vec3 two[] = {{-2, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    uint32_t ends[] = {2, 4};
    auto source = Plot::gen(two, 4u, ends, 2u);
    auto first = Plot::gen(two, 2u);
    auto second = Plot::gen(two, 4u, ends, 2u);
    CHECK(scene && source && first && second);
    if (scene && source && first && second) {
        CHECK(scene->add(source) == Result::Success);
        Object* copy = nullptr;
        CHECK(scene->clone(source, copy) == Result::Success);
        CHECK(copy && static_cast<Plot*>(copy)->contours() == 2u);
        CHECK(scene->add(first) == Result::Success && scene->add(second) == Result::Success);
        Animation animations[] = {Animation::morph(source, first), Animation::morph(source, second)};
        animations[0].copy = animations[1].copy = true;
        CHECK(scene->play(animations, 2u, 1.0f, linear, 0.25f) == Result::Success);
        CHECK(near(scene->duration(), 1.25f));
        BBox bounds;
        CHECK(renderer->bounds(scene, source, 1.25f, bounds) == Result::Success);
        CHECK(renderer->bounds(scene, first, 1.25f, bounds) == Result::Success);
        CHECK(renderer->bounds(scene, second, 1.25f, bounds) == Result::Success);
    } else { delete source; delete first; delete second; }
    delete scene;
}

static void contouredPaths(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto move = [](float x, float y) { return PathCommand::move({x, y, 0.0f}); };
    auto line = [](float x, float y) { return PathCommand::line({x, y, 0.0f}); };

    // Each move starts a contour, and the contours close all or none.
    PathCommand ring[] = {move(-2, -2), line(2, -2), line(2, 2), line(-2, 2), PathCommand::close(),
                          move(-1, -1), line(1, -1), line(1, 1), line(-1, 1), PathCommand::close()};
    PathCommand strokes[] = {move(-2, -2), line(2, -2), move(-2, 2), line(2, 2)};
    PathCommand mixed[] = {move(0, 0), line(1, 0), line(1, 1), PathCommand::close(), move(2, 0), line(3, 0)};
    PathCommand after[] = {move(0, 0), line(1, 0), line(1, 1), PathCommand::close(), line(2, 2)};
    PathCommand lonely[] = {move(0, 0), move(1, 0), line(2, 0)};
    CHECK(Path::gen(mixed, 6u) == nullptr);
    CHECK(Path::gen(after, 5u) == nullptr);
    CHECK(Path::gen(lonely, 3u) == nullptr);
    auto pair = Path::gen(strokes, 4u);
    CHECK(pair && pair->contours() == 2u && !pair->closed() && pair->contourEnd(0) == 2u
          && pair->contourEnd(1) == 4u);
    delete pair;

    // The inner square runs the same way as the outer one and still cuts a hole.
    auto scene = Scene::gen(config);
    auto path = Path::gen(ring, 10u);
    auto circle = Circle::gen({}, 0.5f);
    CHECK(scene && path && circle);
    if (scene && path && circle) {
        CHECK(path->contours() == 2u && path->count() == 8u && path->closed());
        CHECK(path->contourEnd(0) == 4u && path->contourEnd(1) == 8u && path->contourEnd(2) == 8u);
        path->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        circle->opacity = 0.0f;
        CHECK(scene->add(path) == Result::Success && scene->add(circle) == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.0f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {1.5f, 0.0f, 0.0f}), 1.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {2.5f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(static_cast<float>(_inked(renderer, scene, 0.0f)), 10800.0f, 120.0f));
        BBox box;
        CHECK(renderer->bounds(scene, path, 0.0f, box) == Result::Success);
        CHECK(near(box.width, 120.0f, 0.5f) && near(box.height, 120.0f, 0.5f));
        // The layout report records each contour as a path of its own.
        LayoutReport report;
        CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
        CHECK(report.pathCount() == 2u);
        if (report.pathCount() == 2u) {
            CHECK(report.pathAt(0)->count == 4u && report.pathAt(1)->count == 4u);
            CHECK(report.pathAt(0)->closed && report.pathAt(1)->closed);
            CHECK(near(report.pathAt(0)->paintBounds.width, 120.0f, 0.5f));
            CHECK(near(report.pathAt(1)->paintBounds.width, 60.0f, 0.5f));
        }
        // Travel still needs one continuous outline.
        CHECK(scene->play(Animation::moveAlong(circle, path), 1.0f) == Result::InvalidArguments);
    } else {
        delete path;
        delete circle;
    }
    delete scene;

    // While it draws, every contour shows the same share of its own outline.
    scene = Scene::gen(config);
    path = Path::gen(ring, 10u);
    CHECK(scene && path);
    if (scene && path) {
        path->stroke({0, 0, 0, 255}).fill({0, 0, 0, 0});
        path->style.width = 6.0f;
        CHECK(scene->add(path) == Result::Success);
        CHECK(scene->play(Animation::create(path), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, -2.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, -1.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, 2.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.5f, {0.0f, 1.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.0f, 1.0f, 0.0f}), 1.0f, 0.05f));
    } else {
        delete path;
    }
    delete scene;
}

// Scalar contours join cell segments into loops and open pieces, refined against the field.
static void contourPlots(SwRenderer* renderer)
{
    auto circle = [](double x, double y, double& value, void* data) noexcept {
        if (data) ++*static_cast<uint32_t*>(data);
        value = x * x + y * y;
        return true;
    };
    Range range{-1.5f, 1.5f, 0.125f};
    double values[25u * 25u];
    for (auto y = 0u; y < 25u; y++) {
        for (auto x = 0u; x < 25u; x++) {
            auto px = -1.5 + x * 0.125, py = -1.5 + y * 0.125;
            values[y * 25u + x] = px * px + py * py;
        }
    }
    Plot* plot = nullptr;
    uint32_t calls = 0;
    CHECK(Plot::contour(range, range, values, 625u, 1.0, plot, circle, &calls) == Result::Success);
    CHECK(plot && plot->contours() == 1u && plot->count() > 40u && calls > 100u);
    if (plot && plot->count()) {
        auto& first = plot->points()[0];
        auto& last = plot->points()[plot->count() - 1u];
        CHECK(first.x == last.x && first.y == last.y);
        for (auto i = 0u; i < plot->count(); i++) {
            auto& p = plot->points()[i];
            CHECK(std::fabs(static_cast<double>(p.x) * p.x + static_cast<double>(p.y) * p.y - 1.0) < 1.0e-6);
        }
        auto scene = Scene::gen();
        CHECK(scene && scene->add(plot) == Result::Success);
        if (scene) {
            CHECK(scene->play(Animation::create(plot), 0.5f) == Result::Success);
            CHECK(scene->play(Animation::indicate(plot), 0.5f) == Result::Success);
            CHECK(scene->emphasize(Emphasis::passingFlash(plot), 0.5f) == Result::Success);
            auto sampled = calls;
            LayoutReport a, b;
            CHECK(renderer->layout(scene, 0.5f, a) == Result::Success);
            CHECK(renderer->layout(scene, 1.25f, b) == Result::Success);
            CHECK(renderer->layout(scene, 0.5f, b) == Result::Success);
            CHECK(calls == sampled && a.pathCount() == b.pathCount() && a.pathCount() == 1u);
            if (a.pathCount() == 1u && b.pathCount() == 1u) {
                CHECK(a.pathAt(0)->count == b.pathAt(0)->count);
                CHECK(a.pathAt(0)->points[0].x == b.pathAt(0)->points[0].x);
            }
        } else delete plot;
        delete scene;
    } else delete plot;
    plot = nullptr;
    // Two wells have two loops in one plot, with no line joining them.
    auto wells = [](double x, double y, double& value, void*) noexcept {
        value = std::fmin((x - 1.0) * (x - 1.0), (x + 1.0) * (x + 1.0)) + y * y;
        return true;
    };
    Range wide{-2.0f, 2.0f, 0.125f};
    double pairs[33u * 25u];
    for (auto y = 0u; y < 25u; y++) {
        for (auto x = 0u; x < 33u; x++) wells(-2.0 + x * 0.125, -1.5 + y * 0.125, pairs[y * 33u + x], nullptr);
    }
    CHECK(Plot::contour(wide, range, pairs, 825u, 0.16, plot, wells) == Result::Success);
    CHECK(plot && plot->contours() == 2u);
    if (plot) {
        auto begin = 0u;
        for (auto piece = 0u; piece < plot->contours(); piece++) {
            auto end = plot->contourEnd(piece);
            CHECK(plot->points()[begin].x == plot->points()[end - 1u].x
                  && plot->points()[begin].y == plot->points()[end - 1u].y);
            for (auto i = begin; i < end; i++) {
                double value;
                wells(plot->points()[i].x, plot->points()[i].y, value, nullptr);
                CHECK(std::fabs(value - 0.16) < 1.0e-6);
            }
            begin = end;
        }
    }
    delete plot;
    Range cell{0.0f, 1.0f, 1.0f};
    // Both signs of the saddle determinant and its fixed zero tie pairing.
    for (auto pass = 0u; pass < 3u; pass++) {
        double saddle[] = {pass == 0u ? 2.0 : 1.0, -1.0, -1.0, pass == 1u ? 0.5 : 1.0};
        plot = nullptr;
        CHECK(Plot::contour(cell, cell, saddle, 4u, 0.0, plot) == Result::Success);
        CHECK(plot && plot->contours() == 2u && plot->count() == 4u);
        if (plot && plot->count() == 4u) {
            CHECK(plot->points()[0].y == 0.0f);
            CHECK(pass == 1u ? plot->points()[1].x == 0.0f : plot->points()[1].x == 1.0f);
        }
        delete plot;
    }
    plot = nullptr;
    CHECK(Plot::contour(range, range, values, 625u, 10.0, plot) == Result::Success);
    CHECK(plot && !plot->count() && !plot->contours());
    delete plot;
    plot = nullptr;
    CHECK(Plot::contour(range, range, values, 625u, 1.0, plot, circle, nullptr, 8u) == Result::NonSupport && !plot);
    CHECK(Plot::contour(cell, cell, values, 3u, 0.0, plot) == Result::InvalidArguments && !plot);
    CHECK(Plot::contour({0, 0, 1}, cell, values, 4u, 0.0, plot) == Result::InvalidArguments);
    CHECK(Plot::contour({0, 1000, 1}, {0, 1000, 1}, values, 4u, 0.0, plot) == Result::NonSupport);
    double invalid[] = {0, 1, NAN, 1};
    CHECK(Plot::contour(cell, cell, invalid, 4u, 0.5, plot) == Result::InvalidArguments);
    auto fails = [](double, double, double&, void*) noexcept { return false; };
    CHECK(Plot::contour(range, range, values, 625u, 1.0, plot, fails) == Result::InvalidArguments && !plot);
}

// Labels remain centered on sampled curves, with retained text and real stroke gaps.
static void contourLabels(SwRenderer* renderer)
{
    CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
    Range range{-2.0f, 2.0f, 0.25f};
    double values[289];
    for (auto y = 0u; y < 17u; y++) for (auto x = 0u; x < 17u; x++) values[y * 17u + x] = -4.0 + (x + y) * 0.25;
    Plot* curve = nullptr;
    CHECK(Plot::contour(range, range, values, 289u, 0.0, curve) == Result::Success);
    if (!curve) return;
    curve->tag("labelled").stroke({120, 160, 220, 255}, 2.0f);
    ContourLabelOptions options;
    options.size = 0.2f;
    auto invalid = options;
    invalid.format = "%s";
    CHECK(curve->labelContours(invalid) == Result::InvalidArguments && !curve->childCount());
    invalid = options; invalid.spacing = -1.0f;
    CHECK(curve->labelContours(invalid) == Result::InvalidArguments && !curve->childCount());
    invalid = options; invalid.size = 0.0f;
    CHECK(curve->labelContours(invalid) == Result::InvalidArguments && !curve->childCount());
    CHECK(curve->labelContours(options) == Result::Success && curve->childCount() == 1u);
    auto label = curve->childCount() ? static_cast<Text*>(curve->childAt(0)) : nullptr;
    CHECK(label && label->type() == Type::Text && label->orientation == TextOrientation::Plane);
    CHECK(label && std::strcmp(label->tag(), "labelled/label/1") == 0 && std::strcmp(label->text(), "0.0") == 0);
    Config config;
    config.width = 320; config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto scene = Scene::gen(config);
    CHECK(scene && scene->add(curve) == Result::Success);
    if (!scene) { delete curve; return; }
    auto count = curve->count();
    auto moved = new Vec3[count];
    for (auto i = 0u; i < count; i++) moved[i] = curve->points()[i] + Vec3{0.5f, 0.5f, 0.0f};
    auto grid = curve->contourGrid();
    grid.level = 1.0;
    CHECK(scene->play(AnimationTarget::from(curve).geometry(moved, count, nullptr, 1u, nullptr, &grid),
                      1.0f, Easing::Linear) == Result::Success);
    delete[] moved;
    CHECK(curve->labelContours(options) == Result::InsufficientCondition);
    LayoutReport initial, again;
    for (auto time : {0.5f, 1.0f, 0.0f, 0.5f}) {
        LayoutReport report;
        CHECK(renderer->layout(scene, time, report) == Result::Success);
        const LayoutObject* text = nullptr;
        auto textIndex = 0u, curveIndex = 0u;
        for (auto i = 0u; i < report.objectCount(); i++) {
            if (report.objectAt(i)->object == label) { text = report.objectAt(i); textIndex = i; }
            if (report.objectAt(i)->object == curve) curveIndex = i;
        }
        CHECK(text && text->visible);
        if (!text) continue;
        auto center = Vec2{text->paintBounds.x + text->paintBounds.width * 0.5f,
                           text->paintBounds.y + text->paintBounds.height * 0.5f};
        // The diagonal x+y=time projects to x-y=70+30*time in this camera.
        CHECK(std::fabs(center.x - center.y - 70.0f - 30.0f * time) / std::sqrt(2.0f) < 0.6f);
        CHECK(std::fabs(text->paintBounds.width - text->paintBounds.height) < 1.5f);
        auto paths = 0u;
        auto closest = 1000.0f;
        for (auto i = 0u; i < report.pathCount(); i++) {
            auto path = report.pathAt(i);
            if (report.visualAt(path->visual)->object != curveIndex) continue;
            paths++;
            for (auto j = 1u; j < path->count; j++) {
                auto a = path->points[j - 1u], b = path->points[j];
                auto dx = b.x - a.x, dy = b.y - a.y;
                auto den = dx * dx + dy * dy;
                auto t = den > 0.0f ? std::fmax(0.0f, std::fmin(1.0f, ((center.x-a.x)*dx + (center.y-a.y)*dy) / den)) : 0.0f;
                closest = std::fmin(closest, std::hypot(center.x-a.x-t*dx, center.y-a.y-t*dy));
            }
        }
        CHECK(paths == 2u && closest > 3.0f);
        char value[32];
        CHECK(scene->text(label, time, value, sizeof(value)) == 3u);
        CHECK(std::strcmp(value, time == 0.0f ? "0.0" : time == 1.0f ? "1.0" : "0.5") == 0);
        CHECK(textIndex != curveIndex);
    }
    CHECK(renderer->layout(scene, 0.5f, initial) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, again) == Result::Success);
    CHECK(renderer->layout(scene, 0.5f, again) == Result::Success);
    CHECK(initial.pathCount() == again.pathCount());
    for (auto i = 0u; i < initial.pathCount() && i < again.pathCount(); i++) {
        CHECK(initial.pathAt(i)->count == again.pathAt(i)->count);
        CHECK(initial.pathAt(i)->points[0].x == again.pathAt(i)->points[0].x);
    }
    Plot* directed = nullptr;
    CHECK(Plot::contour(range, range, values, 289u, 0.0, directed) == Result::Success);
    auto other = Scene::gen(config);
    if (directed && other) {
        directed->tag("directed").stroke({120, 160, 220, 255}, 2.0f);
        options.upright = false;
        CHECK(directed->labelContours(options) == Result::Success);
        CHECK(other->add(directed) == Result::Success);
        Surface upright, tracing;
        CHECK(renderer->render(scene, 0.0f, upright) == Result::Success);
        CHECK(renderer->render(other, 0.0f, tracing) == Result::Success);
        CHECK(checksum(upright) != checksum(tracing));
    } else delete directed;
    delete other;
    delete scene;

    // A closed curve gains repeat labels; collision boxes keep their text apart.
    for (auto y = 0u; y < 17u; y++) for (auto x = 0u; x < 17u; x++) {
        auto px = -2.0 + x * 0.25, py = -2.0 + y * 0.25;
        values[y * 17u + x] = px * px + py * py;
    }
    curve = nullptr;
    CHECK(Plot::contour(range, range, values, 289u, 1.0, curve) == Result::Success);
    if (!curve) return;
    curve->tag("loop"); options.spacing = 2.0f;
    CHECK(curve->labelContours(options) == Result::Success && curve->childCount() == 3u);
    scene = Scene::gen(config);
    CHECK(scene && scene->add(curve) == Result::Success);
    if (scene) {
        LayoutReport report;
        CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
        auto visible = 0u;
        for (auto i = 0u; i < report.objectCount(); i++) {
            auto object = report.objectAt(i);
            if (object->object->type() == Type::Text && object->visible) visible++;
        }
        CHECK(visible == 3u);
        for (auto i = 0u; i < report.collisionCount(); i++) {
            auto collision = report.collisionAt(i);
            CHECK(!(report.objectAt(collision->first)->object->type() == Type::Text
                    && report.objectAt(collision->second)->object->type() == Type::Text && collision->overlapping));
        }
    } else delete curve;
    delete scene;
}

static void contourStates(SwRenderer* renderer)
{
    struct Field { double a = 1.0; bool wells = false; uint32_t calls = 0; } field;
    auto fn = [](double x, double y, double& value, void* data) noexcept {
        auto f = static_cast<Field*>(data);
        f->calls++;
        value = f->wells ? (x*x - 1.0)*(x*x - 1.0) + y*y : x*x/(f->a*f->a) + y*y - 1.0;
        return true;
    };
    Range x{-2.0f, 2.0f, 0.125f}, y{-1.5f, 1.5f, 0.125f};
    auto trace = [&](double level) {
        double values[825];
        for (auto row = 0u; row < 25u; row++) {
            for (auto column = 0u; column < 33u; column++) {
                fn(-2.0 + column*0.125, -1.5 + row*0.125, values[row*33u + column], &field);
            }
        }
        Plot* plot = nullptr;
        CHECK(Plot::contour(x, y, values, 825u, level, plot, fn, &field) == Result::Success);
        return plot;
    };
    auto scene = Scene::gen();
    field.a = 0.75;
    auto ellipse = trace(0.0);
    CHECK(scene && ellipse && scene->add(ellipse) == Result::Success);
    if (scene && ellipse) {
        for (auto i = 1u; i <= 4u; i++) {
            field.a = 0.75 + i*0.1875;
            auto next = trace(0.0);
            CHECK(next && scene->play(AnimationTarget::from(ellipse).geometry(next->points(), next->count(),
                  nullptr, next->contours()), 0.25f, Easing::Linear) == Result::Success);
            delete next;
        }
        auto calls = field.calls;
        // Copies expose the actual sampled geometry, without projection rounding.
        uint32_t order[] = {4u, 0u, 2u, 1u, 3u};
        for (auto i : order) {
            CHECK(scene->seek(i*0.25f) == Result::Success);
            Object* copy = nullptr;
            CHECK(scene->clone(ellipse, copy) == Result::Success);
            auto sampled = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
            CHECK(sampled && sampled->contours() == 1u && sampled->count() > 20u);
            auto a = 0.75 + i*0.1875;
            for (auto j = 0u; sampled && j < sampled->count(); j++) {
                auto& p = sampled->points()[j];
                CHECK(std::fabs(static_cast<double>(p.x)*p.x/(a*a) + static_cast<double>(p.y)*p.y - 1.0) < 1.0e-6);
            }
            LayoutReport report;
            CHECK(renderer->layout(scene, i*0.25f, report) == Result::Success);
        }
        CHECK(field.calls == calls);
    } else delete ellipse;
    delete scene;

    scene = Scene::gen();
    field.wells = true;
    auto curve = trace(0.5);
    CHECK(scene && curve && curve->contours() == 2u && scene->add(curve) == Result::Success);
    if (scene && curve) {
        double levels[] = {1.1, -0.2, 0.5};
        for (auto level : levels) {
            auto next = trace(level);
            uint32_t ends[2];
            for (auto i = 0u; next && i < next->contours(); i++) ends[i] = next->contourEnd(i);
            CHECK(next && scene->play(AnimationTarget::from(curve).geometry(next->points(), next->count(),
                  next->contours() > 1u ? ends : nullptr, next->contours()), 0.25f, Easing::Linear) == Result::Success);
            delete next;
        }
        auto calls = field.calls;
        float times[] = {0.7f, 0.1f, 0.2f, 0.35f, 0.45f, 0.6f, 0.75f};
        uint32_t pieces[] = {2u, 2u, 1u, 1u, 0u, 0u, 2u};
        for (auto i = 0u; i < 7u; i++) {
            LayoutReport report;
            CHECK(renderer->layout(scene, times[i], report) == Result::Success);
            CHECK(report.pathCount() == pieces[i]);
        }
        CHECK(field.calls == calls);
    } else delete curve;
    delete scene;
}

// A plot in pieces draws nothing between them, ends them flat when it asks to, and draws on
// through one piece after another.
static void plotEndpoints(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    const Vec3 points[] = {{-3, -1, 0}, {-1, -1, 0}, {1, 1, 0}, {3, 1, 0}};
    const uint32_t ends[] = {2u, 4u};
    CHECK(Plot::gen(points, 1u) == nullptr);
    CHECK(Plot::gen(nullptr, 0u) == nullptr);
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    for (auto pass = 0u; pass < 3u; pass++) {
        auto scene = Scene::gen(config);
        auto plot = Plot::gen(points, 4u, ends, 2u);
        CHECK(scene && plot);
        if (!scene || !plot) { delete scene; delete plot; continue; }
        plot->progress = 0.0f;
        plot->stroke({0, 0, 0, 255}, 0.0f);
        plot->endpointRadius = pass ? 4.0f : 0.0f;
        CHECK(scene->add(plot) == Result::Success);
        auto shown = [&](float time, Vec3 point) { return _shownOpacity(renderer, scene, time, point); };
        BBox bounds;
        if (!pass) {
            CHECK(near(shown(0, points[0]), 0, 0.02f));
            CHECK(renderer->bounds(scene, plot, 0, bounds) == Result::InsufficientCondition);
        } else {
            // At manually set progress 0 only the active vertex has paint and bounds.
            CHECK(shown(0, points[0]) > 0.9f && near(shown(0, points[1]), 0, 0.02f));
            CHECK(renderer->bounds(scene, plot, 0, bounds) == Result::Success);
            CHECK(near(bounds.width, 8, 0.5f) && near(bounds.height, 8, 0.5f));
            plot->endpointRadius = 8;
            CHECK(renderer->bounds(scene, plot, 0, bounds) == Result::InvalidArguments);
            plot->endpointRadius = 4;
            Object* copy = nullptr;
            CHECK(scene->clone(plot, copy) == Result::Success);
            CHECK(copy && static_cast<Plot*>(copy)->endpointRadius == 4.0f);
            CHECK(scene->remove(copy) == Result::Success);
            if (pass == 1u) {
                // Segment progress stops on the preceding piece at an exact gap boundary.
                CHECK(scene->play(AnimationTarget::from(plot).progress(1), 1, linear) == Result::Success);
                CHECK(shown(0.25f, {-2, -1, 0}) > 0.9f);
                CHECK(shown(0.5f, points[1]) > 0.9f && near(shown(0.5f, points[2]), 0, 0.02f));
                CHECK(shown(0.75f, {2, 1, 0}) > 0.9f && shown(1, points[3]) > 0.9f);
                CHECK(shown(0.25f, {-2, -1, 0}) > 0.9f);
                CHECK(scene->play(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 0u), 1, linear) == Result::Success);
                CHECK(renderer->bounds(scene, plot, 2, bounds) == Result::InsufficientCondition);
                Object* empty = nullptr;
                CHECK(scene->clone(plot, empty) == Result::Success);
                CHECK(empty && static_cast<Plot*>(empty)->count() == 0u
                      && static_cast<Plot*>(empty)->endpointRadius == 4.0f);
            } else {
                CHECK(scene->seek(1) == Result::Success);
                CHECK(scene->play(Animation::create(plot, DrawDirection::Reverse), 1, linear) == Result::Success);
                CHECK(near(shown(0, points[0]), 0, 0.02f) && near(shown(1, points[3]), 0, 0.02f));
                CHECK(shown(1.25f, {2, 1, 0}) > 0.9f && shown(2, points[0]) > 0.9f);
                CHECK(scene->play(Animation::uncreate(plot, DrawDirection::Reverse), 1, linear) == Result::Success);
                CHECK(near(shown(3, points[3]), 0, 0.02f));
                CHECK(scene->update(AnimationTarget::from(plot).progress(0)) == Result::Success);
                CHECK(shown(3, points[0]) > 0.9f);
            }
        }
        delete scene;
    }
    // Invalid static options fail attachment without transferring ownership.
    for (auto radius : {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto scene = Scene::gen(config);
        auto plot = Plot::gen(points, 2u);
        plot->endpointRadius = radius;
        CHECK(scene->add(plot) == Result::InvalidArguments);
        plot->endpointRadius = 4;
        CHECK(scene->add(plot) == Result::Success);
        delete scene;
    }
    auto scene = Scene::gen(config);
    auto group = Group::gen();
    auto plot = Plot::gen(points, 2u);
    plot->endpointRadius = 4;
    plot->progress = 0;
    plot->stroke({0, 0, 0, 255}, 0);
    group->opacity = 0.5f;
    group->layer = 7;
    CHECK(group->add(plot) == Result::Success && scene->add(group) == Result::Success);
    CHECK(near(_shownOpacity(renderer, scene, 0, points[0]), 0.5f, 0.02f));
    LayoutReport report;
    CHECK(renderer->layout(scene, 0, report) == Result::Success);
    CHECK(report.objectCount() == 2u && report.objectAt(1)->visible
          && report.objectAt(1)->layer == 0);
    CHECK(scene->play(AnimationTarget::from(group).layer(9), 1, linear) == Result::Success);
    CHECK(renderer->layout(scene, 1, report) == Result::Success);
    CHECK(report.objectAt(1)->visible && report.objectAt(1)->layer == 2);
    CHECK(scene->play(AnimationTarget::from(group).opacity(0), 1, linear) == Result::Success);
    CHECK(near(_shownOpacity(renderer, scene, 2, points[0]), 0, 0.02f));
    BBox bounds;
    CHECK(renderer->bounds(scene, plot, 2, bounds) == Result::InsufficientCondition);
    delete scene;
}

static void plotEndpointColor(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 100;
    config.camera.orthoHeight = 5;
    config.background = {255, 255, 255, 255};
    const Vec3 points[] = {{-2, 0, 0}, {2, 0, 0}};
    auto scene = Scene::gen(config);
    auto plot = Plot::gen(points, 2u);
    CHECK(scene && plot);
    if (!scene || !plot) { delete scene; delete plot; return; }
    plot->stroke({255, 0, 0, 255}, 4);
    plot->endpointRadius = 6;
    Gradient gradient;
    gradient.type = GradientType::Linear;
    gradient.stopCount = 2;
    gradient.stops[0] = {0, {0, 0, 255, 255}};
    gradient.stops[1] = {1, {0, 255, 0, 255}};
    CHECK(plot->gradient(gradient) == Result::Success);
    CHECK(scene->add(plot) == Result::Success);
    const Object* candidates[] = {plot};
    PixelSample sample;
    CHECK(renderer->sample(scene, 0, {120.5f, 50.5f}, candidates, 1u, sample) == Result::Success);
    CHECK(sample.color.r == 255 && sample.color.g == 0 && sample.color.b == 0 && sample.color.a == 255);
    // The marker's solid fill leaves the Plot's blue/green gradient stroke unchanged.
    CHECK(renderer->sample(scene, 0, {80.5f, 50.5f}, candidates, 1u, sample) == Result::Success);
    CHECK(sample.color.r == 0 && sample.color.g > 0 && sample.color.b > 0 && sample.color.a == 255);
    CHECK(plot->style.gradient.enabled());
    CHECK(scene->wait(1) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).progress(0), 1, Easing::Linear) == Result::Success);
    CHECK(renderer->sample(scene, 2, {40.5f, 50.5f}, candidates, 1u, sample) == Result::Success);
    CHECK(sample.color.r == 255 && sample.color.g == 0 && sample.color.b == 0 && sample.color.a == 255);
    delete scene;
}

static void plotEndpointProgress(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    const Vec3 points[] = {{-2, -1, 0}, {2, -1, 0}};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto make = [&](float progress) {
        auto plot = Plot::gen(points, 2u);
        plot->endpointRadius = 4;
        plot->progress = progress;
        plot->stroke({0, 0, 0, 255}, 0);
        return plot;
    };
    // Explicit animated progress retains the zero-progress vertex before and after the clip.
    for (auto pass = 0u; pass < 3u; pass++) {
        auto scene = Scene::gen(config);
        auto plot = make(pass == 0u ? 1 : 0);
        auto target = pass == 1u ? 1 : 0;
        CHECK(scene->add(plot) == Result::Success && scene->wait(1) == Result::Success);
        CHECK(scene->play(AnimationTarget::from(plot).progress(target), 1, linear) == Result::Success);
        CHECK(scene->wait(1) == Result::Success);
        CHECK(_shownOpacity(renderer, scene, 0, points[pass == 0u ? 1 : 0]) > 0.9f);
        CHECK(_shownOpacity(renderer, scene, 1, points[pass == 0u ? 1 : 0]) > 0.9f);
        CHECK(_shownOpacity(renderer, scene, 2, points[target ? 1 : 0]) > 0.9f);
        CHECK(_shownOpacity(renderer, scene, 3, points[target ? 1 : 0]) > 0.9f);
        delete scene;
    }
    auto scene = Scene::gen(config);
    auto plot = make(1);
    CHECK(scene->add(plot) == Result::Success);
    CHECK(scene->play(Animation::uncreate(plot), 1, linear) == Result::Success);
    CHECK(scene->wait(0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).progress(0), 1, linear) == Result::Success);
    CHECK(scene->wait(1) == Result::Success);
    CHECK(scene->play(Animation::create(plot), 1, linear) == Result::Success);
    CHECK(near(_shownOpacity(renderer, scene, 1.25f, points[0]), 0, 0.02f));
    CHECK(_shownOpacity(renderer, scene, 1.5f, points[0]) > 0.9f);
    CHECK(_shownOpacity(renderer, scene, 2.5f, points[0]) > 0.9f);
    CHECK(near(_shownOpacity(renderer, scene, 3.5f, points[0]), 0, 0.02f));
    CHECK(_shownOpacity(renderer, scene, 4.5f, points[1]) > 0.9f);
    delete scene;

    // A first create retroactively hides even explicit progress clips before its start.
    scene = Scene::gen(config);
    plot = make(0);
    CHECK(scene->add(plot) == Result::Success && scene->wait(1) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).progress(0), 1, linear) == Result::Success);
    CHECK(scene->wait(1) == Result::Success);
    CHECK(scene->play(Animation::create(plot), 1, linear) == Result::Success);
    for (auto time : {0.0f, 1.0f, 2.0f, 3.0f})
        CHECK(near(_shownOpacity(renderer, scene, time, points[0]), 0, 0.02f));
    CHECK(_shownOpacity(renderer, scene, 3.5f, {0, -1, 0}) > 0.9f);
    CHECK(_shownOpacity(renderer, scene, 4, points[1]) > 0.9f);
    delete scene;

    // Group progress reaches its child; ordinary Space progress does not.
    for (auto pass = 0u; pass < 2u; pass++) {
        scene = Scene::gen(config);
        Object* parent = pass ? static_cast<Object*>(Space::gen()) : static_cast<Object*>(Group::gen());
        parent->stroke({0, 0, 0, 0}, 0);
        plot = make(0);
        CHECK(parent->add(plot) == Result::Success && scene->add(parent) == Result::Success);
        CHECK(scene->wait(1) == Result::Success);
        CHECK(scene->play(Animation::create(parent), 1, linear) == Result::Success);
        auto shown = [&](float time) { return _shownOpacity(renderer, scene, time, points[0]); };
        CHECK(pass ? shown(0) > 0.9f : near(shown(0), 0, 0.02f));
        CHECK(pass ? shown(1) > 0.9f : near(shown(1), 0, 0.02f));
        CHECK(shown(1.5f) > 0.9f && shown(2) > 0.9f);
        CHECK(scene->play(Animation::uncreate(parent), 1, linear) == Result::Success);
        CHECK(shown(2.5f) > 0.9f);
        CHECK(pass ? shown(3) > 0.9f : near(shown(3), 0, 0.02f));
        CHECK(scene->wait(1) == Result::Success);
        CHECK(scene->play(Animation::create(parent), 1, linear) == Result::Success);
        CHECK(pass ? shown(4) > 0.9f : near(shown(4), 0, 0.02f));
        CHECK(shown(4.5f) > 0.9f && shown(5) > 0.9f);
        delete scene;
    }
}

static void plotPieces(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    // Two runs of three segments, a unit below the axis and a unit above it.
    Vec3 points[] = {{-4.0f, -1.0f, 0.0f}, {-3.0f, -1.0f, 0.0f}, {-2.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f},
                     {1.0f, 1.0f, 0.0f},   {2.0f, 1.0f, 0.0f},   {3.0f, 1.0f, 0.0f},   {4.0f, 1.0f, 0.0f}};
    uint32_t ends[] = {4u, 8u};
    uint32_t lonely[] = {1u, 8u};
    uint32_t early[] = {4u, 7u};
    uint32_t falling[] = {6u, 4u};
    CHECK(Plot::gen(points, 8u, lonely, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, early, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, falling, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, nullptr, 2u) == nullptr);
    CHECK(Plot::gen(points, 8u, ends, 0u) == nullptr);
    auto whole = Plot::gen(points, 8u, nullptr, 1u);
    CHECK(whole && whole->contours() == 1u && whole->contourEnd(0) == 8u && !whole->flat);
    delete whole;
    auto make = [&](bool flat, float width) {
        auto plot = Plot::gen(points, 8u, ends, 2u);
        if (plot) {
            plot->stroke({0, 0, 0, 255}, width);
            plot->flat = flat;
        }
        return plot;
    };
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);

    auto scene = Scene::gen(config);
    auto plot = make(true, 4.0f);
    auto circle = Circle::gen({}, 0.5f);
    CHECK(scene && plot && circle);
    if (scene && plot && circle) {
        circle->opacity = 0.0f;
        CHECK(scene->add(plot) == Result::Success && scene->add(circle) == Result::Success);
        CHECK(plot->contours() == 2u && plot->contourEnd(0) == 4u && plot->contourEnd(1) == 8u);
        // Nothing joins (-1, -1) to (1, 1).
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {-2.5f, -1.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {2.5f, 1.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {-0.5f, -0.5f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.0f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.5f, 0.5f, 0.0f}), 0.0f, 0.02f));
        // Travel and a flash still need one continuous outline.
        CHECK(scene->play(Animation::moveAlong(circle, plot), 1.0f) == Result::InvalidArguments);
        CHECK(scene->emphasize(Emphasis::passingFlash(plot), 1.0f) == Result::InvalidArguments);
        // A copy keeps the pieces and the flat ends.
        Object* copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        auto twin = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
        CHECK(twin && twin->flat && twin->count() == 8u && twin->contours() == 2u && twin->contourEnd(0) == 4u);
    } else {
        delete plot;
        delete circle;
    }
    delete scene;

    // A flat end stops at its last point; a round one reaches half the width past it. The layout
    // report records each piece as a path of its own.
    for (auto pass = 0u; pass < 2u; pass++) {
        auto flat = pass == 0u;
        scene = Scene::gen(config);
        plot = make(flat, 12.0f);
        CHECK(scene && plot);
        if (scene && plot) {
            CHECK(scene->add(plot) == Result::Success);
            auto past = _shownOpacity(renderer, scene, 0.0f, {4.1f, 1.0f, 0.0f});
            CHECK(flat ? near(past, 0.0f, 0.02f) : past > 0.9f);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
            CHECK(report.pathCount() == 2u);
            if (report.pathCount() == 2u) {
                CHECK(report.pathAt(0)->count == 4u && !report.pathAt(0)->closed);
                CHECK(report.pathAt(1)->count == 4u && near(report.pathAt(1)->points[0].x, 190.0f, 0.5f));
            }
        } else {
            delete plot;
        }
        delete scene;
    }

    // While it draws, progress runs through the six segments in turn and the gap takes none
    // of it; reversed, it starts at the end of the last piece.
    for (auto pass = 0u; pass < 2u; pass++) {
        auto reverse = pass == 1u;
        scene = Scene::gen(config);
        plot = make(true, 6.0f);
        CHECK(scene && plot);
        if (scene && plot) {
            CHECK(scene->add(plot) == Result::Success);
            auto direction = reverse ? DrawDirection::Reverse : DrawDirection::Forward;
            CHECK(scene->play(Animation::create(plot, direction), 1.0f, linear) == Result::Success);
            auto sign = reverse ? -1.0f : 1.0f;
            auto shown = [&](float time, float x, float y) {
                return _shownOpacity(renderer, scene, time, {sign * x, sign * y, 0.0f});
            };
            CHECK(near(shown(0.25f, -3.5f, -1.0f), 1.0f, 0.05f));
            CHECK(near(shown(0.25f, -2.0f, -1.0f), 0.0f, 0.02f));
            CHECK(near(shown(0.25f, 1.5f, 1.0f), 0.0f, 0.02f));
            CHECK(near(shown(0.5f, -1.2f, -1.0f), 1.0f, 0.05f));
            CHECK(near(shown(0.5f, 1.2f, 1.0f), 0.0f, 0.02f));
            CHECK(near(shown(0.75f, 2.0f, 1.0f), 1.0f, 0.05f));
            CHECK(near(shown(0.75f, 3.0f, 1.0f), 0.0f, 0.02f));
            CHECK(near(shown(1.0f, 3.5f, 1.0f), 1.0f, 0.05f));
        } else {
            delete plot;
        }
        delete scene;
    }

    // A warp carries each piece on its own, and still nothing joins them.
    scene = Scene::gen(config);
    plot = make(true, 4.0f);
    CHECK(scene && plot);
    if (scene && plot) {
        CHECK(scene->add(plot) == Result::Success);
        Vec3 up = {0.0f, 1.0f, 0.0f};
        Object* targets[] = {plot};
        CHECK(scene->warp(targets, 1u, _moved, &up, 1.0f) == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {-2.5f, 0.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {2.5f, 2.0f, 0.0f}), 1.0f, 0.05f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.0f, 1.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 1.0f, {0.5f, 1.5f, 0.0f}), 0.0f, 0.02f));
    } else {
        delete plot;
    }
    delete scene;
}

// The area a path encloses, its counterclockwise contours adding and its clockwise ones taking
// away; only contour when one is named.
static double _pathArea(const Path* path, uint32_t contour = UINT32_MAX)
{
    auto area = 0.0;
    auto start = 0u;
    for (auto i = 0u; i < path->contours(); i++) {
        auto end = path->contourEnd(i);
        for (auto j = start; (contour == UINT32_MAX || contour == i) && j < end; j++) {
            auto& a = path->points()[j];
            auto& b = path->points()[j + 1u < end ? j + 1u : start];
            area += 0.5 * (static_cast<double>(a.x) * b.y - static_cast<double>(a.y) * b.x);
        }
        start = end;
    }
    return area;
}

static Result _combined(Object* a, Object* b, PathOperation operation, Path*& output,
                        const Object* parent = nullptr)
{
    const Object* shapes[] = {a, b};
    return Path::combine(shapes, 2u, operation, parent, output);
}

static void combinedShapes(SwRenderer* renderer)
{
    // Two unit disks a unit apart: whatever the operation, the areas add up as sets do.
    auto left = Circle::gen({-0.5f, 0.0f, 0.0f}, 1.0f);
    auto right = Circle::gen({0.5f, 0.0f, 0.0f}, 1.0f);
    Path* lens = nullptr;
    Path* both = nullptr;
    Path* crescent = nullptr;
    Path* apart = nullptr;
    CHECK(left && right);
    if (left && right) {
        CHECK(_combined(left, right, PathOperation::Intersection, lens) == Result::Success);
        CHECK(_combined(left, right, PathOperation::Union, both) == Result::Success);
        CHECK(_combined(left, right, PathOperation::Difference, crescent) == Result::Success);
        CHECK(_combined(left, right, PathOperation::Exclusion, apart) == Result::Success);
    }
    CHECK(lens && both && crescent && apart);
    if (lens && both && crescent && apart) {
        // A circle draws as a regular polygon of 64 sides.
        auto disk = 32.0 * std::sin(6.283185307179586 / 64.0);
        auto shared = _pathArea(lens);
        // Two unit circles a unit apart share 2 pi / 3 - sqrt(3) / 2 of area.
        CHECK(near(static_cast<float>(shared), 1.22837f, 0.01f));
        CHECK(lens->closed() && lens->contours() == 1u && both->contours() == 1u && crescent->contours() == 1u);
        CHECK(near(static_cast<float>(_pathArea(both) + shared), static_cast<float>(2.0 * disk), 1.0e-4f));
        CHECK(near(static_cast<float>(_pathArea(crescent) + shared), static_cast<float>(disk), 1.0e-4f));
        CHECK(near(static_cast<float>(_pathArea(apart)), static_cast<float>(_pathArea(both) - shared), 1.0e-4f));
        // A shape joined with itself, all of its edges on top of each other, is itself.
        Path* same = nullptr;
        CHECK(_combined(left, left, PathOperation::Union, same) == Result::Success);
        CHECK(same && same->contours() == 1u && same->count() == 64u);
        if (same) CHECK(near(static_cast<float>(_pathArea(same)), static_cast<float>(disk), 1.0e-4f));
        delete same;
        // The two crescents only touch where the circles cross, so they stay two contours.
        CHECK(apart->contours() == 2u);
        auto east = -10.0f;
        for (auto i = 0u; i < crescent->count(); i++)
            east = std::fmax(east, crescent->points()[i].x);
        CHECK(near(east, 0.0f, 1.0e-3f));
        Bounds box;
        CHECK(lens->bounds(box));
        CHECK(near(box.min.x, -0.5f, 1.0e-3f) && near(box.max.x, 0.5f, 1.0e-3f));
        CHECK(near(box.max.y, 0.866f, 0.01f) && near(box.min.y, -0.866f, 0.01f));
    }
    delete lens;
    delete both;
    delete crescent;
    delete apart;

    // A square inside another leaves a ring: the outer contour runs counterclockwise and the
    // hole clockwise.
    auto outer = Rectangle::gen({}, {4.0f, 4.0f});
    auto inner = Rectangle::gen({}, {2.0f, 2.0f});
    Path* ring = nullptr;
    CHECK(outer && inner);
    if (outer && inner) CHECK(_combined(outer, inner, PathOperation::Difference, ring) == Result::Success);
    CHECK(ring && ring->contours() == 2u && ring->count() == 8u);
    if (ring && ring->contours() == 2u) {
        auto first = _pathArea(ring, 0u);
        auto second = _pathArea(ring, 1u);
        CHECK(near(static_cast<float>(first > 0.0 ? first : second), 16.0f, 1.0e-4f));
        CHECK(near(static_cast<float>(first > 0.0 ? second : first), -4.0f, 1.0e-4f));
    }
    Path* empty = nullptr;
    if (outer && inner) {
        // Nothing is left of a shape taken from itself, nor of a shape inside a hole.
        CHECK(_combined(inner, outer, PathOperation::Difference, empty) == Result::InsufficientCondition);
        CHECK(empty == nullptr);
        CHECK(_combined(outer, outer, PathOperation::Exclusion, empty) == Result::InsufficientCondition);
        if (ring) {
            auto core = Rectangle::gen({}, {1.0f, 1.0f});
            CHECK(core && _combined(ring, core, PathOperation::Intersection, empty) == Result::InsufficientCondition);
            delete core;
        }
    }

    // Squares sharing an edge become one rectangle, the points along its long sides gone.
    auto west = Rectangle::gen({-0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto east = Rectangle::gen({0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto corner = Rectangle::gen({1.5f, 1.0f, 0.0f}, {1.0f, 1.0f});
    auto far = Rectangle::gen({5.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(west && east && corner && far);
    if (west && east && corner && far) {
        Path* joined = nullptr;
        CHECK(_combined(west, east, PathOperation::Union, joined) == Result::Success);
        CHECK(joined && joined->contours() == 1u && joined->count() == 4u);
        if (joined) CHECK(near(static_cast<float>(_pathArea(joined)), 2.0f, 1.0e-4f));
        delete joined;
        joined = nullptr;
        // Squares meeting at a corner stay two contours.
        CHECK(_combined(east, corner, PathOperation::Union, joined) == Result::Success);
        CHECK(joined && joined->contours() == 2u);
        if (joined) CHECK(near(static_cast<float>(_pathArea(joined)), 2.0f, 1.0e-4f));
        delete joined;
        joined = nullptr;
        CHECK(_combined(west, far, PathOperation::Union, joined) == Result::Success);
        CHECK(joined && joined->contours() == 2u);
        delete joined;
        CHECK(_combined(west, far, PathOperation::Intersection, empty) == Result::InsufficientCondition);
    }

    // Edges that run along each other part where either ends, and a corner that lands on an
    // edge parts it there.
    auto wide = Rectangle::gen({1.0f, 0.5f, 0.0f}, {2.0f, 1.0f});
    auto shifted = Rectangle::gen({2.0f, 0.5f, 0.0f}, {2.0f, 1.0f});
    auto unit = Rectangle::gen({0.5f, 0.5f, 0.0f}, {1.0f, 1.0f});
    auto tab = Rectangle::gen({1.5f, 0.5f, 0.0f}, {1.0f, 0.5f});
    CHECK(wide && shifted && unit && tab);
    if (wide && shifted && unit && tab) {
        PathOperation operations[] = {PathOperation::Union, PathOperation::Intersection, PathOperation::Difference,
                                      PathOperation::Exclusion};
        float areas[] = {3.0f, 1.0f, 1.0f, 2.0f};
        uint32_t contours[] = {1u, 1u, 1u, 2u};
        for (auto i = 0u; i < 4u; i++) {
            Path* overlap = nullptr;
            CHECK(_combined(wide, shifted, operations[i], overlap) == Result::Success);
            CHECK(overlap && overlap->contours() == contours[i] && overlap->count() == 4u * contours[i]);
            if (overlap) CHECK(near(static_cast<float>(_pathArea(overlap)), areas[i], 1.0e-4f));
            delete overlap;
        }
        Path* tabbed = nullptr;
        CHECK(_combined(unit, tab, PathOperation::Union, tabbed) == Result::Success);
        CHECK(tabbed && tabbed->contours() == 1u && tabbed->count() == 8u);
        if (tabbed) CHECK(near(static_cast<float>(_pathArea(tabbed)), 1.5f, 1.0e-4f));
        delete tabbed;
    }
    delete wide;
    delete shifted;
    delete unit;
    delete tab;

    // A square and the same square turned an eighth of a turn share a regular octagon.
    auto square = Rectangle::gen({}, {2.0f, 2.0f});
    auto turned = Rectangle::gen({}, {2.0f, 2.0f});
    CHECK(square && turned);
    if (square && turned) {
        turned->model = Mat4::rotateZ(0.78539816f);
        Path* octagon = nullptr;
        CHECK(_combined(square, turned, PathOperation::Intersection, octagon) == Result::Success);
        CHECK(octagon && octagon->contours() == 1u && octagon->count() == 8u);
        if (octagon) CHECK(near(static_cast<float>(_pathArea(octagon)), 3.31371f, 1.0e-4f));
        delete octagon;
    }

    // A path of several contours covers by the even-odd rule, its hole included.
    PathCommand rings[] = {PathCommand::move({-2.0f, -2.0f, 0.0f}), PathCommand::line({2.0f, -2.0f, 0.0f}),
                           PathCommand::line({2.0f, 2.0f, 0.0f}), PathCommand::line({-2.0f, 2.0f, 0.0f}),
                           PathCommand::close(), PathCommand::move({-1.0f, -1.0f, 0.0f}),
                           PathCommand::line({1.0f, -1.0f, 0.0f}), PathCommand::line({1.0f, 1.0f, 0.0f}),
                           PathCommand::line({-1.0f, 1.0f, 0.0f}), PathCommand::close()};
    auto holed = Path::gen(rings, 10u);
    auto middle = Rectangle::gen({}, {3.0f, 3.0f});
    CHECK(holed && middle);
    if (holed && middle) {
        Path* frame = nullptr;
        CHECK(_combined(holed, middle, PathOperation::Intersection, frame) == Result::Success);
        CHECK(frame && frame->contours() == 2u);
        if (frame) CHECK(near(static_cast<float>(_pathArea(frame)), 5.0f, 1.0e-4f));
        delete frame;
    }

    // More shapes fold from the left: a square less two others, and the part three share.
    auto pieces = Rectangle::gen({}, {4.0f, 4.0f});
    auto bite = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto nibble = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto up = Rectangle::gen({0.5f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto over = Rectangle::gen({0.0f, 0.5f, 0.0f}, {2.0f, 2.0f});
    CHECK(pieces && bite && nibble && up && over);
    if (pieces && bite && nibble && up && over && square) {
        const Object* taken[] = {pieces, bite, nibble};
        Path* holes = nullptr;
        CHECK(Path::combine(taken, 3u, PathOperation::Difference, nullptr, holes) == Result::Success);
        CHECK(holes && holes->contours() == 3u);
        if (holes) CHECK(near(static_cast<float>(_pathArea(holes)), 14.0f, 1.0e-4f));
        delete holes;
        const Object* shared[] = {square, up, over};
        Path* common = nullptr;
        CHECK(Path::combine(shared, 3u, PathOperation::Intersection, nullptr, common) == Result::Success);
        CHECK(common && common->contours() == 1u);
        if (common) CHECK(near(static_cast<float>(_pathArea(common)), 2.25f, 1.0e-4f));
        delete common;
    }

    // The result lies in the space of the parent it is given, or of the world.
    auto group = Group::gen();
    auto first = Rectangle::gen({-0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto second = Rectangle::gen({0.5f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(group && first && second);
    if (group && first && second) {
        group->model = Mat4::translate({3.0f, 0.0f, 0.0f}) * Mat4::scale({2.0f, 2.0f, 1.0f});
        CHECK(group->add(first) == Result::Success && group->add(second) == Result::Success);
        Path* local = nullptr;
        Path* world = nullptr;
        CHECK(_combined(first, second, PathOperation::Union, local, group) == Result::Success);
        CHECK(_combined(first, second, PathOperation::Union, world) == Result::Success);
        Bounds box;
        CHECK(local && local->bounds(box) && near(box.min.x, -1.0f, 1.0e-4f) && near(box.max.x, 1.0f, 1.0e-4f));
        CHECK(world && world->bounds(box) && near(box.min.x, 1.0f, 1.0e-4f) && near(box.max.x, 5.0f, 1.0e-4f));
        if (local && world) {
            CHECK(near(static_cast<float>(_pathArea(local)), 2.0f, 1.0e-4f));
            CHECK(near(static_cast<float>(_pathArea(world)), 8.0f, 1.0e-4f));
        }
        delete local;
        delete world;
    } else {
        delete first;
        delete second;
    }
    delete group;

    // Only closed shapes enclose a region, and it takes two of them.
    Vec3 wave[] = {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    auto plot = Plot::gen(wave, 3u);
    auto segment = Line::gen({-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    CHECK(plot && segment && square);
    if (plot && segment && square) {
        CHECK(_combined(square, plot, PathOperation::Union, empty) == Result::InvalidArguments);
        CHECK(_combined(segment, square, PathOperation::Union, empty) == Result::InvalidArguments);
        CHECK(_combined(square, nullptr, PathOperation::Union, empty) == Result::InvalidArguments);
        CHECK(_combined(square, turned, static_cast<PathOperation>(9), empty) == Result::InvalidArguments);
        const Object* alone[] = {square};
        CHECK(Path::combine(alone, 1u, PathOperation::Union, nullptr, empty) == Result::InvalidArguments);
        CHECK(empty == nullptr);
    }
    delete plot;
    delete segment;
    delete pieces;
    delete bite;
    delete nibble;
    delete up;
    delete over;
    delete holed;
    delete middle;
    delete square;
    delete turned;
    delete west;
    delete east;
    delete corner;
    delete far;
    delete outer;
    delete inner;
    delete ring;
    delete left;
    delete right;

    // The result draws, animates and morphs like any path.
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto scene = Scene::gen(config);
    outer = Rectangle::gen({}, {4.0f, 4.0f});
    inner = Rectangle::gen({}, {2.0f, 2.0f});
    ring = nullptr;
    if (outer && inner) CHECK(_combined(outer, inner, PathOperation::Difference, ring) == Result::Success);
    delete outer;
    delete inner;
    CHECK(scene && ring);
    if (scene && ring) {
        ring->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        CHECK(scene->add(ring) == Result::Success);
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {0.0f, 0.0f, 0.0f}), 0.0f, 0.02f));
        CHECK(near(_shownOpacity(renderer, scene, 0.0f, {1.5f, 0.0f, 0.0f}), 1.0f, 0.02f));
        CHECK(near(static_cast<float>(_inked(renderer, scene, 0.0f)), 10800.0f, 120.0f));
    } else {
        delete ring;
    }
    delete scene;

    scene = Scene::gen(config);
    left = Circle::gen({-0.5f, 0.0f, 0.0f}, 1.0f);
    right = Circle::gen({0.5f, 0.0f, 0.0f}, 1.0f);
    lens = nullptr;
    if (left && right) CHECK(_combined(left, right, PathOperation::Intersection, lens) == Result::Success);
    auto target = Circle::gen({}, 1.5f);
    CHECK(scene && lens && target);
    if (scene && lens && target) {
        lens->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        target->stroke({0, 0, 0, 0}).fill({0, 0, 0, 255});
        CHECK(scene->add(lens) == Result::Success && scene->add(target) == Result::Success);
        CHECK(scene->play(Animation::morph(lens, target), 1.0f) == Result::Success);
        auto start = _inked(renderer, scene, 0.0f);
        auto end = _inked(renderer, scene, 1.0f);
        CHECK(near(static_cast<float>(start), 1105.0f, 60.0f) && end > start * 5u);
    } else {
        delete lens;
        delete target;
    }
    delete left;
    delete right;
    delete scene;
}

static void highlightedParts(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto scene = Scene::gen(config);
    auto block = Group::gen();
    Group* lines[3] = {};
    Rectangle* words[6] = {};
    Vec3 centers[6];
    auto ready = scene && block;
    for (auto i = 0u; i < 3u; i++) {
        lines[i] = Group::gen();
        ready = ready && lines[i] && block->add(lines[i]) == Result::Success;
        for (auto j = 0u; j < 2u && ready; j++) {
            auto& word = words[2u * i + j];
            centers[2u * i + j] = {static_cast<float>(j) * 1.5f - 0.75f, 1.0f - static_cast<float>(i), 0.0f};
            word = Rectangle::gen(centers[2u * i + j], {1.0f, 0.5f});
            ready = word && lines[i]->add(word) == Result::Success;
            if (word) word->fill(Color{0, 0, 0, 255});
        }
    }
    auto stray = Circle::gen({3.0f, 0.0f, 0.0f}, 0.5f);
    ready = ready && stray && scene->add(block) == Result::Success && scene->add(stray) == Result::Success;
    CHECK(ready);
    if (!ready) {
        delete scene;
        return;
    }

    // Mistakes leave the timeline as it was.
    Object* outside[] = {stray};
    CHECK(scene->highlight(block, outside, 1u) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "objects below it"));
    Object* first[] = {lines[0]};
    CHECK(scene->highlight(block, first, 1u, 1.5f) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "dim opacity"));
    CHECK(scene->highlight(block, first, 1u, 0.3f, 0.0f) == Result::InvalidArguments);
    CHECK(scene->highlight(nullptr, nullptr, 0u) == Result::InvalidArguments);
    CHECK(scene->highlight(block, nullptr, 1u) == Result::InvalidArguments);
    CHECK(scene->clipCount() == 0u && near(scene->cursor(), 0.0f));

    // The first line stays; the shapes of the others dim, and their groups keep still.
    CHECK(scene->highlight(block, first, 1u, 0.3f, 0.5f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->clipCount() == 4u && near(scene->cursor(), 0.5f));
    CHECK(near(_shownOpacity(renderer, scene, 0.25f, centers[2]), 0.65f, 0.01f));
    CHECK(near(_shownOpacity(renderer, scene, 0.5f, centers[5]), 0.3f, 0.01f));
    CHECK(near(_shownOpacity(renderer, scene, 0.5f, centers[1]), 1.0f, 0.01f));
    CHECK(lines[1]->opacity == 1.0f && block->opacity == 1.0f && stray->opacity == 1.0f);

    // One word of a dimmed line: it brightens, and the first line dims.
    Object* word[] = {words[2]};
    CHECK(scene->highlight(block, word, 1u, 0.3f, 0.5f) == Result::Success);
    CHECK(scene->clipCount() == 7u);
    CHECK(near(_shownOpacity(renderer, scene, 1.0f, centers[2]), 1.0f, 0.01f));
    CHECK(near(_shownOpacity(renderer, scene, 1.0f, centers[3]), 0.3f, 0.01f));
    CHECK(near(_shownOpacity(renderer, scene, 1.0f, centers[0]), 0.3f, 0.01f));

    // An empty selection brings everything back; once nothing moves, the call only waits.
    CHECK(scene->highlight(block, nullptr, 0u) == Result::Success);
    CHECK(scene->clipCount() == 12u && near(scene->cursor(), 1.5f));
    CHECK(near(_shownOpacity(renderer, scene, 1.5f, centers[0]), 1.0f, 0.01f));
    CHECK(scene->highlight(block, nullptr, 0u) == Result::Success);
    CHECK(scene->clipCount() == 12u && near(scene->cursor(), 2.0f));

    // A removed shape keeps out of later highlights.
    CHECK(scene->remove(words[5]) == Result::Success);
    CHECK(scene->highlight(block, first, 1u) == Result::Success);
    CHECK(scene->clipCount() == 15u);
    delete scene;
}

static void sceneThemes(SwRenderer* renderer)
{
    auto threeBlueOneEyes = Theme::preset(ThemePreset::ThreeBlueOneEyes);
    auto proWhite = Theme::preset(ThemePreset::ProWhite);
    auto proBlack = Theme::preset(ThemePreset::ProBlack);
    auto adaptiveVscode = Theme::preset(ThemePreset::AdaptiveVscode);
    auto legacyPro = Theme::preset(ThemePreset::Pro);
    CHECK(Theme::ColorLimit == 10u);
    Theme implicit;
    const Theme presets[] = {implicit, threeBlueOneEyes, proWhite, proBlack, adaptiveVscode, legacyPro};
    for (const auto& preset : presets) {
        CHECK(std::strcmp(preset.code.font, "JetBrainsMono") == 0);
    }
    CHECK(threeBlueOneEyes.objectCount == 8u);
    CHECK(proWhite.objectCount == 8u);
    CHECK(proBlack.objectCount == 8u);
    CHECK(near(threeBlueOneEyes.objectWidth, 2.0f));
    CHECK(near(proWhite.objectWidth, 1.25f));
    CHECK(near(proBlack.objectWidth, 1.25f));
    CHECK(same(threeBlueOneEyes.background, Color::hex("#0d1117")));
    CHECK(same(adaptiveVscode.background, proWhite.background));
    CHECK(same(adaptiveVscode.objects[0], proWhite.objects[0]));
    CHECK(same(implicit.background, proWhite.background));
    CHECK(same(implicit.objects[0], proWhite.objects[0]));
    CHECK(same(proWhite.background, Color::hex("#ffffff")));
    CHECK(same(proWhite.objects[0], Color::hex("#b45f06")));
    CHECK(same(proBlack.background, Color::hex("#000000")));
    CHECK(same(proBlack.objects[0], Color::hex("#f28e2b")));
    CHECK(same(proBlack.h1.color, Color::hex("#ffffff")));
    CHECK(same(threeBlueOneEyes.color(ThemeColorRole::Accent), Color::hex("#4cc9f0")));
    CHECK(same(threeBlueOneEyes.color(ThemeColorRole::Warning), Color::hex("#ffd166")));
    CHECK(same(proWhite.color(ThemeColorRole::Danger), Color::hex("#b3261e")));
    CHECK(same(proBlack.color(ThemeColorRole::Success), Color::hex("#7ee787")));
    CHECK(same(proWhite.color(ThemeColorRole::Result), Color::hex("#8a6500")));
    CHECK(same(proBlack.color(ThemeColorRole::Focus), Color::hex("#6ea8fe")));
    CHECK(same(proBlack.color(ThemeColorRole::Background), proBlack.background));
    CHECK(same(legacyPro.background, proWhite.background));
    CHECK(same(legacyPro.objects[0], proWhite.objects[0]));
    CHECK(same(proWhite.axis.x, proWhite.axis.grid));
    CHECK(same(proWhite.axis.y, proWhite.axis.grid));
    CHECK(same(proWhite.axis.z, proWhite.axis.grid));
    CHECK(same(proWhite.axis.grid, Color::hex("#d8dadd")));
    CHECK(same(proBlack.axis.x, proBlack.axis.grid));
    CHECK(same(proBlack.axis.y, proBlack.axis.grid));
    CHECK(same(proBlack.axis.z, proBlack.axis.grid));
    CHECK(same(proBlack.axis.grid, Color::hex("#d8dadd")));
    CHECK(!threeBlueOneEyes.gradient && !proWhite.gradient && !proBlack.gradient);
    CHECK(same(threeBlueOneEyes.endGradientStop, Color::hex("#f72585")));
    CHECK(same(proWhite.endGradientStop, Color::hex("#666666")));
    CHECK(same(proBlack.endGradientStop, Color::hex("#999999")));

    Config config;
    CHECK(same(config.background, proWhite.background));
    auto scene = Scene::gen(config);
    CHECK(scene != nullptr);
    if (!scene) return;
    CHECK(same(scene->theme().background, proWhite.background));
    CHECK(same(scene->theme().objects[0], proWhite.objects[0]));
    CHECK(scene->theme(threeBlueOneEyes) == Result::Success);
    CHECK(same(scene->config().background, threeBlueOneEyes.background));
    CHECK(scene->theme(proWhite) == Result::Success);
    CHECK(same(scene->config().background, proWhite.background));
    CHECK(std::strcmp(scene->theme().h1.font, "Pretendard") == 0);

    auto title = Text::gen("Theme");
    auto axes = Space::gen();
    auto explicitLine = Line::gen({-1.0f, 0.0f}, {1.0f, 0.0f});
    CHECK(title && axes && explicitLine);
    if (!title || !axes || !explicitLine) {
        delete title;
        delete axes;
        delete explicitLine;
        delete scene;
        return;
    }
    title->role = TextRole::H1;
    explicitLine->stroke(Color::hex("#010203"));
    CHECK(scene->add(title) == Result::Success);
    CHECK(scene->add(axes) == Result::Success);
    CHECK(scene->add(explicitLine) == Result::Success);
    CHECK(title->size == proWhite.h1.size);
    CHECK(std::strcmp(title->font(), proWhite.h1.font) == 0);
    CHECK(same(title->style.fill, proWhite.h1.color));
    // Any positive size is kept, including the one the theme sentinel used to claim.
    const float sizes[] = {28.0f, proWhite.text.size, 0.5f};
    for (auto size : sizes) {
        auto sized = Text::gen("sized");
        CHECK(sized && sized->size == 0.0f);
        if (!sized) continue;
        sized->size = size;
        CHECK(scene->add(sized) == Result::Success);
        CHECK(sized->size == size);
    }
    auto negative = Text::gen("negative");
    if (negative) {
        negative->size = -1.0f;
        CHECK(scene->add(negative) == Result::InvalidArguments);
        delete negative;
    }
    CHECK(same(axes->axisX, proWhite.axis.x));
    CHECK(same(axes->axisY, proWhite.axis.y));
    CHECK(same(axes->axisZ, proWhite.axis.z));
    CHECK(same(axes->style.stroke, proWhite.axis.grid));
    CHECK(same(axes->numberColor, proWhite.axis.label));
    CHECK(same(explicitLine->style.stroke, Color::hex("#010203")));
    CHECK(near(explicitLine->style.width, proWhite.objectWidth));

    Line* lines[7] = {};
    for (auto i = 0u; i < 7u; i++) {
        lines[i] = Line::gen({-1.0f, static_cast<float>(i)},
                             {1.0f, static_cast<float>(i)});
        CHECK(lines[i] != nullptr);
        if (lines[i]) CHECK(scene->add(lines[i]) == Result::Success);
    }
    if (lines[6]) {
        CHECK(same(lines[0]->style.stroke, proWhite.objects[0]));
        CHECK(same(lines[5]->style.stroke, proWhite.objects[5]));
        CHECK(same(lines[6]->style.stroke, proWhite.objects[6]));
        CHECK(near(lines[0]->style.width, proWhite.objectWidth));
    }

    auto filled = Point::gen({0.0f, 0.0f});
    auto authoredWidth = Line::gen({-1.0f, -1.0f}, {1.0f, -1.0f});
    Vec3 fillPoints[] = {
        {-1.0f, -1.0f}, {1.0f, -1.0f},
        {-1.0f, 1.0f}, {1.0f, 1.0f},
    };
    auto filledSurface = SurfaceMesh::gen(fillPoints, 2u, 2u);
    CHECK(filled && authoredWidth && filledSurface);
    if (filled) {
        CHECK(scene->add(filled) == Result::Success);
        CHECK(same(filled->style.stroke, proWhite.objects[7]));
        CHECK(same(filled->style.fill, proWhite.objects[7]));
        CHECK(filled->style.fill.a == 255u);
    }
    if (authoredWidth) {
        authoredWidth->strokeWidth(3.5f);
        CHECK(scene->add(authoredWidth) == Result::Success);
        CHECK(near(authoredWidth->style.width, 3.5f));
    }
    if (filledSurface) {
        filledSurface->shading = false;
        CHECK(scene->add(filledSurface) == Result::Success);
        CHECK(same(filledSurface->style.stroke, proWhite.objects[1]));
        CHECK(same(filledSurface->style.fill, proWhite.objects[1]));
        CHECK(filledSurface->style.fill.a == 255u);
    }
    CHECK(scene->theme(threeBlueOneEyes) == Result::InsufficientCondition);
    delete scene;

    auto editorial = proWhite;
    editorial.h1.font = "Source Serif 4";
    editorial.h2.font = "Source Serif 4";
    editorial.h3.font = "Source Serif 4";
    editorial.text.font = "Source Serif 4";
    editorial.code.font = "Source Serif 4";
    scene = Scene::gen();
    auto english = Text::gen("English explanation");
    auto korean = Text::gen(u8"\ud55c\uae00 \uc124\uba85");
    auto explicitKorean = Text::gen(u8"\uc0ac\uc6a9\uc790 \ud3f0\ud2b8");
    CHECK(scene && english && korean && explicitKorean);
    if (scene && english && korean && explicitKorean) {
        CHECK(scene->theme(editorial) == Result::Success);
        english->role = TextRole::Code;
        korean->role = TextRole::H2;
        CHECK(explicitKorean->font("Pretendard") == Result::Success);
        CHECK(scene->add(english) == Result::Success);
        CHECK(scene->add(korean) == Result::Success);
        CHECK(scene->add(explicitKorean) == Result::Success);
        CHECK(std::strcmp(english->font(), "Source Serif 4") == 0);
        CHECK(std::strcmp(korean->font(), "IBM Plex Sans KR") == 0);
        CHECK(std::strcmp(explicitKorean->font(), "Pretendard") == 0);
    } else {
        delete english;
        delete korean;
        delete explicitKorean;
    }
    delete scene;

    auto invalid = threeBlueOneEyes;
    invalid.objectCount = 0u;
    scene = Scene::gen();
    CHECK(scene != nullptr);
    if (scene) CHECK(scene->theme(invalid) == Result::InvalidArguments);
    delete scene;

    invalid = threeBlueOneEyes;
    invalid.objectWidth = 0.0f;
    scene = Scene::gen();
    CHECK(scene != nullptr);
    if (scene) CHECK(scene->theme(invalid) == Result::InvalidArguments);
    delete scene;

    Config gradientConfig;
    gradientConfig.width = 200u;
    gradientConfig.height = 100u;
    gradientConfig.camera.orthoHeight = 4.0f;
    gradientConfig.background = Color::hex("#ffffff");
    auto gradientTheme = threeBlueOneEyes;
    gradientTheme.objects[0] = Color::hex("#ff0000");
    gradientTheme.objectCount = 1u;
    gradientTheme.gradient = true;
    gradientTheme.endGradientStop = Color::hex("#0000ff");
    scene = Scene::gen(gradientConfig);
    auto gradientRectangle = Rectangle::gen({}, {2.0f, 2.0f});
    auto explicitGradient = Line::gen({-1.0f, -1.5f}, {1.0f, -1.5f});
    auto disabledGradient = Line::gen({-1.0f, 1.5f}, {1.0f, 1.5f});
    auto directGradient = Line::gen({-1.0f, 1.8f}, {1.0f, 1.8f});
    CHECK(scene && gradientRectangle && explicitGradient && disabledGradient && directGradient);
    if (scene && gradientRectangle && explicitGradient && disabledGradient && directGradient) {
        CHECK(scene->theme(gradientTheme) == Result::Success);
        gradientRectangle->stroke(Color::hex("#00000000"), 0.0f)
                         .fill(Color::hex("#ff0000"));
        explicitGradient->gradient(Color::hex("#00ff00"));
        disabledGradient->gradient(false);
        directGradient->style.gradient.type = GradientType::Linear;
        directGradient->style.gradientEnd = Color::hex("#00ffff");
        CHECK(scene->add(gradientRectangle) == Result::Success);
        CHECK(scene->add(explicitGradient) == Result::Success);
        CHECK(scene->add(disabledGradient) == Result::Success);
        CHECK(scene->add(directGradient) == Result::Success);
        CHECK(gradientRectangle->style.gradient.enabled());
        CHECK(same(gradientRectangle->style.gradientEnd, Color::hex("#0000ff")));
        CHECK(explicitGradient->style.gradient.enabled());
        CHECK(same(explicitGradient->style.gradientEnd, Color::hex("#00ff00")));
        CHECK(!disabledGradient->style.gradient.enabled());
        CHECK(directGradient->style.gradient.enabled());
        CHECK(same(directGradient->style.gradientEnd, Color::hex("#00ffff")));
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        auto left = surface.data()[50u * surface.stride() + 85u];
        auto right = surface.data()[50u * surface.stride() + 115u];
        CHECK(left != right);
        CHECK((left & 0xffu) > (right & 0xffu));
        CHECK(((left >> 16) & 0xffu) < ((right >> 16) & 0xffu));
    } else {
        delete gradientRectangle;
        delete explicitGradient;
        delete disabledGradient;
        delete directGradient;
    }
    delete scene;

    {
        Gradient invalidGradient;
        invalidGradient.type = GradientType::Linear;
        invalidGradient.stopCount = 1u;
        CHECK(!invalidGradient.valid());
        invalidGradient.stopCount = 2u;
        invalidGradient.stops[0] = {0.6f, Color::hex("#ff0000")};
        invalidGradient.stops[1] = {0.4f, Color::hex("#0000ff")};
        CHECK(!invalidGradient.valid());
        invalidGradient.stops[1].offset = 1.0f;
        CHECK(invalidGradient.valid());
        invalidGradient.sized = true;
        invalidGradient.radius = 1.0f;
        CHECK(!invalidGradient.valid());
        invalidGradient.type = GradientType::Radial;
        CHECK(invalidGradient.valid());
        invalidGradient.radius = 0.0f;
        CHECK(!invalidGradient.valid());
    }

    Config kindsConfig;
    kindsConfig.width = 300u;
    kindsConfig.height = 100u;
    kindsConfig.camera.orthoHeight = 4.0f;
    kindsConfig.background = Color::hex("#000000");
    scene = Scene::gen(kindsConfig);
    auto linearShape = Rectangle::gen({}, {2.0f, 2.0f});
    auto radialShape = Circle::gen({}, 1.0f);
    auto conicShape = Circle::gen({4.0f, 0.0f}, 1.0f);
    CHECK(scene && linearShape && radialShape && conicShape);
    if (scene && linearShape && radialShape && conicShape) {
        Gradient linear;
        linear.type = GradientType::Linear;
        linear.stopCount = 2u;
        linear.stops[0] = {0.0f, Color::hex("#ff0000")};
        linear.stops[1] = {1.0f, Color::hex("#0000ff")};
        linear.from = {-1.0f, 0.0f, 0.0f};
        linear.to = {1.0f, 0.0f, 0.0f};
        linear.line = true;
        Gradient radial;
        radial.type = GradientType::Radial;
        radial.stopCount = 2u;
        radial.stops[0] = {0.0f, Color::hex("#ffffff")};
        radial.stops[1] = {1.0f, Color::hex("#202020")};
        Gradient conic;
        conic.type = GradientType::Conic;
        conic.stopCount = 4u;
        conic.stops[0] = {0.0f, Color::hex("#ff0000")};
        conic.stops[1] = {0.33f, Color::hex("#00ff00")};
        conic.stops[2] = {0.66f, Color::hex("#0000ff")};
        conic.stops[3] = {1.0f, Color::hex("#ff0000")};
        conic.center = {4.0f, 0.0f, 0.0f};
        conic.centered = true;
        Object* shapes[] = {linearShape, radialShape, conicShape};
        for (auto shape : shapes) {
            shape->stroke(Color::hex("#00000000"), 0.0f).fill(Color::hex("#ffffff"));
        }
        CHECK(linearShape->gradient(linear) == Result::Success);
        CHECK(radialShape->gradient(radial) == Result::Success);
        CHECK(conicShape->gradient(conic) == Result::Success);
        // Explicit geometry follows the object model, so shifting moves the gradient too.
        linearShape->shift({-4.0f, 0.0f, 0.0f});
        CHECK(scene->add(linearShape) == Result::Success);
        CHECK(scene->add(radialShape) == Result::Success);
        CHECK(scene->add(conicShape) == Result::Success);
        CHECK(linearShape->style.gradient.type == GradientType::Linear);
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        auto at = [&](float x, float y) {
            auto px = static_cast<uint32_t>(x * 25.0f + 150.0f);
            auto py = static_cast<uint32_t>(50.0f - y * 25.0f);
            return surface.data()[py * surface.stride() + px];
        };
        auto channel = [](uint32_t value, uint32_t shift) { return (value >> shift) & 0xffu; };
        auto sum = [&](uint32_t value) {
            return channel(value, 0u) + channel(value, 8u) + channel(value, 16u);
        };
        auto left = at(-4.8f, 0.0f);
        auto right = at(-3.2f, 0.0f);
        CHECK(left != right);
        CHECK(channel(left, 0u) != channel(right, 0u));
        CHECK(channel(left, 16u) != channel(right, 16u));
        auto middle = at(0.0f, 0.0f);
        auto edge = at(0.85f, 0.0f);
        CHECK(sum(middle) > sum(edge) + 150u);
        auto east = at(4.6f, 0.0f);
        auto north = at(4.0f, 0.6f);
        auto west = at(3.4f, 0.0f);
        auto south = at(4.0f, -0.6f);
        CHECK(east != north);
        CHECK(north != west);
        CHECK(west != south);
        CHECK(south != east);
    } else {
        delete linearShape;
        delete radialShape;
        delete conicShape;
    }
    delete scene;

    scene = Scene::gen();
    Vec3 sourcePoints[] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 1.0f}};
    Vec3 targetPoints[] = {{-1.0f, 0.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}};
    auto source = Polygon::gen(sourcePoints, 3u);
    auto target = Polygon::gen(targetPoints, 3u);
    CHECK(scene && source && target);
    if (scene && source && target) {
        CHECK(scene->add(source) == Result::Success);
        auto sourceColor = source->style.stroke;
        CHECK(scene->add(target) == Result::Success);
        CHECK(!same(target->style.stroke, sourceColor));
        CHECK(scene->play(Animation::morph(source, target), 0.5f) == Result::Success);
        CHECK(same(target->style.stroke, sourceColor));
    } else {
        delete source;
        delete target;
    }
    delete scene;
}

static void groupStyles(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto outer = Group::gen();
    auto inner = Group::gen();
    auto relay = Group::gen();
    auto plain = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto nested = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto own = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto deep = Rectangle::gen({0.0f, -1.0f, 0.0f}, {0.5f, 0.5f});
    auto line = Line::gen({-3.0f, -1.5f, 0.0f}, {3.0f, -1.5f, 0.0f});
    auto label = Text::gen("label", {0.0f, 1.5f, 0.0f});
    auto loose = Circle::gen({3.0f, 1.5f, 0.0f}, 0.25f);
    auto later = Circle::gen({-3.0f, 1.5f, 0.0f}, 0.25f);
    Object* objects[] = {outer, inner, relay, plain, nested, own, deep, line, label, loose, later};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    auto green = Color::hex("#00ff00");
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto yellow = Color::hex("#ffff00");
    auto white = Color::hex("#ffffff");
    outer->stroke(green);
    outer->fill(red);
    outer->strokeWidth(6.0f);
    inner->fill(blue);
    own->stroke(yellow);
    CHECK(inner->add(nested) == Result::Success);
    CHECK(relay->add(deep) == Result::Success);
    CHECK(outer->add(plain) == Result::Success);
    CHECK(outer->add(inner) == Result::Success);
    CHECK(outer->add(relay) == Result::Success);
    CHECK(outer->add(own) == Result::Success);
    CHECK(outer->add(line) == Result::Success);
    CHECK(outer->add(label) == Result::Success);
    CHECK(scene->add(outer) == Result::Success);

    // Shapes take the stroke, fill and width of the nearest group that sets them.
    CHECK(same(plain->style.stroke, green) && same(plain->style.fill, red));
    CHECK(near(plain->style.width, 6.0f));
    CHECK(same(nested->style.stroke, green) && same(nested->style.fill, blue));
    CHECK(near(nested->style.width, 6.0f));
    CHECK(same(deep->style.stroke, green) && same(deep->style.fill, red));
    // A color a shape sets wins, a line has no area to fill, and text keeps its role color.
    CHECK(same(own->style.stroke, yellow) && same(own->style.fill, red));
    CHECK(same(line->style.stroke, green) && same(line->style.fill, Style{}.fill));
    CHECK(same(label->style.fill, scene->theme().text.color));
    // Shapes that inherit their colors leave the theme palette to the next shape.
    CHECK(scene->add(loose) == Result::Success);
    CHECK(same(loose->style.stroke, scene->theme().objects[0]));

    // Animating the group's fill animates the shapes that take it, and only those.
    auto clips = scene->clipCount();
    CHECK(scene->play(AnimationTarget::from(outer).fill(white), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->clipCount() == clips + 4u);
    CHECK(same(plain->style.fill, white) && same(own->style.fill, white) && same(deep->style.fill, white));
    CHECK(same(nested->style.fill, blue) && same(plain->style.stroke, green));
    Surface surface;
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    if (surface.data()) {
        auto halfway = surface.data()[40u * surface.stride() + 40u];
        auto greenChannel = static_cast<uint8_t>(halfway >> 8u);
        CHECK(static_cast<uint8_t>(halfway) > 240u && greenChannel > 110u && greenChannel < 145u);
    }
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    if (surface.data()) CHECK(surface.data()[40u * surface.stride() + 40u] == pixel(white));
    // Effects color them as well, and a shape added later takes the group's new color.
    CHECK(scene->play(Animation::stroke(outer, blue), 0.5f) == Result::Success);
    CHECK(same(plain->style.stroke, blue) && same(nested->style.stroke, blue));
    CHECK(same(own->style.stroke, yellow));
    CHECK(outer->add(later) == Result::Success);
    CHECK(same(later->style.stroke, blue));

    // A color the same call gives a shape itself, or a nearer group, wins, and the shape keeps
    // a color it was animated to.
    AnimationTarget named[] = {AnimationTarget::from(outer).fill(red), AnimationTarget::from(plain).fill(yellow),
                               AnimationTarget::from(relay).fill(blue)};
    CHECK(scene->play(named, 3u, 0.5f) == Result::Success);
    CHECK(same(plain->style.fill, yellow) && same(deep->style.fill, blue) && same(own->style.fill, red));
    CHECK(scene->play(AnimationTarget::from(outer).fill(green), 0.5f) == Result::Success);
    CHECK(same(plain->style.fill, yellow) && same(deep->style.fill, blue) && same(own->style.fill, green));

    // A shape that is still taking a color from one group cannot take it from another.
    auto mark = scene->cursor();
    CHECK(scene->play(AnimationTarget::from(outer).stroke(red), 2.0f) == Result::Success);
    CHECK(scene->seek(mark + 1.0f) == Result::Success);
    CHECK(scene->play(Animation::stroke(relay, white), 1.0f) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "takes its color from the group"));
    CHECK(scene->seek(mark + 2.0f) == Result::Success);
    CHECK(scene->play(Animation::stroke(relay, white), 1.0f) == Result::Success);
    CHECK(same(deep->style.stroke, white) && same(plain->style.stroke, red));
    // A removed shape keeps its colors.
    CHECK(scene->remove(own) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(outer).fill(blue), 0.5f) == Result::Success);
    CHECK(same(own->style.fill, green));
    delete scene;

    // A direct edit of a group before the first animation reaches its shapes too.
    auto direct = Scene::gen(config);
    auto holder = Group::gen();
    auto child = Rectangle::gen();
    CHECK(direct && holder && child);
    if (direct && holder && child) {
        CHECK(holder->add(child) == Result::Success);
        CHECK(direct->add(holder) == Result::Success);
        holder->style.fill = red;
        CHECK(direct->update(holder) == Result::Success);
        CHECK(same(child->style.fill, red));
        CHECK(direct->play(AnimationTarget::from(child).opacity(0.5f), 0.5f) == Result::Success);
    } else {
        delete holder;
        delete child;
    }
    delete direct;
}

static void runtimeModifiers(SwRenderer* renderer)
{
    Config config;
    config.cameraMode = CameraMode::Interactive;
    auto scene = Scene::gen(config);
    auto object = Rectangle::gen({}, {1.0f, 1.0f});
    RuntimeProbe probe;
    RuntimeModifier modifier;
    modifier.object = runtimeObject;
    modifier.camera = runtimeCamera;
    modifier.input = runtimeInput;
    modifier.data = &probe;
    CHECK(scene && object);
    if (scene && object) {
        CHECK(scene->add(object) == Result::Success);
        CHECK(scene->runtime(&modifier) == Result::Success);
        auto invalidAction = CameraInput{static_cast<CameraAction>(255u), {}};
        CHECK(scene->camera(invalidAction) == Result::InvalidArguments);
        CHECK(scene->camera({CameraAction::Pan,
                             {std::numeric_limits<float>::quiet_NaN(), 0.0f}})
              == Result::InvalidArguments);
        CHECK(probe.inputs == 0u);
        CHECK(scene->camera({CameraAction::Pan, {0.1f, 0.0f}}) == Result::Success);
        CHECK(probe.inputs == 1u);

        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(probe.cameras == 1u && probe.objects == 1u);
        probe.invalidateCamera = true;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::InvalidArguments);
        CHECK(probe.cameras == 2u);
        probe.invalidateCamera = false;
        scene->config().camera.near = -1.0f;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::InvalidArguments);
        CHECK(probe.cameras == 2u);
    } else {
        delete object;
    }
    delete scene;

    scene = Scene::gen(config);
    object = Rectangle::gen({}, {1.0f, 1.0f});
    probe = {};
    RuntimeProbe secondProbe;
    modifier = {};
    modifier.object = runtimeObject;
    modifier.camera = runtimeCamera;
    modifier.input = runtimeInput;
    modifier.data = &probe;
    uint8_t firstKey = 0u;
    uint8_t secondKey = 0u;
    modifier.key = &firstKey;
    auto second = modifier;
    second.data = &secondProbe;
    second.key = &secondKey;
    CHECK(scene && object);
    if (scene && object) {
        CHECK(scene->add(object) == Result::Success);
        CHECK(scene->runtime(&modifier) == Result::Success);
        CHECK(scene->runtime(&second) == Result::InsufficientCondition);
        CHECK(scene->runtimeAdd(&second) == Result::Success);
        CHECK(scene->runtimeAdd(&second) == Result::Success);
        auto conflicting = second;
        conflicting.key = &firstKey;
        CHECK(scene->runtimeAdd(&conflicting) == Result::InsufficientCondition);
        Surface surface;
        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
        CHECK(probe.objects == 1u && secondProbe.objects == 1u);
        CHECK(probe.cameras == 1u && secondProbe.cameras == 1u);
        CHECK(scene->camera({CameraAction::Pan, {0.1f, 0.0f}}) == Result::Success);
        CHECK(probe.inputs == 1u && secondProbe.inputs == 0u);
        CHECK(scene->runtimeRemove(&probe) == Result::Success);
        CHECK(scene->camera({CameraAction::Pan, {0.1f, 0.0f}}) == Result::Success);
        CHECK(secondProbe.inputs == 1u);
        CHECK(scene->runtimeRemove(&probe) == Result::InsufficientCondition);
    } else {
        delete object;
    }
    delete scene;

    scene = Scene::gen();
    object = Rectangle::gen({}, {1.0f, 1.0f});
    probe = {};
    modifier = {};
    modifier.object = runtimeObject;
    modifier.data = &probe;
    CHECK(scene && object);
    if (scene && object) {
        CHECK(scene->add(object) == Result::Success);
        CHECK(scene->runtime(&modifier) == Result::Success);
        object->opacity = 2.0f;
        BBox bounds;
        CHECK(renderer->bounds(scene, object, 0.0f, bounds) == Result::InvalidArguments);
        CHECK(probe.objects == 0u);
    } else {
        delete object;
    }
    delete scene;

    scene = Scene::gen();
    object = Rectangle::gen({}, {1.0f, 1.0f});
    probe = {};
    probe.invalidateObject = true;
    modifier = {};
    modifier.object = runtimeObject;
    modifier.data = &probe;
    CHECK(scene && object);
    if (scene && object) {
        CHECK(scene->add(object) == Result::Success);
        CHECK(scene->runtime(&modifier) == Result::Success);
        BBox bounds;
        CHECK(renderer->bounds(scene, object, 0.0f, bounds) == Result::InvalidArguments);
        CHECK(probe.objects == 1u);
    } else {
        delete object;
    }
    delete scene;
}

static uint8_t red(const Surface& surface, uint32_t x, uint32_t y)
{
    return static_cast<uint8_t>(surface.data()[static_cast<size_t>(y) * surface.stride() + x] & 0xffu);
}

static void timelineTracks(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto box = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto late = Rectangle::gen({2.0f, 1.0f, 0.0f}, {0.5f, 0.5f});
    CHECK(scene && box && late);
    if (!scene || !box || !late) {
        delete scene;
        delete box;
        delete late;
        return;
    }
    box->style.stroke.a = 0;
    box->fill(Color::hex("#ffffff"));
    CHECK(scene->add(box) == Result::Success);
    CHECK(near(scene->cursor(), 0.0f));

    // A shift and a fade of one object overlap because they write different channels.
    CHECK(scene->play(Animation::shift(box, {2.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(near(scene->cursor(), 2.0f));
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::fade(box, 0.5f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(near(scene->cursor(), 1.0f));
    CHECK(near(scene->duration(), 2.0f));
    Surface surface;
    BBox bounds;
    CHECK(renderer->bounds(scene, box, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 50.0f, 1.0f));
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(red(surface, 50u, 40u) > 180u && red(surface, 50u, 40u) < 200u);
    CHECK(renderer->bounds(scene, box, 1.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 70.0f, 1.0f));
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(red(surface, 70u, 40u) > 120u && red(surface, 70u, 40u) < 136u);

    // One channel stays in time order, and the failure explains itself: it names the property
    // and the time it is busy until.
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::fill(box, Color::hex("#ff0000")), 0.5f) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    CHECK(scene->remove(box) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(scene->seek(1.25f) == Result::Success);
    CHECK(scene->play(Animation::fill(box, Color::hex("#00ff00")), 0.5f) == Result::InsufficientCondition);
    CHECK(scene->diagnostic()
          && std::strcmp(scene->diagnostic(),
                         "the object already animates its fill color until 1.50 s; begin at that time or later") == 0);
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(box, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(near(scene->duration(), 3.0f));

    // Objects cannot animate before they exist.
    CHECK(scene->seek(4.0f) == Result::Success);
    CHECK(scene->add(late) == Result::Success);
    CHECK(scene->seek(3.5f) == Result::Success);
    CHECK(scene->play(Animation::fadeIn(late), 0.5f) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(near(scene->duration(), 4.0f));

    // Camera moves overlap object clips and add up with each other, lens changes included.
    auto camera = scene->config().camera;
    camera.orthoHeight = 2.0f;
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    camera.orthoHeight = 3.0f;
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    camera.fov = 0.5f;
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    CHECK(near(scene->duration(), 4.0f));

    CHECK(scene->mark("beat") == Result::Success);
    CHECK(scene->mark("beat") == Result::InvalidArguments);
    CHECK(scene->mark("") == Result::InvalidArguments);
    CHECK(scene->mark(nullptr) == Result::InvalidArguments);
    auto time = 0.0f;
    CHECK(scene->marker("beat", time) && near(time, 2.0f));
    CHECK(!scene->marker("missing", time));
    CHECK(scene->markerCount() == 1u);
    auto name = scene->markerAt(0u, time);
    CHECK(name && std::strcmp(name, "beat") == 0 && near(time, 2.0f));
    CHECK(scene->markerAt(1u, time) == nullptr);
    CHECK(scene->seek(-1.0f) == Result::InvalidArguments);
    CHECK(scene->seek(std::numeric_limits<float>::infinity()) == Result::InvalidArguments);
    delete scene;
}

// Mean column of the red pixels, or -1 when none are red.
static float _redColumn(const Surface& surface)
{
    auto sum = 0.0;
    auto count = 0u;
    for (auto y = 0u; y < surface.height(); y++) {
        for (auto x = 0u; x < surface.width(); x++) {
            auto value = surface.data()[static_cast<size_t>(y) * surface.stride() + x];
            if ((value & 0xffu) > 160u && ((value >> 8) & 0xffu) < 90u) {
                sum += x;
                count++;
            }
        }
    }
    return count ? static_cast<float>(sum / count) : -1.0f;
}

// Each object samples the clips of its own list, in the order they were stored, whatever the
// clips of other objects stored between them: forty points switch on one by one, each group of
// ten stored latest first, while a counting text counts up in clips stored among theirs.
static void clipLists(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 100;
    config.camera.orthoHeight = 4.0f;
    auto scene = Scene::gen(config);
    auto counter = Text::gen("0");
    CHECK(scene && counter);
    if (!scene || !counter) {
        delete scene;
        delete counter;
        return;
    }
    constexpr auto Count = 40u;
    Point* points[Count] = {};
    CHECK(counter->count(0.0f, NumberFormat{}) == Result::Success);
    CHECK(scene->add(counter) == Result::Success);
    for (auto i = 0u; i < Count; i++) {
        points[i] = Point::gen({-3.0f + 0.15f * static_cast<float>(i), -1.0f, 0.0f});
        CHECK(points[i] != nullptr);
        if (!points[i]) {
            delete scene;
            return;
        }
        points[i]->opacity = 0.0f;
        CHECK(scene->add(points[i]) == Result::Success);
    }
    CHECK(scene->wait(4.0f) == Result::Success);
    for (auto step = 0u; step < Count / 10u; step++) {
        for (auto j = 10u; j-- > 0u;) {
            auto i = step * 10u + j;
            CHECK(scene->seek(0.1f * static_cast<float>(i)) == Result::Success);
            CHECK(scene->update(AnimationTarget::from(points[i]).opacity(1.0f)) == Result::Success);
        }
        CHECK(scene->seek(0.1f * static_cast<float>(step * 10u + 9u)) == Result::Success);
        auto value = static_cast<float>((step + 1u) * 10u);
        CHECK(scene->play(AnimationTarget::from(counter).value(value), 0.05f, Easing::Linear) == Result::Success);
    }
    CHECK(scene->clipCount() == Count + Count / 10u);
    char shown[16];
    const struct { float time; const char* text; } counts[] = {
        {0.0f, "0"}, {0.85f, "0"}, {0.96f, "10"}, {1.5f, "10"}, {2.0f, "20"}, {3.0f, "30"}, {4.0f, "40"}};
    for (auto& expected : counts) {
        CHECK(scene->text(counter, expected.time, shown, sizeof(shown)) > 0u);
        CHECK(std::strcmp(shown, expected.text) == 0);
    }
    for (auto i = 0u; i < Count; i++) {
        auto on = 0.1f * static_cast<float>(i);
        BBox bounds;
        CHECK(renderer->bounds(scene, points[i], on + 0.01f, bounds) == Result::Success);
        if (i > 0u) CHECK(renderer->bounds(scene, points[i], on - 0.06f, bounds) == Result::InsufficientCondition);
    }
    delete scene;
}

// Placing every object reads each clip once for its object and once for every object below
// it, whether those objects joined before the clip or after it; the scenes of viewports and
// transitions add theirs.
static void sampleWork()
{
    auto scene = Scene::gen();
    auto inset = Scene::gen();
    auto from = Scene::gen();
    auto to = Scene::gen();
    auto staged = Scene::gen();
    auto group = Group::gen();
    auto inner = Group::gen();
    auto first = Circle::gen({}, 0.5f);
    auto second = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    auto third = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto apart = Rectangle::gen({0.0f, 2.0f, 0.0f}, {1.0f, 1.0f});
    auto later = Circle::gen({3.0f, 0.0f, 0.0f}, 0.5f);
    auto shown = Circle::gen({}, 0.25f);
    auto leaving = Circle::gen({}, 0.25f);
    auto coming = Circle::gen({}, 0.25f);
    Scene* scenes[] = {scene, inset, from, to, staged};
    Object* objects[] = {group, inner, first, second, third, apart, later, shown, leaving, coming};
    auto made = true;
    for (auto item : scenes) made = made && item;
    for (auto item : objects) made = made && item;
    CHECK(made);
    if (!made) {
        for (auto item : scenes) delete item;
        for (auto item : objects) delete item;
        return;
    }
    CHECK(inner->add(second) == Result::Success);
    CHECK(inner->add(third) == Result::Success);
    CHECK(group->add(first) == Result::Success);
    CHECK(group->add(inner) == Result::Success);
    CHECK(scene->add(group) == Result::Success);
    CHECK(scene->add(apart) == Result::Success);
    CHECK(scene->sampleWork() == 0u);
    // A move of the group is read for it and the four objects below it, a turn of the inner
    // group for it and its two circles.
    CHECK(scene->play(Animation::shift(group, {1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->sampleWork() == 5u);
    CHECK(scene->play(Animation::rotate(inner, 1.0f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->play(Animation::fade(apart, 0.5f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->clipCount() == 3u && scene->sampleWork() == 9u);
    // A circle that joins the inner group later is read for the clip of each group above it.
    CHECK(inner->add(later) == Result::Success);
    CHECK(scene->sampleWork() == 11u);
    CHECK(scene->play(Animation::shift(later, {0.0f, 1.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->play(Animation::shift(group, {-1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->clipCount() == 5u && scene->sampleWork() == 18u);
    CHECK(inset->add(shown) == Result::Success);
    CHECK(inset->play(Animation::fade(shown, 0.5f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->viewport(inset, {0.5f, 0.0f, 0.5f, 1.0f}) == Result::Success);
    CHECK(scene->sampleWork() == 19u);
    CHECK(from->add(leaving) == Result::Success);
    CHECK(from->play(Animation::fade(leaving, 0.5f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(to->add(coming) == Result::Success);
    CHECK(to->play(Animation::shift(coming, {1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(to->play(Animation::fade(coming, 0.5f), 1.0f, Easing::Linear) == Result::Success);
    Scene* stages[] = {from, to};
    CHECK(staged->transition(stages, 2u, {}, 1.0f, 0.0f, Easing::Linear) == Result::Success);
    CHECK(staged->sampleWork() == 3u);
    delete scene;
    delete staged;

    // A morph from a copy that fails takes the copy back, and with it the work the clip of the
    // group above it read for it.
    auto copying = Scene::gen();
    auto holder = Group::gen();
    auto source = Circle::gen({}, 0.5f);
    auto stale = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(copying && holder && source && stale);
    if (!copying || !holder || !source || !stale) {
        delete copying;
        delete holder;
        delete source;
        delete stale;
        return;
    }
    CHECK(holder->add(source) == Result::Success && copying->add(holder) == Result::Success);
    CHECK(copying->add(stale) == Result::Success);
    CHECK(copying->play(Animation::shift(holder, {1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(copying->sampleWork() == 2u);
    auto held = copying->count();
    auto morph = Animation::morph(source, stale);
    morph.copy = true;
    CHECK(copying->play(morph, 1.0f) != Result::Success);
    CHECK(copying->count() == held && copying->sampleWork() == 2u);
    CHECK(copying->play(Animation::shift(holder, {-1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(copying->sampleWork() == 4u);
    delete copying;
}

// An orbit keeps every angle: a full turn passes behind the scene and returns, where a look
// to the same pose would not move at all.
static void cameraOrbit(SwRenderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 64;
    config.height = 64;
    config.background = {0, 0, 0, 255};
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {0.0f, 0.0f, 10.0f};
    auto scene = Scene::gen(config);
    auto dot = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(scene && dot);
    if (!scene || !dot) {
        delete dot;
        delete scene;
        return;
    }
    dot->stroke({255, 0, 0, 255}).fill({255, 0, 0, 255});
    dot->style.radius = 3.0f;
    CHECK(scene->add(dot) == Result::Success);
    CHECK(scene->orbit(2.0f * PI, {0.0f, 1.0f, 0.0f}, 4.0f, Easing::Linear) == Result::Success);
    CHECK(near(scene->duration(), 4.0f));
    Surface surface;
    auto start = -1.0f;
    if (renderer->render(scene, 0.0f, surface) == Result::Success) start = _redColumn(surface);
    CHECK(start > 36.0f);
    // A quarter turn counterclockwise about +y puts the eye on +x, in line with the dot.
    if (renderer->render(scene, 1.0f, surface) == Result::Success) CHECK(std::fabs(_redColumn(surface) - 31.5f) < 1.0f);
    // Half way the eye is behind the target, so the dot is on the left.
    if (renderer->render(scene, 2.0f, surface) == Result::Success) {
        auto column = _redColumn(surface);
        CHECK(column >= 0.0f && column < 27.0f);
    }
    if (renderer->render(scene, 4.0f, surface) == Result::Success) CHECK(std::fabs(_redColumn(surface) - start) < 0.5f);

    // A look that changes the lens adds to the orbit: the field of view narrows while the eye
    // keeps circling, and the next move starts where both end. A perspective camera puts a point
    // x right of its view at depth d at column 31.5 + 32 x / (d tan(fov / 2)).
    CHECK(scene->seek(3.0f) == Result::Success);
    auto lens = scene->config().camera;
    lens.fov = 0.5f;
    CHECK(scene->look(lens, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    auto column = [](float x, float depth, float fov) { return 31.5f + 32.0f * x / (depth * std::tan(0.5f * fov)); };
    // At 3.5 s the eye is an eighth turn short of +z, which puts the dot sqrt(2) right of the
    // view and sqrt(2) beyond the target.
    auto diagonal = std::sqrt(2.0f);
    CHECK(renderer->render(scene, 3.5f, surface) == Result::Success);
    CHECK(std::fabs(_redColumn(surface) - column(diagonal, 10.0f + diagonal, 0.5f * (0.7853982f + 0.5f))) < 1.0f);
    CHECK(renderer->render(scene, 4.0f, surface) == Result::Success);
    CHECK(std::fabs(_redColumn(surface) - column(2.0f, 10.0f, 0.5f)) < 1.0f);
    CHECK(scene->seek(4.0f) == Result::Success);
    // Over the pole: the up direction turns with the eye, so the pose stays valid.
    CHECK(scene->orbit(PI, {1.0f, 0.0f, 0.0f}, 1.0f, Easing::Linear) == Result::Success);
    if (renderer->render(scene, 4.5f, surface) == Result::Success) CHECK(_redColumn(surface) > 36.0f);
    if (renderer->render(scene, 5.0f, surface) == Result::Success) CHECK(_redColumn(surface) > 36.0f);
    CHECK(near(scene->config().camera.eye.z, -10.0f, 1.0e-3f) && near(scene->config().camera.up.y, -1.0f, 1.0e-4f));

    CHECK(scene->orbit(0.0f, {0.0f, 1.0f, 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(scene->orbit(std::numeric_limits<float>::quiet_NaN(), {0.0f, 1.0f, 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->orbit(PI, {0.0f, 0.0f, 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->orbit(PI, {0.0f, std::numeric_limits<float>::infinity(), 0.0f}, 1.0f) == Result::InvalidArguments);
    CHECK(scene->orbit(PI, {0.0f, 1.0f, 0.0f}, 0.0f) == Result::InvalidArguments);
    CHECK(near(scene->duration(), 5.0f));
    delete scene;

    auto planar = Scene::gen(Config{});
    CHECK(planar);
    if (planar) {
        CHECK(planar->orbit(PI, {0.0f, 0.0f, 1.0f}, 1.0f) == Result::InsufficientCondition);
        CHECK(planar->diagnostic() != nullptr);
    }
    delete planar;
}

static Mat4 _rotationAbout(float radians, const Vec3& pivot)
{
    return Mat4::translate(pivot) * Mat4::rotateZ(radians) * Mat4::translate(pivot * -1.0f);
}

static void intentMotion(SwRenderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto pivoted = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto straight = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto swept = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto sheared = Rectangle::gen({0.0f, 0.0f, 0.0f}, {2.0f, 0.1f});
    auto arcing = Rectangle::gen({1.0f, 1.0f, 0.0f}, {0.1f, 0.1f});
    auto ring = Circle::gen({0.0f, 0.0f, 0.0f}, 1.0f);
    auto car = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.4f, 0.2f});
    auto back = Rectangle::gen({1.0f, 0.0f, 0.0f}, {0.1f, 0.1f});
    auto sun = Circle::gen({-3.0f, -1.0f, 0.0f}, 0.1f);
    auto planet = Circle::gen({-2.5f, -1.0f, 0.0f}, 0.05f);
    auto label = Text::gen("label", {0.0f, 1.5f, 0.0f});
    CHECK(scene && pivoted && straight && swept && sheared && arcing && ring && car && back && sun && planet
          && label);
    if (!scene || !pivoted || !straight || !swept || !sheared || !arcing || !ring || !car || !back || !sun
        || !planet || !label) {
        delete scene;
        delete pivoted;
        delete straight;
        delete swept;
        delete sheared;
        delete arcing;
        delete ring;
        delete car;
        delete back;
        delete sun;
        delete planet;
        delete label;
        return;
    }
    Object* objects[] = {pivoted, straight, swept, sheared, arcing, ring, car, back, sun, planet, label};
    for (auto object : objects) {
        // Fill only, so pixel bounds measure the geometry without stroke width.
        object->style.stroke.a = 0;
        object->fill(Color::hex("#ffffff"));
        CHECK(scene->add(object) == Result::Success);
    }

    // Rotating about a corner keeps the square's size and swings it on a circle. A transform
    // interpolates element-wise, so a quarter turn shrinks midway and a shear keeps the x-axis
    // in place, while an arc equal to the turn carries every point on a circle.
    auto shear = Mat4::identity();
    shear.e[1] = 1.0f;
    Animation group[] = {
        Animation::rotate(pivoted, PI * 0.5f, {-2.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}),
        Animation::transform(straight, _rotationAbout(PI * 0.5f, {2.0f, 0.0f, 0.0f})),
        Animation::shift(arcing, {-2.0f, 0.0f, 0.0f}),
        Animation::moveAlong(car, ring, true),
        Animation::moveAlong(back, ring),
        Animation::rotate(planet, PI * 2.0f, {0.0f, 0.0f, 1.0f}),
        Animation::transform(swept, _rotationAbout(PI * 0.5f, {2.0f, 0.0f, 0.0f})),
        Animation::transform(sheared, shear),
    };
    group[2].arc = PI;
    group[4].from = 1.0f;
    group[4].to = 0.5f;
    group[5].related = sun;
    group[6].arc = PI * 0.5f;
    CHECK(scene->play(group, 8u, 1.0f, Easing::Linear) == Result::Success);

    BBox bounds;
    CHECK(renderer->bounds(scene, pivoted, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.width, 28.28f, 0.6f) && near(bounds.height, 28.28f, 0.6f));
    CHECK(near(bounds.center().x, 30.0f, 0.6f) && near(bounds.center().y, 35.86f, 0.6f));
    CHECK(renderer->bounds(scene, pivoted, 1.0f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 20.0f, 0.6f) && near(bounds.center().y, 40.0f, 0.6f));
    CHECK(renderer->bounds(scene, straight, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.width, 20.0f, 0.6f) && near(bounds.center().x, 120.0f, 0.6f));
    CHECK(renderer->bounds(scene, swept, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.width, 28.28f, 0.6f) && near(bounds.center().x, 120.0f, 0.6f));
    CHECK(renderer->bounds(scene, swept, 0.25f, bounds) == Result::Success);
    CHECK(near(bounds.width, 20.0f * (std::cos(PI / 8.0f) + std::sin(PI / 8.0f)), 0.6f));
    CHECK(near(bounds.center().x, 120.0f, 0.6f) && near(bounds.center().y, 40.0f, 0.6f));
    CHECK(renderer->bounds(scene, sheared, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.height, 2.0f, 0.6f) && near(bounds.width, 41.0f, 0.6f));

    // A half-circle arc passes over the chord's midpoint at the top.
    CHECK(renderer->bounds(scene, arcing, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f, 0.6f) && near(bounds.center().y, 0.0f, 0.6f));

    // Following the circle: a quarter turn reaches the top and turns the car upright;
    // the second follower travels backwards over the last half of the outline.
    CHECK(renderer->bounds(scene, car, 0.25f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f, 0.6f) && near(bounds.center().y, 20.0f, 0.6f));
    CHECK(near(bounds.width, 4.0f, 0.6f) && near(bounds.height, 8.0f, 0.6f));
    CHECK(renderer->bounds(scene, car, 1.0f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 100.0f, 0.6f) && near(bounds.width, 8.0f, 0.6f));
    CHECK(renderer->bounds(scene, back, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 80.0f, 0.6f) && near(bounds.center().y, 60.0f, 0.6f));

    // Turning about another object's center is an orbit.
    CHECK(renderer->bounds(scene, planet, 0.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 10.0f, 0.6f) && near(bounds.center().y, 60.0f, 0.6f));

    // The outline is copied when played: moving the path afterwards keeps the old route.
    CHECK(scene->play(Animation::moveAlong(back, ring), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->play(Animation::shift(ring, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(renderer->bounds(scene, back, 1.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 60.0f, 0.6f) && near(bounds.center().y, 40.0f, 0.6f));

    // Scale about a corner keeps that corner in place.
    CHECK(scene->play(Animation::rescale(pivoted, {2.0f, 2.0f, 1.0f}, {-3.5f, -0.5f, 0.0f}), 1.0f)
          == Result::Success);
    CHECK(renderer->bounds(scene, pivoted, scene->duration(), bounds) == Result::Success);
    CHECK(near(bounds.x, 10.0f, 0.6f) && near(bounds.width, 40.0f, 0.6f));

    // Several followers share one path and the path may animate in the same group.
    Animation shared[] = {Animation::moveAlong(arcing, ring), Animation::moveAlong(car, ring, true),
                          Animation::fade(ring, 0.5f)};
    CHECK(scene->play(shared, 3u, 1.0f, Easing::Linear, 0.2f) == Result::Success);

    auto connector = Connector::gen(sun, planet);
    CHECK(connector && scene->add(connector) == Result::Success);
    CHECK(scene->play(Animation::moveAlong(car, label), 1.0f) == Result::InvalidArguments);
    CHECK(scene->diagnostic() != nullptr);
    CHECK(scene->play(Animation::moveAlong(car, car), 1.0f) == Result::InvalidArguments);
    CHECK(scene->play(Animation::rotate(car, 1.0f, {0.0f, 0.0f, 0.0f}), 1.0f) == Result::InvalidArguments);
    CHECK(scene->play(Animation::rotate(connector, 1.0f), 1.0f) == Result::InvalidArguments);
    auto wide = Animation::shift(car, {1.0f, 0.0f, 0.0f});
    wide.arc = 7.0f;
    CHECK(scene->play(wide, 1.0f) == Result::InvalidArguments);
    auto partial = Animation::moveAlong(car, ring);
    partial.to = 1.5f;
    CHECK(scene->play(partial, 1.0f) == Result::InvalidArguments);
    CHECK(scene->play(AnimationTarget::from(car).shift({0.0f, 1.0f, 0.0f}).arc(PI), 1.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(car).shift({0.0f, 1.0f, 0.0f}).arc(-7.0f), 1.0f)
          == Result::InvalidArguments);
    delete scene;
}

static bool _says(const Scene* scene, const char* text)
{
    auto diagnostic = scene->diagnostic();
    return diagnostic && std::strstr(diagnostic, text);
}

static void timelineDiagnostics()
{
    const Vec3 corners[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    auto scene = Scene::gen({});
    auto triangle = Polygon::gen(corners, 3u);
    auto square = Polygon::gen(corners, 4u);
    auto group = Group::gen();
    auto circle = Circle::gen({0.0f, 0.0f, 0.0f}, 1.0f);
    CHECK(scene && triangle && square && group && circle);
    if (!scene || !triangle || !square || !group || !circle) {
        delete scene;
        delete triangle;
        delete square;
        delete group;
        delete circle;
        return;
    }
    CHECK(scene->add(triangle) == Result::Success);
    CHECK(scene->add(square) == Result::Success);
    CHECK(scene->add(group) == Result::Success);
    CHECK(scene->add(circle) == Result::Success);

    // Layout explains missing bounds; the scene keeps the reason for the failed call.
    CHECK(group->moveTo({1.0f, 0.0f, 0.0f}) == Result::InsufficientCondition);
    CHECK(_says(scene, "no bounds yet"));
    CHECK(scene->play(Animation::morph(triangle, group), 1.0f) == Result::NonSupport);
    CHECK(_says(scene, "morph needs a line, circle, rectangle"));
    CHECK(scene->play(AnimationTarget::from(circle).opacity(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "opacity must be between 0 and 1"));
    CHECK(scene->play(Animation::fade(circle, 0.5f), -1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "positive number of seconds"));
    Animation twice[] = {Animation::fade(circle, 0.5f), Animation::shift(circle, {1.0f, 0.0f, 0.0f})};
    CHECK(scene->play(twice, 2u, 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "only once in one call"));
    CHECK(scene->play(AnimationTarget::from(circle).tip(4.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "tail and tip need"));

    // A successful call keeps working; edits after the timeline starts are explained.
    CHECK(scene->play(Animation::fade(circle, 0.5f), 1.0f) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    CHECK(circle->moveTo({2.0f, 0.0f, 0.0f}) == Result::InsufficientCondition);
    CHECK(_says(scene, "must come before the first play"));
    CHECK(scene->remove(circle) == Result::Success);
    CHECK(scene->play(Animation::fade(circle, 1.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "removed earlier"));
    CHECK(scene->remove(circle) == Result::InsufficientCondition);
    CHECK(_says(scene, "already removed"));
    delete scene;
}

static uint32_t _pixelAt(const Surface& surface, uint32_t x, uint32_t y)
{
    return surface.data() ? surface.data()[y * surface.stride() + x] : 0u;
}

static const LayoutPath* anglePath(const LayoutReport& report, const Object* object)
{
    auto visual = _layoutVisual(report, object);
    for (auto i = 0u; i < report.pathCount(); i++) {
        auto path = report.pathAt(i);
        if (path && report.visualAt(path->visual) == visual) return path;
    }
    return nullptr;
}

static void arcsAndAngles(SwRenderer* renderer)
{
    constexpr auto pi = 3.14159265359f;
    auto arc = Arc::gen({1.0f, 2.0f, 0.0f}, 2.0f, 0.0f, pi * 0.5f);
    CHECK(arc && Arc::gen({}, -1.0f, 0.0f, pi) == nullptr);
    CHECK(Arc::gen({}, 1.0f, 0.0f, 7.0f) == nullptr);
    Bounds bounds;
    if (arc) {
        CHECK(arc->style.fill.a == 0u && arc->bounds(bounds));
        CHECK(near(bounds.min.x, 1.0f) && near(bounds.max.x, 3.0f)
              && near(bounds.min.y, 2.0f) && near(bounds.max.y, 4.0f));
        arc->start = 0.2f; arc->angle = -pi;
        CHECK(arc->bounds(bounds) && near(bounds.min.y, 0.0f));
        arc->sector = true;
        CHECK(arc->bounds(bounds) && bounds.min.x <= arc->center.x && bounds.max.y >= arc->center.y);
        delete arc;
    }
    Config config;
    config.width = 200; config.height = 100; config.camera.orthoHeight = 10.0f;
    auto scene = Scene::gen(config);
    arc = Arc::gen({}, 2.0f, 0.0f, pi * 0.5f);
    auto angle = Angle::gen({}, {-1.0f, 0.2f, 0.0f}, {-1.0f, -0.2f, 0.0f}, 1.0f);
    auto label = Point::gen({});
    CHECK(scene && arc && angle && label);
    if (!scene || !arc || !angle || !label) { delete scene; delete arc; delete angle; delete label; return; }
    angle->right = RightAngle::Never; angle->labelDistance = 2.0f;
    CHECK(near(angle->labelPoint().x, -2.0f) && near(angle->labelPoint().y, 0.0f));
    CHECK(angle->add(label) == Result::Success);
    CHECK(scene->add(arc) == Result::Success && scene->add(angle) == Result::Success);
    Vec3 center[] = {{2.0f, 0.0f, 0.0f}};
    Vec3 rays[] = {{1.0f, 0.0f, 0.0f}, {0.0f, 0.2f, 0.0f}, {0.0f, -0.2f, 0.0f}};
    AnimationTarget changes[] = {AnimationTarget::from(arc).geometry(center, 1u).radius(4.0f).start(pi * 0.5f).angle(pi),
                                 AnimationTarget::from(angle).geometry(rays, 3u)};
    CHECK(scene->play(changes, 2u, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    LayoutReport report;
    CHECK(renderer->layout(scene, 0.5f, report) == Result::Success);
    auto path = anglePath(report, arc);
    CHECK(path && path->count > 2u && !path->closed);
    if (path) {
        // Center (1, 0), radius 3, start pi/4, sweep 3pi/4 at the midpoint.
        CHECK(near(path->points[0].x, 110.0f + 30.0f / std::sqrt(2.0f), 0.001f));
        CHECK(near(path->points[0].y, 50.0f - 30.0f / std::sqrt(2.0f), 0.001f));
        CHECK(near(path->points[path->count - 1u].x, 80.0f, 0.001f));
        for (auto i = 0u; i < path->count; i++) {
            auto x = path->points[i].x - 110.0f; auto y = path->points[i].y - 50.0f;
            CHECK(near(std::hypot(x, y), 30.0f, 0.001f));
        }
    }
    path = anglePath(report, angle);
    CHECK(path && path->count > 3u);
    if (path) {
        for (auto i = 0u; i < path->count; i++) CHECK(path->points[i].x < 100.0f);
    }
    BBox labelBounds;
    CHECK(renderer->bounds(scene, label, 0.5f, labelBounds) == Result::Success);
    CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 85.0f, 0.001f));
    // Sampling backwards gives the original vertex and label rather than the stored final state.
    CHECK(renderer->bounds(scene, label, 0.0f, labelBounds) == Result::Success);
    CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 80.0f, 0.001f));
    Object* copy = nullptr;
    CHECK(scene->clone(angle, copy) == Result::Success && copy && copy->type() == Type::Angle);
    if (copy) {
        CHECK(copy->childCount() == 1u);
        CHECK(renderer->bounds(scene, copy->childAt(0), 1.0f, labelBounds) == Result::Success);
        CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 90.0f, 0.001f));
    }
    CHECK(scene->save(arc) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(arc).radius(2.0f).start(0.0f).angle(pi * 0.5f), 0.5f) == Result::Success);
    CHECK(scene->restore(arc, 0.5f) == Result::Success);
    CHECK(near(arc->radius, 4.0f) && near(arc->start, pi * 0.5f) && near(arc->angle, pi));
    delete scene;

    const bool sides[] = {false, true};
    const float offsets[] = {-0.001f, 0.001f};
    for (auto other : sides) {
        for (auto side : offsets) {
            scene = Scene::gen(config);
            auto first = Vec3{-1.0f, side, 0.0f};
            auto second = Vec3{-1.0f, -0.2f, 0.0f};
            // Reverse one pair so side selection also handles a raw sweep over pi.
            angle = side < 0.0f ? Angle::gen({}, second, first) : Angle::gen({}, first, second);
            angle->right = RightAngle::Never; angle->other = other;
            CHECK(scene->add(angle) == Result::Success);
            CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
            path = anglePath(report, angle);
            CHECK(path && path->count > 2u);
            if (path) {
                auto crossesRight = false;
                for (auto i = 0u; i < path->count; i++) crossesRight |= path->points[i].x > 100.0f;
                CHECK(crossesRight == other);
            }
            delete scene;
        }
    }
    scene = Scene::gen(config);
    angle = Angle::gen({}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    CHECK(scene->add(angle) == Result::Success);
    CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
    path = anglePath(report, angle);
    CHECK(path && path->count == 3u && near(path->points[1].x, 110.0f) && near(path->points[1].y, 40.0f));
    delete scene;

    scene = Scene::gen(config);
    arc = Arc::gen({}, 1.0f, 0.0f, pi); arc->sector = true; arc->fill(Color::hex("#ffffff"));
    auto follower = Point::gen({});
    CHECK(scene->add(arc) == Result::Success && scene->add(follower) == Result::Success);
    CHECK(scene->play(Animation::create(arc), 0.5f, Easing::Linear) == Result::Success);
    CHECK(scene->play(Animation::moveAlong(follower, arc), 1.0f, Easing::Linear) == Result::Success);
    CHECK(renderer->layout(scene, 1.5f, report) == Result::Success);
    path = anglePath(report, arc);
    CHECK(path && path->closed && path->filled);
    CHECK(renderer->bounds(scene, follower, 1.5f, labelBounds) == Result::Success);
    CHECK(near(labelBounds.x + labelBounds.width * 0.5f, 100.0f, 0.001f));
    auto flash = Emphasis::passingFlash(arc); flash.window = 0.2f;
    CHECK(scene->emphasize(flash, 0.5f) == Result::Success);
    CHECK(renderer->layout(scene, 1.75f, report) == Result::Success);
    delete scene;
}

static void mathSymbolFallback(SwRenderer* renderer)
{
    CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
    Config config; config.width = 400; config.height = 100; config.camera.orthoHeight = 100.0f;
    auto scene = Scene::gen(config);
    auto text = Text::gen("\xe2\x88\xa0" "APB = 35\xc2\xb0", {});
    text->size = 21.0f;
    CHECK(text->font("Pretendard") == Result::Success);
    CHECK(scene->add(text) == Result::Success);
    TextExtent full; TextExtent without; TextExtent symbols;
    CHECK(scene->measure(text, full) == Result::Success && full.missing == 0u);
    CHECK(scene->measure(text, "APB = 35\xc2\xb0", without) == Result::Success && full.width > without.width + 10.0f);
    CHECK(scene->measure(text, "\xe2\x88\xa0\xc2\xb0\xe2\x8a\xa5\xe2\x88\xa5\xe2\x96\xb3\xe2\x89\x85\xe2\x88\xbc\xcf\x80\xce\xb8", symbols)
          == Result::Success && symbols.missing == 0u && symbols.width > 100.0f);
    Surface surface; CHECK(surface.resize(400, 100) == Result::Success);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    auto first = checksum(surface);
    CHECK(scene->play(AnimationTarget::from(text).text("APB = 35\xc2\xb0"), 1.0f) == Result::Success);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success && checksum(surface) != first);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success && checksum(surface) == first);
    delete scene;
}

static void shapeProperties(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto black = pixel(config.background);
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto yellow = Color::hex("#ffff00");
    auto white = Color::hex("#ffffff");
    auto none = Color{0, 0, 0, 0};

    // 20 pixels per unit with the origin at pixel (80, 40).
    auto scene = Scene::gen(config);
    auto circle = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto box = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto line = Line::gen({-3.5f, -1.75f, 0.0f}, {3.5f, -1.75f, 0.0f});
    auto dot = Point::gen({0.0f, 1.5f, 0.0f});
    Object* objects[] = {circle, box, line, dot};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    circle->stroke(red);
    circle->fill(red);
    box->stroke(none);
    box->fill(blue);
    line->stroke(yellow);
    dot->stroke(white);
    dot->fill(white);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);
    AnimationTarget targets[] = {
        AnimationTarget::from(circle).radius(1.5f),
        AnimationTarget::from(box).size(Vec2{3.0f, 2.0f}).corner(0.5f),
        AnimationTarget::from(line).width(10.0f),
        AnimationTarget::from(dot).radius(9.0f),
    };
    CHECK(scene->play(targets, 4u, 1.0f, Easing::Linear) == Result::Success);
    CHECK(near(circle->radius, 1.5f) && near(dot->style.radius, 9.0f) && near(line->style.width, 10.0f));
    CHECK(near(box->size.x, 3.0f) && near(box->size.y, 2.0f) && near(box->corner, 0.5f));

    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 55u, 40u) == black);
    CHECK(_pixelAt(surface, 138u, 40u) == black);
    CHECK(_pixelAt(surface, 80u, 71u) == black);
    CHECK(_pixelAt(surface, 87u, 10u) == black);
    // Halfway the circle's radius is 1, the rectangle is 2 wide and the line 6 pixels thick.
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 55u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 65u, 40u) == black);
    CHECK(_pixelAt(surface, 138u, 40u) == pixel(blue));
    CHECK(_pixelAt(surface, 80u, 71u) == black);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 65u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 80u, 71u) == pixel(yellow));
    CHECK(_pixelAt(surface, 87u, 10u) == pixel(white));
    // The rounded corner leaves the rectangle's corner pixel empty.
    CHECK(_pixelAt(surface, 146u, 24u) == pixel(blue));
    CHECK(_pixelAt(surface, 149u, 21u) == black);
    // Size and corner are separate tracks, so they may overlap in time.
    auto mark = scene->cursor();
    CHECK(scene->play(AnimationTarget::from(box).size(Vec2{2.0f, 2.0f}), 1.0f) == Result::Success);
    CHECK(scene->seek(mark) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(box).corner(0.25f), 1.0f) == Result::Success);
    CHECK(scene->seek(mark + 0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(box).size(Vec2{1.0f, 1.0f}), 1.0f) == Result::InsufficientCondition);
    CHECK(scene->seek(mark + 1.0f) == Result::Success);

    // Values that cannot reach the shape are explained.
    CHECK(scene->play(AnimationTarget::from(line).radius(1.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "radius needs a circle"));
    CHECK(scene->play(AnimationTarget::from(circle).size(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "size needs a rectangle"));
    CHECK(scene->play(AnimationTarget::from(box).corner(1.5f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "half the rectangle's shorter side"));
    CHECK(scene->play(AnimationTarget::from(box).size(Vec2{0.4f, 2.0f}), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "half the rectangle's shorter side"));
    CHECK(scene->play(AnimationTarget::from(line).width(-1.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "stroke width"));
    CHECK(scene->play(AnimationTarget::from(box).gradient(white), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keep the current gradient's type"));
    delete scene;

    // A connector meets the sampled edge of a growing shape, not its final size.
    auto linked = Scene::gen(config);
    auto source = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto sink = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto link = source && sink ? Connector::gen(source, sink) : nullptr;
    CHECK(linked && source && sink && link);
    if (linked && source && sink && link) {
        source->stroke(none);
        sink->stroke(none);
        link->stroke(white);
        link->strokeWidth(4.0f);
        CHECK(linked->add(source) == Result::Success);
        CHECK(linked->add(sink) == Result::Success);
        CHECK(linked->add(link) == Result::Success);
        CHECK(linked->play(AnimationTarget::from(source).radius(1.5f), 1.0f, Easing::Linear) == Result::Success);
        CHECK(renderer->render(linked, 0.0f, surface) == Result::Success);
        CHECK(_pixelAt(surface, 60u, 40u) == pixel(white));
        CHECK(renderer->render(linked, 1.0f, surface) == Result::Success);
        CHECK(_pixelAt(surface, 60u, 40u) == black);
        CHECK(_pixelAt(surface, 80u, 40u) == pixel(white));
    } else {
        delete link;
        delete source;
        delete sink;
    }
    delete linked;

    // Text size, gradient stops and a group's width.
    auto styled = Scene::gen(config);
    auto bar = Rectangle::gen({0.0f, 1.0f, 0.0f}, {6.0f, 1.0f});
    auto ramp = Rectangle::gen({0.0f, -0.5f, 0.0f}, {6.0f, 0.5f});
    auto group = Group::gen();
    auto heir = Line::gen({-3.0f, -1.5f, 0.0f}, {3.0f, -1.5f, 0.0f});
    auto own = Line::gen({-3.0f, -1.8f, 0.0f}, {3.0f, -1.8f, 0.0f});
    auto label = Text::gen("size", {0.0f, 10.0f, 0.0f});
    Object* parts[] = {bar, ramp, group, heir, own, label};
    created = styled != nullptr;
    for (auto object : parts) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : parts) delete object;
        delete styled;
        return;
    }
    Gradient stops;
    stops.type = GradientType::Linear;
    stops.stopCount = 2;
    stops.stops[0] = {0.0f, red};
    stops.stops[1] = {1.0f, blue};
    bar->stroke(none);
    bar->fill(white);
    CHECK(bar->gradient(stops) == Result::Success);
    ramp->stroke(none);
    ramp->fill(red);
    ramp->gradient(blue);
    own->strokeWidth(1.0f);
    CHECK(group->add(heir) == Result::Success);
    CHECK(group->add(own) == Result::Success);
    label->size = 20.0f;
    Object* roots[] = {bar, ramp, group, label};
    for (auto object : roots) CHECK(styled->add(object) == Result::Success);
    auto shifted = stops;
    shifted.stops[0].color = Color::hex("#00ff00");
    shifted.stops[1].color = yellow;
    AnimationTarget looks[] = {
        AnimationTarget::from(bar).gradient(shifted),
        AnimationTarget::from(ramp).gradient(Color::hex("#00ff00")),
        AnimationTarget::from(group).width(8.0f),
        AnimationTarget::from(label).size(60.0f),
    };
    CHECK(styled->play(looks, 4u, 1.0f, Easing::Linear) == Result::Success);
    CHECK(styled->diagnostic() == nullptr);
    CHECK(same(bar->style.gradient.stops[1].color, yellow) && same(ramp->style.gradientEnd, Color::hex("#00ff00")));
    CHECK(near(heir->style.width, 8.0f) && near(own->style.width, 1.0f) && near(label->size, 60.0f));
    CHECK(renderer->render(styled, 0.5f, surface) == Result::Success);
    // Red and green meet halfway at the left end of the bar.
    auto left = _pixelAt(surface, 21u, 20u);
    CHECK((left & 0xffu) > 110u && (left & 0xffu) < 145u);
    CHECK(((left >> 8u) & 0xffu) > 110u && ((left >> 8u) & 0xffu) < 145u && ((left >> 16u) & 0xffu) < 20u);
#ifdef VIS_TEST_FONT
    BBox small;
    BBox large;
    CHECK(renderer->bounds(styled, label, 0.0f, small) == Result::Success);
    CHECK(renderer->bounds(styled, label, 1.0f, large) == Result::Success);
    CHECK(large.width > small.width * 2.5f && large.height > small.height * 2.5f);
#endif

    // A gradient with stops owns the colors, so only their alpha may change.
    CHECK(styled->play(AnimationTarget::from(bar).fill(red), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "a gradient with stops sets this shape's colors"));
    CHECK(styled->play(Animation::stroke(bar, red), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "a gradient with stops sets this shape's colors"));
    CHECK(styled->play(AnimationTarget::from(bar).gradient(red), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "an end color animates a two-stop gradient"));
    auto three = shifted;
    three.stopCount = 3;
    three.stops[2] = {1.0f, white};
    CHECK(styled->play(AnimationTarget::from(bar).gradient(three), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "stop count"));
    CHECK(styled->play(AnimationTarget::from(label).width(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "no stroke width"));
    CHECK(styled->play(AnimationTarget::from(label).size(Vec2{10.0f, 20.0f}), 1.0f) == Result::InvalidArguments);
    CHECK(_says(styled, "one positive size"));
    CHECK(styled->play(AnimationTarget::from(bar).fill(Color{255, 255, 255, 128}), 1.0f) == Result::Success);
    delete styled;
}

// Geometry clips carry the points that define a shape point by point, and leave the shape with its
// new points for whatever is authored next.
static void trimmedSegments(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    auto scene = Scene::gen(config);
    auto line = Line::gen({-4.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f});
    auto arrow = Arrow::gen({-4.0f, 2.0f, 0.0f}, {4.0f, 2.0f, 0.0f});
    auto vector = Vector::gen({8.0f, 0.0f, 0.0f}, {-4.0f, -2.0f, 0.0f});
    CHECK(scene && line && arrow && vector);
    if (!scene || !line || !arrow || !vector) {
        delete scene;
        delete line;
        delete arrow;
        delete vector;
        return;
    }
    line->buff = {1.0f, 2.0f};
    arrow->buff = vector->buff = {1.0f, 1.0f};
    Bounds own;
    CHECK(line->bounds(own) && near(own.min.x, -3.0f) && near(own.max.x, 2.0f));
    CHECK(arrow->bounds(own) && near(own.min.x, -3.0f) && near(own.max.x, 3.0f));
    CHECK(vector->bounds(own) && near(own.min.x, -3.0f) && near(own.max.x, 3.0f));
    Object* objects[] = {line, arrow, vector};
    for (auto object : objects) {
        object->stroke(Color::hex("#ffffff"));
        object->strokeWidth(2.0f);
        CHECK(scene->add(object) == Result::Success);
    }
    CHECK(scene->save(line) == Result::Success);
    Vec3 raised[] = {{-4.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(line).geometry(raised, 2u), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(line).buff(Vec2{2.0f, 1.0f}), 1.0f, Easing::Linear) == Result::Success);
    BBox box;
    CHECK(renderer->bounds(scene, line, 0.5f, box) == Result::Success);
    auto length = std::sqrt(73.0f);
    CHECK(near(box.x, 59.0f + 120.0f / length, 0.01f));
    CHECK(near(box.width, 82.0f - 240.0f / length, 0.01f));
    CHECK(near(box.height, 32.0f - 90.0f / length, 0.01f));
    Object* copy = nullptr;
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->clone(line, copy) == Result::Success);
    if (copy) {
        auto twin = static_cast<Line*>(copy);
        CHECK(near(twin->buff.x, 1.5f) && near(twin->buff.y, 1.5f));
        CHECK(near(twin->to.y, 3.0f));
    }
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->restore(line, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(near(line->buff.x, 1.0f) && near(line->buff.y, 2.0f));
    CHECK(scene->update(AnimationTarget::from(line).buff(-1.0f)) == Result::InvalidArguments);
    CHECK(scene->update(AnimationTarget::from(line).buff(20.0f)) == Result::Success);
    CHECK(!line->bounds(own));
    CHECK(renderer->bounds(scene, line, 2.0f, box) == Result::InsufficientCondition);
    delete scene;

    // Equality and over-trimming suppress round caps and arrow markers as well as the shaft.
    for (auto kind = 0; kind < 3; kind++) {
        auto empty = Scene::gen(config);
        Object* object = kind == 0 ? static_cast<Object*>(Line::gen({-1, 0, 0}, {1, 0, 0}))
                       : kind == 1 ? static_cast<Object*>(Arrow::gen({-1, 0, 0}, {1, 0, 0}))
                                   : static_cast<Object*>(Vector::gen({2, 0, 0}, {-1, 0, 0}));
        CHECK(empty && object);
        if (!empty || !object) { delete empty; delete object; continue; }
        CHECK(empty->add(object) == Result::Success);
        CHECK(empty->update(AnimationTarget::from(object).buff(kind == 0 ? 1.0f : 3.0f)) == Result::Success);
        Surface surface;
        CHECK(renderer->render(empty, 0.0f, surface) == Result::Success);
        auto black = pixel(config.background);
        for (auto i = 0u; i < surface.width() * surface.height(); i++) CHECK(surface.data()[i] == black);
        delete empty;
    }
}

static void zeroLengthArrows(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 5.0f;
    config.background = Color::hex("#000000");
    CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);

    for (auto kind = 0; kind < 2; kind++) {
        for (auto degenerate = 0; degenerate < 4; degenerate++) {
            auto scene = Scene::gen(config);
            auto length = degenerate == 0 ? 0.0f : degenerate == 1 ? 0.000004f : 2.0f;
            Object* object = kind == 0 ? static_cast<Object*>(Arrow::gen({}, {length, 0, 0}))
                                      : static_cast<Object*>(Vector::gen({length, 0, 0}));
            CHECK(scene && object);
            if (!scene || !object) { delete scene; delete object; continue; }
            object->stroke(Color::hex("#ffffff"), 4.0f);
            CHECK(scene->add(object) == Result::Success);
            if (degenerate >= 2)
                CHECK(scene->update(AnimationTarget::from(object).buff(degenerate == 2 ? 1.0f : 2.0f)) == Result::Success);
            Surface surface;
            CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
            CHECK(changed(surface, config.background) == 0u);
            LayoutReport report;
            CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
            CHECK(report.objectCount() == 1u);
            if (report.objectCount() == 1u) {
                auto own = report.objectAt(0);
                CHECK(!own->visible && !own->familyVisible);
                CHECK(own->paintBounds.width == 0.0f && own->paintBounds.height == 0.0f);
            }
            BBox bounds;
            CHECK(renderer->bounds(scene, object, 0.0f, bounds) == Result::InsufficientCondition);
            delete scene;
        }
    }

    // Subpixel arrows can still paint at fractional origins and in mounted scenes.
    for (auto height : {5.0f, 9.0f}) {
        config.camera.orthoHeight = height;
        auto scale = static_cast<float>(config.height) / height;
        for (auto offset : {-0.51f, 0.0f, 0.51f}) {
            for (auto length : {0.00008f, 0.015624f, 0.03125f}) {
                for (auto direction : {0.0f, 1.0f}) {
                    for (auto mode : {0, 1, 2}) {
                        auto scene = Scene::gen(config);
                        auto arrow = Arrow::gen({offset / scale, offset / scale, 0},
                                               {(offset + length) / scale, (offset + length * direction) / scale, 0});
                        CHECK(scene && arrow);
                        if (!scene || !arrow) { delete scene; delete arrow; continue; }
                        arrow->stroke(Color::hex("#ffffff"), 4.0f);
                        CHECK(scene->add(arrow) == Result::Success);
                        Surface surface;
                        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
                        auto nativePainted = changed(surface, config.background) > 0u;
                        if (mode == 1) {
                            SceneView view;
                            view.viewport = {0.5f, 0.0f, 0.5f, 0.5f};
                            view.camera.orthoHeight = height * 0.25f;
                            CHECK(scene->view(view) == Result::Success);
                        } else if (mode == 2) {
                            auto parent = Scene::gen(config);
                            CHECK(parent);
                            if (!parent) { delete scene; continue; }
                            CHECK(parent->viewport(scene, {0.5f, 0.0f, 0.5f, 0.5f}) == Result::Success);
                            scene = parent;
                        }
                        CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
                        LayoutReport report;
                        CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
                        auto own = report.objectAt(0);
                        // SceneView redraws have scene records rather than own object bounds.
                        auto painted = mode == 1 ? nativePainted : changed(surface, config.background) > 0u;
                        CHECK(own && own->visible == painted);
                        delete scene;
                    }
                }
            }
        }
    }
    config.camera.orthoHeight = 5.0f;

    // A transformed vector crosses zero with a float residual while its label keeps drawing.
    auto scene = Scene::gen(config);
    auto vector = Vector::gen({2, 1, 0});
    auto label = Text::gen("v", {0, -1, 0});
    CHECK(scene && vector && label);
    if (!scene || !vector || !label) { delete scene; delete vector; delete label; return; }
    label->font("Pretendard");
    vector->model = Mat4::translate({-1, 0, 0}) * Mat4::rotateZ(0.5f);
    vector->stroke(Color::hex("#ff0000"), 4.0f);
    CHECK(vector->label(label) == Result::Success);
    CHECK(scene->add(vector) == Result::Success);
    const Vec3 reversed[] = {{-1.999992f, -1, 0}, {0, 0, 0}};
    CHECK(scene->play(AnimationTarget::from(vector).geometry(reversed, 2u), 2.0f, Easing::Linear) == Result::Success);
    for (auto time : {0.99f, 1.0f, 1.01f}) {
        LayoutReport report;
        CHECK(renderer->layout(scene, time, report) == Result::Success);
        auto own = report.objectAt(0);
        auto child = report.objectAt(1);
        CHECK(own && child && own->object == vector && child->object == label);
        if (!own || !child) continue;
        CHECK(own->visible == (time != 1.0f));
        CHECK(own->familyVisible && child->visible);
        if (time == 1.0f) {
            CHECK(own->paintBounds.width == 0.0f && own->paintBounds.height == 0.0f);
            CHECK(near(own->familyBounds.x, child->paintBounds.x));
            CHECK(near(own->familyBounds.width, child->paintBounds.width));
            CHECK(near(own->familyBounds.height, child->paintBounds.height));
            BBox bounds;
            CHECK(renderer->bounds(scene, vector, time, bounds) == Result::Success);
            CHECK(near(bounds.x, child->paintBounds.x) && near(bounds.width, child->paintBounds.width));
        }
        Surface surface;
        CHECK(renderer->render(scene, time, surface) == Result::Success);
        auto red = 0u;
        for (auto y = 0u; y < surface.height(); y++) {
            for (auto x = 0u; x < surface.width(); x++) {
                auto value = surface.data()[y * surface.stride() + x];
                if ((value & 0xffu) > ((value >> 8u) & 0xffu)) red++;
            }
        }
        CHECK((red > 0u) == (time != 1.0f));
    }
    delete scene;
}

static void geometryClips(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto black = pixel(config.background);
    auto yellow = Color::hex("#ffff00");
    auto cyan = Color::hex("#00ffff");

    // 20 pixels per unit with the origin at pixel (80, 40).
    auto scene = Scene::gen(config);
    auto line = Line::gen({-3.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f});
    auto curve = Curve::gen({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, 30u);
    const Vec3 samples[] = {{-3.0f, 1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    auto plot = Plot::gen(samples, 3u);
    auto vector = Vector::gen({1.0f, 0.0f, 0.0f}, {0.0f, -1.5f, 0.0f});
    auto circle = Circle::gen({3.0f, 1.5f, 0.0f}, 0.25f);
    Object* objects[] = {line, curve, plot, vector, circle};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    line->stroke(yellow);
    line->strokeWidth(4.0f);
    curve->stroke(cyan);
    curve->strokeWidth(4.0f);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);

    // Before the timeline the points become the starting geometry.
    const Vec3 lowered[] = {{-3.0f, -1.5f, 0.0f}, {-1.0f, -1.5f, 0.0f}};
    CHECK(scene->update(AnimationTarget::from(line).geometry(lowered, 2u)) == Result::Success);
    CHECK(near(line->from.y, -1.5f) && near(line->to.y, -1.5f));

    // The line rises from y = -1.5 to y = 0.5 after a second, and the curve bends upward.
    CHECK(scene->wait(1.0f) == Result::Success);
    const Vec3 raised[] = {{-3.0f, 0.5f, 0.0f}, {-1.0f, 0.5f, 0.0f}};
    const Vec3 bent[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, {2.0f, 2.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
    const Vec3 turned[] = {{0.0f, 1.0f, 0.0f}, {0.0f, -1.5f, 0.0f}};
    AnimationTarget targets[] = {
        AnimationTarget::from(line).geometry(raised, 2u),
        AnimationTarget::from(curve).geometry(bent, 4u),
        AnimationTarget::from(vector).geometry(turned, 2u),
    };
    CHECK(scene->play(targets, 3u, 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->diagnostic() == nullptr);
    // The objects hold the geometry their clips end at.
    CHECK(near(line->from.y, 0.5f) && near(line->to.y, 0.5f));
    CHECK(near(curve->controls()[1].y, 2.0f) && near(curve->controls()[2].x, 2.0f));
    CHECK(curve->count() == 31u && near(curve->points()[15].x, 1.5f) && near(curve->points()[15].y, 1.5f));
    CHECK(near(vector->value.y, 1.0f) && near(vector->value.x, 0.0f) && near(vector->origin.y, -1.5f));

    Surface surface;
    // Until the clips begin the shapes keep the geometry they started with.
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 70u) == pixel(yellow));
    CHECK(_pixelAt(surface, 40u, 30u) == black);
    CHECK(_pixelAt(surface, 110u, 40u) == pixel(cyan));
    CHECK(_pixelAt(surface, 110u, 25u) == black);
    // Halfway the line is at y = -0.5 and the middle of the curve at y = 0.75.
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 70u) == black);
    CHECK(_pixelAt(surface, 40u, 50u) == pixel(yellow));
    CHECK(_pixelAt(surface, 110u, 40u) == black);
    CHECK(_pixelAt(surface, 110u, 25u) == pixel(cyan));
    BBox half;
    BBox full;
    CHECK(renderer->bounds(scene, curve, 1.5f, half) == Result::Success);
    CHECK(renderer->bounds(scene, curve, 2.0f, full) == Result::Success);
    CHECK(half.height > 14.0f && half.height < 20.0f && full.height > 29.0f && full.height < 35.0f);
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 30u) == pixel(yellow));
    CHECK(_pixelAt(surface, 110u, 10u) == pixel(cyan));

    // A second change of the same geometry waits for the first, but a move may run with it.
    auto mark = scene->cursor();
    const Vec3 flat[] = {{-3.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(line).geometry(flat, 2u), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(mark) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(line).shift({0.0f, -0.5f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(mark + 0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(line).geometry(raised, 2u), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "already animates its geometry until 3.00 s"));
    CHECK(scene->seek(mark + 1.0f) == Result::Success);
    // The line ends flat at y = 0 and moved down by 0.5.
    CHECK(renderer->render(scene, 3.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 50u) == pixel(yellow));
    CHECK(_pixelAt(surface, 40u, 40u) == black);

    // After the timeline starts an update switches the geometry at the cursor.
    const Vec3 short_[] = {{-3.0f, 0.0f, 0.0f}, {-2.5f, 0.0f, 0.0f}};
    CHECK(scene->update(AnimationTarget::from(line).geometry(short_, 2u)) == Result::Success);
    CHECK(near(line->to.x, -2.5f));
    CHECK(scene->wait(0.5f) == Result::Success);
    CHECK(renderer->render(scene, 2.99f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 50u) == pixel(yellow));
    CHECK(renderer->render(scene, 3.2f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 50u) == black);
    CHECK(_pixelAt(surface, 25u, 50u) == pixel(yellow));

    // A restore carries the geometry back to the one the save kept.
    CHECK(scene->save(curve) == Result::Success);
    const Vec3 dipped[] = {{0.0f, 0.0f, 0.0f}, {1.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(curve).geometry(dipped, 4u), 1.0f) == Result::Success);
    CHECK(near(curve->points()[15].y, -1.5f));
    CHECK(scene->restore(curve, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
    CHECK(near(curve->points()[15].y, 1.5f) && near(curve->controls()[1].y, 2.0f));
    auto end = scene->cursor();
    CHECK(renderer->render(scene, end - 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 70u) == pixel(cyan));
    CHECK(renderer->render(scene, end, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 10u) == pixel(cyan));
    CHECK(_pixelAt(surface, 110u, 70u) == black);
    // Repeating the geometry the shape has keeps it as it is.
    CHECK(scene->play(AnimationTarget::from(curve).geometry(bent, 4u), 0.5f) == Result::Success);
    CHECK(renderer->render(scene, end + 0.25f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 10u) == pixel(cyan));

    // Geometry that cannot reach the shape is explained.
    const Vec3 two[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(plot).geometry(two, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keep the object's point count"));
    CHECK(scene->play(AnimationTarget::from(curve).geometry(two, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "4 for a curve"));
    CHECK(scene->play(AnimationTarget::from(circle).geometry(two, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "geometry needs a line"));
    const Vec3 broken[] = {{0.0f, 0.0f, 0.0f}, {std::numeric_limits<float>::infinity(), 0.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(line).geometry(broken, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "must be finite"));
    const Vec3 points[] = {{-3.0f, 1.0f, 0.0f}, {-2.0f, 1.5f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(plot).geometry(points, 3u), 1.0f) == Result::Success);
    CHECK(near(plot->points()[1].y, 1.5f));
    delete scene;

    // In the 3D view a surface hides a stroke where its points on their way pass behind it, not
    // where the points it started or ends with lie. Seen from above, a line along y = 0 and a plot
    // along y = 0.5 tilt from z = 1 over the plane z = 0 to z = 2x - 1: before the clips both draw
    // whole over the plane; halfway they are z = x, hidden at x = -0.5 and shown at x = 0.3, where
    // the end would hide them.
    auto view = topView();
    auto plate = Color::hex("#2050c0");
    auto white = Color::hex("#ffffff");
    auto tilted = Scene::gen(view);
    auto plane = depthPlate(0.0f, plate);
    auto beam = Line::gen({-2.0f, 0.0f, 1.0f}, {2.0f, 0.0f, 1.0f});
    const Vec3 level[] = {{-2.0f, 0.5f, 1.0f}, {0.0f, 0.5f, 1.0f}, {2.0f, 0.5f, 1.0f}};
    auto trace = Plot::gen(level, 3u);
    Object* parts[] = {plane, beam, trace};
    auto complete = tilted != nullptr;
    for (auto part : parts) complete = complete && part;
    CHECK(complete);
    if (!complete) {
        for (auto part : parts) delete part;
        delete tilted;
        return;
    }
    beam->stroke(white, 4.0f);
    trace->stroke(white, 4.0f);
    for (auto part : parts) CHECK(tilted->add(part) == Result::Success);
    const Vec3 slant[] = {{-2.0f, 0.0f, -5.0f}, {2.0f, 0.0f, 3.0f}};
    const Vec3 slope[] = {{-2.0f, 0.5f, -5.0f}, {0.0f, 0.5f, -1.0f}, {2.0f, 0.5f, 3.0f}};
    AnimationTarget tilts[] = {AnimationTarget::from(beam).geometry(slant, 2u),
                               AnimationTarget::from(trace).geometry(slope, 3u)};
    CHECK(tilted->play(tilts, 2u, 1.0f, Easing::Linear) == Result::Success);
    // 40 pixels per unit with the origin at pixel (100, 100): the line on row 100, the plot on 80.
    const uint32_t rows[] = {100u, 80u};
    Surface frame;
    CHECK(renderer->render(tilted, 0.0f, frame) == Result::Success);
    if (frame.data()) {
        for (auto row : rows) CHECK(close(framePixel(frame, 80u, row), white, 8));
    }
    CHECK(renderer->render(tilted, 0.5f, frame) == Result::Success);
    if (frame.data()) {
        for (auto row : rows) {
            CHECK(close(framePixel(frame, 80u, row), plate, 8));
            CHECK(close(framePixel(frame, 112u, row), white, 8));
            CHECK(close(framePixel(frame, 40u, row), white, 8));
        }
    }
    CHECK(renderer->render(tilted, 1.0f, frame) == Result::Success);
    if (frame.data()) {
        for (auto row : rows) {
            CHECK(close(framePixel(frame, 112u, row), plate, 8));
            CHECK(close(framePixel(frame, 130u, row), white, 8));
        }
    }
    delete tilted;
}

// Function columns keep x fixed and blend y before clipping, including as pieces leave or
// enter the band. A copy retains the sampled column and discontinuity gaps.
static void plotFunctionPlay(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    const Vec3 zero[] = {{-2, 0, 0}, {-1, 0, 0}, {0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    const Vec3 square[] = {{-2, 8, 0}, {-1, 2, 0}, {0, 0, 0}, {1, 2, 0}, {2, 8, 0}};
    const Vec3 cut[] = {{-0.5f, 1, 0}, {0, 0, 0}, {0.5f, 1, 0}};
    PlotSamples from{zero, 5u, true, {-1, 1}}, to{square, 5u, true, {-1, 1}};
    auto scene = Scene::gen(config);
    auto plot = Plot::gen(zero, 5u, nullptr, 1u, &from);
    CHECK(scene && plot && scene->add(plot) == Result::Success);
    if (!scene || !plot) { delete scene; return; }
    Object* initial = nullptr;
    CHECK(scene->clone(plot, initial) == Result::Success);
    CHECK(initial && static_cast<Plot*>(initial)->samples().count == 5u);
    CHECK(scene->play(AnimationTarget::from(plot).geometry(cut, 3u, nullptr, 1u, &to), 1, linear) == Result::Success);
    auto copyAt = [&](float time) {
        CHECK(scene->seek(time) == Result::Success);
        Object* copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        return copy ? static_cast<Plot*>(copy) : nullptr;
    };
    auto quarter = copyAt(0.25f);
    CHECK(quarter && quarter->count() == 5u && quarter->contours() == 1u);
    if (quarter && quarter->count() == 5u) {
        CHECK(near(quarter->points()[0].x, -4.0f / 3.0f) && near(quarter->points()[0].y, 1));
        CHECK(near(quarter->points()[1].x, -1) && near(quarter->points()[1].y, 0.5f));
        CHECK(near(quarter->points()[2].x, 0) && near(quarter->points()[2].y, 0));
        CHECK(near(quarter->samples().points[0].y, 2));
    }
    auto half = copyAt(0.5f);
    CHECK(half && half->count() == 3u && near(half->points()[0].x, -1) && near(half->points()[0].y, 1));
    BBox bounds;
    CHECK(renderer->bounds(scene, plot, 0.25f, bounds) == Result::Success && bounds.width > 80);
    CHECK(renderer->bounds(scene, plot, 0.5f, bounds) == Result::Success && bounds.width < 65);
    // Sampling out of order keeps the same column and cut.
    auto again = copyAt(0.25f);
    CHECK(again && again->count() == quarter->count()
          && std::memcmp(again->points(), quarter->points(), sizeof(Vec3) * quarter->count()) == 0);
    delete scene;

    // A gap remains a gap even when clipping changes the visible pieces and leaves none.
    auto nan = std::numeric_limits<float>::quiet_NaN();
    const Vec3 apart[] = {{-2, 0, 0}, {-1, 0, 0}, {nan, nan, 0}, {1, 0, 0}, {2, 0, 0}};
    const Vec3 raised[] = {{-2, 4, 0}, {-1, 4, 0}, {nan, nan, 0}, {1, 0, 0}, {2, 0, 0}};
    const Vec3 visible[] = {{-2, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    const uint32_t ends[] = {2u, 4u};
    from = {apart, 5u, true, {-1, 1}};
    to = {raised, 5u, true, {-1, 1}};
    scene = Scene::gen(config);
    plot = Plot::gen(visible, 4u, ends, 2u, &from);
    CHECK(scene && plot && scene->add(plot) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).geometry(visible + 2, 2u, nullptr, 1u, &to), 1, linear) == Result::Success);
    auto edge = copyAt(0.25f);
    CHECK(edge && edge->contours() == 2u && near(edge->points()[0].y, 1));
    auto leaving = copyAt(0.5f);
    CHECK(leaving && leaving->count() == 2u && leaving->contours() == 1u && near(leaving->points()[0].x, 1));
    CHECK(near(_shownOpacity(renderer, scene, 0.25f, {0, 0, 0}), 0, 0.01f));
    // Reverse play makes the lost piece enter the band again.
    CHECK(scene->seek(1) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(plot).geometry(visible, 4u, ends, 2u, &from), 1, linear) == Result::Success);
    auto entering = copyAt(1.75f);
    CHECK(entering && entering->contours() == 2u && near(entering->points()[0].y, 1));
    const Vec3 invalid[] = {{1, 0, 0}, {0, 0, 0}};
    to = {invalid, 2u, false, {}};
    CHECK(scene->play(AnimationTarget::from(plot).geometry(visible, 4u, ends, 2u, &to), 1) == Result::InvalidArguments);
    delete scene;
}

// A plot that takes points in other pieces switches halfway; matching pieces travel.
// Copies, saves and clips of one frame each go by the shape the plot shows.
static void plotReplot(SwRenderer* renderer)
{
    Config config;
    config.width = 320;
    config.height = 180;
    config.camera.orthoHeight = 6.0f;
    config.background = {255, 255, 255, 255};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    // A run a unit below the axis; two pieces a unit above it, apart around x = 0; the same two
    // pieces on the axis.
    const Vec3 low[] = {{-4.0f, -1.0f, 0.0f}, {-2.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
                        {2.0f, -1.0f, 0.0f}, {4.0f, -1.0f, 0.0f}};
    const Vec3 high[] = {{-4.0f, 1.0f, 0.0f}, {-2.5f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f},
                         {1.0f, 1.0f, 0.0f}, {2.5f, 1.0f, 0.0f}, {4.0f, 1.0f, 0.0f}};
    const Vec3 level[] = {{-4.0f, 0.0f, 0.0f}, {-2.5f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
                          {1.0f, 0.0f, 0.0f}, {2.5f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}};
    const uint32_t apart[] = {3u, 6u};
    const Vec3 corners[] = {{4.5f, 2.0f, 0.0f}, {5.0f, 2.0f, 0.0f}, {5.0f, 2.5f, 0.0f}};
    Scene* scene = nullptr;
    Plot* plot = nullptr;
    auto shown = [&](float time, float x, float y) { return _shownOpacity(renderer, scene, time, {x, y, 0.0f}); };
    // The pixel row the middle of the plot's bounds lies on at time, or -1 without bounds.
    auto row = [&](float time) {
        BBox bounds;
        if (renderer->bounds(scene, plot, time, bounds) != Result::Success) return -1.0f;
        return bounds.center().y;
    };
    auto start = [&](const Vec3* points, uint32_t count, const uint32_t* ends, uint32_t pieces) {
        scene = Scene::gen(config);
        plot = Plot::gen(points, count, ends, pieces);
        CHECK(scene && plot);
        if (!scene || !plot) {
            delete plot;
            delete scene;
            return false;
        }
        plot->stroke({0, 0, 0, 255}, 4.0f);
        CHECK(scene->add(plot) == Result::Success);
        return true;
    };

    if (start(low, 5u, nullptr, 1u)) {
        // Other pieces: the run shows until halfway, then the two pieces, with nothing between.
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, apart, 2u), 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 6u && plot->contours() == 2u && plot->contourEnd(0) == 3u);
        CHECK(near(shown(0.25f, 0.0f, -1.0f), 1.0f, 0.05f) && near(shown(0.25f, -3.0f, 1.0f), 0.0f, 0.02f));
        CHECK(near(shown(0.45f, 3.0f, -1.0f), 1.0f, 0.05f));
        CHECK(near(shown(0.55f, 3.0f, -1.0f), 0.0f, 0.02f));
        CHECK(near(shown(0.55f, -3.0f, 1.0f), 1.0f, 0.05f) && near(shown(0.55f, 3.0f, 1.0f), 1.0f, 0.05f));
        CHECK(near(shown(0.55f, 0.0f, 1.0f), 0.0f, 0.02f));
        CHECK(near(row(0.25f), 120.0f, 1.0f) && near(row(0.75f), 60.0f, 1.0f));
        // The same pieces: the points travel.
        CHECK(scene->play(AnimationTarget::from(plot).geometry(level, 6u, apart, 2u), 1.0f, linear) == Result::Success);
        CHECK(near(shown(1.5f, -3.0f, 0.5f), 1.0f, 0.05f) && near(shown(1.5f, 0.0f, 0.5f), 0.0f, 0.02f));
        CHECK(near(row(1.5f), 75.0f, 1.0f));
        // No points leave the plot empty, with no bounds, until points come back; points of the
        // old count no longer fit it.
        CHECK(scene->play(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 0u), 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 0u && plot->contours() == 0u);
        CHECK(near(shown(2.25f, -3.0f, 0.0f), 1.0f, 0.05f) && near(shown(2.75f, -3.0f, 0.0f), 0.0f, 0.02f));
        CHECK(row(2.25f) > 0.0f && row(2.75f) < 0.0f);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(level, 6u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(low, 5u, nullptr, 1u), 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 5u && plot->contours() == 1u);
        CHECK(near(shown(3.25f, 0.0f, -1.0f), 0.0f, 0.02f) && near(shown(3.75f, 0.0f, -1.0f), 1.0f, 0.05f));
        // A copy takes the shape shown at the cursor, pieces and all, or none.
        CHECK(scene->seek(0.75f) == Result::Success);
        Object* copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        auto twin = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
        CHECK(twin && twin->count() == 6u && twin->contours() == 2u && twin->contourEnd(0) == 3u);
        CHECK(twin && near(twin->points()[3].x, 1.0f) && near(twin->points()[3].y, 1.0f));
        CHECK(scene->seek(2.75f) == Result::Success);
        copy = nullptr;
        CHECK(scene->clone(plot, copy) == Result::Success);
        auto hollow = copy && copy->type() == Type::Plot ? static_cast<Plot*>(copy) : nullptr;
        CHECK(hollow && hollow->count() == 0u && hollow->contours() == 0u);
        delete scene;
    }

    if (start(high, 6u, apart, 2u)) {
        // A restore brings the saved pieces back halfway through its clip, and creation runs
        // through them.
        auto triangle = Polygon::gen(corners, 3u);
        CHECK(triangle && scene->add(triangle) == Result::Success);
        CHECK(scene->save(plot) == Result::Success);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(low, 5u, nullptr, 1u), 1.0f, linear) == Result::Success);
        CHECK(scene->restore(plot, 1.0f, linear) == Result::Success);
        CHECK(plot->count() == 6u && plot->contours() == 2u && plot->contourEnd(0) == 3u);
        CHECK(near(shown(1.25f, 0.0f, -1.0f), 1.0f, 0.05f));
        CHECK(near(shown(1.75f, 0.0f, -1.0f), 0.0f, 0.02f) && near(shown(1.75f, -3.0f, 1.0f), 1.0f, 0.05f));
        CHECK(scene->play(Animation::create(plot), 1.0f, linear) == Result::Success);
        CHECK(near(shown(2.75f, 2.0f, 1.0f), 1.0f, 0.05f) && near(shown(2.75f, 3.0f, 1.0f), 0.0f, 0.02f));
        // Pieces are a plot's alone and must cover its points.
        CHECK(scene->play(AnimationTarget::from(triangle).geometry(corners, 3u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        CHECK(_says(scene, "only a plot takes pieces"));
        const uint32_t lonely[] = {1u, 6u};
        const uint32_t short_[] = {3u, 5u};
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, lonely, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(_says(scene, "each holding two points or more"));
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, short_, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, nullptr, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 6u, nullptr, 0u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(high, 1u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        delete scene;
    }

    if (start(low, 5u, nullptr, 1u)) {
        // Before the timeline the points and pieces become the plot's own; afterwards a set
        // switches them at the cursor.
        CHECK(scene->update(AnimationTarget::from(plot).geometry(high, 6u, apart, 2u)) == Result::Success);
        CHECK(plot->count() == 6u && plot->contours() == 2u);
        CHECK(near(shown(0.0f, -3.0f, 1.0f), 1.0f, 0.05f) && near(shown(0.0f, 0.0f, 1.0f), 0.0f, 0.02f));
        CHECK(scene->wait(1.0f) == Result::Success);
        CHECK(scene->update(AnimationTarget::from(plot).geometry(nullptr, 0u, nullptr, 0u)) == Result::Success);
        CHECK(near(shown(0.5f, -3.0f, 1.0f), 1.0f, 0.05f) && near(shown(1.0f, -3.0f, 1.0f), 0.0f, 0.02f));
        // A warp of the empty plot keeps it empty, as a warp keeps the outline of any shape.
        Vec3 up = {0.0f, 1.0f, 0.0f};
        Object* targets[] = {plot};
        CHECK(scene->warp(targets, 1u, _moved, &up, 1.0f) == Result::Success);
        CHECK(near(shown(2.0f, -3.0f, 2.0f), 0.0f, 0.02f) && row(2.0f) < 0.0f);
        CHECK(scene->play(AnimationTarget::from(plot).geometry(low, 5u, nullptr, 1u), 1.0f) == Result::InvalidArguments);
        delete scene;
    }

    // Clips of a frame each, as a track plays them, show at every frame the shape of its time,
    // also where the frame falls just before the end of its clip.
    config.fps = 60;
    const Vec3 rest[] = {{-2.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}};
    if (start(rest, 2u, nullptr, 1u)) {
        auto early = 0u;
        for (auto k = 1u; k <= 30u; k++) {
            // A run 0.1 higher each frame, of two points and of three in turn.
            auto y = -2.0f + 0.1f * static_cast<float>(k);
            auto three = k % 2u == 1u;
            Vec3 run[] = {{-2.0f, y, 0.0f}, {three ? 0.0f : 2.0f, y, 0.0f}, {2.0f, y, 0.0f}};
            CHECK(scene->play(AnimationTarget::from(plot).geometry(run, three ? 3u : 2u, nullptr, 1u), 1.0f / 60.0f,
                              linear) == Result::Success);
            auto frame = static_cast<float>(k) / 60.0f;
            if (frame < scene->cursor()) early++;
            CHECK(near(row(frame), 90.0f - 30.0f * y, 1.0f));
        }
        CHECK(early > 0u);
        delete scene;
    }
}

// Whether the object's pixel bounds at time center on world point (x, y) and, when size is
// positive, measure size world units on each side, in the 200 by 200 scene of layeredMotions.
static bool _placed(SwRenderer* renderer, Scene* scene, Object* object, float time, float x, float y,
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

// The camera and view a scene shows, read through a runtime camera hook.
struct CameraSample
{
    Camera camera;
    CameraView view = CameraView::TwoD;
    bool taken = false;
};

static Result _takeCamera(float, Camera& camera, CameraView& view, void* data) noexcept
{
    auto sample = static_cast<CameraSample*>(data);
    sample->camera = camera;
    sample->view = view;
    sample->taken = true;
    return Result::Success;
}

// Whether the scene, whose runtime hook is _takeCamera on sample, shows a valid camera at time.
static bool _cameraAt(Scene* scene, float time, CameraSample& sample)
{
    sample.taken = false;
    scene->view(time);
    return sample.taken;
}

// A camera move may begin while another runs: it adds its shift, turn, zoom and lens change to
// that move, and switches the view and projection when a plain look would.
static void cameraLayers(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto mark = Rectangle::gen({4.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    CHECK(scene && mark);
    if (!scene || !mark) {
        delete mark;
        delete scene;
        return;
    }
    mark->style.stroke.a = 0;
    mark->fill(Color::hex("#ffffff"));
    CHECK(scene->add(mark) == Result::Success);

    // A zoom over three seconds begins half a second into a pan over two: the target travels
    // while the view height shrinks by the same factor every second.
    auto camera = scene->config().camera;
    camera.target = {4.0f, 0.0f, 0.0f};
    camera.eye = {4.0f, 0.0f, 10.0f};
    CHECK(scene->look(camera, CameraView::TwoD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    camera.orthoHeight = 2.5f;
    CHECK(scene->look(camera, CameraView::TwoD, 3.0f, Easing::Linear) == Result::Success);
    // _placed reads a frame 10 units high around the origin, so the mark's offset and size
    // scale by 10 over the view height.
    auto framed = [&](float time, float target, float height) {
        auto zoom = 10.0f / height;
        return _placed(renderer, scene, mark, time, (4.0f - target) * zoom, 0.0f, zoom);
    };
    CHECK(framed(0.5f, 1.0f, 10.0f));
    CHECK(framed(1.0f, 2.0f, 10.0f * std::pow(0.25f, 1.0f / 6.0f)));
    CHECK(framed(2.0f, 4.0f, 5.0f));
    CHECK(framed(3.5f, 4.0f, 2.5f));

    // A look after both starts where they leave the camera.
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.eye = {0.0f, 0.0f, 10.0f};
    camera.orthoHeight = 10.0f;
    CHECK(scene->seek(3.5f) == Result::Success);
    CHECK(scene->look(camera, CameraView::TwoD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(framed(4.0f, 2.0f, 6.25f));

    // Clip planes added to that look ease from the current camera's to their own while it pans
    // and zooms, and the mark, 10 units in front of the eye, goes once the near plane passes it.
    CameraSample sample;
    RuntimeModifier hook;
    hook.camera = _takeCamera;
    hook.data = &sample;
    CHECK(scene->runtime(&hook) == Result::Success);
    CHECK(scene->seek(4.0f) == Result::Success);
    auto planes = camera;
    planes.near = 15.0f;
    planes.far = 40.0f;
    CHECK(scene->look(planes, CameraView::TwoD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(scene, 4.25f, sample) && near(sample.camera.near, 0.05f + 14.95f * 0.25f, 1.0e-3f)
          && near(sample.camera.far, 760.0f, 1.0e-2f) && near(sample.camera.target.x, 1.0f, 1.0e-3f));
    CHECK(framed(4.25f, 1.0f, 8.125f));
    CHECK(framed(4.5f, 0.0f, 10.0f));
    Surface surface;
    CHECK(renderer->render(scene, 4.5f, surface) == Result::Success);
    CHECK(changed(surface, Color::hex("#000000")) == 400u);
    CHECK(renderer->render(scene, 4.9f, surface) == Result::Success);
    CHECK(changed(surface, Color::hex("#000000")) == 0u);
    CHECK(_cameraAt(scene, 5.0f, sample) && near(sample.camera.near, 15.0f) && near(sample.camera.far, 40.0f)
          && sample.view == CameraView::TwoD);
    delete scene;

    // In 3D a zoom adds to an orbit, and an orbit turns about the target a pan moves.
    config.cameraView = CameraView::ThreeD;
    auto spatial = Scene::gen(config);
    auto dot = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(spatial && dot);
    if (!spatial || !dot) {
        delete dot;
        delete spatial;
        return;
    }
    dot->stroke(Color::hex("#ffffff")).fill(Color::hex("#ffffff"));
    dot->style.radius = 3.0f;
    CHECK(spatial->add(dot) == Result::Success);
    constexpr auto QUARTER = 1.5707963f;
    CHECK(spatial->orbit(QUARTER, {0.0f, 1.0f, 0.0f}, 2.0f, Easing::Linear) == Result::Success);
    CHECK(spatial->seek(1.0f) == Result::Success);
    auto zoomed = spatial->config().camera;
    zoomed.orthoHeight = 5.0f;
    CHECK(spatial->look(zoomed, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    // After a turn of angle about +y the dot sits 2 cos(angle) right of the target.
    CHECK(_placed(renderer, spatial, dot, 1.5f, 2.0f * std::cos(0.75f * QUARTER) * std::sqrt(2.0f), 0.0f));
    CHECK(_placed(renderer, spatial, dot, 2.0f, 0.0f, 0.0f));
    // An orbit added to a pan up circles the eye about the rising target.
    auto raised = spatial->config().camera;
    raised.target.y += 1.0f;
    raised.eye.y += 1.0f;
    CHECK(spatial->look(raised, CameraView::ThreeD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(spatial->seek(3.0f) == Result::Success);
    CHECK(spatial->orbit(QUARTER, {0.0f, 1.0f, 0.0f}, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, spatial, dot, 3.5f, 4.0f * std::cos(1.5f * QUARTER), -1.5f));
    CHECK(_placed(renderer, spatial, dot, 4.0f, -4.0f, -2.0f));
    auto& last = spatial->config().camera;
    CHECK(near(last.target.y, 1.0f) && near(last.eye.x, 0.0f, 1.0e-3f) && near(last.eye.y, 1.0f, 1.0e-3f)
          && near(last.eye.z, -10.0f, 1.0e-3f));
    delete spatial;

    // A look that turns the view during a pan turns about the moving target, part way along
    // its shortest turn.
    auto turning = Scene::gen(config);
    auto spot = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(turning && spot);
    if (!turning || !spot) {
        delete spot;
        delete turning;
        return;
    }
    spot->stroke(Color::hex("#ffffff")).fill(Color::hex("#ffffff"));
    spot->style.radius = 3.0f;
    CHECK(turning->add(spot) == Result::Success);
    auto pan = turning->config().camera;
    pan.target.y += 1.0f;
    pan.eye.y += 1.0f;
    CHECK(turning->look(pan, CameraView::ThreeD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(turning->seek(0.5f) == Result::Success);
    auto side = pan;
    side.eye = {10.0f, 1.0f, 0.0f};
    CHECK(turning->look(side, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, turning, spot, 1.0f, 2.0f * std::cos(0.5f * QUARTER), -0.5f));
    CHECK(_placed(renderer, turning, spot, 1.5f, 0.0f, -0.75f));
    CHECK(_placed(renderer, turning, spot, 2.0f, 0.0f, -1.0f));
    delete turning;

    // A projection switch added to a pan switches half way, as a plain look does, while the pan
    // goes on. 10 units from the eye a perspective camera fits 20 tan(fov / 2) units in the frame
    // height, where the orthographic one fits 10.
    auto switched = Scene::gen(config);
    auto square = Rectangle::gen({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    CHECK(switched && square);
    if (!switched || !square) {
        delete square;
        delete switched;
        return;
    }
    square->style.stroke.a = 0;
    square->fill(Color::hex("#ffffff"));
    CHECK(switched->add(square) == Result::Success);
    CHECK(switched->runtime(&hook) == Result::Success);
    auto panned = switched->config().camera;
    panned.target.x += 1.0f;
    panned.eye.x += 1.0f;
    CHECK(switched->look(panned, CameraView::ThreeD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(switched->seek(0.5f) == Result::Success);
    auto perspective = panned;
    perspective.projection = Projection::Perspective;
    CHECK(switched->look(perspective, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(switched, 0.9f, sample) && sample.camera.projection == Projection::Orthographic);
    CHECK(_cameraAt(switched, 1.0f, sample) && sample.camera.projection == Projection::Perspective);
    auto widened = 0.5f / std::tan(0.5f * 0.7853982f);
    CHECK(_placed(renderer, switched, square, 0.9f, -0.45f, 0.0f, 2.0f));
    CHECK(_placed(renderer, switched, square, 1.1f, -0.55f * widened, 0.0f, 2.0f * widened));
    CHECK(_placed(renderer, switched, square, 2.0f, -widened, 0.0f, 2.0f * widened));
    delete switched;

    // A turn into the 3D view added to a pan shows that view once it starts. A turn back to 2D
    // shows the plane straight on only at its end, so it must not end before the moves it adds
    // to, and a turn needs the 3D view for as long as it runs.
    config.cameraView = CameraView::TwoD;
    auto tilted = Scene::gen(config);
    auto tile = Rectangle::gen({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    CHECK(tilted && tile);
    if (!tilted || !tile) {
        delete tile;
        delete tilted;
        return;
    }
    tile->style.stroke.a = 0;
    tile->fill(Color::hex("#ffffff"));
    CHECK(tilted->add(tile) == Result::Success);
    CHECK(tilted->runtime(&hook) == Result::Success);
    auto across = tilted->config().camera;
    across.target = {2.0f, 0.0f, 0.0f};
    across.eye = {2.0f, 0.0f, 10.0f};
    CHECK(tilted->look(across, CameraView::TwoD, 2.0f, Easing::Linear) == Result::Success);
    CHECK(tilted->seek(0.5f) == Result::Success);
    auto oblique = across;
    oblique.eye = {2.0f, -8.0f, 6.0f};
    oblique.projection = Projection::Perspective;
    CHECK(tilted->look(oblique, CameraView::ThreeD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(tilted, 0.5f, sample) && sample.view == CameraView::TwoD
          && sample.camera.projection == Projection::Orthographic);
    CHECK(_cameraAt(tilted, 0.6f, sample) && sample.view == CameraView::ThreeD
          && sample.camera.projection == Projection::Perspective && near(sample.camera.target.x, 0.6f, 1.0e-3f));
    // The eye has turned to the side of the target that the look faces from, as far as the pan
    // has carried the target.
    CHECK(_cameraAt(tilted, 1.5f, sample) && near(sample.camera.eye.x, 1.5f, 1.0e-3f)
          && near(sample.camera.eye.y, -8.0f, 1.0e-3f) && near(sample.camera.eye.z, 6.0f, 1.0e-3f));
    CHECK(tilted->seek(0.0f) == Result::Success);
    CHECK(tilted->orbit(QUARTER, {0.0f, 0.0f, 1.0f}, 1.0f) == Result::InsufficientCondition);
    CHECK(tilted->diagnostic()
          && std::strcmp(tilted->diagnostic(),
                         "the camera is in the 2D view at 0.00 s, while this move would turn it or change its "
                         "projection, which only the 3D view allows") == 0);
    CHECK(tilted->seek(1.0f) == Result::Success);
    CHECK(tilted->look(across, CameraView::TwoD, 0.5f, Easing::Linear) == Result::InsufficientCondition);
    CHECK(tilted->diagnostic()
          && std::strcmp(tilted->diagnostic(),
                         "the camera already moves until 2.00 s; a look that returns to the 2D view ends at "
                         "that time or later") == 0);
    CHECK(tilted->look(across, CameraView::TwoD, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_cameraAt(tilted, 1.9f, sample) && sample.view == CameraView::ThreeD
          && sample.camera.projection == Projection::Perspective);
    CHECK(_cameraAt(tilted, 2.0f, sample) && sample.view == CameraView::TwoD);
    CHECK(_placed(renderer, tilted, tile, 2.0f, -2.0f, 0.0f, 2.0f));
    delete tilted;

    // A turn needs the 3D view for as long as it runs, so a layered orbit or turning look fails
    // while a move stored earlier still shows the 2D view, and runs once none shows it.
    config.cameraView = CameraView::TwoD;
    auto rising = Scene::gen(config);
    CHECK(rising);
    if (!rising) return;
    CHECK(rising->runtime(&hook) == Result::Success);
    auto slanted = rising->config().camera;
    slanted.eye = {0.0f, -8.0f, 6.0f};
    CHECK(rising->seek(1.0f) == Result::Success);
    CHECK(rising->look(slanted, CameraView::ThreeD, 1.0f) == Result::Success);
    CHECK(rising->seek(0.0f) == Result::Success);
    CHECK(rising->orbit(QUARTER, {0.0f, 0.0f, 1.0f}, 2.0f) == Result::InsufficientCondition);
    CHECK(rising->diagnostic() && std::strstr(rising->diagnostic(), "the camera is in the 2D view at 0.00 s"));
    auto sideways = slanted;
    sideways.eye = {8.0f, 0.0f, 6.0f};
    CHECK(rising->look(sideways, CameraView::ThreeD, 2.0f) == Result::InsufficientCondition);
    CHECK(rising->diagnostic() && std::strstr(rising->diagnostic(), "the camera is in the 2D view at 0.00 s"));
    // A zoom does not turn, so it may add to the 2D view.
    auto zoom = slanted;
    zoom.orthoHeight = 5.0f;
    CHECK(rising->look(zoom, CameraView::ThreeD, 1.0f) == Result::Success);
    CHECK(_cameraAt(rising, 0.5f, sample) && sample.view == CameraView::TwoD);
    CHECK(rising->seek(1.5f) == Result::Success);
    CHECK(rising->orbit(QUARTER, {0.0f, 0.0f, 1.0f}, 1.0f) == Result::Success);
    CHECK(_cameraAt(rising, 1.75f, sample) && sample.view == CameraView::ThreeD);
    delete rising;
}

// A shift, turn, scale, travel or effect may begin while another clip moves the object: it adds
// its change on top, turning and scaling about a point of the object that travels with it.
static void layeredMotions(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto grown = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto spun = Rectangle::gen({-2.0f, 3.0f, 0.0f}, {1.0f, 1.0f});
    auto orbited = Rectangle::gen({-2.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto pulsed = Rectangle::gen({-2.0f, -1.5f, 0.0f}, {1.0f, 1.0f});
    auto carried = Rectangle::gen({-2.0f, 2.0f, 0.0f}, {1.0f, 1.0f});
    auto track = Line::gen({-2.0f, 2.0f, 0.0f}, {2.0f, 2.0f, 0.0f});
    auto summed = Rectangle::gen({-2.0f, -1.0f, 0.0f}, {1.0f, 1.0f});
    auto flat = Rectangle::gen({3.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto shape = Circle::gen({0.0f, 0.0f, 0.0f}, 0.5f);
    Object* objects[] = {grown, spun, orbited, pulsed, carried, track, summed, flat, shape};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    for (auto object : objects) {
        if (object == shape) continue;
        object->style.stroke.a = object == track ? 255 : 0;
        if (object != track) object->fill(Color::hex("#ffffff"));
        CHECK(scene->add(object) == Result::Success);
    }

    // A scale while the object travels grows it about its moving center, and a turn played
    // after both starts from where they left it.
    CHECK(scene->play(Animation::shift(grown, {4.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::rescale(grown, {2.0f, 2.0f, 1.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, grown, 0.25f, -1.5f, 0.0f, 1.0f));
    CHECK(_placed(renderer, scene, grown, 1.0f, 0.0f, 0.0f, 1.5f));
    CHECK(_placed(renderer, scene, grown, 1.75f, 1.5f, 0.0f, 2.0f));
    CHECK(_placed(renderer, scene, grown, 2.0f, 2.0f, 0.0f, 2.0f));
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(grown, 1.5707963f), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, grown, 2.0f, 2.0f, 0.0f, 2.0f));
    CHECK(_placed(renderer, scene, grown, 2.5f, 2.0f, 0.0f));
    CHECK(_placed(renderer, scene, grown, 3.0f, 2.0f, 0.0f, 2.0f));

    // A shift during a turn carries the turning object.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(spun, 1.5707963f), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::shift(spun, {2.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, spun, 1.0f, -1.0f, 3.0f));
    CHECK(_placed(renderer, scene, spun, 2.0f, 0.0f, 3.0f, 1.0f));

    // A pivot outside the object stays where it is while the object travels.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(orbited, {1.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(orbited, 3.14159265f, {0.0f, -3.0f, 0.0f}, {0.0f, 0.0f, 1.0f}), 1.0f,
                      Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, orbited, 1.5f, 0.0f, -4.25f));
    CHECK(_placed(renderer, scene, orbited, 2.0f, 1.0f, -3.0f, 1.0f));

    // An indication pulses the moving object about its center and leaves it as it was.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(pulsed, {4.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::indicate(pulsed), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, pulsed, 1.0f, 0.0f, -1.5f, 1.2f));
    CHECK(_placed(renderer, scene, pulsed, 2.0f, 2.0f, -1.5f, 1.0f));
    CHECK(near(pulsed->model.e[0], 1.0f) && near(pulsed->model.e[3], 4.0f));

    // A travel along a path while the object grows keeps its center on the path.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::rescale(carried, {2.0f, 2.0f, 1.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(Animation::moveAlong(carried, track), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, carried, 0.25f, -2.0f, 2.0f, 1.125f));
    CHECK(_placed(renderer, scene, carried, 1.0f, 0.0f, 2.0f, 1.5f));
    CHECK(_placed(renderer, scene, carried, 2.0f, 2.0f, 2.0f, 2.0f));

    // Shifts add up, whether made by shift, by a play target, or by a transform that only moves.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(summed, {2.0f, 0.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::shift(summed, {0.0f, 2.0f, 0.0f}), 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(summed).shift({1.0f, 0.0f, 0.0f}), 1.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->play(Animation::transform(summed, Mat4::translate({0.0f, -1.0f, 0.0f}) * summed->model), 0.5f,
                      Easing::Linear) == Result::Success);
    CHECK(_placed(renderer, scene, summed, 2.0f, 0.5f, 0.0f, 1.0f));
    CHECK(_placed(renderer, scene, summed, 3.0f, 1.0f, 0.0f, 1.0f));
    CHECK(near(summed->model.e[3], 3.0f) && near(summed->model.e[7], 1.0f));

    // A morph that begins during a motion must not end before it.
    CHECK(scene->add(shape) == Result::Success);
    CHECK(scene->play(Animation::morph(summed, shape), 0.25f) == Result::InsufficientCondition);
    CHECK(scene->diagnostic()
          && std::strcmp(scene->diagnostic(), "the object already animates its transform until 3.00 s; a morph that "
                                              "begins before then must end at that time or later") == 0);

    // A turn cannot add to a motion that flattens the object, though a shift can.
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(Animation::rescale(flat, {0.0f, 1.0f, 1.0f}), 2.0f) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::rotate(flat, 1.0f), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    CHECK(scene->play(Animation::shift(flat, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    delete scene;
}

// Whether the points of a probe at its origin and one unit along x and y stand where model puts
// them, in a 200 by 200 scene ten units high.
static bool _probed(SwRenderer* renderer, Scene* scene, Group* probe, float time, const Mat4& model)
{
    const Vec3 points[] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    auto placed = true;
    for (auto i = 0u; i < 3u; i++) {
        auto point = model * points[i];
        placed = _placed(renderer, scene, probe->childAt(i), time, point.x, point.y) && placed;
    }
    return placed;
}

// The matrix a transform without an arc passes through: every entry travels straight.
static Mat4 _blend(const Mat4& from, const Mat4& to, float progress)
{
    Mat4 blended;
    for (auto i = 0u; i < 16u; i++) blended.e[i] = from.e[i] + (to.e[i] - from.e[i]) * progress;
    return blended;
}

// Whether two matrices agree entry by entry.
static bool _matches(const Mat4& lhs, const Mat4& rhs)
{
    for (auto i = 0u; i < 16u; i++) {
        if (!near(lhs.e[i], rhs.e[i])) return false;
    }
    return true;
}

// A transform that turns, scales or skews may begin while another clip moves the object: it adds
// its change from the authored model on top, about the object's moving center, and travels as it
// would alone, so the object lands on its matrix once every motion ends.
static void layeredTransforms(SwRenderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto turned = Group::gen();
    auto swung = Group::gen();
    auto sheared = Group::gen();
    auto jumped = Group::gen();
    auto flat = Group::gen();
    Group* probes[] = {turned, swung, sheared, jumped, flat};
    Object* points[15] = {};
    for (auto i = 0u; i < 15u; i++) points[i] = Point::gen({i % 3u == 1u ? 1.0f : 0.0f, i % 3u == 2u ? 1.0f : 0.0f, 0.0f});
    auto created = scene != nullptr;
    for (auto probe : probes) created = created && probe;
    for (auto point : points) created = created && point;
    CHECK(created);
    if (!created) {
        for (auto probe : probes) delete probe;
        for (auto point : points) delete point;
        delete scene;
        return;
    }
    for (auto i = 0u; i < 15u; i++) CHECK(probes[i / 3u]->add(points[i]) == Result::Success);
    turned->model = Mat4::translate({-3.0f, 3.0f, 0.0f});
    swung->model = Mat4::translate({-3.0f, 0.0f, 0.0f});
    sheared->model = Mat4::translate({-3.0f, -3.0f, 0.0f});
    jumped->model = Mat4::translate({-3.0f, -1.0f, 0.0f});
    flat->model = Mat4::translate({3.0f, -3.0f, 0.0f});
    for (auto probe : probes) CHECK(scene->add(probe) == Result::Success);
    Animation moves[] = {
        Animation::shift(turned, {4.0f, 0.0f, 0.0f}),
        Animation::shift(swung, {4.0f, 0.0f, 0.0f}),
        Animation::rotate(sheared, PI * 0.5f),
        Animation::shift(jumped, {4.0f, 0.0f, 0.0f}),
        Animation::rescale(flat, {0.0f, 1.0f, 1.0f}),
    };
    CHECK(scene->play(moves, 5u, 2.0f, Easing::Linear) == Result::Success);

    // A quarter turn during a shift: without an arc it shrinks midway as it would alone, and
    // with an arc equal to the turn it stays rigid, while the shift carries it either way.
    auto quarter = Mat4::rotateZ(PI * 0.5f);
    auto straight = Mat4::translate({1.0f, 3.0f, 0.0f}) * quarter;
    auto arced = Mat4::translate({1.0f, 0.0f, 0.0f}) * quarter;
    Animation turns[] = {Animation::transform(turned, straight), Animation::transform(swung, arced)};
    turns[1].arc = PI * 0.5f;
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(turns, 2u, 1.0f, Easing::Linear) == Result::Success);
    CHECK(_probed(renderer, scene, turned, 0.25f, Mat4::translate({-2.5f, 3.0f, 0.0f})));
    CHECK(_probed(renderer, scene, turned, 1.0f,
                  Mat4::translate({-1.0f, 3.0f, 0.0f}) * _blend(Mat4::identity(), quarter, 0.5f)));
    CHECK(_probed(renderer, scene, turned, 1.5f, Mat4::translate({0.0f, 3.0f, 0.0f}) * quarter));
    CHECK(_probed(renderer, scene, turned, 2.0f, straight));
    CHECK(_probed(renderer, scene, swung, 1.0f, Mat4::translate({-1.0f, 0.0f, 0.0f}) * Mat4::rotateZ(PI * 0.25f)));
    CHECK(_probed(renderer, scene, swung, 1.5f, Mat4::translate({0.0f, 0.0f, 0.0f}) * quarter));
    CHECK(_probed(renderer, scene, swung, 2.0f, arced));
    CHECK(_matches(turned->model, straight) && _matches(swung->model, arced));

    // A play target skews about the center of a turning object.
    auto start = Mat4::translate({-3.0f, -3.0f, 0.0f});
    auto skew = Mat4::identity();
    skew.e[1] = 0.5f;
    auto skewed = Mat4::translate({-2.5f, -2.5f, 0.0f}) * skew * Mat4::translate({2.5f, 2.5f, 0.0f});
    auto target = skewed * sheared->model;
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(sheared).transform(target), 1.0f, Easing::Linear) == Result::Success);
    CHECK(_probed(renderer, scene, sheared, 1.0f,
                  _blend(Mat4::identity(), skewed, 0.5f) * _rotationAbout(PI * 0.25f, {-2.5f, -2.5f, 0.0f}) * start));
    CHECK(_probed(renderer, scene, sheared, 1.5f, skewed * _rotationAbout(PI * 0.375f, {-2.5f, -2.5f, 0.0f}) * start));
    CHECK(_probed(renderer, scene, sheared, 2.0f, target));

    // A change at the cursor jumps on top of the running shift.
    auto jump = Mat4::translate({1.0f, -1.0f, 0.0f}) * quarter * Mat4::scale({0.5f, 0.5f, 1.0f});
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(jumped).transform(jump)) == Result::Success);
    CHECK(_probed(renderer, scene, jumped, 0.99f, Mat4::translate({-1.02f, -1.0f, 0.0f})));
    CHECK(_probed(renderer, scene, jumped, 1.0f, Mat4::translate({-2.0f, 0.0f, 0.0f}) * jump));
    CHECK(_probed(renderer, scene, jumped, 1.5f, Mat4::translate({-1.0f, 0.0f, 0.0f}) * jump));
    CHECK(_probed(renderer, scene, jumped, 2.0f, jump));
    CHECK(_matches(jumped->model, jump));

    // Only a shift adds to a motion that flattens the object.
    auto turn = Mat4::rotateZ(0.5f) * flat->model;
    CHECK(scene->play(Animation::transform(flat, turn), 0.5f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it, so only a shift can add to it"));
    CHECK(scene->play(AnimationTarget::from(flat).transform(turn), 0.5f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    CHECK(scene->update(AnimationTarget::from(flat).transform(turn)) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    CHECK(scene->play(Animation::transform(flat, Mat4::translate({0.0f, 1.0f, 0.0f}) * flat->model), 0.5f)
          == Result::Success);
    delete scene;
}

// A morph may begin while its source moves: it adds the move to the place of the shape it becomes
// on top of that motion, and ends no sooner, so the shape takes over where the source lands.
static void layeredMorphs(SwRenderer* renderer)
{
    constexpr auto PI = 3.14159265358979f;
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.background = Color::hex("#000000");
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto square = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto twin = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto bar = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 0.5f});
    auto barTwin = Rectangle::gen({0.0f, 0.0f, 0.0f}, {1.0f, 0.5f});
    auto faded = Rectangle::gen({-2.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto flat = Rectangle::gen({3.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
    auto disc = Circle::gen({0.0f, -3.0f, 0.0f}, 0.5f);
    Object* objects[] = {square, twin, bar, barTwin, faded, flat, disc};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    for (auto object : objects) {
        object->style.stroke.a = 0;
        object->fill(Color::hex("#ffffff"));
    }
    // The twin morphs at rest from where the square lands, into a twin of the bar.
    twin->model = Mat4::translate({4.0f, 0.0f, 0.0f});
    bar->model = Mat4::translate({2.0f, 2.0f, 0.0f}) * Mat4::rotateZ(PI * 0.5f);
    barTwin->model = bar->model;
    Object* early[] = {square, twin, faded, flat};
    for (auto object : early) CHECK(scene->add(object) == Result::Success);
    Animation moves[] = {
        Animation::shift(square, {4.0f, 0.0f, 0.0f}),
        Animation::fade(faded, 0.5f),
        Animation::rescale(flat, {0.0f, 1.0f, 1.0f}),
    };
    CHECK(scene->play(moves, 3u, 2.0f, Easing::Linear) == Result::Success);
    CHECK(scene->seek(1.0f) == Result::Success);
    Object* targets[] = {bar, barTwin, disc};
    for (auto object : targets) CHECK(scene->add(object) == Result::Success);
    Animation morphs[] = {Animation::morph(square, bar), Animation::morph(twin, barTwin)};
    CHECK(scene->play(morphs, 2u, 1.0f, Easing::Linear) == Result::Success);

    // The morph during the shift is the morph at rest, carried back by the part of the shift
    // still to come, and the bar takes over where both end.
    CHECK(_placed(renderer, scene, square, 1.0f, 0.0f, 0.0f, 1.0f));
    const float times[] = {1.25f, 1.5f, 1.75f};
    for (auto time : times) {
        BBox moving;
        BBox resting;
        CHECK(renderer->bounds(scene, square, time, moving) == Result::Success);
        CHECK(renderer->bounds(scene, twin, time, resting) == Result::Success);
        CHECK(near(moving.center().x, resting.center().x + 20.0f * (2.0f * time - 4.0f), 1.0f));
        CHECK(near(moving.center().y, resting.center().y, 1.0f));
        CHECK(near(moving.width, resting.width, 1.0f) && near(moving.height, resting.height, 1.0f));
    }
    CHECK(_placed(renderer, scene, bar, 2.0f, 2.0f, 2.0f));
    BBox bounds;
    CHECK(renderer->bounds(scene, bar, 2.0f, bounds) == Result::Success);
    CHECK(near(bounds.width, 10.0f, 1.0f) && near(bounds.height, 20.0f, 1.0f));

    // A morph still waits for another property of its source, and cannot add to a motion that
    // flattens it.
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->play(Animation::morph(faded, disc), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "the object already animates its opacity until 2.00 s; begin at that time or later"));
    CHECK(scene->play(Animation::morph(flat, disc), 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "flattens it"));
    delete scene;
}

static void instantUpdates(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto green = Color::hex("#00ff00");
    auto scene = Scene::gen(config);
    auto circle = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto other = Rectangle::gen({2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto group = Group::gen();
    auto heir = Line::gen({-3.0f, -1.5f, 0.0f}, {3.0f, -1.5f, 0.0f});
    auto line = Line::gen({-3.0f, 1.5f, 0.0f}, {3.0f, 1.5f, 0.0f});
    Object* objects[] = {circle, other, group, heir, line};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    circle->stroke(Color{0, 0, 0, 0});
    CHECK(group->add(heir) == Result::Success);
    Object* roots[] = {circle, other, group, line};
    for (auto object : roots) CHECK(scene->add(object) == Result::Success);

    // Before the timeline starts the values become the starting state, without clips.
    CHECK(scene->update(AnimationTarget::from(circle).fill(red).radius(1.0f)) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(group).width(6.0f)) == Result::Success);
    CHECK(scene->clipCount() == 0u);
    CHECK(near(circle->radius, 1.0f) && same(circle->style.fill, red) && near(heir->style.width, 6.0f));
    // A failed update leaves the object as it was.
    heir->style.fill = blue;
    CHECK(scene->update(AnimationTarget::from(group).width(2.0f)) == Result::InvalidArguments);
    CHECK(_says(scene, "takes its color from the group was edited directly"));
    CHECK(near(group->style.width, 6.0f) && near(heir->style.width, 6.0f));
    CHECK(scene->update(heir) == Result::Success);

    // Afterwards they switch at the cursor, for a group's shapes too.
    CHECK(scene->play(AnimationTarget::from(other).opacity(0.5f), 1.0f) == Result::Success);
    auto clips = scene->clipCount();
    CHECK(scene->update(AnimationTarget::from(circle).fill(blue)) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(group).stroke(green)) == Result::Success);
    CHECK(scene->clipCount() == clips + 3u);
    CHECK(near(scene->cursor(), 1.0f) && near(scene->duration(), 1.0f));
    Surface surface;
    CHECK(renderer->render(scene, 0.99f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(red));
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(blue));
    CHECK(_pixelAt(surface, 80u, 70u) == pixel(green));

    // A change at the cursor must not land inside a running animation of the same property,
    // except a change of the transform, which jumps on top of a running motion.
    CHECK(scene->play(AnimationTarget::from(circle).shift({0.0f, 1.0f, 0.0f}).opacity(0.5f), 2.0f, Easing::Linear)
          == Result::Success);
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(circle).opacity(1.0f)) == Result::InsufficientCondition);
    CHECK(_says(scene, "the object already animates its opacity until 3.00 s"));
    CHECK(scene->update(AnimationTarget::from(circle).shift({1.0f, 0.0f, 0.0f})) == Result::Success);
    BBox bounds;
    CHECK(renderer->bounds(scene, circle, 1.99f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 40.0f, 1.0f));
    CHECK(renderer->bounds(scene, circle, 2.5f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 60.0f, 1.0f) && near(bounds.center().y, 25.0f, 1.0f));
    CHECK(renderer->bounds(scene, circle, 3.0f, bounds) == Result::Success);
    CHECK(near(bounds.center().x, 60.0f, 1.0f) && near(bounds.center().y, 20.0f, 1.0f));
    CHECK(near(circle->model.e[3], 1.0f) && near(circle->model.e[7], 1.0f));
    CHECK(scene->update(AnimationTarget::from(circle).fill(red)) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(circle).shift({1.0f, 0.0f, 0.0f}).arc(1.0f)) == Result::InvalidArguments);
    CHECK(_says(scene, "no travel for an arc"));
    CHECK(scene->update(AnimationTarget::from(line).radius(1.0f)) == Result::InvalidArguments);
    CHECK(_says(scene, "radius needs a circle"));
    CHECK(scene->update(AnimationTarget::from(nullptr).opacity(0.5f)) == Result::InvalidArguments);
    CHECK(_says(scene, "not part of this scene"));
    CHECK(scene->remove(line) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(line).opacity(0.5f)) == Result::InvalidArguments);
    CHECK(_says(scene, "removed earlier"));
    delete scene;
}

static Result _strokeCopy(Object* copy, void* data) noexcept
{
    copy->stroke(*static_cast<const Color*>(data));
    return Result::Success;
}

static Result _shiftCopy(Object* copy, void* data) noexcept
{
    copy->shift(*static_cast<const Vec3*>(data));
    return Result::Success;
}

static Result _refuseCopy(Object*, void*) noexcept
{
    return Result::InvalidArguments;
}

static Result _growCopy(Object* copy, void*) noexcept
{
    auto extra = Circle::gen({}, 0.25f);
    if (!extra) return Result::OutOfMemory;
    if (copy->add(extra) == Result::Success) return Result::Success;
    delete extra;
    return Result::InvalidArguments;
}

static bool _sameModel(const Mat4& first, const Mat4& second)
{
    for (auto i = 0u; i < 16u; i++) {
        if (!near(first.e[i], second.e[i])) return false;
    }
    return true;
}

// Whether copy is a new object of the kind of source, in its place, with its colors.
static bool _copies(const Object* copy, const Object* source)
{
    return copy && copy != source && copy->type() == source->type() && _sameModel(copy->model, source->model)
           && same(copy->style.stroke, source->style.stroke) && same(copy->style.fill, source->style.fill)
           && near(copy->style.width, source->style.width) && near(copy->opacity, source->opacity)
           && near(copy->progress, source->progress) && copy->layer == source->layer;
}

static bool _tagged(const Object* object, const char* tag)
{
    return object && (tag ? object->tag() && std::strcmp(object->tag(), tag) == 0 : !object->tag());
}

static void clonedObjects(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto scene = Scene::gen(config);
    auto dot = Circle::gen({-3.0f, 3.0f, 0.0f}, 0.5f);
    auto row = Group::gen();
    auto first = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
    auto second = Rectangle::gen({1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, 0.2f);
    auto rule = Line::gen({-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f});
    auto word = Text::gen("cat", {3.0f, 3.0f, 0.0f});
    auto counter = Text::gen("", {3.0f, -3.0f, 0.0f});
    Vec3 corners[] = {{-4.0f, -4.0f, 0.0f}, {-2.0f, -4.0f, 0.0f}, {-3.0f, -2.5f, 0.0f}};
    auto triangle = Polygon::gen(corners, 3u);
    PathCommand commands[] = {PathCommand::move({0.0f, -4.0f, 0.0f}), PathCommand::line({1.0f, -3.0f, 0.0f}),
                              PathCommand::quadratic({1.5f, -4.5f, 0.0f}, {2.0f, -4.0f, 0.0f})};
    auto path = Path::gen(commands, 3u);
    auto curve = Curve::gen({-1.0f, 2.0f, 0.0f}, {-0.5f, 3.0f, 0.0f}, {0.5f, 3.0f, 0.0f}, {1.0f, 2.0f, 0.0f});
    auto arrow = Arrow::gen({-4.0f, 1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f});
    Object* objects[] = {dot, row, first, second, rule, word, counter, triangle, path, curve, arrow};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    dot->tag("dot");
    row->tag("row");
    first->tag("row/first");
    second->tag("second");
    word->tag("word");
    word->slant = 0.25f;
    word->box = {1.5f, 0.5f};
    word->boxOffset = {0.1f, -0.2f};
    second->stroke(blue);
    arrow->tail = 4.0f;
    CHECK(counter->count(0.0f) == Result::Success);
    CHECK(row->add(first) == Result::Success && row->add(second) == Result::Success && row->add(rule) == Result::Success);
    Object* roots[] = {dot, row, word, counter, triangle, path, curve, arrow};
    for (auto object : roots) CHECK(scene->add(object) == Result::Success);

    // Every kind copies its geometry, style and place into a new object, without its id.
    Object* kinds[] = {triangle, path, curve, arrow};
    for (auto source : kinds) {
        Object* copy = nullptr;
        CHECK(scene->clone(source, copy) == Result::Success && _copies(copy, source) && _tagged(copy, nullptr));
        CHECK(copy && !copy->parent() && copy->id() != source->id());
    }
    auto count = scene->count();
    Object* copy = nullptr;
    CHECK(scene->clone(dot, copy) == Result::Success && _copies(copy, dot) && _tagged(copy, "dot:copy"));
    auto circle = static_cast<Circle*>(copy);
    CHECK(circle && near(circle->radius, 0.5f) && near(circle->center.x, -3.0f) && scene->count() == count + 1u);
    CHECK(scene->clone(dot, copy) == Result::Success && _tagged(copy, "dot:copy2"));
    CHECK(scene->clone(word, copy) == Result::Success && _copies(copy, word) && _tagged(copy, "word:copy"));
    auto text = static_cast<Text*>(copy);
    CHECK(text && std::strcmp(text->text(), "cat") == 0 && near(text->size, word->size) && text->font()
          && std::strcmp(text->font(), word->font()) == 0 && text->point.x == word->point.x);
    // A leaning text stays leaning, so a copy covers its source exactly.
    CHECK(text && text->slant == word->slant && text->box.x == word->box.x && text->box.y == word->box.y);
    CHECK(text && text->boxOffset.x == word->boxOffset.x && text->boxOffset.y == word->boxOffset.y);
    auto path2 = static_cast<Path*>(nullptr);
    CHECK(scene->clone(path, copy) == Result::Success);
    path2 = static_cast<Path*>(copy);
    CHECK(path2 && path2->count() == path->count() && !path2->closed()
          && std::memcmp(path2->points(), path->points(), sizeof(Vec3) * path->count()) == 0);
    CHECK(scene->clone(arrow, copy) == Result::Success && near(static_cast<Arrow*>(copy)->tail, 4.0f));

    // A group copies what is below it, and the ids below follow the copy's.
    CHECK(scene->clone(row, copy) == Result::Success && _tagged(copy, "row:copy") && copy->childCount() == 3u);
    auto rowCopy = copy;
    CHECK(_copies(rowCopy->childAt(0), first) && _tagged(rowCopy->childAt(0), "row:copy/first"));
    CHECK(_copies(rowCopy->childAt(1), second) && _tagged(rowCopy->childAt(1), "row:copy/second"));
    CHECK(_copies(rowCopy->childAt(2), rule) && _tagged(rowCopy->childAt(2), nullptr));
    CHECK(scene->clone(row, copy) == Result::Success && _tagged(copy, "row:copy2")
          && _tagged(copy->childAt(0), "row:copy2/first") && _tagged(copy->childAt(1), "row:copy2/second"));
    // A given id holds for the copy, and the ids below follow it.
    CloneOptions named;
    named.tag = "twin";
    CHECK(scene->clone(row, copy, named) == Result::Success && _tagged(copy, "twin")
          && _tagged(copy->childAt(0), "twin/first") && _tagged(copy->childAt(1), "twin/second"));
    count = scene->count();
    CHECK(scene->clone(row, copy, named) == Result::InvalidArguments && !copy && scene->count() == count);
    CHECK(_says(scene, "already uses this id"));
    // A copy without an id gives those below the number of their copy.
    auto bare = Group::gen();
    auto leaf = Circle::gen({0.0f, 4.0f, 0.0f}, 0.25f);
    CHECK(bare && leaf);
    if (bare && leaf) {
        leaf->tag("leaf");
        CHECK(bare->add(leaf) == Result::Success);
        CHECK(scene->add(bare) == Result::Success);
        CHECK(scene->clone(bare, copy) == Result::Success && _tagged(copy, nullptr)
              && _tagged(copy->childAt(0), "leaf:copy"));
        CHECK(scene->clone(bare, copy) == Result::Success && _tagged(copy->childAt(0), "leaf:copy2"));
    } else {
        delete bare;
        delete leaf;
    }

    // The copy goes beside the source, or below the given parent.
    CHECK(scene->clone(second, copy) == Result::Success && copy->parent() == row && row->childCount() == 4u
          && row->childAt(3) == copy && _tagged(copy, "second:copy"));
    CloneOptions placed;
    placed.parent = rowCopy;
    CHECK(scene->clone(dot, copy, placed) == Result::Success && copy->parent() == rowCopy
          && rowCopy->childCount() == 4u);
    auto stray = Circle::gen({}, 1.0f);
    placed.parent = stray;
    CHECK(scene->clone(dot, copy, placed) == Result::InvalidArguments && _says(scene, "parent is not part"));
    CHECK(scene->clone(stray, copy) == Result::InvalidArguments && _says(scene, "not part of this scene"));
    CHECK(!scene->shows(stray) && !scene->shows(nullptr));
    delete stray;

    // The edit changes the copy's own settings before it is attached, and a group passes
    // its colors to the copies below that take them from it.
    CloneOptions painted;
    painted.edit = _strokeCopy;
    painted.data = &red;
    CHECK(scene->clone(row, copy, painted) == Result::Success && same(copy->style.stroke, red));
    CHECK(same(copy->childAt(0)->style.stroke, red) && same(copy->childAt(2)->style.stroke, red));
    CHECK(same(copy->childAt(1)->style.stroke, blue) && !same(first->style.stroke, red));
    count = scene->count();
    painted.edit = _refuseCopy;
    CHECK(scene->clone(row, copy, painted) == Result::InvalidArguments && !copy && scene->count() == count);
    CHECK(_says(scene, "settings could not be changed"));
    painted.edit = _growCopy;
    CHECK(scene->clone(row, copy, painted) == Result::InvalidArguments && !copy && scene->count() == count);
    CHECK(_says(scene, "not the objects below it"));

    // A copy takes no color from the theme: the next shape takes the color the copy left.
    auto fresh = Scene::gen(config);
    auto a = Circle::gen({}, 1.0f);
    auto b = Circle::gen({}, 1.0f);
    CHECK(fresh && a && b);
    if (fresh && a && b) {
        CHECK(fresh->add(a) == Result::Success && fresh->clone(a, copy) == Result::Success);
        CHECK(fresh->add(b) == Result::Success);
        CHECK(same(copy->style.stroke, a->style.stroke) && same(b->style.stroke, fresh->theme().objects[1]));
    } else {
        delete a;
        delete b;
    }
    delete fresh;

    // Mid-timeline the copy takes the state at the cursor and shows from there on, with no
    // animation of its own.
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    CHECK(scene->play(AnimationTarget::from(dot).shift({2.0f, 0.0f, 0.0f}).opacity(0.5f), 2.0f, linear)
          == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(word).text("horse"), 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.0f) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(counter).value(100.0f), 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    auto clips = scene->clipCount();
    CHECK(scene->clone(dot, copy) == Result::Success && near(copy->model.e[3], 0.5f) && near(copy->opacity, 0.875f));
    auto moving = copy;
    CHECK(scene->clone(word, copy) == Result::Success && std::strcmp(static_cast<Text*>(copy)->text(), "hots") == 0);
    CHECK(static_cast<Text*>(copy)->texts(nullptr, 0u) == 1u);
    CHECK(scene->clone(counter, copy) == Result::Success && static_cast<Text*>(copy)->counting());
    CHECK(near(static_cast<Text*>(copy)->value(), 50.0f) && std::strcmp(static_cast<Text*>(copy)->text(), "50") == 0);
    CHECK(scene->clipCount() == clips);
    BBox bounds;
    CHECK(renderer->bounds(scene, moving, 0.25f, bounds) == Result::InsufficientCondition);
    CHECK(_placed(renderer, scene, moving, 0.5f, -2.5f, 3.0f));
    CHECK(_placed(renderer, scene, moving, 2.0f, -2.5f, 3.0f));
    CHECK(_placed(renderer, scene, dot, 2.0f, -1.0f, 3.0f));

    // A reveal played on the copy hides it until the reveal, as for any object added there.
    CHECK(scene->seek(2.0f) == Result::Success);
    Vec3 down = {0.0f, -6.0f, 0.0f};
    CloneOptions moved;
    moved.edit = _shiftCopy;
    moved.data = &down;
    CHECK(scene->clone(dot, copy, moved) == Result::Success && near(copy->model.e[7], -6.0f));
    auto revealed = copy;
    CHECK(scene->wait(0.5f) == Result::Success);
    CHECK(scene->play(Animation::fadeIn(revealed), 1.0f, linear) == Result::Success);
    CHECK(renderer->bounds(scene, revealed, 2.25f, bounds) == Result::InsufficientCondition);
    CHECK(_placed(renderer, scene, revealed, 3.5f, -1.0f, -3.0f));

    // What below the source no longer shows is copied, but never shows.
    CHECK(scene->remove(first) == Result::Success);
    CHECK(scene->clone(row, copy) == Result::Success && copy->childCount() == 4u);
    CHECK(renderer->bounds(scene, copy->childAt(0), 3.5f, bounds) == Result::InsufficientCondition);
    CHECK(renderer->bounds(scene, copy->childAt(1), 3.5f, bounds) == Result::Success);
    CHECK(!scene->shows(copy->childAt(0)) && scene->shows(copy->childAt(1)) && scene->shows(copy));
    CHECK(scene->shows(first));
    CHECK(scene->clone(first, copy) == Result::InsufficientCondition && _says(scene, "does not show at the cursor"));
    delete scene;
}

static void clonedLinks(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    auto net = Group::gen();
    auto left = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto right = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto far = Circle::gen({0.0f, 4.0f, 0.0f}, 0.5f);
    auto inner = Connector::gen(left, right);
    auto outer = Connector::gen(left, far);
    auto chain = Group::gen();
    auto start = Circle::gen({-4.0f, 4.0f, 0.0f}, 0.25f);
    auto end = Circle::gen({-3.0f, 4.0f, 0.0f}, 0.25f);
    auto link = Connector::gen(start, end);
    auto chained = Connector::gen(link, far);
    auto window = Group::gen();
    auto stencil = Rectangle::gen({0.0f, -3.0f, 0.0f}, {2.0f, 1.0f});
    auto masked = Circle::gen({0.0f, -3.0f, 0.0f}, 1.0f);
    auto space = Space::gen();
    Object* objects[] = {net,   left, right,  far,   inner,   outer,  chain,
                         start, end,  link,   chained, window, stencil, masked, space};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    CHECK(net->add(left) == Result::Success && net->add(right) == Result::Success);
    CHECK(net->add(inner) == Result::Success && net->add(outer) == Result::Success);
    CHECK(scene->add(far) == Result::Success && scene->add(net) == Result::Success);
    CHECK(window->add(stencil) == Result::Success && window->add(masked) == Result::Success);
    CHECK(scene->add(window) == Result::Success && masked->mask(stencil) == Result::Success);
    CHECK(scene->add(space) == Result::Success);

    // A connector joins the copies of the endpoints it has below the source, and keeps the
    // others.
    Object* copy = nullptr;
    CHECK(scene->clone(net, copy) == Result::Success && copy->childCount() == 4u);
    auto innerCopy = static_cast<Connector*>(copy->childAt(2));
    auto outerCopy = static_cast<Connector*>(copy->childAt(3));
    CHECK(innerCopy->from() == copy->childAt(0) && innerCopy->to() == copy->childAt(1));
    CHECK(outerCopy->from() == copy->childAt(0) && outerCopy->to() == far);
    CHECK(scene->clone(inner, copy) == Result::Success && static_cast<Connector*>(copy)->from() == left
          && copy->parent() == net);
    // A connector that ends at another connector below the source is not copied yet; one that
    // ends at a connector elsewhere keeps it.
    CHECK(chain->add(start) == Result::Success && chain->add(end) == Result::Success);
    CHECK(chain->add(link) == Result::Success && chain->add(chained) == Result::Success);
    CHECK(scene->add(chain) == Result::Success);
    auto count = scene->count();
    CHECK(scene->clone(chain, copy) == Result::NonSupport && _says(scene, "ends at another connector"));
    CHECK(scene->count() == count);
    CHECK(scene->clone(chained, copy) == Result::Success && static_cast<Connector*>(copy)->from() == link);
    // A mask and what it masks are copied together.
    CHECK(scene->clone(window, copy) == Result::Success);
    CHECK(copy->childAt(1)->mask() == copy->childAt(0) && masked->mask() == stencil);
    CHECK(scene->clone(masked, copy) == Result::InvalidArguments && _says(scene, "cloned together"));
    CHECK(scene->clone(stencil, copy) == Result::InvalidArguments);
    // Kinds that hold more than their own configuration are not copied yet.
    CHECK(scene->clone(space, copy) == Result::NonSupport && _says(scene, "a space cannot be cloned yet"));
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);

    // A source on its way through a morph cannot be copied until the morph ends.
    Vec3 corners[] = {{-4.0f, -4.0f, 0.0f}, {-3.0f, -4.0f, 0.0f}, {-3.5f, -3.0f, 0.0f}};
    Vec3 others[] = {{3.0f, -4.0f, 0.0f}, {4.0f, -4.0f, 0.0f}, {3.5f, -3.0f, 0.0f}};
    auto from = Polygon::gen(corners, 3u);
    auto to = Polygon::gen(others, 3u);
    CHECK(from && to && scene->add(from) == Result::Success && scene->add(to) == Result::Success);
    CHECK(scene->play(Animation::replacementTransform(from, to), 1.0f) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    CHECK(scene->clone(from, copy) == Result::InsufficientCondition && _says(scene, "middle of a morph"));
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->clone(from, copy) == Result::InsufficientCondition && _says(scene, "does not show"));
    CHECK(scene->clone(to, copy) == Result::Success);
    delete scene;
}

// Matching carries the currently painted billboard size, including earlier model scaling.
// Repeated samples on either side of the handoff remain stable in one retained scene.
static void matchedBillboard(SwRenderer* renderer)
{
    for (auto copying : {false, true}) {
        for (auto guided : {false, true}) {
            Config config;
            config.width = 960; config.height = 540;
            config.camera.orthoHeight = 540.0f;
            auto scene = Scene::gen(config);
            auto source = Text::gen("copy", {-200.0f, 0.0f, 0.0f});
            auto target = Text::gen("copy", {200.0f, 0.0f, 0.0f});
            CHECK(scene && source && target);
            if (!scene || !source || !target) { delete scene; delete source; delete target; continue; }
            source->size = target->size = 40.0f;
            CHECK(source->font("Pretendard") == Result::Success && target->font("Pretendard") == Result::Success);
            CHECK(scene->add(source) == Result::Success && scene->wait(0.1f) == Result::Success);
            CHECK(scene->play(Animation::rescale(source, {0.2f, 0.2f, 0.2f}), 0.2f, Easing::Linear) == Result::Success);
            CHECK(scene->wait(0.2f) == Result::Success && scene->add(target) == Result::Success);
            BBox before; BBox destination; BBox after;
            CHECK(renderer->bounds(scene, source, 0.499f, before) == Result::Success);
            CHECK(renderer->bounds(scene, target, 0.5f, destination) == Result::Success);
            MatchOptions options;
            options.copy = copying;
            if (guided) {
                Vec3 points[] = {{-200.0f, 0.0f, 0.0f}, {0.0f, 80.0f, 0.0f}, {200.0f, 0.0f, 0.0f}};
                auto path = DirectedRoute::gen(points, 3u);
                CHECK(path && scene->add(path) == Result::Success);
                options.path = path;
            }
            CHECK(scene->matchTransform(source, target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
                  == Result::Success);
            for (auto time : {1.5f, 0.5f, 0.499f, 0.5f}) {
                auto object = time == 0.499f ? source : target;
                CHECK(renderer->bounds(scene, object, time, after) == Result::Success);
                auto& expected = time == 1.5f ? destination : before;
                CHECK(near(after.x, expected.x, 0.01f) && near(after.y, expected.y, 0.01f)
                      && near(after.width, expected.width, 0.01f) && near(after.height, expected.height, 0.01f));
            }
            if (copying) {
                CHECK(renderer->bounds(scene, source, 1.5f, after) == Result::Success);
                CHECK(near(after.width, before.width, 0.01f) && near(after.height, before.height, 0.01f));
            }
            delete scene;
        }
    }
}

// A match guide contributes the same world-space detour to every piece, while each keeps
// its own endpoints. Nonuniform parents do not reparameterize that shared path.
static void matchedPath(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    auto scene = Scene::gen(config);
    auto from = Group::gen();
    auto to = Group::gen();
    auto guideGroup = Group::gen();
    auto a = Circle::gen({-2.0f, -1.0f, 0.0f}, 0.2f);
    auto b = Circle::gen({0.0f, -1.0f, 0.0f}, 0.2f);
    auto c = Circle::gen({0.0f, 1.0f, 0.0f}, 0.2f);
    auto d = Circle::gen({1.0f, 1.0f, 0.0f}, 0.2f);
    Vec3 points[] = {{0.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, {1.0f, 2.0f, 0.0f}};
    auto guide = DirectedRoute::gen(points, 3u);
    CHECK(scene && from && to && guideGroup && a && b && c && d && guide);
    if (!scene || !from || !to || !guideGroup || !a || !b || !c || !d || !guide) {
        delete scene; delete from; delete to; delete guideGroup;
        delete a; delete b; delete c; delete d; delete guide;
        return;
    }
    to->model = Mat4::scale({2.0f, 1.0f, 1.0f});
    guideGroup->model = Mat4::translate({100000000.0f, 0.0f, 0.0f}) * to->model;
    guide->opacity = 0.0f;
    CHECK(from->add(a) == Result::Success && from->add(b) == Result::Success);
    CHECK(to->add(c) == Result::Success && to->add(d) == Result::Success);
    CHECK(guideGroup->add(guide) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->add(guideGroup) == Result::Success);
    CHECK(scene->wait(1.0f) == Result::Success && scene->add(to) == Result::Success);
    MatchOptions options;
    options.copy = true;
    options.shapes = true;
    options.path = guide;
    auto count = scene->count();
    auto clips = scene->clipCount();
    options.arc = 1.0f;
    CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::InvalidArguments);
    CHECK(scene->count() == count && scene->clipCount() == clips && near(scene->cursor(), 1.0f));
    options.arc = 0.0f;
    options.path = from;
    CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::InvalidArguments);
    CHECK(scene->count() == count && scene->clipCount() == clips && near(scene->cursor(), 1.0f));
    options.path = guide;
    CHECK(scene->matchTransform(from, to, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), options)
          == Result::Success);
    // Moving the guide later cannot change the retained match; sample out of order.
    CHECK(scene->play(Animation::shift(guideGroup, {1.0f, -1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(_placed(renderer, scene, c, 2.0f, 0.0f, 1.0f) && _placed(renderer, scene, d, 2.0f, 2.0f, 1.0f));
    CHECK(_placed(renderer, scene, c, 1.5f, -2.0f, 1.0f) && _placed(renderer, scene, d, 1.5f, 0.0f, 1.0f));
    CHECK(_placed(renderer, scene, c, 1.0f, -2.0f, -1.0f) && _placed(renderer, scene, d, 1.0f, 0.0f, -1.0f));
    CHECK(_placed(renderer, scene, c, 1.25f, -2.0f, 0.0f) && _placed(renderer, scene, d, 1.75f, 1.0f, 1.0f));
    CHECK(_placed(renderer, scene, a, 1.5f, -2.0f, -1.0f) && _placed(renderer, scene, b, 3.0f, 0.0f, -1.0f));
    delete scene;
}

// Copied transforms animate a cursor-time clone and leave the source in place.
static void copiedTransforms(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    Vec3 corners[] = {{-4.0f, 2.0f, 0.0f}, {-2.0f, 2.0f, 0.0f}, {-2.0f, 4.0f, 0.0f}, {-4.0f, 4.0f, 0.0f}};
    auto square = Polygon::gen(corners, 4u);
    auto disc = Circle::gen({3.0f, 3.0f, 0.0f}, 1.0f);
    auto spare = Circle::gen({3.0f, -1.0f, 0.0f}, 1.0f);
    auto left = Circle::gen({-3.0f, -3.0f, 0.0f}, 0.5f);
    auto right = Circle::gen({3.0f, -3.0f, 0.0f}, 0.5f);
    auto gone = Circle::gen({0.0f, -4.0f, 0.0f}, 0.5f);
    auto into = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
    auto other = Circle::gen({1.0f, 0.0f, 0.0f}, 0.5f);
    auto from = Group::gen();
    auto to = Group::gen();
    auto moving = Group::gen();
    auto p = Circle::gen({-3.0f, -2.0f, 0.0f}, 0.5f);
    auto a = Text::gen("a", {-1.0f, -2.0f, 0.0f});
    auto q = Circle::gen({3.0f, -4.0f, 0.0f}, 0.5f);
    auto a2 = Text::gen("a", {1.0f, -4.0f, 0.0f});
    auto d = Text::gen("a", {0.0f, 4.0f, 0.0f});
    Object* objects[] = {square, disc, spare, left, right, gone, into, other, from, to, moving, p, a, q, a2, d};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    square->tag("square");
    from->tag("from");
    CHECK(scene->add(square) == Result::Success && scene->add(spare) == Result::Success);
    CHECK(scene->add(disc) == Result::Success);
    auto morph = Animation::morph(square, disc);
    morph.copy = true;
    CHECK(scene->play(morph, 1.0f) == Result::Success && scene->wait(1.0f) == Result::Success);
    auto copy = scene->object("square:copy");
    CHECK(copy && copy->type() == Type::Polygon && copy->parent() == square->parent());
    // The square stays; its copy turns into the disc and leaves as the disc arrives.
    BBox bounds;
    CHECK(_placed(renderer, scene, square, 0.5f, -3.0f, 3.0f) && _placed(renderer, scene, square, 1.5f, -3.0f, 3.0f));
    CHECK(renderer->bounds(scene, copy, 0.5f, bounds) == Result::Success);
    CHECK(bounds.center().x > 60.0f && bounds.center().x < 140.0f);
    CHECK(renderer->bounds(scene, copy, 1.5f, bounds) != Result::Success);
    CHECK(renderer->bounds(scene, disc, 0.5f, bounds) != Result::Success);
    CHECK(_placed(renderer, scene, disc, 1.5f, 3.0f, 3.0f));

    // Only a morph plays from a copy, and a play that fails leaves no copy behind.
    auto count = scene->count();
    auto fade = Animation::fade(square, 0.5f);
    fade.copy = true;
    CHECK(scene->play(fade, 1.0f) == Result::InvalidArguments && _says(scene, "only a morph can play from a copy"));
    auto stale = Animation::morph(square, spare);
    stale.copy = true;
    CHECK(scene->play(stale, 1.0f) == Result::InsufficientCondition && _says(scene, "right before the morph"));
    CHECK(scene->count() == count && !scene->object("square:copy2"));
    // One object may give a copy to each of several morphs at once.
    CHECK(scene->add(left) == Result::Success && scene->add(right) == Result::Success);
    Animation both[] = {Animation::morph(square, left), Animation::morph(square, right)};
    both[0].copy = both[1].copy = true;
    CHECK(scene->play(both, 2u, 1.0f, AnimCurve{}, 0.0f) == Result::Success);
    CHECK(scene->object("square:copy2") && scene->object("square:copy3"));
    // When a later copy cannot be made, the earlier ones go again.
    CHECK(scene->add(gone) == Result::Success && scene->wait(0.5f) == Result::Success);
    CHECK(scene->remove(gone) == Result::Success);
    CHECK(scene->add(into) == Result::Success && scene->add(other) == Result::Success);
    count = scene->count();
    Animation broken[] = {Animation::morph(square, into), Animation::morph(gone, other)};
    broken[0].copy = broken[1].copy = true;
    CHECK(scene->play(broken, 2u, 1.0f, AnimCurve{}, 0.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "does not show at the cursor"));
    CHECK(scene->count() == count && !scene->object("square:copy4"));

    // A match from a copy leaves its source in place; pairs still name pieces of the source,
    // so the target circle starts where the source circle stands and travels to its own place.
    CHECK(from->add(p) == Result::Success && from->add(a) == Result::Success);
    CHECK(scene->add(from) == Result::Success && scene->wait(1.0f) == Result::Success);
    CHECK(to->add(q) == Result::Success && to->add(a2) == Result::Success && scene->add(to) == Result::Success);
    auto start = scene->cursor();
    MatchOptions options;
    options.copy = true;
    Object* pairs[] = {p, q};
    options.pairs = pairs;
    options.pairCount = 1u;
    CHECK(scene->matchTransform(from, to, 1.0f, {}, options) == Result::Success);
    CHECK(scene->wait(1.0f) == Result::Success);
    auto fromCopy = scene->object("from:copy");
    CHECK(fromCopy && fromCopy->childCount() == 2u && fromCopy->parent() == from->parent());
    if (fromCopy && fromCopy->childCount() == 2u) {
        auto middle = start + 0.5f;
        auto after = start + 1.5f;
        auto copied = fromCopy->childAt(0);
        CHECK(_placed(renderer, scene, p, middle, -3.0f, -2.0f) && _placed(renderer, scene, p, after, -3.0f, -2.0f));
        CHECK(_placed(renderer, scene, q, middle, 0.0f, -3.0f) && _placed(renderer, scene, q, after, 3.0f, -4.0f));
        // The copy of a circle that pairs with a circle leaves at once, as the circle takes its place.
        CHECK(renderer->bounds(scene, copied, middle, bounds) != Result::Success);
        // The source meets the copy and the target on the way, as the copy meets the target.
        CHECK(scene->handoff(a, a2, middle) && scene->handoff(p, copied, middle) && scene->handoff(copied, q, middle));
        CHECK(!scene->handoff(a, q, middle) && !scene->handoff(p, a2, middle));
        CHECK(!scene->handoff(a, copied, middle) && !scene->handoff(p, fromCopy->childAt(1), middle));
        CHECK(!scene->handoff(p, q, after) && !scene->handoff(p, copied, after));
        // The pieces the match lets go of at once never show, so the audit passes over them.
        CHECK(!scene->shows(copied) && !scene->shows(fromCopy->childAt(1)));
        CHECK(scene->shows(fromCopy) && scene->shows(p) && scene->shows(a) && scene->shows(a2));
    }
    // A copied shape may split into several targets. Each extra meets the corresponding
    // original, but unrelated siblings and labels retain ordinary collision checks.
    auto splitFrom = Group::gen();
    auto splitTo = Group::gen();
    auto seed = Circle::gen({-1.0f, 0.0f, 0.0f}, 0.5f);
    auto first = Circle::gen({1.0f, 1.0f, 0.0f}, 0.5f);
    auto extra = Circle::gen({1.0f, -1.0f, 0.0f}, 0.5f);
    auto sibling = Text::gen("unrelated", {0.0f, 2.0f, 0.0f});
    CHECK(splitFrom && splitTo && seed && first && extra && sibling);
    CHECK(splitFrom->add(seed) == Result::Success && splitFrom->add(sibling) == Result::Success);
    CHECK(splitTo->add(first) == Result::Success && splitTo->add(extra) == Result::Success);
    CHECK(scene->add(splitFrom) == Result::Success && scene->add(splitTo) == Result::Success);
    auto splitTime = scene->cursor();
    MatchOptions split;
    split.copy = true;
    split.shapes = true;
    split.strings = false;
    CHECK(scene->matchTransform(splitFrom, splitTo, 1.0f, {}, split) == Result::Success);
    CHECK(scene->handoff(seed, first, splitTime + 0.5f) && scene->handoff(seed, extra, splitTime + 0.5f));
    CHECK(!scene->handoff(sibling, first, splitTime + 0.5f) && !scene->handoff(sibling, extra, splitTime + 0.5f));
    // The source may be moving, since its copy starts still; a failed match leaves no copy.
    CHECK(scene->play(Animation::shift(from, {0.0f, 1.0f, 0.0f}), 1.0f) == Result::Success);
    CHECK(scene->seek(scene->cursor() - 0.5f) == Result::Success);
    CHECK(moving->add(d) == Result::Success && scene->add(moving) == Result::Success);
    CHECK(scene->matchTransform(from, moving, 1.0f) == Result::InsufficientCondition && _says(scene, "at rest"));
    MatchOptions copying;
    copying.copy = true;
    count = scene->count();
    CHECK(scene->matchTransform(from, to, 1.0f, {}, copying) == Result::InsufficientCondition);
    CHECK(_says(scene, "right before the match") && scene->count() == count && !scene->object("from:copy2"));
    CHECK(scene->matchTransform(from, moving, 1.0f, {}, copying) == Result::Success && scene->object("from:copy2"));
    delete scene;
}

// A warped clone retains sampled geometry; later warps start from that snapshot.
static void clonedWarps(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen(config);
    auto line = Line::gen({-1.5f, 2.0f, 0.0f}, {1.5f, 2.0f, 0.0f});
    auto pair = Group::gen();
    auto ring = Circle::gen({0.0f, -2.0f, 0.0f}, 1.0f);
    auto rider = Point::gen({-4.0f, -4.0f, 0.0f});
    auto rule = Line::gen({-4.0f, -3.0f, 0.0f}, {-2.0f, -3.0f, 0.0f});
    Object* objects[] = {line, pair, ring, rider, rule};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    line->tag("line");
    line->stroke(Color::hex("#ffffff"), 1.0f);
    CHECK(pair->add(ring) == Result::Success);
    CHECK(scene->add(line) == Result::Success && scene->add(pair) == Result::Success);
    CHECK(scene->add(rider) == Result::Success);
    Object* bent[] = {line, pair};
    CHECK(scene->warp(bent, 2u, _lifted, nullptr, 1.0f, linear) == Result::Success);

    // Three units right of the source, the copy of the line is the parabola y = 2 + x^2 moved
    // with it, and the copy of the group holds the lifted circle, which spans -3 to -0.75.
    Vec3 right = {3.0f, 0.0f, 0.0f};
    CloneOptions moved;
    moved.edit = _shiftCopy;
    moved.data = &right;
    Object* copy = nullptr;
    CHECK(scene->clone(line, copy, moved) == Result::Success);
    auto curve = copy;
    CHECK(scene->clone(pair, copy, moved) == Result::Success && copy && copy->childCount() == 1u);
    auto lifted = copy && copy->childCount() == 1u ? copy->childAt(0) : nullptr;
    if (!curve || !lifted) {
        delete rule;
        delete scene;
        return;
    }
    Bounds box;
    CHECK(curve->bounds(box) && near(box.min.x, 1.5f) && near(box.max.x, 4.5f) && near(box.min.y, 2.0f)
          && near(box.max.y, 4.25f));
    CHECK(lifted->bounds(box) && near(box.min.y, -3.0f, 2.0e-3f) && near(box.max.y, -0.75f, 2.0e-3f));
    // It draws the images from the moment it is attached, while the source still shows its
    // own flat line before its warp.
    BBox drawn;
    CHECK(renderer->bounds(scene, curve, 0.5f, drawn) == Result::InsufficientCondition);
    CHECK(_placed(renderer, scene, curve, 1.0f, 3.0f, 3.125f) && _placed(renderer, scene, lifted, 1.0f, 3.0f, -1.875f));
    CHECK(renderer->bounds(scene, curve, 1.0f, drawn) == Result::Success && near(drawn.height, 45.0f, 2.0f));
    CHECK(renderer->bounds(scene, line, 0.0f, drawn) == Result::Success && drawn.height < 3.0f);

    // A warp of the copy goes on from the images, and one of the source's group leaves the
    // copy of its circle where it is.
    auto down = Vec3{0.0f, -1.0f, 0.0f};
    Object* again[] = {curve, pair};
    CHECK(scene->warp(again, 2u, _moved, &down, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, curve, 1.5f, 3.0f, 2.625f) && _placed(renderer, scene, curve, 2.0f, 3.0f, 2.125f));
    CHECK(_placed(renderer, scene, ring, 2.0f, 0.0f, -2.875f) && _placed(renderer, scene, lifted, 2.0f, 3.0f, -1.875f));
    // Its outline is the images too: a dot moved along it ends where the moved parabola ends.
    CHECK(scene->play(Animation::moveAlong(rider, curve), 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, rider, 3.0f, 4.5f, 3.25f));
    // The copied circle keeps the outline its warp gave, so its radius no longer plays, and a
    // copy of the copy takes the images of its latest warp.
    CHECK(scene->play(AnimationTarget::from(lifted).radius(2.0f), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keeps the outline its warp gave"));
    CHECK(scene->clone(curve, copy) == Result::Success && copy->bounds(box) && near(box.max.y, 3.25f));

    // A morph from a copy of the line starts from the parabola, in the line's place.
    CHECK(scene->add(rule) == Result::Success);
    auto morph = Animation::morph(line, rule);
    morph.copy = true;
    auto start = scene->cursor();
    CHECK(scene->play(morph, 1.0f, linear) == Result::Success);
    auto morphed = scene->object("line:copy2");
    CHECK(morphed && _placed(renderer, scene, morphed, start, 0.0f, 3.125f));
    CHECK(morphed && renderer->bounds(scene, morphed, start, drawn) == Result::Success
          && near(drawn.height, 45.0f, 2.0f));
    // A morph into a copy of the line made at the cursor ends in the parabola, which the copy
    // then shows.
    auto flat = Line::gen({-1.5f, -4.0f, 0.0f}, {1.5f, -4.0f, 0.0f});
    CHECK(flat && scene->add(flat) == Result::Success && scene->clone(line, copy) == Result::Success);
    start = scene->cursor();
    CHECK(scene->play(Animation::morph(flat, copy), 1.0f, linear) == Result::Success);
    CHECK(renderer->bounds(scene, flat, start + 0.999f, drawn) == Result::Success && near(drawn.height, 45.0f, 2.0f));
    CHECK(_placed(renderer, scene, copy, start + 1.0f, 0.0f, 3.125f));
    CHECK(renderer->bounds(scene, copy, start + 1.0f, drawn) == Result::Success && near(drawn.height, 45.0f, 2.0f));

    // While a warp runs, neither its object nor a group above it is copied, and a refused clone
    // attaches nothing.
    CHECK(scene->seek(1.5f) == Result::Success);
    auto count = scene->count();
    CHECK(scene->clone(curve, copy) == Result::InsufficientCondition && _says(scene, "middle of a warp"));
    CHECK(scene->clone(pair, copy) == Result::InsufficientCondition && _says(scene, "middle of a warp"));
    CHECK(scene->count() == count);
    delete scene;
}

// How many pixels of row y, from x0 up to x1, have a red channel above 64.
static uint32_t _litRun(const Surface& frame, uint32_t y, uint32_t x0, uint32_t x1)
{
    auto lit = 0u;
    for (auto x = x0; x < x1; x++) lit += framePixel(frame, x, y).r > 64u ? 1u : 0u;
    return lit;
}

// A copy starts from what its source shows at the cursor, also the parts no field of it holds:
// how much of its fill shows, the scale of its pixel sizes, and how its outline draws. A layout
// that moves it before it animates, and a match into it, read its fields into its starting state
// again and keep those parts.
static void rebasedCopies(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen(config);
    auto box = Rectangle::gen({-3.0f, 3.0f, 0.0f}, {2.0f, 1.0f});
    Vec3 corners[] = {{-4.0f, -1.0f, 0.0f}, {-2.0f, -1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f}, {-4.0f, 1.0f, 0.0f}};
    auto square = Polygon::gen(corners, 4u);
    auto ring = Circle::gen({3.0f, 3.0f, 0.0f}, 1.5f);
    auto seed = Circle::gen({-3.0f, -4.0f, 0.0f}, 0.5f);
    Object* objects[] = {box, square, ring, seed};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    box->fill(Color::hex("#ff0000"));
    square->fill(Color::hex("#0000ff"));
    ring->stroke(Color::hex("#ffffff"), 8.0f);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);
    Animation animations[] = {Animation::fillReveal(box), Animation::write(square, DrawDirection::Reverse),
                              Animation::growFromCenter(ring)};
    CHECK(scene->play(animations, 3u, 2.0f, linear, 0.0f) == Result::Success);

    // Halfway, a copy of the box shows half its fill, and one of the square the half written
    // backwards from its first corner, filled as Write fills it. Arranged side by side, they
    // grow into their places as they are.
    CHECK(scene->seek(1.0f) == Result::Success);
    Object* twin = nullptr;
    Object* scribe = nullptr;
    CHECK(scene->clone(box, twin) == Result::Success && scene->clone(square, scribe) == Result::Success);
    Object* items[] = {twin, scribe};
    CHECK(twin && scribe
          && scene->arrange(items, 2u, {1.0f, 0.0f, 0.0f}, 1.0f, {2.0f, 0.0f, 0.0f}, 0.5f, linear)
                 == Result::Success);
    // Matched into, a copy of the circle half grown keeps the stroke it has at that size, half
    // its own 8 pixels.
    CHECK(scene->seek(1.0f) == Result::Success);
    Vec3 down = {0.0f, -6.0f, 0.0f};
    CloneOptions moved;
    moved.edit = _shiftCopy;
    moved.data = &down;
    Object* hoop = nullptr;
    CHECK(scene->clone(ring, hoop, moved) == Result::Success);
    CHECK(hoop && scene->matchTransform(seed, hoop, 0.5f, linear) == Result::Success);

    Surface frame;
    CHECK(renderer->render(scene, 2.5f, frame) == Result::Success);
    if (frame.data() && twin && scribe && hoop) {
        CHECK(_placed(renderer, scene, twin, 2.5f, 0.5f, 0.0f));
        CHECK(_placed(renderer, scene, scribe, 2.5f, 3.5f, 0.0f));
        CHECK(close(framePixel(frame, 110u, 100u), {128, 0, 0, 255}, 2));
        CHECK(close(framePixel(frame, 156u, 86u), {0, 0, 255, 255}, 0));
        CHECK(close(framePixel(frame, 184u, 114u), {0, 0, 0, 255}, 0));
        auto stroke = _litRun(frame, 160u, 135u, 155u);
        CHECK(stroke >= 3u && stroke <= 5u);
        // The sources played on, unchanged.
        CHECK(close(framePixel(frame, 40u, 40u), {255, 0, 0, 255}, 0));
        CHECK(_litRun(frame, 40u, 120u, 145u) >= 7u);
    }
    delete scene;
}

// The points a geometry clip moves meet clones, warps and views: a copy keeps the points its
// source shows as its own, a warp maps the points the clips end at and waits for a clip that
// runs, a warped shape takes no new points, and a view draws them on their way.
static void geometryCopies(SwRenderer* renderer)
{
    Config config;
    config.width = 200;
    config.height = 200;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen(config);
    auto line = Line::gen({-4.0f, -2.0f, 0.0f}, {-2.0f, -2.0f, 0.0f});
    auto curve = Curve::gen({1.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}, {3.0f, -2.0f, 0.0f}, {4.0f, -2.0f, 0.0f}, 30u);
    auto rule = Line::gen({1.0f, 3.0f, 0.0f}, {4.0f, 3.0f, 0.0f});
    Object* objects[] = {line, curve, rule};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    line->tag("line");
    line->strokeWidth(1.0f);
    curve->tag("curve");
    curve->strokeWidth(1.0f);
    CHECK(scene->add(line) == Result::Success && scene->add(curve) == Result::Success);

    // Over the first second the line rises from y = -2 to y = 2 and the curve bends up.
    const Vec3 raised[] = {{-4.0f, 2.0f, 0.0f}, {-2.0f, 2.0f, 0.0f}};
    const Vec3 bent[] = {{1.0f, -2.0f, 0.0f}, {2.0f, 2.0f, 0.0f}, {3.0f, 2.0f, 0.0f}, {4.0f, -2.0f, 0.0f}};
    AnimationTarget targets[] = {
        AnimationTarget::from(line).geometry(raised, 2u),
        AnimationTarget::from(curve).geometry(bent, 4u),
    };
    CHECK(scene->play(targets, 2u, 1.0f, linear) == Result::Success);

    // Halfway, a copy of each takes the points its source shows as its own: the line at y = 0
    // and the curve with its controls at y = 0, whose middle point lies at y = -0.5.
    CHECK(scene->seek(0.5f) == Result::Success);
    Object* copy = nullptr;
    CHECK(scene->clone(line, copy) == Result::Success);
    auto level = copy && copy->type() == Type::Line ? static_cast<Line*>(copy) : nullptr;
    copy = nullptr;
    CHECK(scene->clone(curve, copy) == Result::Success);
    auto arch = copy && copy->type() == Type::Curve ? static_cast<Curve*>(copy) : nullptr;
    if (!level || !arch) {
        delete rule;
        delete scene;
        return;
    }
    CHECK(near(level->from.x, -4.0f) && near(level->from.y, 0.0f) && near(level->to.y, 0.0f));
    CHECK(near(arch->controls()[0].y, -2.0f) && near(arch->controls()[1].y, 0.0f));
    CHECK(near(arch->controls()[2].x, 3.0f) && near(arch->controls()[2].y, 0.0f));
    CHECK(arch->count() == 31u && near(arch->points()[15].x, 2.5f) && near(arch->points()[15].y, -0.5f));
    // The copies stay there while the sources go on.
    CHECK(_placed(renderer, scene, level, 0.5f, -3.0f, 0.0f) && _placed(renderer, scene, line, 0.5f, -3.0f, 0.0f));
    CHECK(_placed(renderer, scene, level, 1.0f, -3.0f, 0.0f) && _placed(renderer, scene, line, 1.0f, -3.0f, 2.0f));
    CHECK(_placed(renderer, scene, arch, 1.0f, 2.5f, -1.25f) && _placed(renderer, scene, curve, 1.0f, 2.5f, -0.5f));
    CHECK(scene->save(level) == Result::Success);
    // A copy changes its points from those it was given: the level line sinks to y = -4.
    CHECK(scene->seek(1.0f) == Result::Success);
    const Vec3 sunk[] = {{-4.0f, -4.0f, 0.0f}, {-2.0f, -4.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(level).geometry(sunk, 2u), 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, level, 1.5f, -3.0f, -2.0f) && _placed(renderer, scene, level, 2.0f, -3.0f, -4.0f));

    // A warp maps the points the line's clip ended at, and lifts the line from y = 2 to y = 3.
    auto up = Vec3{0.0f, 1.0f, 0.0f};
    Object* lifting[] = {line};
    CHECK(scene->warp(lifting, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, line, 2.0f, -3.0f, 2.0f) && _placed(renderer, scene, line, 2.5f, -3.0f, 2.5f));
    CHECK(_placed(renderer, scene, line, 3.0f, -3.0f, 3.0f));
    // A warped shape keeps the outline its warp gave, so neither it nor a copy of it takes new
    // points.
    CHECK(scene->play(AnimationTarget::from(line).geometry(sunk, 2u), 1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "keeps the outline its warp gave, so the points that define it no longer change"));
    CHECK(scene->update(AnimationTarget::from(line).geometry(sunk, 2u)) == Result::InvalidArguments);
    CHECK(_says(scene, "keeps the outline its warp gave"));
    copy = nullptr;
    CHECK(scene->clone(line, copy) == Result::Success && copy);
    if (copy) {
        CHECK(scene->play(AnimationTarget::from(copy).geometry(sunk, 2u), 1.0f) == Result::InvalidArguments);
        CHECK(_placed(renderer, scene, copy, 3.0f, -3.0f, 3.0f));
    }

    // A warp waits for a geometry clip that runs: the curve flattens again from 3 s to 4 s.
    const Vec3 flat[] = {{1.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}, {3.0f, -2.0f, 0.0f}, {4.0f, -2.0f, 0.0f}};
    CHECK(scene->play(AnimationTarget::from(curve).geometry(flat, 4u), 1.0f, linear) == Result::Success);
    CHECK(scene->seek(3.5f) == Result::Success);
    Object* bending[] = {curve};
    auto clips = scene->clipCount();
    CHECK(scene->warp(bending, 1u, _moved, &up) == Result::InsufficientCondition);
    CHECK(_says(scene, "'curve' already animates its geometry until 4.00 s"));
    CHECK(scene->clipCount() == clips && near(scene->cursor(), 3.5f));
    // A morph from a copy starts from the points the curve shows then, halfway flat again.
    CHECK(scene->add(rule) == Result::Success);
    auto morph = Animation::morph(curve, rule);
    morph.copy = true;
    CHECK(scene->play(morph, 0.5f, linear) == Result::Success);
    auto morphed = scene->object("curve:copy2");
    CHECK(morphed && _placed(renderer, scene, morphed, 3.5f, 2.5f, -1.25f));
    CHECK(near(scene->cursor(), 4.0f));
    CHECK(scene->warp(bending, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, curve, 4.0f, 2.5f, -2.0f) && _placed(renderer, scene, curve, 5.0f, 2.5f, -1.0f));

    // A restore leaves the images of a warp, as for other shapes: the level line, saved at y = 0,
    // stays where the warp takes it.
    Object* sinking[] = {level};
    CHECK(scene->warp(sinking, 1u, _moved, &up, 1.0f, linear) == Result::Success);
    CHECK(scene->restore(level, 1.0f, linear) == Result::Success);
    CHECK(_placed(renderer, scene, level, 6.0f, -3.0f, -3.0f) && _placed(renderer, scene, level, 7.0f, -3.0f, -3.0f));
    delete scene;

    // A view draws the points on their way: a line that rises through y = 0 at 0.5 s, shown twice
    // as close in the lower right quarter, where the view's center is (-2.5, 0).
    auto viewed = Scene::gen(config);
    auto beam = Line::gen({-3.5f, -1.0f, 0.0f}, {-1.5f, -1.0f, 0.0f});
    CHECK(viewed && beam);
    if (!viewed || !beam) {
        delete beam;
        delete viewed;
        return;
    }
    auto white = Color::hex("#ffffff");
    auto blue = Color::hex("#0000ff");
    beam->stroke(white, 4.0f);
    CHECK(viewed->add(beam) == Result::Success);
    SceneView view;
    view.viewport = {0.5f, 0.5f, 0.5f, 0.5f};
    view.camera.eye = {-2.5f, 0.0f, 10.0f};
    view.camera.target = {-2.5f, 0.0f, 0.0f};
    view.camera.orthoHeight = 2.5f;
    view.optical = false;
    view.background = blue;
    CHECK(viewed->view(view) == Result::Success);
    const Vec3 high[] = {{-3.5f, 1.0f, 0.0f}, {-1.5f, 1.0f, 0.0f}};
    CHECK(viewed->play(AnimationTarget::from(beam).geometry(high, 2u), 1.0f, linear) == Result::Success);
    Surface surface;
    // 40 pixels per unit in the view: the beam at y = -1, 0 and 1 crosses rows 190, 150 and 110.
    CHECK(renderer->render(viewed, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 150u, 190u) == pixel(white) && _pixelAt(surface, 150u, 150u) == pixel(blue));
    CHECK(renderer->render(viewed, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 150u, 150u) == pixel(white) && _pixelAt(surface, 150u, 190u) == pixel(blue));
    CHECK(_pixelAt(surface, 115u, 150u) == pixel(white) && _pixelAt(surface, 50u, 100u) == pixel(white));
    CHECK(renderer->render(viewed, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 150u, 110u) == pixel(white) && _pixelAt(surface, 150u, 150u) == pixel(blue));
    delete viewed;
}

// The layer an object of the scene has in a layout report at time, or INT32_MIN without one.
static int32_t _reportedLayer(SwRenderer* renderer, Scene* scene, const Object* object, float time)
{
    LayoutReport report;
    if (renderer->layout(scene, time, report) != Result::Success) return INT32_MIN;
    for (auto i = 0u; i < report.objectCount(); i++) {
        auto item = report.objectAt(i);
        if (item && item->object == object) return item->layer;
    }
    return INT32_MIN;
}

static void drawLayers(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto blue = Color::hex("#0000ff");
    auto green = Color::hex("#00ff00");
    auto white = Color::hex("#ffffff");
    auto none = Color{0, 0, 0, 0};

    // 20 pixels per unit with the origin at pixel (80, 40): low and high overlap at (50, 40),
    // the shape in the group and the cover at (130, 40).
    auto scene = Scene::gen(config);
    auto low = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto high = Rectangle::gen({-1.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto group = Group::gen();
    auto inner = Rectangle::gen({2.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto cover = Rectangle::gen({3.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    Object* objects[] = {low, high, group, inner, cover};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    low->stroke(none).fill(red);
    high->stroke(none).fill(blue);
    inner->stroke(none).fill(green);
    cover->stroke(none).fill(white);
    cover->layer = 1;
    CHECK(group->add(inner) == Result::Success);
    Object* roots[] = {low, high, group, cover};
    for (auto object : roots) CHECK(scene->add(object) == Result::Success);
    Surface surface;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(blue));
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));

    // Before the timeline starts a group's new layer moves the starting layers below it.
    CHECK(scene->update(AnimationTarget::from(group).layer(2)) == Result::Success);
    CHECK(scene->clipCount() == 0u && group->layer == 2 && inner->layer == 2);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(green));

    // A higher layer applies as the animation begins.
    CHECK(scene->play(AnimationTarget::from(low).layer(1), 1.0f) == Result::Success);
    CHECK(low->layer == 1);
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(red));
    // Afterwards a group's layer moves the objects below it while the group holds it.
    CHECK(scene->update(AnimationTarget::from(group).layer(0)) == Result::Success);
    CHECK(group->layer == 0 && inner->layer == 2);
    CHECK(renderer->render(scene, 0.99f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(green));
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));
    // A lower layer applies as the animation ends.
    CHECK(scene->play(AnimationTarget::from(low).layer(-1), 1.0f) == Result::Success);
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(red));
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 50u, 40u) == pixel(blue));
    CHECK(low->layer == -1);
    // The layout report gives the layer each object draws at then.
    CHECK(_reportedLayer(renderer, scene, low, 0.5f) == 1);
    CHECK(_reportedLayer(renderer, scene, low, 2.0f) == -1);
    CHECK(_reportedLayer(renderer, scene, inner, 0.5f) == 2);
    CHECK(_reportedLayer(renderer, scene, inner, 1.5f) == 0);
    // An isolated group draws as one layer at the layer it has then.
    CHECK(group->isolate() == Result::Success);
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(green));
    CHECK(renderer->render(scene, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));

    // A layer is a channel of its own: a move may run beside it, a second change waits.
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->update(AnimationTarget::from(low).layer(3)) == Result::InsufficientCondition);
    CHECK(_says(scene, "already animates its layer until 2.00 s"));
    CHECK(scene->play(AnimationTarget::from(low).shift({0.0f, 0.5f, 0.0f}), 0.5f) == Result::Success);
    delete scene;

    // The layers below a group must stay 32-bit integers.
    auto other = Scene::gen(config);
    auto outer = Group::gen();
    auto deep = Rectangle::gen({}, {1.0f, 1.0f});
    CHECK(other && outer && deep);
    if (!other || !outer || !deep) {
        delete other;
        delete outer;
        delete deep;
        return;
    }
    deep->layer = INT32_MAX;
    CHECK(outer->add(deep) == Result::Success);
    CHECK(other->add(outer) == Result::Success);
    CHECK(other->update(AnimationTarget::from(outer).layer(1)) == Result::InvalidArguments);
    CHECK(_says(other, "32-bit integer"));
    CHECK(outer->layer == 0 && deep->layer == INT32_MAX);
    // And each must still be in its starting state: a direct edit needs update() first.
    deep->layer = 0;
    CHECK(other->update(AnimationTarget::from(outer).layer(1)) == Result::InvalidArguments);
    CHECK(_says(other, "below the target was edited directly"));
    CHECK(other->update(deep) == Result::Success);
    CHECK(other->update(AnimationTarget::from(outer).layer(1)) == Result::Success);
    CHECK(outer->layer == 1 && deep->layer == 1);
    delete other;
}

// The center of an object's bounds in pixels at time, or (-1, -1) without them.
static Vec2 _centerAt(SwRenderer* renderer, const Scene* scene, const Object* object, float time)
{
    BBox bounds;
    if (renderer->bounds(scene, object, time, bounds) != Result::Success) return {-1.0f, -1.0f};
    return bounds.center();
}

static void saveRestore(SwRenderer* renderer)
{
    Config config;
    config.width = 160;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.antialiasing = false;
    config.background = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto green = Color::hex("#00ff00");
    auto blue = Color::hex("#0000ff");
    auto white = Color::hex("#ffffff");
    auto none = Color{0, 0, 0, 0};
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);

    // 20 pixels per unit with the origin at pixel (80, 40): the card at (40, 40), the dot at
    // (120, 40).
    auto scene = Scene::gen(config);
    auto card = Rectangle::gen({-2.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    auto dot = Circle::gen({2.0f, 0.0f, 0.0f}, 0.5f);
    auto title = Text::gen("cat", {0.0f, 1.5f, 0.0f});
    auto total = Text::gen("");
    Object* objects[] = {card, dot, title, total};
    auto created = scene != nullptr;
    for (auto object : objects) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : objects) delete object;
        delete scene;
        return;
    }
    card->stroke(none).fill(red).tag("card");
    dot->stroke(none).fill(blue);
    CHECK(total->count(5.0f) == Result::Success);
    for (auto object : objects) CHECK(scene->add(object) == Result::Success);

    // A save records the state at the cursor and adds nothing to the timeline. Before the
    // timeline it keeps its own copy of a string, which a text may still replace.
    CHECK(scene->save(objects, 4u) == Result::Success);
    CHECK(scene->clipCount() == 0u && near(scene->cursor(), 0.0f));
    CHECK(scene->update(AnimationTarget::from(title).text("dog")) == Result::Success);
    CHECK(scene->clipCount() == 0u && std::strcmp(title->text(), "dog") == 0);

    // A move, a recolor, a shape tween, text and number changes, then a fade that shifts and
    // shrinks the dot.
    AnimationTarget changes[] = {
        AnimationTarget::from(card).shift({1.0f, 1.0f, 0.0f}).fill(green).size(Vec2{2.0f, 1.0f}).corner(0.25f),
        AnimationTarget::from(title).text("horse"),
        AnimationTarget::from(total).value(100.0f),
    };
    CHECK(scene->play(changes, 3u, 1.0f, linear) == Result::Success);
    CHECK(scene->play(Animation::fadeOut(dot, {0.0f, 1.0f, 0.0f}, 0.5f), 1.0f) == Result::Success);
    Surface surface;
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 120u, 40u) == pixel(config.background));

    // Restoring brings every value back over the duration.
    CHECK(scene->restore(objects, 4u, 1.0f, linear) == Result::Success);
    CHECK(near(scene->cursor(), 3.0f));
    auto center = _centerAt(renderer, scene, card, 2.5f);
    CHECK(near(center.x, 50.0f, 1.0f) && near(center.y, 30.0f, 1.0f));
    CHECK(renderer->render(scene, 3.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 52u, 40u) == pixel(config.background));
    CHECK(_pixelAt(surface, 120u, 40u) == pixel(blue));
    center = _centerAt(renderer, scene, dot, 3.0f);
    CHECK(near(center.x, 120.0f, 1.0f) && near(center.y, 40.0f, 1.0f));
    CHECK(std::strcmp(_shown(scene, title, 2.0f), "horse") == 0);
    CHECK(std::strcmp(_shown(scene, title, 3.0f), "cat") == 0);
    CHECK(std::strcmp(_shown(scene, total, 2.0f), "100") == 0);
    CHECK(std::strcmp(_shown(scene, total, 3.0f), "5") == 0);
    // The saved values are the objects' latest state again.
    CHECK(near(card->size.x, 1.0f) && near(card->size.y, 1.0f) && near(card->corner, 0.0f));
    CHECK(same(card->style.fill, red) && near(card->model.e[3], 0.0f) && near(card->model.e[7], 0.0f));
    CHECK(near(dot->opacity, 1.0f) && std::strcmp(title->text(), "cat") == 0 && near(total->value(), 5.0f));

    // A restore takes the save, so a second one has nothing to bring back.
    CHECK(scene->restore(card) == Result::InsufficientCondition);
    CHECK(_says(scene, "'card' has no saved state; save it before restoring"));
    CHECK(scene->restore(dot, 1.0f) == Result::InsufficientCondition);
    CHECK(_says(scene, "the object has no saved state"));

    // Saves nest, and each restore takes the newest state of its target.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(card).fill(blue), 1.0f) == Result::Success);
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(card).shift({0.0f, 1.0f, 0.0f}).opacity(0.5f), 1.0f) == Result::Success);
    auto clips = scene->clipCount();
    // Without a duration the values switch at the cursor.
    CHECK(scene->restore(card) == Result::Success);
    CHECK(near(scene->cursor(), 5.0f) && scene->clipCount() == clips + 1u);
    center = _centerAt(renderer, scene, card, 4.99f);
    CHECK(near(center.y, 20.0f, 1.0f));
    CHECK(renderer->render(scene, 5.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(blue));
    CHECK(scene->restore(card, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 6.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 40u, 40u) == pixel(red));
    CHECK(scene->restore(card) == Result::InsufficientCondition);
    // When nothing differs, a restore waits for its duration.
    CHECK(scene->save(card) == Result::Success);
    clips = scene->clipCount();
    CHECK(scene->restore(card, 0.5f) == Result::Success);
    CHECK(scene->clipCount() == clips && near(scene->cursor(), 6.5f));

    // A change of place alone adds to a motion that still runs, as a shift does.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(card).shift({2.0f, 0.0f, 0.0f}), 2.0f, linear) == Result::Success);
    CHECK(scene->seek(7.5f) == Result::Success);
    CHECK(scene->restore(card, 1.0f, linear) == Result::Success);
    CHECK(near(_centerAt(renderer, scene, card, 7.5f).x, 60.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, card, 8.0f).x, 50.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, card, 8.5f).x, 40.0f, 1.0f));
    CHECK(near(card->model.e[3], 0.0f));
    // A turn must end first, and a failed restore keeps the save.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->play(Animation::rotate(card, 1.0f), 1.0f) == Result::Success);
    CHECK(scene->seek(9.0f) == Result::Success);
    CHECK(scene->restore(card, 0.25f) == Result::InsufficientCondition);
    CHECK(_says(scene, "'card' already animates its transform until 9.50 s; begin at that time or later"));
    CHECK(scene->seek(9.5f) == Result::Success);
    CHECK(scene->restore(card, 0.5f) == Result::Success);
    CHECK(near(card->model.e[0], 1.0f) && near(card->model.e[1], 0.0f));

    // Later targets begin later by the lag, as in play().
    Object* pair[] = {card, dot};
    CHECK(scene->save(pair, 2u) == Result::Success);
    AnimationTarget away[] = {AnimationTarget::from(card).shift({0.0f, 1.0f, 0.0f}),
                              AnimationTarget::from(dot).shift({0.0f, 1.0f, 0.0f})};
    CHECK(scene->play(away, 2u, 1.0f) == Result::Success);
    CHECK(scene->restore(pair, 2u, 1.0f, linear, 0.5f) == Result::Success);
    CHECK(near(scene->cursor(), 12.5f));
    CHECK(near(_centerAt(renderer, scene, card, 11.5f).y, 30.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, dot, 11.5f).y, 20.0f, 1.0f));
    CHECK(near(_centerAt(renderer, scene, dot, 12.5f).y, 40.0f, 1.0f));

    // Errors name the problem and change nothing.
    CHECK(scene->save(card) == Result::Success);
    CHECK(scene->restore(card, -1.0f) == Result::InvalidArguments);
    CHECK(_says(scene, "zero or a positive number of seconds"));
    CHECK(scene->restore(nullptr) == Result::InvalidArguments);
    CHECK(_says(scene, "not part of this scene"));
    CHECK(scene->restore(objects, 0u) == Result::InvalidArguments);
    CHECK(_says(scene, "nothing to restore"));
    Object* twice[] = {card, card};
    CHECK(scene->save(twice, 2u) == Result::InvalidArguments);
    CHECK(_says(scene, "only once in one call"));
    CHECK(scene->restore(twice, 2u) == Result::InvalidArguments);
    CHECK(scene->save(nullptr) == Result::InvalidArguments);
    CHECK(scene->remove(dot) == Result::Success);
    CHECK(scene->save(dot) == Result::InvalidArguments);
    CHECK(_says(scene, "removed earlier"));
    CHECK(scene->restore(card) == Result::Success);

    // A string saved on the timeline is one the text keeps, so a restore makes no copy of it,
    // even after more changes than a text looks back through for a string that reads the same.
    CHECK(scene->save(title) == Result::Success);
    char word[8];
    for (auto i = 0u; i < 17u; i++) {
        std::snprintf(word, sizeof(word), "w%u", i);
        CHECK(scene->play(AnimationTarget::from(title).text(word), 0.1f) == Result::Success);
    }
    auto strings = title->texts(nullptr, 0u);
    CHECK(scene->restore(title) == Result::Success);
    CHECK(title->texts(nullptr, 0u) == strings && std::strcmp(title->text(), "cat") == 0);
    CHECK(std::strcmp(_shown(scene, title, scene->cursor()), "cat") == 0);
    delete scene;

    // A group comes back with the objects below it, each in the layer that keeps the order it
    // had: inner draws over under (layer -1) and below cover (layer 1). Pixel (110, 40) shows
    // inner or under, pixel (130, 40) cover or inner.
    auto other = Scene::gen(config);
    auto group = Group::gen();
    auto inner = Rectangle::gen({2.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    auto gone = Circle::gen({-2.0f, 0.0f, 0.0f}, 0.5f);
    auto late = Circle::gen({-2.0f, 1.0f, 0.0f}, 0.25f);
    auto under = Rectangle::gen({1.5f, 0.0f, 0.0f}, {1.0f, 2.0f});
    auto cover = Rectangle::gen({3.0f, 0.0f, 0.0f}, {2.0f, 2.0f});
    Object* parts[] = {group, inner, gone, late, under, cover};
    created = other != nullptr;
    for (auto object : parts) created = created && object;
    CHECK(created);
    if (!created) {
        for (auto object : parts) delete object;
        delete other;
        return;
    }
    inner->stroke(none).fill(green);
    under->stroke(none).fill(red);
    under->layer = -1;
    cover->stroke(none).fill(white);
    cover->layer = 1;
    CHECK(group->add(inner) == Result::Success && group->add(gone) == Result::Success);
    CHECK(other->add(group) == Result::Success && other->add(under) == Result::Success
          && other->add(cover) == Result::Success);
    CHECK(other->save(group) == Result::Success);
    // Objects added below the group since the save stay as they are.
    CHECK(group->add(late) == Result::Success);
    // Before the timeline the group's new layer moves the starting layers below it.
    CHECK(other->update(AnimationTarget::from(group).layer(2)) == Result::Success);
    CHECK(group->layer == 2 && inner->layer == 2);
    AnimationTarget raised[] = {AnimationTarget::from(inner).fill(blue).layer(4),
                                AnimationTarget::from(late).fill(blue)};
    CHECK(other->play(raised, 2u, 1.0f) == Result::Success);
    CHECK(renderer->render(other, 0.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) != pixel(white));
    // Objects removed since the save stay removed.
    CHECK(other->remove(gone) == Result::Success);
    CHECK(other->restore(group, 1.0f) == Result::Success);
    // The higher layer holds until the restore ends, then the saved order is back.
    CHECK(_reportedLayer(renderer, other, inner, 1.5f) == 4);
    CHECK(_reportedLayer(renderer, other, inner, 2.0f) == 0);
    CHECK(renderer->render(other, 1.5f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 130u, 40u) != pixel(white));
    CHECK(renderer->render(other, 2.0f, surface) == Result::Success);
    CHECK(_pixelAt(surface, 110u, 40u) == pixel(green));
    CHECK(_pixelAt(surface, 130u, 40u) == pixel(white));
    CHECK(group->layer == 0 && inner->layer == 2 && same(inner->style.fill, green));
    CHECK(same(late->style.fill, blue));

    // An object below the target edited directly must be updated first.
    CHECK(other->save(group) == Result::Success);
    inner->opacity = 0.25f;
    CHECK(other->restore(group) == Result::InvalidArguments);
    CHECK(_says(other, "below the target was edited directly"));
    CHECK(other->save(group) == Result::InvalidArguments);
    inner->opacity = 1.0f;
    CHECK(other->restore(group) == Result::Success);
    delete other;
}

// The pixels of color c inside the rectangle from (x0, y0) to (x1, y1), excluded: how many, and
// their mean column and row.
static uint32_t _viewPixels(const Surface& surface, Color c, uint32_t x0, uint32_t y0, uint32_t x1,
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

// A view draws the scene again through its own camera into its rectangle: magnified where its
// camera looks, with optical pixel sizes, clipped at its edges, between the layers it divides,
// and as it moves, fades and follows.
static void viewRendering(SwRenderer* renderer)
{
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto black = Color::hex("#000000");
    auto red = Color::hex("#ff0000");
    auto yellow = Color::hex("#ffff00");
    auto green = Color::hex("#00ff00");
    auto blue = Color::hex("#0000ff");
    auto white = Color::hex("#ffffff");
    auto magenta = Color::hex("#ff00ff");
    auto none = Color{0, 0, 0, 0};
    // 10 pixels per unit with the origin at pixel (100, 50); the view fills the bottom right
    // quarter, (100, 50) to (200, 100), and looks at (2, 1) 1.25 units high: 40 pixels per unit,
    // four times the scene's camera, with (2, 1) at pixel (150, 75).
    auto make = [&](bool optical, Object** dot = nullptr) -> Scene* {
        Config config;
        config.width = 200;
        config.height = 100;
        config.camera.orthoHeight = 10.0f;
        config.antialiasing = false;
        config.background = black;
        auto scene = Scene::gen(config);
        if (!scene) return nullptr;
        auto point = Point::gen({2.0f, 1.0f, 0.0f});
        auto rail = Line::gen({3.0f, -5.0f, 0.0f}, {3.0f, 5.0f, 0.0f});
        auto beam = Line::gen({-10.0f, 1.5f, 0.0f}, {10.0f, 1.5f, 0.0f});
        auto under = Rectangle::gen({5.0f, -3.0f, 0.0f}, {1.0f, 1.0f});
        auto ghost = Circle::gen({1.75f, 0.75f, 0.0f}, 0.2f);
        auto cover = Rectangle::gen({9.0f, -4.5f, 0.0f}, {2.0f, 1.0f});
        auto badge = Rectangle::gen({2.25f, 1.0f, 0.0f}, {0.1f, 0.1f});
        Object* objects[] = {point, rail, beam, under, ghost, cover, badge};
        auto created = true;
        for (auto object : objects) created = created && object;
        if (!created) {
            for (auto object : objects) delete object;
            delete scene;
            return nullptr;
        }
        point->stroke(red, 2.0f).fill(red);
        point->style.radius = 2.0f;
        rail->stroke(yellow, 2.0f);
        beam->stroke(green, 2.0f);
        under->stroke(none).fill(white);
        ghost->stroke(magenta).fill(magenta);
        cover->stroke(none).fill(white);
        cover->layer = 1;
        badge->stroke(none).fill(white);
        badge->layer = 1;
        for (auto object : objects) CHECK(scene->add(object) == Result::Success);
        SceneView view;
        view.viewport = {0.5f, 0.5f, 0.5f, 0.5f};
        view.camera.eye = {2.0f, 1.0f, 10.0f};
        view.camera.target = {2.0f, 1.0f, 0.0f};
        view.camera.orthoHeight = 1.25f;
        view.layer = 0;
        view.optical = optical;
        view.background = blue;
        const Object* hidden[] = {ghost};
        view.hidden = hidden;
        view.hiddenCount = 1u;
        CHECK(scene->view(view) == Result::Success);
        if (dot) *dot = point;
        return scene;
    };

    Object* dot = nullptr;
    auto scene = make(true, &dot);
    CHECK(scene != nullptr);
    if (!scene) return;
    Surface surface;
    float column;
    float row;
    CHECK(renderer->render(scene, 0.0f, surface) == Result::Success);
    // The scene itself, outside the view and below it.
    CHECK(_pixelAt(surface, 120u, 40u) == pixel(red));
    CHECK(_pixelAt(surface, 130u, 20u) == pixel(yellow));
    CHECK(_pixelAt(surface, 117u, 42u) == pixel(magenta));
    CHECK(_pixelAt(surface, 50u, 75u) == pixel(black));
    // The dot, 3 pixels in radius with its stroke, sits at the view's center 12 in radius.
    auto dotPixels = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
    CHECK(dotPixels > 420u && dotPixels < 540u);
    CHECK(std::fabs(column - 149.5f) < 1.0f && std::fabs(row - 74.5f) < 1.0f);
    CHECK(_pixelAt(surface, 165u, 75u) == pixel(blue));
    // The rail at x = 3 runs down the view at column 190, 8 pixels thick instead of 2.
    auto railPixels = _viewPixels(surface, yellow, 150u, 60u, 200u, 61u, column, row);
    CHECK(railPixels >= 7u && railPixels <= 9u && std::fabs(column - 189.5f) < 1.0f);
    // The beam at y = 1.5 crosses the whole view along row 55, 8 pixels thick, and stops at its
    // left edge.
    CHECK(_pixelAt(surface, 101u, 55u) == pixel(green));
    CHECK(_pixelAt(surface, 198u, 55u) == pixel(green));
    CHECK(_pixelAt(surface, 98u, 55u) == pixel(black));
    CHECK(_viewPixels(surface, green, 101u, 50u, 102u, 100u, column, row) == 8u && near(row, 54.5f));
    // The cover on layer 1 draws over the view, the badge on layer 1 stays out of it, the hidden
    // circle is left out and the view's background hides the white square on layer 0 below it.
    CHECK(_pixelAt(surface, 190u, 95u) == pixel(white));
    CHECK(_pixelAt(surface, 140u, 85u) == pixel(blue));
    CHECK(_pixelAt(surface, 150u, 88u) == pixel(blue));
    CHECK(_viewPixels(surface, white, 100u, 50u, 180u, 90u, column, row) == 0u);
    CHECK(_viewPixels(surface, magenta, 100u, 50u, 200u, 100u, column, row) == 0u);

    // The layout report lists the view as a scene of its own that covers what lies below it.
    LayoutReport report;
    CHECK(renderer->layout(scene, 0.0f, report) == Result::Success);
    CHECK(report.sceneCount() == 2u);
    auto shown = report.sceneAt(1u);
    CHECK(shown && shown->kind == LayoutSceneKind::View && shown->scene == scene);
    CHECK(shown && shown->path && std::strcmp(shown->path, "root/view:0") == 0);
    CHECK(shown && near(shown->bounds.x, 100.0f) && near(shown->bounds.width, 100.0f) && near(shown->bounds.height, 50.0f));
    CHECK(shown && near(shown->scaleX, 4.0f, 1.0e-3f) && near(shown->scaleY, 4.0f, 1.0e-3f) && shown->parent == 0u);
    CHECK(report.objectCount() == scene->count());
    for (auto i = 0u; i < report.visualCount(); i++) {
        auto visual = report.visualAt(i);
        auto object = report.objectAt(visual->object)->object;
        if (object->type() != Type::Rectangle) continue;
        auto& size = static_cast<const Rectangle*>(object)->size;
        // The square below the view is hidden by it; the cover above it is not.
        if (size.x == 1.0f) CHECK(visual->occluded && visual->occluder == 1u);
        if (size.x == 2.0f) CHECK(!visual->occluded);
    }

    // A view that is not optical keeps the pixel sizes.
    auto plain = make(false);
    CHECK(plain != nullptr);
    if (plain) {
        CHECK(renderer->render(plain, 0.0f, surface) == Result::Success);
        CHECK(_viewPixels(surface, yellow, 150u, 60u, 200u, 61u, column, row) == 2u);
        dotPixels = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
        CHECK(dotPixels > 24u && dotPixels < 48u && std::fabs(column - 149.5f) < 1.0f);
        CHECK(_viewPixels(surface, green, 101u, 50u, 102u, 100u, column, row) == 2u);
        CHECK(renderer->layout(plain, 0.0f, report) == Result::Success);
        CHECK(report.sceneCount() == 2u && near(report.sceneAt(1u)->scaleX, 1.0f));
    }
    delete plain;

    // The view's camera pans to (1.5, 1) and widens to 2.5 units over a second: the dot ends
    // half a unit right of the center, 20 pixels per unit, twice its size in the scene.
    auto wide = Camera{};
    wide.eye = {1.5f, 1.0f, 10.0f};
    wide.target = {1.5f, 1.0f, 0.0f};
    wide.orthoHeight = 2.5f;
    CHECK(scene->look(0u, wide, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(scene->fade(0u, 0.5f, 1.0f, linear) == Result::Success);
    CHECK(renderer->render(scene, 0.5f, surface) == Result::Success);
    auto halfway = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
    CHECK(halfway > 160u && halfway < 420u && column > 150.5f && column < 159.5f);
    CHECK(renderer->render(scene, 1.0f, surface) == Result::Success);
    dotPixels = _viewPixels(surface, red, 100u, 50u, 200u, 100u, column, row);
    CHECK(std::fabs(column - 159.5f) < 1.0f && std::fabs(row - 74.5f) < 1.0f);
    CHECK(dotPixels > 100u && dotPixels < 160u);
    CHECK(_viewPixels(surface, yellow, 150u, 60u, 200u, 61u, column, row) == 4u);
    CHECK(std::fabs(column - 179.5f) < 1.0f);
    // Then it fades to half: its background lies half over the black scene.
    CHECK(renderer->render(scene, 2.0f, surface) == Result::Success);
    auto mixed = _pixelAt(surface, 110u, 95u);
    CHECK((mixed & 0xffffu) == 0u && std::abs(static_cast<int>((mixed >> 16) & 0xffu) - 128) <= 2);
    CHECK(_pixelAt(surface, 190u, 95u) == pixel(white));

    // Seeking a retained renderer back and forth draws what a fresh one draws.
    float times[] = {2.0f, 0.0f, 0.5f, 1.5f, 1.0f, 0.0f};
    uint64_t expected[6] = {};
    for (auto i = 0u; i < 6u; i++) {
        auto fresh = SwRenderer::gen();
        CHECK(fresh != nullptr);
        if (fresh && fresh->render(scene, times[i], surface) == Result::Success) expected[i] = checksum(surface);
        delete fresh;
    }
    for (auto i = 0u; i < 6u; i++) {
        CHECK(renderer->render(scene, times[i], surface) == Result::Success);
        CHECK(checksum(surface) == expected[i]);
    }
    delete scene;

    // A view made at 0.5 s that follows a moving dot keeps it at its center.
    Config config;
    config.width = 200;
    config.height = 100;
    config.camera.orthoHeight = 10.0f;
    config.antialiasing = false;
    config.background = black;
    auto followed = Scene::gen(config);
    auto mover = Point::gen({-4.0f, 0.0f, 0.0f});
    CHECK(followed && mover);
    if (followed && mover) {
        mover->stroke(red, 2.0f).fill(red);
        mover->style.radius = 2.0f;
        CHECK(followed->add(mover) == Result::Success);
        CHECK(followed->play(AnimationTarget::from(mover).shift({8.0f, 0.0f, 0.0f}), 2.0f, linear) == Result::Success);
        CHECK(followed->seek(0.5f) == Result::Success);
        SceneView inset;
        inset.viewport = {0.0f, 0.0f, 0.25f, 0.5f};
        inset.camera.orthoHeight = 2.5f;
        inset.background = blue;
        inset.follow = mover;
        CHECK(followed->view(inset) == Result::Success);
        CHECK(followed->seek(2.0f) == Result::Success);
        CHECK(followed->remove(mover) == Result::Success);
        CHECK(renderer->render(followed, 0.25f, surface) == Result::Success);
        CHECK(_viewPixels(surface, blue, 0u, 0u, 200u, 100u, column, row) == 0u);
        float moments[] = {0.5f, 1.0f, 1.75f};
        for (auto time : moments) {
            CHECK(renderer->render(followed, time, surface) == Result::Success);
            dotPixels = _viewPixels(surface, red, 0u, 0u, 50u, 50u, column, row);
            CHECK(dotPixels > 100u && dotPixels < 160u);
            CHECK(std::fabs(column - 24.5f) < 1.0f && std::fabs(row - 24.5f) < 1.0f);
        }
        // Once the dot is gone the view holds the camera it was given.
        CHECK(renderer->render(followed, 2.5f, surface) == Result::Success);
        CHECK(_pixelAt(surface, 25u, 25u) == pixel(blue));
    } else {
        delete mover;
    }
    delete followed;

    // A perspective view orbits on its own: a quarter turn about +y puts its eye on +x, in line
    // with a dot at (2, 0, 0), which then sits at the center of the view on the left half while
    // the scene's camera still shows it right of the frame's center.
    config.cameraView = CameraView::ThreeD;
    config.camera.projection = Projection::Perspective;
    config.camera.eye = {0.0f, 0.0f, 10.0f};
    auto spatial = Scene::gen(config);
    auto ball = Point::gen({2.0f, 0.0f, 0.0f});
    CHECK(spatial && ball);
    if (spatial && ball) {
        ball->stroke(red).fill(red);
        ball->style.radius = 3.0f;
        CHECK(spatial->add(ball) == Result::Success);
        SceneView orbiting;
        orbiting.viewport = {0.0f, 0.0f, 0.5f, 1.0f};
        orbiting.camera = config.camera;
        orbiting.cameraView = CameraView::ThreeD;
        orbiting.background = blue;
        uint32_t index = 0u;
        CHECK(spatial->view(orbiting, &index) == Result::Success);
        CHECK(spatial->orbit(index, 1.5707964f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::Success);
        CHECK(renderer->render(spatial, 0.0f, surface) == Result::Success);
        _viewPixels(surface, red, 0u, 0u, 100u, 100u, column, row);
        CHECK(column > 65.0f);
        CHECK(renderer->render(spatial, 1.0f, surface) == Result::Success);
        _viewPixels(surface, red, 0u, 0u, 100u, 100u, column, row);
        CHECK(std::fabs(column - 49.5f) < 1.0f);
        _viewPixels(surface, red, 100u, 0u, 200u, 100u, column, row);
        CHECK(column > 115.0f);
    } else {
        delete ball;
    }
    delete spatial;
}

// Views of Scene::view(): each keeps its own camera timeline and fades, made and moved with the
// checks of the scene's camera, and none of it touches the scene's own camera.
static void viewTimelines()
{
    auto linear = AnimCurve::preset(AnimCurvePreset::Linear);
    auto scene = Scene::gen();
    auto dot = Point::gen({1.0f, 0.5f, 0.0f});
    auto overlay = Group::gen();
    auto foreign = Point::gen({});
    CHECK(scene && dot && overlay && foreign);
    if (!scene || !dot || !overlay || !foreign) {
        delete scene;
        delete dot;
        delete overlay;
        delete foreign;
        return;
    }
    CHECK(scene->add(dot) == Result::Success);
    CHECK(scene->add(overlay) == Result::Success);
    auto opening = scene->config().camera;

    SceneView view;
    view.viewport = {0.5f, 0.0f, 0.5f, 0.5f};
    view.camera = opening;
    view.camera.orthoHeight = 2.0f;
    view.follow = dot;
    view.opacity = 0.0f;
    {
        const Object* hidden[] = {overlay};
        view.hidden = hidden;
        view.hiddenCount = 1u;
        uint32_t index = 99u;
        CHECK(scene->view(view, &index) == Result::Success);
        CHECK(index == 0u);
    }
    CHECK(scene->viewCount() == 1u);
    SceneView made;
    CHECK(scene->viewAt(0u, made));
    CHECK(!scene->viewAt(1u, made));
    // The view keeps its own copy of the hidden list.
    CHECK(made.hiddenCount == 1u && made.hidden && made.hidden[0] == overlay);
    CHECK(made.viewport.x == 0.5f && made.viewport.height == 0.5f);
    CHECK(made.layer == INT32_MAX && made.optical && made.follow == dot);
    CHECK(made.opacity == 0.0f && made.camera.orthoHeight == 2.0f);
    // Making a view is no timeline call: the scene may still change its objects' starting states.
    CHECK(scene->update(dot) == Result::Success);
    CHECK(near(scene->cursor(), 0.0f));

    // Rejected views say why and leave the scene as it was.
    auto rejected = view;
    rejected.hidden = nullptr;
    rejected.hiddenCount = 0u;
    rejected.viewport = {0.75f, 0.0f, 0.5f, 0.5f};
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    CHECK(scene->diagnostic() != nullptr);
    rejected.viewport = {0.0f, 0.0f, 0.0f, 0.5f};
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.viewport = view.viewport;
    rejected.camera.projection = Projection::Perspective;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.camera = view.camera;
    rejected.opacity = 1.5f;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.opacity = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    rejected.opacity = 1.0f;
    rejected.follow = foreign;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "part of this scene"));
    rejected.follow = nullptr;
    rejected.hiddenCount = 1u;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    {
        const Object* hidden[] = {foreign};
        rejected.hidden = hidden;
        CHECK(scene->view(rejected) == Result::InvalidArguments);
    }
    rejected.hidden = nullptr;
    rejected.hiddenCount = 257u;
    CHECK(scene->view(rejected) == Result::InvalidArguments);
    CHECK(scene->viewCount() == 1u);

    // A look of the view moves its camera alone and advances the cursor like any camera move.
    auto closer = view.camera;
    closer.orthoHeight = 1.0f;
    CHECK(scene->look(0u, closer, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(near(scene->cursor(), 1.0f));
    CHECK(scene->viewAt(0u, made) && made.camera.orthoHeight == 1.0f);
    CHECK(scene->config().camera.orthoHeight == opening.orthoHeight);
    CHECK(scene->update(dot) == Result::InsufficientCondition);
    // A second move of the same view may begin while the first runs; the scene's own camera
    // starts its timeline afresh.
    CHECK(scene->seek(0.5f) == Result::Success);
    auto panned = closer;
    panned.target.x = panned.eye.x = 2.0f;
    CHECK(scene->look(0u, panned, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(scene->seek(0.5f) == Result::Success);
    auto wide = opening;
    wide.orthoHeight = 16.0f;
    CHECK(scene->look(wide, CameraView::TwoD, 1.0f, linear) == Result::Success);
    CHECK(scene->config().camera.orthoHeight == 16.0f);
    CHECK(scene->viewAt(0u, made) && made.camera.target.x == 2.0f && made.camera.orthoHeight == 1.0f);
    CHECK(near(scene->duration(), 1.5f));

    // A 2D view does not orbit, and the camera checks of the scene apply to views.
    CHECK(scene->orbit(0u, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() != nullptr);
    auto tilted = closer;
    tilted.eye.x += 1.0f;
    CHECK(scene->look(0u, tilted, CameraView::TwoD, 1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->look(0u, closer, CameraView::TwoD, 0.0f, linear) == Result::InvalidArguments);
    CHECK(scene->look(1u, closer, CameraView::TwoD, 1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "no view"));
    CHECK(scene->orbit(3u, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::InvalidArguments);

    // Fades of a view follow one another.
    CHECK(scene->seek(1.5f) == Result::Success);
    CHECK(scene->fade(0u, 1.0f, 0.5f, linear) == Result::Success);
    CHECK(scene->viewAt(0u, made) && made.opacity == 1.0f);
    CHECK(scene->seek(1.75f) == Result::Success);
    CHECK(scene->fade(0u, 0.0f, 0.5f, linear) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "fades until 2.00"));
    CHECK(scene->seek(2.0f) == Result::Success);
    CHECK(scene->fade(0u, 1.5f, 0.5f, linear) == Result::InvalidArguments);
    CHECK(scene->fade(0u, 0.5f, -1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->fade(2u, 0.5f, 1.0f, linear) == Result::InvalidArguments);
    CHECK(scene->fade(0u, 0.5f, 1.0f, linear) == Result::Success);
    CHECK(near(scene->duration(), 3.0f));

    // A view shows from the cursor at which it is made; it cannot move or fade before that.
    view.hidden = nullptr;
    view.hiddenCount = 0u;
    view.follow = nullptr;
    view.cameraView = CameraView::ThreeD;
    view.camera.projection = Projection::Perspective;
    view.camera.eye = {0.0f, 0.0f, 10.0f};
    uint32_t late = 0u;
    CHECK(scene->view(view, &late) == Result::Success);
    CHECK(late == 1u);
    CHECK(scene->seek(1.0f) == Result::Success);
    CHECK(scene->fade(late, 1.0f, 0.5f, linear) == Result::InsufficientCondition);
    CHECK(scene->orbit(late, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "made after"));
    CHECK(scene->seek(3.0f) == Result::Success);
    CHECK(scene->orbit(late, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, linear) == Result::Success);
    CHECK(scene->viewAt(late, made) && made.cameraView == CameraView::ThreeD && made.camera.eye.x > 0.0f);
    CHECK(near(scene->duration(), 4.0f));

    for (auto i = scene->viewCount(); i < 64u; i++) CHECK(scene->view(view) == Result::Success);
    CHECK(scene->view(view) == Result::InsufficientCondition);
    CHECK(scene->viewCount() == 64u);

    // An interactive scene camera leaves the views to their own moves.
    Config interactive;
    interactive.cameraMode = CameraMode::Interactive;
    auto steered = Scene::gen(interactive);
    CHECK(steered != nullptr);
    if (steered) {
        SceneView inset;
        CHECK(steered->view(inset) == Result::Success);
        auto zoomed = inset.camera;
        zoomed.orthoHeight = 2.0f;
        CHECK(steered->look(zoomed, CameraView::TwoD, 1.0f, linear) == Result::InsufficientCondition);
        CHECK(steered->look(0u, zoomed, CameraView::TwoD, 1.0f, linear) == Result::Success);
    }
    delete steered;

    // A scene shown in another one can no longer gain views.
    auto parent = Scene::gen();
    CHECK(parent != nullptr);
    if (parent) {
        CHECK(parent->viewport(scene, {0.0f, 0.0f, 0.5f, 0.5f}) == Result::Success);
        CHECK(scene->view(SceneView{}) == Result::InsufficientCondition);
        CHECK(scene->look(0u, closer, CameraView::TwoD, 1.0f, linear) == Result::InsufficientCondition);
        CHECK(scene->fade(0u, 1.0f, 1.0f, linear) == Result::InsufficientCondition);
        delete parent;
    } else {
        delete scene;
    }
    delete foreign;
}

int main()
{
    springResponses();
    auto renderer = SwRenderer::gen();
    CHECK(renderer != nullptr);
    if (renderer) {
        pngCompression();
        scene2D(renderer);
        coplanarDrawOrder(renderer);
        rendererPixelSampling(renderer);
        spaceNumbers(renderer);
        renderBounds(renderer);
        layoutOcclusion(renderer);
        dashedStrokes(renderer);
        textAnimationRegression(renderer);
        repeatedCreateRegression(renderer);
        drawDirectionAnimations(renderer);
        arrowCompositing(renderer);
        directedRouteRendering(renderer);
        fadeAnimations(renderer);
        scaleAndIndicateAnimations(renderer);
        emphasisEffects(renderer);
        morphAnimations(renderer);
        warpedGeometry(renderer);
        spaceTicks(renderer);
        spaceLabels();
        spaceScreenLabels(renderer);
        spaceScreenLabelWarps(renderer);
        spaceLabelOpacity(renderer);
        cursorDecimals(renderer);
        scene3D(renderer);
        assetsAndCells(renderer);
        voxelSamples(renderer);
        cellSamples();
        riemannSums();
        vectorFields();
        streamLines();
        sampleTime(renderer);
        dimensionTransition(renderer);
        viewports(renderer);
        liveSceneTransitions(renderer);
        sceneTransitions(renderer);
        transitionLayoutVisuals(renderer);
        transitionStyles(renderer);
        objectTree(renderer);
        objectIds();
        boundsAndLayout();
        offsetBoxes(renderer);
        sceneUpdate(renderer);
        groupAndRectangleRendering(renderer);
        connectorAnimation(renderer);
        connectorOutlines(renderer);
        animationTargets(renderer);
        timelineTracks(renderer);
        clipLists(renderer);
        sampleWork();
        cameraOrbit(renderer);
        viewTimelines();
        viewRendering(renderer);
        intentMotion(renderer);
        timelineDiagnostics();
        pathAndCurveShapes(renderer);
        pathFillAnimations(renderer);
        surfacePlaneTextAndWrite(renderer);
        surfaceColors(renderer);
        surfaceMeshLines(renderer);
        surfaceLight(renderer);
        surfaceMorph(renderer);
        depthSplitStrokes(renderer);
        depthSplitSpace(renderer);
        depthSplitPlotPieces(renderer);
        depthSplitSpaceTicks(renderer);
        countingText(renderer);
        textChanges(renderer);
        matchedPieces(renderer);
        diffedLines(renderer);
        matchedShapes(renderer);
        morphedOutlines(renderer);
        pieceMorphs(renderer);
        contouredPaths(renderer);
        plotEndpoints(renderer);
        plotEndpointProgress(renderer);
        plotEndpointColor(renderer);
        plotPieces(renderer);
        contourPlots(renderer);
        contourStates(renderer);
        contourLabels(renderer);
        combinedShapes(renderer);
        arrangedRows(renderer);
        handedOver(renderer);
        replacementContacts();
        highlightedParts(renderer);
        sceneThemes(renderer);
        groupStyles(renderer);
        arcsAndAngles(renderer);
        mathSymbolFallback(renderer);
        shapeProperties(renderer);
        trimmedSegments(renderer);
        zeroLengthArrows(renderer);
        geometryClips(renderer);
        plotReplot(renderer);
        plotFunctionPlay(renderer);
        instantUpdates(renderer);
        drawLayers(renderer);
        saveRestore(renderer);
        clonedObjects(renderer);
        clonedLinks(renderer);
        matchedBillboard(renderer);
        matchedPath(renderer);
        copiedTransforms(renderer);
        clonedWarps(renderer);
        rebasedCopies(renderer);
        geometryCopies(renderer);
        layeredMotions(renderer);
        layeredTransforms(renderer);
        layeredMorphs(renderer);
        cameraLayers(renderer);
        runtimeModifiers(renderer);
        boundaries(renderer);
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && defined(VIS_SAVER_GIF) \
    && defined(VIS_SAVER_VIDEO)
        videoCadence(renderer);
#endif
    }
    delete renderer;
    if (failures) std::fprintf(stderr, "%d checks failed\n", failures);
    return failures ? 1 : 0;
}
