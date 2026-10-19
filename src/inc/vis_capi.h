// The C API of the engine: the C++ API behind opaque handles and integer results, for hosts
// without C++.
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

VIS_API VisEngine vis_create(void);
VIS_API void vis_destroy(VisEngine engine);
VIS_API VisResult vis_load_lua(VisEngine engine, const char* source, uint32_t size, const char* name);
VIS_API VisResult vis_load_font(VisEngine engine, const char* name, const uint8_t* data, uint32_t size,
                                      const char* mime);
/* Fonts the loaded scene's text needs that are not loaded, one per line: the first font a text
   names, and a later one of its comma list when the fonts before it lack some of its characters.
   Engine-owned; valid until the next call or the engine is destroyed. */
VIS_API const char* vis_missing_fonts(VisEngine engine);
VIS_API VisResult vis_load_asset(VisEngine engine, const char* name, const uint8_t* data, uint32_t size,
                                       const char* mime);
/* Host colors, as 0xRRGGBBAA, for the scenes loaded after it that use the adaptive_vscode theme;
   dark picks the pro_black base over pro_white, and objects holds 1 to 10 object colors. */
VIS_API VisResult vis_host_theme(VisEngine engine, uint8_t dark, uint32_t background, uint32_t foreground,
                                 uint32_t muted, uint32_t accent, uint32_t secondary, uint32_t success,
                                 uint32_t warning, uint32_t danger, uint32_t info, uint32_t surface,
                                 uint32_t line, uint32_t result, uint32_t focus, const uint32_t* objects,
                                 uint32_t object_count);
/* 1 when the loaded scene uses the adaptive_vscode theme, and so the host colors; else 0. */
VIS_API uint8_t vis_adaptive_theme(VisEngine engine);
/* Renders the frame at time, with pixel_ratio (1 or more) pixels for each scene pixel along each
   axis. */
VIS_API VisResult vis_render(VisEngine engine, float time, uint32_t pixel_ratio);
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
/* The RGBA pixels of the last render; the pointer expires on engine mutation/destruction. */
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
