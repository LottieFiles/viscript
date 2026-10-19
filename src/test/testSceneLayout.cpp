#include "testSceneChecks.h"

void objectTree(Renderer* renderer)
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
void objectIds()
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
    CHECK(first->play(Animation::fade(stranger, 0.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::InvalidArguments);
    CHECK(first->diagnostic() && std::strcmp(first->diagnostic(), "the object is not part of this scene") == 0);
    CHECK(first->remove(stranger) != Result::Success);
    CHECK(first->play(Animation::fade(later, 0.5f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(second->play(Animation::fade(stranger, 0.25f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    delete first;
    delete second;
}

// A text's box moves off where align pins it by boxOffset, and bounds and the layout calls see
// it there, while the glyphs stay where align pins them.
void offsetBoxes(Renderer* renderer)
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
    BBox drawn;
    BBox kept;
    CHECK(renderer->bounds(scene, word, 0.0f, drawn) == Result::Success);
    plain->opacity = 1.0f;
    word->opacity = 0.0f;
    CHECK(scene->update(plain) == Result::Success && scene->update(word) == Result::Success);
    CHECK(renderer->bounds(scene, plain, 0.0f, kept) == Result::Success);
    CHECK(near(drawn.x, kept.x) && near(drawn.y, kept.y) && near(drawn.width, kept.width)
          && near(drawn.height, kept.height));
    delete scene;
    // An offset must be finite.
    auto broken = Text::gen("x");
    auto other = Scene::gen();
    if (broken) broken->boxOffset = {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    CHECK(broken && other && other->add(broken) == Result::InvalidArguments);
    delete broken;
    delete other;
}

void boundsAndLayout()
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

void sceneUpdate(Renderer* renderer)
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
            CHECK(historical->play(Animation::create(historicalRectangle), 1.0f,
                                   AnimCurve::preset(AnimCurvePreset::Linear))
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

void groupAndRectangleRendering(Renderer* renderer)
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

void connectorOutlines(Renderer* renderer)
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
    CHECK(corners->play(AnimationTarget::from(box).size(Vec2{72, 72}).corner(20), 1,
                        AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
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
    CHECK(scene->play(AnimationTarget::from(source).radius(36).shift({20, 0, 0}), 1,
                      AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->seek(0) == Result::Success);
    auto ride = Animation::moveAlong(token, edge, true);
    ride.from = 0.8f;
    ride.to = 0.2f;
    CHECK(scene->play(ride, 1, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
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
    CHECK(scene->play(Animation::shift(sink, {50, 0, 0}), 1, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(renderer->bounds(scene, token, 1.5f, held) == Result::Success);
    CHECK(near(held.center().x - 480, finalPosition.x, 0.001f));
    CHECK(near(270 - held.center().y, finalPosition.y, 0.001f));
    CHECK(scene->play(Animation::shift(token, {5, 0, 0}), 1, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
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
    CHECK(parentScene->play(Animation::shift(parent, {20, 0, 0}), 1, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(parentScene->seek(0) == Result::Success);
    CHECK(parentScene->play(Animation::moveAlong(child, parentEdge), 1, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
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

void connectorAnimation(Renderer* renderer)
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
                    CHECK(scene->play(target, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
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
                                   AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
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
                                      AnimCurve::preset(AnimCurvePreset::Linear)) == Result::Success);
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

void animationTargets(Renderer* renderer)
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
            CHECK(scene->play(atomic, 2, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), 0.25f)
                  == Result::InvalidArguments);
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
            CHECK(scene->play(duplicate, 2, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), 0.0f)
                  == Result::InvalidArguments);
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
                              AnimCurve::preset(AnimCurvePreset::Linear), 0.0f) == Result::InvalidArguments);
            CHECK(scene->play(&noProperties, 0, 1.0f, AnimCurve::preset(AnimCurvePreset::Linear), 0.0f)
                  == Result::InvalidArguments);
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
            CHECK(scene->play(targets, 2, 2.0f, AnimCurve::preset(AnimCurvePreset::Linear), 0.25f) == Result::Success);
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
