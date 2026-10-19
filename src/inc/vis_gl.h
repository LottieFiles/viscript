// The experimental OpenGL renderer module: draws a scene through ThorVG's GL engine into a
// framebuffer of the host's GL context. Built with -Dgl=true as libvis-gl, for native hosts
// only; WebAssembly builds render on the CPU. The host owns the context and makes it current
// before every call.
#ifndef _VIS_GL_H_
#define _VIS_GL_H_

#include "vis.h"

namespace vis::gl
{

// Where a frame is drawn: the current GL context and the framebuffer id to draw into (0 for
// the default one), at the scene's size. display and surface are the EGL display and surface
// of an EGL context, and nullptr otherwise.
struct Target
{
    void* display = nullptr;
    void* surface = nullptr;
    void* context = nullptr;
    int32_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct Renderer
{
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // nullptr when ThorVG's GL engine is not available in this process.
    static Renderer* gen() noexcept;
    Result font(const char* path) noexcept;
    Result font(const char* name, const void* data, uint32_t size, const char* mime = "ttf") noexcept;
    // Why the most recent font or render call failed, in plain words; nullptr when the
    // renderer has no reason.
    const char* diagnostic() const noexcept;
    // Draws the scene at time into the target; InvalidArguments when the target has no
    // context or does not match the scene's size, NonSupport when the engine cannot use it.
    Result render(const Scene* scene, float time, const Target& target) noexcept;

private:
    struct Impl;
    Impl* pImpl = nullptr;
    Renderer() noexcept;
};

}  // namespace vis::gl

#endif
