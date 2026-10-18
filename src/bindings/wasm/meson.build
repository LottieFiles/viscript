# Emscripten runtime. The executable is declared in the root meson.build as
# `vis_wasm` so vis-wasm.js and its copied companions stay in the build root.
wasm_exports = [
    'malloc', 'free', 'vis_create', 'vis_create_with_engine',
    'vis_default_render_engine_set', 'vis_default_render_engine',
    'vis_render_engine_enabled', 'vis_render_engine', 'vis_destroy',
    'vis_host_theme', 'vis_adaptive_theme', 'vis_load_lua', 'vis_load_font',
    'vis_load_asset', 'vis_render', 'vis_render_scaled', 'vis_render_gl',
    'vis_bounds', 'vis_intersects', 'vis_layout_report', 'vis_layout_report_data',
    'vis_layout_report_size', 'vis_resize', 'vis_camera_mode', 'vis_camera_view',
    'vis_pixels', 'vis_pixels_size', 'vis_width', 'vis_height', 'vis_duration',
    'vis_loop', 'vis_last_error', 'vis_missing_fonts',
]
if ui_enabled or input_enabled
    wasm_exports += ['vis_camera', 'vis_input']
endif
if ui_enabled
    wasm_exports += [
        'vis_ui_input', 'vis_ui_sample_handle', 'vis_ui_sample_tag', 'vis_ui_sample_type',
        'vis_ui_sample_x', 'vis_ui_sample_y', 'vis_ui_sample_u', 'vis_ui_sample_v',
        'vis_ui_sample_rgba',
    ]
endif
if input_enabled
    wasm_exports += [
        'vis_input_begin', 'vis_input_release',
        'vis_input_key_state', 'vis_input_key_begin', 'vis_input_key_end',
        'vis_input_pointer_state', 'vis_input_pointer_x',
        'vis_input_pointer_y', 'vis_input_pointer_dx', 'vis_input_pointer_dy',
        'vis_input_pointer_wheel_x', 'vis_input_pointer_wheel_y',
        'vis_input_pointer_previous_buttons', 'vis_input_pointer_buttons',
        'vis_input_pointer_pressed', 'vis_input_pointer_released',
    ]
endif
if lua_runtime_enabled
    wasm_exports += [
        'vis_runtime_active', 'vis_runtime_advance', 'vis_runtime_time',
        'vis_runtime_dropped', 'vis_runtime_tick',
        'vis_runtime_steps', 'vis_runtime_interpolation',
        'vis_runtime_sound_count', 'vis_runtime_sound',
    ]
endif
if audio_enabled
    wasm_exports += [
        'vis_audio_supported', 'vis_audio_load', 'vis_audio_start',
        'vis_audio_play', 'vis_audio_stop', 'vis_audio_active',
        'vis_audio_master_gain', 'vis_audio_bus_gain', 'vis_audio_transport',
    ]
endif

wasm_sources = files('visWasm.cpp')
wasm_dependencies = [vis_dep] + vis_module_deps
wasm_cpp_args = vis_module_args
wasm_link_args = [
    '--no-entry',
    '-sALLOW_MEMORY_GROWTH=1',
    '-sMODULARIZE=1',
    '-sEXPORT_ES6=1',
    '-sENVIRONMENT=web,worker',
    '-sEXPORTED_FUNCTIONS=[_' + ',_'.join(wasm_exports) + ']',
    '-sEXPORTED_RUNTIME_METHODS=[UTF8ToString,HEAPU8]',
    '-sFILESYSTEM=0',
]

# Files copied next to vis-wasm.js: JS bindings plus runtime assets.
wasm_copy_files = vis_js_files + files(
    '../../examples/assets/pixels.svg',
    '../../examples/assets/Pretendard.ttf',
    '../../examples/assets/Pretendard-OFL.txt',
    '../../examples/lua/surface_plot.lua',
)
