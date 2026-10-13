miniaudio_project = subproject('miniaudio')
miniaudio_lib = miniaudio_project.get_variable('miniaudio_lib')
miniaudio_include = miniaudio_project.get_variable('miniaudio_include')
miniaudio_args = miniaudio_project.get_variable('miniaudio_args')
miniaudio_system_deps = miniaudio_project.get_variable('miniaudio_system_deps')
miniaudio_license = miniaudio_project.get_variable('miniaudio_license')
vis_audio_sources = files('visAudio.cpp', 'visAudioPlayer.cpp')
vis_audio_dependencies = [vis_dep]
if lua_enabled
    vis_audio_sources += files('visAudioLua.cpp')
    vis_audio_dependencies += lua_dep
else
    vis_audio_sources += files('visAudioLuaStub.cpp')
endif
vis_audio_lib = static_library(
    'vis-audio',
    vis_audio_sources,
    include_directories: [
        include_directories('../../inc', '../lua'),
        miniaudio_include,
    ],
    dependencies: vis_audio_dependencies,
    cpp_args: miniaudio_args,
    link_whole: miniaudio_lib,
    install: true,
)
vis_audio_dep = declare_dependency(
    link_with: vis_audio_lib,
    include_directories: include_directories('../../inc'),
    dependencies: [vis_dep] + miniaudio_system_deps,
)
install_headers('../../inc/vis_audio.h')
install_data(
    miniaudio_license,
    install_dir: get_option('datadir') / 'licenses' / 'vis-audio',
    rename: 'LICENSE-miniaudio.txt',
)
