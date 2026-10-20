#include "testSceneChecks.h"

void highlightedParts(Renderer* renderer)
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

void sceneThemes(Renderer* renderer)
{
    auto proWhite = Theme::preset(ThemePreset::ProWhite);
    auto proBlack = Theme::preset(ThemePreset::ProBlack);
    auto adaptiveVscode = Theme::preset(ThemePreset::AdaptiveVscode);
    CHECK(Theme::ColorLimit == 10u);
    Theme implicit;
    const Theme presets[] = {implicit, proWhite, proBlack, adaptiveVscode};
    for (const auto& preset : presets) {
        CHECK(std::strcmp(preset.code.font, "JetBrainsMono") == 0);
    }
    CHECK(proWhite.objectCount == 8u);
    CHECK(proBlack.objectCount == 8u);
    CHECK(near(proWhite.objectWidth, 1.25f));
    CHECK(near(proBlack.objectWidth, 1.25f));
    // Every preset keeps structural boxes square; a custom Theme may round them.
    CHECK(implicit.roundness == 0.0f && proWhite.roundness == 0.0f && proBlack.roundness == 0.0f
          && adaptiveVscode.roundness == 0.0f);
    CHECK(same(adaptiveVscode.background, proWhite.background));
    CHECK(same(adaptiveVscode.objects[0], proWhite.objects[0]));
    CHECK(same(implicit.background, proWhite.background));
    CHECK(same(implicit.objects[0], proWhite.objects[0]));
    CHECK(same(proWhite.background, Color::hex("#ffffff")));
    CHECK(same(proWhite.objects[0], Color::hex("#b45f06")));
    CHECK(same(proBlack.background, Color::hex("#000000")));
    CHECK(same(proBlack.objects[0], Color::hex("#f28e2b")));
    CHECK(same(proBlack.h1.color, Color::hex("#ffffff")));
    CHECK(same(proWhite.color(ThemeColorRole::Danger), Color::hex("#b3261e")));
    CHECK(same(proBlack.color(ThemeColorRole::Success), Color::hex("#7ee787")));
    CHECK(same(proWhite.color(ThemeColorRole::Result), Color::hex("#8a6500")));
    CHECK(same(proBlack.color(ThemeColorRole::Focus), Color::hex("#6ea8fe")));
    CHECK(same(proBlack.color(ThemeColorRole::Background), proBlack.background));
    CHECK(same(proWhite.axis.x, proWhite.axis.grid));
    CHECK(same(proWhite.axis.y, proWhite.axis.grid));
    CHECK(same(proWhite.axis.z, proWhite.axis.grid));
    CHECK(same(proWhite.axis.grid, Color::hex("#d8dadd")));
    CHECK(same(proBlack.axis.x, proBlack.axis.grid));
    CHECK(same(proBlack.axis.y, proBlack.axis.grid));
    CHECK(same(proBlack.axis.z, proBlack.axis.grid));
    CHECK(same(proBlack.axis.grid, Color::hex("#d8dadd")));
    CHECK(!proWhite.gradient && !proBlack.gradient);
    CHECK(same(proWhite.endGradientStop, Color::hex("#666666")));
    CHECK(same(proBlack.endGradientStop, Color::hex("#999999")));

    Config config;
    CHECK(same(config.background, proWhite.background));
    auto scene = Scene::gen(config);
    CHECK(scene != nullptr);
    if (!scene) return;
    CHECK(same(scene->theme().background, proWhite.background));
    CHECK(same(scene->theme().objects[0], proWhite.objects[0]));
    CHECK(scene->theme(proBlack) == Result::Success);
    CHECK(same(scene->config().background, proBlack.background));
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
    CHECK(scene->theme(proBlack) == Result::InsufficientCondition);
    CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "the theme must come before objects"));
    delete scene;

    // A text that names no font takes its role's as it is, a font list included, whatever its
    // string: the renderer sets each character in the first font of the list that has it, so
    // Korean in a role of Source Serif 4 alone stays in that font, and a list sets the Hangul a
    // text shows only later, which JetBrainsMono, like Source Serif 4, lacks.
    auto editorial = proWhite;
    editorial.h2.font = "Source Serif 4, Pretendard";
    editorial.h3.font = "JetBrainsMono, Pretendard";
    editorial.code.font = "Source Serif 4";
    scene = Scene::gen();
    auto listed = Text::gen(u8"\ud55c\uae00 \uc124\uba85");
    auto alone = Text::gen(u8"\ud55c\uae00 \uc124\uba85");
    auto named = Text::gen(u8"\uc0ac\uc6a9\uc790 \ud3f0\ud2b8");
    auto later = Text::gen("English first");
    CHECK(scene && listed && alone && named && later);
    if (scene && listed && alone && named && later) {
        CHECK(scene->theme(editorial) == Result::Success);
        listed->role = TextRole::H2;
        alone->role = TextRole::Code;
        later->role = TextRole::H3;
        CHECK(named->font("Custom Sans") == Result::Success);
        CHECK(scene->add(listed) == Result::Success);
        CHECK(scene->add(alone) == Result::Success);
        CHECK(scene->add(named) == Result::Success);
        CHECK(scene->add(later) == Result::Success);
        CHECK(std::strcmp(listed->font(), "Source Serif 4, Pretendard") == 0);
        CHECK(std::strcmp(alone->font(), "Source Serif 4") == 0);
        CHECK(std::strcmp(named->font(), "Custom Sans") == 0);
        TextExtent hangul;
        CHECK(scene->measure(later, u8"\ud55c\uae00", hangul) == Result::Success && hangul.missing == 0u);
    } else {
        delete listed;
        delete alone;
        delete named;
        delete later;
    }
    delete scene;

    auto invalid = proBlack;
    invalid.objectCount = 0u;
    scene = Scene::gen();
    CHECK(scene != nullptr);
    if (scene) CHECK(scene->theme(invalid) == Result::InvalidArguments);
    if (scene) CHECK(scene->diagnostic() && std::strstr(scene->diagnostic(), "1 to 10 object colors"));
    delete scene;

    invalid = proBlack;
    invalid.objectWidth = 0.0f;
    scene = Scene::gen();
    CHECK(scene != nullptr);
    if (scene) CHECK(scene->theme(invalid) == Result::InvalidArguments);
    delete scene;

    for (auto roundness : {-0.01f, 0.51f, std::numeric_limits<float>::quiet_NaN()}) {
        invalid = proBlack;
        invalid.roundness = roundness;
        scene = Scene::gen();
        CHECK(scene != nullptr);
        if (scene) CHECK(scene->theme(invalid) == Result::InvalidArguments);
        delete scene;
    }
    auto pill = proBlack;
    pill.roundness = 0.5f;
    scene = Scene::gen();
    CHECK(scene != nullptr);
    if (scene) CHECK(scene->theme(pill) == Result::Success && scene->theme().roundness == 0.5f);
    delete scene;

    Config gradientConfig;
    gradientConfig.width = 200u;
    gradientConfig.height = 100u;
    gradientConfig.camera.orthoHeight = 4.0f;
    gradientConfig.background = Color::hex("#ffffff");
    auto gradientTheme = proBlack;
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

void groupStyles(Renderer* renderer)
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
    CHECK(scene->play(AnimationTarget::from(outer).fill(white), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
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
