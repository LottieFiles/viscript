vis_states_sources = files('visStates.cpp', 'visStatesLua.cpp')
vis_states_lib = static_library(
    'vis-states',
    vis_states_sources,
    include_directories: include_directories(
        '../../inc', '../common', '../lua',
    ),
    dependencies: lua_dep,
    install: true,
)
vis_states_dep = declare_dependency(
    link_with: vis_states_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_states.h')
