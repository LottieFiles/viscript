#include "visSaver.h"

namespace vis::saver
{

Result video(const Scene* scene, SwRenderer* renderer, const char* path, uint32_t fps,
             const char* soundtrack) noexcept
{
    static constexpr auto SILENT = "-an -c:v libx264 -pix_fmt yuv420p -f mp4";
    static constexpr auto SOUND = "-c:v libx264 -pix_fmt yuv420p -c:a aac -b:a 192k -f mp4";
    return frames(scene, renderer, path, fps, soundtrack ? SOUND : SILENT, true, soundtrack);
}

}  // namespace vis::saver
