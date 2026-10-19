# Repository workflow

This reference is intentionally tied to a viscript source checkout.

## Source map

- Public C++ API: `src/inc/vis.h`; C ABI: `src/inc/vis_capi.h`.
- Scene ownership and timeline: `src/core/scene/visScene.cpp`, `src/core/scene/visObject.cpp`, and internal scene headers.
- Lua binding: `src/core/lua/visLua.cpp`.
- JavaScript builder and runtime: `src/bindings/js/bindings.js`, `src/bindings/js/client.js`, and their `.d.ts` files.
- C/WASM bridge: `src/bindings/wasm/visWasm.cpp`.
- ThorVG lowering/rendering: `src/core/renderer/`; savers: `src/core/savers/`.
- Executable scenes: `src/examples/lua/`.
- Contracts and tests: `docs/`, `src/test/`.
- Meson: root `meson.build` holds options, feature flags, ThorVG/Lua, and `subdir()` calls. Each folder owns its `meson.build`: folders under `src/core/` append to `vis_sources`/`vis_inc`; `src/core/meson.build` builds the library and optional modules; `src/meson.build` adds pkg-config; `src/cli/` and `src/bindings/wasm/` configure the CLI and WASM runtime (declared at the root to keep `build/viscript` and `build/vis-wasm.js`); tests are tables in `src/test/meson.build`, `src/examples/meson.build`, and `tools/skill/meson.build`. A new source file means editing only its folder's `meson.build`; a new example or skill template means one table entry.

Search before editing. Follow a public method from its declaration through the binding/compiler, core implementation, renderer or saver if applicable, and existing tests.

## Build matrix

Use the configured directory that matches the change; do not silently reconfigure a user's existing build.

```sh
meson compile -C build-full
meson test -C build-full --print-errorlogs
meson compile -C build-wasm
```

Inspect each build directory's configured options before using it. Native and WASM builds verify different host paths.

For a new checkout, inspect `meson_options.txt` and the README before selecting options. Keep bundled ThorVG, the `gl` renderer module, test, and example choices explicit when they affect the result. The core renders on the CPU; the GL module is experimental and opt-in (`-Dgl=true`), and WASM builds never include it.

Validate renderer work with a fresh build, and with `-Dgl=true` when the GL module is affected; its parity tests run on macOS. Distributed browser rendering uses the CPU WASM bundle; read renderer API comments for backend/target selection and readback contracts.

## Targeted verification

- Math or camera behavior: numeric C++ tests first.
- Ownership, layout, animation, or renderer behavior: scene tests plus a deterministic render invariant.
- Lua surface: load and render a representative `.lua` file.
- JavaScript builder/runtime: TypeScript declarations and `src/test/testJsBinding.mjs`.
- C/WASM bridge: C API tests and the WASM build.
- Media saver: parse the output and validate dimensions, timing, supported families, and failure behavior.

Repository examples are executable tests, not screenshots. Preserve stable IDs, deterministic data, semantic colors, and a readable final hold. When changing one example's native Lua and browser-embedded version, keep their intent and timing aligned.

## Render and layout checks

Use the CLI built by the selected directory:

```sh
build-full/viscript inspect path/to/scene.lua
build-full/viscript render path/to/scene.lua --time 1.25 -o review.png
build-full/viscript layout path/to/scene.lua --time 1.25 --padding 4
```

For scene/example visual review, use the sibling [boundary review](../../vis-skills/references/review.md): inspect meaningful animation starts/ends and the exact final state, deduplicate shared times, and add an interior peak only for a specific transient effect or suspected defect. Reuse selected review files and summarize layout results; do not accumulate PNGs or reports for every frame. Register the repository's test font when text bounds matter. Treat intended containment or connector contact differently from unrelated overlap. Engine regression tests that require exhaustive sampling still run with their configured coverage.
