// Saves a GIF through ffmpeg with a generated palette (see visSaver.h).
#include "visSaver.h"

namespace vis::saver
{

Result gif(const Scene* scene, Renderer* renderer, const char* path, uint32_t fps) noexcept
{
    static constexpr auto OPTIONS =
        "-filter_complex '[0:v]split[a][b];[a]palettegen[p];[b][p]paletteuse' -f gif";
    return frames(scene, renderer, path, fps, OPTIONS, false);
}

}  // namespace vis::saver
