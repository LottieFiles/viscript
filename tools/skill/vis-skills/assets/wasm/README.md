# viscript WASM bundle

This directory is a portable, optimized viscript 0.1.0 browser runtime. Keep all files together when copying it into an application.

- Import `createVis` and `vis` from `client.js`.
- Author scenes with the JavaScript builder, or pass Lua source to `createVis`.
- Use `object.label(options)` or Lua `object:label(config)` for an ordinary child Text that follows the Object; placement and styling stay explicit.
- Use `route(options)` or Lua `route(config)` for one marker-aware polyline; dashed shafts keep solid head/tail markers and one Create/Uncreate state.
- This build enables the Lua-only `vis.diagram` authoring sidecar; it lowers into ordinary Scene Objects before rendering.
- This build enables the optional `tex` module: Lua `scene:tex{...}` and JavaScript `tex()` set TeX math as ordinary Objects in the Scene's fonts, and `{tex}` parts of `spans` set formulas inline.
- Lua `grow_from_edge` and JavaScript `growFromEdge` keep a selected bounds edge fixed while revealing the perpendicular dimension.
- Serve browser applications over HTTP so the module can fetch `vis-wasm.wasm`.
- Register `Pretendard.ttf` as `Pretendard` before rendering body text; it also sets Korean text, including the Korean letters of the list `Source Serif 4, Pretendard` that the VS Code Viewer gives the `adaptive` theme.
- Register `SourceSerif4-Semibold.ttf` as `Source Serif 4` when the host theme names it, as the VS Code Viewer's does.
- Use `runtime.layoutReport(time, padding)` for root-pixel collision, clipping, stretch, and parent/child containment review across nested Viewports.
- UI/Input, action maps, retained Lua runtime, audio, and interaction loops are documented by `vis-interactive`; this bundle README covers only mounting and rendering finished scenes.
- In Node, pass the contents of `vis-wasm.wasm` as the `wasmBinary` module option.
- Preserve the included `LICENSE-*.txt` files (viscript, ThorVG, LodePNG, Lua, miniaudio) and the Pretendard, JetBrains Mono, Source Serif 4, and Noto Sans Math OFL notices when redistributing the bundle.

`BUILD-MANIFEST.json` records the exact toolchain, enabled features, file sizes, and SHA-256 checksums.
