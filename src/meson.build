# libviscript, optional module libraries: src/core
subdir('core')

if host_machine.system() != 'emscripten'
    pkg = import('pkgconfig')
    private_requires = ['thorvg-1']
    if lua_enabled and not lua_fallback
        private_requires += lua_pc
    endif
    pkg.generate(
        vis_lib,
        filebase: 'viscript',
        name: 'viscript',
        description: 'Lightweight ThorVG mathematical animation engine',
        subdirs: '.',
        requires_private: private_requires,
    )
    foreach module : vis_module_pkgs
        pkg.generate(
            module['lib'],
            filebase: module['name'],
            name: module['name'],
            description: module['description'],
            subdirs: '.',
            requires_private: module.get('requires', ['viscript']),
            libraries_private: module.get('libraries', []),
        )
    endforeach
endif

# Host programs are configured here; the executables are declared at the
# project root so their build-tree paths stay build/viscript and build/vis-wasm.js.
if cli_enabled
    subdir('cli')
endif
if lua_enabled and host_machine.system() == 'emscripten'
    subdir('bindings/js')
    subdir('bindings/wasm')
endif
