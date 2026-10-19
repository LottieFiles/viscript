// The limits and storage of morphs (see visMorph.cpp).
#ifndef _VIS_MORPH_H_
#define _VIS_MORPH_H_

#include "visScene.h"

namespace vis
{

// Points the morphs of one scene keep in all.
static constexpr uint32_t MAX_MORPH_POINTS = 1048576;
static constexpr uint32_t MAX_MORPH_COLOR_STEPS = 64;

static inline void _freeMorph(MorphShape& shape)
{
    delete[] shape.from;
    delete[] shape.to;
    delete[] shape.ends;
    delete[] shape.growth;
    delete[] shape.colors[0];
    delete[] shape.colors[1];
    shape = {};
}

// Piece indices follow the retained path, plot or warp, before pairing reorders them.
static inline uint32_t _morphPieceCount(const Object* object)
{
    if (auto warp = warped(object)) return warp->runCount;
    if (object->type() == Type::Path) return static_cast<const Path*>(object)->contours();
    if (object->type() == Type::Plot) return static_cast<const Plot*>(object)->contours();
    return 1u;
}

Result pairMorph(const Object* source, const Object* target, MorphShape& shape);
Result pairMorphPieces(const Animation& animation, MorphShape& shape);
bool validMorphPairs(const Animation& animation);

}  // namespace vis

#endif
