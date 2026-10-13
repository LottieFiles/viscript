#ifndef _VIS_RENDERER_BACKEND_H_
#define _VIS_RENDERER_BACKEND_H_

#include "vis.h"

namespace vis
{

struct SwRenderer;
struct GlRenderer;

namespace renderer
{

bool cpuEnabled() noexcept;
bool glEnabled() noexcept;
SwRenderer* genCpu() noexcept;
GlRenderer* genGl() noexcept;
// Sets text in a font that a renderer loaded and reports the room it takes, in the units of
// size, its outlines leaning by slant (see Text::measure()).
Result measureText(const char* font, float size, const char* text, float slant, bool tabular, TextExtent& output) noexcept;

}  // namespace renderer

}  // namespace vis

#endif
