// The JSON a layout report is written as, which the WebAssembly binding hands JavaScript and
// the command line's layout command prints, and the path the report gives a scene, which the
// audit rebuilds to find it.
#ifndef _VIS_LAYOUT_JSON_H_
#define _VIS_LAYOUT_JSON_H_

#include "vis.h"
#include "../common/visJson.h"

namespace vis
{

// The names the JSON gives a scene's and a visual's kind, such as "viewport" and "fade-in".
const char* layoutSceneKind(LayoutSceneKind kind) noexcept;
const char* layoutVisualKind(LayoutVisualKind kind) noexcept;

// The path of the scene of kind at index under the scene at parent: parent, then the name of
// kind and index, as in "root/viewport:0", in new storage, or nullptr.
char* layoutScenePath(const char* parent, LayoutSceneKind kind, uint32_t index) noexcept;

// Appends report, taken of root, as one object: its time, padding and root's size, then its
// scenes, objects, visuals, paths, collisions and containments, each list in the report's order
// so that the indices in its records point into it. Numbers have 9 significant digits, which
// keep a float exact. False when memory runs out.
bool layoutJson(const LayoutReport& report, const Scene* root, JsonBuffer& json) noexcept;

}  // namespace vis

#endif
