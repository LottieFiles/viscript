native = host_machine.system() != 'emscripten'
lua_inc = include_directories('../core/lua')
font_args = [
    '-DVIS_TEST_FONT="' + (meson.project_source_root() / 'src/examples/assets/Pretendard.ttf') + '"',
    '-DVIS_TEST_MONO_FONT="' + (meson.project_source_root() / 'src/examples/assets/JetBrainsMono.ttf') + '"',
]
scene_test_sources = files(
    'testScene.cpp',
    'testSceneAnimations.cpp',
    'testSceneClone.cpp',
    'testSceneContours.cpp',
    'testSceneLayers.cpp',
    'testSceneLayout.cpp',
    'testSceneMorph.cpp',
    'testSceneSampling.cpp',
    'testSceneShapes.cpp',
    'testSceneSpace.cpp',
    'testSceneStyle.cpp',
    'testSceneSurface.cpp',
    'testSceneText.cpp',
    'testSceneTimeline.cpp',
    'testSceneTransitions.cpp',
)
lua_test_sources = files(
    'testLua.cpp',
    'testLuaAnimations.cpp',
    'testLuaCamera.cpp',
    'testLuaCode.cpp',
    'testLuaCopies.cpp',
    'testLuaLayout.cpp',
    'testLuaPlots.cpp',
    'testLuaSampling.cpp',
    'testLuaStyle.cpp',
    'testLuaText.cpp',
    'testLuaTrackers.cpp',
    'testLuaTransitions.cpp',
)

# Optional extra dependencies/arguments that several tests share.
ui_deps = ui_enabled ? [vis_ui_dep] : []
ui_args = ui_enabled ? ['-DVIS_UI'] : []
input_deps = input_enabled ? [vis_input_dep] : []
input_args = input_enabled ? ['-DVIS_INPUT'] : []
runtime_deps = lua_runtime_enabled ? [vis_lua_runtime_dep] : []
runtime_args = lua_runtime_enabled ? ['-DVIS_LUA_RUNTIME'] : []
gl_deps = []
if gl_enabled and host_machine.system() == 'darwin'
    gl_deps += dependency('appleframeworks', modules: 'OpenGL')
endif

# C++ unit tests. Each entry: test name, executable, source (default: exe.cpp),
# build condition, dependencies, cpp args, extra include directories.
cpp_tests = [
    {'name': 'math', 'exe': 'testMath', 'when': true},
    {
        'name': 'renderer', 'exe': 'testRenderer', 'when': true,
        'inc': [include_directories('../core/renderer')],
    },
]
if diagram_enabled
    cpp_tests += {
        'name': 'diagram', 'exe': 'testDiagram', 'when': true, 'deps': [vis_diagram_dep],
        'args': font_args,
    }
endif
if states_enabled
    cpp_tests += [
        {'name': 'states', 'exe': 'testStates', 'when': true, 'deps': [vis_states_dep]},
    ]
endif
if tex_enabled
    cpp_tests += [
        {
            'name': 'tex', 'exe': 'testTex', 'when': true, 'deps': [vis_tex_dep],
            'inc': [include_directories('../core/tex')],
        },
        {
            'name': 'tex-scene', 'exe': 'testTexScene', 'when': native,
            'deps': [vis_tex_dep], 'args': font_args,
        },
    ]
endif

# Renderer tests.
cpp_tests += [
    {
        'name': 'sw-scene', 'exe': 'testScene', 'src': scene_test_sources,
        'when': native, 'args': font_args,
    },
    {'name': 'composite', 'exe': 'testComposite', 'when': native},
    {'name': 'timing-options', 'exe': 'testTimingOptions', 'when': native},
    {
        'name': 'ui', 'exe': 'testUi', 'when': ui_enabled, 'deps': ui_deps, 'args': ui_args,
        'inc': [lua_inc],
    },
    {
        'name': 'input', 'exe': 'testInput', 'when': input_enabled,
        'deps': input_deps + ui_deps, 'args': input_args + ui_args, 'inc': [lua_inc],
    },
]
if states_enabled
    cpp_tests += {'name': 'states-sw', 'exe': 'testStatesSw', 'when': true, 'deps': [vis_states_dep]}
endif
cpp_tests += {
    'name': 'lua-runtime', 'exe': 'testLuaRuntime', 'when': lua_runtime_enabled,
    'deps': runtime_deps, 'args': runtime_args + input_args, 'inc': [lua_inc],
}
if audio_enabled
    cpp_tests += {
        'name': 'audio', 'exe': 'testAudio', 'when': true,
        'deps': [vis_audio_dep] + runtime_deps, 'args': ['-DVIS_AUDIO'] + runtime_args,
        'inc': [lua_inc, miniaudio_include],
    }
endif
# The GL module's tests make an offscreen CGL context, so they run on macOS only.
if gl_deps.length() > 0
    svg_arg = '-DVIS_TEST_SVG="' + (meson.project_source_root() / 'src/examples/assets/pixels.svg') + '"'
    cpp_tests += [
        {'name': 'gl-offscreen', 'exe': 'testGl', 'when': true, 'deps': [vis_gl_dep] + gl_deps},
        {
            'name': 'gl-parity', 'exe': 'testGlParity', 'when': true,
            'deps': [vis_gl_dep] + gl_deps, 'args': font_args + [svg_arg],
        },
    ]
endif

# Lua binding tests.
if states_enabled
    cpp_tests += {
        'name': 'states-lua', 'exe': 'testStatesLua', 'when': true,
        'deps': [vis_states_dep] + runtime_deps, 'args': ['-DVIS_STATES'] + runtime_args,
        'inc': [lua_inc],
    }
endif
if diagram_enabled
    cpp_tests += {
        'name': 'authoring-modules', 'exe': 'testAuthoringModules', 'when': true,
        'deps': [vis_diagram_dep] + ui_deps + input_deps,
        'args': ['-DVIS_DIAGRAM'] + ui_args + input_args, 'inc': [lua_inc],
    }
endif
if tex_enabled
    cpp_tests += {
        'name': 'tex-lua', 'exe': 'testTexLua', 'when': native,
        'deps': [vis_tex_dep], 'args': font_args + ['-DVIS_TEX'], 'inc': [lua_inc],
    }
endif
cpp_tests += [
    {
        'name': 'lua-extension', 'exe': 'testLuaExtension', 'when': true, 'inc': [lua_inc],
        'args': native ? font_args : [],
    },
    {
        'name': 'lua-scene', 'exe': 'testLua', 'src': lua_test_sources,
        'when': native, 'args': font_args, 'inc': [lua_inc],
    },
    {'name': 'c-api', 'exe': 'testCapi', 'when': native},
    {'name': 'memory', 'exe': 'testMemory', 'when': native},
    {
        'name': 'retained', 'exe': 'testRetained', 'when': native, 'inc': [lua_inc],
        'args': font_args + ['-DVIS_TEST_EXAMPLES="' + (meson.project_source_root() / 'src/examples/lua') + '"'],
    },
]

foreach t : cpp_tests
    if t['when']
        exe = executable(
            t['exe'],
            t.get('src', t['exe'] + '.cpp'),
            dependencies: t.get('deps', [vis_dep]),
            include_directories: t.get('inc', []),
            cpp_args: t.get('args', []),
        )
        test(t['name'], exe)
    endif
endforeach

# Node/TypeScript tests drive the CLI and the JS bindings, and keep the editor's API schema true.
node = find_program('node', required: false)
if node.found()
    # The VS Code extension's schema and its generated LuaLS metadata must follow the Lua binding.
    test('api-sync', node, args: [files('../../tools/vscode/test/api-sync.mjs')])
endif
if cli_enabled
    if node.found()
        node_tests = {
            'js-binding': [files('testJsBinding.mjs', '../bindings/js/bindings.js'), vis_cli],
        }
        if audit_enabled
            node_tests += {
                'audit-cli': [
                    files('testAuditCli.mjs'), vis_cli,
                    files(
                        'fixtures/audit/layout-good.lua',
                        'fixtures/audit/layout-transient-failure.lua',
                        'fixtures/audit/layout-final-failure.lua',
                    ),
                ],
            }
        endif
        if audit_enabled and diagram_enabled
            node_tests += {
                'diagram-audit': [
                    files('testDiagramAudit.mjs'), vis_cli,
                    files(
                        '../examples/lua/diagram_native_flow.lua',
                        'fixtures/audit/diagram-static-failure.lua',
                        'fixtures/audit/diagram-scene-paths.lua',
                        'fixtures/audit/diagram-auto-route.lua',
                        'fixtures/audit/diagram-sampled-failure.lua',
                    ),
                ],
            }
        endif
        # Room for a busy machine: js-binding alone takes about 15 s, half of meson's default.
        foreach name, args : node_tests
            test(name, node, args: args, timeout: 120)
        endforeach
    endif

    # A contact sheet takes seconds, the names of scene marks, and end; a name the scene lacks fails.
    sheet_scene = files('fixtures/cli/sheet-marks.lua')
    test('cli-sheet', vis_cli, args: ['render', sheet_scene, '--times', '0,moved,end', '-o', 'cli-sheet.png'])
    test(
        'cli-sheet-unknown-mark',
        vis_cli,
        args: ['render', sheet_scene, '--times', '0,turned', '-o', 'cli-sheet-unknown.png'],
        should_fail: true,
    )

    # tsc from PATH, or else the one npm installs for the VS Code extension or the player.
    typescript = find_program(
        'tsc',
        meson.project_source_root() / 'tools/vscode/node_modules/.bin/tsc',
        meson.project_source_root() / 'tools/player/node_modules/.bin/tsc',
        required: false,
    )
    if typescript.found()
        test(
            'ts-binding',
            typescript,
            args: [
                '--noEmit', '--strict', '--target', 'ES2022', '--module', 'ES2022',
                '--moduleResolution', 'bundler', '--lib', 'ES2022,DOM', files('testTypes.ts'),
            ],
            timeout: 120,
        )
    else
        message('Skipping ts-binding: no tsc on PATH or in tools/vscode or tools/player (npm ci there)')
    endif
endif
