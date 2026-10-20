#include "testSceneChecks.h"

void viewports(Renderer* renderer)
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
        CHECK(timedChild->play(Animation::create(timedCircle), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
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

void liveSceneTransitions(Renderer* renderer)
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
                                  i ? 0.5f : 4.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
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
    CHECK(stages[0]->play(Animation::shift(dot, {2, 0}), 3, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(root->transition(stages, 2u, {}, 1, 2, AnimCurve::preset(AnimCurvePreset::Linear),
                           nullptr, 0u, false) == Result::Success);
    LayoutReport report;
    CHECK(renderer->layout(root, 1, report) == Result::Success);
    CHECK(near(report.sceneAt(1)->time, 3));
    CHECK(near(report.objectAt(0)->paintBounds.x, 144, 0.5f));
    delete root;
}

void sceneTransitions(Renderer* renderer)
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
        CHECK(root->transition(nullptr, 2u, {}, 1.0f, 0.25f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::InvalidArguments);
        CHECK(root->transition(stages, 1u, {}, 1.0f, 0.25f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::InvalidArguments);
        CHECK(root->transition(duplicate, 2u, {}, 1.0f, 0.25f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "distinct scenes"));
        // A stage that already nests viewports 16 scenes deep leaves the transition no room.
        auto deep = Scene::gen(config);
        for (auto level = 1u; level < 16u; level++) {
            auto above = Scene::gen(config);
            CHECK(above && above->viewport(deep, {}) == Result::Success);
            deep = above;
        }
        Scene* nested[] = {first, deep};
        CHECK(root->transition(nested, 2u) == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "16 scenes deep"));
        delete deep;
        // A transition or a hold that would end past the largest time says so.
        float holds[] = {0.25f, 3.0e38f};
        CHECK(root->transition(stages, 2u, {}, 3.0e38f, 3.0e38f) == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "representable time range"));
        CHECK(root->transition(stages, 2u, {}, 3.0e38f, holds, 2u, {}) == Result::InvalidArguments);
        CHECK(root->diagnostic() && std::strstr(root->diagnostic(), "representable time range"));
        CHECK(root->transition(stages, 2u, {}, 1.0f, 0.25f, AnimCurve::preset(AnimCurvePreset::Linear))
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

void transitionLayoutVisuals(Renderer* renderer)
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
                                       AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);

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
                                   AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);

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
                                        AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);

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
                                     AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);

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
                                       AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);

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
void transitionStyles(Renderer* renderer)
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
                    CHECK(scene->play(Animation::shift(object, {1.0f, 1.0f, 0.0f}), 1.0f,
                                      AnimCurve::preset(AnimCurvePreset::Linear))
                          == Result::Success);
                    auto camera = config.camera;
                    camera.eye.x += 0.5f;
                    camera.target.x += 0.5f;
                    camera.eye.y += 0.5f;
                    camera.target.y += 0.5f;
                    camera.orthoHeight = 2.0f;
                    CHECK(scene->look(camera, CameraView::TwoD, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
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
