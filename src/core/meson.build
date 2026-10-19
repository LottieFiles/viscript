# Everything compiled into libviscript (and the optional module libraries).
# Each core folder appends to these shared variables.
vis_inc = [include_directories('../inc')]
vis_sources = []
vis_dependencies = [tvg_dep]
if host_machine.system() == 'linux'
    vis_dependencies += dependency('threads')
endif

subdir('common')
subdir('loaders')
subdir('lua')
subdir('scene')
subdir('renderer')
subdir('savers')

# Native builds expose the WebAssembly C ABI through the library as well.
if host_machine.system() != 'emscripten'
    vis_sources += files('../bindings/wasm/visWasm.cpp')
endif

vis_lib = static_library(
    'viscript',
    vis_sources,
    include_directories: vis_inc,
    dependencies: vis_dependencies,
    link_whole: lua_link_whole,
    cpp_args: vis_args,
    install: true,
)

vis_dep = declare_dependency(
    link_with: vis_lib,
    include_directories: include_directories('../inc'),
    dependencies: vis_dependencies,
)

install_headers('../inc/vis.h', '../inc/vis_capi.h')

# Optional modules. Each records its define/dependency for hosts (CLI, WASM)
# and its pkg-config metadata; the GL renderer is a library for native hosts
# only, with no define or Lua hook.
vis_module_args = []
vis_module_deps = []
vis_module_pkgs = []

if ui_enabled
    subdir('ui')
    vis_module_args += '-DVIS_UI'
    vis_module_deps += vis_ui_dep
    vis_module_pkgs += {
        'lib': vis_ui_lib, 'name': 'vis-ui',
        'description': 'Experimental interactive control UI module for viscript',
    }
endif
if input_enabled
    subdir('input')
    vis_module_args += '-DVIS_INPUT'
    vis_module_deps += vis_input_dep
    vis_module_pkgs += {
        'lib': vis_input_lib, 'name': 'vis-input',
        'description': 'Experimental keyboard, pointer, and camera input module for viscript',
    }
endif
if lua_runtime_enabled
    subdir('lua_runtime')
    vis_module_args += '-DVIS_LUA_RUNTIME'
    vis_module_deps += vis_lua_runtime_dep
    vis_module_pkgs += {
        'lib': vis_lua_runtime_lib, 'name': 'vis-lua-runtime',
        'description': 'Experimental retained Lua fixed-step runtime for viscript',
        'requires': input_enabled ? ['viscript', 'vis-input'] : ['viscript'],
    }
endif
if audio_enabled
    subdir('audio')
    vis_module_args += '-DVIS_AUDIO'
    vis_module_deps += vis_audio_dep
    vis_module_pkgs += {
        'lib': vis_audio_lib, 'name': 'vis-audio',
        'description': 'Experimental synchronized audio module for viscript',
        'libraries': miniaudio_system_deps,
    }
endif
if states_enabled
    subdir('states')
    vis_module_args += '-DVIS_STATES'
    vis_module_deps += vis_states_dep
    vis_module_pkgs += {
        'lib': vis_states_lib, 'name': 'vis-states',
        'description': 'Experimental named object states and transitions for viscript',
    }
endif
if diagram_enabled
    subdir('diagram')
    vis_module_args += '-DVIS_DIAGRAM'
    vis_module_deps += vis_diagram_dep
    vis_module_pkgs += {
        'lib': vis_diagram_lib, 'name': 'vis-diagram',
        'description': 'Experimental semantic diagram authoring module for viscript',
    }
endif
if tex_enabled
    subdir('tex')
    vis_module_args += '-DVIS_TEX'
    vis_module_deps += vis_tex_dep
    vis_module_pkgs += {
        'lib': vis_tex_lib, 'name': 'vis-tex',
        'description': 'Experimental TeX math module for viscript',
    }
endif
if gl_enabled
    subdir('gl')
    vis_module_pkgs += {
        'lib': vis_gl_lib, 'name': 'vis-gl',
        'description': 'Experimental OpenGL renderer module for viscript',
    }
endif
