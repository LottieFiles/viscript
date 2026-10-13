vis_ui_sources = files('visUi.cpp')
vis_ui_dependencies = []
if lua_enabled
    vis_ui_sources += files('visUiLua.cpp')
    vis_ui_dependencies += lua_dep
else
    vis_ui_sources += files('visUiLuaStub.cpp')
endif
vis_ui_lib = static_library(
    'vis-ui',
    vis_ui_sources,
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: vis_ui_dependencies,
    install: true,
)
vis_ui_dep = declare_dependency(
    link_with: vis_ui_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_ui.h')
