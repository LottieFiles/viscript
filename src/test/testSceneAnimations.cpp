#include "testSceneChecks.h"

void textAnimationRegression(SwRenderer* renderer)
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

void repeatedCreateRegression(SwRenderer* renderer)
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

void drawDirectionAnimations(SwRenderer* renderer)
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

void arrowCompositing(SwRenderer* renderer)
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

void directedRouteRendering(SwRenderer* renderer)
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

void fadeAnimations(SwRenderer* renderer)
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

void scaleAndIndicateAnimations(SwRenderer* renderer)
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
void emphasisEffects(SwRenderer* renderer)
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

void warpedGeometry(SwRenderer* renderer)
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

void morphAnimations(SwRenderer* renderer)
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
