project(
    'lua',
    'c',
    version: '5.4.8',
    default_options: ['c_std=c99', 'default_library=static'],
)

lua_sources = files(
    'lapi.c',
    'lauxlib.c',
    'lbaselib.c',
    'lcode.c',
    'lcorolib.c',
    'lctype.c',
    'ldebug.c',
    'ldo.c',
    'ldump.c',
    'lfunc.c',
    'lgc.c',
    'llex.c',
    'lmathlib.c',
    'lmem.c',
    'lobject.c',
    'lopcodes.c',
    'lparser.c',
    'lstate.c',
    'lstring.c',
    'lstrlib.c',
    'ltable.c',
    'ltablib.c',
    'ltm.c',
    'lundump.c',
    'lutf8lib.c',
    'lvm.c',
    'lzio.c',
)

lua_lib = static_library(
    'lua',
    lua_sources,
    c_args: ['-DLUA_USE_JUMPTABLE=0'],
    include_directories: include_directories('.'),
    override_options: ['warning_level=0', 'werror=false'],
    install: false,
)

lua_dep = declare_dependency(
    link_with: lua_lib,
    include_directories: include_directories('.'),
)
