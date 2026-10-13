#include "visSaver.h"

namespace vis
{

#ifndef VIS_SAVER_GIF
Result saver::gif(const Scene* scene, SwRenderer* renderer, const char* path,
                  uint32_t) noexcept
{
    if (!scene || !renderer || !path || !path[0]) return Result::InvalidArguments;
    return Result::NonSupport;
}
#endif

#ifndef VIS_SAVER_VIDEO
Result saver::video(const Scene* scene, SwRenderer* renderer, const char* path,
                    uint32_t, const char*) noexcept
{
    if (!scene || !renderer || !path || !path[0]) return Result::InvalidArguments;
    return Result::NonSupport;
}
#endif


}  // namespace vis
