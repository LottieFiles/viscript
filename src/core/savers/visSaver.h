// The frame pipeline to ffmpeg that the GIF and video savers share.
#ifndef _VIS_SAVER_H_
#define _VIS_SAVER_H_

#include "vis.h"

namespace vis::saver
{

bool ends(const char* value, const char* suffix) noexcept;
Result timeline(const Scene* scene, uint32_t& fps, uint32_t& frames) noexcept;
// Pipes the frames to ffmpeg with the output options. A soundtrack becomes the second input,
// read for as long as the video lasts, and options must then encode its audio.
Result frames(const Scene* scene, Renderer* renderer, const char* path, uint32_t fps,
              const char* options, bool evenDimensions, const char* soundtrack = nullptr) noexcept;
Result gif(const Scene* scene, Renderer* renderer, const char* path, uint32_t fps) noexcept;
Result video(const Scene* scene, Renderer* renderer, const char* path, uint32_t fps,
             const char* soundtrack = nullptr) noexcept;

}  // namespace vis::saver

#endif
