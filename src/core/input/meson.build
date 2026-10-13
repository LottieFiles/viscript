vis_input_sources = files('visInput.cpp', 'visInputState.cpp')
vis_input_dependencies = []
if lua_enabled
    vis_input_sources += files('visInputLua.cpp')
    vis_input_dependencies += lua_dep
else
    vis_input_sources += files('visInputLuaStub.cpp')
endif
vis_input_lib = static_library(
    'vis-input',
    vis_input_sources,
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: vis_input_dependencies,
    install: true,
)
vis_input_dep = declare_dependency(
    link_with: vis_input_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_input.h')
