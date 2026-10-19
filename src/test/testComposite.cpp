// Composition: objects that draw themselves and their descendants into a layer of their own,
// which the renderer composites at once onto what lies beneath.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "vis.h"
#include "testCheck.h"

using namespace vis;

static constexpr Color Black = {0, 0, 0, 255};
static constexpr Color Red = {255, 0, 0, 255};
static constexpr Color Blue = {0, 0, 255, 255};
static constexpr Color Green = {0, 255, 0, 255};

// 120 x 80 pixels at 20 pixels per world unit, world origin at pixel (60, 40), +y up.
static Scene* _scene()
{
    Config config;
    config.width = 120;
    config.height = 80;
    config.camera.orthoHeight = 4.0f;
    config.background = Black;
    config.antialiasing = false;
    auto scene = Scene::gen(config);
    CHECK(scene);
    return scene;
}

static Rectangle* _square(float x, float y, float side, Color fill, int32_t layer = 0)
{
    auto square = Rectangle::gen({x, y, 0.0f}, {side, side});
    CHECK(square);
    if (!square) std::exit(1);
    square->style.stroke.a = 0;
    square->fill(fill);
    square->layer = layer;
    return square;
}

static Circle* _circle(float x, float y, float radius, Color fill)
{
    auto circle = Circle::gen({x, y, 0.0f}, radius);
    CHECK(circle);
    if (!circle) std::exit(1);
    circle->style.stroke.a = 0;
    circle->fill(fill);
    return circle;
}

static Color _at(const Surface& surface, uint32_t x, uint32_t y)
{
    auto value = surface.data()[static_cast<size_t>(y) * surface.stride() + x];
    return {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8u),
            static_cast<uint8_t>(value >> 16u), static_cast<uint8_t>(value >> 24u)};
}

static bool _near(Color color, Color expected, int tolerance = 3)
{
    return std::abs(color.r - expected.r) <= tolerance && std::abs(color.g - expected.g) <= tolerance
           && std::abs(color.b - expected.b) <= tolerance;
}

static bool _render(const Scene* scene, float time, Surface& surface)
{
    auto renderer = Renderer::gen();
    CHECK(renderer);
    if (!renderer) return false;
    auto result = renderer->render(scene, time, surface);
    CHECK(result == Result::Success);
    delete renderer;
    return result == Result::Success;
}

// Renders with a renderer kept across frames and with a fresh one, which must agree.
static void _retained(Renderer* reused, const Scene* scene, float time, const char* label)
{
    Surface kept;
    Surface fresh;
    auto first = reused->render(scene, time, kept);
    CHECK(first == Result::Success);
    if (first != Result::Success || !_render(scene, time, fresh)) return;
    auto same = equal(kept, fresh);
    if (!same) std::fprintf(stderr, "retained frame differs: %s t=%g\n", label, time);
    CHECK(same);
}

// Two overlapping opaque squares in a half transparent group: the overlap shows through
// unless the group is isolated.
static void isolatedGroups()
{
    for (auto isolated : {false, true}) {
        auto scene = _scene();
        auto group = Group::gen();
        CHECK(group);
        if (!scene || !group) {
            delete group;
            delete scene;
            continue;
        }
        group->opacity = 0.5f;
        CHECK(group->isolate(isolated) == Result::Success);
        CHECK(group->isolated() == isolated);
        CHECK(group->add(_square(-0.5f, 0.0f, 2.0f, Red)) == Result::Success);
        CHECK(group->add(_square(0.5f, 0.0f, 2.0f, Red)) == Result::Success);
        CHECK(scene->add(group) == Result::Success);
        Surface surface;
        if (_render(scene, 0.0f, surface)) {
            auto single = _at(surface, 45u, 40u);
            auto overlap = _at(surface, 60u, 40u);
            CHECK(_near(single, {128, 0, 0, 255}));
            CHECK(_near(overlap, isolated ? Color{128, 0, 0, 255} : Color{191, 0, 0, 255}));
            CHECK(_near(_at(surface, 5u, 5u), Black, 0));
        }
        delete scene;
    }
}

// Opacity of nested layers multiplies, as does that of plain groups between them, and a
// fade of the isolated group fades the layer.
static void nestedAndAnimatedLayers()
{
    auto scene = _scene();
    auto outer = Group::gen();
    auto plain = Group::gen();
    auto inner = Group::gen();
    CHECK(outer && plain && inner);
    if (!scene || !outer || !plain || !inner) {
        delete outer;
        delete plain;
        delete inner;
        delete scene;
        return;
    }
    outer->opacity = 0.5f;
    plain->opacity = 0.5f;
    inner->opacity = 0.5f;
    CHECK(outer->isolate() == Result::Success);
    CHECK(inner->isolate() == Result::Success);
    CHECK(inner->add(_square(-1.5f, 0.0f, 1.0f, Red)) == Result::Success);
    CHECK(plain->add(inner) == Result::Success);
    CHECK(outer->add(plain) == Result::Success);
    CHECK(outer->add(_square(1.5f, 0.0f, 1.0f, Red)) == Result::Success);
    CHECK(scene->add(outer) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(outer).opacity(0.0f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);

    Surface surface;
    if (_render(scene, 0.0f, surface)) {
        CHECK(_near(_at(surface, 30u, 40u), {32, 0, 0, 255}));
        CHECK(_near(_at(surface, 90u, 40u), {128, 0, 0, 255}));
    }
    if (_render(scene, 0.5f, surface)) {
        CHECK(_near(_at(surface, 30u, 40u), {16, 0, 0, 255}));
        CHECK(_near(_at(surface, 90u, 40u), {64, 0, 0, 255}));
    }
    if (_render(scene, 1.0f, surface)) {
        CHECK(_near(_at(surface, 30u, 40u), Black, 0));
        CHECK(_near(_at(surface, 90u, 40u), Black, 0));
    }
    delete scene;
}

// A layer draws at its object's layer among its siblings; the layers of its children order
// them only inside it.
static void layerOrder()
{
    for (auto isolated : {false, true}) {
        auto scene = _scene();
        auto group = Group::gen();
        CHECK(group);
        if (!scene || !group) {
            delete group;
            delete scene;
            continue;
        }
        CHECK(group->isolate(isolated) == Result::Success);
        CHECK(group->add(_square(-1.0f, 0.0f, 2.0f, Red, 0)) == Result::Success);
        CHECK(group->add(_square(1.0f, 0.0f, 2.0f, Blue, 10)) == Result::Success);
        CHECK(scene->add(group) == Result::Success);
        CHECK(scene->add(_square(0.0f, 0.0f, 1.0f, Green, 5)) == Result::Success);
        Surface surface;
        if (_render(scene, 0.0f, surface)) {
            // The green square sits above the red one either way, and above the blue one only
            // when the group draws as one layer at layer 0.
            CHECK(_near(_at(surface, 55u, 40u), Green, 0));
            CHECK(_near(_at(surface, 65u, 40u), isolated ? Green : Blue, 0));
        }
        delete scene;
    }
}

// A renderer kept across frames follows objects into and out of layers and layers that come
// and go, as a fresh renderer would draw each frame.
static void retainedLayers()
{
    auto scene = _scene();
    auto group = Group::gen();
    auto later = Group::gen();
    auto reused = Renderer::gen();
    CHECK(group && later && reused);
    if (!scene || !group || !later || !reused) {
        delete group;
        delete later;
        delete reused;
        delete scene;
        return;
    }
    group->opacity = 0.5f;
    later->opacity = 0.75f;
    CHECK(later->isolate() == Result::Success);
    CHECK(group->add(_square(-0.5f, 0.0f, 2.0f, Red)) == Result::Success);
    CHECK(group->add(_square(0.5f, 0.0f, 2.0f, Blue)) == Result::Success);
    CHECK(later->add(_square(2.0f, 1.0f, 1.0f, Green)) == Result::Success);
    CHECK(scene->add(group) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(group).shift({0.5f, 0.0f, 0.0f}), 1.0f,
                      AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->add(later) == Result::Success);
    CHECK(scene->play(AnimationTarget::from(later).opacity(0.25f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(scene->remove(later) == Result::Success);

    _retained(reused, scene, 0.0f, "plain");
    CHECK(group->isolate() == Result::Success);
    _retained(reused, scene, 0.5f, "isolated");
    _retained(reused, scene, 1.5f, "second layer");
    CHECK(group->isolate(false) == Result::Success);
    _retained(reused, scene, 1.6f, "plain again");
    _retained(reused, scene, 2.5f, "second layer removed");
    CHECK(group->isolate() == Result::Success);
    _retained(reused, scene, 0.25f, "seek back");
    delete reused;
    delete scene;
}

// Layers carry over to the stages of a scene transition, and a scene shown inside another
// one keeps its composition.
static void transitionsAndMounts()
{
    auto root = _scene();
    auto first = _scene();
    auto second = _scene();
    auto group = Group::gen();
    CHECK(group);
    if (!root || !first || !second || !group) {
        delete group;
        delete root;
        delete first;
        delete second;
        return;
    }
    group->opacity = 0.5f;
    CHECK(group->isolate() == Result::Success);
    CHECK(group->add(_square(-0.5f, 0.0f, 2.0f, Red)) == Result::Success);
    CHECK(group->add(_square(0.5f, 0.0f, 2.0f, Red)) == Result::Success);
    CHECK(first->add(group) == Result::Success);
    CHECK(first->play(AnimationTarget::from(group).shift({0.0f, 0.5f, 0.0f}), 1.0f) == Result::Success);
    CHECK(second->add(_square(0.0f, 0.0f, 1.0f, Blue)) == Result::Success);
    CHECK(second->play(AnimationTarget::from(second->objectAt(0)).opacity(0.5f), 1.0f) == Result::Success);
    Scene* stages[] = {first, second};
    CHECK(root->transition(stages, 2u, {}, 1.0f) == Result::Success);
    auto reused = Renderer::gen();
    CHECK(reused);
    if (reused) {
        Surface surface;
        // Halfway through the cross-fade the outgoing layer keeps its opacity and its squares
        // fade by half; objects fade one by one, so their overlap shows during the fade.
        if (_render(root, 0.5f, surface)) {
            CHECK(_near(_at(surface, 45u, 20u), {64, 0, 0, 255}));
            CHECK(_at(surface, 60u, 20u).r > 64);
        }
        for (auto time : {0.0f, 0.25f, 0.5f, 0.75f, 2.0f, 0.5f})
            _retained(reused, root, time, "transition");
        delete reused;
    }
    CHECK(group->isolate(false) == Result::InsufficientCondition);
    CHECK(group->isolated());
    delete root;
}

// A 3 x 3 red square seen through a circle of radius 1 at (1.5, 0): pixel 80 lies in both,
// pixel 40 only in the square and pixel 100 only in the circle.
struct Spotlight
{
    Scene* scene = nullptr;
    Rectangle* square = nullptr;
    Circle* hole = nullptr;

    explicit Spotlight(Color hole = {255, 255, 255, 255})
    {
        scene = _scene();
        square = _square(0.0f, 0.0f, 3.0f, Red);
        this->hole = _circle(1.5f, 0.0f, 1.0f, hole);
        if (scene) {
            CHECK(scene->add(square) == Result::Success);
            CHECK(scene->add(this->hole) == Result::Success);
        }
    }

    ~Spotlight()
    {
        delete scene;
    }

    // The colors at pixels 40, 80 and 100 of row 40.
    void expect(float time, Color square, Color both, Color hole, const char* label) const
    {
        Surface surface;
        if (!scene || !_render(scene, time, surface)) return;
        Color seen[] = {_at(surface, 40u, 40u), _at(surface, 80u, 40u), _at(surface, 100u, 40u)};
        auto ok = _near(seen[0], square) && _near(seen[1], both) && _near(seen[2], hole);
        if (!ok) {
            std::fprintf(stderr, "mask colors differ: %s t=%g:", label, time);
            for (auto& color : seen)
                std::fprintf(stderr, " (%d %d %d)", color.r, color.g, color.b);
            std::fprintf(stderr, "\n");
        }
        CHECK(ok);
    }
};

// Each mode shows the square where the circle is, or is not, opaque or bright; the circle
// itself never draws.
static void maskModes()
{
    {
        Spotlight spot;
        CHECK(spot.square->mask() == nullptr);
        CHECK(spot.square->mask(spot.hole) == Result::Success);
        CHECK(spot.square->mask() == spot.hole);
        CHECK(spot.square->maskMode() == MaskMode::Alpha);
        spot.expect(0.0f, Black, Red, Black, "alpha");
        CHECK(spot.square->mask(spot.hole, MaskMode::InverseAlpha) == Result::Success);
        spot.expect(0.0f, Red, Black, Black, "inverse alpha");
        CHECK(spot.square->mask(spot.hole, MaskMode::Luma) == Result::Success);
        spot.expect(0.0f, Black, Red, Black, "luma");
        CHECK(spot.square->mask(spot.hole, MaskMode::InverseLuma) == Result::Success);
        spot.expect(0.0f, Red, Black, Black, "inverse luma");
        CHECK(spot.square->mask(nullptr) == Result::Success);
        CHECK(spot.square->mask() == nullptr);
        spot.expect(0.0f, Red, {255, 255, 255, 255}, {255, 255, 255, 255}, "unmasked");
    }
    {
        // A dark mask shows nothing through luma, but its alpha still counts.
        Spotlight spot(Black);
        CHECK(spot.square->mask(spot.hole, MaskMode::Luma) == Result::Success);
        spot.expect(0.0f, Black, Black, Black, "dark luma");
        CHECK(spot.square->mask(spot.hole, MaskMode::Alpha) == Result::Success);
        spot.expect(0.0f, Black, Red, Black, "dark alpha");
    }
}

// The target moves and fades on its own timeline; before it is shown and after it is
// removed the mask is empty.
static void maskTimeline()
{
    Spotlight spot;
    if (!spot.scene) return;
    CHECK(spot.square->mask(spot.hole) == Result::Success);
    CHECK(spot.scene->play(AnimationTarget::from(spot.hole).shift({-3.0f, 0.0f, 0.0f}), 1.0f,
                           AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(spot.scene->play(AnimationTarget::from(spot.hole).opacity(0.5f), 1.0f,
                           AnimCurve::preset(AnimCurvePreset::Linear))
          == Result::Success);
    CHECK(spot.scene->remove(spot.hole) == Result::Success);
    spot.expect(0.0f, Black, Red, Black, "start");
    spot.expect(1.0f, Red, Black, Black, "moved");
    spot.expect(1.5f, {191, 0, 0, 255}, Black, Black, "fading");
    spot.expect(2.5f, Black, Black, Black, "removed");

    auto reused = Renderer::gen();
    CHECK(reused);
    if (!reused) return;
    for (auto time : {0.0f, 0.5f, 1.0f, 1.5f, 2.5f, 0.25f})
        _retained(reused, spot.scene, time, "moving mask");
    delete reused;
}

// Group opacity above both the target and the masked object fades the result once; the mask
// takes the opacity of the groups between the target and that shared group, and a target
// inside the masked object moves with it.
static void maskGroups()
{
    {
        Spotlight spot;
        if (!spot.scene) return;
        auto group = Group::gen();
        CHECK(group);
        if (!group) return;
        group->opacity = 0.5f;
        auto square = _square(0.0f, 0.0f, 3.0f, Red);
        auto hole = _circle(1.5f, 0.0f, 1.0f, {255, 255, 255, 255});
        CHECK(group->add(square) == Result::Success);
        CHECK(group->add(hole) == Result::Success);
        CHECK(spot.scene->remove(spot.square) == Result::Success);
        CHECK(spot.scene->remove(spot.hole) == Result::Success);
        CHECK(spot.scene->add(group) == Result::Success);
        CHECK(square->mask(hole) == Result::Success);
        Surface surface;
        if (_render(spot.scene, 0.1f, surface)) {
            CHECK(_near(_at(surface, 80u, 40u), {128, 0, 0, 255}));
            CHECK(_near(_at(surface, 40u, 40u), Black));
        }
    }
    {
        Spotlight spot;
        if (!spot.scene) return;
        auto group = Group::gen();
        CHECK(group);
        if (!group) return;
        group->opacity = 0.5f;
        auto hole = _circle(1.5f, 0.0f, 1.0f, {255, 255, 255, 255});
        CHECK(group->add(hole) == Result::Success);
        CHECK(spot.scene->add(group) == Result::Success);
        CHECK(spot.scene->remove(spot.hole) == Result::Success);
        CHECK(spot.square->mask(hole) == Result::Success);
        Surface surface;
        if (_render(spot.scene, 0.1f, surface)) {
            CHECK(_near(_at(surface, 80u, 40u), {128, 0, 0, 255}));
            CHECK(_near(_at(surface, 100u, 40u), Black));
        }
    }
    {
        auto scene = _scene();
        auto card = Group::gen();
        CHECK(card);
        if (!scene || !card) {
            delete card;
            delete scene;
            return;
        }
        auto hole = _circle(1.5f, 0.0f, 1.0f, {255, 255, 255, 255});
        CHECK(card->add(_square(0.0f, 0.0f, 3.0f, Red)) == Result::Success);
        CHECK(card->add(hole) == Result::Success);
        CHECK(scene->add(card) == Result::Success);
        CHECK(card->mask(hole) == Result::Success);
        CHECK(scene->play(AnimationTarget::from(card).shift({-1.0f, 0.0f, 0.0f}), 1.0f,
                          AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        Surface surface;
        if (_render(scene, 1.0f, surface)) {
            CHECK(_near(_at(surface, 60u, 40u), Red));
            CHECK(_near(_at(surface, 80u, 40u), Black));
            CHECK(_near(_at(surface, 40u, 40u), Black));
        }
        delete scene;
    }
}

// Masks that could draw themselves, foreign targets and shared targets are refused; layers
// turn into masks and back between frames; masks stay out of picks and layout reports.
static void maskRules()
{
    Spotlight spot;
    auto other = _scene();
    if (!spot.scene || !other) {
        delete other;
        return;
    }
    auto group = Group::gen();
    auto inner = _circle(0.0f, 0.0f, 0.5f, Blue);
    auto stranger = _circle(0.0f, 0.0f, 0.5f, Blue);
    auto loose = _circle(0.0f, 0.0f, 0.5f, Blue);
    CHECK(group && inner);
    if (!group || !inner) return;
    CHECK(group->add(inner) == Result::Success);
    CHECK(spot.scene->add(group) == Result::Success);
    CHECK(other->add(stranger) == Result::Success);

    CHECK(spot.square->mask(spot.square) == Result::InvalidArguments);
    CHECK(inner->mask(group) == Result::InvalidArguments);
    CHECK(spot.square->mask(stranger) == Result::InvalidArguments);
    CHECK(spot.square->mask(loose) == Result::InvalidArguments);
    CHECK(loose->mask(spot.hole) == Result::InvalidArguments);
    CHECK(spot.square->mask(spot.hole, static_cast<MaskMode>(9)) == Result::InvalidArguments);
    CHECK(spot.square->mask(spot.hole) == Result::Success);
    CHECK(group->mask(spot.hole) == Result::InvalidArguments);
    CHECK(spot.hole->mask(inner) == Result::InvalidArguments);
    CHECK(group->mask(spot.square) == Result::InvalidArguments);
    CHECK(inner->mask(group) == Result::InvalidArguments);
    CHECK(spot.square->mask(group) == Result::Success);
    CHECK(inner->mask(spot.hole) == Result::InvalidArguments);
    CHECK(spot.square->mask(spot.hole) == Result::Success);
    CHECK(group->mask(spot.square) == Result::InvalidArguments);

    auto reused = Renderer::gen();
    CHECK(reused);
    if (reused) {
        CHECK(spot.square->mask(nullptr) == Result::Success);
        CHECK(spot.hole->isolate() == Result::Success);
        _retained(reused, spot.scene, 0.0f, "layer");
        CHECK(spot.square->mask(spot.hole) == Result::Success);
        _retained(reused, spot.scene, 0.0f, "layer becomes a mask");
        CHECK(spot.square->mask(nullptr) == Result::Success);
        _retained(reused, spot.scene, 0.0f, "mask becomes a layer");
        CHECK(spot.square->mask(spot.hole, MaskMode::InverseAlpha) == Result::Success);
        _retained(reused, spot.scene, 0.0f, "inverse");
        CHECK(spot.square->mask(group) == Result::Success);
        _retained(reused, spot.scene, 0.0f, "another target");

        const Object* candidates[] = {spot.hole};
        PixelSample sample;
        CHECK(reused->sample(spot.scene, 0.0f, {100.0f, 40.0f}, candidates, 1u, sample) == Result::Success);
        CHECK(spot.square->mask(spot.hole) == Result::Success);
        CHECK(reused->sample(spot.scene, 0.0f, {100.0f, 40.0f}, candidates, 1u, sample)
              == Result::InsufficientCondition);
        LayoutReport report;
        CHECK(reused->layout(spot.scene, 0.0f, report) == Result::Success);
        auto squareSeen = false;
        for (auto i = 0u; i < report.visualCount(); i++) {
            auto visual = report.visualAt(i);
            auto object = visual ? report.objectAt(visual->object) : nullptr;
            CHECK(!object || object->object != spot.hole);
            if (object && object->object == spot.square) squareSeen = true;
        }
        CHECK(squareSeen);
        delete reused;
    }

    auto root = _scene();
    if (root) {
        CHECK(root->viewport(spot.scene, {}) == Result::Success);
        CHECK(spot.square->mask(nullptr) == Result::InsufficientCondition);
        CHECK(spot.square->mask() == spot.hole);
        spot.scene = nullptr;
        delete root;
    }
    delete loose;
    delete other;
}

// A red square over a gray one combines by its blend mode; where it lies over the black
// background alone, black takes part too.
static void blendModes()
{
    struct Case
    {
        BlendMode mode;
        Color overGray;
        Color overBlack;
    };
    static constexpr Color Gray = {128, 128, 128, 255};
    static constexpr Case cases[] = {
        {BlendMode::Normal, Red, Red},
        {BlendMode::Multiply, {128, 0, 0, 255}, Black},
        {BlendMode::Screen, {255, 128, 128, 255}, Red},
        {BlendMode::Darken, {128, 0, 0, 255}, Black},
        {BlendMode::Lighten, {255, 128, 128, 255}, Red},
        {BlendMode::Difference, {127, 128, 128, 255}, Red},
        {BlendMode::Add, {255, 128, 128, 255}, Red},
    };
    for (auto& entry : cases) {
        auto scene = _scene();
        if (!scene) continue;
        auto top = _square(1.0f, 0.0f, 2.0f, Red);
        CHECK(scene->add(_square(-0.5f, 0.0f, 2.0f, Gray)) == Result::Success);
        CHECK(scene->add(top) == Result::Success);
        CHECK(top->blend(entry.mode) == Result::Success);
        CHECK(top->blend() == entry.mode);
        Surface surface;
        if (_render(scene, 0.0f, surface)) {
            auto overGray = _at(surface, 60u, 40u);
            auto overBlack = _at(surface, 85u, 40u);
            auto ok = _near(overGray, entry.overGray) && _near(overBlack, entry.overBlack);
            if (!ok) {
                std::fprintf(stderr, "blend %d: (%d %d %d) (%d %d %d)\n", static_cast<int>(entry.mode), overGray.r,
                             overGray.g, overGray.b, overBlack.r, overBlack.g, overBlack.b);
            }
            CHECK(ok);
        }
        delete scene;
    }

    // Inside an isolated group a blended child combines only with its siblings; the group
    // then draws normally over what lies beneath.
    auto scene = _scene();
    auto group = Group::gen();
    CHECK(group);
    if (scene && group) {
        auto top = _square(0.5f, 0.0f, 2.0f, Red);
        CHECK(scene->add(_square(0.0f, 0.0f, 4.0f, {128, 128, 128, 255})) == Result::Success);
        CHECK(group->add(_square(-0.5f, 0.0f, 2.0f, {255, 255, 255, 255})) == Result::Success);
        CHECK(group->add(top) == Result::Success);
        CHECK(top->blend(BlendMode::Multiply) == Result::Success);
        CHECK(group->isolate() == Result::Success);
        CHECK(scene->add(group) == Result::Success);
        auto reused = Renderer::gen();
        Surface surface;
        if (_render(scene, 0.0f, surface)) {
            CHECK(_near(_at(surface, 60u, 40u), Red));
            CHECK(_near(_at(surface, 75u, 40u), Red));
        }
        CHECK(group->isolate(false) == Result::Success);
        if (_render(scene, 0.0f, surface)) {
            CHECK(_near(_at(surface, 60u, 40u), Red));
            CHECK(_near(_at(surface, 75u, 40u), {128, 0, 0, 255}));
        }
        if (reused) {
            _retained(reused, scene, 0.0f, "blend");
            CHECK(top->blend(BlendMode::Screen) == Result::Success);
            _retained(reused, scene, 0.0f, "blend changed");
            CHECK(top->blend(BlendMode::Normal) == Result::Success);
            _retained(reused, scene, 0.0f, "blend removed");
            CHECK(top->blend(BlendMode::Multiply) == Result::Success);
            CHECK(group->isolate() == Result::Success);
            _retained(reused, scene, 0.0f, "blend isolated");
            CHECK(top->blend(BlendMode::Normal) == Result::Success);
            _retained(reused, scene, 0.0f, "isolated without blend");
            delete reused;
        }
        CHECK(top->blend(static_cast<BlendMode>(40)) == Result::InvalidArguments);
        auto root = _scene();
        if (root) {
            CHECK(root->viewport(scene, {}) == Result::Success);
            CHECK(top->blend(BlendMode::Multiply) == Result::InsufficientCondition);
            scene = nullptr;
            delete root;
        }
    } else {
        delete group;
    }
    delete scene;
}

// A hard shadow falls right and down by its pixel offset beneath the square; a blur softens
// the edges of the layer; both travel with animations and fade with the groups above.
static void effects()
{
    {
        auto scene = _scene();
        if (!scene) return;
        auto square = _square(0.0f, 0.0f, 2.0f, Red);
        square->shadow = {Blue, {20.0f, -20.0f}, 0.0f};
        CHECK(scene->add(square) == Result::Success);
        Surface surface;
        if (_render(scene, 0.0f, surface)) {
            CHECK(_near(_at(surface, 90u, 70u), Blue));
            CHECK(_near(_at(surface, 70u, 50u), Red));
            CHECK(_near(_at(surface, 50u, 30u), Red));
            CHECK(_near(_at(surface, 30u, 70u), Black, 0));
            CHECK(_near(_at(surface, 90u, 30u), Black, 0));
        }
        delete scene;
    }
    {
        auto scene = _scene();
        auto group = Group::gen();
        CHECK(group);
        if (!scene || !group) {
            delete group;
            delete scene;
            return;
        }
        auto square = _square(0.0f, 0.0f, 2.0f, Red);
        square->blur = 4.0f;
        group->opacity = 0.5f;
        CHECK(group->add(square) == Result::Success);
        CHECK(scene->add(group) == Result::Success);
        Surface surface;
        if (_render(scene, 0.0f, surface)) {
            auto center = _at(surface, 60u, 40u);
            auto edge = _at(surface, 40u, 40u);
            auto outside = _at(surface, 34u, 40u);
            CHECK(_near(center, {128, 0, 0, 255}, 6));
            CHECK(edge.r > 30 && edge.r < 100);
            CHECK(outside.r > 0 && outside.r < edge.r);
            CHECK(_near(_at(surface, 15u, 40u), Black, 0));
        }
        delete scene;
    }
    {
        // Animated from nothing: the square is a layer for the whole timeline.
        auto scene = _scene();
        if (!scene) return;
        auto square = _square(0.0f, 0.0f, 2.0f, Red);
        CHECK(scene->add(square) == Result::Success);
        CHECK(scene->play(AnimationTarget::from(square).blur(6.0f), 1.0f, AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        CHECK(scene->play(AnimationTarget::from(square).shadow({Blue, {10.0f, 0.0f}, 2.0f}), 1.0f,
                          AnimCurve::preset(AnimCurvePreset::Linear))
              == Result::Success);
        CHECK(square->blur == 6.0f);
        Surface surface;
        if (_render(scene, 0.0f, surface)) {
            CHECK(_near(_at(surface, 38u, 40u), Black, 0));
            CHECK(_near(_at(surface, 85u, 40u), Black, 0));
        }
        if (_render(scene, 1.0f, surface)) CHECK(_at(surface, 38u, 40u).r > 0);
        if (_render(scene, 2.0f, surface)) CHECK(_at(surface, 85u, 40u).b > 64);
        auto reused = Renderer::gen();
        CHECK(reused);
        if (reused) {
            for (auto time : {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 0.25f})
                _retained(reused, scene, time, "effects");
            delete reused;
        }
        CHECK(scene->play(AnimationTarget::from(square).blur(-1.0f), 1.0f) == Result::InvalidArguments);
        CHECK(scene->play(AnimationTarget::from(square).blur(300.0f), 1.0f) == Result::InvalidArguments);
        Shadow broken = {Blue, {NAN, 0.0f}, 0.0f};
        CHECK(scene->play(AnimationTarget::from(square).shadow(broken), 1.0f) == Result::InvalidArguments);
        delete scene;
    }
    {
        // A blurred mask feathers the edge of a spotlight.
        Spotlight spot;
        if (!spot.scene) return;
        spot.hole->blur = 6.0f;
        CHECK(spot.scene->update(spot.hole) == Result::Success);
        CHECK(spot.square->mask(spot.hole) == Result::Success);
        Surface surface;
        if (_render(spot.scene, 0.0f, surface)) {
            CHECK(_at(surface, 80u, 40u).r > 200);
            auto edge = _at(surface, 70u, 40u);
            CHECK(edge.r > 40 && edge.r < 215);
        }
    }
    {
        auto scene = _scene();
        if (!scene) return;
        auto square = _square(0.0f, 0.0f, 2.0f, Red);
        square->blur = 400.0f;
        CHECK(scene->add(square) == Result::InvalidArguments);
        square->blur = 0.0f;
        square->shadow.blur = -1.0f;
        CHECK(scene->add(square) == Result::InvalidArguments);
        delete square;
        delete scene;
    }
}

int main()
{
    isolatedGroups();
    nestedAndAnimatedLayers();
    layerOrder();
    retainedLayers();
    transitionsAndMounts();
    maskModes();
    maskTimeline();
    maskGroups();
    maskRules();
    blendModes();
    effects();
    return report();
}
