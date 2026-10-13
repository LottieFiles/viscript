vis_inc += include_directories('.', 'tvg')
vis_sources += files(
    'visDepthSplit.cpp',
    'visDisplayBlend.cpp',
    'visDisplayList.cpp',
    'visRenderer.cpp',
    'visRendererData.cpp',
    'tvg/visTvgRenderer.cpp',
)

# Disabled engines link a stub so the backend table stays complete.
if cpu_enabled
    vis_sources += files('tvg/visTvgSwRenderer.cpp')
else
    vis_sources += files('tvg/visTvgSwRendererStub.cpp')
endif
if gl_enabled
    vis_sources += files('tvg/visTvgGlRenderer.cpp')
else
    vis_sources += files('tvg/visTvgGlRendererStub.cpp')
endif
