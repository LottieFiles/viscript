project(
    'viscript',
    ['c', 'cpp'],
    version: '0.1.0',
    meson_version: '>=1.3.0',
    default_options: [
        'c_std=c11',
        'cpp_std=c++17',
        'cpp_eh=none',
        'cpp_rtti=false',
        'default_library=static',
        'warning_level=3',
    ],
)

cpp = meson.get_compiler('cpp')
lua_option = get_option('lua')
modules = get_option('modules')
all_modules = modules.contains('all')
savers = get_option('savers')
all_savers = savers.contains('all')
gif_saver_enabled = all_savers or savers.contains('gif')
video_saver_enabled = all_savers or savers.contains('video')
tools = get_option('tools')
all_tools = tools.contains('all')
cli_requested = all_tools or tools.contains('cli') or tools.contains('audit')
audit_requested = all_tools or tools.contains('audit')
ui_enabled = all_modules or modules.contains('ui')
input_enabled = all_modules or modules.contains('input')
lua_runtime_enabled = all_modules or modules.contains('runtime')
audio_enabled = all_modules or modules.contains('audio')
motion_enabled = all_modules or modules.contains('motion')
diagram_enabled = all_modules or modules.contains('diagram')
tex_enabled = all_modules or modules.contains('tex')
engines = get_option('engines')
cpu_enabled = engines.contains('cpu')
gl_enabled = engines.contains('gl')

# ThorVG loads GL at runtime; disable it when the platform lacks a GL library.
if gl_enabled
    gl_platform = false
    gl_system = host_machine.system()
    if gl_system == 'emscripten'
        warning('GL engine disabled for Emscripten builds; the WebAssembly API renders through the CPU engine')
    elif gl_system == 'darwin'
        gl_platform = dependency('appleframeworks', modules: 'OpenGL', required: false).found()
    elif gl_system == 'windows'
        gl_platform = true
    else
        gl_platform = (cpp.find_library('GL', required: false).found()
            or cpp.find_library('EGL', required: false).found()
            or cpp.find_library('GLESv2', required: false).found())
    endif
    if not gl_platform
        if gl_system != 'emscripten'
            warning('GL engine disabled: no OpenGL library found for ' + gl_system + ' (use -Dengines=cpu to silence)')
        endif
        gl_enabled = false
        engines = cpu_enabled ? ['cpu'] : []
    endif
endif

if engines.length() == 0
    error('Select at least one render engine: -Dengines=cpu, -Dengines=gl, or -Dengines=cpu,gl')
endif
if host_machine.system() == 'emscripten' and not cpu_enabled
    error('Emscripten builds require the CPU engine for the RGBA WebAssembly API')
endif

tvg_engines = []
foreach engine : ['cpu', 'gl']
    if engines.contains(engine)
        tvg_engines += engine
    endif
endforeach

# External dependencies: ThorVG (system or bundled subproject), then Lua.
tvg_options = [
    'engines=' + ','.join(tvg_engines),
    'default_library=static',
    'static=true',
    'loaders=svg,png,jpg,webp,ttf',
    'savers=',
    'bindings=',
    'extra=',
    'threads=false',
    'tests=false',
    'werror=false',
]
if host_machine.system() == 'emscripten'
    tvg_options += 'file=false'
endif

tvg_bundled = get_option('bundled_thorvg') or host_machine.system() == 'emscripten'
tvg_dep = dependency('', required: false)
# Probe system ThorVG's GL engine on macOS before resolving the dependency.
# Other platforms report a missing engine when the host creates its renderer.
if gl_enabled and not tvg_bundled and host_machine.system() == 'darwin' and not meson.is_cross_build()
    pkgconfig = find_program('pkg-config', 'pkgconf', required: false)
    tvg_probe_ok = true
    if pkgconfig.found()
        tvg_pc = run_command(pkgconfig, '--cflags', '--libs', 'thorvg-1 >= 1.2.0', check: false)
        if tvg_pc.returncode() == 0
            tvg_probe = cpp.run(
                '''#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/OpenGL.h>
#include <thorvg.h>
int main() {
    CGLPixelFormatAttribute attributes[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL3_Core, (CGLPixelFormatAttribute)0};
    CGLPixelFormatObj format = nullptr;
    GLint count = 0;
    CGLContextObj context = nullptr;
    if (CGLChoosePixelFormat(attributes, &format, &count) != kCGLNoError || !format) return 0;
    if (CGLCreateContext(format, nullptr, &context) != kCGLNoError || CGLSetCurrentContext(context) != kCGLNoError) return 0;
    if (tvg::Initializer::init() != tvg::Result::Success) return 1;
    auto canvas = tvg::GlCanvas::gen();
    auto ok = canvas != nullptr;
    delete canvas;
    return ok ? 0 : 1;
}''',
                args: tvg_pc.stdout().strip().split() + ['-framework', 'OpenGL'],
                name: 'system ThorVG GL engine',
            )
            tvg_probe_ok = tvg_probe.compiled() and tvg_probe.returncode() == 0
        endif
    endif
    if not tvg_probe_ok
        message('System ThorVG lacks a working GL engine; using the bundled ThorVG subproject')
        tvg_bundled = true
    endif
endif
if not tvg_bundled
    tvg_dep = dependency('thorvg-1', version: '>=1.2.0', required: false, allow_fallback: false)
endif
tvg_fallback = not tvg_dep.found()
if tvg_fallback
    tvg_dep = subproject('thorvg', default_options: tvg_options).get_variable('thorvg_dep')
endif


# Compile definitions shared by the core library and tests.
vis_args = (gif_saver_enabled ? ['-DVIS_SAVER_GIF'] : []) + (video_saver_enabled ? ['-DVIS_SAVER_VIDEO'] : [])

lua_fallback = false
lua_pc = ''
lua_link_whole = []
lua_dep = dependency('', required: false)
if not lua_option.disabled()
    foreach name : ['lua-5.4', 'lua54']
        if not lua_dep.found()
            lua_dep = dependency(name, required: false, allow_fallback: false)
            lua_pc = lua_dep.found() ? name : ''
        endif
    endforeach
    if not lua_dep.found()
        lua_project = subproject('lua', required: lua_option)
        if lua_project.found()
            lua_fallback = true
            lua_dep = lua_project.get_variable('lua_dep')
            lua_link_whole += lua_project.get_variable('lua_lib')
        endif
    endif
endif
lua_enabled = lua_dep.found()
cli_enabled = lua_enabled and cli_requested and host_machine.system() != 'emscripten'
if audit_requested and not cpu_enabled
    error('The layout audit tool requires the CPU render engine')
endif
audit_enabled = cli_enabled and audit_requested
if lua_runtime_enabled and not lua_enabled
    error('Experimental retained Lua runtime requires Lua support')
endif

subdir('src')

# Keep CLI and WASM output paths at the build root.
if cli_enabled
    vis_cli = executable(
        'viscript',
        vis_cli_sources,
        dependencies: vis_cli_dependencies,
        include_directories: vis_cli_inc,
        cpp_args: vis_cli_args,
        install: true,
    )
endif

if lua_enabled and host_machine.system() == 'emscripten'
    vis_wasm = executable(
        'vis-wasm',
        wasm_sources,
        dependencies: wasm_dependencies,
        include_directories: vis_inc,
        cpp_args: wasm_cpp_args,
        name_suffix: 'js',
        link_args: wasm_link_args,
    )
    fs = import('fs')
    foreach file : wasm_copy_files
        configure_file(input: file, output: fs.name(file), copy: true)
    endforeach
endif

subdir('src/examples')
if get_option('tests')
    if cli_enabled
        subdir('tools/skill')
    endif
    subdir('src/test')
endif

summary(
    {
        'ThorVG': tvg_dep.version(),
        'CPU engine': cpu_enabled,
        'GL engine': gl_enabled,
        'Module selector': modules,
        'Saver selector': savers,
        'Tool selector': tools,
        'CLI': cli_enabled,
        'Layout audit': audit_enabled,
        'Lua': lua_enabled,
        'Experimental UI': ui_enabled,
        'Experimental Input': input_enabled,
        'Experimental retained Lua runtime': lua_runtime_enabled,
        'Experimental Audio': audio_enabled,
        'Experimental Motion': motion_enabled,
        'Experimental Diagram': diagram_enabled,
        'Experimental TeX': tex_enabled,
        'Tests': get_option('tests'),
    },
    section: 'viscript',
    bool_yn: true,
)
