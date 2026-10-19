#ifndef _VIS_TEST_GL_CONTEXT_H_
#define _VIS_TEST_GL_CONTEXT_H_

#include <cstdint>
#include <dlfcn.h>

#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

// Offscreen macOS CGL context with an RGBA8 + depth/stencil FBO for GL tests.
struct GlTestContext
{
    CGLContextObj context = nullptr;
    void* library = nullptr;
    GLuint fbo = 0;
    GLuint color = 0;
    GLuint depth = 0;
    uint32_t width = 160;
    uint32_t height = 90;
    PFNGLGENFRAMEBUFFERSPROC genFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC bindFramebuffer = nullptr;
    PFNGLGENRENDERBUFFERSPROC genRenderbuffers = nullptr;
    PFNGLBINDRENDERBUFFERPROC bindRenderbuffer = nullptr;
    PFNGLRENDERBUFFERSTORAGEPROC renderbufferStorage = nullptr;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC framebufferRenderbuffer = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebufferStatus = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers = nullptr;
    PFNGLDELETERENDERBUFFERSPROC deleteRenderbuffers = nullptr;
    PFNGLREADPIXELSPROC readPixels = nullptr;

    template<typename T>
    bool resolve(T& function, const char* name)
    {
        function = reinterpret_cast<T>(dlsym(library, name));
        return function;
    }

    bool init()
    {
        // ThorVG owns same-named loader symbols, so keep test entry points under local aliases.
        library = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
        if (!library) library = dlopen("/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
        if (!library || !resolve(genFramebuffers, "glGenFramebuffers")
            || !resolve(bindFramebuffer, "glBindFramebuffer")
            || !resolve(genRenderbuffers, "glGenRenderbuffers")
            || !resolve(bindRenderbuffer, "glBindRenderbuffer")
            || !resolve(renderbufferStorage, "glRenderbufferStorage")
            || !resolve(framebufferRenderbuffer, "glFramebufferRenderbuffer")
            || !resolve(checkFramebufferStatus, "glCheckFramebufferStatus")
            || !resolve(deleteFramebuffers, "glDeleteFramebuffers")
            || !resolve(deleteRenderbuffers, "glDeleteRenderbuffers")
            || !resolve(readPixels, "glReadPixels")) {
            return false;
        }
        CGLPixelFormatAttribute attributes[] = {
            kCGLPFAOpenGLProfile, static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_GL3_Core),
            kCGLPFAColorSize, static_cast<CGLPixelFormatAttribute>(32),
            kCGLPFADepthSize, static_cast<CGLPixelFormatAttribute>(24),
            kCGLPFAStencilSize, static_cast<CGLPixelFormatAttribute>(8),
            static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format = nullptr;
        GLint count = 0;
        if (CGLChoosePixelFormat(attributes, &format, &count) != kCGLNoError || !format || !count) return false;
        auto result = CGLCreateContext(format, nullptr, &context);
        CGLDestroyPixelFormat(format);
        if (result != kCGLNoError || !context || CGLSetCurrentContext(context) != kCGLNoError) return false;

        genFramebuffers(1, &fbo);
        bindFramebuffer(GL_FRAMEBUFFER, fbo);
        genRenderbuffers(1, &color);
        bindRenderbuffer(GL_RENDERBUFFER, color);
        renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
        genRenderbuffers(1, &depth);
        bindRenderbuffer(GL_RENDERBUFFER, depth);
        renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
        auto complete = checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        bindFramebuffer(GL_FRAMEBUFFER, 0);
        return complete;
    }

    ~GlTestContext()
    {
        if (context) {
            CGLSetCurrentContext(context);
            if (deleteFramebuffers && fbo) deleteFramebuffers(1, &fbo);
            if (deleteRenderbuffers && color) deleteRenderbuffers(1, &color);
            if (deleteRenderbuffers && depth) deleteRenderbuffers(1, &depth);
            CGLSetCurrentContext(nullptr);
            CGLDestroyContext(context);
        }
        if (library) dlclose(library);
    }
};

#endif
