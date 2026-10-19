// How many frames Saver::save encodes from a scene's timeline, for the CLI's soundtrack and
// the audit to match.
#ifndef _VIS_SAVER_H_
#define _VIS_SAVER_H_

#include "vis.h"

namespace vis::saver
{

Result timeline(const Scene* scene, uint32_t& fps, uint32_t& frames) noexcept;

}  // namespace vis::saver

#endif
