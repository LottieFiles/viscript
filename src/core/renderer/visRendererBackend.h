// Text measurement the scene asks the renderer for, and the font lists both read.
#ifndef _VIS_RENDERER_BACKEND_H_
#define _VIS_RENDERER_BACKEND_H_

#include <cstdint>

#include "vis.h"

namespace vis
{

namespace renderer
{

// The next name of the font list at reads, a comma list of font names (see Text::font()): the
// text up to the next comma without the spaces around it. Moves at past that comma; false
// once the list holds no more names.
static inline bool fontName(const char*& at, const char*& name, uint32_t& length) noexcept
{
    while (*at) {
        name = at;
        while (*at && *at != ',') at++;
        auto end = at;
        if (*at) at++;
        while (name < end && *name == ' ') name++;
        while (end > name && end[-1] == ' ') end--;
        length = static_cast<uint32_t>(end - name);
        if (length) return true;
    }
    return false;
}

// Sets text in a font that a renderer loaded and reports the room it takes, in the units of
// size, its outlines leaning by slant (see Text::measure()).
Result measureText(const char* font, float size, const char* text, float slant, bool tabular, TextExtent& output) noexcept;

// The fonts of the list font that are not loaded but that text needs, a bit for each in the
// order the list names them: the first, and a later one when some character of text has a
// glyph in no loaded font the list names before it.
uint32_t missingFonts(const char* font, const char* text) noexcept;

}  // namespace renderer

}  // namespace vis

#endif
