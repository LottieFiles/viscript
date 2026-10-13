native = host_machine.system() != 'emscripten'
lua_inc = include_directories('../core/lua')
font_arg = '-DVIS_TEST_FONT="' + (meson.project_source_root() / 'src/examples/assets/Pretendard.ttf') + '"'
mono_font_arg = '-DVIS_TEST_MONO_FONT="' + (meson.project_source_root() / 'src/examples/assets/JetBrainsMono.ttf') + '"'
expect_lua = lua_enabled ? ['-DVIS_EXPECT_LUA'] : []
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
renderer_args = (cpu_enabled ? ['-DVIS_EXPECT_CPU'] : []) + (gl_enabled ? ['-DVIS_EXPECT_GL'] : [])
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
    {'name': 'renderer', 'exe': 'testRenderer', 'when': true, 'args': renderer_args},
]
if diagram_enabled
    cpp_tests += {
        'name': 'diagram', 'exe': 'testDiagram', 'when': true, 'deps': [vis_diagram_dep],
        'args': cpu_enabled ? ['-DVIS_EXPECT_CPU', font_arg] : [],
    }
endif
if motion_enabled
    cpp_tests += [
        {'name': 'motion', 'exe': 'testMotion', 'when': true, 'deps': [vis_motion_dep]},
    ]
endif
if tex_enabled
    cpp_tests += [
        {
            'name': 'tex', 'exe': 'testTex', 'when': true, 'deps': [vis_tex_dep],
            'inc': [include_directories('../core/tex')],
        },
        {
            'name': 'tex-scene', 'exe': 'testTexScene', 'when': cpu_enabled and native,
            'deps': [vis_tex_dep], 'args': [font_arg],
        },
    ]
endif

# Software-renderer tests.
cpp_tests += [
    {
        'name': 'sw-scene', 'exe': 'testScene', 'src': scene_test_sources,
        'when': cpu_enabled and native,
        'args': vis_args + (gl_enabled ? ['-DVIS_EXPECT_GL'] : []) + [font_arg],
    },
    {'name': 'composite', 'exe': 'testComposite', 'when': cpu_enabled and native},
    {'name': 'timing-options', 'exe': 'testTimingOptions', 'when': cpu_enabled and native, 'args': expect_lua},
    {'name': 'ui', 'exe': 'testUi', 'when': cpu_enabled and ui_enabled, 'deps': ui_deps},
    {
        'name': 'input', 'exe': 'testInput', 'when': cpu_enabled and input_enabled,
        'deps': input_deps + ui_deps, 'args': expect_lua + ui_args,
    },
]
if motion_enabled
    cpp_tests += {'name': 'motion-sw', 'exe': 'testMotionSw', 'when': cpu_enabled, 'deps': [vis_motion_dep]}
endif
cpp_tests += {
    'name': 'lua-runtime', 'exe': 'testLuaRuntime', 'when': cpu_enabled and lua_runtime_enabled,
    'deps': runtime_deps, 'args': runtime_args + input_args, 'inc': [lua_inc],
}
if audio_enabled
    cpp_tests += {
        'name': 'audio', 'exe': 'testAudio', 'when': cpu_enabled,
        'deps': [vis_audio_dep] + runtime_deps,
        'args': expect_lua + (lua_runtime_enabled ? ['-DVIS_AUDIO', '-DVIS_LUA_RUNTIME', '-DVIS_EXPECT_LUA_RUNTIME'] : []),
        'inc': lua_runtime_enabled ? [lua_inc] : [],
    }
endif
cpp_tests += {
    'name': 'gl-offscreen', 'exe': 'testGl', 'when': gl_deps.length() > 0,
    'deps': [vis_dep] + gl_deps, 'args': expect_lua,
}
svg_arg = '-DVIS_TEST_SVG="' + (meson.project_source_root() / 'src/examples/assets/pixels.svg') + '"'
cpp_tests += {
    'name': 'gl-parity', 'exe': 'testGlParity', 'when': gl_deps.length() > 0 and cpu_enabled,
    'deps': [vis_dep] + gl_deps, 'args': [font_arg, svg_arg],
}

# Lua binding tests.
if lua_enabled
    if motion_enabled
        cpp_tests += {
            'name': 'motion-lua', 'exe': 'testMotionLua', 'when': true,
            'deps': [vis_motion_dep] + runtime_deps, 'args': ['-DVIS_MOTION'] + runtime_args,
            'inc': [lua_inc],
        }
    endif
    if diagram_enabled
        cpp_tests += [
            {
                'name': 'diagram-lua', 'exe': 'testDiagramLua', 'src': 'testAuthoringModules.cpp',
                'when': true, 'deps': [vis_diagram_dep], 'args': ['-DVIS_DIAGRAM'], 'inc': [lua_inc],
            },
            {
                'name': 'authoring-modules', 'exe': 'testAuthoringModules', 'when': true,
                'deps': [vis_diagram_dep] + ui_deps + input_deps,
                'args': ['-DVIS_DIAGRAM'] + ui_args + input_args, 'inc': [lua_inc],
            },
        ]
    endif
    if tex_enabled
        cpp_tests += {
            'name': 'tex-lua', 'exe': 'testTexLua', 'when': cpu_enabled and native,
            'deps': [vis_tex_dep], 'args': [font_arg, '-DVIS_TEX'], 'inc': [lua_inc],
        }
    endif
    cpp_tests += [
        {
            'name': 'lua-extension', 'exe': 'testLuaExtension', 'when': true, 'inc': [lua_inc],
            'args': cpu_enabled and native ? ['-DVIS_EXPECT_CPU', font_arg] : [],
        },
        {
            'name': 'lua-scene', 'exe': 'testLua', 'src': lua_test_sources,
            'when': cpu_enabled and native, 'args': [font_arg],
        },
        {'name': 'c-api', 'exe': 'testCapi', 'when': cpu_enabled and native},
        {
            'name': 'retained', 'exe': 'testRetained', 'when': cpu_enabled and native,
            'args': [font_arg, '-DVIS_TEST_EXAMPLES="' + (meson.project_source_root() / 'src/examples/lua') + '"'],
        },
    ]
endif

foreach t : cpp_tests
    if t['when']
        exe = executable(
            t['exe'],
            t.get('src', t['exe'] + '.cpp'),
            dependencies: t.get('deps', [vis_dep]),
            include_directories: t.get('inc', []),
            cpp_args: t.get('args', []) + (t.get('args', []).contains(font_arg) ? [mono_font_arg] : []),
        )
        test(t['name'], exe)
    endif
endforeach

# Node/TypeScript tests drive the CLI and the JS bindings.
if cli_enabled
    node = find_program('node', required: false)
    if node.found() and cpu_enabled
        node_tests = {
            'js-binding': [files('testJsBinding.mjs', '../bindings/js/bindings.js'), vis_cli],
        }
        if audit_enabled
            node_tests += {
                'audit-cli': [
                    files('testAuditCli.mjs'), vis_cli, example_font,
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
                    files('testDiagramAudit.mjs'), vis_cli, example_font,
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
        foreach name, args : node_tests
            test(name, node, args: args)
        endforeach
    endif

    typescript = find_program('tsc', required: false)
    if typescript.found()
        test(
            'ts-binding',
            typescript,
            args: [
                '--noEmit', '--strict', '--target', 'ES2022', '--module', 'ES2022',
                '--moduleResolution', 'bundler', '--lib', 'ES2022,DOM', files('testTypes.ts'),
            ],
        )
    endif
endif
