vis_inc += include_directories('.')
vis_sources += files('visAssetLoader.cpp')

# ThorVG-backed asset decoding is only available with the CPU engine.
if cpu_enabled
    vis_sources += files('tvg/visTvgAssetDecoder.cpp')
else
    vis_sources += files('tvg/visTvgAssetDecoderStub.cpp')
endif
