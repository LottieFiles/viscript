# Modules and build configuration

viscript packages the scene engine as `libviscript` and optional capabilities as separate
libraries. Hosts link the modules they use and assemble their Lua hooks.

| Module | Library | Purpose |
|---|---|---|
| `diagram` | `vis-diagram` | Compile nodes, edges, and layouts into Objects |
| `tex` | `vis-tex` | Typeset TeX math into Objects |
| `motion` | `vis-motion` | Apply named states on an interaction clock |
| `ui` | `vis-ui` | Controls, hit testing, and pointer capture |
| `input` | `vis-input` | Logical input, action maps, and camera controls |
| `runtime` | `vis-lua-runtime` | Run retained Lua fixed-step callbacks |
| `audio` | `vis-audio` | Play and mix audio through miniaudio |

The core owns Object trees, timelines, cameras, display lists, and rendering.
Modules own their authoring or interaction behavior. Host tools own editor state,
platform input, asset delivery, and playback transport.

## Build combinations

```sh
# Core and CPU renderer.
meson setup build-core -Dmodules='' -Dengines=cpu

# Diagram and formula authoring.
meson setup build-author -Dmodules=diagram,tex -Dengines=cpu

# Interactive host.
meson setup build-interactive -Dmodules=ui,input,runtime,audio,motion -Dengines=cpu

# All modules and render engines.
meson setup build-full -Dmodules=all
```

`modules` defaults to none. Each selected module adds its library, installed header,
pkg-config entry, Lua hook, tests, and dependencies. Lua Runtime uses Lua; the other
modules expose C++ APIs independently of their Lua hooks. `scripts/wasm-bundle.sh`
packages all seven modules.

Select exporters and native tools independently:

```sh
meson setup build-publisher -Dmodules=diagram,tex -Dsavers=gif,video -Dtools=cli,audit
meson setup build-library -Dmodules='' -Dsavers='' -Dtools=''
```

PNG snapshots are part of core. `gif` and `video` add timeline encoders; video uses
FFmpeg. `tools=cli,audit` builds the executable and its audit subcommand.

## Adding a module

1. Add a public header in `src/inc/` and a source directory in `src/core/`.
2. Add the name to `modules` in `meson_options.txt` and select its directory in
   `src/core/meson.build`.
3. Define its library and dependency in the directory's `meson.build`, linking
   `vis_dep`.
4. Register its Lua hook in hosts that link the library. A module part for `spans`
   uses `luaSetSpanPart`; its key also belongs in the core's `MODULE_PARTS` table.
5. Link the module into CLI or WASM hosts and expose capability detection.
6. Run core, selected-module, and full builds to verify the boundary.

See [Architecture](architecture.md) for the sampling and host composition paths.
