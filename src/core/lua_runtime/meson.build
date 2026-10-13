vis_lua_runtime_dependencies = [lua_dep]
vis_lua_runtime_args = []
vis_lua_runtime_public_dependencies = [vis_dep]
if input_enabled
    vis_lua_runtime_dependencies += vis_input_dep
    vis_lua_runtime_public_dependencies += vis_input_dep
    vis_lua_runtime_args += '-DVIS_INPUT'
endif
vis_lua_runtime_lib = static_library(
    'vis-lua-runtime',
    'visLuaRuntime.cpp',
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: vis_lua_runtime_dependencies,
    cpp_args: vis_lua_runtime_args,
    install: true,
)
vis_lua_runtime_dep = declare_dependency(
    link_with: vis_lua_runtime_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_lua_runtime_public_dependencies,
)
install_headers('../../inc/vis_lua_runtime.h')
