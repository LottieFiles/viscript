project(
    'miniaudio',
    'c',
    version: '0.11.25',
    license: 'MIT-0',
    license_files: 'LICENSE',
    default_options: [
        'default_library=static',
        'warning_level=0',
        'werror=false',
    ],
)

cc = meson.get_compiler('c')
miniaudio_args = [
    '-DMA_NO_ENCODING',
    '-DMA_NO_GENERATION',
]
miniaudio_overrides = ['c_std=c11']
miniaudio_system_deps = []

if host_machine.system() == 'emscripten'
    miniaudio_args += [
        '-DMA_NO_RUNTIME_LINKING',
        '-DMA_ENABLE_ONLY_SPECIFIC_BACKENDS',
        '-DMA_ENABLE_WEBAUDIO',
    ]
    miniaudio_overrides = ['c_std=gnu11']
elif host_machine.system() == 'linux'
    miniaudio_system_deps += dependency('threads')
    foreach library : ['m', 'dl']
        found = cc.find_library(library, required: false)
        if found.found()
            miniaudio_system_deps += found
        endif
    endforeach
    if cc.sizeof('void *') == 4
        atomic = cc.find_library('atomic', required: false)
        if atomic.found()
            miniaudio_system_deps += atomic
        endif
    endif
elif host_machine.system().endswith('bsd')
    miniaudio_system_deps += dependency('threads')
    math = cc.find_library('m', required: false)
    if math.found()
        miniaudio_system_deps += math
    endif
endif

miniaudio_include = include_directories('.')
miniaudio_lib = static_library(
    'miniaudio',
    'miniaudio.c',
    include_directories: miniaudio_include,
    dependencies: miniaudio_system_deps,
    c_args: miniaudio_args,
    override_options: miniaudio_overrides,
    install: false,
)

miniaudio_license = files('LICENSE')
