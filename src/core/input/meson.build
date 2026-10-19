vis_input_sources = files('visInput.cpp', 'visInputState.cpp', 'visInputLua.cpp')
vis_input_lib = static_library(
    'vis-input',
    vis_input_sources,
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: lua_dep,
    install: true,
)
vis_input_dep = declare_dependency(
    link_with: vis_input_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_input.h')
