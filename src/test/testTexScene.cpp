#include <cmath>
#include <cstdio>
#include <cstring>

#include "vis.h"
#include "vis_tex.h"
#include "testCheck.h"

using namespace vis;

// A formula set from parts: a group per part below its typeset group, its pieces keyed by
// their TeX, and the pairs it gives a match transform.
static void _parts(Scene* scene)
{
    const char* parts[] = {"a^2", "+", "b ^ 2"};
    char error[256] = {};
    auto formula = tex::Formula::gen(scene, parts, 3u, {}, error, sizeof(error));
    CHECK(formula != nullptr);
    if (!formula) std::fprintf(stderr, "formula: %s\n", error);
    auto other = tex::Formula::gen(scene, "b^2+a^2", {}, error, sizeof(error));
    CHECK(other != nullptr);
    if (formula && other) {
        CHECK(formula->root()->typeset && formula->parts() == 3u && formula->pieces() == 5u);
        CHECK(other->parts() == 0u && other->pieces() == 5u);
        CHECK(std::strcmp(formula->source(2), "b^2") == 0 && !formula->source(3));
        CHECK(std::strcmp(formula->key(0), "a") == 0 && formula->partOf(4) == 2u);
        CHECK(formula->piece(4)->parent() == formula->part(2) && formula->part(2)->parent() == formula->root());
        CHECK(other->piece(0)->parent() == other->root() && std::strcmp(other->source(0), "b^2+a^2") == 0);
        Object* pairs[10] = {};
        CHECK(tex::pair(*formula, *other, pairs, 5u) == 5u);
        // "2+2" is the longest run both keep, and a then pairs with a wherever it stands.
        CHECK(pairs[0] == formula->piece(0) && pairs[1] == other->piece(3));
        CHECK(pairs[2] == formula->piece(1) && pairs[3] == other->piece(1));
        CHECK(pairs[4] == formula->piece(2) && pairs[5] == other->piece(2));
        CHECK(tex::pair(*formula, *other, nullptr, 2u) == 2u);
        CHECK(scene->add(formula->take()) == Result::Success);
    }
    delete formula;
    delete other;
}

// Glyphs are texts in the scene's font, math italic or leaning where the font has none;
// bars, roots, stretched delimiters and accents are filled shapes; the box is pinned to the
// point by align.
static void _pieces(Scene* scene)
{
    tex::Options options;
    options.point = {1.0f, 0.5f, 0.0f};
    options.role = TextRole::H2;
    char error[256] = {};
    auto formula = tex::Formula::gen(scene, "\\frac{a}{\\sqrt{x}} \\left( \\frac{1}{2} \\right) \\hat{y}", options,
                                     error, sizeof(error));
    CHECK(formula != nullptr);
    if (!formula) {
        std::fprintf(stderr, "formula: %s\n", error);
        return;
    }
    const char* keys[] = {"\\frac", "a", "\\sqrt", "\\sqrt", "x", "\\left(", "\\frac", "1", "2", "\\right)", "\\hat", "y"};
    const Type types[] = {Type::Rectangle, Type::Text, Type::Path, Type::Rectangle, Type::Text, Type::Path,
                          Type::Rectangle, Type::Text, Type::Text, Type::Path, Type::Path, Type::Text};
    CHECK(formula->pieces() == 12u);
    for (auto i = 0u; i < formula->pieces() && i < 12u; i++) {
        CHECK(std::strcmp(formula->key(i), keys[i]) == 0 && formula->piece(i)->type() == types[i]);
        if (types[i] != Type::Text) CHECK(formula->piece(i)->style.stroke.a == 0u);
    }
    auto a = static_cast<const Text*>(formula->piece(1));
    auto one = static_cast<const Text*>(formula->piece(7));
    CHECK(std::strcmp(a->text(), "a") == 0 && a->slant > 0.1f && a->role == TextRole::H2);
    CHECK(std::strcmp(one->text(), "1") == 0 && one->slant == 0.0f);
    auto& color = scene->theme().h2.color;
    auto& fill = formula->root()->style.fill;
    CHECK(fill.r == color.r && fill.g == color.g && fill.b == color.b && fill.a == color.a);
    CHECK(scene->add(formula->take()) == Result::Success);
    Bounds bounds;
    CHECK(formula->root()->bounds(bounds));
    CHECK(std::fabs(bounds.center().x - 1.0f) < 0.05f && std::fabs(bounds.center().y - 0.5f) < 0.05f);
    CHECK(bounds.size().x > 1.0f && bounds.size().y > 0.2f);
    delete formula;
}

// Every glyph's box is the box its ink covers as painted, leaning letters included, so bounds
// frame what the reader sees; shapes keep their outlines. The glyphs paint where the layout
// sets them, whatever their boxes.
static void _inked(Renderer* renderer)
{
    Config config;
    config.width = 800;
    config.height = 400;
    config.camera.orthoHeight = 400.0f;
    auto scene = Scene::gen(config);
    tex::Options options;
    options.point = {0.0f, 60.0f, 0.0f};
    options.size = 40.0f;
    char error[256] = {};
    auto formula = scene ? tex::Formula::gen(scene, "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a} + \\sum_{n} f", options,
                                             error, sizeof(error))
                         : nullptr;
    CHECK(formula != nullptr);
    if (!formula) {
        std::fprintf(stderr, "formula: %s\n", error);
        delete scene;
        return;
    }
    CHECK(scene->add(formula->take()) == Result::Success);
    auto leaning = 0u;
    auto checked = 0u;
    for (auto i = 0u; i < formula->pieces(); i++) {
        auto piece = formula->piece(i);
        if (piece->type() != Type::Text) continue;
        auto glyph = static_cast<const Text*>(piece);
        Bounds bounds;
        BBox painted;
        CHECK(piece->bounds(bounds));
        CHECK(renderer->bounds(scene, piece, 0.0f, painted) == Result::Success);
        // One world unit is one pixel, from the middle of the frame, y down.
        auto left = bounds.min.x + 400.0f;
        auto right = bounds.max.x + 400.0f;
        auto top = 200.0f - bounds.max.y;
        auto bottom = 200.0f - bounds.min.y;
        auto fits = std::fabs(left - painted.x) < 0.75f && std::fabs(right - painted.x - painted.width) < 0.75f
                    && std::fabs(top - painted.y) < 0.75f && std::fabs(bottom - painted.y - painted.height) < 0.75f;
        CHECK(fits);
        if (!fits) {
            std::fprintf(stderr, "  %s: box %.2f %.2f %.2f %.2f, painted %.2f %.2f %.2f %.2f\n", glyph->text(), left,
                         right, top, bottom, painted.x, painted.x + painted.width, painted.y,
                         painted.y + painted.height);
        }
        if (glyph->slant != 0.0f) leaning++;
        checked++;
    }
    // x, b, b, a, c, a, n and f lean in a font without math italics.
    CHECK(checked == 17u && leaning == 8u);
    delete formula;
    delete scene;
}

// Where the baseline of a glyph text lies: its point lies the font's ascent less align lines
// above the baseline.
static float _glyphBaseline(Scene* scene, const Object* piece)
{
    auto glyph = static_cast<const Text*>(piece);
    TextExtent extent;
    CHECK(piece->type() == Type::Text && scene->measure(glyph, extent) == Result::Success);
    return glyph->point.y - extent.ascent + glyph->align.y * extent.height;
}

// Options::baseline pins the formula's baseline at the point, whatever align.y says, while
// align.x still places it along the line.
static void _baseline(Scene* scene)
{
    tex::Options options;
    options.point = {2.0f, -0.75f, 0.0f};
    options.align = {1.0f, 0.0f};
    options.baseline = true;
    char error[256] = {};
    auto pinned = tex::Formula::gen(scene, "y^2", options, error, sizeof(error));
    options.baseline = false;
    auto boxed = tex::Formula::gen(scene, "y^2", options, error, sizeof(error));
    CHECK(pinned && boxed);
    if (pinned && boxed) {
        CHECK(std::fabs(_glyphBaseline(scene, pinned->piece(0)) + 0.75f) < 1.0e-4f);
        // align.y = 0 hangs the box from the point instead, its baseline lower by its height.
        CHECK(_glyphBaseline(scene, boxed->piece(0)) < -0.8f);
        auto y = static_cast<const Text*>(pinned->piece(0));
        auto other = static_cast<const Text*>(boxed->piece(0));
        CHECK(y->point.x == other->point.x);
    }
    delete pinned;
    delete boxed;
}

static void _errors(Scene* scene)
{
    char error[256] = {};
    CHECK(!tex::Formula::gen(scene, "\\frac{", {}, error, sizeof(error)) && std::strlen(error) > 0u);
    const char* broken[] = {"a", "\\foo"};
    CHECK(!tex::Formula::gen(scene, broken, 2u, {}, error, sizeof(error)));
    CHECK(std::strstr(error, "part 2: unknown command '\\foo' at 1") != nullptr);
    CHECK(!tex::Formula::gen(nullptr, "x", {}, error, sizeof(error)));
}

int main()
{
    auto renderer = Renderer::gen();
    auto scene = Scene::gen();
    CHECK(renderer && scene);
    if (renderer && scene) {
        CHECK(renderer->font(VIS_TEST_FONT) == Result::Success);
        CHECK(renderer->font(VIS_TEST_MONO_FONT) == Result::Success);
        _parts(scene);
        _pieces(scene);
        _baseline(scene);
        _errors(scene);
        _inked(renderer);
    }
    delete scene;
    delete renderer;
    return report();
}
