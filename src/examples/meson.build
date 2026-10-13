# Example stem -> validation mode.
#   'render'      inspect, plus a CPU render to PNG
#   'inspect'     inspect only
#   'cpu-inspect' inspect only, and only with the CPU engine
# Test names derive from the stem ('_' -> '-'); `example_names` overrides that.
example_scenes = {
    'matrix_transform': 'inspect',
    'image_cells': 'cpu-inspect',
    'cell_field': 'render',
    'voxel_field': 'render',
    'surface_plot': 'render',
    'solids3d': 'render',
    'gradient_kinds': 'render',
    'theme_showcase': 'render',
    'animation_gallery': 'render',
    'scene_transition': 'render',
    'object_gallery': 'render',
    'text_reveal_transitions': 'render',
    'vis_quickstart_cheatsheet': 'render',
}
if diagram_enabled
    example_scenes += {'diagram_native_flow': 'render'}
endif
example_names = {'solids3d': 'solids-3d'}
example_font = files('assets/Pretendard.ttf')
example_fonts = ['--font', example_font, '--font', files('assets/JetBrainsMono.ttf')]

if get_option('examples')
    example_files = []
    foreach stem, _ : example_scenes
        example_files += 'lua' / stem + '.lua'
    endforeach
    install_data(example_files, install_dir: get_option('datadir') / 'viscript' / 'examples')
    install_data(
        'assets/pixels.svg',
        'assets/Pretendard.ttf',
        'assets/Pretendard-OFL.txt',
        'assets/JetBrainsMono.ttf',
        'assets/JetBrainsMono-OFL.txt',
        install_dir: get_option('datadir') / 'viscript' / 'assets',
    )
endif

if get_option('tests') and cli_enabled
    foreach stem, mode : example_scenes
        name = example_names.get(stem, stem.replace('_', '-'))
        scene = files('lua' / stem + '.lua')
        if mode != 'cpu-inspect' or cpu_enabled
            test('example-' + name, vis_cli, args: ['inspect', scene])
        endif
        if mode == 'render' and cpu_enabled
            test(
                'render-' + name,
                vis_cli,
                args: ['render', scene] + example_fonts + ['-o', 'example-' + name + '.png'],
            )
        endif
    endforeach
endif
