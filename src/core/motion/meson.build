vis_motion_sources = files('visMotion.cpp')
vis_motion_dependencies = []
if lua_enabled
    vis_motion_sources += files('visMotionLua.cpp')
    vis_motion_dependencies += lua_dep
else
    vis_motion_sources += files('visMotionLuaStub.cpp')
endif
vis_motion_lib = static_library(
    'vis-motion',
    vis_motion_sources,
    include_directories: include_directories(
        '../../inc', '../common', '../lua',
    ),
    dependencies: vis_motion_dependencies,
    install: true,
)
vis_motion_dep = declare_dependency(
    link_with: vis_motion_lib,
    include_directories: include_directories('../../inc'),
    dependencies: vis_dep,
)
install_headers('../../inc/vis_motion.h')
