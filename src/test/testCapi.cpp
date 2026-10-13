#include <cstdio>
#include <cstring>

#include "vis_capi.h"

static auto failures = 0;

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                                        \
        }                                                                                      \
    } while (false)

int main()
{
    CHECK(vis_render_engine_enabled(VIS_RENDER_ENGINE_CPU) == 1u);
    CHECK(vis_render_engine_enabled(static_cast<VisRenderEngine>(42)) == 0u);
    CHECK(vis_default_render_engine_set(static_cast<VisRenderEngine>(42))
          == VIS_RESULT_INVALID_ARGUMENTS);
    CHECK(vis_default_render_engine_set(VIS_RENDER_ENGINE_CPU) == VIS_RESULT_SUCCESS);
    CHECK(vis_default_render_engine() == VIS_RENDER_ENGINE_CPU);
    auto explicitCpu = vis_create_with_engine(VIS_RENDER_ENGINE_CPU);
    CHECK(explicitCpu != nullptr);
    CHECK(vis_render_engine(explicitCpu) == VIS_RENDER_ENGINE_CPU);
    vis_destroy(explicitCpu);
    CHECK(vis_create_with_engine(static_cast<VisRenderEngine>(42)) == nullptr);

    auto glEnabled = vis_render_engine_enabled(VIS_RENDER_ENGINE_GL) != 0u;
    auto explicitGl = vis_create_with_engine(VIS_RENDER_ENGINE_GL);
    CHECK((explicitGl != nullptr) == glEnabled);
    if (explicitGl) {
        CHECK(vis_render_engine(explicitGl) == VIS_RENDER_ENGINE_GL);
        vis_destroy(explicitGl);
    }
    CHECK(vis_default_render_engine_set(VIS_RENDER_ENGINE_GL)
          == (glEnabled ? VIS_RESULT_SUCCESS : VIS_RESULT_NON_SUPPORT));
    if (glEnabled) {
        auto defaultGl = vis_create();
        CHECK(defaultGl != nullptr);
        CHECK(vis_render_engine(defaultGl) == VIS_RENDER_ENGINE_GL);
        vis_destroy(defaultGl);
    }
    CHECK(vis_default_render_engine_set(VIS_RENDER_ENGINE_CPU) == VIS_RESULT_SUCCESS);

    auto engine = vis_create();
    CHECK(engine != nullptr);
    if (!engine) return 1;
    CHECK(vis_render_engine(engine) == VIS_RENDER_ENGINE_CPU);

    CHECK(vis_render(engine, 0.0f) == VIS_RESULT_INSUFFICIENT_CONDITION);
    CHECK(vis_layout_report(engine, 0.0f, 0.0f) == VIS_RESULT_INSUFFICIENT_CONDITION);
    CHECK(vis_layout_report_data(engine) == nullptr && vis_layout_report_size(engine) == 0);
    CHECK(vis_loop(engine) == 0);
    CHECK(std::strcmp(vis_last_error(engine), "InsufficientCondition") == 0);
    CHECK(vis_load_lua(engine, nullptr, 0, "invalid.lua") == VIS_RESULT_INVALID_ARGUMENTS);
    CHECK(std::strcmp(vis_last_error(engine), "InvalidArguments") == 0);
    static constexpr char asset[] = "<svg xmlns='http://www.w3.org/2000/svg' width='4' height='2'>"
                                    "<rect width='2' height='2' fill='#ff4050'/><rect x='2' width='2' height='2' fill='#40a0ff'/>"
                                    "</svg>";
    CHECK(vis_load_asset(engine, "pixels", reinterpret_cast<const uint8_t*>(asset),
                           sizeof(asset) - 1u, "svg")
          == VIS_RESULT_SUCCESS);

    static constexpr char source[] = R"lua(
local scene = vis.scene {
    width = 160, height = 90, loop = true,
    camera = {mode = "interactive", view = "2d", height = 6}
}
local world = scene:space {}
world:point {point = {0, 0}, fill = "#ffffff", id = "origin"}
scene:rectangle {center = {-1, 0}, size = {1, 1}, fill = "#ffffff", id = "box-a"}
scene:rectangle {center = {-0.25, 0}, size = {1, 1}, fill = "#ffffff", id = "box-b"}
scene:rectangle {center = {0.2, 0}, size = {1, 1}, fill = "#ffffff", id = "box-c"}
local panel = scene:rectangle {center = {2, 0}, size = {1, 1}, fill = "#ffffff", id = "panel"}
panel:rectangle {center = {2.55, 0}, size = {0.4, 0.4}, fill = "#ffffff", id = "panel-overflow"}
return scene
    )lua";
    CHECK(vis_load_lua(engine, source, sizeof(source) - 1u, "capi.lua") == 0);
    CHECK(vis_width(engine) == 160 && vis_height(engine) == 90);
    CHECK(vis_loop(engine) == 1);
    VisGlTarget glTarget = {};
    CHECK(vis_render_gl(engine, 0.0f, nullptr) == VIS_RESULT_INVALID_ARGUMENTS);
    CHECK(vis_render_gl(engine, 0.0f, &glTarget) == VIS_RESULT_NON_SUPPORT);
    CHECK(vis_pixels(engine) == nullptr && vis_pixels_size(engine) == 0);
    VisBBox bounds = {};
    CHECK(vis_bounds(engine, "box-a", 0.0f, &bounds) == VIS_RESULT_SUCCESS);
    CHECK(bounds.width > 10.0f && bounds.height > 10.0f);
    uint8_t intersects = 0;
    CHECK(vis_intersects(engine, "box-a", "box-b", 0.0f, 0.0f, &intersects)
          == VIS_RESULT_SUCCESS);
    CHECK(intersects == 1u);
    CHECK(vis_intersects(engine, "box-a", "box-c", 0.0f, 0.0f, &intersects)
          == VIS_RESULT_SUCCESS);
    CHECK(intersects == 0u);
    CHECK(vis_intersects(engine, "box-a", "box-c", 0.0f, 6.0f, &intersects)
          == VIS_RESULT_SUCCESS);
    CHECK(intersects == 1u);
    CHECK(vis_bounds(engine, "missing", 0.0f, &bounds) == VIS_RESULT_INVALID_ARGUMENTS);
    CHECK(vis_layout_report(engine, 0.0f, 2.0f) == VIS_RESULT_SUCCESS);
    auto layout = reinterpret_cast<const char*>(vis_layout_report_data(engine));
    CHECK(layout != nullptr && vis_layout_report_size(engine) > 0u);
    if (layout) {
        CHECK(std::strstr(layout, "\"scenes\":[") != nullptr);
        CHECK(std::strstr(layout, "\"visuals\":[") != nullptr);
        CHECK(std::strstr(layout, "\"paths\":[") != nullptr);
        CHECK(std::strstr(layout, "\"points\":[") != nullptr);
        CHECK(std::strstr(layout, "\"collisions\":[") != nullptr);
        CHECK(std::strstr(layout, "\"containments\":[") != nullptr);
        CHECK(std::strstr(layout, "\"kind\":\"stable\"") != nullptr);
        CHECK(std::strstr(layout, "\"counterpart\":null") != nullptr);
        CHECK(std::strstr(layout, "\"occluder\":null") != nullptr);
        CHECK(std::strstr(layout, "\"occluded\":false") != nullptr);
        CHECK(std::strstr(layout, "\"id\":\"panel-overflow\"") != nullptr);
        CHECK(std::strstr(layout, "\"contained\":false") != nullptr);
    }

    static constexpr char visualSource[] = R"lua(
local root = vis.scene {width = 160, height = 90, background = "#00000000"}
root:rectangle {center = {0, 0}, size = {1, 1}, fill = "#ffffff", id = "base"}

local cover = vis.scene {width = 160, height = 90, background = "#101820"}
root:viewport(cover, {})

local first = vis.scene {width = 160, height = 90, background = "#00000000"}
first:rectangle {center = {-1, 0}, size = {1, 1}, fill = "#ffffff", id = "morph"}
first:picture {asset = "pixels", center = {1, 0}, width = 1, id = "fade"}

local second = vis.scene {width = 160, height = 90, background = "#00000000"}
second:rectangle {center = {-0.5, 0}, size = {1.5, 1}, fill = "#ffffff", id = "morph"}
second:rectangle {center = {1, 0}, size = {1, 1}, fill = "#ffffff", id = "fade"}

root:scene_transition({first, second}, {duration = 1, easing = "linear"})
return root
    )lua";
    CHECK(vis_load_lua(engine, visualSource, sizeof(visualSource) - 1u, "layout-visuals.lua")
          == VIS_RESULT_SUCCESS);
    CHECK(vis_layout_report(engine, 0.5f, 0.0f) == VIS_RESULT_SUCCESS);
    layout = reinterpret_cast<const char*>(vis_layout_report_data(engine));
    CHECK(layout != nullptr && vis_layout_report_size(engine) > 0u);
    if (layout) {
        CHECK(std::strstr(layout,
                          "\"object\":0,\"counterpart\":null,\"occluder\":1,\"kind\":\"stable\"")
              != nullptr);
        CHECK(std::strstr(layout, "\"occluded\":true") != nullptr);
        CHECK(std::strstr(layout,
                          "\"object\":1,\"counterpart\":3,\"occluder\":null,\"kind\":\"morph\"")
              != nullptr);
        CHECK(std::strstr(layout,
                          "\"object\":2,\"counterpart\":4,\"occluder\":null,\"kind\":\"fade-out\"")
              != nullptr);
        CHECK(std::strstr(layout,
                          "\"object\":4,\"counterpart\":2,\"occluder\":null,\"kind\":\"fade-in\"")
              != nullptr);
    }

    static constexpr char assetSource[] = R"lua(
local scene = vis.scene {width = 160, height = 90, camera = {mode = "interactive", view = "2d", height = 6}}
local world = scene:space {opacity = 0}
world:picture {asset = "pixels", center = {1.5, 0}, width = 2, filter = "nearest"}
world:cell {texture = "pixels", origin = {-2.5, -1}, size = {4, 2}, mode = "full"}
return scene
    )lua";
    CHECK(vis_load_lua(engine, assetSource, sizeof(assetSource) - 1u, "assets.lua") == VIS_RESULT_SUCCESS);
    CHECK(vis_loop(engine) == 0);
    CHECK(vis_layout_report_data(engine) == nullptr && vis_layout_report_size(engine) == 0);
    CHECK(vis_render(engine, 0.0f) == VIS_RESULT_SUCCESS);
    CHECK(vis_pixels(engine) != nullptr && vis_pixels_size(engine) == 160u * 90u * 4u);
    if (auto rendered = reinterpret_cast<const uint8_t*>(vis_pixels(engine))) {
        // The svg-bytes asset renders through Picture with its source colors.
        auto red = 0u;
        for (auto i = 0u; i < 160u * 90u; i++) {
            auto pixel = rendered + i * 4u;
            if (pixel[0] >= 240u && pixel[1] <= 80u && pixel[2] <= 100u) red++;
        }
        CHECK(red > 100u);
    }
    CHECK(vis_render_scaled(engine, 0.0f, 2u) == VIS_RESULT_SUCCESS);
    CHECK(vis_width(engine) == 160u && vis_height(engine) == 90u);
    CHECK(vis_pixels(engine) != nullptr && vis_pixels_size(engine) == 320u * 180u * 4u);
    CHECK(vis_render_scaled(engine, 0.0f, 0u) == VIS_RESULT_INVALID_ARGUMENTS);
    CHECK(vis_pixels(engine) != nullptr && vis_pixels_size(engine) == 320u * 180u * 4u);
    CHECK(vis_resize(engine, 320, 180) == 0);
    CHECK(vis_pixels(engine) == nullptr && vis_pixels_size(engine) == 0);
    CHECK(vis_render(engine, 0.0f) == 0);
    CHECK(vis_pixels(engine) != nullptr);
    CHECK(vis_pixels_size(engine) == 320u * 180u * 4u);
    CHECK(vis_camera_mode(engine) == VIS_CAMERA_INTERACTIVE);
    CHECK(vis_camera_view(engine) == VIS_CAMERA_VIEW_2D);
    CHECK(vis_camera(engine, VIS_CAMERA_PAN, 0.1f, -0.1f)
          == VIS_RESULT_NON_SUPPORT);
    CHECK(vis_camera_view(engine) == VIS_CAMERA_VIEW_2D);
    CHECK(vis_load_lua(engine, source, sizeof(source) - 1u, "reloaded.lua") == 0);
    CHECK(vis_width(engine) == 160 && vis_height(engine) == 90);
    CHECK(vis_pixels(engine) == nullptr && vis_pixels_size(engine) == 0);

    static constexpr char fixedSource[] = R"lua(
return vis.scene {camera = {mode = "fixed", view = "2d", height = 8}}
    )lua";
    CHECK(vis_load_lua(engine, fixedSource, sizeof(fixedSource) - 1u, "fixed.lua") == VIS_RESULT_SUCCESS);
    CHECK(vis_camera_mode(engine) == VIS_CAMERA_FIXED);
    CHECK(vis_camera(engine, VIS_CAMERA_PAN, 0.1f, 0.0f) == VIS_RESULT_INSUFFICIENT_CONDITION);

    static constexpr char viewportSource[] = R"lua(
local root = vis.scene {width = 200, height = 100, background = "#101820"}
local left = vis.scene {width = 100, height = 100, background = "#d84040"}
local leftSpace = left:space {opacity = 0}
leftSpace:point {point = {0, 0}, fill = "#ffffff"}
left:wait(0.4)
local right = vis.scene {width = 100, height = 100, background = "#4080d8"}
local rightSpace = right:space {opacity = 0}
rightSpace:point {point = {0, 0}, fill = "#ffffff"}
right:wait(1.1)
root:viewport(left, {x = 0, y = 0, width = 0.5, height = 1})
root:viewport(right, {x = 0.5, y = 0, width = 0.5, height = 1})
return root
    )lua";
    CHECK(vis_load_lua(engine, viewportSource, sizeof(viewportSource) - 1u, "viewport.lua") == VIS_RESULT_SUCCESS);
    CHECK(vis_width(engine) == 200 && vis_height(engine) == 100);
    CHECK(vis_duration(engine) == 1.1f);
    CHECK(vis_render(engine, vis_duration(engine)) == VIS_RESULT_SUCCESS);
    CHECK(vis_pixels(engine) != nullptr && vis_pixels_size(engine) == 200u * 100u * 4u);
    CHECK(vis_resize(engine, 400, 200) == VIS_RESULT_SUCCESS);
    CHECK(vis_pixels(engine) == nullptr && vis_pixels_size(engine) == 0);
    CHECK(vis_render(engine, vis_duration(engine)) == VIS_RESULT_SUCCESS);
    CHECK(vis_pixels_size(engine) == 400u * 200u * 4u);

    vis_destroy(engine);
    if (failures) std::fprintf(stderr, "%d C API checks failed\n", failures);
    return failures ? 1 : 0;
}
