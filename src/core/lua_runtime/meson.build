vis_lua_runtime_lib = static_library(
    'vis-lua-runtime',
    'visLuaRuntime.cpp',
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: [lua_dep, vis_input_dep],
    install: true,
)
vis_lua_runtime_dep = declare_dependency(
    link_with: vis_lua_runtime_lib,
    include_directories: include_directories('../../inc'),
    dependencies: [vis_dep, vis_input_dep],
)
install_headers('../../inc/vis_lua_runtime.h')
