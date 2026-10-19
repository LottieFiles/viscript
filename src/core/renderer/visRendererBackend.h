// Text measurement the scene asks the renderer for.
#ifndef _VIS_RENDERER_BACKEND_H_
#define _VIS_RENDERER_BACKEND_H_

#include "vis.h"

namespace vis
{

namespace renderer
{

// Sets text in a font that a renderer loaded and reports the room it takes, in the units of
// size, its outlines leaning by slant (see Text::measure()).
Result measureText(const char* font, float size, const char* text, float slant, bool tabular, TextExtent& output) noexcept;

}  // namespace renderer

}  // namespace vis

#endif
