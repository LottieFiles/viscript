vis_states_sources = files('visStates.cpp')
vis_states_dependencies = []
if lua_enabled
    vis_states_sources += files('visStatesLua.cpp')
    vis_states_dependencies += lua_dep
else
    vis_states_sources += files('visStatesLuaStub.cpp')
endif
vis_states_lib = static_library(
    'vis-states',
    vis_states_sources,
    include_directories: include_directories(
        '../../inc', '../common', '../lua',
    ),
    dependencies: vis_states_dependencies,
    install: true,
)
vis_states_dep = declare_dependency(
    link_with: vis_states_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_states.h')
