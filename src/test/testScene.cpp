#include <cstdlib>
#include <type_traits>
#include <vector>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "testSceneChecks.h"

static_assert(!std::is_copy_constructible<Object>::value, "Object owns its tag");
static_assert(!std::is_copy_constructible<Scene>::value, "Scene owns its object graph");
static_assert(!std::is_copy_constructible<Renderer>::value, "Renderer owns a canvas");
static_assert(!std::is_constructible<Object, Type>::value, "Object hierarchy is closed");

int failures = 0;

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

static void renderBounds(Renderer* renderer)
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

static void layoutOcclusion(Renderer* renderer)
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

static uint32_t strokePixels(Renderer* renderer, const float* pattern, uint32_t count)
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

static void dashedStrokes(Renderer* renderer)
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

static void scene2D(Renderer* renderer)
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

static void coplanarDrawOrder(Renderer* renderer)
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

static void rendererPixelSampling(Renderer* renderer)
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

static void spaceNumbers(Renderer* renderer)
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

static void boundaries(Renderer* renderer)
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
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
        CHECK(Saver::save(odd, *renderer, "invalid.mp4") == Result::InvalidArguments);
#else
        CHECK(Saver::save(odd, *renderer, "invalid.mp4") == Result::NonSupport);
#endif
        CHECK(Saver::save(odd, *renderer, "invalid.webm") == Result::InvalidArguments);
        delete odd;
    }
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
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

static void videoCadence(Renderer* renderer)
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
        CHECK(Saver::save(scene, *renderer, "cadence.mp4", 30) == Result::Success);
        CHECK(Saver::save(scene, *renderer, "cadence.gif", 30) == Result::Success);
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
        CHECK(Saver::save(invalidScene, *renderer, "preserved.mp4", 30) == Result::InvalidArguments);
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
    auto cpu = Renderer::gen();
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

static void scene3D(Renderer* renderer)
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
static uint32_t pictureBlend(Renderer* renderer, const Asset* asset)
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

static void pictureSharpness(Renderer* renderer, const Asset* svg)
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

static void assetsAndCells(Renderer* renderer)
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

static void dimensionTransition(Renderer* renderer)
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
        CHECK(interactive->cameraInput({CameraAction::View3D, {}}) == Result::NonSupport);
        CHECK(interactive->cameraInput({CameraAction::Orbit, {0.1f, -0.05f}})
              == Result::NonSupport);
        CHECK(interactive->look(spatialCamera, CameraView::ThreeD) == Result::InsufficientCondition);
    }
    delete interactive;
}

static void pathAndCurveShapes(Renderer* renderer)
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

static void pathFillAnimations(Renderer* renderer)
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

static void runtimeModifiers(Renderer* renderer)
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
        CHECK(scene->cameraInput(invalidAction) == Result::InvalidArguments);
        CHECK(scene->cameraInput({CameraAction::Pan,
                             {std::numeric_limits<float>::quiet_NaN(), 0.0f}})
              == Result::InvalidArguments);
        CHECK(probe.inputs == 0u);
        CHECK(scene->cameraInput({CameraAction::Pan, {0.1f, 0.0f}}) == Result::Success);
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
        CHECK(scene->cameraInput({CameraAction::Pan, {0.1f, 0.0f}}) == Result::Success);
        CHECK(probe.inputs == 1u && secondProbe.inputs == 0u);
        CHECK(scene->runtimeRemove(&probe) == Result::Success);
        CHECK(scene->cameraInput({CameraAction::Pan, {0.1f, 0.0f}}) == Result::Success);
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

int main()
{
    springResponses();
    auto renderer = Renderer::gen();
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
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
        videoCadence(renderer);
#endif
    }
    delete renderer;
    if (failures) std::fprintf(stderr, "%d checks failed\n", failures);
    return failures ? 1 : 0;
}
