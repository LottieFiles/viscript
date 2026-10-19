// The Lua memory and path limits that the binding and the modules share.
#ifndef _VIS_LUA_BUDGET_H_
#define _VIS_LUA_BUDGET_H_

#include <cstddef>

namespace vis
{

static constexpr size_t LUA_MEMORY_LIMIT = 64u * 1024u * 1024u;
static constexpr size_t LUA_PATH_LIMIT = 4096;

}  // namespace vis

#endif
