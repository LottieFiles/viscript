#ifndef _VIS_TEX_SCENE_H_
#define _VIS_TEX_SCENE_H_

#include "vis_tex.h"

namespace vis::tex::detail
{

// One thing a formula draws, a glyph, bar or shape: the part it belongs to and the TeX it
// comes from without blanks, the key that pairs it in a match transform.
struct Piece
{
    Object* object = nullptr;
    char* key = nullptr;
    uint32_t part = 0;
};

// A formula read and set in ems, which prepare() makes and draw() places.
struct Laid;

// A formula set in the fonts of a scene, which deletes what it still owns with itself.
struct Built
{
    // The formula prepare() set, which draw() places and then frees.
    Laid* laid = nullptr;
    // The box of the prepared formula in world units: its width, its height above the baseline
    // and depth below it, and the ascent and descent of the line box of its font.
    float width = 0.0f;
    float height = 0.0f;
    float depth = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    Group* root = nullptr;
    // The groups of a formula set from parts, and the sources of the parts without blanks;
    // a formula set from one source has one source and no part groups.
    Group** parts = nullptr;
    char** sources = nullptr;
    uint32_t partCount = 0;
    uint32_t sourceCount = 0;
    // In the order of the TeX, which is the order of the tree.
    Piece* pieces = nullptr;
    uint32_t pieceCount = 0;
    // Whether the build still owns root, and each part group not below root.
    bool ownsRoot = false;
    bool* ownsPart = nullptr;

    Built() = default;
    Built(const Built&) = delete;
    Built& operator=(const Built&) = delete;
    ~Built();
};

// Sets count sources as one formula in the fonts and theme of scene, as build() does, but
// draws nothing yet: the box of output tells how large it is, so that it can be placed.
bool prepare(Scene* scene, const char* const* sources, uint32_t count, bool split, const Options& options,
             Built& output, char* error, uint32_t errorSize) noexcept;

// Draws the formula output prepared with the left end of its baseline at origin, in the
// parent's space. detached leaves the part groups out of root, so that each can be styled and
// attached below it on its own.
bool draw(Built& output, const Vec3& origin, bool detached, char* error, uint32_t errorSize) noexcept;

// The left end of the baseline of the formula output prepared, where the point and align of
// options pin its box, or its baseline.
Vec3 origin(const Built& output, const Options& options) noexcept;

// Sets count sources as one formula in the fonts and theme of scene and draws it where the
// point and align of options pin its box. split sets them as parts, one group each.
bool build(Scene* scene, const char* const* sources, uint32_t count, bool split, const Options& options,
           bool detached, Built& output, char* error, uint32_t errorSize) noexcept;

// The TeX of source from begin to end without the blanks TeX ignores, keeping one after a
// control word that a letter follows, written to output, which has room for end - begin + 1.
void key(const char* source, uint32_t begin, uint32_t end, char* output) noexcept;

// The pieces of one side of a match: pieces in order with their part, and the part sources.
struct Side
{
    const Piece* pieces = nullptr;
    uint32_t count = 0;
    const char* const* sources = nullptr;
    uint32_t sourceCount = 0;
};

// Completes partners, the target piece of each source piece or UINT32_MAX, as pair() pairs
// them, keeping the pairs already in it. False when out of memory.
bool match(const Side& source, const Side& target, uint32_t* partners) noexcept;

}  // namespace vis::tex::detail

#endif
