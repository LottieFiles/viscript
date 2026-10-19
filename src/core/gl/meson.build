# The experimental OpenGL renderer: libvis-gl and vis_gl.h, on ThorVG's GL engine. It is a
# library for native hosts that own a GL context; no host program here links it.
vis_gl_lib = static_library(
    'vis-gl',
    files('visGlRenderer.cpp'),
    include_directories: include_directories('../../inc', '../renderer/tvg'),
    dependencies: [vis_dep],
    install: true,
)
vis_gl_dep = declare_dependency(
    link_with: vis_gl_lib,
    include_directories: include_directories('../../inc'),
    dependencies: [vis_dep],
)
install_headers('../../inc/vis_gl.h')
