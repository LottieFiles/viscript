// The pieces of a match transform and how they pair (see visMatch.cpp).
#ifndef _VIS_MATCH_H_
#define _VIS_MATCH_H_

#include <cstring>

#include "visScene.h"

namespace vis
{

static constexpr uint32_t MAX_ANIMATION_GROUP = 4096;

// One piece of a match, with the clip it plays.
struct MatchPiece
{
    Object* object = nullptr;
    ObjectEntry* entry = nullptr;
    // The index of the paired piece, or UINT32_MAX.
    uint32_t partner = UINT32_MAX;
    // An unpaired piece cross-fading at the same authored anchor.
    uint32_t contact = UINT32_MAX;
    float contactBegin = 0.0f;
    float contactEnd = 0.0f;
    // Paired texts that read the same, and paired shapes of one kind, travel as one piece;
    // other pairs cross-fade.
    bool same = false;
    // One of the pieces after the first that pair with one piece of the other object: it fades
    // in from that piece's place and look, or out into them, while the first travels as one
    // with it.
    bool extra = false;
    // For a diff: pieces share a key only when they are texts that read the same.
    uint32_t key = 0;
    VisualState from;
    VisualState to;
    uint32_t channels = 0;
    Mat4 pathSpace;
};

// Groups open into their children at any depth; every other object is one piece.
static inline uint32_t _pieces(Object* object, MatchPiece* output, uint32_t count = 0u)
{
    if (object->type() != Type::Group) {
        if (output) output[count].object = object;
        return count + 1u;
    }
    for (auto i = 0u; i < object->childCount(); i++) count = _pieces(object->childAt(i), output, count);
    return count;
}

static inline bool _sameText(const Object* first, const Object* second)
{
    if (first->type() != Type::Text || second->type() != Type::Text) return false;
    auto a = static_cast<const Text*>(first)->text();
    auto b = static_cast<const Text*>(second)->text();
    return a && b && !strcmp(a, b);
}

Result diffMatch(const Object* source, const Object* target, MatchPiece* pieces,
                 uint32_t sourceCount, uint32_t total);
bool pairShapes(MatchPiece* pieces, uint32_t sourceCount, uint32_t total);

}  // namespace vis

#endif
