# Bundled WASM runtime

Use skill-relative `assets/wasm/` only when the user lacks a viscript installation. Copy the entire directory together: `client.js`, `bindings.js`, `vis-wasm.js`, `vis-wasm.wasm`, declarations, fonts, manifest, and license notices have intentional relative relationships.

It provides JavaScript/TypeScript construction and embedded Lua rendering. UI/Input, action maps, retained Lua runtime, audio, and interaction loops route to `vis-interactive`; this reference covers only rendering or mounting a finished scene.

## Browser host

Serve copied files over HTTP so the adjacent WASM binary can load. Import from the copied directory and destroy the runtime at teardown:

```js
import {createVis, vis} from "./vis-wasm/client.js";

const definition = vis.scene({width: 640, height: 360});
definition.circle({radius: 1.2, fill: "#2563eb", id: "subject"});
const runtime = await createVis(definition, "scene.js");
runtime.draw(document.querySelector("canvas"), runtime.duration);
window.addEventListener("pagehide", () => runtime.destroy(), {once: true});
```

If deployment moves the WASM file, provide `locateFile`. For Lua, pass trusted source text to `createVis(luaSource, "scene.lua")`; JavaScript owns loading and Canvas output. Use the typed builder for application scene construction.

## Fonts, Node, and distribution

Register every selected Theme font before rendering. The presets use `Pretendard` for heading and body roles and monospaced `JetBrainsMono` for code. `hostTheme(palette)` gives adaptive Scenes, which name no other theme, the host app's colors and, through `font`, the font of their heading and body roles, `Pretendard` when left out. The VS Code Viewer passes the list `Source Serif 4, Pretendard`, which sets Korean letters in Pretendard and the rest in Source Serif 4, so register both for it; a custom Theme that uses Source Serif 4 for Korean text names the same list. In Node, read the `.wasm` bytes and pass `wasmBinary`; the pixels `render(time)` returns are a copy that later calls leave alone. Use `layoutReport(time, 4)` for sampled rendered-layout review, not live layout.

Keep each font with its OFL notice, retain all runtime license files, and verify copied releases against `BUILD-MANIFEST.json`.
