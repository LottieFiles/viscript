# Bundled WASM runtime

Use skill-relative `assets/wasm/` only when the user lacks a viscript installation. Copy the entire directory together: `client.js`, `bindings.js`, `vis-wasm.js`, `vis-wasm.wasm`, declarations, fonts, manifest, and license notices have intentional relative relationships.

It provides JavaScript/TypeScript construction and embedded Lua rendering. UI/Input, action maps, retained Lua runtime, audio, and interaction loops route to `vis-interactive`; this reference covers only rendering or mounting a finished scene.

## Browser host

Serve copied files over HTTP so the adjacent WASM binary can load. Import from the copied directory and destroy the runtime at teardown:

```js
import {createVis, vis} from "./vis-wasm/client.js";

const definition = vis.scene({width: 640, height: 360, loop: false});
definition.circle({center: [0, 0], radius: 1.2, fill: "#2563eb", id: "subject"});
const runtime = await createVis(definition, "scene.js");
runtime.draw(document.querySelector("canvas"), runtime.duration);
window.addEventListener("pagehide", () => runtime.destroy(), {once: true});
```

If deployment moves the WASM file, provide `locateFile`. For Lua, pass trusted source text to `createVis(luaSource, "scene.lua")`; JavaScript owns loading and Canvas output. Use the typed builder for application scene construction.

## Fonts, Node, and distribution

Register every selected Theme font before rendering. The presets use `Pretendard` for heading and body roles and monospaced `JetBrainsMono` for code. `hostTheme` (the VS Code Viewer's palette) sets heading and body roles in `Source Serif 4`, and a Source Serif Text that contains Korean switches to `IBM Plex Sans KR`, so register both whenever a host Theme or a custom Theme uses Source Serif 4. In Node, read the `.wasm` bytes and pass `wasmBinary`; the pixels `render(time)` returns are a copy that later calls leave alone. Use `layoutReport(time, 4)` for sampled rendered-layout review, not live layout.

Keep each font with its OFL notice, retain all runtime license files, and verify copied releases against `BUILD-MANIFEST.json`.
