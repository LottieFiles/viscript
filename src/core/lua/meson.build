vis_inc += include_directories('.')

if lua_enabled
    vis_sources += files('visLua.cpp', 'visLuaApi.cpp', 'visCode.cpp')
    vis_dependencies += lua_dep
else
    vis_sources += files('visLuaStub.cpp')
endif
