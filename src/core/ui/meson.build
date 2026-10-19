vis_ui_sources = files('visUi.cpp', 'visUiLua.cpp')
vis_ui_lib = static_library(
    'vis-ui',
    vis_ui_sources,
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: lua_dep,
    install: true,
)
vis_ui_dep = declare_dependency(
    link_with: vis_ui_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_ui.h')
