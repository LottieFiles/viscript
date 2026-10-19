vis_tex_sources = files(
    'visTex.cpp',
    'visTexLayout.cpp',
    'visTexParse.cpp',
    'visTexScene.cpp',
    'visTexLua.cpp',
)
vis_tex_lib = static_library(
    'vis-tex',
    vis_tex_sources,
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: lua_dep,
    install: true,
)
vis_tex_dep = declare_dependency(
    link_with: vis_tex_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_tex.h')
