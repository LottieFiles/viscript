vis_diagram_sources = files(
    'visDiagram.cpp',
    'visDiagramRoute.cpp',
    'visDiagramLayout.cpp',
    'visDiagramAudit.cpp',
    'visDiagramLua.cpp',
)
vis_diagram_lib = static_library(
    'vis-diagram',
    vis_diagram_sources,
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: lua_dep,
    install: true,
)
vis_diagram_dep = declare_dependency(
    link_with: vis_diagram_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_diagram.h')
