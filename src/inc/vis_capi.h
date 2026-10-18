#ifndef _VIS_CAPI_H_
#define _VIS_CAPI_H_

#include <stdint.h>

#if defined(__GNUC__) && !defined(_WIN32)
#define VIS_API __attribute__((visibility("default")))
#else
#define VIS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* VisEngine;
typedef int32_t VisResult;

enum
{
    VIS_RESULT_SUCCESS = 0,
    VIS_RESULT_INVALID_ARGUMENTS = 1,
    VIS_RESULT_INSUFFICIENT_CONDITION = 2,
    VIS_RESULT_NON_SUPPORT = 3,
    VIS_RESULT_OUT_OF_MEMORY = 4,
    VIS_RESULT_IO_ERROR = 5,
    VIS_RESULT_SCRIPT_ERROR = 6,
    VIS_RESULT_UNKNOWN = 7
};

/* Integer ABI permits unknown engine values to reach boundary validation. */
typedef int32_t VisRenderEngine;

enum
{
    VIS_RENDER_ENGINE_CPU = 0,
    VIS_RENDER_ENGINE_GL = 1
};

typedef enum VisCameraMode
{
    VIS_CAMERA_FIXED = 0,
    VIS_CAMERA_INTERACTIVE = 1
} VisCameraMode;

typedef enum VisCameraView
{
    VIS_CAMERA_VIEW_2D = 0,
    VIS_CAMERA_VIEW_3D = 1
} VisCameraView;

typedef enum VisCameraAction
{
    VIS_CAMERA_PAN = 0,
    VIS_CAMERA_ORBIT = 1,
    VIS_CAMERA_ZOOM = 2,
    VIS_CAMERA_RESET = 3,
    VIS_CAMERA_SET_2D = 4,
    VIS_CAMERA_SET_3D = 5
} VisCameraAction;

typedef struct VisBBox
{
    float x;
    float y;
    float width;
    float height;
} VisBBox;

typedef struct VisGlTarget
{
    void* display;
    void* surface;
    void* context;
    int32_t id;
    uint32_t width;
    uint32_t height;
} VisGlTarget;

VIS_API VisEngine vis_create(void);
VIS_API VisEngine vis_create_with_engine(VisRenderEngine render_engine);
VIS_API VisResult vis_default_render_engine_set(VisRenderEngine render_engine);
VIS_API VisRenderEngine vis_default_render_engine(void);
VIS_API uint8_t vis_render_engine_enabled(VisRenderEngine render_engine);
VIS_API VisRenderEngine vis_render_engine(VisEngine engine);
VIS_API void vis_destroy(VisEngine engine);
VIS_API VisResult vis_load_lua(VisEngine engine, const char* source, uint32_t size, const char* name);
VIS_API VisResult vis_load_font(VisEngine engine, const char* name, const uint8_t* data, uint32_t size,
                                      const char* mime);
/* Fonts the loaded scene's text is set in that are not loaded, one per line; the default font
   draws that text. Engine-owned; valid until the next call or the engine is destroyed. */
VIS_API const char* vis_missing_fonts(VisEngine engine);
VIS_API VisResult vis_load_asset(VisEngine engine, const char* name, const uint8_t* data, uint32_t size,
                                       const char* mime);
VIS_API VisResult vis_render(VisEngine engine, float time);
VIS_API VisResult vis_render_scaled(VisEngine engine, float time, uint32_t pixel_ratio);
// Experimental: needs a build with the GL engine (-Dengines=cpu,gl).
VIS_API VisResult vis_render_gl(VisEngine engine, float time, const VisGlTarget* target);
VIS_API VisResult vis_bounds(VisEngine engine, const char* object, float time, VisBBox* output);
VIS_API VisResult vis_intersects(VisEngine engine, const char* first, const char* second, float time,
                                       float padding, uint8_t* output);
VIS_API VisResult vis_layout_report(VisEngine engine, float time, float padding);
/* Engine-owned JSON bytes; valid until the report is replaced or the engine is destroyed. */
VIS_API const uint8_t* vis_layout_report_data(VisEngine engine);
VIS_API uint32_t vis_layout_report_size(VisEngine engine);
VIS_API VisResult vis_resize(VisEngine engine, uint32_t width, uint32_t height);
VIS_API VisResult vis_camera(VisEngine engine, VisCameraAction action, float x, float y);
VIS_API VisCameraMode vis_camera_mode(VisEngine engine);
VIS_API VisCameraView vis_camera_view(VisEngine engine);
/* RGBA bridge requires the CPU engine; pointer expires on engine mutation/destruction. */
VIS_API const uint8_t* vis_pixels(VisEngine engine);
VIS_API uint32_t vis_pixels_size(VisEngine engine);
VIS_API uint32_t vis_width(VisEngine engine);
VIS_API uint32_t vis_height(VisEngine engine);
VIS_API float vis_duration(VisEngine engine);
VIS_API uint8_t vis_loop(VisEngine engine);
VIS_API const char* vis_last_error(VisEngine engine);

#ifdef __cplusplus
}
#endif

#endif
