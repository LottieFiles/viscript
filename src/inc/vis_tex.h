#ifndef _VIS_TEX_H_
#define _VIS_TEX_H_

#include "vis.h"

// TeX math typesetting provided by the optional tex module (-Dmodules=tex).
namespace vis::tex
{

// Validates a math subset, up to 16384 source bytes. InvalidArguments reports a
// one-based character position. Macros, definitions, color, hbox and middle are unsupported.
Result check(const char* source, char* error = nullptr, uint32_t errorSize = 0) noexcept;

// How a formula is set.
struct Options
{
    // The font of the glyphs, or nullptr for the font the scene's theme gives role.
    const char* font = nullptr;
    // The theme role whose font, size and color the formula takes unless it sets its own.
    TextRole role = TextRole::Text;
    TextOrientation orientation = TextOrientation::Billboard;
    // Point size: em = size * 4/3, in billboard pixels or plane world units.
    // Zero uses the theme role size, converting plane text at the authored camera.
    float size = 0.0f;
    // Display style, as $$...$$ sets math; false sets it in text style, as inline $...$.
    bool display = true;
    // Where the formula's box, its width by its height and depth, is pinned, as Text pins
    // its box: point in the parent's space, align from the left and from the top.
    Vec3 point;
    Vec2 align = {0.5f, 0.5f};
    // Pins the formula's baseline at point instead, so that it sits on the line of a text
    // there: align.x still places the box along the baseline, and align.y does nothing.
    bool baseline = false;
};

struct Formula;

namespace detail
{
struct Built;
}

// Filled glyph/shape group using scene fonts and theme color. Each explicit part has
// a child group; a single source has direct glyph/shape children. Math letters use italic
// glyphs or a slant fallback; bars, roots, accents and delimiters use filled geometry.
// Stroke, gradient, width, radius and dash properties do not apply.
struct Formula
{
    ~Formula();
    Formula(const Formula&) = delete;
    Formula& operator=(const Formula&) = delete;

    // Sets count parts, TeX math read one after another as one formula, a blank between
    // each. nullptr when one does not parse, the scene cannot measure the font, or memory
    // runs out, with why in error, such as "part 2: unknown command '\foo' at 3".
    // Register fonts before layout and keep their registering renderer alive while using
    // measured geometry. Inline spans own point/align/orientation/baseline of their TeX parts.
    static Formula* gen(Scene* scene, const char* const* parts, uint32_t count,
                        const Options& options = {}, char* error = nullptr,
                        uint32_t errorSize = 0) noexcept;
    // Sets one source, whose glyphs and shapes lie right below the group.
    static Formula* gen(Scene* scene, const char* source, const Options& options = {},
                        char* error = nullptr, uint32_t errorSize = 0) noexcept;

    // The group, which the formula deletes with itself until take() hands it over to be
    // attached with Scene::add or Object::add.
    Group* root() const noexcept;
    Group* take() noexcept;
    // The groups of the parts, none for a formula set from one source.
    uint32_t parts() const noexcept;
    Group* part(uint32_t index) const noexcept;
    // The TeX of a part, or of the whole formula, without the blanks TeX ignores, such as
    // "c^2"; nullptr past the last part.
    const char* source(uint32_t index) const noexcept;
    // What the formula draws, in the order of its TeX: each glyph, bar and shape, and the
    // TeX it comes from without blanks, such as "x", "\frac" for a bar or "\left(".
    uint32_t pieces() const noexcept;
    Object* piece(uint32_t index) const noexcept;
    const char* key(uint32_t index) const noexcept;
    // The part a piece belongs to; zero for a formula set from one source.
    uint32_t partOf(uint32_t index) const noexcept;

private:
    Formula() noexcept = default;
    detail::Built* built = nullptr;
    friend uint32_t pair(const Formula& source, const Formula& target, Object** pairs,
                         uint32_t capacity) noexcept;
};

// Pairs equal parts, then equal token keys in ordered runs, then remaining equal keys.
// Writes alternating source/target pieces to 2 * capacity slots and returns the pair count.
// Unpaired pieces fade in Scene::matchTransform(); explicit parts/pairs resolve duplicates.
uint32_t pair(const Formula& source, const Formula& target, Object** pairs,
              uint32_t capacity) noexcept;

struct Lua
{
    // Runs a Lua scene script with the tex module's factories, as vis::Lua::load() does.
    static Result load(const char* source, uint32_t size, const char* name, Scene** scene,
                       char* error, uint32_t errorSize, vis::Lua::AssetResolver resolver = nullptr,
                       void* resolverData = nullptr, const Theme* adaptiveTheme = nullptr,
                       bool* adaptiveThemeUsed = nullptr) noexcept;
};

}  // namespace vis::tex

#endif
