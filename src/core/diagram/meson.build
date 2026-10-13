vis_diagram_sources = files(
    'visDiagram.cpp',
    'visDiagramRoute.cpp',
    'visDiagramLayout.cpp',
    'visDiagramAudit.cpp',
)
vis_diagram_dependencies = []
if lua_enabled
    vis_diagram_sources += files('visDiagramLua.cpp')
    vis_diagram_dependencies += lua_dep
else
    vis_diagram_sources += files('visDiagramLuaStub.cpp')
endif
vis_diagram_lib = static_library(
    'vis-diagram',
    vis_diagram_sources,
    include_directories: include_directories('../../inc', '../lua'),
    dependencies: vis_diagram_dependencies,
    install: true,
)
vis_diagram_dep = declare_dependency(
    link_with: vis_diagram_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_diagram.h')
